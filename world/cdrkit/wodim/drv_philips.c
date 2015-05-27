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

/* @(#)drv_philips.c	1.69 05/05/16 Copyright 1997-2005 J. Schilling */
/*
 *	CDR device implementation for
 *	Philips/Yamaha/Ricoh/Plasmon
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

#include <mconfig.h>

#include <stdio.h>
#include <unixstd.h>	/* Include sys/types.h to make off_t available */
#include <standard.h>
#include <intcvt.h>
#include <schily.h>

#include <usal/scsireg.h>
#include <usal/scsitransp.h>
#include <usal/usalcmd.h>
#include <usal/scsidefs.h>	/* XXX Only for DEV_RICOH_RO_1060C */

#include "wodim.h"

extern	int	debug;
extern	int	lverbose;

static	int	load_unload_philips(SCSI *usalp, int);
static	int	philips_load(SCSI *usalp, cdr_t *dp);
static	int	philips_unload(SCSI *usalp, cdr_t *dp);
static	int	philips_dumbload(SCSI *usalp, cdr_t *dp);
static	int	philips_dumbunload(SCSI *usalp, cdr_t *dp);
static	int	plasmon_buf(SCSI *, long *, long *);
static	int	recover_philips(SCSI *usalp, cdr_t *dp, int);
static	int	speed_select_yamaha(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	speed_select_philips(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	speed_select_oldphilips(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	speed_select_dumbphilips(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	speed_select_pioneer(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	philips_init(SCSI *usalp, cdr_t *dp);
static	int	philips_getdisktype(SCSI *usalp, cdr_t *dp);
static	BOOL	capacity_philips(SCSI *usalp, long *lp);
static	int	first_writable_addr_philips(SCSI *usalp, long *, int, int, int, 
														 int);
static	int	next_wr_addr_philips(SCSI *usalp, track_t *trackp, long *ap);
static	int	reserve_track_philips(SCSI *usalp, unsigned long);
static	int	scsi_cdr_write_philips(SCSI *usalp, caddr_t bp, long sectaddr, 
												  long size, int blocks, BOOL islast);
static	int	write_track_info_philips(SCSI *usalp, int);
static	int	write_track_philips(SCSI *usalp, long, int);
static	int	open_track_philips(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	open_track_plasmon(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	open_track_oldphilips(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	open_track_yamaha(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	close_track_philips(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	fixation_philips(SCSI *usalp, cdr_t *dp, track_t *trackp);

static	int	philips_attach(SCSI *usalp, cdr_t *);
static	int	plasmon_attach(SCSI *usalp, cdr_t *);
static	int	ricoh_attach(SCSI *usalp, cdr_t *);
static	int	philips_getlilo(SCSI *usalp, long *lilenp, long *lolenp);


struct cdd_52x_mode_page_21 {	/* write track information */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x0E = 14 Bytes */
	Uchar	res_2;
	Uchar	sectype;
	Uchar	track;
	Uchar	ISRC[9];
	Uchar	res[2];
};

struct cdd_52x_mode_page_23 {	/* speed selection */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x06 = 6 Bytes */
	Uchar	speed;
	Uchar	dummy;
	Uchar	res[4];

};

#if defined(_BIT_FIELDS_LTOH)	/* Intel byteorder */

struct yamaha_mode_page_31 {	/* drive configuration */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x02 = 2 Bytes */
	Uchar	res;
	Ucbit	dummy		: 4;
	Ucbit	speed		: 4;
};

#else				/* Motorola byteorder */

struct yamaha_mode_page_31 {	/* drive configuration */
		MP_P_CODE;		/* parsave & pagecode */
	Uchar	p_len;			/* 0x02 = 2 Bytes */
	Uchar	res;
	Ucbit	speed		: 4;
	Ucbit	dummy		: 4;
};
#endif

struct cdd_52x_mode_data {
	struct scsi_mode_header	header;
	union cdd_pagex	{
		struct cdd_52x_mode_page_21	page21;
		struct cdd_52x_mode_page_23	page23;
		struct yamaha_mode_page_31	page31;
	} pagex;
};


cdr_t	cdr_philips_cdd521O = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"philips_cdd521_old",
	"driver for Philips old CDD-521",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_oldphilips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_oldphilips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_philips_dumb = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"philips_dumb",
	"driver for Philips CDD-521 with pessimistic assumptions",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_dumbload,
	philips_dumbunload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_dumbphilips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_oldphilips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_philips_cdd521 = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"philips_cdd521",
	"driver for Philips CDD-521",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_philips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_philips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_philips_cdd522 = {
	0, 0,
/*	CDR_TAO|CDR_SAO|CDR_TRAYLOAD,*/
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"philips_cdd522",
	"driver for Philips CDD-522",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_philips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_philips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_tyuden_ew50 = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD|CDR_SWABAUDIO,
	CDR_CDRW_NONE,
	2, 2,
	"tyuden_ew50",
	"driver for Taiyo Yuden EW-50",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_philips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_philips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_kodak_pcd600 = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	6, 6,
	"kodak_pcd_600",
	"driver for Kodak PCD-600",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_philips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_oldphilips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_plasmon_rf4100 = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD,
	CDR_CDRW_NONE,
	2, 4,
	"plasmon_rf4100",
	"driver for Plasmon RF 4100",
	0,
	(dstat_t *)0,
	drive_identify,
	plasmon_attach,
	philips_init,
	philips_getdisktype,
	philips_load,
	philips_unload,
	plasmon_buf,
	recovery_needed,
	recover_philips,
	speed_select_philips,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_plasmon,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_pioneer_dw_s114x = {
	0, 0,
	CDR_TAO|CDR_TRAYLOAD|CDR_SWABAUDIO,
	CDR_CDRW_NONE,
	2, 4,
	"pioneer_dws114x",
	"driver for Pioneer DW-S114X",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	philips_getdisktype,
	scsi_load,
	scsi_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_pioneer,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
/*	open_track_yamaha,*/
/*???*/	open_track_oldphilips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_yamaha_cdr100 = {
	0, 0,
/*	CDR_TAO|CDR_SAO|CDR_CADDYLOAD|CDR_SWABAUDIO,*/
	CDR_TAO|CDR_CADDYLOAD|CDR_SWABAUDIO,
	CDR_CDRW_NONE,
	2, 4,
	"yamaha_cdr100",
	"driver for Yamaha CDR-100 / CDR-102",
	0,
	(dstat_t *)0,
	drive_identify,
	philips_attach,
	philips_init,
	drive_getdisktype,
	scsi_load,
	philips_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_yamaha,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_yamaha,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_ricoh_ro1060 = {
	0, 0,
/*	CDR_TAO|CDR_SAO|CDR_CADDYLOAD,*/
	CDR_TAO|CDR_CADDYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"ricoh_ro1060c",
	"driver for Ricoh RO-1060C",
	0,
	(dstat_t *)0,
	drive_identify,
	ricoh_attach,
	philips_init,
	philips_getdisktype,
	scsi_load,
	scsi_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_yamaha,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_philips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_ricoh_ro1420 = {
	0, 0,
/*	CDR_TAO|CDR_SAO|CDR_CADDYLOAD,*/
	CDR_TAO|CDR_CADDYLOAD,
	CDR_CDRW_NONE,
	2, 2,
	"ricoh_ro1420c",
	"driver for Ricoh RO-1420C",
	0,
	(dstat_t *)0,
	drive_identify,
	ricoh_attach,
	philips_init,
	philips_getdisktype,
	scsi_load,
	scsi_unload,
	buf_dummy,
	recovery_needed,
	recover_philips,
	speed_select_yamaha,
	select_secsize,
	next_wr_addr_philips,
	reserve_track_philips,
	scsi_cdr_write_philips,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_philips,
	close_track_philips,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	fixation_philips,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};


static int load_unload_philips(SCSI *usalp, int load)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xE7;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.count[1] = !load;

	usalp->cmdname = "philips medium load/unload";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static int 
philips_load(SCSI *usalp, cdr_t *dp)
{
	return (load_unload_philips(usalp, 1));
}

static int 
philips_unload(SCSI *usalp, cdr_t *dp)
{
	return (load_unload_philips(usalp, 0));
}

static int 
philips_dumbload(SCSI *usalp, cdr_t *dp)
{
	int	ret;

	usalp->silent++;
	ret = load_unload_philips(usalp, 1);
	usalp->silent--;
	if (ret < 0)
		return (scsi_load(usalp, dp));
	return (0);
}

static int 
philips_dumbunload(SCSI *usalp, cdr_t *dp)
{
	int	ret;

	usalp->silent++;
	ret = load_unload_philips(usalp, 0);
	usalp->silent--;
	if (ret < 0)
		return (scsi_unload(usalp, dp));
	return (0);
}

static int 
plasmon_buf(SCSI *usalp, 
            long *sp /* Size pointer */, 
            long *fp /* Free space pointer */)
{
	/*
	 * There's no way to obtain these values from the
	 * Plasmon RF41xx devices.  This function stub is only
	 * present to prevent cdrecord.c from calling the READ BUFFER
	 * SCSI cmd which is implemented non standard compliant in
	 * the Plasmon drive. Calling READ BUFFER would only jam the Plasmon
	 * as the non standard implementation in the Plasmon firmware
	 * expects different parameters.
	 */

	if (sp)
		*sp = 0L;
	if (fp)
		*fp = 0L;

	return (100);	/* 100 % */
}

static int 
recover_philips(SCSI *usalp, cdr_t *dp, int track)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xEC;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "philips recover";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static int 
speed_select_yamaha(SCSI *usalp, cdr_t *dp, int *speedp)
{
	struct	scsi_mode_page_header	*mp;
	char				mode[256];
	int				len = 16;
	int				page = 0x31;
	struct yamaha_mode_page_31	*xp;
	struct cdd_52x_mode_data md;
	int	count;
	int	speed = 1;
	BOOL	dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (speedp) {
		speed = *speedp;
	} else {
		fillbytes((caddr_t)mode, sizeof (mode), '\0');

		if (!get_mode_params(usalp, page, "Speed/Dummy information",
			(Uchar *)mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
			return (-1);
		}
		if (len == 0)
			return (-1);

		mp = (struct scsi_mode_page_header *)
			(mode + sizeof (struct scsi_mode_header) +
			((struct scsi_mode_header *)mode)->blockdesc_len);

		xp = (struct yamaha_mode_page_31 *)mp;
		speed = xp->speed;
	}

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	count  = sizeof (struct scsi_mode_header) +
		sizeof (struct yamaha_mode_page_31);

	speed >>= 1;
	md.pagex.page31.p_code = 0x31;
	md.pagex.page31.p_len =  0x02;
	md.pagex.page31.speed = speed;
	md.pagex.page31.dummy = dummy?1:0;

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

static int 
speed_select_philips(SCSI *usalp, cdr_t *dp, int *speedp)
{
	struct	scsi_mode_page_header	*mp;
	char				mode[256];
	int				len = 20;
	int				page = 0x23;
	struct cdd_52x_mode_page_23	*xp;
	struct cdd_52x_mode_data	md;
	int				count;
	int				speed = 1;
	BOOL				dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (speedp) {
		speed = *speedp;
	} else {
		fillbytes((caddr_t)mode, sizeof (mode), '\0');

		if (!get_mode_params(usalp, page, "Speed/Dummy information",
			(Uchar *)mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
			return (-1);
		}
		if (len == 0)
			return (-1);

		mp = (struct scsi_mode_page_header *)
			(mode + sizeof (struct scsi_mode_header) +
			((struct scsi_mode_header *)mode)->blockdesc_len);

		xp = (struct cdd_52x_mode_page_23 *)mp;
		speed = xp->speed;
	}

	fillbytes((caddr_t)&md, sizeof (md), '\0');

	count  = sizeof (struct scsi_mode_header) +
		sizeof (struct cdd_52x_mode_page_23);

	md.pagex.page23.p_code = 0x23;
	md.pagex.page23.p_len =  0x06;
	md.pagex.page23.speed = speed;
	md.pagex.page23.dummy = dummy?1:0;

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

static int 
speed_select_pioneer(SCSI *usalp, cdr_t *dp, int *speedp)
{
	if (speedp != 0 && *speedp < 2) {
		*speedp = 2;
		if (lverbose)
			printf("WARNING: setting to minimum speed (2).\n");
	}
	return (speed_select_philips(usalp, dp, speedp));
}

static int 
speed_select_oldphilips(SCSI *usalp, cdr_t *dp, int *speedp)
{
	BOOL	dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (lverbose)
		printf("WARNING: ignoring selected speed.\n");
	if (dummy) {
		errmsgno(EX_BAD, "Cannot set dummy writing for this device.\n");
		return (-1);
	}
	return (0);
}

static int 
speed_select_dumbphilips(SCSI *usalp, cdr_t *dp, int *speedp)
{
	if (speed_select_philips(usalp, dp, speedp) < 0)
		return (speed_select_oldphilips(usalp, dp, speedp));
	return (0);
}

static int 
philips_init(SCSI *usalp, cdr_t *dp)
{
	return ((*dp->cdr_set_speed_dummy)(usalp, dp, NULL));
}


#define	IS(what, flag)	printf("  Is %s%s\n", flag?"":"not ", what);

static int 
philips_getdisktype(SCSI *usalp, cdr_t *dp)
{
	dstat_t	*dsp = dp->cdr_dstat;
	char	sbuf[16];
	long	dummy;
	long	lilen;
	long	lolen;
	msf_t	msf;
	int	audio = -1;

	usalp->silent++;
	dummy = (*dp->cdr_next_wr_address)(usalp, (track_t *)0, &lilen);
	usalp->silent--;

	/*
	 * Check for "Command sequence error" first.
	 */
	if ((dsp->ds_cdrflags & RF_WRITE) != 0 &&
	    dummy < 0 &&
	    (usal_sense_key(usalp) != SC_ILLEGAL_REQUEST ||
						usal_sense_code(usalp) != 0x2C)) {
		reload_media(usalp, dp);
	}

	usalp->silent++;
	if (read_subchannel(usalp, sbuf, 0, 12, 0, 1, 0xf0) >= 0) {
		if (sbuf[2] == 0 && sbuf[3] == 8)
			audio = (sbuf[7] & 0x40) != 0;
	}
	usalp->silent--;

	if ((dp->cdr_dstat->ds_cdrflags & RF_PRATIP) != 0 &&
						dummy >= 0 && lilen == 0) {
		usalp->silent++;
		dummy = philips_getlilo(usalp, &lilen, &lolen);
		usalp->silent--;

		if (dummy >= 0) {
/*			printf("lead-in len: %d lead-out len: %d\n", lilen, lolen);*/
			lba_to_msf(-150 - lilen, &msf);

			printf("ATIP info from disk:\n");
			if (audio >= 0)
				IS("unrestricted", audio);
			if (audio == 1 || (audio == 0 && (sbuf[7] & 0x3F) != 0x3F))
				printf("  Disk application code: %d\n", sbuf[7] & 0x3F);
			printf("  ATIP start of lead in:  %ld (%02d:%02d/%02d)\n",
				-150 - lilen, msf.msf_min, msf.msf_sec, msf.msf_frame);

			if (capacity_philips(usalp, &lolen)) {
				lba_to_msf(lolen, &msf);
				printf(
				"  ATIP start of lead out: %ld (%02d:%02d/%02d)\n",
				lolen, msf.msf_min, msf.msf_sec, msf.msf_frame);
			}
			lba_to_msf(-150 - lilen, &msf);
			pr_manufacturer(&msf,
					FALSE,		/* Always not erasable */
					audio > 0);	/* Audio from read subcode */
		}
	}

	if (capacity_philips(usalp, &lolen)) {
		dsp->ds_maxblocks = lolen;
		dsp->ds_maxrblocks = disk_rcap(&msf, dsp->ds_maxblocks,
					FALSE,		/* Always not erasable */
					audio > 0);	/* Audio from read subcode */
	}
	usalp->silent++;
	/*read_subchannel(usalp, bp, track, cnt, msf, subq, fmt); */

	if (read_subchannel(usalp, sbuf, 0, 14, 0, 0, 0xf1) >= 0)
		usal_prbytes("Disk bar code:", (Uchar *)sbuf, 14 - usal_getresid(usalp));
	usalp->silent--;

	return (drive_getdisktype(usalp, dp));
}

static BOOL 
capacity_philips(SCSI *usalp, long *lp)
{
	long	l = 0L;
	BOOL	succeed = TRUE;

	usalp->silent++;
	if (read_B0(usalp, FALSE, NULL, &l) >= 0) {
		if (debug)
			printf("lead out B0: %ld\n", l);
		*lp = l;
	} else if (read_trackinfo(usalp, 0xAA, &l, NULL, NULL, NULL, NULL) >= 0) {
		if (debug)
			printf("lead out AA: %ld\n", l);
		*lp = l;
	} if (read_capacity(usalp) >= 0) {
		l = usalp->cap->c_baddr + 1;
		if (debug)
			printf("lead out capacity: %ld\n", l);
	} else {
		succeed = FALSE;
	}
	*lp = l;
	usalp->silent--;
	return (succeed);
}

struct	fwa {
	char	len;
	char	addr[4];
	char	res;
};

static int 
first_writable_addr_philips(SCSI *usalp, long *ap, int track, int isaudio, 
										int preemp, int npa)
{
	struct	fwa	fwa;
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)&fwa, sizeof (fwa), '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)&fwa;
	scmd->size = sizeof (fwa);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xE2;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.addr[0] = track;
	scmd->cdb.g1_cdb.addr[1] = isaudio ? (preemp ? 5 : 4) : 1;

	scmd->cdb.g1_cdb.count[0] = npa?1:0;
	scmd->cdb.g1_cdb.count[1] = sizeof (fwa);

	usalp->cmdname = "first writeable address philips";

	if (usal_cmd(usalp) < 0)
		return (-1);

	if (ap)
		*ap = a_to_4_byte(fwa.addr);
	return (0);
}

static int 
next_wr_addr_philips(SCSI *usalp, track_t *trackp, long *ap)
{

/*	if (first_writable_addr_philips(usalp, ap, 0, 0, 0, 1) < 0)*/
	if (first_writable_addr_philips(usalp, ap, 0, 0, 0, 0) < 0)
		return (-1);
	return (0);
}

static int 
reserve_track_philips(SCSI *usalp, unsigned long len)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xE4;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	i_to_4_byte(&scmd->cdb.g1_cdb.addr[3], len);

	usalp->cmdname = "philips reserve_track";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static int 
scsi_cdr_write_philips(SCSI *usalp, 
                       caddr_t bp       /* address of buffer */,
                       long sectaddr    /* disk address (sector) to put */,
                       long size        /* number of bytes to transfer */, 
                       int blocks       /* sector count */, 
                       BOOL islast      /* last write for track */)
{
	return (write_xg0(usalp, bp, 0, size, blocks));
}

static int 
write_track_info_philips(SCSI *usalp, int sectype)
{
	struct cdd_52x_mode_data md;
	int	count = sizeof (struct scsi_mode_header) +
			sizeof (struct cdd_52x_mode_page_21);

	fillbytes((caddr_t)&md, sizeof (md), '\0');
	md.pagex.page21.p_code = 0x21;
	md.pagex.page21.p_len =  0x0E;
				/* is sectype ok ??? */
	md.pagex.page21.sectype = sectype & ST_MASK;
	md.pagex.page21.track = 0;	/* 0 : create new track */

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

static int 
write_track_philips(SCSI *usalp, 
							long track /* track number 0 == new track */, 
                    	int sectype)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
/*	scmd->flags = SCG_DISRE_ENA;*/
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xE6;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, track);
	scmd->cdb.g1_cdb.res6 = sectype & ST_MASK;

	usalp->cmdname = "philips write_track";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static int 
open_track_philips(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	if (select_secsize(usalp, trackp->secsize) < 0)
		return (-1);

	if (write_track_info_philips(usalp, trackp->sectype) < 0)
		return (-1);

	if (write_track_philips(usalp, 0, trackp->sectype) < 0)
		return (-1);

	return (0);
}

static int 
open_track_plasmon(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	if (select_secsize(usalp, trackp->secsize) < 0)
		return (-1);

	if (write_track_info_philips(usalp, trackp->sectype) < 0)
		return (-1);

	return (0);
}

static int 
open_track_oldphilips(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	if (write_track_philips(usalp, 0, trackp->sectype) < 0)
		return (-1);

	return (0);
}

static int 
open_track_yamaha(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	if (select_secsize(usalp, trackp->secsize) < 0)
		return (-1);

	if (write_track_philips(usalp, 0, trackp->sectype) < 0)
		return (-1);

	return (0);
}

static int 
close_track_philips(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	return (scsi_flush_cache(usalp, FALSE));
}

static int fixation_philips(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 8 * 60;		/* Needs up to 4 minutes */
	scmd->cdb.g1_cdb.cmd = 0xE9;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.count[1] =
			((track_base(trackp)->tracktype & TOCF_MULTI) ? 8 : 0) |
			(track_base(trackp)->tracktype & TOC_MASK);

	usalp->cmdname = "philips fixation";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static const char *sd_cdd_521_error_str[] = {
	"\003\000tray out",				/* 0x03 */
	"\062\000write data error with CU",		/* 0x32 */	/* Yamaha */
	"\063\000monitor atip error",			/* 0x33 */
	"\064\000absorbtion control error",		/* 0x34 */
#ifdef	YAMAHA_CDR_100
	/* Is this the same ??? */
	"\120\000write operation in progress",		/* 0x50 */
#endif
	"\127\000unable to read TOC/PMA/Subcode/ATIP",	/* 0x57 */
	"\132\000operator medium removal request",	/* 0x5a */
	"\145\000verify failed",			/* 0x65 */
	"\201\000illegal track number",			/* 0x81 */
	"\202\000command now not valid",		/* 0x82 */
	"\203\000medium removal is prevented",		/* 0x83 */
	"\204\000tray out",				/* 0x84 */
	"\205\000track at one not in PMA",		/* 0x85 */
	"\240\000stopped on non data block",		/* 0xa0 */
	"\241\000invalid start adress",			/* 0xa1 */
	"\242\000attampt to cross track-boundary",	/* 0xa2 */
	"\243\000illegal medium",			/* 0xa3 */
	"\244\000disk write protected",			/* 0xa4 */
	"\245\000application code conflict",		/* 0xa5 */
	"\246\000illegal blocksize for command",	/* 0xa6 */
	"\247\000blocksize conflict",			/* 0xa7 */
	"\250\000illegal transfer length",		/* 0xa8 */
	"\251\000request for fixation failed",		/* 0xa9 */
	"\252\000end of medium reached",		/* 0xaa */
#ifdef	REAL_CDD_521
	"\253\000non reserved reserved track",		/* 0xab */
#else
	"\253\000illegal track number",			/* 0xab */
#endif
	"\254\000data track length error",		/* 0xac */
	"\255\000buffer under run",			/* 0xad */
	"\256\000illegal track mode",			/* 0xae */
	"\257\000optical power calibration error",	/* 0xaf */
	"\260\000calibration area almost full",		/* 0xb0 */
	"\261\000current program area empty",		/* 0xb1 */
	"\262\000no efm at search address",		/* 0xb2 */
	"\263\000link area encountered",		/* 0xb3 */
	"\264\000calibration area full",		/* 0xb4 */
	"\265\000dummy data blocks added",		/* 0xb5 */
	"\266\000block size format conflict",		/* 0xb6 */
	"\267\000current command aborted",		/* 0xb7 */
	"\270\000program area not empty",		/* 0xb8 */
#ifdef	YAMAHA_CDR_100
	/* Used while writing lead in in DAO */
	"\270\000write leadin in progress",		/* 0xb8 */
#endif
	"\271\000parameter list too large",		/* 0xb9 */
	"\277\000buffer overflow",			/* 0xbf */	/* Yamaha */
	"\300\000no barcode available",			/* 0xc0 */
	"\301\000barcode reading error",		/* 0xc1 */
	"\320\000recovery needed",			/* 0xd0 */
	"\321\000cannot recover track",			/* 0xd1 */
	"\322\000cannot recover pma",			/* 0xd2 */
	"\323\000cannot recover leadin",		/* 0xd3 */
	"\324\000cannot recover leadout",		/* 0xd4 */
	"\325\000cannot recover opc",			/* 0xd5 */
	"\326\000eeprom failure",			/* 0xd6 */
	"\340\000laser current over",			/* 0xe0 */	/* Yamaha */
	"\341\000servo adjustment over",		/* 0xe0 */	/* Yamaha */
	NULL
};

static const char *sd_ro1420_error_str[] = {
	"\004\000logical unit is in process of becoming ready", /* 04 00 */
	"\011\200radial skating error",				/* 09 80 */
	"\011\201sledge servo failure",				/* 09 81 */
	"\011\202pll no lock",					/* 09 82 */
	"\011\203servo off track",				/* 09 83 */
	"\011\204atip sync error",				/* 09 84 */
	"\011\205atip/subcode jumped error",			/* 09 85 */
	"\127\300subcode not found",				/* 57 C0 */
	"\127\301atip not found",				/* 57 C1 */
	"\127\302no atip or subcode",				/* 57 C2 */
	"\127\303pma error",					/* 57 C3 */
	"\127\304toc read error",				/* 57 C4 */
	"\127\305disk informatoion error",			/* 57 C5 */
	"\144\200read in leadin",				/* 64 80 */
	"\144\201read in leadout",				/* 64 81 */
	"\201\000illegal track",				/* 81 00 */
	"\202\000command not now valid",			/* 82 00 */
	"\220\000reserve track check error",			/* 90 00 */
	"\220\001verify blank error",				/* 90 01 */
	"\221\001mode of last track error",			/* 91 01 */
	"\222\000header search error",				/* 92 00 */
	"\230\001header monitor error",				/* 98 01 */
	"\230\002edc error",					/* 98 02 */
	"\230\003read link, run-in run-out",			/* 98 03 */
	"\230\004last one block error",				/* 98 04 */
	"\230\005illegal blocksize",				/* 98 05 */
	"\230\006not all data transferred",			/* 98 06 */
	"\230\007cdbd over run error",				/* 98 07 */
	"\240\000stopped on non_data block",			/* A0 00 */
	"\241\000invalid start address",			/* A1 00 */
	"\243\000illegal medium",				/* A3 00 */
	"\246\000illegal blocksize for command",		/* A6 00 */
	"\251\000request for fixation failed",			/* A9 00 */
	"\252\000end of medium reached",			/* AA 00 */
	"\253\000illegal track number",				/* AB 00 */
	"\255\000buffer underrun",				/* AD 00 */
	"\256\000illegal track mode",				/* AE 00 */
	"\257\200power range error",				/* AF 80 */
	"\257\201moderation error",				/* AF 81 */
	"\257\202beta upper range error",			/* AF 82 */
	"\257\203beta lower range error",			/* AF 83 */
	"\257\204alpha upper range error",			/* AF 84 */
	"\257\205alpha lower range error",			/* AF 85 */
	"\257\206alpha and power range error",			/* AF 86 */
	"\260\000calibration area almost full",			/* B0 00 */
	"\261\000current program area empty",			/* B1 00 */
	"\262\000no efm at search address",			/* B2 00 */
	"\264\000calibration area full",			/* B4 00 */
	"\265\000dummy blocks added",				/* B5 00 */
	"\272\000write audio on reserved track",		/* BA 00 */
	"\302\200syscon rom error",				/* C2 80 */
	"\302\201syscon ram error",				/* C2 81 */
	"\302\220efm encoder error",				/* C2 90 */
	"\302\221efm decoder error",				/* C2 91 */
	"\302\222servo ic error",				/* C2 92 */
	"\302\223motor controller error",			/* C2 93 */
	"\302\224dac error",					/* C2 94 */
	"\302\225syscon eeprom error",				/* C2 95 */
	"\302\240block decoder communication error",		/* C2 A0 */
	"\302\241block encoder communication error",		/* C2 A1 */
	"\302\242block encoder/decoder path error",		/* C2 A2 */
	"\303\000CD-R engine selftest error",			/* C3 xx */
	"\304\000buffer parity error",				/* C4 00 */
	"\305\000data transfer error",				/* C5 00 */
	"\340\00012V failure",					/* E0 00 */
	"\341\000undefined syscon error",			/* E1 00 */
	"\341\001syscon communication error",			/* E1 01 */
	"\341\002unknown syscon error",				/* E1 02 */
	"\342\000syscon not ready",				/* E2 00 */
	"\343\000command rejected",				/* E3 00 */
	"\344\000command not accepted",				/* E4 00 */
	"\345\000verify error at beginning of track",		/* E5 00 */
	"\345\001verify error at ending of track",		/* E5 01 */
	"\345\002verify error at beginning of lead-in",		/* E5 02 */
	"\345\003verify error at ending of lead-in",		/* E5 03 */
	"\345\004verify error at beginning of lead-out",	/* E5 04 */
	"\345\005verify error at ending of lead-out",		/* E5 05 */
	"\377\000command phase timeout error",			/* FF 00 */
	"\377\001data in phase timeout error",			/* FF 01 */
	"\377\002data out phase timeout error",			/* FF 02 */
	"\377\003status phase timeout error",			/* FF 03 */
	"\377\004message in phase timeout error",		/* FF 04 */
	"\377\005message out phase timeout error",		/* FF 05 */
	NULL
};

static int 
philips_attach(SCSI *usalp, cdr_t *dp)
{
	usal_setnonstderrs(usalp, sd_cdd_521_error_str);
	return (0);
}

static int 
plasmon_attach(SCSI *usalp, cdr_t *dp)
{
	usalp->inq->data_format = 1;	/* Correct the ly */

	usal_setnonstderrs(usalp, sd_cdd_521_error_str);
	return (0);
}

static int 
ricoh_attach(SCSI *usalp, cdr_t *dp)
{
	if (dp == &cdr_ricoh_ro1060) {
		errmsgno(EX_BAD, "No support for Ricoh RO-1060C\n");
		return (-1);
	}
	usal_setnonstderrs(usalp, sd_ro1420_error_str);
	return (0);
}

static int 
philips_getlilo(SCSI *usalp, long *lilenp, long *lolenp)
{
	char	buf[4];
	long	li, lo;
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = buf;
	scmd->size = sizeof (buf);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xEE;	/* Read session info */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, sizeof (buf));

	usalp->cmdname = "philips read session info";

	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usalp->verbose)
		usal_prbytes("Session info data: ", (Uchar *)buf, sizeof (buf) - usal_getresid(usalp));

	li = a_to_u_2_byte(buf);
	lo = a_to_u_2_byte(&buf[2]);

	if (lilenp)
		*lilenp = li;
	if (lolenp)
		*lolenp = lo;

	return (0);
}
