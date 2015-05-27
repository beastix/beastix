/*
 * This file has been modified for the cdrkit suite.
 *
 * The behaviour and appearence of the program code below can differ to a major
 * extent from the version distributed by the original author(s).
 *
 * For details, see Changelog file distributed with the cdrkit package. If you
 * received this file from another source then ask the distributing person for
 * a log of modifications.
 *
 */

/* @(#)scsi-bsd.c	1.42 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the NetBSD/FreeBSD/OpenBSD generic SCSI implementation.
 *
 *	This is a hack, that tries to emulate the functionality
 *	of the usal driver.
 *	The SCSI tranport of the generic *BSD implementation is very
 *	similar to the SCSI command transport of the
 *	6 years older usal driver.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1997 J. Schilling
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef HAVE_CAMLIB_H

#undef	sense
#include <sys/scsiio.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-bsd.c-1.42";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

/*#define	MAX_DMA_BSD	(32*1024)*/
#define	MAX_DMA_BSD	(60*1024)	/* More seems to make problems */

#if	defined(__NetBSD__) && defined(TYPE_ATAPI)
/*
 * NetBSD 1.3 has a merged SCSI/ATAPI system, so this structure
 * is slightly different.
 */
#define	MAYBE_ATAPI
#define	SADDR_ISSCSI(a)	((a).type == TYPE_SCSI)

#define	SADDR_BUS(a)	(SADDR_ISSCSI(a)?(a).addr.scsi.scbus:(MAX_SCG-1))
#define	SADDR_TARGET(a)	(SADDR_ISSCSI(a)?(a).addr.scsi.target:(a).addr.atapi.atbus*2+(a).addr.atapi.drive)
#define	SADDR_LUN(a)	(SADDR_ISSCSI(a)?(a).addr.scsi.lun:0)
#else

#if	defined(__OpenBSD__) && defined(TYPE_ATAPI)
#define	SADDR_ISSCSI(a)	((a).type == TYPE_SCSI)
#else
#define	SADDR_ISSCSI(a)	(1)
#endif

#define	SADDR_BUS(a)	(a).scbus
#define	SADDR_TARGET(a)	(a).target
#define	SADDR_LUN(a)	(a).lun
#endif	/* __NetBSD__ && TYPE_ATAPI */

static	BOOL	usal_setup(SCSI *usalp, int f, int busno, int tgt, int tlun);

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_version(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (__sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "SCIOCCOMMAND", "SCSI for devices known by *BSD",
		"", "device or bus,target,lun", "/dev/rcd0a:@ or 1,2,0", FALSE, TRUE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
		int	tlun	= usal_lun(usalp);
	register int	f;
	register int	b;
	register int	t;
	register int	l;
	register int	nopen = 0;
	char		devname[64];

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2))
		goto openbydev;

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {

		snprintf(devname, sizeof (devname),
				"/dev/su%d-%d-%d", busno, tgt, tlun);
		f = open(devname, O_RDWR);
		if (f < 0) {
			goto openbydev;
		}
		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
		return (1);

	} else for (b = 0; b < MAX_SCG; b++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				snprintf(devname, sizeof (devname),
							"/dev/su%d-%d-%d", b, t, l);
				f = open(devname, O_RDWR);
/*				fprintf(stderr, "open (%s) = %d\n", devname, f);*/

				if (f < 0) {
					if (errno != ENOENT &&
					    errno != ENXIO &&
					    errno != ENODEV) {
						if (usalp->errstr)
							snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
								"Cannot open '%s'",
								devname);
						return (0);
					}
				} else {
					if (usal_setup(usalp, f, b, t, l))
						nopen++;
				}
			}
		}
	}
	/*
	 * Could not open /dev/su-* or got dev=devname:b,l,l / dev=devname:@,l
	 * We do the apropriate tests and try our best.
	 */
openbydev:
	if (nopen == 0) {
		struct scsi_addr saddr;

		if (device == NULL || device[0] == '\0')
			return (0);
		f = open(device, O_RDWR);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					device);
			return (0);
		}
		if (tgt == -2) {
			if (ioctl(f, SCIOCIDENTIFY, &saddr) < 0) {
				close(f);
				errno = EINVAL;
				return (0);
			}
			busno	= SADDR_BUS(saddr);
			tgt	= SADDR_TARGET(saddr);
			if ((tlun >= 0) && (tlun != SADDR_LUN(saddr))) {
				close(f);
				errno = EINVAL;
				return (0);
			}
			tlun	= SADDR_LUN(saddr);
			usal_settarget(usalp, busno, tgt, tlun);
		}
		if (usal_setup(usalp, f, busno, tgt, tlun))
			nopen++;
	}
	return (nopen);
}

