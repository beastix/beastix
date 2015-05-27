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

/* @(#)drv_mmc.c	1.163 06/01/12 Copyright 1997-2006 J. Schilling */
/*
 *	CDR device implementation for
 *	SCSI-3/mmc conforming drives
 *	e.g. Yamaha CDR-400, Ricoh MP6200
 *
 *	Copyright (c) 1997-2006 J. Schilling
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

/*#define	DEBUG*/
#define	PRINT_ATIP
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <timedefs.h>

#include <utypes.h>
#include <btorder.h>
#include <intcvt.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsimmc.h"
#include "mmcvendor.h"
#include "wodim.h"

extern	char	*driveropts;

extern	int	debug;
extern	int	lverbose;
extern	int	xdebug;

static	int	curspeed = 1;

static	char	clv_to_speed[16] = {
/*		0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 */
		0, 2, 4, 6, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static	char	hs_clv_to_speed[16] = {
/*		0  1  2  3  4  5  6  7   8  9 10 11 12 13 14 15 */
		0, 2, 4, 6, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static	char	us_clv_to_speed[16] = {
/*		0  1  2  3  4  5   6  7  8   9   10  11 12 13 14 15 */
		0, 2, 4, 8, 0, 0, 16, 0, 24, 32, 40, 48, 0, 0, 0, 0
};

#ifdef	__needed__
static	int	mmc_load(SCSI *usalp, cdr_t *dp);
static	int	mmc_unload(SCSI *usalp, cdr_t *dp);
#endif
void				mmc_opthelp(cdr_t *dp, int excode);
char				*hasdrvopt(char *optstr, char *optname);
static	cdr_t	*identify_mmc(SCSI *usalp, cdr_t *, struct scsi_inquiry *);
static	int	attach_mmc(SCSI *usalp, cdr_t *);
static   int   attach_mdvd(SCSI *usalp, cdr_t *);
int				check_writemodes_mmc(SCSI *usalp, cdr_t *dp);
int     			check_writemodes_mdvd(SCSI *usalp, cdr_t *dp);
static	int	deflt_writemodes_mmc(SCSI *usalp, BOOL reset_dummy);
static   int   deflt_writemodes_mdvd(SCSI *usalp, BOOL reset_dummy);
static	int	get_diskinfo(SCSI *usalp, struct disk_info *dip);
static	void	di_to_dstat(struct disk_info *dip, dstat_t *dsp);
static	int	get_atip(SCSI *usalp, struct atipinfo *atp);
#ifdef	PRINT_ATIP
static	int	get_pma(SCSI *usalp);
#endif
static	int	init_mmc(SCSI *usalp, cdr_t *dp);
static	int	getdisktype_mmc(SCSI *usalp, cdr_t *dp);
static  int   getdisktype_mdvd(SCSI *usalp, cdr_t *dp);
static	int	speed_select_mmc(SCSI *usalp, cdr_t *dp, int *speedp);
static  int   speed_select_mdvd(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	mmc_set_speed(SCSI *usalp, int readspeed, int writespeed, 
									  int rotctl);
static	int	next_wr_addr_mmc(SCSI *usalp, track_t *trackp, long *ap);
static  int   next_wr_addr_mdvd(SCSI *usalp, track_t *trackp, long *ap);
static	int	write_leadin_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	open_track_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static  int   open_track_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	close_track_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static  int   close_track_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp); 
static	int	open_session_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static  int   open_session_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	waitfix_mmc(SCSI *usalp, int secs);
static	int	fixate_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static  int   fixate_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	blank_mmc(SCSI *usalp, cdr_t *dp, long addr, int blanktype);
static	int	format_mdvd(SCSI *usalp, cdr_t *dp, int formattype);
static	int	send_opc_mmc(SCSI *usalp, caddr_t, int cnt, int doopc);
static	int	opt1_mmc(SCSI *usalp, cdr_t *dp);
static	int	opt1_mdvd(SCSI *usalp, cdr_t *dp);
static	int	opt2_mmc(SCSI *usalp, cdr_t *dp);
static	int	scsi_sony_write(SCSI *usalp, caddr_t bp, long sectaddr, long size, 
										 int blocks, BOOL islast);
static	int	gen_cue_mmc(track_t *trackp, void *vcuep, BOOL needgap);
static	void	fillcue(struct mmc_cue *cp, int ca, int tno, int idx, int dataform,
							 int scms, msf_t *mp);
static	int	send_cue_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp);
static 	int	stats_mmc(SCSI *usalp, cdr_t *dp);
static 	BOOL	mmc_isplextor(SCSI *usalp);
static 	BOOL	mmc_isyamaha(SCSI *usalp);
static 	void	do_varirec_plextor(SCSI *usalp);
static 	int	do_gigarec_plextor(SCSI *usalp);
static 	int	drivemode_plextor(SCSI *usalp, caddr_t bp, int cnt, int modecode, 
											void *modeval);
static 	int	drivemode2_plextor(SCSI *usalp, caddr_t bp, int cnt, int modecode, 
											 void *modeval);
static 	int	check_varirec_plextor(SCSI *usalp);
static 	int	check_gigarec_plextor(SCSI *usalp);
static 	int	varirec_plextor(SCSI *usalp, BOOL on, int val);
static 	int	gigarec_plextor(SCSI *usalp, int val);
static 	Int32_t	gigarec_mult(int code, Int32_t	val);
static 	int	check_ss_hide_plextor(SCSI *usalp);
static 	int	check_speed_rd_plextor(SCSI *usalp);
static 	int	check_powerrec_plextor(SCSI *usalp);
static 	int	ss_hide_plextor(SCSI *usalp, BOOL do_ss, BOOL do_hide);
static 	int	speed_rd_plextor(SCSI *usalp, BOOL do_speedrd);
static 	int	powerrec_plextor(SCSI *usalp, BOOL do_powerrec);
static 	int	get_speeds_plextor(SCSI *usalp, int *selp, int *maxp, int *lastp);
static 	int	bpc_plextor(SCSI *usalp, int mode, int *bpp);
static 	int	set_audiomaster_yamaha(SCSI *usalp, cdr_t *dp, BOOL keep_mode);

struct 	ricoh_mode_page_30 * get_justlink_ricoh(SCSI *usalp, Uchar *mode);
static 	int	force_speed_yamaha(SCSI *usalp, int readspeed, int writespeed);
static 	BOOL	get_tattoo_yamaha(SCSI *usalp, BOOL print, Int32_t *irp, 
										Int32_t *orp);
static 	int	do_tattoo_yamaha(SCSI *usalp, FILE *f);
static 	int	yamaha_write_buffer(SCSI *usalp, int mode, int bufferid, long offset,
										  long parlen, void *buffer, long buflen);
static	int	dvd_dual_layer_split(SCSI *usalp, cdr_t *dp, long tsize);

#ifdef	__needed__
static int 
mmc_load(SCSI *usalp, cdr_t *dp)
{
	return (scsi_load_unload(usalp, 1));
}

static int 
mmc_unload(SCSI *usalp, cdr_t *dp)
{
	return (scsi_load_unload(usalp, 0));
}
#endif

/*
 * MMC CD-writer
 */
cdr_t	cdr_mmc = {
	0, 0,
	CDR_SWABAUDIO,
	CDR_CDRW_ALL,
	372, 372,
	"mmc_cdr",
	"generic SCSI-3/mmc   CD-R/CD-RW driver",
	0,
	(dstat_t *)0,
	identify_mmc,
	attach_mmc,
	init_mmc,
	getdisktype_mmc,
	scsi_load,
	scsi_unload,
	read_buff_cap,
	cmd_dummy,					/* check_recovery */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_mmc,
	select_secsize,
	next_wr_addr_mmc,
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	scsi_cdr_write,
	gen_cue_mmc,
	send_cue_mmc,
	write_leadin_mmc,
	open_track_mmc,
	close_track_mmc,
	open_session_mmc,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	fixate_mmc,
	stats_mmc,
	blank_mmc,
	format_dummy,
	send_opc_mmc,
	opt1_mmc,
	opt2_mmc,
};

cdr_t   cdr_mdvd = {
	0, 0,
	CDR_SWABAUDIO,
	CDR_CDRW_ALL,
	370,370,
	"mmc_mdvd",
	"generic SCSI-3/mmc DVD-R(W) driver",
	0,
	(dstat_t *)0,
	identify_mmc,
	attach_mdvd,
	init_mmc,
	getdisktype_mdvd,
	scsi_load,
	scsi_unload,
	read_buff_cap,
	cmd_dummy,                                       /* check_recovery */
	(int(*)__PR((SCSI *, cdr_t *, int)))cmd_dummy,   /* recover     */
	speed_select_mdvd,
	select_secsize,
	next_wr_addr_mdvd,
	(int(*)(SCSI *, Ulong))cmd_ill,   /* reserve_track        */
	scsi_cdr_write,
	(int(*)__PR((track_t *, void *, BOOL)))cmd_dummy, /* gen_cue */
	(int(*)__PR((SCSI *usalp, cdr_t *, track_t *)))cmd_dummy, /* send_cue */
	write_leadin_mmc,
	open_track_mdvd,
	close_track_mdvd,
	open_session_mdvd,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	fixate_mdvd,
	stats_mmc,
	blank_mmc,
	format_mdvd,
	send_opc_mmc,
	opt1_mdvd,
	opt2_mmc,
	dvd_dual_layer_split,
};

/*
 * Sony MMC CD-writer
 */
cdr_t	cdr_mmc_sony = {
	0, 0,
	CDR_SWABAUDIO,
	CDR_CDRW_ALL,
	372, 372,
	"mmc_cdr_sony",
	"generic SCSI-3/mmc   CD-R/CD-RW driver (Sony 928 variant)",
	0,
	(dstat_t *)0,
	identify_mmc,
	attach_mmc,
	init_mmc,
	getdisktype_mmc,
	scsi_load,
	scsi_unload,
	read_buff_cap,
	cmd_dummy,					/* check_recovery */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_mmc,
	select_secsize,
	next_wr_addr_mmc,
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	scsi_sony_write,
	gen_cue_mmc,
	send_cue_mmc,
	write_leadin_mmc,
	open_track_mmc,
	close_track_mmc,
	open_session_mmc,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	fixate_mmc,
	cmd_dummy,				/* stats		*/
	blank_mmc,
	format_dummy,
	send_opc_mmc,
	opt1_mmc,
	opt2_mmc,
};

/*
 * SCSI-3/mmc conformant CD-ROM drive
 */
cdr_t	cdr_cd = {
	0, 0,
	CDR_ISREADER|CDR_SWABAUDIO,
	CDR_CDRW_NONE,
	372, 372,
	"mmc_cd",
	"generic SCSI-3/mmc   CD-ROM driver",
	0,
	(dstat_t *)0,
	identify_mmc,
	attach_mmc,
	cmd_dummy,
	drive_getdisktype,
	scsi_load,
	scsi_unload,
	read_buff_cap,
	cmd_dummy,					/* check_recovery */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_mmc,
	select_secsize,
	(int(*)(SCSI *usalp, track_t *, long *))cmd_ill,	/* next_wr_addr		*/
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	scsi_cdr_write,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_mmc,
	close_track_mmc,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,	/* fixation */
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

/*
 * Old pre SCSI-3/mmc CD drive
 */
cdr_t	cdr_oldcd = {
	0, 0,
	CDR_ISREADER,
	CDR_CDRW_NONE,
	372, 372,
	"scsi2_cd",
	"generic SCSI-2       CD-ROM driver",
	0,
	(dstat_t *)0,
	identify_mmc,
	drive_attach,
	cmd_dummy,
	drive_getdisktype,
	scsi_load,
	scsi_unload,
	buf_dummy,
	cmd_dummy,					/* check_recovery */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_mmc,
	select_secsize,
	(int(*)(SCSI *usal, track_t *, long *))cmd_ill,	/* next_wr_addr		*/
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	scsi_cdr_write,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_mmc,
	close_track_mmc,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset_philips,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,	/* fixation */
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

/*
 * SCSI-3/mmc conformant CD or DVD writer
 * Checks the current medium and then returns either cdr_mmc or cdr_dvd
 */
cdr_t	cdr_cd_dvd = {
	0, 0,
	CDR_SWABAUDIO,
	CDR_CDRW_ALL,
	372, 372,
	"mmc_cd_dvd",
	"generic SCSI-3/mmc   CD/DVD driver (checks media)",
	0,
	(dstat_t *)0,
	identify_mmc,
	attach_mmc,
	cmd_dummy,
	drive_getdisktype,
	scsi_load,
	scsi_unload,
	read_buff_cap,
	cmd_dummy,					/* check_recovery */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_mmc,
	select_secsize,
	(int(*)(SCSI *usalp, track_t *, long *))cmd_ill,	/* next_wr_addr		*/
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	scsi_cdr_write,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	no_sendcue,
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_mmc,
	close_track_mmc,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy,	/* fixation */
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

void 
mmc_opthelp(cdr_t *dp, int excode)
{
	BOOL	haveopts = FALSE;

	fprintf(stderr, "Driver options:\n");
	if (dp->cdr_flags & CDR_BURNFREE) {
		fprintf(stderr, "burnfree	Prepare writer to use BURN-Free technology\n");
		fprintf(stderr, "noburnfree	Disable using BURN-Free technology\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_VARIREC) {
		fprintf(stderr, "varirec=val	Set VariRec Laserpower to -2, -1, 0, 1, 2\n");
		fprintf(stderr, "		Only works for audio and if speed is set to 4\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_GIGAREC) {
		fprintf(stderr, "gigarec=val	Set GigaRec capacity ratio to 0.6, 0.7, 0.8, 1.0, 1.2, 1.3, 1.4\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_AUDIOMASTER) {
		fprintf(stderr, "audiomaster	Turn Audio Master feature on (SAO CD-R Audio/Data only)\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_FORCESPEED) {
		fprintf(stderr, "forcespeed	Tell the drive to force speed even for low quality media\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_SPEEDREAD) {
		fprintf(stderr, "speedread	Tell the drive to read as fast as possible\n");
		fprintf(stderr, "nospeedread	Disable to read as fast as possible\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_DISKTATTOO) {
		fprintf(stderr, "tattooinfo	Print image size info for DiskT@2 feature\n");
		fprintf(stderr, "tattoofile=name	Use 'name' as DiskT@2 image file\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_SINGLESESS) {
		fprintf(stderr, "singlesession	Tell the drive to behave as single session only drive\n");
		fprintf(stderr, "nosinglesession	Disable single session only mode\n");
		haveopts = TRUE;
	}
	if (dp->cdr_flags & CDR_HIDE_CDR) {
		fprintf(stderr, "hidecdr		Tell the drive to hide CD-R media\n");
		fprintf(stderr, "nohidecdr	Disable hiding CD-R media\n");
		haveopts = TRUE;
	}
	if (!haveopts) {
		fprintf(stderr, "None supported for this drive.\n");
	}
	exit(excode);
}

char *
hasdrvopt(char *optstr, char *optname)
{
	char	*ep;
	char	*np;
	char	*ret = NULL;
	int	optnamelen;
	int	optlen;
	BOOL	not = FALSE;

	if (optstr == NULL)
		return (ret);

	optnamelen = strlen(optname);

	while (*optstr) {
		not = FALSE;			/* Reset before every token */
		if ((ep = strchr(optstr, ',')) != NULL) {
			optlen = ep - optstr;
			np = &ep[1];
		} else {
			optlen = strlen(optstr);
			np = &optstr[optlen];
		}
		if ((ep = strchr(optstr, '=')) != NULL) {
			if (ep < np)
				optlen = ep - optstr;
		}
		if (optstr[0] == '!') {
			optstr++;
			optlen--;
			not = TRUE;
		}
		if (strncmp(optstr, "no", 2) == 0) {
			optstr += 2;
			optlen -= 2;
			not = TRUE;
		}
		if (strncmp(optstr, optname, optlen) == 0) {
			ret = &optstr[optlen];
			break;
		}
		optstr = np;
	}
	if (ret != NULL) {
		if (*ret == ',' || *ret == '\0') {
			if (not)
				return ("0");
			return ("1");
		}
		if (*ret == '=') {
			if (not)
				return (NULL);
			return (++ret);
		}
	}
	return (ret);
}

static cdr_t *
identify_mmc(SCSI *usalp, cdr_t *dp, struct scsi_inquiry *ip)
{
	BOOL	cdrr	 = FALSE;	/* Read CD-R	*/
	BOOL	cdwr	 = FALSE;	/* Write CD-R	*/
	BOOL	cdrrw	 = FALSE;	/* Read CD-RW	*/
	BOOL	cdwrw	 = FALSE;	/* Write CD-RW	*/
	BOOL	dvdwr	 = FALSE;	/* DVD writer	*/
	BOOL	is_dvd	 = FALSE;	/* use DVD driver*/
	Uchar	mode[0x100];
	struct	cd_mode_page_2A *mp;
	int	profile;

	if (ip->type != INQ_WORM && ip->type != INQ_ROMD)
		return ((cdr_t *)0);

	allow_atapi(usalp, TRUE); /* Try to switch to 10 byte mode cmds */

	usalp->silent++;
	mp = mmc_cap(usalp, mode);	/* Get MMC capabilities */
	usalp->silent--;
	if (mp == NULL)
		return (&cdr_oldcd);	/* Pre SCSI-3/mmc drive		*/

	/*
	 * At this point we know that we have a SCSI-3/mmc compliant drive.
	 * Unfortunately ATAPI drives violate the SCSI spec in returning
	 * a response data format of '1' which from the SCSI spec would
	 * tell us not to use the "PF" bit in mode select. As ATAPI drives
	 * require the "PF" bit to be set, we 'correct' the inquiry data.
	 *
	 * XXX xxx_identify() should not have any side_effects ??
	 */
	if (ip->data_format < 2)
		ip->data_format = 2;

	/*
	 * First handle exceptions....
	 */
	if (strncmp(ip->vendor_info, "SONY", 4) == 0 &&
	    strncmp(ip->prod_ident, "CD-R   CDU928E", 14) == 0) {
		return (&cdr_mmc_sony);
	}

	/*
	 * Now try to do it the MMC-3 way....
	 */
	profile = get_curprofile(usalp);
	if (xdebug)
		printf("Current profile: 0x%04X\n", profile);
	if (profile == 0) {
		if (xdebug)
			print_profiles(usalp);
		/*
		 * If the current profile is 0x0000, then the
		 * drive does not know about the media. First
		 * close the tray and then try to issue the
		 * get_curprofile() command again.
		 */
		usalp->silent++;
		load_media(usalp, dp, FALSE);
		usalp->silent--;
		profile = get_curprofile(usalp);
		scsi_prevent_removal(usalp, 0);
		if (xdebug)
			printf("Current profile: 0x%04X\n", profile);
	}
	if (profile >= 0) {
		if (lverbose)
			print_profiles(usalp);
		if (profile == 0 || (profile >= 0x10 && profile <= 0x15) || profile > 0x19) {
		    /*
		     * 10h DVD-ROM
		     * 11h DVD-R
		     * 12h DVD-RAM
		     * 13h DVD-RW (Restricted overwrite)
		     * 14h DVD-RW (Sequential recording)
		     * 1Ah DVD+RW
		     * 1Bh DVD+R
		     * 2Bh DVD+R DL
		     * 
		     */
		    if (profile == 0x11 || profile == 0x13 || profile == 0x14 || profile == 0x1A || profile == 0x1B || profile == 0x2B) {
			is_dvd = TRUE;
			dp = &cdr_mdvd;
		    } else {
			is_dvd = FALSE;
			dp = &cdr_cd;

			if (profile == 0) {		/* No Medium */
				BOOL	is_cdr = FALSE;

				/*
				 * Check for CD-writer
				 */
				get_wproflist(usalp, &is_cdr, NULL,
							NULL, NULL);
				if (is_cdr)
					return (&cdr_mmc);
				/*
				 * Other MMC-3 drive without media
				 */
				return (dp);
			} if (profile == 0x12) {	/* DVD-RAM */
				errmsgno(EX_BAD,
				"Found unsupported DVD-RAM media.\n");
				return (dp);
			}
		    }
		}
	} else {
		if (xdebug)
			printf("Drive is pre MMC-3\n");
	}

	mmc_getval(mp, &cdrr, &cdwr, &cdrrw, &cdwrw, NULL, &dvdwr);

	if (!cdwr && !cdwrw) {	/* SCSI-3/mmc CD drive		*/
		/*
		 * If the drive does not support to write CD's, we select the
		 * CD-ROM driver here. If we have DVD-R/DVD-RW support compiled
		 * in, we may later decide to switch to the DVD driver.
		 */
		dp = &cdr_cd;
	} else {
		/*
		 * We need to set the driver to cdr_mmc because we may come
		 * here with driver set to cdr_cd_dvd which is not a driver
		 * that may be used for actual CD/DVD writing.
		 */
		dp = &cdr_mmc;
	}

/*#define	DVD_DEBUG*/
#ifdef	DVD_DEBUG
	if (1) {	/* Always check for DVD media in debug mode */
#else
	if ((cdwr || cdwrw) && dvdwr) {
#endif
		char	xb[32];

#ifndef	DVD_DEBUG
		usalp->silent++;
#else
		fprintf(stderr, "identify_dvd: checking for DVD media\n");
#endif
		if (read_dvd_structure(usalp, (caddr_t)xb, 32, 0, 0, 0) >= 0) {
			/*
			 * If read DVD structure is supported and works, then
			 * we must have a DVD media in the drive. Signal to
			 * use the DVD driver.
			 */
			is_dvd = TRUE;
		} else {
			if (usal_sense_key(usalp) == SC_NOT_READY) {
				/*
				 * If the SCSI sense key is NOT READY, then the
				 * drive does not know about the media. First
				 * close the tray and then try to issue the
				 * read_dvd_structure() command again.
				 */
				load_media(usalp, dp, FALSE);
				if (read_dvd_structure(usalp, (caddr_t)xb, 32, 0, 0, 0) >= 0) {
					is_dvd = TRUE;
				}
				scsi_prevent_removal(usalp, 0);
			}
		}
#ifndef	DVD_DEBUG
		usalp->silent--;
#else
		fprintf(stderr, "identify_dvd: is_dvd: %d\n", is_dvd);
#endif
	}
	if (is_dvd) {
     if(lverbose>2) 
        fprintf(stderr, "Found DVD media: using cdr_mdvd.\n");  
     dp = &cdr_mdvd; 
	}
	dp->profile = profile;
	dp->is_dvd = is_dvd;
	return (dp);
}

static int 
attach_mmc(SCSI *usalp, cdr_t *dp)
{
	int	ret;
	Uchar	mode[0x100];
	struct	cd_mode_page_2A *mp;
	struct	ricoh_mode_page_30 *rp = NULL;

	allow_atapi(usalp, TRUE); /* Try to switch to 10 byte mode cmds */

	usalp->silent++;
	mp = mmc_cap(usalp, NULL); /* Get MMC capabilities in allocated mp */
	usalp->silent--;
	if (mp == NULL)
		return (-1);	/* Pre SCSI-3/mmc drive		*/

	dp->cdr_cdcap = mp;	/* Store MMC cap pointer	*/

	dp->cdr_dstat->ds_dr_max_rspeed = a_to_u_2_byte(mp->max_read_speed)/176;
	if (dp->cdr_dstat->ds_dr_max_rspeed == 0)
		dp->cdr_dstat->ds_dr_max_rspeed = 372;
	dp->cdr_dstat->ds_dr_cur_rspeed = a_to_u_2_byte(mp->cur_read_speed)/176;
	if (dp->cdr_dstat->ds_dr_cur_rspeed == 0)
		dp->cdr_dstat->ds_dr_cur_rspeed = 372;

	dp->cdr_dstat->ds_dr_max_wspeed = a_to_u_2_byte(mp->max_write_speed)/176;
	if (mp->p_len >= 28)
		dp->cdr_dstat->ds_dr_cur_wspeed = a_to_u_2_byte(mp->v3_cur_write_speed)/176;
	else
		dp->cdr_dstat->ds_dr_cur_wspeed = a_to_u_2_byte(mp->cur_write_speed)/176;

	if (dp->cdr_speedmax > dp->cdr_dstat->ds_dr_max_wspeed)
		dp->cdr_speedmax = dp->cdr_dstat->ds_dr_max_wspeed;

	if (dp->cdr_speeddef > dp->cdr_speedmax)
		dp->cdr_speeddef = dp->cdr_speedmax;

	rp = get_justlink_ricoh(usalp, mode);

	if (mp->p_len >= 28)
		dp->cdr_flags |= CDR_MMC3;
	if (mp->p_len >= 24)
		dp->cdr_flags |= CDR_MMC2;
	dp->cdr_flags |= CDR_MMC;

	if (mp->loading_type == LT_TRAY)
		dp->cdr_flags |= CDR_TRAYLOAD;
	else if (mp->loading_type == LT_CADDY)
		dp->cdr_flags |= CDR_CADDYLOAD;

	if (mp->BUF != 0) {
		dp->cdr_flags |= CDR_BURNFREE;
	} else if (rp) {
		if ((dp->cdr_cmdflags & F_DUMMY) && rp->TWBFS && rp->BUEFS)
			dp->cdr_flags |= CDR_BURNFREE;

		if (rp->BUEFS)
			dp->cdr_flags |= CDR_BURNFREE;
	}

	if (mmc_isplextor(usalp)) {
		if (check_varirec_plextor(usalp) >= 0)
			dp->cdr_flags |= CDR_VARIREC;

		if (check_gigarec_plextor(usalp) >= 0)
			dp->cdr_flags |= CDR_GIGAREC;

		if (check_ss_hide_plextor(usalp) >= 0)
			dp->cdr_flags |= CDR_SINGLESESS|CDR_HIDE_CDR;

		if (check_powerrec_plextor(usalp) >= 0)
			dp->cdr_flags |= CDR_FORCESPEED;

		if (check_speed_rd_plextor(usalp) >= 0)
			dp->cdr_flags |= CDR_SPEEDREAD;
	}
	if (mmc_isyamaha(usalp)) {
		if (set_audiomaster_yamaha(usalp, dp, FALSE) >= 0)
			dp->cdr_flags |= CDR_AUDIOMASTER;

		/*
		 * Starting with CRW 2200 / CRW 3200
		 */
		if ((mp->p_len+2) >= (unsigned)28)
			dp->cdr_flags |= CDR_FORCESPEED;

		if (get_tattoo_yamaha(usalp, FALSE, 0, 0))
			dp->cdr_flags |= CDR_DISKTATTOO;
	}

	if (rp && rp->AWSCS)
		dp->cdr_flags |= CDR_FORCESPEED;

#ifdef	FUTURE_ROTCTL
	if (mp->p_len >= 28) {
		int	val;

		val = dp->cdr_dstat->ds_dr_cur_wspeed;
		if (val == 0)
			val = 372;

		usalp->verbose++;
		if (scsi_set_speed(usalp, -1, val, ROTCTL_CAV) < 0) {
			fprintf(stderr, "XXX\n");
		}
		usalp->verbose--;
	}
#endif

	check_writemodes_mmc(usalp, dp);

	/* Enable Burnfree by default, it can be disabled later */
	if ((dp->cdr_flags & CDR_BURNFREE) != 0)
		dp->cdr_dstat->ds_cdrflags |= RF_BURNFREE;

	if (driveropts != NULL) {
		char	*p;

		if (strcmp(driveropts, "help") == 0) {
			mmc_opthelp(dp, 0);
		}

		p = hasdrvopt(driveropts, "varirec");
		if (p != NULL && (dp->cdr_flags & CDR_VARIREC) != 0) {
			dp->cdr_dstat->ds_cdrflags |= RF_VARIREC;
		}

		p = hasdrvopt(driveropts, "gigarec");
		if (p != NULL && (dp->cdr_flags & CDR_GIGAREC) != 0) {
			dp->cdr_dstat->ds_cdrflags |= RF_GIGAREC;
		}

		p = hasdrvopt(driveropts, "audiomaster");
		if (p != NULL && *p == '1' && (dp->cdr_flags & CDR_AUDIOMASTER) != 0) {
			dp->cdr_dstat->ds_cdrflags |= RF_AUDIOMASTER;
			dp->cdr_dstat->ds_cdrflags &= ~RF_BURNFREE;
		}
		p = hasdrvopt(driveropts, "forcespeed");
		if (p != NULL && *p == '1' && (dp->cdr_flags & CDR_FORCESPEED) != 0) {
			dp->cdr_dstat->ds_cdrflags |= RF_FORCESPEED;
		}
		p = hasdrvopt(driveropts, "tattooinfo");
		if (p != NULL && *p == '1' && (dp->cdr_flags & CDR_DISKTATTOO) != 0) {
			get_tattoo_yamaha(usalp, TRUE, 0, 0);
		}
		p = hasdrvopt(driveropts, "tattoofile");
		if (p != NULL && (dp->cdr_flags & CDR_DISKTATTOO) != 0) {
			FILE	*f;

			if ((f = fileopen(p, "rb")) == NULL)
				comerr("Cannot open '%s'.\n", p);

			if (do_tattoo_yamaha(usalp, f) < 0)
				errmsgno(EX_BAD, "Cannot do DiskT@2.\n");
			fclose(f);
		}
		p = hasdrvopt(driveropts, "singlesession");
		if (p != NULL && (dp->cdr_flags & CDR_SINGLESESS) != 0) {
			if (*p == '1') {
				dp->cdr_dstat->ds_cdrflags |= RF_SINGLESESS;
			} else if (*p == '0') {
				dp->cdr_dstat->ds_cdrflags &= ~RF_SINGLESESS;
			}
		}
		p = hasdrvopt(driveropts, "hidecdr");
		if (p != NULL && (dp->cdr_flags & CDR_HIDE_CDR) != 0) {
			if (*p == '1') {
				dp->cdr_dstat->ds_cdrflags |= RF_HIDE_CDR;
			} else if (*p == '0') {
				dp->cdr_dstat->ds_cdrflags &= ~RF_HIDE_CDR;
			}
		}
		p = hasdrvopt(driveropts, "speedread");
		if (p != NULL && (dp->cdr_flags & CDR_SPEEDREAD) != 0) {
			if (*p == '1') {
				dp->cdr_dstat->ds_cdrflags |= RF_SPEEDREAD;
			} else if (*p == '0') {
				dp->cdr_dstat->ds_cdrflags &= ~RF_SPEEDREAD;
			}
		}
	}

	if ((ret = get_supported_cdrw_media_types(usalp)) < 0) {
		dp->cdr_cdrw_support = CDR_CDRW_ALL;
		return (0);
	}
	dp->cdr_cdrw_support = ret;
	if (lverbose > 1)
		printf("Supported CD-RW media types: %02X\n", dp->cdr_cdrw_support);

	return (0);
}

static int 
attach_mdvd(SCSI *usalp, cdr_t *dp)
{
	struct  cd_mode_page_2A *mp;


	allow_atapi(usalp, TRUE);/* Try to switch to 10 byte mode cmds */

	usalp->silent++;
	mp = mmc_cap(usalp, NULL);/* Get MMC capabilities in allocated mp */
	usalp->silent--;
	if (mp == NULL)
		return (-1);    /* Pre SCSI-3/mmc drive         */

	dp->cdr_cdcap = mp;     /* Store MMC cap pointer        */

	dp->cdr_dstat->ds_dr_max_rspeed = a_to_u_2_byte(mp->max_read_speed)/1385;
	if (dp->cdr_dstat->ds_dr_max_rspeed == 0)
		dp->cdr_dstat->ds_dr_max_rspeed = 1385;
	dp->cdr_dstat->ds_dr_cur_rspeed = a_to_u_2_byte(mp->cur_read_speed)/1385;
	if (dp->cdr_dstat->ds_dr_cur_rspeed == 0)
		dp->cdr_dstat->ds_dr_cur_rspeed = 1385;

	dp->cdr_dstat->ds_dr_max_wspeed = a_to_u_2_byte(mp->max_write_speed)/1385;
	if (mp->p_len >= 28)
		dp->cdr_dstat->ds_dr_cur_wspeed = a_to_u_2_byte(mp->v3_cur_write_speed)/1385;
	else
		dp->cdr_dstat->ds_dr_cur_wspeed = a_to_u_2_byte(mp->cur_write_speed)/1385;

	if (dp->cdr_speedmax > dp->cdr_dstat->ds_dr_max_wspeed)
		dp->cdr_speedmax = dp->cdr_dstat->ds_dr_max_wspeed;

	if (dp->cdr_speeddef > dp->cdr_speedmax)
		dp->cdr_speeddef = dp->cdr_speedmax;


        if (mp->loading_type == LT_TRAY)
                dp->cdr_flags |= CDR_TRAYLOAD;
        else if (mp->loading_type == LT_CADDY)
                dp->cdr_flags |= CDR_CADDYLOAD;

        if (mp->BUF != 0)
                dp->cdr_flags |= CDR_BURNFREE;

        check_writemodes_mdvd(usalp, dp);

        if (driveropts != NULL) {
                if (strcmp(driveropts, "help") == 0) {
                        mmc_opthelp(dp, 0);
                }
        }

        return (0);
}

int 
check_writemodes_mmc(SCSI *usalp, cdr_t *dp)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	if (xdebug)
		printf("Checking possible write modes: ");

	/*
	 * Reset mp->test_write (-dummy) here.
	 */
	deflt_writemodes_mmc(usalp, TRUE);

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	usalp->silent++;
	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
		usalp->silent--;
		return (-1);
	}
	if (len == 0) {
		usalp->silent--;
		return (-1);
	}

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);
#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif

	/*
	 * mp->test_write has already been reset in deflt_writemodes_mmc()
	 * Do not reset mp->test_write (-dummy) here. It should be set
	 * only at one place and only one time.
	 */

	mp->write_type = WT_TAO;
	mp->track_mode = TM_DATA;
	mp->dbtype = DB_ROM_MODE1;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_TAO;
		if (xdebug)
			printf("TAO ");
	} else
		dp->cdr_flags &= ~CDR_TAO;

	mp->write_type = WT_PACKET;
	mp->track_mode |= TM_INCREMENTAL;
/*	mp->fp = (trackp->pktsize > 0) ? 1 : 0;*/
/*	i_to_4_byte(mp->packet_size, trackp->pktsize);*/
	mp->fp = 0;
	i_to_4_byte(mp->packet_size, 0);

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_PACKET;
		if (xdebug)
			printf("PACKET ");
	} else
		dp->cdr_flags &= ~CDR_PACKET;
	mp->fp = 0;
	i_to_4_byte(mp->packet_size, 0);
	mp->track_mode = TM_DATA;
	mp->write_type = WT_SAO;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_SAO;
		if (xdebug)
			printf("SAO ");
	} else
		dp->cdr_flags &= ~CDR_SAO;

	if (dp->cdr_flags & CDR_SAO) {
		mp->dbtype = DB_RAW_PQ;

#ifdef	__needed__
		if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
			dp->cdr_flags |= CDR_SRAW16;
			if (xdebug)
				printf("SAO/R16 ");
		}
#endif

		mp->dbtype = DB_RAW_PW;

		if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
			dp->cdr_flags |= CDR_SRAW96P;
			if (xdebug)
				printf("SAO/R96P ");
		}

		mp->dbtype = DB_RAW_PW_R;

		if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
			dp->cdr_flags |= CDR_SRAW96R;
			if (xdebug)
				printf("SAO/R96R ");
		}
	}

	mp->write_type = WT_RAW;
	mp->dbtype = DB_RAW_PQ;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_RAW;
		dp->cdr_flags |= CDR_RAW16;
		if (xdebug)
			printf("RAW/R16 ");
	}

	mp->dbtype = DB_RAW_PW;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_RAW;
		dp->cdr_flags |= CDR_RAW96P;
		if (xdebug)
			printf("RAW/R96P ");
	}

	mp->dbtype = DB_RAW_PW_R;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_RAW;
		dp->cdr_flags |= CDR_RAW96R;
		if (xdebug)
			printf("RAW/R96R ");
	}

	if (xdebug)
		printf("\n");

	/*
	 * Reset mp->test_write (-dummy) here.
	 */
	deflt_writemodes_mmc(usalp, TRUE);
	usalp->silent--;

	return (0);
}

