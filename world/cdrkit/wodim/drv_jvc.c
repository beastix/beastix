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

/** @(#)drv_jvc.c	1.82 05/05/16 Copyright 1997-2005 J. Schilling */
/*
 *	CDR device implementation for
 *	JVC/TEAC
 *
 *	Copyright (c) 1997-2005 J. Schilling
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
/*#define	XXDEBUG*/
/*#define	XXBUFFER*/

#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <unixstd.h>
#ifdef	XXDEBUG
#include <stdxlib.h>
#endif

#include <utypes.h>
#include <btorder.h>
#include <intcvt.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "wodim.h"

/* just a hack */
long	lba_addr;
BOOL	last_done;

/*
 * macros for building MSF values from LBA
 */
#define	LBA_MIN(x)	((x)/(60*75))
#define	LBA_SEC(x)	(((x)%(60*75))/75)
#define	LBA_FRM(x)	((x)%75)
#define	MSF_CONV(a)	((((a)%(unsigned)100)/10)*16 + ((a)%(unsigned)10))

extern	int	lverbose;

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */
struct teac_mode_page_21 {		/* teac dummy selection */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x01 = 1 Byte */
	Ucbit	dummy		: 2;
	Ucbit	res		: 6;
};
#else
struct teac_mode_page_21 {		/* teac dummy selection */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x01 = 1 Byte */
	Ucbit	res		: 6;
	Ucbit	dummy		: 2;
};
#endif

struct teac_mode_page_31 {		/* teac speed selection */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x02 = 2 Byte */
	Uchar	speed;
	Uchar	res;
};

struct cdd_52x_mode_data {
	struct scsi_mode_header	header;
	union cdd_pagex	{
		struct teac_mode_page_21	teac_page21;
		struct teac_mode_page_31	teac_page31;
	} pagex;
};

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct pgm_subcode {		/* subcode for progam area */
	Uchar	subcode;
	Ucbit	addr		: 4;
	Ucbit	control		: 4;
	Uchar	track;
	Uchar	index;
};

#else

struct pgm_subcode {		/* subcode for progam area */
	Uchar	subcode;
	Ucbit	control		: 4;
	Ucbit	addr		: 4;
	Uchar	track;
	Uchar	index;
};

#endif

#define	set_pgm_subcode(sp, t, c, a, tr, idx)		(\
			(sp)->subcode = (t),		 \
			(sp)->control = (c),		 \
			(sp)->addr = (a),		 \
			(sp)->track = MSF_CONV(tr),	 \
			(sp)->index = (idx))

#define	SC_P		1	/* Subcode defines pre-gap (Pause)	*/
#define	SC_TR		0	/* Subcode defines track data		*/

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

typedef struct lin_subcode {	/* subcode for lead in area */
	Ucbit	addr		: 4;
	Ucbit	control		: 4;
	Uchar	track;
	Uchar	msf[3];
} lsc_t;

#else

typedef struct lin_subcode {	/* subcode for lead in area */
	Ucbit	control		: 4;
	Ucbit	addr		: 4;
	Uchar	track;
	Uchar	msf[3];
} lsc_t;

#endif

#define	set_toc_subcode(sp, c, a, tr, bno)				(\
			((lsc_t *)sp)->control = (c),			 \
			((lsc_t *)sp)->addr = (a),			 \
			((lsc_t *)sp)->track = MSF_CONV(tr),		 \
			((lsc_t *)sp)->msf[0] = MSF_CONV(LBA_MIN(bno)),	 \
			((lsc_t *)sp)->msf[1] = MSF_CONV(LBA_SEC(bno)),	 \
			((lsc_t *)sp)->msf[2] = MSF_CONV(LBA_FRM(bno)),	 \
			&((lsc_t *)sp)->msf[3])

#define	set_lin_subcode(sp, c, a, pt, min, sec, frm)			(\
			((lsc_t *)sp)->control = (c),			 \
			((lsc_t *)sp)->addr = (a),			 \
			((lsc_t *)sp)->track = (pt),			 \
			((lsc_t *)sp)->msf[0] = (min),			 \
			((lsc_t *)sp)->msf[1] = (sec),			 \
			((lsc_t *)sp)->msf[2] = (frm),			 \
			&((lsc_t *)sp)->msf[3])

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct upc_subcode {		/* subcode for upc/bar code */
	Uchar	res;
	Ucbit	addr		: 4;
	Ucbit	control		: 4;
	Uchar	upc[13];
};

#else

struct upc_subcode {		/* subcode for upc/bar code */
	Uchar	res;
	Ucbit	control		: 4;
	Ucbit	addr		: 4;
	Uchar	upc[13];
};

#endif

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct isrc_subcode {		/* subcode for ISRC code */
	Uchar	res;
	Ucbit	addr		: 4;
	Ucbit	control		: 4;
	Uchar	isrc[12];
	Uchar	res14;
};

#else

struct isrc_subcode {		/* subcode for ISRC code */
	Uchar	res;
	Ucbit	control		: 4;
	Ucbit	addr		: 4;
	Uchar	isrc[12];
	Uchar	res14;
};

