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

/* @(#)scsi-vms.c	1.33 04/01/15 Copyright 1997 J. Schilling */
/*
 *	Interface for the VMS generic SCSI implementation.
 *
 *	The idea for an elegant mapping to VMS device dontroller names
 *	is from Chip Dancy Chip.Dancy@hp.com. This allows up to
 *	26 IDE controllers (DQ[A-Z][0-1]).
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

#include <iodef.h>
#include <ssdef.h>
#include <descrip.h>
#include <starlet.h>
#include <string.h>
#include <LIB$ROUTINES.H>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-vms.c-1.33";	/* The version for this transport*/

#define	VMS_MAX_DK	4		/* DK[A-D] VMS device controllers */
#define	VMS_MAX_GK	4		/* GK[A-D] VMS device controllers */
#define	VMS_MAX_DQ	26		/* DQ[A-Z] VMS device controllers */

#define	VMS_DKRANGE_MAX	VMS_MAX_DK
#define	VMS_GKRANGE_MAX	(VMS_DKRANGE_MAX + VMS_MAX_GK)
#define	VMS_DQRANGE_MAX	(VMS_GKRANGE_MAX + VMS_MAX_DQ)

#define	MAX_SCG 	VMS_DQRANGE_MAX	/* Max # of SCSI controllers */
#define	MAX_TGT 	16
#define	MAX_LUN 	8

#define	MAX_DMA_VMS	(63*1024)	/* Check if this is not too big */
#define	MAX_PHSTMO_VMS	300
#define	MAX_DSCTMO_VMS	((64*1024)-1)	/* max value for OpenVMS/AXP 7.1 ehh*/

/*
 * Define a mapping from the scsi busno to the three character
 * VMS device controller.
 * The valid busno values are broken into three ranges, one for each of
 * the three supported devices: dk, gk, and dq.
 * The vmschar[] and vmschar1[] arrays are subscripted by an offset
 * corresponding to each of the three ranges [0,1,2] to provide the
 * two characters of the VMS device.
 * The offset of the busno value within its range is used to define the
 * third character, using the vmschar2[] array.
 */
static	char	vmschar[]	= {'d', 'g', 'd'};
static	char	vmschar1[]	= {'k', 'k', 'q'};
static	char	vmschar2[]	= {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
				    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
				    'u', 'v', 'w', 'x', 'y', 'z'};


static	int	do_usal_cmd(SCSI *usalp, struct usal_cmd *sp);
static	int	do_usal_sense(SCSI *usalp, struct usal_cmd *sp);

#define	DEVICE_NAMELEN 8

struct SCSI$DESC {
	Uint	SCSI$L_OPCODE;		/* SCSI Operation Code */
	Uint	SCSI$L_FLAGS;		/* SCSI Flags Bit Map */
	char	*SCSI$A_CMD_ADDR;	/* ->SCSI command buffer */
	Uint	SCSI$L_CMD_LEN; 	/* SCSI command length, bytes */
	char	*SCSI$A_DATA_ADDR;	/* ->SCSI data buffer */
	Uint	SCSI$L_DATA_LEN;	/* SCSI data length, bytes */
	Uint	SCSI$L_PAD_LEN; 	/* SCSI pad length, bytes */
	Uint	SCSI$L_PH_CH_TMOUT;	/* SCSI phase change timeout */
	Uint	SCSI$L_DISCON_TMOUT;	/* SCSI disconnect timeout */
	Uint	SCSI$L_RES_1;		/* Reserved */
	Uint	SCSI$L_RES_2;		/* Reserved */
	Uint	SCSI$L_RES_3;		/* Reserved */
	Uint	SCSI$L_RES_4;		/* Reserved */
	Uint	SCSI$L_RES_5;		/* Reserved */
	Uint	SCSI$L_RES_6;		/* Reserved */
};

#ifdef __ALPHA
#pragma member_alignment save
#pragma nomember_alignment
#endif

struct SCSI$IOSB {
	Ushort	SCSI$W_VMS_STAT;	/* VMS status code */
	Ulong	SCSI$L_IOSB_TFR_CNT;	/* Actual #bytes transferred */
	char	SCSI$B_IOSB_FILL_1;
	Uchar	SCSI$B_IOSB_STS;	/* SCSI device status */
};

