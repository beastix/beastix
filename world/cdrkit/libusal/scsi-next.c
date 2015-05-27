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

/* @(#)scsi-next.c	1.32 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the NeXT Step generic SCSI implementation.
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

#include <bsd/dev/scsireg.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-next.c-1.32";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
	int	usalfile;
	int	max_scsibus;
	int	cur_scsibus;
	int	cur_target;
	int	cur_lun;
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

/*#define	MAX_DMA_NEXT	(32*1024)*/
#define	MAX_DMA_NEXT	(64*1024)	/* Check if this is not too big */


static	BOOL	usal_setup(SCSI *usalp, int busno, int tgt, int tlun, BOOL ex);

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
	__usal_help(f, "SGIOCREQ", "Generic SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
		int	tlun	= usal_lun(usalp);
	register int	f;
	register int	i;
	char		devname[64];

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

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

		usallocal(usalp)->usalfile		= -1;
		usallocal(usalp)->max_scsibus	= -1;
		usallocal(usalp)->cur_scsibus	= -1;
		usallocal(usalp)->cur_target	= -1;
		usallocal(usalp)->cur_lun		= -1;
	}

	for (i = 0; i < 4; i++) {
		snprintf(devname, sizeof (devname), "/dev/sg%d", i);
		f = open(devname, O_RDWR);
		if (usalp->debug > 0)
			errmsg("open(devname: '%s') : %d\n", devname, f);
		if (f < 0)
			continue;
		usallocal(usalp)->usalfile = f;
		break;

	}
	if (f >= 0) {
		if (usallocal(usalp)->max_scsibus < 0) {
			for (i = 0; i < MAX_SCG; i++) {
				if (!SCGO_HAVEBUS(usalp, i))
					break;
			}
			usallocal(usalp)->max_scsibus = i;
		}
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"maxbus: %d\n", usallocal(usalp)->max_scsibus);
		}
		if (usallocal(usalp)->max_scsibus <= 0) {
			usallocal(usalp)->max_scsibus = 1;
			usallocal(usalp)->cur_scsibus = 0;
		}

		ioctl(f, SGIOCENAS);
		if (busno > 0 && tgt > 0 && tlun > 0)
			usal_setup(usalp, busno, tgt, tlun, TRUE);
		return (1);
	}
	if (usalp->errstr)
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			"Cannot open '/dev/sg*'");
	return (0);
}

static int
usalo_close(SCSI *usalp)
{
	if (usalp->local == NULL)
		return (-1);

	if (usallocal(usalp)->usalfile >= 0)
		close(usallocal(usalp)->usalfile);
	usallocal(usalp)->usalfile = -1;
	return (0);
}

static BOOL
usal_setup(SCSI *usalp, int busno, int tgt, int tlun, BOOL ex)
{
	scsi_adr_t sadr;

	sadr.sa_target = tgt;
	sadr.sa_lun = tlun;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usal_setup curbus %d -> %d\n", usallocal(usalp)->cur_scsibus, busno);
	}

	if (usalp->debug > 0 && ((usallocal(usalp)->cur_scsibus < 0 || usallocal(usalp)->cur_scsibus != busno)))
		fprintf((FILE *)usalp->errfile, "setting SCSI bus to: %d\n", busno);
	if ((usallocal(usalp)->cur_scsibus < 0 || usallocal(usalp)->cur_scsibus != busno) &&
				ioctl(usallocal(usalp)->usalfile, SGIOCCNTR, &busno) < 0) {

		usallocal(usalp)->cur_scsibus = -1;	/* Driver is in undefined state */
		if (ex)
/*			comerr("Cannot set SCSI bus\n");*/
			errmsg("Cannot set SCSI bus\n");
		return (FALSE);
	}
	usallocal(usalp)->cur_scsibus	= busno;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"setting target/lun to: %d/%d\n", tgt, tlun);
	}
	if (ioctl(usallocal(usalp)->usalfile, SGIOCSTL, &sadr) < 0) {
		if (ex)
			comerr("Cannot set SCSI address\n");
		return (FALSE);
	}
	usallocal(usalp)->cur_scsibus	= busno;
	usallocal(usalp)->cur_target	= tgt;
	usallocal(usalp)->cur_lun		= tlun;
	return (TRUE);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long maxdma = MAX_DMA_NEXT;
#ifdef	SGIOCMAXDMA
	int  m;

	if (ioctl(usallocal(usalp)->usalfile, SGIOCMAXDMA, &m) >= 0) {
		maxdma = m;
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"maxdma: %d\n", maxdma);
		}
	}