#endif


static	int	teac_attach(SCSI *usalp, cdr_t *dp);
static	int	teac_init(SCSI *usalp, cdr_t *dp);
static	int	teac_getdisktype(SCSI *usalp, cdr_t *dp);
static	int	speed_select_teac(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	select_secsize_teac(SCSI *usalp, track_t *trackp);
static	int	next_wr_addr_jvc(SCSI *usalp, track_t *, long *ap);
static	int	write_teac_xg1(SCSI *usalp, caddr_t, long, long, int, BOOL);
static	int	cdr_write_teac(SCSI *usalp, caddr_t bp, long sectaddr, long size, 
										int blocks, BOOL islast);
static	int	open_track_jvc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	teac_fixation(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	close_track_teac(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	teac_open_session(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	initsub_teac(SCSI *usalp, int toctype, int multi);
static	int	teac_doopc(SCSI *usalp);
static	int	teac_opc(SCSI *usalp, caddr_t, int cnt, int doopc);
static	int	opt_power_judge(SCSI *usalp, int judge);
static	int	clear_subcode(SCSI *usalp);
static	int	set_limits(SCSI *usalp, long lba, long length);
static	int	set_subcode(SCSI *usalp, Uchar *subcode_data, int length);
static	int	read_disk_info_teac(SCSI *usalp, Uchar *data, int length, 
											  int type);
static	int	teac_freeze(SCSI *usalp, int bp_flag);
static	int	teac_wr_pma(SCSI *usalp);
static	int	teac_rd_pma(SCSI *usalp);
static	int	next_wr_addr_teac(SCSI *usalp, long start_lba, long last_lba);
static	int	blank_jvc(SCSI *usalp, cdr_t *dp, long addr, int blanktype);
static	int	buf_cap_teac(SCSI *usalp, long *sp, long *fp);
static	long	read_peak_buffer_cap_teac(SCSI *usalp);
static	int	buffer_inquiry_teac(SCSI *usalp, int fmt);
#ifdef	XXBUFFER
static	void	check_buffer_teac(SCSI *usalp);
#endif
#ifdef	XXDEBUG
static	void	xxtest_teac(SCSI *usalp);
#endif


cdr_t	cdr_teac_cdr50 = {
	0, 0,
/*	CDR_TAO|CDR_SAO|CDR_SWABAUDIO|CDR_NO_LOLIMIT,*/
	CDR_TAO|CDR_SWABAUDIO|CDR_NO_LOLIMIT,
	CDR_CDRW_ALL,
	2, 4,
	"teac_cdr50",
	"driver for Teac CD-R50S, Teac CD-R55S, JVC XR-W2010, Pinnacle RCD-5020",
	0,
	(dstat_t *)0,
	drive_identify,
	teac_attach,
	teac_init,
	teac_getdisktype,
	scsi_load,
	scsi_unload,
	buf_cap_teac,
	cmd_dummy,					/* recovery_needed */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_teac,
	select_secsize,
	next_wr_addr_jvc,
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	cdr_write_teac,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_jvc,
	close_track_teac,
	teac_open_session,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	teac_fixation,
	cmd_dummy,					/* stats	*/
/*	blank_dummy,*/
	blank_jvc,
	format_dummy,
	teac_opc,
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

static int 
teac_init(SCSI *usalp, cdr_t *dp)
{
	return (speed_select_teac(usalp, dp, NULL));
}

static int 
teac_getdisktype(SCSI *usalp, cdr_t *dp)
{
	dstat_t	*dsp = dp->cdr_dstat;
	struct scsi_mode_data md;
	int	count = sizeof (struct scsi_mode_header) +
			sizeof (struct scsi_mode_blockdesc);
	int	len;
	int	page = 0;
	long	l;

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	(void) test_unit_ready(usalp);
	if (mode_sense(usalp, (Uchar *)&md, count, page, 0) < 0) {	/* Page n current */
		return (-1);
	} else {
		len = ((struct scsi_mode_header *)&md)->sense_data_len + 1;
	}
	if (((struct scsi_mode_header *)&md)->blockdesc_len < 8)
		return (-1);

	l = a_to_u_3_byte(md.blockdesc.nlblock);
	dsp->ds_maxblocks = l;
	return (drive_getdisktype(usalp, dp));
}

static int 
speed_select_teac(SCSI *usalp, cdr_t *dp, int *speedp)
{
	struct cdd_52x_mode_data md;
	int	count;
	int	status;
	int	speed = 1;
	BOOL	dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (speedp)
		speed = *speedp;

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	count  = sizeof (struct scsi_mode_header) +
		sizeof (struct teac_mode_page_21);

	md.pagex.teac_page21.p_code = 0x21;
	md.pagex.teac_page21.p_len =  0x01;
	md.pagex.teac_page21.dummy = dummy?3:0;

	status = mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2);
	if (status < 0)
		return (status);

	if (speedp == 0)
		return (0);

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	count  = sizeof (struct scsi_mode_header) +
		sizeof (struct teac_mode_page_31);

	speed >>= 1;
	md.pagex.teac_page31.p_code = 0x31;
	md.pagex.teac_page31.p_len =  0x02;
	md.pagex.teac_page31.speed = speed;

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

static int 
select_secsize_teac(SCSI *usalp, track_t *trackp)
{
	struct scsi_mode_data md;
	int	count = sizeof (struct scsi_mode_header) +
			sizeof (struct scsi_mode_blockdesc);
	int	len;
	int	page = 0;

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	(void) test_unit_ready(usalp);
	if (mode_sense(usalp, (Uchar *)&md, count, page, 0) < 0) {	/* Page n current */
		return (-1);
	} else {
		len = ((struct scsi_mode_header *)&md)->sense_data_len + 1;
	}
	if (((struct scsi_mode_header *)&md)->blockdesc_len < 8)
		return (-1);

	md.header.sense_data_len = 0;
	md.header.blockdesc_len = 8;

	md.blockdesc.density = 1;
	if (trackp->secsize == 2352)
		md.blockdesc.density = 4;
	i_to_3_byte(md.blockdesc.lblen, trackp->secsize);

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

static int 
next_wr_addr_jvc(SCSI *usalp, track_t *trackp, long *ap)
{
	if (trackp != 0 && trackp->track > 0) {
		*ap = lba_addr;
	} else {
		long	nwa;

		if (read_B0(usalp, TRUE, &nwa, NULL) < 0)
			return (-1);

		*ap = nwa + 150;
	}
	return (0);
}

static int 
write_teac_xg1(SCSI *usalp, 
               caddr_t bp       /* address of buffer */, 
               long sectaddr    /* disk address (sector) to put */, 
               long size        /* number of bytes to transfer */,
               int blocks       /* sector count */, 
               BOOL extwr       /* is an extended write */)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
/*	scmd->flags = SCG_DISRE_ENA;*/
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = SC_EWRITE;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, sectaddr);
	g1_cdblen(&scmd->cdb.g1_cdb, blocks);
	scmd->cdb.g1_cdb.vu_97 = extwr;

	usalp->cmdname = "write_teac_g1";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (size - usal_getresid(usalp));
}

static int 
cdr_write_teac(SCSI *usalp, 
               caddr_t bp       /* address of buffer */,
               long sectaddr    /* disk address (sector) to put */,
               long size        /* number of bytes to transfer */,
               int blocks       /* sector count */,
               BOOL islast      /* last write for track */)
{
	int	ret;

	if (islast)
		last_done = TRUE;

	ret = write_teac_xg1(usalp, bp, sectaddr, size, blocks, !islast);
	if (ret < 0)
		return (ret);

	lba_addr = sectaddr + blocks;
#ifdef	XXBUFFER
	check_buffer_teac(usalp);
#endif
	return (ret);
}

static int 
open_track_jvc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	status;
	long	blocks;
	long	pregapsize;
	struct	pgm_subcode	sc;

	last_done = FALSE;

	if (select_secsize_teac(usalp, trackp) < 0)
		return (-1);

	status = clear_subcode(usalp);
/*next_wr_addr_teac(usalp);*/
	if (status < 0)
		return (status);

if (trackp->pregapsize != 0) {
	if (lverbose > 1) {
		printf("set_limits(%ld, %ld)-> %ld\n",
		lba_addr, trackp->pregapsize, lba_addr + trackp->pregapsize);
	}

	status = set_limits(usalp, lba_addr, trackp->pregapsize);
	if (status < 0)
		return (status);

	/*
	 * Set pre-gap (pause - index 0)
	 */
	set_pgm_subcode(&sc, SC_P,
			st2mode[trackp->sectype&ST_MASK], ADR_POS, trackp->trackno, 0);

	if (lverbose > 1)
		usal_prbytes("Subcode:", (Uchar *)&sc, sizeof (sc));

	status = set_subcode(usalp, (Uchar *)&sc, sizeof (sc));
	if (status < 0)
		return (status);

	pregapsize = trackp->pregapsize;
	if (!is_audio(trackp)) {
		lba_addr += 5;	/* link & run in blocks */
		pregapsize -= 5;
	}
	if (lverbose > 1) {
		printf("pad_track(%ld, %ld)-> %ld\n",
			lba_addr, pregapsize, lba_addr + pregapsize);
	}
	/*
	 * XXX Do we need to check isecsize too?
	 */
	if (pad_track(usalp, dp, trackp,
			lba_addr, (Llong)pregapsize*trackp->secsize,
			FALSE, (Llong *)0) < 0)
		return (-1);
}

	blocks = trackp->tracksize/trackp->secsize +
		    (trackp->tracksize%trackp->secsize?1:0);
	blocks += trackp->padsecs;
	if (blocks < 300)
		blocks = 300;
	if (!is_audio(trackp))
		blocks += 2;
if (!is_last(trackp) && trackp[1].pregapsize == 0)
		blocks -= 150;

	/*
	 * set the limits for the new subcode - seems to apply to all
	 * of the data track.
	 * Unknown tracksize is handled in open_session.
	 * We definitely need to know the tracksize in this driver.
	 */
	if (lverbose > 1) {
		printf("set_limits(%ld, %ld)-> %ld\n",
			lba_addr, blocks, lba_addr + blocks);
	}
	status = set_limits(usalp, lba_addr, blocks);
	if (status < 0)
		return (status);

	/*
	 * Set track start (index 1)
	 */
	set_pgm_subcode(&sc, SC_TR,
			st2mode[trackp->sectype&ST_MASK], ADR_POS, trackp->trackno, 1);

	if (lverbose > 1)
		usal_prbytes("Subcode:", (Uchar *)&sc, sizeof (sc));

	status = set_subcode(usalp, (Uchar *)&sc, sizeof (sc));
	if (status < 0)
		return (status);

if (!is_last(trackp) && trackp[1].pregapsize == 0) {
	blocks += lba_addr;
	pregapsize = 150;

	if (lverbose > 1) {
		printf("set_limits(%ld, %ld)-> %ld\n",
		blocks, pregapsize, blocks + pregapsize);
	}

	status = set_limits(usalp, blocks, pregapsize);
	if (status < 0)
		return (status);

	/*
	 * Set pre-gap (pause - index 0)
	 */
	trackp++;
	set_pgm_subcode(&sc, SC_P,
			st2mode[trackp->sectype&ST_MASK], ADR_POS, trackp->trackno, 0);

	if (lverbose > 1)
		usal_prbytes("Subcode:", (Uchar *)&sc, sizeof (sc));

	status = set_subcode(usalp, (Uchar *)&sc, sizeof (sc));
	if (status < 0)
		return (status);
}
	return (status);
}

static	char	sector[3000];

static int 
close_track_teac(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	ret = 0;

	if (!last_done) {
		printf("WARNING: adding dummy block to close track.\n");
		/*
		 * need read sector size
		 * XXX do we really need this ?
		 * XXX if we need this can we set blocks to 0 ?
		 */
		ret =  write_teac_xg1(usalp, sector, lba_addr, 2352, 1, FALSE);
		lba_addr++;
	}
	if (!is_audio(trackp))
		lba_addr += 2;
	teac_wr_pma(usalp);
	return (ret);
}



static const char *sd_teac50_error_str[] = {
	"\100\200diagnostic failure on component parts",	/* 40 80 */
	"\100\201diagnostic failure on memories",		/* 40 81 */
	"\100\202diagnostic failure on cd-rom ecc circuit",	/* 40 82 */
	"\100\203diagnostic failure on gate array",		/* 40 83 */
	"\100\204diagnostic failure on internal SCSI controller",	/* 40 84 */
	"\100\205diagnostic failure on servo processor",	/* 40 85 */
	"\100\206diagnostic failure on program rom",		/* 40 86 */
	"\100\220thermal sensor failure",			/* 40 90 */
	"\200\000controller prom error",			/* 80 00 */	/* JVC */
	"\201\000no disk present - couldn't get focus",		/* 81 00 */	/* JVC */
	"\202\000no cartridge present",				/* 82 00 */	/* JVC */
	"\203\000unable to spin up",				/* 83 00 */	/* JVC */
	"\204\000addr exceeded the last valid block addr",	/* 84 00 */	/* JVC */
	"\205\000sync error",					/* 85 00 */	/* JVC */
	"\206\000address can't find or not data track",		/* 86 00 */	/* JVC */
	"\207\000missing track",				/* 87 00 */	/* JVC */
	"\213\000cartridge could not be ejected",		/* 8B 00 */	/* JVC */
	"\215\000audio not playing",				/* 8D 00 */	/* JVC */
	"\216\000read toc error",				/* 8E 00 */	/* JVC */
	"\217\000a blank disk is detected by read toc",		/* 8F 00 */
	"\220\000pma less disk - not a recordable disk",	/* 90 00 */
	"\223\000mount error",					/* 93 00 */	/* JVC */
	"\224\000toc less disk",				/* 94 00 */
	"\225\000disc information less disk",			/* 95 00 */	/* JVC */
	"\226\000disc information read error",			/* 96 00 */	/* JVC */
	"\227\000linear velocity measurement error",		/* 97 00 */	/* JVC */
	"\230\000drive sequence stop",				/* 98 00 */	/* JVC */
	"\231\000actuator velocity control error",		/* 99 00 */	/* JVC */
	"\232\000slider velocity control error",		/* 9A 00 */	/* JVC */
	"\233\000opc initialize error",				/* 9B 00 */
	"\233\001power calibration not executed",		/* 9B 01 */
	"\234\000opc execution eror",				/* 9C 00 */
	"\234\001alpc error - opc execution",			/* 9C 01 */
	"\234\002opc execution timeout",			/* 9C 02 */
	"\245\000disk application code does not match host application code",	/* A5 00 */
	"\255\000completed preview write",			/* AD 00 */
	"\256\000invalid B0 value",				/* AE 00 */	/* JVC */
	"\257\000pca area full",				/* AF 00 */
	"\260\000efm isn't detected",				/* B0 00 */	/* JVC */
	"\263\000no logical sector",				/* B3 00 */	/* JVC */
	"\264\000full pma area",				/* B4 00 */
	"\265\000read address is atip area - blank",		/* B5 00 */
	"\266\000write address is efm area - aleady written",	/* B6 00 */
	"\271\000abnormal spinning - servo irq",		/* B9 00 */	/* JVC */
	"\272\000no write data - buffer empty",			/* BA 00 */
	"\273\000write emergency occurred",			/* BB 00 */
	"\274\000read timeout",					/* BC 00 */	/* JVC */
	"\277\000abnormal spin - nmi",				/* BF 00 */	/* JVC */
	"\301\0004th run-in block detected",			/* C1 00 */
	"\302\0003rd run-in block detected",			/* C2 00 */
	"\303\0002nd run-in block detected",			/* C3 00 */
	"\304\0001st run-in block detected",			/* C4 00 */
	"\305\000link block detected",				/* C5 00 */
	"\306\0001st run-out block detected",			/* C6 00 */
	"\307\0002nd run-out block detected",			/* C7 00 */
	"\314\000write request means mixed data mode",		/* CC 00 */
	"\315\000unable to ensure reliable writing with the inserted disk - unsupported disk",	 /* CD 00 */
	"\316\000unable to ensure reliable writing as the inserted disk does not support speed", /* CE 00 */
	"\317\000unable to ensure reliable writing as the inserted disk has no char id code",	 /* CF 00 */
	NULL
};

static int 
teac_attach(SCSI *usalp, cdr_t *dp)
{
	usal_setnonstderrs(usalp, sd_teac50_error_str);
#ifdef	XXDEBUG
	xxtest_teac(usalp);
	exit(0);
#endif
	return (0);
}

static int 
teac_fixation(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	long	lba;
	int	status;
	Uchar	*sp;
	Uint	i;
extern	char	*buf;

	if (trackp->tracks < 1) {
		/*
		 * We come here if wodim isonly called with the -fix option.
		 * As long as we cannot read and interpret the PMA, we must
		 * abort here.
		 */
		teac_rd_pma(usalp);
/*		errmsgno(EX_BAD, "Cannot fixate zero track disk.\n");*/
		errmsgno(EX_BAD, "Cannot fixate without track list (not yet implemented).\n");
		return (-1);
	}
	sp = (Uchar *)buf;

	sleep(1);

	status = clear_subcode(usalp);
	sleep(1);
	if (status < 0)
		return (status);

	sp[0] = 0;		/* reserved */
	sp[1] = 0;		/* reserved */
	sp[2] = 0;		/* Q TNO */

	sp = &sp[3];		/* point past header */

	/*
	 * Set up TOC entries for all tracks
	 */
	for (i = 1; i <= trackp->tracks; i++) {
		lba = trackp[i].trackstart+150;	/* MSF=00:02:00 is LBA=0 */

		sp = set_toc_subcode(sp,
				/* ctrl/adr for this track */
				st2mode[trackp[i].sectype&ST_MASK], ADR_POS,
					trackp[i].trackno, lba);
	}

	/*
	 * Set first track on disk
	 *
	 * XXX We set the track type for the lead-in to the track type
	 * XXX of the first track. The TEAC manual states that we should use
	 * XXX audio if the disk contains both, audio and data tracks.
	 */
	sp = set_lin_subcode(sp,
		/* ctrl/adr for first track */
		st2mode[trackp[1].sectype&ST_MASK], ADR_POS,
		0xA0,				/* Point A0 */
		trackp[1].trackno,		/* first track # */
		toc2sess[track_base(trackp)->tracktype & TOC_MASK],	/* disk type */
		0);				/* reserved */

	/*
	 * Set last track on disk
	 */
	sp = set_lin_subcode(sp,
		/* ctrl/adr for first track */
		st2mode[trackp[1].sectype&ST_MASK], ADR_POS,
		0xA1,				/* Point A1 */
		MSF_CONV(trackp[trackp->tracks].trackno), /* last track # */
		0,				/* reserved */
		0);				/* reserved */

	/*
	 * Set start of lead out area in MSF
	 * MSF=00:02:00 is LBA=0
	 */
	lba = lba_addr + 150;
	if (lverbose > 1)
	printf("lba: %ld lba_addr: %ld\n", lba, lba_addr);

	if (lverbose > 1)
	printf("Lead out start: (%02d:%02d/%02d)\n",
			minutes(lba*2352),
			seconds(lba*2352),
			frames(lba*2352));

	sp = set_lin_subcode(sp,
		/* ctrl/adr for first track */
		st2mode[trackp[1].sectype&ST_MASK], ADR_POS,
		0xA2,				/* Point A2 */
		MSF_CONV(LBA_MIN(lba)),
		MSF_CONV(LBA_SEC(lba)),
		MSF_CONV(LBA_FRM(lba)));

	status = sp - ((Uchar *)buf);
	if (lverbose > 1) {
		printf("Subcode len: %d\n", status);
		usal_prbytes("Subcode:", (Uchar *)buf, status);
	}
	status = set_subcode(usalp, (Uchar *)buf, status);
	sleep(1);
	if (status < 0)
		return (status);

	/*
	 * now write the toc
	 */
	status = teac_freeze(usalp, (track_base(trackp)->tracktype & TOCF_MULTI) == 0);
	return (status);

}

static int 
teac_open_session(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uint	i;

	for (i = 1; i <= trackp->tracks; i++) {
		if (trackp[i].tracksize < (tsize_t)0) {
			/*
			 * XXX How about setting the subcode range to infinity.
			 * XXX and correct it in clode track before writing
			 * XXX the PMA?
			 */
			errmsgno(EX_BAD, "Track %d has unknown length.\n", i);
			return (-1);
		}
	}
	return (initsub_teac(usalp, track_base(trackp)->tracktype & TOC_MASK,
				track_base(trackp)->tracktype & TOCF_MULTI));
}

static int 
initsub_teac(SCSI *usalp, int toctype, int multi)
{
	int	status;

	usalp->silent++;
	if (read_B0(usalp, TRUE, &lba_addr, NULL) < 0)
		lba_addr = -150;
	usalp->silent--;

	status = clear_subcode(usalp);
	if (status < 0)
		return (status);

	return (0);
}

static int 
teac_doopc(SCSI *usalp)
{
	int	status;

	if (lverbose) {
		fprintf(stdout, "Judging disk...");
		flush();
	}
	status = opt_power_judge(usalp, 1);
	if (status < 0) {
		printf("\n");
		return (status);
	}
	if (lverbose) {
		fprintf(stdout, "done.\nCalibrating laser...");
		flush();
	}

	status = opt_power_judge(usalp, 0);
	if (lverbose) {
		fprintf(stdout, "done.\n");
	}
	/*
	 * Check for error codes 0xCD ... 0xCF
	 */
	usalp->silent++;
	if (next_wr_addr_teac(usalp, -1, -1) < 0) {
		if (usalp->verbose == 0 && usal_sense_key(usalp) != SC_ILLEGAL_REQUEST)
			usal_printerr(usalp);
	}
	usalp->silent--;
	return (status);
}

static int 
teac_opc(SCSI *usalp, caddr_t bp, int cnt, int doopc)
{
	int	status;
	int	count = 0;

	do {
		status = teac_doopc(usalp);
	} while (++count <= 1 && status < 0);

	return (status);
}

/*--------------------------------------------------------------------------*/
#define	SC_SET_LIMITS		0xb3		/* teac 12 byte command */
#define	SC_SET_SUBCODE		0xc2		/* teac 10 byte command */
#define	SC_READ_PMA		0xc4		/* teac 10 byte command */
#define	SC_READ_DISK_INFO	0xc7		/* teac 10 byte command */
#define	SC_BUFFER_INQUIRY	0xe0		/* teac 12 byte command */

#define	SC_WRITE_PMA		0xe1		/* teac 12 byte command */
#define	SC_FREEZE		0xe3		/* teac 12 byte command */
#define	SC_OPC_EXECUTE		0xec		/* teac 12 byte command */
#define	SC_CLEAR_SUBCODE	0xe4		/* teac 12 byte command */
#define	SC_NEXT_WR_ADDRESS	0xe6		/* teac 12 byte command */

#define	SC_READ_PEAK_BUF_CAP	0xef		/* teac 12 byte command */

/*
 * Optimum power calibration for Teac Drives.
 */
static int 
opt_power_judge(SCSI *usalp, int judge)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 60;

	scmd->cdb.g5_cdb.cmd = SC_OPC_EXECUTE;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.reladr = judge; /* Judge the Disc */

	usalp->cmdname = "opt_power_judge";

	return (usal_cmd(usalp));
}

/*
 * Clear subcodes for Teac Drives.
 */
static int 
clear_subcode(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_CLEAR_SUBCODE;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.addr[3] = 0x80;

	usalp->cmdname = "clear subcode";

	return (usal_cmd(usalp));
}

/*
 * Set limits for command linking for Teac Drives.
 */
static int 
set_limits(SCSI *usalp, long lba, long length)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_SET_LIMITS;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	i_to_4_byte(&scmd->cdb.g5_cdb.addr[0], lba);
	i_to_4_byte(&scmd->cdb.g5_cdb.count[0], length);

	usalp->cmdname = "set limits";

	return (usal_cmd(usalp));
}

/*
 * Set subcode for Teac Drives.
 */
static int 
set_subcode(SCSI *usalp, Uchar *subcode_data, int length)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)subcode_data;
	scmd->size = length;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g1_cdb.cmd = SC_SET_SUBCODE;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, length);

	usalp->cmdname = "set subcode";

	return (usal_cmd(usalp));
}