static int
usalo_close(SCSI *usalp)
{
	register int	f;
	register int	b;
	register int	t;
	register int	l;

	if (usalp->local == NULL)
		return (-1);

	for (b = 0; b < MAX_SCG; b++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				f = usallocal(usalp)->usalfiles[b][t][l];
				if (f >= 0)
					close(f);
				usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}
	return (0);
}

static BOOL
usal_setup(SCSI *usalp, int f, int busno, int tgt, int tlun)
{
	struct scsi_addr saddr;
	int	Bus;
	int	Target;
	int	Lun;
	BOOL	onetarget = FALSE;

	if (usal_scsibus(usalp) >= 0 && usal_target(usalp) >= 0 && usal_lun(usalp) >= 0)
		onetarget = TRUE;

	if (ioctl(f, SCIOCIDENTIFY, &saddr) < 0) {
		errmsg("Cannot get SCSI addr.\n");
		close(f);
		return (FALSE);
	}
	Bus	= SADDR_BUS(saddr);
	Target	= SADDR_TARGET(saddr);
	Lun	= SADDR_LUN(saddr);

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"Bus: %d Target: %d Lun: %d\n", Bus, Target, Lun);
	}

	if (Bus >= MAX_SCG || Target >= MAX_TGT || Lun >= MAX_LUN) {
		close(f);
		return (FALSE);
	}

	if (usallocal(usalp)->usalfiles[Bus][Target][Lun] == (short)-1)
		usallocal(usalp)->usalfiles[Bus][Target][Lun] = (short)f;

	if (onetarget) {
		if (Bus == busno && Target == tgt && Lun == tlun) {
			return (TRUE);
		} else {
			usallocal(usalp)->usalfiles[Bus][Target][Lun] = (short)-1;
			close(f);
		}
	}
	return (FALSE);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long maxdma = MAX_DMA_BSD;

	return (maxdma);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = valloc((size_t)(amt));
	return (usalp->bufbase);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	register int	t;
	register int	l;

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (usalp->local == NULL)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++)
			if (usallocal(usalp)->usalfiles[busno][t][l] >= 0)
				return (TRUE);
	}
	return (FALSE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (usalp->local == NULL)
		return (-1);

	return ((int)usallocal(usalp)->usalfiles[busno][tgt][tlun]);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
#ifdef	MAYBE_ATAPI
	struct scsi_addr saddr;

	if (ioctl(usalp->fd, SCIOCIDENTIFY, &saddr) < 0)
		return (-1);

	if (!SADDR_ISSCSI(saddr))
		return (TRUE);
#endif
	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	/*
	 * XXX Does this reset TGT or BUS ???
	 */
	return (ioctl(usalp->fd, SCIOCRESET, 0));
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	scsireq_t	req;
	register long	*lp1;
	register long	*lp2;
	int		ret = 0;

/*	fprintf((FILE *)usalp->errfile, "fd: %d\n", usalp->fd);*/
	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}
	req.flags = SCCMD_ESCAPE;	/* We set the SCSI cmd len */
	if (sp->flags & SCG_RECV_DATA)
		req.flags |= SCCMD_READ;
	else if (sp->size > 0)
		req.flags |= SCCMD_WRITE;

	req.timeout = sp->timeout * 1000;
	lp1 = (long *)sp->cdb.cmd_cdb;
	lp2 = (long *)req.cmd;
	*lp2++ = *lp1++;
	*lp2++ = *lp1++;
	*lp2++ = *lp1++;
	*lp2++ = *lp1++;
	req.cmdlen = sp->cdb_len;
	req.databuf = sp->addr;
	req.datalen = sp->size;
	req.datalen_used = 0;
	fillbytes(req.sense, sizeof (req.sense), '\0');
	if (sp->sense_len > sizeof (req.sense))
		req.senselen = sizeof (req.sense);
	else if (sp->sense_len < 0)
		req.senselen = 0;
	else
		req.senselen = sp->sense_len;
	req.senselen_used = 0;
	req.status = 0;
	req.retsts = 0;
	req.error = 0;

	if (ioctl(usalp->fd, SCIOCCOMMAND, (void *)&req) < 0) {
		ret  = -1;
		sp->ux_errno = geterrno();
		if (sp->ux_errno != ENOTTY)
			ret = 0;
	} else {
		sp->ux_errno = 0;
		if (req.retsts != SCCMD_OK)
			sp->ux_errno = EIO;
	}
	fillbytes(&sp->scb, sizeof (sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof (sp->u_sense.cmd_sense), '\0');
	sp->resid = req.datalen - req.datalen_used;
	sp->sense_count = req.senselen_used;
	if (sp->sense_count > SCG_MAX_SENSE)
		sp->sense_count = SCG_MAX_SENSE;
	movebytes(req.sense, sp->u_sense.cmd_sense, sp->sense_count);
	sp->u_scb.cmd_scb[0] = req.status;

	switch (req.retsts) {

	case SCCMD_OK:
#ifdef	BSD_SCSI_SENSE_BUG
				sp->u_scb.cmd_scb[0] = 0;
				sp->ux_errno = 0;
#endif
				sp->error = SCG_NO_ERROR;	break;
	case SCCMD_TIMEOUT:	sp->error = SCG_TIMEOUT;	break;
	default:
	case SCCMD_BUSY:	sp->error = SCG_RETRYABLE;	break;
	case SCCMD_SENSE:	sp->error = SCG_RETRYABLE;	break;
	case SCCMD_UNKNOWN:	sp->error = SCG_FATAL;		break;
	}

	return (ret);
}
#define	sense	u_sense.Sense