int 
check_writemodes_mdvd(SCSI *usalp, cdr_t *dp)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	if (xdebug)
		printf("Checking possible write modes: ");

	deflt_writemodes_mdvd(usalp, FALSE);

	fillbytes((caddr_t)mode, sizeof(mode), '\0');

	usalp->silent++;
	if (!get_mode_params(usalp, 0x05, "DVD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
		usalp->silent--;
		return (-1);
	}
	if (len == 0) {
		usalp->silent--;
		return (-1);
	}

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof(struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	mp->test_write = 0;

	/*We only check for PACKET and SAO since these are the only supported modes for DVD */
	/*XXX these checks are irrelevant because they are not medium sensitive. ie the device returns 
	  error only when it does not support a given mode for ALL mediums. It should check using 
	  GET CONFIGURATION command.*/

	mp->write_type = WT_PACKET;
	mp->fp = 0;
	i_to_4_byte(mp->packet_size, 0);

	if (set_mode_params(usalp, "DVD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_PACKET;
		if (xdebug)
		  printf("PACKET ");
	} else
	  dp->cdr_flags &= ~CDR_PACKET;
	mp->fp = 0;
	i_to_4_byte(mp->packet_size, 0);
	mp->track_mode = TM_DATA; 


	mp->write_type = WT_SAO;

	if (set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
		dp->cdr_flags |= CDR_SAO;
		if (xdebug)
			printf("SAO ");
	} else
		dp->cdr_flags &= ~CDR_SAO;


	if (xdebug)
		printf("\n");

	deflt_writemodes_mdvd(usalp, TRUE);
	usalp->silent--;
	return (0);
}

static int 
deflt_writemodes_mmc(SCSI *usalp, BOOL reset_dummy)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	usalp->silent++;
	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
		usalp->silent--;
		return (-1);
	}
	if (len == 0) {
		usalp->silent--;
		return (-1);
	}

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);
#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
	fprintf(stderr, "Audio pause len: %d\n", a_to_2_byte(mp->audio_pause_len));
