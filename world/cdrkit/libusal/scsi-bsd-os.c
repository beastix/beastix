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

/* @(#)scsi-bsd-os.c	1.28 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the BSD/OS user-land raw SCSI implementation.
 *
 *	This is a hack, that tries to emulate the functionality
 *	of the usal driver.
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

#undef	sense

#define	scsi_sense	bsdi_scsi_sense
#define	scsi_inquiry	bsdi_scsi_inquiry

/*
 * Must use -I/sys...
 * The next two files are in /sys/dev/scsi
 */
#include <dev/scsi/scsi.h>
#include <dev/scsi/scsi_ioctl.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-bsd-os.c-1.28";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#include <machine/param.h>

#define	MAX_DMA_BSDI	MAXPHYS		/* More makes problems */


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
	__usal_help(f, "SCSIRAWCDB", "Generic SCSI for devices known by BSDi",
		"", "devname:@,lun", "/dev/rsr0a:@,0", FALSE, TRUE);
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
		f = open(devname, O_RDWR|O_NONBLOCK);
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
				f = open(devname, O_RDWR|O_NONBLOCK);
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
		if (device == NULL || device[0] == '\0')
			return (0);
		f = open(device, O_RDWR|O_NONBLOCK);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					device);
			return (0);
		}
		if (tlun == -2) {	/* If 'lun' is not known, we reject */
			close(f);
			errno = EINVAL;
			return (0);
		}
		busno = 0;		/* use fake number, we cannot get it */
		tgt   = 0;		/* use fake number, we cannot get it */
		usal_settarget(usalp, busno, tgt, tlun);
		/* 'lun' has been specified on command line */
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
	int	Bus;
	int	Target;
	int	Lun;
	BOOL	onetarget = FALSE;

	if (usal_scsibus(usalp) >= 0 && usal_target(usalp) >= 0 && usal_lun(usalp) >= 0)
		onetarget = TRUE;

	/*
	 * Unfortunately there is no way to get the right values from kernel.
	 */
	Bus	= busno;
	Target	= tgt;
	Lun	= tlun;

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
	long maxdma = MAX_DMA_BSDI;

	return (maxdma);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = malloc((size_t)(amt));
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
	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	/*
	 * Cannot reset on BSD/OS
	 */
	errno = EINVAL;
	return (-1);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	scsi_user_cdb_t	suc;
	int		ret = 0;

/*	fprintf((FILE *)usalp->errfile, "f: %d\n", f);*/
	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	/* Zero the structure... */
	fillbytes(&suc, sizeof (suc), '\0');

	/* Read or write? */
	if (sp->flags & SCG_RECV_DATA) {
		suc.suc_flags |= SUC_READ;
	} else if (sp->size > 0) {
		suc.suc_flags |= SUC_WRITE;
	}

	suc.suc_timeout = sp->timeout;

	suc.suc_cdblen = sp->cdb_len;
	movebytes(sp->cdb.cmd_cdb, suc.suc_cdb, suc.suc_cdblen);

	suc.suc_datalen = sp->size;
	suc.suc_data = sp->addr;

	if (ioctl(usalp->fd, SCSIRAWCDB, &suc) < 0) {
		ret  = -1;
		sp->ux_errno = geterrno();
		if (sp->ux_errno != ENOTTY)
			ret = 0;
	} else {
		sp->ux_errno = 0;
		if (suc.suc_sus.sus_status != STS_GOOD)
			sp->ux_errno = EIO;
	}
	fillbytes(&sp->scb, sizeof (sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof (sp->u_sense.cmd_sense), '\0');
#if 0
	/*
	 * Unfortunalety, BSD/OS has no idea of DMA residual count.
	 */
	sp->resid = req.datalen - req.datalen_used;
	sp->sense_count = req.senselen_used;
#else
	sp->resid = 0;
	sp->sense_count = sizeof (suc.suc_sus.sus_sense);
#endif
	if (sp->sense_count > SCG_MAX_SENSE)
		sp->sense_count = SCG_MAX_SENSE;
	movebytes(suc.suc_sus.sus_sense, sp->u_sense.cmd_sense, sp->sense_count);
	sp->u_scb.cmd_scb[0] = suc.suc_sus.sus_status;

	switch (suc.suc_sus.sus_status) {

	case STS_GOOD:
				sp->error = SCG_NO_ERROR;	break;
	case STS_CMD_TERMINATED:sp->error = SCG_TIMEOUT;	break;
	case STS_BUSY:		sp->error = SCG_RETRYABLE;	break;
	case STS_CHECKCOND:	sp->error = SCG_RETRYABLE;	break;
	case STS_QUEUE_FULL:	sp->error = SCG_RETRYABLE;	break;
	default:		sp->error = SCG_FATAL;		break;
	}

	return (ret);
}

#define	sense	u_sense.Sense

#undef scsi_sense
#undef scsi_inquiry
