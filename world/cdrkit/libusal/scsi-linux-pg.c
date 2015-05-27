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

/* @(#)scsi-linux-pg.c	1.43 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the Linux PARIDE implementation.
 *
 *	We emulate the functionality of the usal driver, via the pg driver.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1997  J. Schilling
 *	Copyright (c) 1998  Grant R. Guenther	<grant@torque.net>
 *			    Under the terms of the GNU public license.
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

#include <string.h>
#ifdef	HAVE_LINUX_PG_H
#include <linux/pg.h>
#else
#include "pg.h"		/* Use local version as Linux sometimes doesn't have */
#endif			/* installed. Now libusal always supports PP SCSI    */

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version_pg[] = "scsi-linux-pg.c-1.43";	/* The version for this transport*/

#ifdef	USE_PG_ONLY

#define	MAX_SCG		1	/* Max # of SCSI controllers */
#define	MAX_TGT		8
#define	MAX_LUN		8

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
	short	buscookies[MAX_SCG];
	int	pgbus;
	char	*SCSIbuf;
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#else

#define	usalo_version	pg_version
#define	usalo_help	pg_help
#define	usalo_open	pg_open
#define	usalo_close	pg_close
#define	usalo_send	pg_send
#define	usalo_maxdma	pg_maxdma
#define	usalo_initiator_id pg_initiator_id
#define	usalo_isatapi	pg_isatapi
#define	usalo_reset	pg_reset

static	char	*pg_version(SCSI *usalp, int what);
static	int	pg_help(SCSI *usalp, FILE *f);
static	int	pg_open(SCSI *usalp, char *device);
static	int	pg_close(SCSI *usalp);
static	long	pg_maxdma(SCSI *usalp, long amt);
static	int 	pg_initiator_id(SCSI *usalp);
static	int 	pg_isatapi(SCSI *usalp);
static	int	pg_reset(SCSI *usalp, int what);
static	int	pg_send(SCSI *usalp);

#endif

static	int	do_usal_cmd(SCSI *usalp, struct usal_cmd *sp);
static	int	do_usal_sense(SCSI *usalp, struct usal_cmd *sp);


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
			return (_usal_trans_version_pg);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (___sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "pg", "SCSI transport for ATAPI over Parallel Port",
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
#ifdef	USE_PG_ONLY
	register int	t;
	register int	l;
#endif
	register int	nopen = 0;
	char		devname[32];

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

#ifndef	USE_PG_ONLY
	/*
	 * We need to find a fake bus number for the parallel port interface.
	 * Unfortunatly, the buscookie array may contain holes if
	 * SCSI_IOCTL_GET_BUS_NUMBER works, so we are searching backwards
	 * for some place for us.
	 * XXX Should add extra space in buscookies for a "PP bus".
	 */

	if (usallocal(usalp)->buscookies[MAX_SCG-1] != (short)-1)
		return (0);			/* No space for pgbus */

	for (b = MAX_SCG-1; b >= 0; b--) {
		if (usallocal(usalp)->buscookies[b] != (short)-1) {
			usallocal(usalp)->pgbus = ++b;
			break;
		}
	}
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"PP Bus: %d\n", usallocal(usalp)->pgbus);
	}