#endif

	/*
	 * This is the only place where we reset mp->test_write (-dummy)
	 */
	if (reset_dummy)
		mp->test_write = 0;

	/*
	 * Set default values:
	 * Write type = 01 (track at once)
	 * Track mode = 04 (CD-ROM)
	 * Data block type = 08 (CD-ROM)
	 * Session format = 00 (CD-ROM)
	 *
	 * XXX Note:	the same code appears in check_writemodes_mmc() and
	 * XXX		in speed_select_mmc().
	 */
	mp->write_type = WT_TAO;
	mp->track_mode = TM_DATA;
	mp->dbtype = DB_ROM_MODE1;
	mp->session_format = SES_DA_ROM; /* Matsushita has illegal def. value */

	i_to_2_byte(mp->audio_pause_len, 150);	/* LG has illegal def. value */

#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {

		mp->write_type	= WT_SAO;
		mp->LS_V	= 0;
		mp->copy	= 0;
		mp->fp		= 0;
		mp->multi_session  = MS_NONE;
		mp->host_appl_code = 0;

		if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1)) {
			usalp->silent--;
			return (-1);
		}
	}
	usalp->silent--;
	return (0);
}

static int 
deflt_writemodes_mdvd(SCSI *usalp, BOOL reset_dummy)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	fillbytes((caddr_t)mode, sizeof(mode), '\0');

	usalp->silent++;
	if (!get_mode_params(usalp, 0x05, "DVD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
		usalp->silent--;
		return (-1);
	}
	if (len == 0) {
		usalp->silent--;
		return (-1);
	}

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof(struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	mp->test_write = 0;
	/*
	 * This is the only place where we reset mp->test_write (-dummy) for DVD
	 */
	if (reset_dummy)
		mp->test_write = 0;

	/*
	 * Set default values:
	 * Write type = 02 (session at once)
	 *
	 * XXX Note:	the same code appears in check_writemodes_mmc() and
	 * XXX		in speed_select_mmc().
	 */
	mp->write_type = WT_SAO;
	mp->track_mode = TM_DATA; 
	mp->dbtype = DB_ROM_MODE1;
	mp->session_format = SES_DA_ROM;


	if (set_mode_params(usalp, "DVD write parameter", mode, len, 0, -1) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;
	return (0);
}

#ifdef	PRINT_ATIP
static	void	print_di(struct disk_info *dip);
static	void	atip_printspeed(char *fmt, int speedindex, char speedtab[]);
static	void	print_atip(SCSI *usalp, struct atipinfo *atp);
#endif	/* PRINT_ATIP */

static int 
get_diskinfo(SCSI *usalp, struct disk_info *dip)
{
	int	len;
	int	ret;

	fillbytes((caddr_t)dip, sizeof (*dip), '\0');

	/*
	 * Used to be 2 instead of 4 (now). But some Y2k ATAPI drives as used
	 * by IOMEGA create a DMA overrun if we try to transfer only 2 bytes.
	 */
/*	if (read_disk_info(usalp, (caddr_t)dip, 2) < 0)*/
	if (read_disk_info(usalp, (caddr_t)dip, 4) < 0)
		return (-1);
	len = a_to_u_2_byte(dip->data_len);
	len += 2;
	ret = read_disk_info(usalp, (caddr_t)dip, len);

#ifdef	DEBUG
	usal_prbytes("Disk info:", (Uchar *)dip,
				len-usal_getresid(usalp));
#endif
	return (ret);
}

static void 
di_to_dstat(struct disk_info *dip, dstat_t *dsp)
{
	dsp->ds_diskid = a_to_u_4_byte(dip->disk_id);
	if (dip->did_v)
		dsp->ds_flags |= DSF_DID_V;
	dsp->ds_disktype = dip->disk_type;
	dsp->ds_diskstat = dip->disk_status;
	dsp->ds_sessstat = dip->sess_status;
	if (dip->erasable)
		dsp->ds_flags |= DSF_ERA;

	dsp->ds_trfirst	   = dip->first_track;
	dsp->ds_trlast	   = dip->last_track_ls;
	dsp->ds_trfirst_ls = dip->first_track_ls;

	dsp->ds_maxblocks = msf_to_lba(dip->last_lead_out[1],
					dip->last_lead_out[2],
					dip->last_lead_out[3], TRUE);
	/*
	 * Check for 0xFF:0xFF/0xFF which is an indicator for a complete disk
	 */
	if (dsp->ds_maxblocks == 1166730)
		dsp->ds_maxblocks = -1L;

	dsp->ds_first_leadin = msf_to_lba(dip->last_lead_in[1],
					dip->last_lead_in[2],
					dip->last_lead_in[3], FALSE);
	if (dsp->ds_first_leadin > 0)
		dsp->ds_first_leadin = 0;

	if (dsp->ds_last_leadout == 0 && dsp->ds_maxblocks >= 0)
		dsp->ds_last_leadout = dsp->ds_maxblocks;
	dsp->ds_trfirst=dip->first_track; 
	dsp->ds_trlast=dip->last_track_ls;
	dsp->ds_trfirst_ls=dip->first_track_ls;
}

static int 
get_atip(SCSI *usalp, struct atipinfo *atp)
{
	int	len;
	int	ret;

	fillbytes((caddr_t)atp, sizeof (*atp), '\0');

	/*
	 * Used to be 2 instead of sizeof (struct tocheader), but all
	 * other places in the code use sizeof (struct tocheader) too and
	 * some Y2k ATAPI drives as used by IOMEGA create a DMA overrun if we
	 * try to transfer only 2 bytes.
	 */
	if (read_toc(usalp, (caddr_t)atp, 0, sizeof (struct tocheader), 0, FMT_ATIP) < 0)
		return (-1);
	len = a_to_u_2_byte(atp->hd.len);
	len += 2;
	ret = read_toc(usalp, (caddr_t)atp, 0, len, 0, FMT_ATIP);

#ifdef	DEBUG
	usal_prbytes("ATIP info:", (Uchar *)atp,
				len-usal_getresid(usalp));
#endif
	/*
	 * Yamaha sometimes returns zeroed ATIP info for disks without ATIP
	 */
	if (atp->desc.lead_in[1] == 0 &&
			atp->desc.lead_in[2] == 0 &&
			atp->desc.lead_in[3] == 0 &&
			atp->desc.lead_out[1] == 0 &&
			atp->desc.lead_out[2] == 0 &&
			atp->desc.lead_out[3] == 0)
		return (-1);

	if (atp->desc.lead_in[1] >= 0x90 && debug) {
		/*
		 * Only makes sense with buggy Ricoh firmware.
		 */
		errmsgno(EX_BAD, "Converting ATIP from BCD\n");
		atp->desc.lead_in[1] = from_bcd(atp->desc.lead_in[1]);
		atp->desc.lead_in[2] = from_bcd(atp->desc.lead_in[2]);
		atp->desc.lead_in[3] = from_bcd(atp->desc.lead_in[3]);

		atp->desc.lead_out[1] = from_bcd(atp->desc.lead_out[1]);
		atp->desc.lead_out[2] = from_bcd(atp->desc.lead_out[2]);
		atp->desc.lead_out[3] = from_bcd(atp->desc.lead_out[3]);
	}

	return (ret);
}

#ifdef	PRINT_ATIP

static int 
get_pma(SCSI *usalp)
{
	int	len;
	int	ret;
	char	atp[1024];

	fillbytes((caddr_t)atp, sizeof (*atp), '\0');

	/*
	 * Used to be 2 instead of sizeof (struct tocheader), but all
	 * other places in the code use sizeof (struct tocheader) too and
	 * some Y2k ATAPI drives as used by IOMEGA create a DMA overrun if we
	 * try to transfer only 2 bytes.
	 */
/*	if (read_toc(usalp, (caddr_t)atp, 0, 2, 1, FMT_PMA) < 0)*/
/*	if (read_toc(usalp, (caddr_t)atp, 0, 2, 0, FMT_PMA) < 0)*/
	if (read_toc(usalp, (caddr_t)atp, 0, sizeof (struct tocheader), 0, FMT_PMA) < 0)
		return (-1);
/*	len = a_to_u_2_byte(atp->hd.len);*/
	len = a_to_u_2_byte(atp);
	len += 2;
/*	ret = read_toc(usalp, (caddr_t)atp, 0, len, 1, FMT_PMA);*/
	ret = read_toc(usalp, (caddr_t)atp, 0, len, 0, FMT_PMA);

#ifdef	DEBUG
	usal_prbytes("PMA:", (Uchar *)atp,
				len-usal_getresid(usalp));
#endif
	ret = read_toc(usalp, (caddr_t)atp, 0, len, 1, FMT_PMA);

#ifdef	DEBUG
	usal_prbytes("PMA:", (Uchar *)atp,
				len-usal_getresid(usalp));
#endif
	return (ret);
}

#endif	/* PRINT_ATIP */

static int 
init_mmc(SCSI *usalp, cdr_t *dp)
{
	return (speed_select_mmc(usalp, dp, NULL));
}

static int 
getdisktype_mdvd(SCSI *usalp, cdr_t *dp)
{
	int ret = 0;
	dstat_t	*dsp = dp->cdr_dstat;

	struct track_info track_info;
	printf("HINT: use dvd+rw-mediainfo from dvd+rw-tools for information extraction.\n");
	/* if(getdisktype_mmc(usalp, dp)<0)
		return -1;
		*/

	/* read rzone info to get the space left on disk */
	/*ds_trlast is the last rzone on disk, can be invisible */
	if(read_rzone_info(usalp, (caddr_t)&track_info, sizeof(track_info))>=0)
		dsp->ds_maxblocks=a_to_u_4_byte(track_info.free_blocks)+a_to_4_byte(track_info.next_writable_addr);

	dsp->ds_disktype&= ~DT_CD;
	dsp->ds_disktype|= DT_DVD;

	return (ret);

}

static int 
getdisktype_mmc(SCSI *usalp, cdr_t *dp)
{
extern	char	*buf;
	dstat_t	*dsp = dp->cdr_dstat;
	struct disk_info *dip;
	Uchar	mode[0x100];
	msf_t	msf;
	BOOL	did_atip = FALSE;
	BOOL	did_dummy = FALSE;
	int 	rplus;

	msf.msf_min = msf.msf_sec = msf.msf_frame = 0;

	/*
	 * It seems that there are drives that do not support to
	 * read ATIP (e.g. HP 7100)
	 * Also if a NON CD-R media is inserted, this will never work.
	 * For this reason, make a failure non-fatal.
	 */
	usalp->silent++;
	if (get_atip(usalp, (struct atipinfo *)mode) >= 0) {
		struct atipinfo *atp = (struct atipinfo *)mode;

		msf.msf_min =		mode[8];
		msf.msf_sec =		mode[9];
		msf.msf_frame =		mode[10];
		if (atp->desc.erasable) {
			dsp->ds_flags |= DSF_ERA;
			if (atp->desc.sub_type == 1)
				dsp->ds_flags |= DSF_HIGHSP_ERA;
			else if (atp->desc.sub_type == 2)
				dsp->ds_flags |= DSF_ULTRASP_ERA;
			else if (atp->desc.sub_type == 3)
				dsp->ds_flags |= DSF_ULTRASP_ERA | DSF_ULTRASPP_ERA;
		}
		if (atp->desc.a1_v) {
			if (atp->desc.clv_low != 0)
				dsp->ds_at_min_speed = clv_to_speed[atp->desc.clv_low];
			if (atp->desc.clv_high != 0)
				dsp->ds_at_max_speed = clv_to_speed[atp->desc.clv_high];

			if (atp->desc.erasable && atp->desc.sub_type == 1) {
				if (atp->desc.clv_high != 0)
					dsp->ds_at_max_speed = hs_clv_to_speed[atp->desc.clv_high];
			}
		}
		if (atp->desc.a2_v && atp->desc.erasable && (atp->desc.sub_type == 2 || atp->desc.sub_type == 3)) {
			Uint	vlow;
			Uint	vhigh;

			vlow = (atp->desc.a2[0] >> 4) & 0x07;
			vhigh = atp->desc.a2[0] & 0x0F;
			if (vlow != 0)
				dsp->ds_at_min_speed = us_clv_to_speed[vlow];
			if (vhigh != 0)
				dsp->ds_at_max_speed = us_clv_to_speed[vhigh];
		}
		did_atip = TRUE;
	}
	usalp->silent--;

#ifdef	PRINT_ATIP
	if ((dp->cdr_dstat->ds_cdrflags & RF_PRATIP) != 0 && did_atip) {
		print_atip(usalp, (struct atipinfo *)mode);
		pr_manufacturer(&msf,
			((struct atipinfo *)mode)->desc.erasable,
			((struct atipinfo *)mode)->desc.uru);
	}
#endif
again:
	dip = (struct disk_info *)buf;
	if (get_diskinfo(usalp, dip) < 0)
		return (-1);

	/*
	 * Check for non writable disk first.
	 */
	
	/* DVD+RW does not need to be blanked */
	rplus = dsp->ds_cdrflags;
	if (dp->profile == 0x1A) rplus = RF_BLANK;
	
	if (dip->disk_status == DS_COMPLETE &&
			(rplus & dsp->ds_cdrflags & (RF_WRITE|RF_BLANK)) == RF_WRITE) {
		if (!did_dummy) {
			int	xspeed = 0xFFFF;
			int	oflags = dp->cdr_cmdflags;

			/*
			 * Try to clear the dummy bit to reset the virtual
			 * drive status. Not all drives support it even though
			 * it is mentioned in the MMC standard.
			 */
			if (lverbose)
				printf("Trying to clear drive status.\n");

			dp->cdr_cmdflags &= ~F_DUMMY;
			speed_select_mmc(usalp, dp, &xspeed);
			dp->cdr_cmdflags = oflags;
			did_dummy = TRUE;
			goto again;
		}
		/*
		 * Trying to clear drive status did not work...
		 */
		reload_media(usalp, dp);
	}
	if (get_diskinfo(usalp, dip) < 0)
		return (-1);
	di_to_dstat(dip, dsp);
	if (!did_atip && dsp->ds_first_leadin < 0)
		lba_to_msf(dsp->ds_first_leadin, &msf);

	if ((dp->cdr_dstat->ds_cdrflags & RF_PRATIP) != 0 && !did_atip) {
		print_min_atip(dsp->ds_first_leadin, dsp->ds_last_leadout);
		if (dsp->ds_first_leadin < 0)
				pr_manufacturer(&msf,
				dip->erasable,
				dip->uru);
	}
	dsp->ds_maxrblocks = disk_rcap(&msf, dsp->ds_maxblocks,
				dip->erasable,
				dip->uru);


#ifdef	PRINT_ATIP
#ifdef	DEBUG
	if (get_atip(usalp, (struct atipinfo *)mode) < 0)
		return (-1);
	/*
	 * Get pma gibt Ärger mit CW-7502
	 * Wenn Die Disk leer ist, dann stuerzt alles ab.
	 * Firmware 4.02 kann nicht get_pma
	 */
	if (dip->disk_status != DS_EMPTY) {
/*		get_pma();*/
	}
	printf("ATIP lead in:  %ld (%02d:%02d/%02d)\n",
		msf_to_lba(mode[8], mode[9], mode[10], FALSE),
		mode[8], mode[9], mode[10]);
	printf("ATIP lead out: %ld (%02d:%02d/%02d)\n",
		msf_to_lba(mode[12], mode[13], mode[14], TRUE),
		mode[12], mode[13], mode[14]);
	print_di(dip);
	print_atip(usalp, (struct atipinfo *)mode);
#endif
#endif	/* PRINT_ATIP */
	return (drive_getdisktype(usalp, dp));
}

#ifdef	PRINT_ATIP

#define	DOES(what, flag)	printf("  Does %s%s\n", flag?"":"not ", what);
#define	IS(what, flag)		printf("  Is %s%s\n", flag?"":"not ", what);
#define	VAL(what, val)		printf("  %s: %d\n", what, val[0]*256 + val[1]);
#define	SVAL(what, val)		printf("  %s: %s\n", what, val);

static void 
print_di(struct disk_info *dip)
{
	static	char *ds_name[] = { "empty", "incomplete/appendable", "complete", "illegal" };
	static	char *ss_name[] = { "empty", "incomplete/appendable", "illegal", "complete", };

	IS("erasable", dip->erasable);
	printf("disk status: %s\n", ds_name[dip->disk_status]);
	printf("session status: %s\n", ss_name[dip->sess_status]);
	printf("first track: %d number of sessions: %d first track in last sess: %d last track in last sess: %d\n",
		dip->first_track,
		dip->numsess,
		dip->first_track_ls,
		dip->last_track_ls);
	IS("unrestricted", dip->uru);
	printf("Disk type: ");
	switch (dip->disk_type) {

	case SES_DA_ROM:	printf("CD-DA or CD-ROM");	break;
	case SES_CDI:		printf("CDI");			break;
	case SES_XA:		printf("CD-ROM XA");		break;
	case SES_UNDEF:		printf("undefined");		break;
	default:		printf("reserved");		break;
	}
	printf("\n");
	if (dip->did_v)
		printf("Disk id: 0x%lX\n", a_to_u_4_byte(dip->disk_id));

	printf("last start of lead in: %ld\n",
		msf_to_lba(dip->last_lead_in[1],
		dip->last_lead_in[2],
		dip->last_lead_in[3], FALSE));
	printf("last start of lead out: %ld\n",
		msf_to_lba(dip->last_lead_out[1],
		dip->last_lead_out[2],
		dip->last_lead_out[3], TRUE));

	if (dip->dbc_v)
		printf("Disk bar code: 0x%lX%lX\n",
			a_to_u_4_byte(dip->disk_barcode),
			a_to_u_4_byte(&dip->disk_barcode[4]));

	if (dip->num_opc_entries > 0) {
		printf("OPC table:\n");
	}
}

char	*cdr_subtypes[] = {
	"Normal Rewritable (CLV) media",
	"High speed Rewritable (CAV) media",
	"Medium Type A, low Beta category (A-)",
	"Medium Type A, high Beta category (A+)",
	"Medium Type B, low Beta category (B-)",
	"Medium Type B, high Beta category (B+)",
	"Medium Type C, low Beta category (C-)",
	"Medium Type C, high Beta category (C+)",
};

char	*cdrw_subtypes[] = {
	"Normal Rewritable (CLV) media",
	"High speed Rewritable (CAV) media",
	"Ultra High speed Rewritable media",
	"Ultra High speed+ Rewritable media",
	"Medium Type B, low Beta category (B-)",
	"Medium Type B, high Beta category (B+)",
	"Medium Type C, low Beta category (C-)",
	"Medium Type C, high Beta category (C+)",
};

static void 
atip_printspeed(char *fmt, int speedindex, char speedtab[])
{
	printf("%s:", fmt);
	if (speedtab[speedindex] == 0) {
		printf(" %2d (reserved val %2d)",
			speedtab[speedindex], speedindex);
	} else {
		printf(" %2d", speedtab[speedindex]);
	}
}

static void 
print_atip(SCSI *usalp, struct atipinfo *atp)
{
	char	*sub_type;
	char	*speedvtab = clv_to_speed;

	if (usalp->verbose)
		usal_prbytes("ATIP info: ", (Uchar *)atp, sizeof (*atp));

	printf("ATIP info from disk:\n");
	printf("  Indicated writing power: %d\n", atp->desc.ind_wr_power);
	if (atp->desc.erasable || atp->desc.ref_speed)
		printf("  Reference speed: %d\n", clv_to_speed[atp->desc.ref_speed]);
	IS("unrestricted", atp->desc.uru);
/*	printf("  Disk application code: %d\n", atp->desc.res5_05);*/
	IS("erasable", atp->desc.erasable);
	if (atp->desc.erasable)
		sub_type = cdrw_subtypes[atp->desc.sub_type];
	else
		sub_type = cdr_subtypes[atp->desc.sub_type];
	if (atp->desc.sub_type)
		printf("  Disk sub type: %s (%d)\n", sub_type, atp->desc.sub_type);
	printf("  ATIP start of lead in:  %ld (%02d:%02d/%02d)\n",
		msf_to_lba(atp->desc.lead_in[1],
		atp->desc.lead_in[2],
		atp->desc.lead_in[3], FALSE),
		atp->desc.lead_in[1],
		atp->desc.lead_in[2],
		atp->desc.lead_in[3]);
	printf("  ATIP start of lead out: %ld (%02d:%02d/%02d)\n",
		msf_to_lba(atp->desc.lead_out[1],
		atp->desc.lead_out[2],
		atp->desc.lead_out[3], TRUE),
		atp->desc.lead_out[1],
		atp->desc.lead_out[2],
		atp->desc.lead_out[3]);
	if (atp->desc.a1_v) {
		if (atp->desc.erasable && atp->desc.sub_type == 1) {
			speedvtab = hs_clv_to_speed;
		}
		if (atp->desc.a2_v && (atp->desc.sub_type == 2 || atp->desc.sub_type == 3)) {
			speedvtab = us_clv_to_speed;
		}
		if (atp->desc.clv_low != 0 || atp->desc.clv_high != 0) {
			atip_printspeed("  1T speed low",
				atp->desc.clv_low, speedvtab);
			atip_printspeed(" 1T speed high",
				atp->desc.clv_high, speedvtab);
			printf("\n");
		}
	}
	if (atp->desc.a2_v) {
		Uint	vlow;
		Uint	vhigh;

		vlow = (atp->desc.a2[0] >> 4) & 0x07;
		vhigh = atp->desc.a2[0] & 0x0F;

		if (vlow != 0 || vhigh != 0) {
			atip_printspeed("  2T speed low",
					vlow, speedvtab);
			atip_printspeed(" 2T speed high",
					vhigh, speedvtab);
			printf("\n");
		}
	}
	if (atp->desc.a1_v) {
		printf("  power mult factor: %d %d\n", atp->desc.power_mult, atp->desc.tgt_y_pow);
		if (atp->desc.erasable)
			printf("  recommended erase/write power: %d\n", atp->desc.rerase_pwr_ratio);
	}
	if (atp->desc.a1_v) {
		printf("  A1 values: %02X %02X %02X\n",
				(&atp->desc.res15)[1],
				(&atp->desc.res15)[2],
				(&atp->desc.res15)[3]);
	}
	if (atp->desc.a2_v) {
		printf("  A2 values: %02X %02X %02X\n",
				atp->desc.a2[0],
				atp->desc.a2[1],
				atp->desc.a2[2]);
	}
	if (atp->desc.a3_v) {
		printf("  A3 values: %02X %02X %02X\n",
				atp->desc.a3[0],
				atp->desc.a3[1],
				atp->desc.a3[2]);
	}
}
#endif	/* PRINT_ATIP */

static int 
speed_select_mmc(SCSI *usalp, cdr_t *dp, int *speedp)
{
	Uchar	mode[0x100];
	Uchar	moder[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;
	struct	ricoh_mode_page_30 *rp = NULL;
	int	val;
	BOOL	forcespeed = FALSE;
	BOOL	dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (speedp)
		curspeed = *speedp;

	/*
	 * Do not reset mp->test_write (-dummy) here.
	 */
	deflt_writemodes_mmc(usalp, FALSE);

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
		return (-1);
	if (len == 0)
		return (-1);

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);
#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif

    if(dummy) {
        mp->test_write = 1;
        /* but it does not work on DVD+RW and -RAM, also bail out on other
         * types that have not been tested yet */
        int profile=get_curprofile(usalp);
        switch(profile) {
            case(0x12):
            case(0x1a):
            case(0x2a):
            case(0x43):
            case(0x52):
                {
                    fprintf(stderr, 
                            "Dummy mode not possible with %s.\n",
                            mmc_obtain_profile_name(profile) );
                    exit(EXIT_FAILURE);
                }
        }
    }
    else
        mp->test_write = 0;

#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1))
		return (-1);

	/*
	 * Neither set nor get speed.
	 */
	if (speedp == 0)
		return (0);


	rp = get_justlink_ricoh(usalp, moder);
	if (mmc_isyamaha(usalp)) {
		forcespeed = FALSE;
	} else if (mmc_isplextor(usalp) && (dp->cdr_flags & CDR_FORCESPEED) != 0) {
		int	pwr;

		pwr = check_powerrec_plextor(usalp);
		if (pwr >= 0)
			forcespeed = (pwr == 0);
	} else if ((dp->cdr_flags & CDR_FORCESPEED) != 0) {
		forcespeed = rp && rp->AWSCD != 0;
	}

	if (lverbose && (dp->cdr_flags & CDR_FORCESPEED) != 0)
		printf("Forcespeed is %s.\n", forcespeed?"ON":"OFF");

	if (!forcespeed && (dp->cdr_dstat->ds_cdrflags & RF_FORCESPEED) != 0) {
		printf("Turning forcespeed on\n");
		forcespeed = TRUE;
	}
	if (forcespeed && (dp->cdr_dstat->ds_cdrflags & RF_FORCESPEED) == 0) {
		printf("Turning forcespeed off\n");
		forcespeed = FALSE;
	}
	if (mmc_isplextor(usalp) && (dp->cdr_flags & CDR_FORCESPEED) != 0) {
		powerrec_plextor(usalp, !forcespeed);
	}
	if (!mmc_isyamaha(usalp) && (dp->cdr_flags & CDR_FORCESPEED) != 0) {

		if (rp) {
			rp->AWSCD = forcespeed?1:0;
			set_mode_params(usalp, "Ricoh Vendor Page", moder, moder[0]+1, 0, -1);
			rp = get_justlink_ricoh(usalp, moder);
		}
	}

	/*
	 * 44100 * 2 * 2 =  176400 bytes/s
	 *
	 * The right formula would be:
	 * tmp = (((long)curspeed) * 1764) / 10;
	 *
	 * But the standard is rounding the wrong way.
	 * Furtunately rounding down is guaranteed.
	 */
	val = curspeed*177;
	if (val > 0xFFFF)
		val = 0xFFFF;
	if (mmc_isyamaha(usalp) && forcespeed) {
		if (force_speed_yamaha(usalp, -1, val) < 0)
			return (-1);
	} else if (mmc_set_speed(usalp, -1, val, ROTCTL_CLV) < 0) {
		return (-1);
	}

	if (scsi_get_speed(usalp, 0, &val) >= 0) {
		if (val > 0) {
		        fprintf(stderr, "Speed set to %d KB/s\n", val); 
			curspeed = val / 176;
			*speedp = curspeed;
		}
	}
	return (0);
}

/*
 * Some drives do not round up when writespeed is e.g. 1 and
 * the minimum write speed of the drive is higher. Try to increment
 * the write speed unti it gets accepted by the drive.
 */
static int 
mmc_set_speed(SCSI *usalp, int readspeed, int writespeed, int rotctl)
{
	int	rs;
	int	ws;
	int	ret = -1;
	int	c;
	int	k;

	if (scsi_get_speed(usalp, &rs, &ws) >= 0) {
		if (readspeed < 0)
			readspeed = rs;
		if (writespeed < 0)
			writespeed = ws;
	}
	if (writespeed < 0 || writespeed > 0xFFFF)
		return (ret);

	usalp->silent++;
	while (writespeed <= 0xFFFF) {
		ret = scsi_set_speed(usalp, readspeed, writespeed, rotctl);
		if (ret >= 0)
			break;
		c = usal_sense_code(usalp);
		k = usal_sense_key(usalp);
		/*
		 * Abort quickly if it does not make sense to repeat.
		 * 0x24 == Invalid field in cdb
		 * 0x24 means illegal speed.
		 */
		if ((k != SC_ILLEGAL_REQUEST) || (c != 0x24)) {
			if (usalp->silent <= 1)
				usal_printerr(usalp);
			usalp->silent--;
			return (-1);
		}
		writespeed += 177;
	}
	if (ret < 0 && usalp->silent <= 1)
		usal_printerr(usalp);
	usalp->silent--;

	return (ret);
}

static int 
speed_select_mdvd(SCSI *usalp, cdr_t *dp, int *speedp)
{
  int retcode;
  char perf_desc[28];
  int write_speed = *speedp * 1385;
   
  /* For the moment we just divide the CD speed by 7*/

  if(speedp!=NULL)
     (*speedp)=(*speedp)*8;
  
  memset(perf_desc, 0, sizeof(perf_desc));

  /* Write Rotation Control = ROTCTL_CLV 
   * | Restore Logical Unit Defaults = 0 
   * | Exact = 0 
   * | Random Access = 0) 
   */
  perf_desc[0]= ROTCTL_CLV << 3 | 0 << 2 | 0 << 1 | 0; 
  /* Start LBA to 0 */
  perf_desc[4] = 0;
  perf_desc[5] = 0;
  perf_desc[6] = 0;
  perf_desc[7] = 0;
  /* End LBA set to 0 (setting to 0xffffffff failed on my LG burner
   */
  perf_desc[8] = 0;
  perf_desc[9] = 0;
  perf_desc[10] = 0;
  perf_desc[11] = 0;
  /* Read Speed = 0xFFFF */
  perf_desc[12] = 0;
  perf_desc[13] = 0;
  perf_desc[14] = 0xFF;
  perf_desc[15] = 0xFF;
  /* Read Time = 1s */
  perf_desc[18] = 1000 >> 8;
  perf_desc[19] = 1000 & 0xFF;   
  /* Write Speed */
  perf_desc[20] = write_speed >> 24;
  perf_desc[21] = write_speed >> 16 & 0xFF;
  perf_desc[22] = write_speed >> 8 & 0xFF;
  perf_desc[23] = write_speed & 0xFF;
  /* Write Time = 1s */
  perf_desc[26] = 1000 >> 8;
  perf_desc[27] = 1000 & 0xFF;  
  
  /* retcode = scsi_set_streaming(usalp, NULL, 0); */
  retcode = scsi_set_streaming(usalp, perf_desc, sizeof(perf_desc));
  if (retcode == -1) return retcode;
  retcode = speed_select_mmc(usalp, dp, speedp);
  if(speedp!=NULL)
     (*speedp)=(*speedp)/7;
   return retcode;
}

static int 
next_wr_addr_mmc(SCSI *usalp, track_t *trackp, long *ap)
{
	struct	track_info	track_info;
	long	next_addr;
	int	result = -1;


	/*
	 * Reading info for current track may require doing the read_track_info
	 * with either the track number (if the track is currently being written)
	 * or with 0xFF (if the track hasn't been started yet and is invisible
	 */

	if (trackp != 0 && trackp->track > 0 && is_packet(trackp)) {
		usalp->silent++;
		result = read_track_info(usalp, (caddr_t)&track_info, TI_TYPE_TRACK,
							trackp->trackno,
							sizeof (track_info));
		usalp->silent--;
	}

	if (result < 0) {
		if (read_track_info(usalp, (caddr_t)&track_info, TI_TYPE_TRACK, 0xFF,
						sizeof (track_info)) < 0) {
			errmsgno(EX_BAD, "Cannot get next writable address for 'invisible' track.\n");
			errmsgno(EX_BAD, "This means that we are checking recorded media.\n");
			errmsgno(EX_BAD, "This media cannot be written in streaming mode anymore.\n");
			errmsgno(EX_BAD, "If you like to write to 'preformatted' RW media, try to blank the media first.\n");
			return (-1);
		}
	}
	if (usalp->verbose)
		usal_prbytes("track info:", (Uchar *)&track_info,
				sizeof (track_info)-usal_getresid(usalp));
	next_addr = a_to_4_byte(track_info.next_writable_addr);
	if (ap)
		*ap = next_addr;
	return (0);
}

static int 
write_leadin_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uint	i;
	long	startsec = 0L;

/*	if (flags & F_SAO) {*/
	if (wm_base(dp->cdr_dstat->ds_wrmode) == WM_SAO) {
		if (debug || lverbose) {
			printf("Sending CUE sheet...\n");
			flush();
		}
		if ((*dp->cdr_send_cue)(usalp, dp, trackp) < 0) {
			errmsgno(EX_BAD, "Cannot send CUE sheet.\n");
			return (-1);
		}

		(*dp->cdr_next_wr_address)(usalp, &trackp[0], &startsec);
		if (trackp[0].flags & TI_TEXT) {
			startsec = dp->cdr_dstat->ds_first_leadin;
			printf("SAO startsec: %ld\n", startsec);
		} else if (startsec <= 0 && startsec != -150) {
			if(lverbose>2)
				fprintf(stderr, "WARNING: Drive returns wrong startsec (%ld) using -150\n",
					startsec);
			startsec = -150;
		}
		if (debug)
			printf("SAO startsec: %ld\n", startsec);

		if (trackp[0].flags & TI_TEXT) {
			if (startsec > 0) {
				errmsgno(EX_BAD, "CD-Text must be in first session.\n");
				return (-1);
			}
			if (debug || lverbose)
				printf("Writing lead-in...\n");
			if (write_cdtext(usalp, dp, startsec) < 0)
				return (-1);

			dp->cdr_dstat->ds_cdrflags |= RF_LEADIN;
		} else for (i = 1; i <= trackp->tracks; i++) {
			trackp[i].trackstart += startsec +150;
		}
#ifdef	XXX
		if (debug || lverbose)
			printf("Writing lead-in...\n");

		pad_track(usalp, dp, &trackp[1], -150, (Llong)0,
					FALSE, 0);
#endif
	}
/*	if (flags & F_RAW) {*/
    if (wm_base(dp->cdr_dstat->ds_wrmode) == WM_RAW) {
        /*
		 * In RAW write mode, we now write the lead in (TOC).
		 */
		(*dp->cdr_next_wr_address)(usalp, &trackp[0], &startsec);
		if (startsec > -4500) {
			/*
			 * There must be at least 1 minute lead-in.
			 */
			errmsgno(EX_BAD, "WARNING: Drive returns wrong startsec (%ld) using %ld from ATIP\n",
					startsec, (long)dp->cdr_dstat->ds_first_leadin);
			startsec = dp->cdr_dstat->ds_first_leadin;
		}
		if (startsec > -4500) {
			errmsgno(EX_BAD, "Illegal startsec (%ld)\n", startsec);
			return (-1);
		}
		if (debug || lverbose)
			printf("Writing lead-in at sector %ld\n", startsec);
		if (write_leadin(usalp, dp, trackp, startsec) < 0)
			return (-1);
		dp->cdr_dstat->ds_cdrflags |= RF_LEADIN;
	}
	return (0);
}

int	st2mode[] = {
	0,		/* 0			*/
	TM_DATA,	/* 1 ST_ROM_MODE1	*/
	TM_DATA,	/* 2 ST_ROM_MODE2	*/
	0,		/* 3			*/
	0,		/* 4 ST_AUDIO_NOPRE	*/
	TM_PREEM,	/* 5 ST_AUDIO_PRE	*/
	0,		/* 6			*/
	0,		/* 7			*/
};

static int 
next_wr_addr_mdvd(SCSI *usalp, track_t *trackp, long *ap)
{
	int     track=0;
	struct	track_info	track_info;
	long	next_addr;
	int	result = -1;
	struct  disk_info disk_info;
	if (trackp){
	    track = trackp->trackno;
	}

	if (trackp != 0 && track > 0 && is_packet(trackp)) {
		usalp->silent++;
		result = read_track_info(usalp, (caddr_t)&track_info, TI_TYPE_SESS, track, sizeof(track_info));
		usalp->silent--;
		if (scsi_in_progress(usalp)){
		  return -1;
		}
		
	}

	if (result < 0) {
	  /* Get the last rzone*/
	        if(read_disk_info(usalp,(caddr_t)&disk_info,8)<0)
		  return (-1);
	     
		/* if (read_track_info(usalp, (caddr_t)&track_info, TI_TYPE_SESS, 0xFF, sizeof(track_info)) < 0) */
		    if (read_rzone_info(usalp, (caddr_t)&track_info, sizeof(track_info)) < 0)
			return (-1);
	}
	if (usalp->verbose)
		usal_prbytes("track info:", (Uchar *)&track_info,
				sizeof(track_info)-usal_getresid(usalp));
	next_addr = a_to_4_byte(track_info.next_writable_addr);
	if (ap)
		*ap = next_addr;
	return (0);
}

static int 
open_track_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	if (!is_tao(trackp) && !is_packet(trackp)) {
		if (trackp->pregapsize > 0 && (trackp->flags & TI_PREGAP) == 0) {
			if (lverbose) {
				printf("Writing pregap for track %d at %ld\n",
					(int)trackp->trackno,
					trackp->trackstart-trackp->pregapsize);
			}
			/*
			 * XXX Do we need to check isecsize too?
			 */
			pad_track(usalp, dp, trackp,
				trackp->trackstart-trackp->pregapsize,
				(Llong)trackp->pregapsize*trackp->secsize,
					FALSE, 0);
		}
		return (0);
	}

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
		return (-1);
	if (len == 0)
		return (-1);

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);