#ifdef __ALPHA
#pragma member_alignment restore
#endif

#define	SCSI$K_GOOD_STATUS		0
#define	SCSI$K_CHECK_CONDITION		0x2
#define	SCSI$K_CONDITION_MET		0x4
#define	SCSI$K_BUSY			0x8
#define	SCSI$K_INTERMEDIATE		0x10
#define	SCSI$K_INTERMEDIATE_C_MET	0x14
#define	SCSI$K_RESERVATION_CONFLICT	0x18
#define	SCSI$K_COMMAND_TERMINATED	0x22
#define	SCSI$K_QUEUE_FULL		0x28


#define	SCSI$K_WRITE		0X0	/* direction of transfer=write */
#define	SCSI$K_READ		0X1	/* direction of transfer=read */
#define	SCSI$K_FL_ENAB_DIS	0X2	/* enable disconnects */
#define	SCSI$K_FL_ENAB_SYNC	0X4	/* enable sync */
#define	GK_EFN			0	/* Event flag number */

static char	gk_device[8];		/* XXX JS hoffentlich gibt es keinen Ueberlauf */
static Ushort	gk_chan;
static Ushort	transfer_length;
static int	i;
static int	status;
static $DESCRIPTOR(gk_device_desc, gk_device);
static struct SCSI$IOSB gk_iosb;
static struct SCSI$DESC gk_desc;
static FILE *fp;


struct usal_local {
	Ushort	gk_chan;
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
	__usal_help(f, "IO$_DIAGNOSE", "Generic SCSI",
		"", "bus,target,lun", "1,2,0", FALSE, FALSE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
	int	tlun	= usal_lun(usalp);
	char	devname[DEVICE_NAMELEN];
	char	buschar;
	char	buschar1;
	char	buschar2;
	int	range;
	int	range_offset;

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
	if (busno < 0 || tgt < 0 || tlun < 0) {
		/*
		 * There is no real reason why we cannot scan on VMS,
		 * but for now it is not possible.
		 */
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Unable to scan on VMS");
		return (0);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);
	}

	if (busno < VMS_DKRANGE_MAX) {			/* in the dk range?   */
		range = 0;
		range_offset = busno;
	} else if (busno < VMS_GKRANGE_MAX) {		/* in the gk range?   */
		range = 1;
		range_offset = busno - VMS_DKRANGE_MAX;
	} else if (busno < VMS_DQRANGE_MAX) {		/* in the dq range?   */
		range = 2;
		range_offset = busno - VMS_GKRANGE_MAX;
	}
	buschar = vmschar[range];			/* get first device char*/
	buschar1 = vmschar1[range];			/* get 2nd device char*/
	buschar2 = vmschar2[range_offset];		/* get controller char*/

	snprintf(devname, sizeof (devname), "%c%c%c%d0%d:",
					buschar, buschar1, buschar2,
					tgt, tlun);
	strcpy(gk_device, devname);
	status = sys$assign(&gk_device_desc, &gk_chan, 0, 0);
	if (!(status & 1)) {
		fprintf((FILE *)usalp->errfile,
			"Unable to access scsi-device \"%s\"\n", &gk_device[0]);
		return (-1);
	}
	if (usalp->debug > 0) {
		fp = fopen("cdrecord_io.log", "w", "rfm=stmlf", "rat=cr");
		if (fp == NULL) {
			perror("Failing opening i/o-logfile");
			exit(SS$_NORMAL);
		}
	}
	return (status);
}