static int 
read_disk_info_teac(SCSI *usalp, Uchar *data, int length, int type)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)data;
	scmd->size = length;
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g1_cdb.cmd = SC_READ_DISK_INFO;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	scmd->cdb.g1_cdb.reladr = type & 1;
	scmd->cdb.g1_cdb.res    = (type & 2) >> 1;

	usalp->cmdname = "read disk info teac";

	return (usal_cmd(usalp));
}

/*
 * Perform the freeze command for Teac Drives.
 */
static int 
teac_freeze(SCSI *usalp, int bp_flag)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 8 * 60;		/* Needs up to 4 minutes */

	scmd->cdb.g5_cdb.cmd = SC_FREEZE;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.addr[3] = bp_flag ? 0x80 : 0;

	usalp->cmdname = "teac_freeze";

	return (usal_cmd(usalp));
}

static int 
teac_wr_pma(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_WRITE_PMA;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "teac_write_pma";

	return (usal_cmd(usalp));
}

/*
 * Read PMA for Teac Drives.
 */
static int 
teac_rd_pma(SCSI *usalp)
{
	unsigned char	xx[256];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g1_cdb.cmd = SC_READ_PMA;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	g1_cdblen(&scmd->cdb.g1_cdb, sizeof (xx));

	usalp->cmdname = "teac_read_pma";

/*	return (usal_cmd(usalp));*/
	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usalp->verbose) {
		usal_prbytes("PMA Data", xx, sizeof (xx) - usal_getresid(usalp));
	}
	if (lverbose) {
		unsigned i;
		Uchar	*p;

		usal_prbytes("PMA Header: ", xx, 4);
		i = xx[2];
		p = &xx[4];
		for (; i <= xx[3]; i++) {
			usal_prbytes("PMA: ", p, 10);
			p += 10;
		}
	}
	return (0);
}

