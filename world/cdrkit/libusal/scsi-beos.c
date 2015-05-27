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

/* @(#)scsi-beos.c	1.23 06/02/05 Copyright 1998 J. Schilling */
/*
 *	Interface for the BeOS user-land raw SCSI implementation.
 *
 *	This is a hack, that tries to emulate the functionality
 *	of the usal driver.
 *
 *	First version done by swetland@be.com
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


/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-beos.c-1.23";	/* The version for this transport*/

/*
 * There are also defines for:
 *	B_BEOS_VERSION_4
 *	B_BEOS_VERSION_4_5
 *
 * in BeOS 5
 */
#ifndef	B_BEOS_VERSION_5
/*
 * New BeOS seems to include <be/kernel/OS.h> from device/scsi.h
 */

/* nasty hack to avoid broken def of bool in SupportDefs.h */
#define	_SUPPORT_DEFS_H

#ifndef _SYS_TYPES_H
typedef unsigned long			ulong;
typedef unsigned int			uint;
typedef unsigned short			ushort;
#endif	/* _SYS_TYPES_H */

#include <BeBuild.h>
#include <sys/types.h>
#include <Errors.h>


/*-------------------------------------------------------------*/
/*----- Shorthand type formats --------------------------------*/

typedef	signed char			int8;
typedef unsigned char			uint8;
typedef volatile signed char		vint8;
typedef volatile unsigned char		vuint8;

typedef	short				int16;
typedef unsigned short			uint16;
typedef volatile short			vint16;
typedef volatile unsigned short		vuint16;

typedef	long				int32;
typedef unsigned long			uint32;
typedef volatile long			vint32;
typedef volatile unsigned long		vuint32;

typedef	long long			int64;
typedef unsigned long long		uint64;
typedef volatile long long		vint64;
typedef volatile unsigned long long	vuint64;

typedef volatile long			vlong;
typedef volatile int			vint;
typedef volatile short			vshort;
typedef volatile char			vchar;

typedef volatile unsigned long		vulong;
typedef volatile unsigned int		vuint;
typedef volatile unsigned short		vushort;
typedef volatile unsigned char		vuchar;

typedef unsigned char			uchar;
typedef unsigned short			unichar;



/*-------------------------------------------------------------*/
/*----- Descriptive formats -----------------------------------*/
typedef int32					status_t;
typedef int64					bigtime_t;
typedef uint32					type_code;
typedef uint32					perform_code;

/* end nasty hack */

#endif	/* ! B_BEOS_VERSION_5 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <usal/usalio.h>

/* this is really really dumb (tm) */
/*#undef sense*/
/*#undef scb*/
#include <device/scsi.h>

#undef bool
#include <drivers/CAM.h>

struct _fdmap_ {
	struct _fdmap_ *next;
	int bus;
	int targ;
	int lun;
	int fd;
};

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
	__usal_help(f, "CAM", "Generic transport independent SCSI (BeOS CAM variant)",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
#ifdef	nonono
	int	tlun	= usal_lun(usalp);
#endif

#ifdef	nonono
	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}
#endif

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
	}
	return (1);
}