#else
	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);

		usallocal(usalp)->pgbus = -2;
		usallocal(usalp)->SCSIbuf = (char *)-1;

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}
#endif
	if (usallocal(usalp)->pgbus < 0)
		usallocal(usalp)->pgbus = 0;

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2))
		goto openbydev;

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {
#ifndef	USE_PG_ONLY
		if (usallocal(usalp)->pgbus != busno)
			return (0);
#endif
		snprintf(devname, sizeof (devname), "/dev/pg%d", tgt);
		f = sg_open_excl(devname, O_RDWR | O_NONBLOCK);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'", devname);
			return (0);
		}
		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
		return (1);
	} else {
		tlun = 0;
		for (tgt = 0; tgt < MAX_TGT; tgt++) {
			snprintf(devname, sizeof (devname), "/dev/pg%d", tgt);
			f = sg_open_excl(devname, O_RDWR | O_NONBLOCK);
			if (f < 0) {
				/*
				 * Set up error string but let us clear it later
				 * if at least one open succeeded.
				 */
				if (usalp->errstr)
					snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
							"Cannot open '/dev/pg*'");
				if (errno != ENOENT && errno != ENXIO && errno != ENODEV) {
					if (usalp->errstr)
						snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
							"Cannot open '%s'", devname);
					return (0);
				}
			} else {
				usallocal(usalp)->usalfiles[usallocal(usalp)->pgbus][tgt][tlun] = f;
				nopen++;
			}
		}
	}
	if (nopen > 0 && usalp->errstr)
		usalp->errstr[0] = '\0';

openbydev:
	if (device != NULL && *device != '\0') {
		char	*p;

		if (tlun < 0)
			return (0);
		f = open(device, O_RDWR | O_NONBLOCK);
/*		if (f < 0 && errno == ENOENT) {*/
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					device);
			return (0);
		}

		p = device + strlen(device) -1;
		tgt = *p - '0';
		if (tgt < 0 || tgt > 9)
			return (0);
		usallocal(usalp)->usalfiles[usallocal(usalp)->pgbus][tgt][tlun] = f;
		usal_settarget(usalp, usallocal(usalp)->pgbus, tgt, tlun);

		return (++nopen);
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
	if (usallocal(usalp)->pgbus < 0)
		return (0);
	b = usallocal(usalp)->pgbus;
	usallocal(usalp)->buscookies[b] = (short)-1;

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++) {
			f = usallocal(usalp)->usalfiles[b][t][l];
			if (f >= 0)
				close(f);
			usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
		}
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (PG_MAX_DATA);
}

#ifdef	USE_PG_ONLY

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	char    *ret;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	ret = valloc((size_t)(amt+getpagesize()));
	if (ret == NULL)
		return (ret);
	usalp->bufbase = ret;
	ret += getpagesize();
	usallocal(usalp)->SCSIbuf = ret;
	return ((void *)ret);

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
#endif	/* USE_PG_ONLY */

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (TRUE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	struct pg_write_hdr hdr = {PG_MAGIC, PG_RESET, 0};

	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	/*
	 * XXX Does this reset TGT or BUS ???
	 */
	return (write(usalp->fd, (char *)&hdr, sizeof (hdr)));

}

#ifndef MAX
#define	MAX(a, b)	((a) > (b) ? (a):(b))
#endif

#define	RHSIZE	sizeof (struct pg_read_hdr)
#define	WHSIZE  sizeof (struct pg_write_hdr)
#define	LEAD	MAX(RHSIZE, WHSIZE)

