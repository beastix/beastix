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

/* @(#)scsi-aix.c	1.36 04/01/14 Copyright 1997 J. Schilling */
/*
 *	Interface for the AIX generic SCSI implementation.
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

#include <sys/scdisk.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-aix.c-1.36";	/* The version for this transport*/


#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local{
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local*)((p)->local))

#define	MAX_DMA_AIX (64*1024)

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
	__usal_help(f, "DKIOCMD", "SCSI transport for targets known by AIX drivers",
		"", "bus,target,lun or UNIX device", "1,2,0 or /dev/rcd0@", FALSE, TRUE);
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
	char		devname[32];

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

		snprintf(devname, sizeof (devname), "/dev/rcd%d", tgt);
		f = openx(devname, 0, 0, SC_DIAGNOSTIC);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'. Specify device number (1 for cd1) as target (1,0)",
					devname);
			return (0);
		}
		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
		return (1);
	} else {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Unable to scan on AIX");
		return (0);
	}
openbydev:
	if (device != NULL && *device != '\0' && busno >= 0 && tgt >= 0 && tlun >= 0) {
		f = openx(device, 0, 0, SC_DIAGNOSTIC);
		if (f < 0) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Cannot open '%s'",
					devname);
			return (0);
		}

		usallocal(usalp)->usalfiles[busno][tgt][tlun] = f;
		usal_settarget(usalp, busno, tgt, tlun);

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

	if (usalp->local== NULL)
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
	return (MAX_DMA_AIX);
}

#define	palign(x, a)	(((char *)(x)) + ((a) - 1 - (((UIntptr_t)((x)-1))%(a))))

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
/* assume having a modern AIX here */
#ifdef HAVE_ALLOCA_H
    usalp->bufbase = (void *)valloc((size_t)amt);
    return (usalp->bufbase);
#else
	void	*ret;
	int	pagesize = getpagesize();

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"usalo_getbuf: %ld bytes\n", amt);
	}
	/*
	 * Damn AIX is a paged system but has no valloc()
	 */
	usalp->bufbase = ret = malloc((size_t)(amt+pagesize));
	if (ret == NULL)
		return (ret);
	ret = palign(ret, pagesize);
	return (ret);
#endif
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
	/*
	 * XXX Does this reset TGT or BUS ???
	 */
	return (ioctl(usalp->fd, SCIORESET, IDLUN(usal_target(usalp), usal_lun(usalp))));
}

static int
do_usal_cmd(SCSI *usalp, struct usal_cmd *sp)
{
	struct sc_iocmd req;
	int	ret;

	if (sp->cdb_len > 12)
		comerrno(EX_BAD, "Can't do %d byte command.\n", sp->cdb_len);

	fillbytes(&req, sizeof (req), '\0');

	req.flags = SC_ASYNC;
	if (sp->flags & SCG_RECV_DATA) {
		req.flags |= B_READ;
	} else if (sp->size > 0) {
		req.flags |= B_WRITE;
	}
	req.data_length = sp->size;
	req.buffer = sp->addr;
	req.timeout_value = sp->timeout;
	req.command_length = sp->cdb_len;

	movebytes(&sp->cdb, req.scsi_cdb, 12);
	errno = 0;
	ret = ioctl(usalp->fd, DKIOCMD, &req);

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "ret: %d errno: %d (%s)\n", ret, errno, errmsgstr(errno));
		fprintf((FILE *)usalp->errfile, "data_length:     %d\n", req.data_length);
		fprintf((FILE *)usalp->errfile, "buffer:          0x%X\n", req.buffer);
		fprintf((FILE *)usalp->errfile, "timeout_value:   %d\n", req.timeout_value);
		fprintf((FILE *)usalp->errfile, "status_validity: %d\n", req.status_validity);
		fprintf((FILE *)usalp->errfile, "scsi_bus_status: 0x%X\n", req.scsi_bus_status);
		fprintf((FILE *)usalp->errfile, "adapter_status:  0x%X\n", req.adapter_status);
		fprintf((FILE *)usalp->errfile, "adap_q_status:   0x%X\n", req.adap_q_status);
		fprintf((FILE *)usalp->errfile, "q_tag_msg:       0x%X\n", req.q_tag_msg);
		fprintf((FILE *)usalp->errfile, "flags:           0X%X\n", req.flags);
	}
	if (ret < 0) {
		sp->ux_errno = geterrno();
		/*
		 * Check if SCSI command cound not be send at all.
		 */
		if (sp->ux_errno == ENOTTY || sp->ux_errno == ENXIO ||
		    sp->ux_errno == EINVAL || sp->ux_errno == EACCES) {
			return (-1);
		}
	} else {
		sp->ux_errno = 0;
	}
	ret = 0;
	sp->sense_count = 0;
	sp->resid = 0;		/* AIX is the same rubbish as Linux here */

	fillbytes(&sp->scb, sizeof (sp->scb), '\0');
	fillbytes(&sp->u_sense.cmd_sense, sizeof (sp->u_sense.cmd_sense), '\0');

	if (req.status_validity == 0) {
		sp->error = SCG_NO_ERROR;
		return (0);
	}
	if (req.status_validity & 1) {
		sp->u_scb.cmd_scb[0] = req.scsi_bus_status;
		sp->error = SCG_RETRYABLE;
	}
	if (req.status_validity & 2) {
		if (req.adapter_status & SC_NO_DEVICE_RESPONSE) {
			sp->error = SCG_FATAL;

		} else if (req.adapter_status & SC_CMD_TIMEOUT) {
			sp->error = SCG_TIMEOUT;

		} else if (req.adapter_status != 0) {
			sp->error = SCG_RETRYABLE;
		}
	}

	return (ret);
}

static int
do_usal_sense(SCSI *usalp, struct usal_cmd *sp)
{
	int		ret;
	struct usal_cmd	s_cmd;

	fillbytes((caddr_t)&s_cmd, sizeof (s_cmd), '\0');
	s_cmd.addr = sp->u_sense.cmd_sense;
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
	if (s_cmd.u_scb.cmd_scb[0] & 02) {
		/* XXX ??? Check condition on request Sense ??? */
	}
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
	if (sp->u_scb.cmd_scb[0] & 02)
		ret = do_usal_sense(usalp, sp);
	return (ret);
}