/*
 * Next writable address for Teac Drives.
 */
static int 
next_wr_addr_teac(SCSI *usalp, long start_lba, long last_lba)
{
	unsigned char	xx[256];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_NEXT_WR_ADDRESS;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	i_to_4_byte(&scmd->cdb.g5_cdb.addr[0], start_lba);
	i_to_4_byte(&scmd->cdb.g5_cdb.count[0], last_lba);

	if (usalp->verbose)
		printf("start lba: %ld last lba: %ld\n",
					start_lba, last_lba);

	usalp->cmdname = "next writable address";

/*	return (usal_cmd(usalp));*/
	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usalp->verbose) {
		usal_prbytes("WRa Data", xx, sizeof (xx) - usal_getresid(usalp));
		printf("NWA: %ld\n", a_to_4_byte(xx));
	}
	return (0);
}

static int 
blank_jvc(SCSI *usalp, cdr_t *dp, long addr, int blanktype)
{
	extern	char	*blank_types[];

	if (lverbose) {
		printf("Blanking %s\n", blank_types[blanktype & 0x07]);
		flush();
	}

	return (scsi_blank(usalp, addr, blanktype, FALSE));
}

static int 
buf_cap_teac(SCSI *usalp, long *sp, long *fp)
{
	Ulong	freespace;
	Ulong	bufsize;
	long	ret;
	int	per;

	ret = read_peak_buffer_cap_teac(usalp);
	if (ret < 0)
		return (-1);
	bufsize = ret;
	freespace = 0;
	if (sp)
		*sp = bufsize;
	if (fp)
		*fp = freespace;

	if (usalp->verbose || (sp == 0 && fp == 0))
		printf("BFree: %ld K BSize: %ld K\n", freespace >> 10, bufsize >> 10);

	if (bufsize == 0)
		return (0);
	per = (100 * (bufsize - freespace)) / bufsize;
	if (per < 0)
		return (0);
	if (per > 100)
		return (100);
	return (per);
}