/*	mp->track_mode = ???;*/
	mp->track_mode = st2mode[trackp->sectype & ST_MASK];
/*	mp->copy = ???;*/
	mp->dbtype = trackp->dbtype;

/*i_to_short(mp->audio_pause_len, 300);*/
/*i_to_short(mp->audio_pause_len, 150);*/
/*i_to_short(mp->audio_pause_len, 0);*/

	if (is_packet(trackp)) {
		mp->write_type = WT_PACKET;
		mp->track_mode |= TM_INCREMENTAL;
		mp->fp = (trackp->pktsize > 0) ? 1 : 0;
		i_to_4_byte(mp->packet_size, trackp->pktsize);
	} else if (is_tao(trackp)) {
		mp->write_type = WT_TAO;
		mp->fp = 0;
		i_to_4_byte(mp->packet_size, 0);
	} else {
		errmsgno(EX_BAD, "Unknown write mode.\n");
		return (-1);
	}
	if (trackp->isrc) {
		mp->ISRC[0] = 0x80;	/* Set ISRC valid */
		strncpy((char *)&mp->ISRC[1], trackp->isrc, 12);

	} else {
		fillbytes(&mp->ISRC[0], sizeof (mp->ISRC), '\0');
	}

#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, trackp->secsize))
		return (-1);

	return (0);
}