#endif
	return (maxdma);
}
#ifdef	XXX
#define	SGIOCENAS	_IO('s', 2)			/* enable autosense */
#define	SGIOCDAS	_IO('s', 3)			/* disable autosense */
#define	SGIOCRST	_IO('s', 4)			/* reset SCSI bus */
#define	SGIOCCNTR	_IOW('s', 6, int)		/* select controller */
#define	SGIOCGAS	_IOR('s', 7, int)		/* get autosense */
#define	SGIOCMAXDMA	_IOR('s', 8, int)		/* max DMA size */
#define	SGIOCNUMTARGS	_IOR('s', 9, int)		/* # of targets/bus */
#endif

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
	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (usalp->local == NULL)
		return (FALSE);

	if (usallocal(usalp)->max_scsibus > 0 && busno >= usallocal(usalp)->max_scsibus)
		return (FALSE);

	return (usal_setup(usalp, busno, 0, 0, FALSE));
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);
	if (usallocal(usalp)->max_scsibus > 0 && busno >= usallocal(usalp)->max_scsibus)
		return (-1);

	if (usalp->local == NULL)
		return (-1);

	if ((busno != usallocal(usalp)->cur_scsibus) || (tgt != usallocal(usalp)->cur_target) || (tlun != usallocal(usalp)->cur_lun)) {
		if (!usal_setup(usalp, busno, tgt, tlun, FALSE))
			return (-1);
	}
	return (usallocal(usalp)->usalfile);
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
	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	return (ioctl(usalp->fd, SGIOCRST, 0));
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	struct scsi_req	req;
	register long	*lp1;
	register long	*lp2;
	int		ret = 0;

	if (usalp->fd < 0 || (sp->cdb_len > sizeof (req.sr_cdb))) {
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		return (0);
	}
	fillbytes(&req, sizeof (req), '\0');
	movebytes(sp->cdb.cmd_cdb, &req.sr_cdb, sp->cdb_len);
	if (sp->size) {
		req.sr_dma_dir = SR_DMA_WR;
		if (sp->flags & SCG_RECV_DATA)
			req.sr_dma_dir = SR_DMA_RD;
	}
	req.sr_addr = sp->addr;
	req.sr_dma_max = sp->size;
	req.sr_ioto = sp->timeout;
	if (ioctl(usalp->fd, SGIOCREQ, (void *)&req) < 0) {
		ret  = -1;
		sp->ux_errno = geterrno();
		if (sp->ux_errno != ENOTTY)
			ret = 0;
	} else {
		sp->ux_errno = 0;
	}
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "dma_dir:     %X\n", req.sr_dma_dir);
		fprintf((FILE *)usalp->errfile, "dma_addr:    %X\n", req.sr_addr);
		fprintf((FILE *)usalp->errfile, "io_time:     %d\n", req.sr_ioto);
		fprintf((FILE *)usalp->errfile, "io_status:   %d\n", req.sr_io_status);
		fprintf((FILE *)usalp->errfile, "scsi_status: %X\n", req.sr_scsi_status);
		fprintf((FILE *)usalp->errfile, "dma_xfer:    %d\n", req.sr_dma_xfr);
	}
	sp->u_scb.cmd_scb[0] = req.sr_scsi_status;
	sp->sense_count = sizeof (esense_reply_t);
	if (sp->sense_count > sp->sense_len)
		sp->sense_count = sp->sense_len;
	if (sp->sense_count > SCG_MAX_SENSE)
		sp->sense_count = SCG_MAX_SENSE;
	if (sp->sense_count < 0)
		sp->sense_count = 0;
	movebytes(&req.sr_esense, sp->u_sense.cmd_sense, sp->sense_count);
	sp->resid = sp->size - req.sr_dma_xfr;

	switch (req.sr_io_status) {

	case SR_IOST_GOOD:	sp->error = SCG_NO_ERROR;	break;

	case SR_IOST_CHKSNV:	sp->sense_count = 0;
	case SR_IOST_CHKSV:	sp->error = SCG_RETRYABLE;
				break;

	case SR_IOST_SELTO:
	case SR_IOST_DMAOR:
				sp->error = SCG_FATAL;		break;

	case SR_IOST_IOTO:	sp->error = SCG_TIMEOUT;	break;

	case SR_IOST_PERM:
	case SR_IOST_NOPEN:
				sp->error = SCG_FATAL;
				ret = (-1);
				break;

	default:		sp->error = SCG_RETRYABLE;	break;

	}
	return (ret);
}