static long 
read_peak_buffer_cap_teac(SCSI *usalp)
{
	Uchar	xx[4];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_READ_PEAK_BUF_CAP;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "read peak buffer capacity";

#define	BDEBUG
#ifndef	BDEBUG
	return (usal_cmd(usalp));
#else
	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usalp->verbose) {
		usal_prbytes("WRa Data", xx, sizeof (xx) - usal_getresid(usalp));
		printf("Buffer cap: %ld\n", a_to_u_3_byte(&xx[1]));
	}
	return (a_to_u_3_byte(&xx[1]));
/*	return (0);*/
#endif
}

#define	BI_ONE_BYTE	0xC0
#define	BI_448_BYTE	0x40
#define	BI_APP_CODE	0x10

static int 
buffer_inquiry_teac(SCSI *usalp, int fmt)
{
	Uchar	xx[448];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->size = 448;
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = SC_BUFFER_INQUIRY;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	if (fmt > 0) {
		scmd->cdb.g5_cdb.addr[3] = fmt;
		if (fmt == BI_ONE_BYTE)
			scmd->size = 1;
	} else {
		scmd->cdb.g5_cdb.addr[3] = BI_448_BYTE;
/*		scmd->cdb.g5_cdb.addr[3] = BI_APP_CODE;*/
	}

	usalp->cmdname = "buffer inquiry";

#define	BDEBUG
#ifndef	BDEBUG
	return (usal_cmd(usalp));
#else
	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usalp->verbose) {
/*		usal_prbytes("WRa Data", xx, sizeof (xx) - usal_getresid(usalp));*/
/*		usal_prbytes("WRa Data", xx, 1);*/

		if (fmt > 0) printf("fmt: %X ", fmt);
		usal_prbytes("WRa Data", xx, 9);
		printf("%d\n", xx[8] - xx[1]);
/*		printf("Buffer cap: %ld\n", a_to_u_3_byte(&xx[1]));*/
	}
	return (0);
