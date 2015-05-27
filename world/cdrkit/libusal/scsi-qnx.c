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

/* @(#)scsi-qnx.c	1.3 04/01/15 Copyright 1998-2003 J. Schilling */
/*
 *	Interface for QNX (Neutrino generic SCSI implementation).
 *	First version adopted from the OSF-1 version by
 *	Kevin Chiles <kchiles@qnx.com>
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1998-2003 J. Schilling
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

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/dcmd_cam.h>
#include <sys/cam_device.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-qnx.c-1.3";	/* The version for this transport*/

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	int		fd;
};

#define	usallocal(p)	((struct usal_local *)((p)->local))
#define	QNX_CAM_MAX_DMA	(32*1024)

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
			return ("Initial Version adopted from OSF-1 by QNX-people");
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
	int fd;

	if (device == NULL || *device == '\0') {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"'devname' must be specified on this OS");
		return (-1);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);
		usallocal(usalp)->fd = -1;
	}

	if (usallocal(usalp)->fd != -1)	/* multiple open? */
		return (1);

	if ((usallocal(usalp)->fd = open(device, O_RDONLY, 0)) < 0) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Cannot open '%s'", device);
		return (-1);
	}

	usal_settarget(usalp, 0, 0, 0);

	return (1);
}

static int
usalo_close(SCSI *usalp)
{
	if (usalp->local == NULL)
		return (-1);

	if (usallocal(usalp)->fd >= 0)
		close(usallocal(usalp)->fd);
	usallocal(usalp)->fd = -1;
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long maxdma = QNX_CAM_MAX_DMA;

	return (maxdma);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	void	*addr;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "usalo_getbuf: %ld bytes\n", amt);
	}

	if ((addr = mmap(NULL, amt, PROT_READ | PROT_WRITE | PROT_NOCACHE,
						MAP_ANON | MAP_PHYS | MAP_NOX64K, NOFD, 0)) == MAP_FAILED) {
		return (NULL);
	}

	usalp->bufbase = addr;
	return (addr);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		munmap(usalp->bufbase, QNX_CAM_MAX_DMA);
	usalp->bufbase = NULL;
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	return (FALSE);
}


static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (usalp->local == NULL)
		return (-1);

	return ((busno < 0 || busno >= MAX_SCG) ? -1 : usallocal(usalp)->fd);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	cam_devinfo_t	cinfo;

	if (devctl(usalp->fd, DCMD_CAM_DEVINFO, &cinfo, sizeof (cinfo), NULL) != EOK) {
		return (TRUE);		/* default to ATAPI */
	}
	return ((cinfo.flags & DEV_ATAPI) ? TRUE : FALSE);
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
	int		i;
	struct usal_cmd	*sp;
	int		icnt;
	iov_t   	iov[3];
	CAM_PASS_THRU	cpt;

	icnt	= 1;
	sp	= usalp->scmd;
	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	memset(&cpt, 0, sizeof (cpt));

	sp->sense_count	= 0;
	sp->ux_errno	= 0;
	sp->error	= SCG_NO_ERROR;
	cpt.cam_timeout	= sp->timeout;
	cpt.cam_cdb_len = sp->cdb_len;
	memcpy(cpt.cam_cdb, sp->cdb.cmd_cdb, sp->cdb_len);

	if (sp->sense_len != -1) {
		cpt.cam_sense_len	= sp->sense_len;
		cpt.cam_sense_ptr	= sizeof (cpt);	/* XXX Offset from start of struct to data ??? */
		icnt++;
	} else {
		cpt.cam_flags |= CAM_DIS_AUTOSENSE;
	}

	if (cpt.cam_dxfer_len = sp->size) {
		icnt++;
		cpt.cam_data_ptr	= (paddr_t)sizeof (cpt) + cpt.cam_sense_len;
		if (sp->flags & SCG_RECV_DATA) {
			cpt.cam_flags |= CAM_DIR_IN;
		} else {
			cpt.cam_flags |= CAM_DIR_OUT;
		}
	} else {
		cpt.cam_flags |= CAM_DIR_NONE;
	}

	SETIOV(&iov[0], &cpt, sizeof (cpt));
	SETIOV(&iov[1], sp->u_sense.cmd_sense, cpt.cam_sense_len);
	SETIOV(&iov[2], sp->addr, sp->size);
	if (devctlv(usallocal(usalp)->fd, DCMD_CAM_PASS_THRU, icnt, icnt, iov, iov, NULL)) {
		sp->ux_errno = geterrno();
		sp->error = SCG_FATAL;
		if (usalp->debug > 0) {
			errmsg("cam_io failed\n");
		}
		return (0);
	}

	sp->resid		= cpt.cam_resid;
	sp->u_scb.cmd_scb[0]	= cpt.cam_scsi_status;

	switch (cpt.cam_status & CAM_STATUS_MASK) {
		case CAM_REQ_CMP:
			break;

		case CAM_SEL_TIMEOUT:
			sp->error	= SCG_FATAL;
			sp->ux_errno	= EIO;
			break;

		case CAM_CMD_TIMEOUT:
			sp->error	= SCG_TIMEOUT;
			sp->ux_errno	= EIO;
			break;

		default:
			sp->error	= SCG_RETRYABLE;
			sp->ux_errno	= EIO;
			break;
	}

	if (cpt.cam_status & CAM_AUTOSNS_VALID) {
		sp->sense_count = min(cpt.cam_sense_len - cpt.cam_sense_resid,
							SCG_MAX_SENSE);
		sp->sense_count = min(sp->sense_count, sp->sense_len);
		if (sp->sense_len < 0)
			sp->sense_count = 0;
	}

	return (0);
}
