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

/* @(#)scsi-sgi.c	1.36 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the SGI generic SCSI implementation.
 *
 *	First Hacky implementation
 *	(needed libds, did not support bus scanning and had no error checking)
 *	from "Frank van Beek" <frank@neogeo.nl>
 *
 *	Actual implementation supports all usal features.
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

#include <dslib.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-sgi.c-1.36";	/* The version for this transport*/

#ifdef	USE_DSLIB

struct dsreq * dsp = 0;
#define	MAX_SCG		1	/* Max # of SCSI controllers */

#else

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#endif

#define	MAX_DMA_SGI	(256*1024)	/* Check if this is not too big */


#ifndef	USE_DSLIB
static	int	usal_sendreq(SCSI *usalp, struct usal_cmd *sp, struct dsreq *dsp);
#endif


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
	__usal_help(f, "DS", "Generic SCSI",
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

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {

		snprintf(devname, sizeof (devname),
				"/dev/scsi/sc%dd%dl%d", busno, tgt, tlun);
#ifdef	USE_DSLIB
		dsp = dsopen(devname, O_RDWR);
		if (dsp == 0)
			return (-1);
#else
		f = open(devname, O_RDWR);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					devname);
			return (-1);
		}
		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
#endif
		return (1);
	} else {
#ifdef	USE_DSLIB
		return (-1);
#else
		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
/*				for (l = 0; l < MAX_LUN; l++) {*/
				for (l = 0; l < 1; l++) {
					snprintf(devname, sizeof (devname),
							"/dev/scsi/sc%dd%dl%d", b, t, l);
					f = open(devname, O_RDWR);
					if (f >= 0) {
						usallocal(usalp)->usalfiles[b][t][l] = (short)f;
						nopen++;
					}
				}
			}
		}
#endif
	}
	return (nopen);
}

static int
usalo_close(SCSI *usalp)
{
#ifndef	USE_DSLIB
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
#else
	dsclose(dsp);
#endif
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (MAX_DMA_SGI);
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
#ifdef	USE_DSLIB
	if (dsp == NULL)
		return (-1);
	return (getfd(dsp));
#else
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (usalp->local == NULL)
		return (-1);

	return ((int)usallocal(usalp)->usalfiles[busno][tgt][tlun]);
#endif
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
	 * Do we have a SCSI reset on SGI?
	 */
#ifdef	DS_RESET
	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	/*
	 * XXX Does this reset TGT or BUS ???
	 */
	return (ioctl(usalp->fd, DS_RESET, 0));
#else
	return (-1);
#endif
}

#ifndef	USE_DSLIB
static int
usal_sendreq(SCSI *usalp, struct usal_cmd *sp, struct dsreq *dsp)
{
	int	ret;
	int	retries = 4;
	Uchar	status;

/*	if ((sp->flags & SCG_CMD_RETRY) == 0)*/
/*		retries = 0;*/

	while (--retries > 0) {
		ret = ioctl(usalp->fd, DS_ENTER, dsp);
		if (ret < 0)  {
			RET(dsp) = DSRT_DEVSCSI;
			return (-1);
		}
		/*
		 * SGI implementattion botch!!!
		 * If a target does not select the bus,
		 * the return code is DSRT_TIMEOUT
		 */
		if (RET(dsp) == DSRT_TIMEOUT) {
			struct timeval to;

			to.tv_sec = TIME(dsp)/1000;
			to.tv_usec = TIME(dsp)%1000;
			__usal_times(usalp);

			if (sp->cdb.g0_cdb.cmd == SC_TEST_UNIT_READY &&
			    usalp->cmdstop->tv_sec < to.tv_sec ||
			    (usalp->cmdstop->tv_sec == to.tv_sec &&
				usalp->cmdstop->tv_usec < to.tv_usec)) {

				RET(dsp) = DSRT_NOSEL;
				return (-1);
			}
		}
		if (RET(dsp) == DSRT_NOSEL)
			continue;		/* retry noselect 3X */

		status = STATUS(dsp);
		switch (status) {

		case 0x08:		/*  BUSY */
		case 0x18:		/*  RESERV CONFLICT */
			if (retries > 0)
				sleep(2);
			continue;
		case 0x00:		/*  GOOD */
		case 0x02:		/*  CHECK CONDITION */
		case 0x10:		/*  INTERM/GOOD */
		default:
			return (status);
		}
	}
	return (-1);	/* failed retry limit */
}
#endif

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	int	ret;
	int	i;
	int	amt = sp->cdb_len;
	int flags;
#ifndef	USE_DSLIB
	struct dsreq	ds;
	struct dsreq	*dsp = &ds;

	dsp->ds_iovbuf = 0;
	dsp->ds_iovlen = 0;
#endif

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	flags = DSRQ_SENSE;
	if (sp->flags & SCG_RECV_DATA)
		flags |= DSRQ_READ;
	else if (sp->size > 0)
		flags |= DSRQ_WRITE;

	dsp->ds_flags	= flags;
	dsp->ds_link	= 0;
	dsp->ds_synch	= 0;
	dsp->ds_ret  	= 0;

	DATABUF(dsp) 	= sp->addr;
	DATALEN(dsp)	= sp->size;
	CMDBUF(dsp)	= (void *) &sp->cdb;
	CMDLEN(dsp)	= sp->cdb_len;
	SENSEBUF(dsp)	= (caddr_t)sp->u_sense.cmd_sense;
	SENSELEN(dsp)	= sizeof (sp->u_sense.cmd_sense);
	TIME(dsp)	= (sp->timeout * 1000) + 100;

	errno		= 0;
	sp->ux_errno	= 0;
	sp->sense_count	= 0;

#ifdef	USE_DSLIB
	ret = doscsireq(usalp->fd, dsp);
#else
	ret = usal_sendreq(usalp, sp, dsp);
#endif

	if (RET(dsp) != DSRT_DEVSCSI)
		ret = 0;

	if (RET(dsp)) {
		if (RET(dsp) == DSRT_SHORT) {
			sp->resid = DATALEN(dsp)- DATASENT(dsp);
		} else if (errno) {
			sp->ux_errno = errno;
		} else {
			sp->ux_errno = EIO;
		}

		sp->u_scb.cmd_scb[0] = STATUS(dsp);

		sp->sense_count = SENSESENT(dsp);
		if (sp->sense_count > SCG_MAX_SENSE)
			sp->sense_count = SCG_MAX_SENSE;

	}
	switch (RET(dsp)) {

	default:
		sp->error = SCG_RETRYABLE;	break;

	case 0:			/* OK					*/
	case DSRT_SHORT:	/* not implemented			 */
		sp->error = SCG_NO_ERROR;	break;

	case DSRT_UNIMPL:	/* not implemented			*/
	case DSRT_REVCODE:	/* software obsolete must recompile 	*/
	case DSRT_NOSEL:
		sp->u_scb.cmd_scb[0] = 0;
		sp->error = SCG_FATAL;		break;

	case DSRT_TIMEOUT:
		sp->u_scb.cmd_scb[0] = 0;
		sp->error = SCG_TIMEOUT;	break;
	}
	return (ret);
}
