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

/* @(#)scsi-osf.c	1.26 04/01/15 Copyright 1998 J. Schilling */
/*
 *	Interface for Digital UNIX (OSF/1 generic SCSI implementation (/dev/cam).
 *
 *	Created out of the hacks from:
 *		Stefan Traby <stefan@sime.com> and
 *		Bruno Achauer <bruno@tk.uni-linz.ac.at>
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1998 J. Schilling
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

#include <sys/types.h>
#include <io/common/iotypes.h>
#include <io/cam/cam.h>
#include <io/cam/uagt.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-osf.c-1.26";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	int	usalfile;	/* Used for ioctl()	*/
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

static	BOOL	scsi_checktgt(SCSI *usalp, int f, int busno, int tgt, int tlun);

/*
 * I don't have any documentation about CAM
 */
#define	MAX_DMA_OSF_CAM	(64*1024)

#ifndef	AUTO_SENSE_LEN
#	define	AUTO_SENSE_LEN	32	/* SCG_MAX_SENSE */
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
	__usal_help(f, "CAM", "Generic transport independent SCSI (Common Access Method)",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
		int	tlun	= usal_lun(usalp);
	register int	b;
	register int	t;
	register int	l;

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
		usallocal(usalp)->usalfile = -1;

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = 0;
			}
		}
	}

	if (usallocal(usalp)->usalfile != -1)	/* multiple opens ??? */
		return (1);			/* not yet ready .... */

	if ((usallocal(usalp)->usalfile = open("/dev/cam", O_RDWR, 0)) < 0) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Cannot open '/dev/cam'");
		return (-1);
	}

	if (busno >= 0 && tgt >= 0 && tlun >= 0) {
		/* scsi_checktgt() ??? */
		for (l = 0; l < MAX_LUN; l++)
			usallocal(usalp)->usalfiles[b][t][l] = 1;
		return (1);
	}
	/*
	 * There seems to be no clean way to check whether
	 * a SCSI bus is present in the current system.
	 * scsi_checktgt() is used as a workaround for this problem.
	 */
	for (b = 0; b < MAX_SCG; b++) {
		for (t = 0; t < MAX_TGT; t++) {
			if (scsi_checktgt(usalp, usallocal(usalp)->usalfile, b, t, 0)) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = 1;
				/*
				 * Found a target on this bus.
				 * Comment the 'break' for a complete scan.
				 */
				break;
			}
		}
	}
	return (1);
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

/*
 * We send a test unit ready command to the target to check whether the
 * OS is considering this target to be valid.
 * XXX Is this really needed? We should rather let the cmd fail later.
 */
static BOOL
scsi_checktgt(SCSI *usalp, int f, int busno, int tgt, int tlun)
{
	struct usal_cmd	*sp = usalp->scmd;
	struct usal_cmd	sc;
	int	ret;
	int	ofd  = usalp->fd;
	int	obus = usal_scsibus(usalp);
	int	otgt = usal_target(usalp);
	int	olun = usal_lun(usalp);

	usal_settarget(usalp, busno, tgt, tlun);
	usalp->fd = f;

	sc = *sp;
	fillbytes((caddr_t)sp, sizeof (*sp), '\0');
	sp->addr = (caddr_t)0;
	sp->size = 0;
	sp->flags = SCG_DISRE_ENA | SCG_SILENT;
	sp->cdb_len = SC_G0_CDBLEN;
	sp->sense_len = CCS_SENSE_LEN;
	sp->cdb.g0_cdb.cmd = SC_TEST_UNIT_READY;
	sp->cdb.g0_cdb.lun = usal_lun(usalp);

	usalo_send(usalp);
	usal_settarget(usalp, obus, otgt, olun);
	usalp->fd = ofd;

	if (sp->error != SCG_FATAL)
		return (TRUE);
	ret = sp->ux_errno != EINVAL;
	*sp = sc;
	return (ret);
}


static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long maxdma = MAX_DMA_OSF_CAM;

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

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (usalp->local == NULL)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		if (usallocal(usalp)->usalfiles[busno][t][0] != 0)
			return (TRUE);
	}
	return (FALSE);
}