static int 
open_track_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	if (is_packet(trackp)) {
	       fillbytes((caddr_t)mode, sizeof(mode), '\0');
	  
	       if (!get_mode_params(usalp, 0x05, "DVD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
	              return (-1);
	       if (len == 0)
		      return (-1);

	        mp = (struct cd_mode_page_05 *)
	              (mode + sizeof(struct scsi_mode_header) +
		      ((struct scsi_mode_header *)mode)->blockdesc_len);

		mp->write_type = WT_PACKET;
		mp->LS_V = 1;
		/*For now we set the link size to 0x10(32k) because Pioneer-A03 only support this */
		mp->link_size=0x10;
		mp->fp = 1;
		i_to_4_byte(mp->packet_size, trackp->pktsize);
	} else {
	     return 0;
	}
 
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, trackp->secsize))
		return (-1);

	return (0);
}

static int 
close_track_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	ret;

	if (!is_tao(trackp) && !is_packet(trackp))
		return (0);

	if (scsi_flush_cache(usalp, (dp->cdr_cmdflags&F_IMMED) != 0) < 0) {
		printf("Trouble flushing the cache\n");
		return (-1);
	}
	wait_unit_ready(usalp, 300);		/* XXX Wait for ATAPI */
	if (is_packet(trackp) && !is_noclose(trackp)) {
			/* close the incomplete track */
		ret = scsi_close_tr_session(usalp, CL_TYPE_TRACK, 0xFF,
				(dp->cdr_cmdflags&F_IMMED) != 0);
		wait_unit_ready(usalp, 300);	/* XXX Wait for ATAPI */
		return (ret);
	}
	return (0);
}

static int 
close_track_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	ret;
	if (!is_packet(trackp))
	     return (0);

	if (scsi_flush_cache(usalp, (dp->cdr_cmdflags&F_IMMED) != 0) < 0) {
		printf("Trouble flushing the cache\n");
		return -1;
	}
	wait_unit_ready(usalp, 300);		/* XXX Wait for ATAPI */
	if (is_packet(trackp) && !is_noclose(trackp)) {
			/* close the incomplete track */
		ret = scsi_close_tr_session(usalp, 1, 0xFF, (dp->cdr_cmdflags&F_IMMED) != 0);
		wait_unit_ready(usalp, 300);	/* XXX Wait for ATAPI */
		return (ret);
	}
	return (0);
}

int	toc2sess[] = {
	SES_DA_ROM,	/* CD-DA		 */
	SES_DA_ROM,	/* CD-ROM		 */
	SES_XA,		/* CD-ROM XA mode 1	 */
	SES_XA,		/* CD-ROM XA MODE 2	 */
	SES_CDI,	/* CDI			 */
	SES_DA_ROM,	/* Invalid - use default */
	SES_DA_ROM,	/* Invalid - use default */
};

static int 
open_session_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uchar	mode[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
		return (-1);
	if (len == 0)
		return (-1);

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	mp->write_type = WT_TAO; /* fix to allow DAO later */
	/*
	 * We need to set the right dbtype here because Sony drives
	 * don't like multi session in to be set with DB_ROM_MODE1
	 * which is set by us at the beginning as default as some drives
	 * have illegal default values.
	 */
	mp->track_mode = st2mode[trackp[0].sectype & ST_MASK];
	mp->dbtype = trackp[0].dbtype;

	if (!is_tao(trackp) && !is_packet(trackp)) {
		mp->write_type = WT_SAO;
		if (dp->cdr_dstat->ds_cdrflags & RF_AUDIOMASTER)
			mp->write_type = 8;
		mp->track_mode = 0;
		mp->dbtype = DB_RAW;
	}
	if (is_raw(trackp)) {
		mp->write_type = WT_RAW;
		mp->track_mode = 0;

		if (is_raw16(trackp)) {
			mp->dbtype = DB_RAW_PQ;
		} else if (is_raw96r(trackp)) {
			mp->dbtype = DB_RAW_PW_R;
		} else {
			mp->dbtype = DB_RAW_PW;
		}
	}

	mp->multi_session = (track_base(trackp)->tracktype & TOCF_MULTI) ?
				MS_MULTI : MS_NONE;
	mp->session_format = toc2sess[track_base(trackp)->tracktype & TOC_MASK];

	if (trackp->isrc) {
		mp->media_cat_number[0] = 0x80;	/* Set MCN valid */
		strncpy((char *)&mp->media_cat_number[1], trackp->isrc, 13);

	} else {
		fillbytes(&mp->media_cat_number[0], sizeof (mp->media_cat_number), '\0');
	}
#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1))
		return (-1);

	return (0);
}

static int 
open_session_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	Uchar	mode[0x100];
	int	tracks = trackp->tracks;

	int	len;
	struct	cd_mode_page_05 *mp;
	Ulong totalsize;
	int i;
	int profile;

	fillbytes((caddr_t)mode, sizeof(mode), '\0');

	if (!get_mode_params(usalp, 0x05, "DVD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
		return (-1);
	if (len == 0)
		return (-1);

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof(struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);
	if(is_packet(trackp)){
	  mp->write_type=WT_PACKET;
	  mp->fp=0;
	  mp->BUFE=1;
	  mp->track_mode=1;
	}else{
	  mp->write_type = WT_SAO; 
	}

	mp->multi_session = (track_base(trackp)->tracktype & TOCF_MULTI) ?
				MS_MULTI : MS_NONE;
	mp->session_format = toc2sess[track_base(trackp)->tracktype & TOC_MASK];

	/* Enable Burnfree by default, allow to disable. XXX Sucks, duplicated functionality. */
	if (dp->cdr_cdcap->BUF != 0) {
		if (lverbose > 2)
			fprintf(stderr, 
					"BURN-Free is %s.\n"
					"Turning BURN-Free on\n",
					mp->BUFE?"ON":"OFF");
		mp->BUFE = 1;
	}
	if (driveropts != NULL) {
		if ((strcmp(driveropts, "noburnproof") == 0 ||
					strcmp(driveropts, "noburnfree") == 0)) {
			if(lverbose>1)
				fprintf(stderr, "Turning BURN-Free off\n");
			mp->BUFE = 0;
		}
		else if ((strcmp(driveropts, "burnproof") == 0 ||
					strcmp(driveropts, "burnfree") == 0)) {
			/* a NOP, we enable burnfree by default */
			if(lverbose>2)
				fprintf(stderr, "Found burnproof/burnfree in driveropts, those options are enabled by default now.");
		}
		else if (strcmp(driveropts, "help") == 0) {
			mmc_opthelp(dp, 0);
		} 
		else {
			errmsgno(EX_BAD, "Bad driver opts '%s'.\n", driveropts);
			mmc_opthelp(dp, EX_BAD);
		}
	}


	if (!set_mode_params(usalp, "DVD write parameter", mode, len, 0, -1))
		return (-1);

		
	totalsize=0;
	for(i=1;i<=tracks;i++) {
	  totalsize+=trackp[i].tracksecs;
	}
       
	profile = get_curprofile(usalp);
	if(!is_packet(trackp) && profile != 0x1A){
	  /* in DAO mode we need to reserve space for the track*/
	  if(reserve_track(usalp, totalsize)<0)
	    return (-1);
	  }
	return (0);
}

static int 
waitfix_mmc(SCSI *usalp, int secs)
{
	char	dibuf[16];
	int	i;
	int	key;
#define	W_SLEEP	2

	usalp->silent++;
	for (i = 0; i < secs/W_SLEEP; i++) {
		if (read_disk_info(usalp, dibuf, sizeof (dibuf)) >= 0) {
			usalp->silent--;
			return (0);
		}
		key = usal_sense_key(usalp);
		if (key != SC_UNIT_ATTENTION && key != SC_NOT_READY)
			break;
		sleep(W_SLEEP);
	}
	usalp->silent--;
	return (-1);
#undef	W_SLEEP
}

static int 
fixate_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int	ret = 0;
	int	key = 0;
	int	code = 0;
	struct timeval starttime;
	struct timeval stoptime;
	int	dummy = (track_base(trackp)->tracktype & TOCF_DUMMY) != 0;

	if(debug)
		 printf("fixate_mmc\n");
	starttime.tv_sec = 0;
	starttime.tv_usec = 0;
	stoptime = starttime;
	gettimeofday(&starttime, (struct timezone *)0);

	if (dummy && lverbose)
		printf("WARNING: Some drives don't like fixation in dummy mode.\n");

	usalp->silent++;
	if(debug)
		 printf("is_tao: %d,is_packet: %d\n", is_tao(trackp), is_packet(trackp));
	if (is_tao(trackp) || is_packet(trackp)) {
		ret = scsi_close_tr_session(usalp, CL_TYPE_SESSION, 0,
				(dp->cdr_cmdflags&F_IMMED) != 0);
	} else {
		if (scsi_flush_cache(usalp, (dp->cdr_cmdflags&F_IMMED) != 0) < 0) {
			if (!scsi_in_progress(usalp))
				printf("Trouble flushing the cache\n");
		}
	}
	usalp->silent--;
	key = usal_sense_key(usalp);
	code = usal_sense_code(usalp);

	usalp->silent++;
	if (debug && !unit_ready(usalp)) {
		fprintf(stderr, "Early return from fixating. Ret: %d Key: %d, Code: %d\n", ret, key, code);
	}
	usalp->silent--;

	if (ret >= 0) {
		wait_unit_ready(usalp, 420/curspeed);	/* XXX Wait for ATAPI */
		waitfix_mmc(usalp, 420/curspeed);	/* XXX Wait for ATAPI */
		return (ret);
	}

	if ((dummy != 0 && (key != SC_ILLEGAL_REQUEST)) ||
		/*
		 * Try to suppress messages from drives that don't like fixation
		 * in -dummy mode.
		 */
		((dummy == 0) &&
		(((key != SC_UNIT_ATTENTION) && (key != SC_NOT_READY)) ||
				((code != 0x2E) && (code != 0x04))))) {
		/*
		 * UNIT ATTENTION/2E seems to be a magic for old Mitsumi ATAPI drives
		 * NOT READY/ code 4 qual 7 (logical unit not ready, operation in progress)
		 * seems to be a magic for newer Mitsumi ATAPI drives
		 * NOT READY/ code 4 qual 8 (logical unit not ready, long write in progress)
		 * seems to be a magic for SONY drives
		 * when returning early from fixating.
		 * Try to supress the error message in this case to make
		 * simple minded users less confused.
		 */
		usal_printerr(usalp);
		usal_printresult(usalp);	/* XXX restore key/code in future */
	}

	if (debug && !unit_ready(usalp)) {
		fprintf(stderr, "Early return from fixating. Ret: %d Key: %d, Code: %d\n", ret, key, code);
	}

	wait_unit_ready(usalp, 420);	 /* XXX Wait for ATAPI */
	waitfix_mmc(usalp, 420/curspeed); /* XXX Wait for ATAPI */

	if (!dummy &&
		(ret >= 0 || (key == SC_UNIT_ATTENTION && code == 0x2E))) {
		/*
		 * Some ATAPI drives (e.g. Mitsumi) imply the
		 * IMMED bit in the SCSI cdb. As there seems to be no
		 * way to properly check for the real end of the
		 * fixating process we wait for the expected time.
		 */
		gettimeofday(&stoptime, (struct timezone *)0);
		timevaldiff(&starttime, &stoptime);
		if (stoptime.tv_sec < (220 / curspeed)) {
			unsigned secs;

			if (lverbose) {
				printf("Actual fixating time: %ld seconds\n",
							(long)stoptime.tv_sec);
			}
			secs = (280 / curspeed) - stoptime.tv_sec;
			if (lverbose) {
				printf("ATAPI early return: sleeping %d seconds.\n",
								secs);
			}
			sleep(secs);
		}
	}
	return (ret);
}