#else /* BSD_CAM */
/*
 *	Interface for the FreeBSD CAM passthrough device.
 *
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998 Kenneth D. Merry <ken@kdm.org>
 * Copyright (c) 1998 Joerg Schilling <schilling@fokus.gmd.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#undef	sense
#define	scsi_sense CAM_scsi_sense
#define	scsi_inquiry CAM_scsi_inquiry
#include <sys/param.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#include <camlib.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-bsd.c-1.42";	/* The version for this transport*/

#define	CAM_MAXDEVS	128
struct usal_local {
	struct cam_device *cam_devices[CAM_MAXDEVS + 1];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_version(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (__sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "CAM", "Generic transport independent SCSI (Common Access Method)",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

/*
 * Build a list of everything we can find.
 */
static int
usalo_open(SCSI *usalp, char *device)
{
	int				busno	= usal_scsibus(usalp);
	int				tgt	= usal_target(usalp);
	int				tlun	= usal_lun(usalp);
	char				name[16];
	int				unit;
	int				nopen = 0;
	union ccb			ccb;
	int				bufsize;
	struct periph_match_pattern	*match_pat;
	int				fd;

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);

		for (unit = 0; unit <= CAM_MAXDEVS; unit++) {
			usallocal(usalp)->cam_devices[unit] = (struct cam_device *)-1;
		}
	}


	/*
	 * If we're not scanning the bus, just open one device.
	 */
	if (busno >= 0 && tgt >= 0 && tlun >= 0) {
		usallocal(usalp)->cam_devices[0] = cam_open_btl(busno, tgt, tlun, O_RDWR, NULL);
		if (usallocal(usalp)->cam_devices[0] == NULL)
			return (-1);
		nopen++;
		return (nopen);
	}

	/*
	 * First open the transport layer device.  There's no point in the
	 * rest of this if we can't open it.
	 */

	if ((fd = open(XPT_DEVICE, O_RDWR)) < 0) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Open of %s failed", XPT_DEVICE);
		return (-1);
	}
	fillbytes(&ccb, sizeof (union ccb), '\0');

	/*
	 * Get a list of up to CAM_MAXDEVS passthrough devices in the
	 * system.
	 */
	ccb.ccb_h.func_code = XPT_DEV_MATCH;