static int
usalo_close(SCSI *usalp)
{
	/*
	 * XXX close gk_chan ???
	 */
	/*
	 * sys$dassgn()
	 */

	status = sys$dassgn(gk_chan);

	return (status);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (MAX_DMA_VMS);
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	if (gk_chan == 0)
		return (FALSE);
	return (TRUE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (gk_chan == 0)
		return (-1);
	return (gk_chan);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	int	busno = usal_scsibus(usalp);

	if (busno >= 8)
		return (TRUE);

	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	errno = EINVAL;
	return (-1);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = malloc((size_t)(amt));	/* XXX JS XXX valloc() ??? */
	return (usalp->bufbase);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static int
do_usal_cmd(SCSI *usalp, struct usal_cmd *sp)
{
	char		*cmdadr;
	int		notcmdretry;
	int		len;
	Uchar		scsi_sts;
	int		severity;

	/* XXX JS XXX This cannot be OK */
	notcmdretry = (sp->flags & SCG_CMD_RETRY)^SCG_CMD_RETRY;
	/* error corrected ehh	*/
/*
 * XXX JS Wenn das notcmdretry Flag bei VMS auch 0x08 ist und Du darauf hoffst,
 * XXX	Dasz ich den Wert nie aendere, dann ist das richtig.
 * XXX Siehe unten: Das gleiche gilt fuer SCG_RECV_DATA und SCG_DISRE_ENA !!!
 */

	cmdadr = (char *)sp->cdb.cmd_cdb;
	/* XXX JS XXX This cannot be OK */
	gk_desc.SCSI$L_FLAGS = ((sp->flags & SCG_RECV_DATA) |
				(sp->flags & SCG_DISRE_ENA)|
				notcmdretry);
				/* XXX siehe oben, das ist ein bitweises oder!!! */
	gk_desc.SCSI$A_DATA_ADDR = sp->addr;
	gk_desc.SCSI$L_DATA_LEN = sp->size;
	gk_desc.SCSI$A_CMD_ADDR = cmdadr;
	gk_desc.SCSI$L_CMD_LEN = sp->cdb_len;
	gk_desc.SCSI$L_PH_CH_TMOUT = sp->timeout;
	gk_desc.SCSI$L_DISCON_TMOUT = sp->timeout;
	if (gk_desc.SCSI$L_PH_CH_TMOUT > MAX_PHSTMO_VMS)
	    gk_desc.SCSI$L_PH_CH_TMOUT = MAX_PHSTMO_VMS;
	if (gk_desc.SCSI$L_DISCON_TMOUT > MAX_DSCTMO_VMS)
	    gk_desc.SCSI$L_DISCON_TMOUT = MAX_DSCTMO_VMS;
	gk_desc.SCSI$L_OPCODE = 1;	/* SCSI Operation Code */
	gk_desc.SCSI$L_PAD_LEN = 0;	/* SCSI pad length, bytes */
	gk_desc.SCSI$L_RES_1 = 0;	/* Reserved */
	gk_desc.SCSI$L_RES_2 = 0;	/* Reserved */
	gk_desc.SCSI$L_RES_3 = 0;	/* Reserved */
	gk_desc.SCSI$L_RES_4 = 0;	/* Reserved */
	gk_desc.SCSI$L_RES_5 = 0;	/* Reserved */
	gk_desc.SCSI$L_RES_6 = 0;	/* Reserved */
	if (usalp->debug > 0) {
		fprintf(fp, "***********************************************************\n");
		fprintf(fp, "SCSI VMS-I/O parameters\n");
		fprintf(fp, "OPCODE: %d", gk_desc.SCSI$L_OPCODE);
		fprintf(fp, " FLAGS: %d\n", gk_desc.SCSI$L_FLAGS);
		fprintf(fp, "CMD:");
		for (i = 0; i < gk_desc.SCSI$L_CMD_LEN; i++) {
			fprintf(fp, "%x ", sp->cdb.cmd_cdb[i]);
		}
		fprintf(fp, "\n");
		fprintf(fp, "DATA_LEN: %d\n", gk_desc.SCSI$L_DATA_LEN);
		fprintf(fp, "PH_CH_TMOUT: %d", gk_desc.SCSI$L_PH_CH_TMOUT);
		fprintf(fp, " DISCON_TMOUT: %d\n", gk_desc.SCSI$L_DISCON_TMOUT);
	}
	status = sys$qiow(GK_EFN, gk_chan, IO$_DIAGNOSE, &gk_iosb, 0, 0,
			&gk_desc, sizeof (gk_desc), 0, 0, 0, 0);


	if (usalp->debug > 0) {
		fprintf(fp, "qiow-status: %i\n", status);
		fprintf(fp, "VMS status code %i\n", gk_iosb.SCSI$W_VMS_STAT);
		fprintf(fp, "Actual #bytes transferred %i\n", gk_iosb.SCSI$L_IOSB_TFR_CNT);
		fprintf(fp, "SCSI device status %i\n", gk_iosb.SCSI$B_IOSB_STS);
		if (gk_iosb.SCSI$L_IOSB_TFR_CNT != gk_desc.SCSI$L_DATA_LEN) {
			fprintf(fp, "#bytes transferred != DATA_LEN\n");
		}
	}

	if (!(status & 1)) {		/* Fehlerindikation fuer sys$qiow() */
		sp->ux_errno = geterrno();
		/* schwerwiegender nicht SCSI bedingter Fehler => return (-1) */
		if (sp->ux_errno == ENOTTY || sp->ux_errno == ENXIO ||
		    sp->ux_errno == EINVAL || sp->ux_errno == EACCES) {
			return (-1);
		}
		if (sp->ux_errno == 0)
			sp->ux_errno == EIO;
	} else {
		sp->ux_errno = 0;
	}

	sp->resid = gk_desc.SCSI$L_DATA_LEN - gk_iosb.SCSI$L_IOSB_TFR_CNT;

	if (usalo_isatapi(usalp)) {
		scsi_sts = ((gk_iosb.SCSI$B_IOSB_STS >> 4) & 0x7);
	} else {
		scsi_sts = gk_iosb.SCSI$B_IOSB_STS;
	}

	if (gk_iosb.SCSI$W_VMS_STAT == SS$_NORMAL && scsi_sts == 0) {
		sp->error = SCG_NO_ERROR;
		if (usalp->debug > 0) {
			fprintf(fp, "scsi_sts == 0\n");
			fprintf(fp, "gk_iosb.SCSI$B_IOSB_STS == 0\n");
			fprintf(fp, "sp->error %i\n", sp->error);
			fprintf(fp, "sp->resid %i\n", sp->resid);
		}
		return (0);
	}

	severity = gk_iosb.SCSI$W_VMS_STAT & 0x7;

	if (severity == 4) {
		sp->error = SCG_FATAL;
		if (usalp->debug > 0) {
			fprintf(fp, "scsi_sts & 2\n");
			fprintf(fp, "gk_iosb.SCSI$B_IOSB_STS & 2\n");
			fprintf(fp, "gk_iosb.SCSI$W_VMS_STAT & 0x7 == SS$_FATAL\n");
			fprintf(fp, "sp->error %i\n", sp->error);
		}
		return (0);
	}
	if (gk_iosb.SCSI$W_VMS_STAT == SS$_TIMEOUT) {
		sp->error = SCG_TIMEOUT;
		if (usalp->debug > 0) {
			fprintf(fp, "scsi_sts & 2\n");
			fprintf(fp, "gk_iosb.SCSI$B_IOSB_STS & 2\n");
			fprintf(fp, "gk_iosb.SCSI$W_VMS_STAT == SS$_TIMEOUT\n");
			fprintf(fp, "sp->error %i\n", sp->error);
		}
		return (0);
	}
	sp->error = SCG_RETRYABLE;
	sp->u_scb.cmd_scb[0] = scsi_sts;
	if (usalp->debug > 0) {
		fprintf(fp, "scsi_sts & 2\n");
		fprintf(fp, "gk_iosb.SCSI$B_IOSB_STS & 2\n");
		fprintf(fp, "gk_iosb.SCSI$W_VMS_STAT != 0\n");
		fprintf(fp, "sp->error %i\n", sp->error);
	}
	return (0);
}

static int
do_usal_sense(SCSI *usalp, struct usal_cmd *sp)
{
	int		ret;
	struct usal_cmd	s_cmd;

	fillbytes((caddr_t)&s_cmd, sizeof (s_cmd), '\0');
	s_cmd.addr = (char *)sp->u_sense.cmd_sense;
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