static int 
fixate_mdvd(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	int ret;
	if (scsi_flush_cache(usalp, (dp->cdr_cmdflags&F_IMMED) != 0) < 0) {
		printf("Trouble flushing the cache\n");
		return -1;
	}
	wait_unit_ready(usalp, 300);		/* XXX Wait for ATAPI */
      /*set a really BIG timeout and call fixate_mmc
	 The BIG timeout is needed in case there was a very short rzone to write at the 
	 beginning of the disk, because lead-out needs to be at some distance.
      */
	if(debug)
		printf("fixate_mdvd\n");
      usal_settimeout(usalp, 1000);
      if(is_packet(trackp) || dp->profile == 0x1B){
	  scsi_close_tr_session(usalp, CL_TYPE_SESSION, 0, FALSE);
      }
      ret = fixate_mmc(usalp, dp, trackp);
      if (dp->profile == 0x2B) {
	  scsi_close_tr_session(usalp, CL_TYPE_OPEN_SESSION, 0, FALSE);
	  scsi_close_tr_session(usalp, CL_TYPE_FINALISE_MINRAD, 0, FALSE);
      }
      usal_settimeout(usalp, 200);

      return ret;
}

char	*blank_types[] = {
	"entire disk",
	"PMA, TOC, pregap",
	"incomplete track",
	"reserved track",
	"tail of track",
	"closing of last session",
	"last session",
	"reserved blanking type",
};

char	*format_types[] = {
	"full format",
	"background format",
	"forced format",
};

static int 
blank_mmc(SCSI *usalp, cdr_t *dp, long addr, int blanktype)
{
	BOOL	cdrr	 = FALSE;	/* Read CD-R	*/
	BOOL	cdwr	 = FALSE;	/* Write CD-R	*/
	BOOL	cdrrw	 = FALSE;	/* Read CD-RW	*/
	BOOL	cdwrw	 = FALSE;	/* Write CD-RW	*/
	int	ret;

	mmc_check(usalp, &cdrr, &cdwr, &cdrrw, &cdwrw, NULL, NULL);
	if (!cdwrw)
		return (blank_dummy(usalp, dp, addr, blanktype));

	if (dp->profile == 0x1A) {
		printf("Error: this media does not support blanking, ignoring.\n");
		return (blank_dummy(usalp, dp, addr, blanktype));
	}
	if (lverbose) {
		printf("Blanking %s\n", blank_types[blanktype & 0x07]);
		flush();
	}

	ret = scsi_blank(usalp, addr, blanktype, (dp->cdr_cmdflags&F_IMMED) != 0);
	if (ret < 0)
		return (ret);

	wait_unit_ready(usalp, 90*60/curspeed);	/* XXX Wait for ATAPI */
	waitfix_mmc(usalp, 90*60/curspeed);	/* XXX Wait for ATAPI */
	return (ret);
}

static int format_mdvd(SCSI *usalp, cdr_t *dp, int formattype)
{
extern	char	*buf;
	BOOL	dvdwr	 = FALSE;	/* Write DVD	*/
	int	ret;
	int 	profile;
	char	addr[12];
	struct disk_info *dip;

	if (debug || lverbose > 2)
		printf("format_mdvd\n");
	mmc_check(usalp, NULL, NULL, NULL, NULL, NULL, &dvdwr);
	if (!dvdwr)
		return (format_dummy(usalp, dp, formattype));

	if (debug || lverbose > 2)
		printf("format_mdvd: drive is a dvd burner.\n");
	profile = get_curprofile(usalp);
	if (profile != 0x1A) {
		printf("Error: only support DVD+RW formating, ignoring.\n");
	        return (format_dummy(usalp, dp, formattype));
	}
	dip = (struct disk_info *)buf;
	if (get_diskinfo(usalp, dip) < 0)
		return -1;
	
	if (dip->disk_status & 3 && formattype != FORCE_FORMAT) {
		printf("Error: disk already formated, ignoring.\n");
	        return -1;
        }
	addr[0] = 0;           /* "Reserved" */
	addr[1] = 2;           /* "IMMED" flag */
	addr[2] = 0;           /* "Descriptor Length" (MSB) */
	addr[3] = 8;           /* "Descriptor Length" (LSB) */
	addr[4+0] = 0xff;
	addr[4+1] = 0xff;
	addr[4+2] = 0xff;
	addr[4+3] = 0xff;
	addr[4+4] = 0x26<<2;
	addr[4+5] = 0;
	addr[4+6] = 0;
	addr[4+7] = 0;
	if (formattype == FORCE_FORMAT) {
	    printf("format_mdvd: forcing reformat.\n"); 
	    formattype = FULL_FORMAT;
	    addr[4+0] = 0;
	    addr[4+1] = 0;
	    addr[4+2] = 0;
	    addr[4+3] = 0;
	    addr[4+7] = 1;
	} else {
	    printf("format_mdvd: media is unformated.\n"); 
	}

	if (lverbose) {
		printf("Formating %s\n", format_types[formattype & 0x07]);
		flush();
	}
	if (formattype == FULL_FORMAT) {
	    ret = scsi_format(usalp, &addr, sizeof(addr), FALSE);
	} else {
	    ret = scsi_format(usalp, &addr, sizeof(addr), TRUE);
	}
	if (ret < 0)
		return (ret);

	wait_unit_ready(usalp, 90*60/curspeed);	/* XXX Wait for ATAPI */
	waitfix_mmc(usalp, 90*60/curspeed);	/* XXX Wait for ATAPI */
	return (ret);
}

static int 
send_opc_mmc(SCSI *usalp, caddr_t bp, int cnt, int doopc)
{
	int	ret;

	usalp->silent++;
	ret = send_opc(usalp, bp, cnt, doopc);
	usalp->silent--;

	if (ret >= 0)
		return (ret);

	/* BEGIN CSTYLED */
	/*
	 * Hack for a mysterioys drive ....
	 * Device type    : Removable CD-ROM
	 * Version        : 0
	 * Response Format: 1
	 * Vendor_info    : 'RWD     '
	 * Identifikation : 'RW2224          '
	 * Revision       : '2.53'
	 * Device seems to be: Generic mmc CD-RW.
	 *
	 * Performing OPC...
	 * CDB:  54 01 00 00 00 00 00 00 00 00
	 * Sense Bytes: 70 00 06 00 00 00 00 0A 00 00 00 00 5A 03 00 00
	 * Sense Key: 0x6 Unit Attention, Segment 0
	 * Sense Code: 0x5A Qual 0x03 (operator selected write permit) Fru 0x0
	 * Sense flags: Blk 0 (not valid)
	 */
	/* END CSTYLED */
	if (usal_sense_key(usalp) == SC_UNIT_ATTENTION &&
	    usal_sense_code(usalp) == 0x5A &&
	    usal_sense_qual(usalp) == 0x03)
		return (0);

	/*
	 * Do not make the condition:
	 * "Power calibration area almost full" a fatal error.
	 * It just flags that we have a single and last chance to write now.
	 */
	if ((usal_sense_key(usalp) == SC_RECOVERABLE_ERROR ||
	    usal_sense_key(usalp) == SC_MEDIUM_ERROR) &&
	    usal_sense_code(usalp) == 0x73 &&
	    usal_sense_qual(usalp) == 0x01)
		return (0);

	/*
	 * Send OPC is optional.
	 */
	if (usal_sense_key(usalp) != SC_ILLEGAL_REQUEST) {
		if (usalp->silent <= 0)
			usal_printerr(usalp);
		return (ret);
	}
	return (0);
}

static int 
opt1_mmc(SCSI *usalp, cdr_t *dp)
{
	int	oflags = dp->cdr_dstat->ds_cdrflags;

	if ((dp->cdr_dstat->ds_cdrflags & RF_AUDIOMASTER) != 0) {
		printf("Turning Audio Master Q. R. on\n");
		if (set_audiomaster_yamaha(usalp, dp, TRUE) < 0)
			return (-1);
		if (!debug && lverbose <= 1)
			dp->cdr_dstat->ds_cdrflags &= ~RF_PRATIP;
		if (getdisktype_mmc(usalp, dp) < 0) {
			dp->cdr_dstat->ds_cdrflags = oflags;
			return (-1);
		}
		dp->cdr_dstat->ds_cdrflags = oflags;
		if (oflags & RF_PRATIP) {
			msf_t   msf;
			lba_to_msf(dp->cdr_dstat->ds_first_leadin, &msf);
			printf("New start of lead in: %ld (%02d:%02d/%02d)\n",
				(long)dp->cdr_dstat->ds_first_leadin,
				msf.msf_min,
				msf.msf_sec,
				msf.msf_frame);
			lba_to_msf(dp->cdr_dstat->ds_maxblocks, &msf);
			printf("New start of lead out: %ld (%02d:%02d/%02d)\n",
				(long)dp->cdr_dstat->ds_maxblocks,
				msf.msf_min,
				msf.msf_sec,
				msf.msf_frame);
		}
	}
	if (mmc_isplextor(usalp)) {
		int	gcode;

		if ((dp->cdr_flags & (CDR_SINGLESESS|CDR_HIDE_CDR)) != 0) {
			if (ss_hide_plextor(usalp,
			    (dp->cdr_dstat->ds_cdrflags & RF_SINGLESESS) != 0,
			    (dp->cdr_dstat->ds_cdrflags & RF_HIDE_CDR) != 0) < 0)
				return (-1);
		}

		if ((dp->cdr_flags & CDR_SPEEDREAD) != 0) {
			if (speed_rd_plextor(usalp,
			    (dp->cdr_dstat->ds_cdrflags & RF_SPEEDREAD) != 0) < 0)
				return (-1);
		}

		if ((dp->cdr_cmdflags & F_SETDROPTS) ||
		    (wm_base(dp->cdr_dstat->ds_wrmode) == WM_SAO) ||
		    (wm_base(dp->cdr_dstat->ds_wrmode) == WM_RAW))
			gcode = do_gigarec_plextor(usalp);
		else
			gcode = gigarec_plextor(usalp, 0);
		if (gcode != 0) {
			msf_t   msf;

			dp->cdr_dstat->ds_first_leadin =
					gigarec_mult(gcode, dp->cdr_dstat->ds_first_leadin);
			dp->cdr_dstat->ds_maxblocks =
					gigarec_mult(gcode, dp->cdr_dstat->ds_maxblocks);

			if (oflags & RF_PRATIP) {
				lba_to_msf(dp->cdr_dstat->ds_first_leadin, &msf);
				printf("New start of lead in: %ld (%02d:%02d/%02d)\n",
					(long)dp->cdr_dstat->ds_first_leadin,
					msf.msf_min,
					msf.msf_sec,
					msf.msf_frame);
				lba_to_msf(dp->cdr_dstat->ds_maxblocks, &msf);
				printf("New start of lead out: %ld (%02d:%02d/%02d)\n",
					(long)dp->cdr_dstat->ds_maxblocks,
					msf.msf_min,
					msf.msf_sec,
					msf.msf_frame);
			}
		}
	}
	return (0);
}

static int 
opt2_mmc(SCSI *usalp, cdr_t *dp)
{
	Uchar	mode[0x100];
	Uchar	moder[0x100];
	int	len;
	struct	cd_mode_page_05 *mp;
	struct	ricoh_mode_page_30 *rp = NULL;
	BOOL	burnfree = FALSE;

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len))
		return (-1);
	if (len == 0)
		return (-1);

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);


	rp = get_justlink_ricoh(usalp, moder);

	if (dp->cdr_cdcap->BUF != 0) {
		burnfree = (mp->BUFE != 0);
	} else if ((dp->cdr_flags & CDR_BURNFREE) != 0) {
		burnfree = (rp && (rp->BUEFE != 0));
	}

	if (lverbose>2 && (dp->cdr_flags & CDR_BURNFREE) != 0)
		printf("BURN-Free is %s.\n", burnfree?"ON":"OFF");

	if (!burnfree && (dp->cdr_dstat->ds_cdrflags & RF_BURNFREE) != 0) {
		if(lverbose>2)
			printf("Turning BURN-Free on\n");
		burnfree = TRUE;
	}
	if (burnfree && (dp->cdr_dstat->ds_cdrflags & RF_BURNFREE) == 0) {
		if(lverbose>2)
			printf("Turning BURN-Free off\n");
		burnfree = FALSE;
	}
	if (dp->cdr_cdcap->BUF != 0) {
		mp->BUFE = burnfree?1:0;
	} 
    else if ((dp->cdr_flags & CDR_BURNFREE) != 0) {

		if (rp)
			rp->BUEFE = burnfree?1:0;
	}
	if (rp) {
		/*
		 * Clear Just-Link counter
		 */
		i_to_2_byte(rp->link_counter, 0);
		if (xdebug)
			usal_prbytes("Mode Select Data ", moder, moder[0]+1);

		if (!set_mode_params(usalp, "Ricoh Vendor Page", moder, moder[0]+1, 0, -1))
			return (-1);
		rp = get_justlink_ricoh(usalp, moder);
	}

#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif
	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1))
		return (-1);

	if (mmc_isplextor(usalp)) {
		/*
		 * Clear Burn-Proof counter
		 */
		usalp->silent++;
		bpc_plextor(usalp, 1, NULL);
		usalp->silent--;

		do_varirec_plextor(usalp);
	}

	return (0);
}

static int 
opt1_mdvd(SCSI *usalp, cdr_t *dp)
{
	int	oflags = dp->cdr_dstat->ds_cdrflags;

	if ((dp->cdr_dstat->ds_cdrflags & RF_AUDIOMASTER) != 0) {
		printf("Turning Audio Master Q. R. on\n");
		if (set_audiomaster_yamaha(usalp, dp, TRUE) < 0)
			return (-1);
		if (!debug && lverbose <= 1)
			dp->cdr_dstat->ds_cdrflags &= ~RF_PRATIP;
		if (getdisktype_mdvd(usalp, dp) < 0) {
			dp->cdr_dstat->ds_cdrflags = oflags;
			return (-1);
		}
		dp->cdr_dstat->ds_cdrflags = oflags;
		if (oflags & RF_PRATIP) {
			msf_t   msf;
			lba_to_msf(dp->cdr_dstat->ds_first_leadin, &msf);
			printf("New start of lead in: %ld (%02d:%02d/%02d)\n",
				(long)dp->cdr_dstat->ds_first_leadin,
		                msf.msf_min,
        		        msf.msf_sec,
                		msf.msf_frame);
			lba_to_msf(dp->cdr_dstat->ds_maxblocks, &msf);
			printf("New start of lead out: %ld (%02d:%02d/%02d)\n",
				(long)dp->cdr_dstat->ds_maxblocks,
		                msf.msf_min,
        		        msf.msf_sec,
                		msf.msf_frame);
		}
	}
	return (0);
}