	/*
	 * Setup the result buffer.
	 */
	bufsize = sizeof (struct dev_match_result) * CAM_MAXDEVS;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
	if (ccb.cdm.matches == NULL) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Couldn't malloc match buffer");
		close(fd);
		return (-1);
	}
	ccb.cdm.num_matches = 0;

	/*
	 * Setup the pattern buffer.  We're matching against all
	 * peripherals named "pass".
	 */
	ccb.cdm.num_patterns = 1;
	ccb.cdm.pattern_buf_len = sizeof (struct dev_match_pattern);
	ccb.cdm.patterns = (struct dev_match_pattern *)malloc(
		sizeof (struct dev_match_pattern));
	if (ccb.cdm.patterns == NULL) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Couldn't malloc pattern buffer");
		close(fd);
		free(ccb.cdm.matches);
		return (-1);
	}
	ccb.cdm.patterns[0].type = DEV_MATCH_PERIPH;
	match_pat = &ccb.cdm.patterns[0].pattern.periph_pattern;
	snprintf(match_pat->periph_name, sizeof (match_pat->periph_name),
								"pass");
	match_pat->flags = PERIPH_MATCH_NAME;

	if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"CAMIOCOMMAND ioctl failed");
		close(fd);
		free(ccb.cdm.matches);
		free(ccb.cdm.patterns);
		return (-1);
	}

	if ((ccb.ccb_h.status != CAM_REQ_CMP) ||
	    ((ccb.cdm.status != CAM_DEV_MATCH_LAST) &&
	    (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
/*		errmsgno(EX_BAD, "Got CAM error 0x%X, CDM error %d.\n",*/
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Got CAM error 0x%X, CDM error %d",
			ccb.ccb_h.status, ccb.cdm.status);
		close(fd);
		free(ccb.cdm.matches);
		free(ccb.cdm.patterns);
		return (-1);
	}

	free(ccb.cdm.patterns);
	close(fd);

	for (unit = 0; unit < MIN(CAM_MAXDEVS, ccb.cdm.num_matches); unit++) {
		struct periph_match_result *periph_result;

		/*
		 * We shouldn't have anything other than peripheral
		 * matches in here.  If we do, it means an error in the
		 * device matching code in the transport layer.
		 */
		if (ccb.cdm.matches[unit].type != DEV_MATCH_PERIPH) {
/*			errmsgno(EX_BAD, "Kernel error! got periph match type %d!!\n",*/
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Kernel error! got periph match type %d!!",
					ccb.cdm.matches[unit].type);
			free(ccb.cdm.matches);
			return (-1);
		}
		periph_result = &ccb.cdm.matches[unit].result.periph_result;

		snprintf(name, sizeof (name),
				"/dev/%s%d", periph_result->periph_name,
			periph_result->unit_number);

		/*
		 * cam_open_pass() avoids all lookup and translation from
		 * "regular device name" to passthrough unit number and
		 * just opens the device in question as a passthrough device.
		 */
		usallocal(usalp)->cam_devices[unit] = cam_open_pass(name, O_RDWR, NULL);

		/*
		 * If we get back NULL from the open routine, it wasn't
		 * able to open the given passthrough device for one reason
		 * or another.
		 */
		if (usallocal(usalp)->cam_devices[unit] == NULL) {
#ifdef	OLD
			errmsgno(EX_BAD, "Error opening /dev/%s%d\n",
				periph_result->periph_name,
				periph_result->unit_number);
			errmsgno(EX_BAD, "%s\n", cam_errbuf);
#endif
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Error opening /dev/%s%d Cam error '%s'",
					periph_result->periph_name,
					periph_result->unit_number,
					cam_errbuf);
			break;
		}
		nopen++;
	}

	free(ccb.cdm.matches);
	return (nopen);
}