static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (usalp->local == NULL)
		return (-1);

	return ((busno < 0 || busno >= MAX_SCG) ? -1 : usallocal(usalp)->usalfile);
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
	errno = EINVAL;
	return (-1);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	CCB_SCSIIO	ccb;
	UAGT_CAM_CCB	ua;
	unsigned char	*cdb;
	CCB_RELSIM	relsim;
	UAGT_CAM_CCB	relua;
	int		i;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	fillbytes(&ua, sizeof (UAGT_CAM_CCB), 0);
	fillbytes(&ccb, sizeof (CCB_SCSIIO), 0);

	ua.uagt_ccb = (CCB_HEADER *) &ccb;
	ua.uagt_ccblen = sizeof (CCB_SCSIIO);
	ccb.cam_ch.my_addr = (CCB_HEADER *) &ccb;
	ccb.cam_ch.cam_ccb_len = sizeof (CCB_SCSIIO);

	ua.uagt_snsbuf = ccb.cam_sense_ptr = sp->u_sense.cmd_sense;
	ua.uagt_snslen = ccb.cam_sense_len = AUTO_SENSE_LEN;

	cdb = (unsigned char *) ccb.cam_cdb_io.cam_cdb_bytes;

	ccb.cam_timeout = sp->timeout;

	ccb.cam_data_ptr = ua.uagt_buffer = (Uchar *) sp->addr;
	ccb.cam_dxfer_len = ua.uagt_buflen = sp->size;
	ccb.cam_ch.cam_func_code = XPT_SCSI_IO;
	ccb.cam_ch.cam_flags = 0;	/* CAM_DIS_CALLBACK; */

	if (sp->size == 0) {
		ccb.cam_data_ptr = ua.uagt_buffer = (Uchar *) NULL;
		ccb.cam_ch.cam_flags |= CAM_DIR_NONE;
	} else {
		if (sp->flags & SCG_RECV_DATA) {
			ccb.cam_ch.cam_flags |= CAM_DIR_IN;
		} else {
			ccb.cam_ch.cam_flags |= CAM_DIR_OUT;
		}
	}

	ccb.cam_cdb_len = sp->cdb_len;
	for (i = 0; i < sp->cdb_len; i++)
		cdb[i] = sp->cdb.cmd_cdb[i];

	ccb.cam_ch.cam_path_id	  = usal_scsibus(usalp);
	ccb.cam_ch.cam_target_id  = usal_target(usalp);
	ccb.cam_ch.cam_target_lun = usal_lun(usalp);

	sp->sense_count = 0;
	sp->ux_errno = 0;
	sp->error = SCG_NO_ERROR;


	if (ioctl(usalp->fd, UAGT_CAM_IO, (caddr_t) &ua) < 0) {
		sp->ux_errno = geterrno();
		sp->error = SCG_FATAL;
		if (usalp->debug > 0) {
			errmsg("ioctl(fd, UAGT_CAM_IO, dev=%d,%d,%d) failed.\n",
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
		}
		return (0);
	}
	if (usalp->debug > 0) {
		errmsgno(EX_BAD, "cam_status = 0x%.2X scsi_status = 0x%.2X dev=%d,%d,%d\n",
					ccb.cam_ch.cam_status,
					ccb.cam_scsi_status,
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
		fflush(stderr);
	}
	switch (ccb.cam_ch.cam_status & CAM_STATUS_MASK) {

	case CAM_REQ_CMP:	break;

	case CAM_SEL_TIMEOUT:	sp->error = SCG_FATAL;
				sp->ux_errno = EIO;
				break;

	case CAM_CMD_TIMEOUT:	sp->error = SCG_TIMEOUT;
				sp->ux_errno = EIO;
				break;

	default:		sp->error = SCG_RETRYABLE;
				sp->ux_errno = EIO;
				break;
	}

	sp->u_scb.cmd_scb[0] = ccb.cam_scsi_status;

	if (ccb.cam_ch.cam_status & CAM_AUTOSNS_VALID) {
		sp->sense_count = MIN(ccb.cam_sense_len - ccb.cam_sense_resid,
			SCG_MAX_SENSE);
		sp->sense_count = MIN(sp->sense_count, sp->sense_len);
		if (sp->sense_len < 0)
			sp->sense_count = 0;
	}
	sp->resid = ccb.cam_resid;


	/*
	 * this is perfectly wrong.
	 * But without this, we hang...
	 */
	if (ccb.cam_ch.cam_status & CAM_SIM_QFRZN) {
		fillbytes(&relsim, sizeof (CCB_RELSIM), 0);
		relsim.cam_ch.cam_ccb_len = sizeof (CCB_SCSIIO);
		relsim.cam_ch.cam_func_code = XPT_REL_SIMQ;
		relsim.cam_ch.cam_flags = CAM_DIR_IN | CAM_DIS_CALLBACK;
		relsim.cam_ch.cam_path_id	= usal_scsibus(usalp);
		relsim.cam_ch.cam_target_id	= usal_target(usalp);
		relsim.cam_ch.cam_target_lun	= usal_lun(usalp);

		relua.uagt_ccb = (struct ccb_header *) & relsim;	/* wrong cast */
		relua.uagt_ccblen = sizeof (relsim);
		relua.uagt_buffer = NULL;
		relua.uagt_buflen = 0;

		if (ioctl(usalp->fd, UAGT_CAM_IO, (caddr_t) & relua) < 0)
			errmsg("DEC CAM -> LMA\n");
	}
	return (0);
}