static int
scsi_sony_write(SCSI *usalp, 
                caddr_t bp      /* address of buffer */, 
                long sectaddr   /* disk address (sector) to put */, 
                long size       /* number of bytes to transfer */, 
                int blocks      /* sector count */, 
                BOOL islast     /* last write for track */)
{
	return (write_xg5(usalp, bp, sectaddr, size, blocks));
}

Uchar	db2df[] = {
	0x00,			/*  0 2352 bytes of raw data			*/
	0xFF,			/*  1 2368 bytes (raw data + P/Q Subchannel)	*/
	0xFF,			/*  2 2448 bytes (raw data + P-W Subchannel)	*/
	0xFF,			/*  3 2448 bytes (raw data + P-W raw Subchannel)*/
	0xFF,			/*  4 -    Reserved				*/
	0xFF,			/*  5 -    Reserved				*/
	0xFF,			/*  6 -    Reserved				*/
	0xFF,			/*  7 -    Vendor specific			*/
	0x10,			/*  8 2048 bytes Mode 1 (ISO/IEC 10149)		*/
	0x30,			/*  9 2336 bytes Mode 2 (ISO/IEC 10149)		*/
	0xFF,			/* 10 2048 bytes Mode 2! (CD-ROM XA form 1)	*/
	0xFF,			/* 11 2056 bytes Mode 2 (CD-ROM XA form 1)	*/
	0xFF,			/* 12 2324 bytes Mode 2 (CD-ROM XA form 2)	*/
	0xFF,			/* 13 2332 bytes Mode 2 (CD-ROM XA 1/2+subhdr)	*/
	0xFF,			/* 14 -    Reserved				*/
	0xFF,			/* 15 -    Vendor specific			*/
};

static int 
gen_cue_mmc(track_t *trackp, void *vcuep, BOOL needgap)
{
	int	tracks = trackp->tracks;
	int	i;
	struct mmc_cue	**cuep = vcuep;
	struct mmc_cue	*cue;
	struct mmc_cue	*cp;
	int	ncue = 0;
	int	icue = 0;
	int	pgsize;
	msf_t	m;
	int	ctl;
	int	df;
	int	scms;

	cue = malloc(1);

	for (i = 0; i <= tracks; i++) {
		ctl = (st2mode[trackp[i].sectype & ST_MASK]) << 4;
		if (is_copy(&trackp[i]))
			ctl |= TM_ALLOW_COPY << 4;
		if (is_quadro(&trackp[i]))
			ctl |= TM_QUADRO << 4;
		df = db2df[trackp[i].dbtype & 0x0F];
		if (trackp[i].tracktype == TOC_XA2 &&
		    trackp[i].sectype   == (SECT_MODE_2_MIX|ST_MODE_RAW)) {
			/*
			 * Hack for CUE with MODE2/CDI and
			 * trackp[i].dbtype == DB_RAW
			 */
			df = 0x21;
		}

		if (trackp[i].isrc) {	/* MCN or ISRC */
			ncue += 2;
			cue = realloc(cue, ncue * sizeof (*cue));
			cp = &cue[icue++];
			if (i == 0) {
				cp->cs_ctladr = 0x02;
				movebytes(&trackp[i].isrc[0], &cp->cs_tno, 7);
				cp = &cue[icue++];
				cp->cs_ctladr = 0x02;
				movebytes(&trackp[i].isrc[7], &cp->cs_tno, 7);
			} else {
				cp->cs_ctladr = 0x03;
				cp->cs_tno = i;
				movebytes(&trackp[i].isrc[0], &cp->cs_index, 6);
				cp = &cue[icue++];
				cp->cs_ctladr = 0x03;
				cp->cs_tno = i;
				movebytes(&trackp[i].isrc[6], &cp->cs_index, 6);
			}
		}
		if (i == 0) {	/* Lead in */
			df &= ~7;	/* Mask off data size & nonRAW subch */
			if (df < 0x10)
				df |= 1;
			else
				df |= 4;
			if (trackp[0].flags & TI_TEXT)	/* CD-Text in Lead-in*/
				df |= 0x40;
			lba_to_msf(-150, &m);
			cue = realloc(cue, ++ncue * sizeof (*cue));
			cp = &cue[icue++];
			fillcue(cp, ctl|0x01, i, 0, df, 0, &m);
		} else {
			scms = 0;

			if (is_scms(&trackp[i]))
				scms = 0x80;
			pgsize = trackp[i].pregapsize;
			if (pgsize == 0 && needgap)
				pgsize++;
			lba_to_msf(trackp[i].trackstart-pgsize, &m);
			cue = realloc(cue, ++ncue * sizeof (*cue));
			cp = &cue[icue++];
			fillcue(cp, ctl|0x01, i, 0, df, scms, &m);

			if (trackp[i].nindex == 1) {
				lba_to_msf(trackp[i].trackstart, &m);
				cue = realloc(cue, ++ncue * sizeof (*cue));
				cp = &cue[icue++];
				fillcue(cp, ctl|0x01, i, 1, df, scms, &m);
			} else {
				int	idx;
				long	*idxlist;

				ncue += trackp[i].nindex;
				idxlist = trackp[i].tindex;
				cue = realloc(cue, ncue * sizeof (*cue));

				for (idx = 1; idx <= trackp[i].nindex; idx++) {
					lba_to_msf(trackp[i].trackstart + idxlist[idx], &m);
					cp = &cue[icue++];
					fillcue(cp, ctl|0x01, i, idx, df, scms, &m);
				}
			}
		}
	}
	/* Lead out */
	ctl = (st2mode[trackp[tracks+1].sectype & ST_MASK]) << 4;
	if (is_copy(&trackp[i]))
		ctl |= TM_ALLOW_COPY << 4;
	if (is_quadro(&trackp[i]))
		ctl |= TM_QUADRO << 4;
	df = db2df[trackp[tracks+1].dbtype & 0x0F];
	if (trackp[i].tracktype == TOC_XA2 &&
	    trackp[i].sectype   == (SECT_MODE_2_MIX|ST_MODE_RAW)) {
		/*
		 * Hack for CUE with MODE2/CDI and
		 * trackp[i].dbtype == DB_RAW
		 */
		df = 0x21;
	}
	df &= ~7;	/* Mask off data size & nonRAW subch */
	if (df < 0x10)
		df |= 1;
	else
		df |= 4;
	lba_to_msf(trackp[tracks+1].trackstart, &m);
	cue = realloc(cue, ++ncue * sizeof (*cue));
	cp = &cue[icue++];
	fillcue(cp, ctl|0x01, 0xAA, 1, df, 0, &m);

	if (lverbose > 1) {
		for (i = 0; i < ncue; i++) {
			usal_prbytes("", (Uchar *)&cue[i], 8);
		}
	}
	if (cuep)
		*cuep = cue;
	else
		free(cue);
	return (ncue);
}

static void 
fillcue(struct mmc_cue *cp  /* The target cue entry */, 
        int ca              /* Control/adr for this entry */, 
        int tno             /* Track number for this entry */, 
        int idx             /* Index for this entry */, 
        int dataform        /* Data format for this entry */, 
        int scms            /* Serial copy management */, 
        msf_t *mp           /* MSF value for this entry */)
{
	cp->cs_ctladr = ca;		/* XXX wie lead in */
	cp->cs_tno = tno;
	cp->cs_index = idx;
	cp->cs_dataform = dataform;	/* XXX wie lead in */
	cp->cs_scms = scms;
	cp->cs_min = mp->msf_min;
	cp->cs_sec = mp->msf_sec;
	cp->cs_frame = mp->msf_frame;
}

static int 
send_cue_mmc(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	struct mmc_cue	*cp;
	int		ncue;
	int		ret;
	Uint		i;

	for (i = 1; i <= trackp->tracks; i++) {
		if (trackp[i].tracksize < (tsize_t)0) {
			errmsgno(EX_BAD, "Track %d has unknown length.\n", i);
			return (-1);
		}
	}
	ncue = (*dp->cdr_gen_cue)(trackp, &cp, FALSE);

	usalp->silent++;
	ret = send_cue_sheet(usalp, (caddr_t)cp, ncue*8);
	usalp->silent--;
	free(cp);
	if (ret < 0) {
		errmsgno(EX_BAD, "CUE sheet not accepted. Retrying with minimum pregapsize = 1.\n");
		ncue = (*dp->cdr_gen_cue)(trackp, &cp, TRUE);
		ret = send_cue_sheet(usalp, (caddr_t)cp, ncue*8);
		if (ret < 0) {
			errmsgno(EX_BAD,
			"CUE sheet still not accepted. Please try to write in RAW (-raw96r) mode.\n");
		}
		free(cp);
	}
	return (ret);
}

static int 
stats_mmc(SCSI *usalp, cdr_t *dp)
{
	Uchar mode[256];
	struct	ricoh_mode_page_30 *rp;
	UInt32_t count;

	if (mmc_isplextor(usalp) && lverbose) {
		int	sels;
		int	maxs;
		int	lasts;

		/*
		 * Run it in silent mode as old drives do not support it.
		 * As this function looks to be a part of the PowerRec
		 * features, we may want to check
		 * dp->cdr_flags & CDR_FORCESPEED
		 */
		usalp->silent++;
		if (get_speeds_plextor(usalp, &sels, &maxs, &lasts) >= 0) {
			printf("Last selected write speed: %dx\n",
						sels / 176);
			printf("Max media write speed:     %dx\n",
						maxs / 176);
			printf("Last actual write speed:   %dx\n",
						lasts / 176);
		}
		usalp->silent--;
	}

	if ((dp->cdr_dstat->ds_cdrflags & RF_BURNFREE) == 0)
		return (0);

	if (mmc_isplextor(usalp)) {
		int	i = 0;
		int	ret;

		/*
		 * Read Burn-Proof counter
		 */
		usalp->silent++;
		ret = bpc_plextor(usalp, 2, &i);
		usalp->silent--;
		if (ret < 0)
			return (-1);
		count = i;
		/*
		 * Clear Burn-Proof counter
		 */
		bpc_plextor(usalp, 1, NULL);
	} else {
		rp = get_justlink_ricoh(usalp, mode);
		if (rp)
			count = a_to_u_2_byte(rp->link_counter);
		else
			return (-1);
	}
	if (lverbose) {
		if (count == 0)
			printf("BURN-Free was never needed.\n");
		else
			printf("BURN-Free was %d times used.\n",
				(int)count);
	}
	return (0);
}
/*--------------------------------------------------------------------------*/
static BOOL 
mmc_isplextor(SCSI *usalp)
{
	if (usalp->inq != NULL &&
			strncmp(usalp->inq->vendor_info, "PLEXTOR", 7) == 0) {
		return (TRUE);
	}
	return (FALSE);
}

static BOOL 
mmc_isyamaha(SCSI *usalp)
{
	if (usalp->inq != NULL &&
			strncmp(usalp->inq->vendor_info, "YAMAHA", 6) == 0) {
		return (TRUE);
	}
	return (FALSE);
}

static void 
do_varirec_plextor(SCSI *usalp)
{
	char	*p;
	int	voff;

	p = hasdrvopt(driveropts, "varirec=");
	if (p == NULL || curspeed != 4) {
		if (check_varirec_plextor(usalp) >= 0)
			varirec_plextor(usalp, FALSE, 0);
	} else {
		if (*astoi(p, &voff) != '\0')
			comerrno(EX_BAD,
				"Bad varirec value '%s'.\n", p);
		if (check_varirec_plextor(usalp) < 0)
			comerrno(EX_BAD, "Drive does not support VariRec.\n");
		varirec_plextor(usalp, TRUE, voff);
	}
}

/*
 * GigaRec value table
 */
struct gr {
	Uchar	val;
	char	vadd;
	char	*name;
} gr[] = {
	{ 0x00,	0,  "off", },
	{ 0x00,	0,  "1.0", },
	{ 0x01,	2,  "1.2", },
	{ 0x02,	3,  "1.3", },
	{ 0x03,	4,  "1.4", },
	{ 0x81,	-2, "0.8", },
	{ 0x82,	-3, "0.7", },
	{ 0x83,	-4, "0.6", },
	{ 0x00,	0,  NULL, },
};

static int 
do_gigarec_plextor(SCSI *usalp)
{
	char	*p;
	int	val = 0;	/* Make silly GCC happy */

	p = hasdrvopt(driveropts, "gigarec=");
	if (p == NULL) {
		if (check_gigarec_plextor(usalp) >= 0)
			gigarec_plextor(usalp, 0);
	} else {
		struct gr *gp = gr;

		for (; gp->name != NULL; gp++) {
			if (streql(p, gp->name)) {
				val = gp->val;
				break;
			}
		}
		if (gp->name == NULL)
			comerrno(EX_BAD,
				"Bad gigarec value '%s'.\n", p);
		if (check_gigarec_plextor(usalp) < 0)
			comerrno(EX_BAD, "Drive does not support GigaRec.\n");
		return (gigarec_plextor(usalp, val));
	}
	return (0);
}

static int 
drivemode_plextor(SCSI *usalp, caddr_t bp, int cnt, int modecode, void *modeval)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	if (modeval == NULL) {
		scmd->flags |= SCG_RECV_DATA;
		scmd->addr = bp;
		scmd->size = cnt;
	} else {
		scmd->cdb.g5_cdb.res = 0x08;
	}
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xE9;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.addr[0] = modecode;
	if (modeval)
		movebytes(modeval, &scmd->cdb.g1_cdb.addr[1], 6);
	else
		i_to_2_byte(&scmd->cdb.g1_cdb.count[2], cnt);

	usalp->cmdname = "plextor drive mode";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

/*
 * #defines for drivemode_plextor()...
 */
#define	MODE_CODE_SH	0x01	/* Mode code for Single Session & Hide-CDR */
#define	MB1_SS		0x01	/* Single Session Mode			   */
#define	MB1_HIDE_CDR	0x02	/* Hide CDR Media			   */

#define	MODE_CODE_VREC	0x02	/* Mode code for Vari Rec		   */

#define	MODE_CODE_GREC	0x04	/* Mode code for Giga Rec		   */

#define	MODE_CODE_SPEED	0xbb	/* Mode code for Speed Read		   */
#define	MBbb_SPEAD_READ	0x01	/* Spead Read				   */
				/* Danach Speed auf 0xFFFF 0xFFFF setzen   */

static int 
drivemode2_plextor(SCSI *usalp, caddr_t bp, int cnt, int modecode, void *modeval)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	if (modeval == NULL) {
		scmd->flags |= SCG_RECV_DATA;
		scmd->addr = bp;
		scmd->size = cnt;
	} else {
		scmd->cdb.g5_cdb.res = 0x08;
	}
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xED;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.addr[0] = modecode;
	if (modeval)
		scmd->cdb.g5_cdb.reladr = *(char *)modeval != 0 ? 1 : 0;
	else
		i_to_2_byte(&scmd->cdb.g1_cdb.count[1], cnt);

	usalp->cmdname = "plextor drive mode2";

	if (usal_cmd(usalp) < 0)
		return (-1);

	return (0);
}

static int 
check_varirec_plextor(SCSI *usalp)
{
	int	modecode = 2;
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	return (0);
}

static int 
check_gigarec_plextor(SCSI *usalp)
{
	int	modecode = 4;
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	return (0);
}

static int 
varirec_plextor(SCSI *usalp, BOOL on, int val)
{
	int	modecode = 2;
	Uchar	setmode[8];
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));


	fillbytes(setmode, sizeof (setmode), '\0');
	setmode[0] = on?1:0;
	if (on) {
		if (val < -2 || val > 2)
			comerrno(EX_BAD, "Bad VariRec offset %d\n", val);
		printf("Turning Varirec on.\n");
		printf("Varirec offset is %d.\n", val);

		if (val > 0) {
			setmode[1] = val & 0x7F;
		} else {
			setmode[1] = (-val) & 0x7F;
			setmode[1] |= 0x80;
		}
	}

	if (drivemode_plextor(usalp, NULL, 0, modecode, setmode) < 0)
		return (-1);

	fillbytes(getmode, sizeof (getmode), '\0');
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0)
		return (-1);

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));

	return (0);
}

static int 
gigarec_plextor(SCSI *usalp, int val)
{
	int	modecode = 4;
	Uchar	setmode[8];
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));


	fillbytes(setmode, sizeof (setmode), '\0');
	setmode[1] = val;

	if (drivemode_plextor(usalp, NULL, 0, modecode, setmode) < 0)
		return (-1);

	fillbytes(getmode, sizeof (getmode), '\0');
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0)
		return (-1);

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));

	{
		struct gr *gp = gr;

		for (; gp->name != NULL; gp++) {
			if (getmode[3] == gp->val)
				break;
		}
		if (gp->name == NULL)
			printf("Unknown GigaRec value 0x%X.\n", getmode[3]);
		else
			printf("GigaRec %sis %s.\n", gp->val?"value ":"", gp->name);
	}
	return (getmode[3]);
}