#endif
}

#ifdef	XXBUFFER
static void 
check_buffer_teac(SCSI *usalp)
{
	printf("-------\n");
	buffer_inquiry_teac(usalp, 0);
#ifdef	SL
	usleep(40000);
	buffer_inquiry_teac(usalp, 0);
#endif
	read_peak_buffer_cap_teac(usalp);
}
#endif
/*--------------------------------------------------------------------------*/
#ifdef	XXDEBUG
#include "scsimmc.h"

static	int	g7_teac(SCSI *usalp);
static	int	g6_teac(SCSI *usalp);

static int 
g7_teac(SCSI *usalp)
{
	Uchar	xx[2048];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g5_cdb.cmd = 0xDf;
/*	scmd->cdb.g5_cdb.cmd = 0xE5;*/
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

/*	scmd->cdb.g5_cdb.addr[3] = BI_ONE_BYTE;*/
/*	scmd->size = 1;*/

/*	scmd->cdb.g5_cdb.addr[3] = BI_448_BYTE;*/
/*	scmd->cdb.g5_cdb.addr[3] = BI_APP_CODE;*/

	usalp->cmdname = "g7 teac";

/*	return (usal_cmd(usalp));*/
	if (usal_cmd(usalp) < 0)
		return (-1);

/*	if (usalp->verbose) {*/
		usal_prbytes("WRa Data", xx, sizeof (xx) - usal_getresid(usalp));
/*		usal_prbytes("WRa Data", xx, 1);*/
/*		usal_prbytes("WRa Data", xx, 9);*/
/*printf("%d\n", xx[8] - xx[1]);*/
/*		printf("Buffer cap: %ld\n", a_to_u_3_byte(&xx[1]));*/
/*	}*/
	return (0);
}