static int
do_usal_cmd(SCSI *usalp, struct usal_cmd *sp)
{

	char	local[LEAD+PG_MAX_DATA];
	int	use_local, i, r;
	int	inward = (sp->flags & SCG_RECV_DATA);

	struct pg_write_hdr *whp;
	struct pg_read_hdr  *rhp;
	char		    *dbp;

	if (sp->cdb_len > 12)
		comerrno(EX_BAD, "Can't do %d byte command.\n", sp->cdb_len);

	if (sp->addr == usallocal(usalp)->SCSIbuf) {
		use_local = 0;
		dbp = sp->addr;
	} else {
		use_local = 1;
		dbp = &local[LEAD];
		if (!inward)
			movebytes(sp->addr, dbp, sp->size);
	}

	whp = (struct pg_write_hdr *)(dbp - WHSIZE);
	rhp = (struct pg_read_hdr *)(dbp - RHSIZE);

	whp->magic   = PG_MAGIC;
	whp->func    = PG_COMMAND;
	whp->dlen    = sp->size;
	whp->timeout = sp->timeout;

	for (i = 0; i < 12; i++) {
		if (i < sp->cdb_len)
			whp->packet[i] = sp->cdb.cmd_cdb[i];
		else
			whp->packet[i] = 0;
	}

	i = WHSIZE;
	if (!inward)
		i += sp->size;

	r = write(usalp->fd, (char *)whp, i);

	if (r < 0) {				/* command was not sent */
		sp->ux_errno = geterrno();
		if (sp->ux_errno == ETIME) {
			/*
			 * If the previous command timed out, we cannot send
			 * any further command until the command in the drive
			 * is ready. So we behave as if the drive did not
			 * respond to the command.
			 */
			sp->error = SCG_FATAL;
			return (0);
		}
		return (-1);
	}

	if (r != i)
		errmsg("usalo_send(%s) wrote %d bytes (expected %d).\n",
			usalp->cmdname, r, i);

	sp->ux_errno = 0;
	sp->sense_count = 0;

	r = read(usalp->fd, (char *)rhp, RHSIZE+sp->size);

	if (r < 0) {
		sp->ux_errno = geterrno();
		if (sp->ux_errno == ETIME) {
			sp->error = SCG_TIMEOUT;
			return (0);
		}
		sp->error = SCG_FATAL;
		return (-1);
	}

	i = rhp->dlen;
	if (i > sp->size) {
		/*
		 * "DMA overrun" should be handled in the kernel.
		 * However this may happen with flaky PP adapters.
		 */
		errmsgno(EX_BAD,
			"DMA (read) overrun by %d bytes (requested %d bytes).\n",
			i - sp->size, sp->size);
		sp->resid = sp->size - i;
		sp->error = SCG_RETRYABLE;
		i = sp->size;
	} else {
		sp->resid = sp->size - i;
	}

	if (use_local && inward)
		movebytes(dbp, sp->addr, i);

	fillbytes(&sp->scb, sizeof (sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof (sp->u_sense.cmd_sense), '\0');

	sp->error = SCG_NO_ERROR;
	i = rhp->scsi?2:0;
/*	i = rhp->scsi;*/
	sp->u_scb.cmd_scb[0] = i;
	if (i & 2) {
		if (sp->ux_errno == 0)
			sp->ux_errno = EIO;
		/*
		 * If there is no DMA overrun and there is a
		 * SCSI Status byte != 0 then the SCSI cdb transport was OK
		 * and sp->error must be SCG_NO_ERROR.
		 */
/*		sp->error = SCG_RETRYABLE;*/
	}

	return (0);

}

static int
do_usal_sense(SCSI *usalp, struct usal_cmd *sp)
{
	int		ret;
	struct usal_cmd 	s_cmd;

	fillbytes((caddr_t)&s_cmd, sizeof (s_cmd), '\0');
	s_cmd.addr = (caddr_t)sp->u_sense.cmd_sense;
	s_cmd.size = sp->sense_len;
	s_cmd.flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	s_cmd.cdb_len = SC_G0_CDBLEN;
	s_cmd.sense_len = CCS_SENSE_LEN;
	s_cmd.cdb.g0_cdb.cmd = SC_REQUEST_SENSE;
	s_cmd.cdb.g0_cdb.lun = sp->cdb.g0_cdb.lun;
	s_cmd.cdb.g0_cdb.count = sp->sense_len;
	ret = do_usal_cmd(usalp, &s_cmd);

	if (ret < 0)
		return (ret);

	sp->sense_count = sp->sense_len - s_cmd.resid;
	return (ret);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	int	ret;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}
	ret = do_usal_cmd(usalp, sp);
	if (ret < 0)
		return (ret);
	if (sp->u_scb.cmd_scb[0] & 2)
		ret = do_usal_sense(usalp, sp);
	return (ret);
}

/* end of scsi-linux-pg.c */

#ifndef	USE_PG_ONLY

#undef	usalo_version
#undef	usalo_help
#undef	usalo_open
#undef	usalo_close
#undef	usalo_send
#undef	usalo_maxdma
#undef	usalo_initiator_id
#undef	usalo_isatapi
#undef	usalo_reset

#endif
