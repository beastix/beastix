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

/* @(#)scsi-hpux.c	1.31 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the HP-UX generic SCSI implementation.
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
#include <sys/scsi.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-hpux.c-1.31";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#ifdef	SCSI_MAXPHYS
#	define	MAX_DMA_HP	SCSI_MAXPHYS
#else
#	define	MAX_DMA_HP	(63*1024)	/* Check if this is not too big */
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
	__usal_help(f, "SIOC", "Generic SCSI",
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
				"/dev/rscsi/c%xt%xl%x", busno, tgt, tlun);
		f = open(devname, O_RDWR);
		if (f < 0)
			return (-1);
		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
		return (1);
	} else {
		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
/*				for (l = 0; l < MAX_LUN; l++) {*/
				for (l = 0; l < 1; l++) {
					snprintf(devname, sizeof (devname),
							"/dev/rscsi/c%xt%xl%x", b, t, l);
/*fprintf(stderr, "name: '%s'\n", devname);*/
					f = open(devname, O_RDWR);
					if (f >= 0) {
						usallocal(usalp)->usalfiles[b][t][l] = (short)f;
						nopen++;
					} else if (usalp->debug > 0) {
						errmsg("open '%s'\n", devname);
					}
				}
			}
		}
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

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return	(MAX_DMA_HP);
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
	return (ioctl(usalp->fd, SIOC_RESET_BUS, 0));
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	int		ret;
	int		flags;
	struct sctl_io	sctl_io;

	if ((usalp->fd < 0) || (sp->cdb_len > sizeof (sctl_io.cdb))) {
		sp->error = SCG_FATAL;
		return (0);
	}

	fillbytes((caddr_t)&sctl_io, sizeof (sctl_io), '\0');

	flags = 0;
/*	flags = SCTL_INIT_WDTR|SCTL_INIT_SDTR;*/
	if (sp->flags & SCG_RECV_DATA)
		flags |= SCTL_READ;
	if ((sp->flags & SCG_DISRE_ENA) == 0)
		flags |= SCTL_NO_ATN;

	sctl_io.flags		= flags;

	movebytes(&sp->cdb, sctl_io.cdb, sp->cdb_len);
	sctl_io.cdb_length	= sp->cdb_len;

	sctl_io.data_length	= sp->size;
	sctl_io.data		= sp->addr;

	if (sp->timeout == 0)
		sctl_io.max_msecs = 0;
	else
		sctl_io.max_msecs = (sp->timeout * 1000) + 500;

	errno		= 0;
	sp->error	= SCG_NO_ERROR;
	sp->sense_count	= 0;
	sp->u_scb.cmd_scb[0] = 0;
	sp->resid	= 0;

	ret = ioctl(usalp->fd, SIOC_IO, &sctl_io);
	if (ret < 0) {
		sp->error = SCG_FATAL;
		sp->ux_errno = errno;
		return (ret);
	}
if (usalp->debug > 0)
fprintf(stderr, "cdb_status: %X, size: %d xfer: %d\n", sctl_io.cdb_status, sctl_io.data_length, sctl_io.data_xfer);

	if (sctl_io.cdb_status == 0 || sctl_io.cdb_status == 0x02)
		sp->resid = sp->size - sctl_io.data_xfer;

	if (sctl_io.cdb_status & SCTL_SELECT_TIMEOUT ||
			sctl_io.cdb_status & SCTL_INVALID_REQUEST) {
		sp->error = SCG_FATAL;
	} else if (sctl_io.cdb_status & SCTL_INCOMPLETE) {
		sp->error = SCG_TIMEOUT;
	} else if (sctl_io.cdb_status > 0xFF) {
		errmsgno(EX_BAD, "SCSI problems: cdb_status: %X\n", sctl_io.cdb_status);

	} else if ((sctl_io.cdb_status & 0xFF) != 0) {
		sp->error = SCG_RETRYABLE;
		sp->ux_errno = EIO;

		sp->u_scb.cmd_scb[0] = sctl_io.cdb_status & 0xFF;

		sp->sense_count = sctl_io.sense_xfer;
		if (sp->sense_count > SCG_MAX_SENSE)
			sp->sense_count = SCG_MAX_SENSE;

		if (sctl_io.sense_status != S_GOOD) {
			sp->sense_count = 0;
		} else {
			movebytes(sctl_io.sense, sp->u_sense.cmd_sense, sp->sense_count);
		}

	}
	return (ret);
}
#define	sense	u_sense.Sense