static int 
g6_teac(SCSI *usalp)
{
	Uchar	xx[2048];
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)xx, sizeof (xx), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)xx;
	scmd->size = sizeof (xx);
	scmd->flags = SCG_RECV_DATA |SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	scmd->cdb.g1_cdb.cmd = 0xC1;
	scmd->cdb.g1_cdb.cmd = 0xC3;
	scmd->cdb.g1_cdb.cmd = 0xC6;
	scmd->cdb.g1_cdb.cmd = 0xC7;	/* Read TOC */
	scmd->cdb.g1_cdb.cmd = 0xCe;
	scmd->cdb.g1_cdb.cmd = 0xCF;
	scmd->cdb.g1_cdb.cmd = 0xC7;	/* Read TOC */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "g6 teac";

/*	return (usal_cmd(usalp));*/
	if (usal_cmd(usalp) < 0)
		return (-1);

/*	if (usalp->verbose) {*/
		usal_prbytes("WRa Data", xx, sizeof (xx) - usal_getresid(usalp));
/*		usal_prbytes("WRa Data", xx, 1);*/
/*		usal_prbytes("WRa Data", xx, 9);*/
/*printf("%d\n", xx[8] - xx[1]);*/
/*		printf("Buffer cap: %ld\n", a_to_u_3_byte(&xx[1]));*/
/*	}*/
	return (0);
}

static void 
xxtest_teac(SCSI *usalp)
{
	read_peak_buffer_cap_teac(usalp);

/*#define	XDI*/
#ifdef	XDI
	{
		Uchar cbuf[512];

/*		read_disk_info_teac(usalp, data, length, type)*/
/*		read_disk_info_teac(usalp, cbuf, 512, 2);*/
/*		read_disk_info_teac(usalp, cbuf, 512, 2);*/
		read_disk_info_teac(usalp, cbuf, 512, 3);
		usal_prbytes("DI Data", cbuf, sizeof (cbuf) - usal_getresid(usalp));
	}
#endif	/* XDI */

	buffer_inquiry_teac(usalp, -1);

/*#define	XBU*/
#ifdef	XBU
	{
		int i;

		for (i = 0; i < 63; i++) {
			usalp->silent++;
			buffer_inquiry_teac(usalp, i<<2);
			usalp->silent--;
		}
	}
#endif	/* XBU */

/*	printf("LLLL\n");*/
/*	g7_teac(usalp);*/
/*	g6_teac(usalp);*/
}
#endif	/* XXDEBUG */