static Int32_t 
gigarec_mult(int code, Int32_t val)
{
	Int32_t	add;
	struct gr *gp = gr;

	for (; gp->name != NULL; gp++) {
		if (code == gp->val)
			break;
	}
	if (gp->vadd == 0)
		return (val);

	add = val * gp->vadd / 10;
	return (val + add);
}

static int 
check_ss_hide_plextor(SCSI *usalp)
{
	int	modecode = 1;
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	return (getmode[2] & 0x03);
}

static int 
check_speed_rd_plextor(SCSI *usalp)
{
	int	modecode = 0xBB;
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	return (getmode[2] & 0x01);
}

static int 
check_powerrec_plextor(SCSI *usalp)
{
	int	modecode = 0;
	Uchar	getmode[8];

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode2_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (getmode[2] & 1)
		return (1);

	return (0);
}

static int 
ss_hide_plextor(SCSI *usalp, BOOL do_ss, BOOL do_hide)
{
	int	modecode = 1;
	Uchar	setmode[8];
	Uchar	getmode[8];
	BOOL	is_ss;
	BOOL	is_hide;

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));


	is_ss = (getmode[2] & MB1_SS) != 0;
	is_hide = (getmode[2] & MB1_HIDE_CDR) != 0;

	if (lverbose > 0) {
		printf("Single session is %s.\n", is_ss ? "ON":"OFF");
		printf("Hide CDR is %s.\n", is_hide ? "ON":"OFF");
	}

	fillbytes(setmode, sizeof (setmode), '\0');
	setmode[0] = getmode[2];		/* Copy over old values */
	if (do_ss >= 0) {
		if (do_ss)
			setmode[0] |= MB1_SS;
		else
			setmode[0] &= ~MB1_SS;
	}
	if (do_hide >= 0) {
		if (do_hide)
			setmode[0] |= MB1_HIDE_CDR;
		else
			setmode[0] &= ~MB1_HIDE_CDR;
	}

	if (do_ss >= 0 && do_ss != is_ss)
		printf("Turning single session %s.\n", do_ss?"on":"off");
	if (do_hide >= 0 && do_hide != is_hide)
		printf("Turning hide CDR %s.\n", do_hide?"on":"off");

	if (drivemode_plextor(usalp, NULL, 0, modecode, setmode) < 0)
		return (-1);

	fillbytes(getmode, sizeof (getmode), '\0');
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0)
		return (-1);

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));

	return (0);
}

static int 
speed_rd_plextor(SCSI *usalp, BOOL do_speedrd)
{
	int	modecode = 0xBB;
	Uchar	setmode[8];
	Uchar	getmode[8];
	BOOL	is_speedrd;

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));


	is_speedrd = (getmode[2] & MBbb_SPEAD_READ) != 0;

	if (lverbose > 0)
		printf("Speed-Read is %s.\n", is_speedrd ? "ON":"OFF");

	fillbytes(setmode, sizeof (setmode), '\0');
	setmode[0] = getmode[2];		/* Copy over old values */
	if (do_speedrd >= 0) {
		if (do_speedrd)
			setmode[0] |= MBbb_SPEAD_READ;
		else
			setmode[0] &= ~MBbb_SPEAD_READ;
	}

	if (do_speedrd >= 0 && do_speedrd != is_speedrd)
		printf("Turning Speed-Read %s.\n", do_speedrd?"on":"off");

	if (drivemode_plextor(usalp, NULL, 0, modecode, setmode) < 0)
		return (-1);

	fillbytes(getmode, sizeof (getmode), '\0');
	if (drivemode_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0)
		return (-1);

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));

	/*
	 * Set current read speed to new max value.
	 */
	if (do_speedrd >= 0 && do_speedrd != is_speedrd)
		scsi_set_speed(usalp, 0xFFFF, -1, ROTCTL_CAV);

	return (0);
}

static int 
powerrec_plextor(SCSI *usalp, BOOL do_powerrec)
{
	int	modecode = 0;
	Uchar	setmode[8];
	Uchar	getmode[8];
	BOOL	is_powerrec;
	int	speed;

	fillbytes(getmode, sizeof (getmode), '\0');
	usalp->silent++;
	if (drivemode2_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0) {
		usalp->silent--;
		return (-1);
	}
	usalp->silent--;

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));


	is_powerrec = (getmode[2] & 1) != 0;

	speed = a_to_u_2_byte(&getmode[4]);

	if (lverbose > 0) {
		printf("Power-Rec is %s.\n", is_powerrec ? "ON":"OFF");
		printf("Power-Rec write speed:     %dx (recommended)\n", speed / 176);
	}

	fillbytes(setmode, sizeof (setmode), '\0');
	setmode[0] = getmode[2];		/* Copy over old values */
	if (do_powerrec >= 0) {
		if (do_powerrec)
			setmode[0] |= 1;
		else
			setmode[0] &= ~1;
	}

	if (do_powerrec >= 0 && do_powerrec != is_powerrec)
		printf("Turning Power-Rec %s.\n", do_powerrec?"on":"off");

	if (drivemode2_plextor(usalp, NULL, 0, modecode, setmode) < 0)
		return (-1);

	fillbytes(getmode, sizeof (getmode), '\0');
	if (drivemode2_plextor(usalp, (caddr_t)getmode, sizeof (getmode), modecode, NULL) < 0)
		return (-1);

	if (lverbose > 1)
		usal_prbytes("Modes", getmode, sizeof (getmode));

	return (0);
}

static int 
get_speeds_plextor(SCSI *usalp, int *selp, int *maxp, int *lastp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	char	buf[10];
	int	i;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	fillbytes((caddr_t)buf, sizeof (buf), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->flags |= SCG_RECV_DATA;
	scmd->addr = buf;
	scmd->size = sizeof (buf);
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xEB;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	i_to_2_byte(&scmd->cdb.g1_cdb.count[1], sizeof (buf));

	usalp->cmdname = "plextor get speedlist";

	if (usal_cmd(usalp) < 0)
		return (-1);

	i = a_to_u_2_byte(&buf[4]);
	if (selp)
		*selp = i;

	i = a_to_u_2_byte(&buf[6]);
	if (maxp)
		*maxp = i;

	i = a_to_u_2_byte(&buf[8]);
	if (lastp)
		*lastp = i;

	return (0);
}

static int 
bpc_plextor(SCSI *usalp, int mode, int *bpp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	char	buf[4];
	int	i;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	fillbytes((caddr_t)buf, sizeof (buf), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->flags |= SCG_RECV_DATA;
	scmd->addr = buf;
	scmd->size = sizeof (buf);
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xF5;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	scmd->cdb.g5_cdb.addr[1] = 0x08;
	scmd->cdb.g5_cdb.addr[2] = mode;

	i_to_2_byte(&scmd->cdb.g1_cdb.count[1], sizeof (buf));

	usalp->cmdname = "plextor read bpc";

	if (usal_cmd(usalp) < 0)
		return (-1);

	if (usal_getresid(usalp) > 2)
		return (0);

	i = a_to_u_2_byte(buf);
	if (bpp)
		*bpp = i;

	return (0);
}

static int 
set_audiomaster_yamaha(SCSI *usalp, cdr_t *dp, BOOL keep_mode)
{
	Uchar	mode[0x100];
	int	len;
	int	ret = 0;
	struct	cd_mode_page_05 *mp;

	if (xdebug && !keep_mode)
		printf("Checking for Yamaha Audio Master feature: ");

	/*
	 * Do not reset mp->test_write (-dummy) here.
	 */
	deflt_writemodes_mmc(usalp, FALSE);

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	usalp->silent++;
	if (!get_mode_params(usalp, 0x05, "CD write parameter",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {
		usalp->silent--;
		return (-1);
	}
	if (len == 0) {
		usalp->silent--;
		return (-1);
	}

	mp = (struct cd_mode_page_05 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);
#ifdef	DEBUG
	usal_prbytes("CD write parameter:", (Uchar *)mode, len);
#endif

	/*
	 * Do not set mp->test_write (-dummy) here. It should be set
	 * only at one place and only one time.
	 */
	mp->BUFE = 0;

	mp->write_type = 8;
	mp->track_mode = 0;
	mp->dbtype = DB_RAW;

	if (!set_mode_params(usalp, "CD write parameter", mode, len, 0, -1))
		ret = -1;

	/*
	 * Do not reset mp->test_write (-dummy) here.
	 */
	if (!keep_mode || ret < 0)
		deflt_writemodes_mmc(usalp, FALSE);
	usalp->silent--;

	return (ret);
}

struct 
ricoh_mode_page_30 *get_justlink_ricoh(SCSI *usalp, Uchar *mode)
{
	Uchar	modec[0x100];
	int	len;
	struct	ricoh_mode_page_30 *mp;

	usalp->silent++;
	if (!get_mode_params(usalp, 0x30, "Ricoh Vendor Page", mode, modec, NULL, NULL, &len)) {
		usalp->silent--;
		return ((struct ricoh_mode_page_30 *)0);
	}
	usalp->silent--;

	/*
	 * SCSI mode header + 6 bytes mode page 30.
	 * This is including the Burn-Free counter.
	 */
	if (len < 10)
		return ((struct ricoh_mode_page_30 *)0);

	if (xdebug) {
		fprintf(stderr, "Mode len: %d\n", len);
		usal_prbytes("Mode Sense Data ", mode, len);
		usal_prbytes("Mode Sence CData", modec, len);
	}

	mp = (struct ricoh_mode_page_30 *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	/*
	 * 6 bytes mode page 30.
	 * This is including the Burn-Free counter.
	 */
	if ((len - ((Uchar *)mp - mode) -1) < 5)
		return ((struct ricoh_mode_page_30 *)0);

	if (xdebug) {
		fprintf(stderr, "Burnfree counter: %d\n", a_to_u_2_byte(mp->link_counter));
	}
	return (mp);
}

static int 
force_speed_yamaha(SCSI *usalp, int readspeed, int writespeed)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xBB;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);

	if (readspeed < 0)
		i_to_2_byte(&scmd->cdb.g5_cdb.addr[0], 0xFFFF);
	else
		i_to_2_byte(&scmd->cdb.g5_cdb.addr[0], readspeed);
	if (writespeed < 0)
		i_to_2_byte(&scmd->cdb.g5_cdb.addr[2], 0xFFFF);
	else
		i_to_2_byte(&scmd->cdb.g5_cdb.addr[2], writespeed);

	scmd->cdb.cmd_cdb[11] = 0x80;

	usalp->cmdname = "yamaha force cd speed";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

static BOOL
get_tattoo_yamaha(SCSI *usalp, BOOL print, Int32_t *irp, Int32_t *orp)
{
	Uchar	mode[0x100];
	int	len;
	UInt32_t ival;
	UInt32_t oval;
	Uchar	*mp;

	usalp->silent++;
	if (!get_mode_params(usalp, 0x31, "Yamaha Tattoo Page", mode, NULL, NULL, NULL, &len)) {
		usalp->silent--;
		return (FALSE);
	}
	usalp->silent--;

	/*
	 * SCSI mode header + 16 bytes mode page 31.
	 * This is including the Burn-Free counter.
	 */
	if (len < 20)
		return (FALSE);

	mp = (Uchar *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	/*
	 * 10 bytes mode page 31.
	 * This is including the Burn-Free counter.
	 */
	if ((len - ((Uchar *)mp - mode) -1) < 10)
		return (FALSE);

	ival = a_to_u_3_byte(&mp[4]);
	oval = a_to_u_3_byte(&mp[7]);

	if (irp)
		*irp = ival;
	if (orp)
		*orp = oval;

	if (print && ival > 0 && oval > 0) {
		printf("DiskT@2 inner r: %d\n", (int)ival);
		printf("DiskT@2 outer r: %d\n", (int)oval);
		printf("DiskT@2 image size: 3744 x %d pixel.\n",
						(int)(oval-ival)+1);
	}

	return (TRUE);
}

static int 
do_tattoo_yamaha(SCSI *usalp, FILE *f)
{
	Int32_t ival = 0;
	Int32_t oval = 0;
	Int32_t	lines;
	off_t	fsize;
	char	*buf = usalp->bufptr;
	long	bufsize = usalp->maxbuf;
	long	nsecs;
	long	amt;

	nsecs = bufsize / 2048;
	bufsize = nsecs * 2048;

	if (!get_tattoo_yamaha(usalp, FALSE, &ival, &oval)) {
		errmsgno(EX_BAD, "Cannot get DiskT@2 info.\n");
		return (-1);
	}

	if (ival == 0 || oval == 0) {
		errmsgno(EX_BAD, "DiskT@2 info not valid.\n");
		return (-1);
	}

	lines = oval - ival + 1;
	fsize = filesize(f);
	if ((fsize % 3744) != 0 || fsize < (lines*3744)) {
		errmsgno(EX_BAD, "Illegal DiskT@2 file size.\n");
		return (-1);
	}
	if (fsize > (lines*3744))
		fsize = lines*3744;

	if (lverbose)
		printf("Starting to write DiskT@2 data.\n");
	fillbytes(buf, bufsize, '\0');
	if ((amt = fileread(f, buf, bufsize)) <= 0) {
		errmsg("DiskT@2 file read error.\n");
		return (-1);
	}

	if (yamaha_write_buffer(usalp, 1, 0, ival, amt/2048, buf, amt) < 0) {
		errmsgno(EX_BAD, "DiskT@2 1st write error.\n");
		return (-1);
	}
	amt = (amt+2047) / 2048 * 2048;
	fsize -= amt;

	while (fsize > 0) {
		fillbytes(buf, bufsize, '\0');
		if ((amt = fileread(f, buf, bufsize)) <= 0) {
			errmsg("DiskT@2 file read error.\n");
			return (-1);
		}
		amt = (amt+2047) / 2048 * 2048;
		fsize -= amt;
		if (yamaha_write_buffer(usalp, 1, 0, 0, amt/2048, buf, amt) < 0) {
			errmsgno(EX_BAD, "DiskT@2 write error.\n");
			return (-1);
		}
	}

	if (yamaha_write_buffer(usalp, 1, 0, oval, 0, buf, 0) < 0) {
		errmsgno(EX_BAD, "DiskT@2 final error.\n");
		return (-1);
	}

	wait_unit_ready(usalp, 1000);	/* Wait for DiskT@2 */
	waitfix_mmc(usalp, 1000);	/* Wait for DiskT@2 */

	return (0);
}

/*
 * Yamaha specific version of 'write buffer' that offers an additional
 * Parameter Length 'parlen' parameter.
 */
static int 
yamaha_write_buffer(SCSI *usalp, int mode, int bufferid, long offset,
                    long parlen, void *buffer, long buflen)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
		Uchar	*CDB;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = buffer;
	scmd->size = buflen;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x3B;

	CDB    = (Uchar *)scmd->cdb.cmd_cdb;
	CDB[1] = mode & 7;
	CDB[2] = bufferid;
	i_to_3_byte(&CDB[3], offset);
	i_to_3_byte(&CDB[6], parlen);

	usalp->cmdname = "write_buffer";

	if (usal_cmd(usalp) >= 0)
		return (1);
	return (0);
}

static int 
dvd_dual_layer_split(SCSI *usalp, cdr_t *dp, long tsize)
{
    unsigned char	xb[12];
    long 	l0_size;
    
    /* Get the Layer 0 defined data zone*/
    if (read_dvd_structure(usalp, (caddr_t)xb, 12, 0, 0, 0x20) >= 0) {
	if ((xb[1] | xb[0] << 8) < 13) {
	    fprintf(stderr, "dvd_dual_layer_split: read_dvd_structure returns invalid data\n");
	    return 1;
	}
	if (xb[4] & 0x80) {
	    printf("L0 zone size already set\n");
	    return 1;
	}
	l0_size = xb[11] | xb[10] << 8 | xb[9] << 16 | xb[8] << 24;
	if (tsize < l0_size) {
	    fprintf(stderr, "track size smaller than one layer, use --force to force burning.");
	    return 0;
	}
	printf("L0 size: %ld (track size %ld)\n", l0_size, tsize);
	l0_size = tsize / 2;
	l0_size = l0_size - 1 + 16 - (l0_size - 1) % 16;
	printf("New L0 size: %ld\n", l0_size);

	memset (xb, 0, sizeof(xb));
	xb[1]  = sizeof(xb) - 2;
	xb[8]  = l0_size >> 24;
	xb[9]  = l0_size >> 16;
	xb[10] = l0_size >> 8;
	xb[11] = l0_size;
	if (send_dvd_structure(usalp, (caddr_t)xb, 12, 0, 0x20)) {
	    fprintf(stderr, "dvd_dual_layer_split: send_dvd_structure failed, could not set middle zone location.\n");
	    return 0;
	}
    }
   return 1;
}