static int
usalo_close(SCSI *usalp)
{
	register int	i;

	if (usalp->local == NULL)
		return (-1);

	for (i = 0; i <= CAM_MAXDEVS; i++) {
		if (usallocal(usalp)->cam_devices[i] != (struct cam_device *)-1)
			cam_close_device(usallocal(usalp)->cam_devices[i]);
		usallocal(usalp)->cam_devices[i] = (struct cam_device *)-1;
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
#ifdef	DFLTPHYS
	return (DFLTPHYS);
#else
	return (MAXPHYS);
#endif
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = valloc((size_t)(amt));
	return (usalp->bufbase);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	int		unit;

	if (usalp->local == NULL)
		return (FALSE);

	/*
	 * There's a "cleaner" way to do this, using the matching code, but
	 * it would involve more code than this solution...
	 */
	for (unit = 0; usallocal(usalp)->cam_devices[unit] != (struct cam_device *)-1; unit++) {
		if (usallocal(usalp)->cam_devices[unit] == NULL)
			continue;
		if (usallocal(usalp)->cam_devices[unit]->path_id == busno)
			return (TRUE);
	}
	return (FALSE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int unit, int tlun)
{
	int		i;

	if (usalp->local == NULL)
		return (-1);

	for (i = 0; usallocal(usalp)->cam_devices[i] != (struct cam_device *)-1; i++) {
		if (usallocal(usalp)->cam_devices[i] == NULL)
			continue;
		if ((usallocal(usalp)->cam_devices[i]->path_id == busno) &&
		    (usallocal(usalp)->cam_devices[i]->target_id == unit) &&
		    (usallocal(usalp)->cam_devices[i]->target_lun == tlun))
			return (i);
	}
	return (-1);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	/* XXX synchronous reset command - is this wise? */
	errno = EINVAL;
	return (-1);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd		*sp = usalp->scmd;
	struct cam_device	*dev;
	union ccb		ccb_space;
	union ccb		*ccb = &ccb_space;
	int			rv, result;
	u_int32_t		ccb_flags;

	if (usalp->fd < 0) {
#if 0
		fprintf((FILE *)usalp->errfile,
			"attempt to reference invalid unit %d\n", usalp->fd);
#endif
		sp->error = SCG_FATAL;
		return (0);
	}

	dev = usallocal(usalp)->cam_devices[usalp->fd];
	fillbytes(&ccb->ccb_h, sizeof (struct ccb_hdr), '\0');
	ccb->ccb_h.path_id = dev->path_id;
	ccb->ccb_h.target_id = dev->target_id;
	ccb->ccb_h.target_lun = dev->target_lun;

	/* Build the CCB */
	fillbytes(&(&ccb->ccb_h)[1], sizeof (struct ccb_scsiio), '\0');
	movebytes(sp->cdb.cmd_cdb, &ccb->csio.cdb_io.cdb_bytes, sp->cdb_len);

	/*
	 * Set the data direction flags.
	 */
	if (sp->size != 0) {
		ccb_flags = (sp->flags & SCG_RECV_DATA) ?   CAM_DIR_IN :
							    CAM_DIR_OUT;
	} else {
		ccb_flags = CAM_DIR_NONE;
	}

	ccb_flags |= CAM_DEV_QFRZDIS;

	/*
	 * If you don't want to bother with sending tagged commands under CAM,
	 * we don't need to do anything to cdrecord.  If you want to send
	 * tagged commands to those devices that support it, we'll need to set
	 * the tag action valid field like this in usalo_send():
	 *
	 *	ccb_flags |= CAM_DEV_QFRZDIS | CAM_TAG_ACTION_VALID;
	 */

	cam_fill_csio(&ccb->csio,
			/* retries */	1,
			/* cbfncp */	NULL,
			/* flags */	ccb_flags,
			/* tag_action */ MSG_SIMPLE_Q_TAG,
			/* data_ptr */	(u_int8_t *)sp->addr,
			/* dxfer_len */	sp->size,
			/* sense_len */	SSD_FULL_SIZE,
			/* cdb_len */	sp->cdb_len,
			/* timeout */	sp->timeout * 1000);

	/* Run the command */
	errno = 0;
	if ((rv = cam_send_ccb(dev, ccb)) == -1) {
		return (rv);
	} else {
		/*
		 * Check for command status.  Selection timeouts are fatal.
		 * For command timeouts, we pass back the appropriate
		 * error.  If we completed successfully, there's (obviously)
		 * no error.  We declare anything else "retryable".
		 */
		switch (ccb->ccb_h.status & CAM_STATUS_MASK) {
			case CAM_SEL_TIMEOUT:
				result = SCG_FATAL;
				break;
			case CAM_CMD_TIMEOUT:
				result = SCG_TIMEOUT;
				break;
			case CAM_REQ_CMP:
				result = SCG_NO_ERROR;
				break;
			default:
				result = SCG_RETRYABLE;
				break;
		}
	}

	sp->error = result;
	if (result != SCG_NO_ERROR)
		sp->ux_errno = EIO;

	/* Pass the result back up */
	fillbytes(&sp->scb, sizeof (sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof (sp->u_sense.cmd_sense), '\0');
	sp->resid = ccb->csio.resid;
	sp->sense_count = SSD_FULL_SIZE - ccb->csio.sense_resid;

	/*
	 * Determine how much room we have for sense data.
	 */
	if (sp->sense_count > SCG_MAX_SENSE)
		sp->sense_count = SCG_MAX_SENSE;

	/* Copy the sense data out */
	movebytes(&ccb->csio.sense_data, &sp->u_sense.cmd_sense, sp->sense_count);

	sp->u_scb.cmd_scb[0] = ccb->csio.scsi_status;

	return (0);
}

#undef scsi_sense
#undef scsi_inquiry
#define	sense u_sense.Sense

#endif /* BSD_CAM */