static int
usalo_close(SCSI *usalp)
{
	struct _fdmap_	*f;
	struct _fdmap_	*fnext;

	for (f = (struct _fdmap_ *)usalp->local; f; f = fnext) {
		usalp->local = 0;
		fnext = f->next;
		close(f->fd);
		free(f);
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (128*1024);
	return (256*1024);
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
	struct stat	sb;
	char		buf[128];

	if (busno < 8)
		snprintf(buf, sizeof (buf), "/dev/bus/scsi/%d", busno);
	else
		snprintf(buf, sizeof (buf), "/dev/disk/ide/atapi/%d", busno-8);
	if (stat(buf, &sb))
		return (FALSE);
	return (TRUE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	struct _fdmap_	*f;
	char		buf[128];
	int		fd;

	for (f = (struct _fdmap_ *)usalp->local; f; f = f->next) {
		if (f->bus == busno && f->targ == tgt && f->lun == tlun)
			return (f->fd);
	}
	if (busno < 8) {
		snprintf(buf, sizeof (buf),
					"/dev/bus/scsi/%d/%d/%d/raw",
					busno, tgt, tlun);
	} else {
		char *tgtstr = (tgt == 0) ? "master" : (tgt == 1) ? "slave" : "dummy";
		snprintf(buf, sizeof (buf),
					"/dev/disk/ide/atapi/%d/%s/%d/raw",
					busno-8, tgtstr, tlun);
	}
	fd = open(buf, 0);

	if (fd >= 0) {
		f = (struct _fdmap_ *) malloc(sizeof (struct _fdmap_));
		f->bus = busno;
		f->targ = tgt;
		f->lun = tlun;
		f->fd = fd;
		f->next = (struct _fdmap_ *)usalp->local;
		usalp->local = f;
	}
	return (fd);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	/*
	 * XXX Should check for ATAPI
	 */
	return (-1);
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
	struct usal_cmd		*sp = usalp->scmd;
	int			e;
	int 			scsi_error;
	int			cam_error;
	raw_device_command	rdc;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	memcpy(rdc.command, &(sp->cdb), 12);
	rdc.command_length = sp->cdb_len;
	rdc.data = sp->addr;
	rdc.data_length = sp->size;
	rdc.sense_data_length = sp->sense_len;
	rdc.sense_data = sp->u_sense.cmd_sense;
	rdc.flags = sp->flags & SCG_RECV_DATA ? B_RAW_DEVICE_DATA_IN : 0;
	if (sp->size > 0)
		rdc.flags |= B_RAW_DEVICE_REPORT_RESIDUAL;
	rdc.timeout = sp->timeout * 1000000;

	sp->error		= SCG_NO_ERROR;
	sp->sense_count		= 0;
	sp->u_scb.cmd_scb[0]	= 0;
	sp->resid		= 0;

	if (usalp->debug > 0) {
		fprintf(stderr, "SEND(%d): cmd %02x, cdblen = %d, datalen = %ld, senselen = %ld\n",
			usalp->fd, rdc.command[0], rdc.command_length,
			rdc.data_length, rdc.sense_data_length);
	}
	e = ioctl(usalp->fd, B_RAW_DEVICE_COMMAND, &rdc, sizeof (rdc));
	if (usalp->debug > 0) {
		fprintf(stderr, "SEND(%d): -> %d CAM Status %02X SCSI status %02X\n", e, rdc.cam_status, rdc.scsi_status);
	}
	sp->ux_errno = 0;
#ifdef	DEBUG
	fprintf(stderr, "err %d errno %x CAM %X SL %d DL %d/%d FL %X\n",
		e, geterrno(), rdc.cam_status,
		rdc.sense_data_length, rdc.data_length, sp->size, rdc.flags);
#endif
	if (!e) {
		cam_error = rdc.cam_status;
		scsi_error = rdc.scsi_status;
		sp->u_scb.cmd_scb[0] = scsi_error;
		if (sp->size > 0)
			sp->resid = sp->size - rdc.data_length;

		switch (cam_error & CAM_STATUS_MASK) {

		case CAM_REQ_CMP:
			sp->error = SCG_NO_ERROR;
			break;

		case CAM_REQ_CMP_ERR:
			sp->sense_count = sp->sense_len; /* XXX */
			sp->error = SCG_NO_ERROR;
			sp->ux_errno = EIO;
			break;

		case CAM_CMD_TIMEOUT:
			sp->error = SCG_TIMEOUT;
			sp->ux_errno = EIO;

		case CAM_SEL_TIMEOUT:
			sp->error = SCG_FATAL;
			sp->ux_errno = EIO;
			break;

		default:
			sp->error = SCG_RETRYABLE;
			sp->ux_errno = EIO;
		}
	} else {
		sp->error = SCG_FATAL;
		sp->ux_errno = geterrno();
		sp->resid = sp->size;
	}
	return (0);
}
