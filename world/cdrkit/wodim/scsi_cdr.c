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

/* @(#)scsi_cdr.c	1.137 04/05/25 Copyright 1995-2004 J. Schilling */
/*
 *	SCSI command functions for cdrecord
 *	covering pre-MMC standard functions up to MMC-2
 *
 *	Copyright (c) 1995-2004 J. Schilling
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
 * NOTICE:	The Philips CDD 521 has several firmware bugs.
 *		One of them is not to respond to a SCSI selection
 *		within 200ms if the general load on the
 *		SCSI bus is high. To deal with this problem
 *		most of the SCSI commands are send with the
 *		SCG_CMD_RETRY flag enabled.
 */
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
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
#include "wodim.h"

#define	strbeg(s1, s2)	(strstr((s2), (s1)) == (s2))

BOOL	unit_ready(SCSI *usalp);
BOOL	wait_unit_ready(SCSI *usalp, int secs);
BOOL	scsi_in_progress(SCSI *usalp);
BOOL	cdr_underrun(SCSI *usalp);
int	test_unit_ready(SCSI *usalp);
int	rezero_unit(SCSI *usalp);
int	request_sense(SCSI *usalp);
int	request_sense_b(SCSI *usalp, caddr_t bp, int cnt);
int	inquiry(SCSI *usalp, caddr_t, int);
int	read_capacity(SCSI *usalp);
void	print_capacity(SCSI *usalp, FILE *f);
int	scsi_load_unload(SCSI *usalp, int);
int	scsi_prevent_removal(SCSI *usalp, int);
int	scsi_start_stop_unit(SCSI *usalp, int, int, BOOL immed);
int	scsi_set_speed(SCSI *usalp, int readspeed, int writespeed, int rotctl);
int	scsi_get_speed(SCSI *usalp, int *readspeedp, int *writespeedp);
int	qic02(SCSI *usalp, int);
int	write_xscsi(SCSI *usalp, caddr_t, long, long, int);
int	write_xg0(SCSI *usalp, caddr_t, long, long, int);
int	write_xg1(SCSI *usalp, caddr_t, long, long, int);
int	write_xg5(SCSI *usalp, caddr_t, long, long, int);
int	seek_scsi(SCSI *usalp, long addr);
int	seek_g0(SCSI *usalp, long addr);
int	seek_g1(SCSI *usalp, long addr);
int	scsi_flush_cache(SCSI *usalp, BOOL immed);
int	read_buffer(SCSI *usalp, caddr_t bp, int cnt, int mode);
int	write_buffer(SCSI *usalp, char *buffer, long length, int mode, 
						 int bufferid, long offset);
int	read_subchannel(SCSI *usalp, caddr_t bp, int track, int cnt, int msf, 
							 int subq, int fmt);
int	read_toc(SCSI *usalp, caddr_t, int, int, int, int);
int	read_toc_philips(SCSI *usalp, caddr_t, int, int, int, int);
int	read_header(SCSI *usalp, caddr_t, long, int, int);
int	read_disk_info(SCSI *usalp, caddr_t, int);
int	read_track_info(SCSI *usalp, caddr_t, int type, int addr, int cnt);
int	read_rzone_info(SCSI *usalp, caddr_t bp, int cnt);
int	reserve_tr_rzone(SCSI *usalp, long size);
int	read_dvd_structure(SCSI *usalp, caddr_t bp, int cnt, int addr, int layer, 
								 int fmt);
int	send_dvd_structure(SCSI *usalp, caddr_t bp, int cnt, int layer, int fmt);
int	send_opc(SCSI *usalp, caddr_t, int cnt, int doopc);
int	read_track_info_philips(SCSI *usalp, caddr_t, int, int);
int	scsi_close_tr_session(SCSI *usalp, int type, int track, BOOL immed);
int	read_master_cue(SCSI *usalp, caddr_t bp, int sheet, int cnt);
int	send_cue_sheet(SCSI *usalp, caddr_t bp, long size);
int	read_buff_cap(SCSI *usalp, long *, long *);
int	scsi_blank(SCSI *usalp, long addr, int blanktype, BOOL immed);
int	scsi_format(SCSI *usalp, caddr_t addr, int size, BOOL background);
int	scsi_set_streaming(SCSI *usalp, caddr_t addr, int size);
BOOL	allow_atapi(SCSI *usalp, BOOL new);
int	mode_select(SCSI *usalp, Uchar *, int, int, int);
int	mode_sense(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf);
int	mode_select_sg0(SCSI *usalp, Uchar *, int, int, int);
int	mode_sense_sg0(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf);
int	mode_select_g0(SCSI *usalp, Uchar *, int, int, int);
int	mode_select_g1(SCSI *usalp, Uchar *, int, int, int);
int	mode_sense_g0(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf);
int	mode_sense_g1(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf);
int	read_tochdr(SCSI *usalp, cdr_t *, int *, int *);
int	read_cdtext(SCSI *usalp);
int	read_trackinfo(SCSI *usalp, int, long *, struct msf *, int *, int *, 
							int *);
int	read_B0(SCSI *usalp, BOOL isbcd, long *b0p, long *lop);
int	read_session_offset(SCSI *usalp, long *);
int	read_session_offset_philips(SCSI *usalp, long *);
int	sense_secsize(SCSI *usalp, int current);
int	select_secsize(SCSI *usalp, int);
BOOL	is_cddrive(SCSI *usalp);
BOOL	is_unknown_dev(SCSI *usalp);
int	read_scsi(SCSI *usalp, caddr_t, long, int);
int	read_g0(SCSI *usalp, caddr_t, long, int);
int	read_g1(SCSI *usalp, caddr_t, long, int);
BOOL	getdev(SCSI *usalp, BOOL);
void	printinq(SCSI *usalp, FILE *f);
void	printdev(SCSI *usalp);
BOOL	do_inquiry(SCSI *usalp, BOOL);
BOOL	recovery_needed(SCSI *usalp, cdr_t *);
int	scsi_load(SCSI *usalp, cdr_t *);
int	scsi_unload(SCSI *usalp, cdr_t *);
int	scsi_cdr_write(SCSI *usalp, caddr_t bp, long sectaddr, long size, 
							int blocks, BOOL islast);
struct cd_mode_page_2A * mmc_cap(SCSI *usalp, Uchar *modep);
void	mmc_getval(struct cd_mode_page_2A *mp, BOOL *cdrrp, BOOL *cdwrp, 
					  BOOL *cdrrwp, BOOL *cdwrwp, BOOL *dvdp, BOOL *dvdwp);
BOOL	is_mmc(SCSI *usalp, BOOL *cdwp, BOOL *dvdwp);
BOOL	mmc_check(SCSI *usalp, BOOL *cdrrp, BOOL *cdwrp, BOOL *cdrrwp, 
					 BOOL *cdwrwp, BOOL *dvdp, BOOL *dvdwp);
static	void	print_speed(char *fmt, int val);
void	print_capabilities(SCSI *usalp);

BOOL
unit_ready(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (test_unit_ready(usalp) >= 0)		/* alles OK */
		return (TRUE);
	else if (scmd->error >= SCG_FATAL)	/* nicht selektierbar */
		return (FALSE);

	if (usal_sense_key(usalp) == SC_UNIT_ATTENTION) {
		if (test_unit_ready(usalp) >= 0)	/* alles OK */
			return (TRUE);
	}
	if ((usal_cmd_status(usalp) & ST_BUSY) != 0) {
		/*
		 * Busy/reservation_conflict
		 */
		usleep(500000);
		if (test_unit_ready(usalp) >= 0)	/* alles OK */
			return (TRUE);
	}
	if (usal_sense_key(usalp) == -1) {	/* non extended Sense */
		if (usal_sense_code(usalp) == 4)	/* NOT_READY */
			return (FALSE);
		return (TRUE);
	}
						/* FALSE wenn NOT_READY */
	return (usal_sense_key(usalp) != SC_NOT_READY);
}

BOOL
wait_unit_ready(SCSI *usalp, int secs)
{
	int	i;
	int	c;
	int	k;
	int	ret;

	usalp->silent++;
	ret = test_unit_ready(usalp);		/* eat up unit attention */
	if (ret < 0)
		ret = test_unit_ready(usalp);	/* got power on condition? */
	usalp->silent--;

	if (ret >= 0)				/* success that's enough */
		return (TRUE);

	usalp->silent++;
	for (i = 0; i < secs && (ret = test_unit_ready(usalp)) < 0; i++) {
		if (usalp->scmd->scb.busy != 0) {
			sleep(1);
			continue;
		}
		c = usal_sense_code(usalp);
		k = usal_sense_key(usalp);
		/*
		 * Abort quickly if it does not make sense to wait.
		 * 0x30 == Cannot read medium
		 * 0x3A == Medium not present
		 */
		if ((k == SC_NOT_READY && (c == 0x3A || c == 0x30)) ||
		    (k == SC_MEDIUM_ERROR)) {
			if (usalp->silent <= 1)
				usal_printerr(usalp);
			usalp->silent--;
			return (FALSE);
		}
		sleep(1);
	}
	usalp->silent--;
	if (ret < 0)
		return (FALSE);
	return (TRUE);
}

BOOL
scsi_in_progress(SCSI *usalp)
{
	if (usal_sense_key(usalp) == SC_NOT_READY &&
		/*
		 * Logigal unit not ready operation/long_write in progress
		 */
	    usal_sense_code(usalp) == 0x04 &&
	    (usal_sense_qual(usalp) == 0x04 || /* CyberDr. "format in progress"*/
	    usal_sense_qual(usalp) == 0x07 || /* "operation in progress"	    */
	    usal_sense_qual(usalp) == 0x08)) { /* "long write in progress"    */
		return (TRUE);
	} else {
		if (usalp->silent <= 1)
			usal_printerr(usalp);
	}
	return (FALSE);
}

BOOL
cdr_underrun(SCSI *usalp)
{
	if ((usal_sense_key(usalp) != SC_ILLEGAL_REQUEST &&
	    usal_sense_key(usalp) != SC_MEDIUM_ERROR))
		return (FALSE);

	if ((usal_sense_code(usalp) == 0x21 &&
	    (usal_sense_qual(usalp) == 0x00 ||	/* logical block address out of range */
	    usal_sense_qual(usalp) == 0x02)) ||	/* invalid address for write */

	    (usal_sense_code(usalp) == 0x0C &&
	    usal_sense_qual(usalp) == 0x09)) {	/* write error - loss of streaming */
		return (TRUE);
	}
	/*
	 * XXX Bei manchen Brennern kommt mach dem der Brennvorgang bereits
	 * XXX eine Weile gelaufen ist ein 5/24/0 Invalid field in CDB.
	 * XXX Daher sollte man testen ob schon geschrieben wurde...
	 */
	return (FALSE);
}

int
test_unit_ready(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA | (usalp->silent ? SCG_SILENT:0);
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_TEST_UNIT_READY;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "test unit ready";

	return (usal_cmd(usalp));
}

int
rezero_unit(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_REZERO_UNIT;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);

	usalp->cmdname = "rezero unit";

	return (usal_cmd(usalp));
}

int
request_sense(SCSI *usalp)
{
		char	sensebuf[CCS_SENSE_LEN];
	register struct	usal_cmd	*scmd = usalp->scmd;


	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = sensebuf;
	scmd->size = sizeof (sensebuf);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_REQUEST_SENSE;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = CCS_SENSE_LEN;

	usalp->cmdname = "request_sense";

	if (usal_cmd(usalp) < 0)
		return (-1);
	usal_prsense((Uchar *)sensebuf, CCS_SENSE_LEN - usal_getresid(usalp));
	return (0);
}

int
request_sense_b(SCSI *usalp, caddr_t bp, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;


	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_REQUEST_SENSE;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = cnt;

	usalp->cmdname = "request_sense";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
inquiry(SCSI *usalp, caddr_t bp, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes(bp, cnt, '\0');
	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_INQUIRY;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = cnt;

	usalp->cmdname = "inquiry";

	if (usal_cmd(usalp) < 0)
		return (-1);
	if (usalp->verbose)
		usal_prbytes("Inquiry Data   :", (Uchar *)bp, cnt - usal_getresid(usalp));
	return (0);
}

int
read_capacity(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)usalp->cap;
	scmd->size = sizeof (struct scsi_capacity);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x25;	/* Read Capacity */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, 0); /* Full Media */

	usalp->cmdname = "read capacity";

	if (usal_cmd(usalp) < 0) {
		return (-1);
	} else {
		long	cbsize;
		long	cbaddr;

		/*
		 * c_bsize & c_baddr are signed Int32_t
		 * so we use signed int conversion here.
		 */
		cbsize = a_to_4_byte(&usalp->cap->c_bsize);
		cbaddr = a_to_4_byte(&usalp->cap->c_baddr);
		usalp->cap->c_bsize = cbsize;
		usalp->cap->c_baddr = cbaddr;
	}
	return (0);
}

void
print_capacity(SCSI *usalp, FILE *f)
{
	long	kb;
	long	mb;
	long	prmb;
	double	dkb;

	dkb =  (usalp->cap->c_baddr+1.0) * (usalp->cap->c_bsize/1024.0);
	kb = dkb;
	mb = dkb / 1024.0;
	prmb = dkb / 1000.0 * 1.024;
	fprintf(f, "Capacity: %ld Blocks = %ld kBytes = %ld MBytes = %ld prMB\n",
		(long)usalp->cap->c_baddr+1, kb, mb, prmb);
	fprintf(f, "Sectorsize: %ld Bytes\n", (long)usalp->cap->c_bsize);
}

int
scsi_load_unload(SCSI *usalp, int load)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xA6;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.addr[1] = load?3:2;
	scmd->cdb.g5_cdb.count[2] = 0; /* slot # */

	usalp->cmdname = "medium load/unload";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
scsi_prevent_removal(SCSI *usalp, int prevent)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = 0x1E;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = prevent & 1;

	usalp->cmdname = "prevent/allow medium removal";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}


int
scsi_start_stop_unit(SCSI *usalp, int flg, int loej, BOOL immed)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = 0x1B;	/* Start Stop Unit */
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = (flg ? 1:0) | (loej ? 2:0);

	if (immed)
		scmd->cdb.cmd_cdb[1] |= 0x01;

	usalp->cmdname = "start/stop unit";

	return (usal_cmd(usalp));
}

int
scsi_set_streaming(SCSI *usalp, caddr_t perf_desc, int size)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = perf_desc;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xB6;
	scmd->cdb.cmd_cdb[11] = 0;
	scmd->cdb.cmd_cdb[10] = size;

	usalp->cmdname = "set streaming";

  if(usalp->verbose) 
     fprintf(stderr, "scsi_set_streaming\n");
	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}
    
int
scsi_set_speed(SCSI *usalp, int readspeed, int writespeed, int rotctl)
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

	scmd->cdb.cmd_cdb[1] |= rotctl & 0x03;

	usalp->cmdname = "set cd speed";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
scsi_get_speed(SCSI *usalp, int *readspeedp, int *writespeedp)
{
	struct	cd_mode_page_2A *mp;
	Uchar	m[256];
	int	val;

	usalp->silent++;
	mp = mmc_cap(usalp, m); /* Get MMC capabilities in allocated mp */
	usalp->silent--;
	if (mp == NULL)
		return (-1);	/* Pre SCSI-3/mmc drive		*/

	val = a_to_u_2_byte(mp->cur_read_speed);
	if (readspeedp)
		*readspeedp = val;

	if (mp->p_len >= 28)
		val = a_to_u_2_byte(mp->v3_cur_write_speed);
	else
		val = a_to_u_2_byte(mp->cur_write_speed);
	if (writespeedp)
		*writespeedp = val;

	return (0);
}


int
qic02(SCSI *usalp, int cmd)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = DEF_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = 0x0D;	/* qic02 Sysgen SC4000 */
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.mid_addr = cmd;

	usalp->cmdname = "qic 02";
	return (usal_cmd(usalp));
}

#define	G0_MAXADDR	0x1FFFFFL

int 
write_xscsi(SCSI *usalp, caddr_t bp, long addr, long size, int cnt)
{
	if (addr <= G0_MAXADDR)
		return (write_xg0(usalp, bp, addr, size, cnt));
	else
		return (write_xg1(usalp, bp, addr, size, cnt));
}

int 
write_xg0(SCSI *usalp, 
          caddr_t bp    /* address of buffer */, 
          long addr     /* disk address (sector) to put */, 
          long size     /* number of bytes to transfer */, 
          int cnt       /* sectorcount */)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
/*	scmd->flags = SCG_DISRE_ENA;*/
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_WRITE;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	g0_cdbaddr(&scmd->cdb.g0_cdb, addr);
	scmd->cdb.g0_cdb.count = cnt;

	usalp->cmdname = "write_g0";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (size - usal_getresid(usalp));
}

int 
write_xg1(SCSI *usalp, 
          caddr_t bp    /* address of buffer */, 
          long addr     /* disk address (sector) to put */, 
          long size     /* number of bytes to transfer */, 
          int cnt       /* sectorcount */)
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
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "write_g1";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (size - usal_getresid(usalp));
}

int 
write_xg5(SCSI *usalp,
          caddr_t bp    /* address of buffer */, 
          long addr     /* disk address (sector) to put */, 
          long size     /* number of bytes to transfer */, 
          int cnt       /* sectorcount */)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
/*	scmd->flags = SCG_DISRE_ENA;*/
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xAA;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	g5_cdbaddr(&scmd->cdb.g5_cdb, addr);
	g5_cdblen(&scmd->cdb.g5_cdb, cnt);

	usalp->cmdname = "write_g5";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (size - usal_getresid(usalp));
}

int
seek_scsi(SCSI *usalp, long addr)
{
	if (addr <= G0_MAXADDR)
		return (seek_g0(usalp, addr));
	else
		return (seek_g1(usalp, addr));
}

int
seek_g0(SCSI *usalp, long addr)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = 0x0B;	/* Seek */
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	g0_cdbaddr(&scmd->cdb.g0_cdb, addr);

	usalp->cmdname = "seek_g0";

	return (usal_cmd(usalp));
}

int
seek_g1(SCSI *usalp, long addr)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x2B;	/* Seek G1 */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);

	usalp->cmdname = "seek_g1";

	return (usal_cmd(usalp));
}

int
scsi_flush_cache(SCSI *usalp, BOOL immed)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 2 * 60;		/* Max: sizeof (CDR-cache)/150KB/s */
	scmd->cdb.g1_cdb.cmd = 0x35;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	if (immed)
		scmd->cdb.cmd_cdb[1] |= 0x02;

	usalp->cmdname = "flush cache";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_buffer(SCSI *usalp, caddr_t bp, int cnt, int mode)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->dma_read = 1;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x3C;	/* Read Buffer */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.cmd_cdb[1] |= (mode & 7);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read buffer";

	return (usal_cmd(usalp));
}

int
write_buffer(SCSI *usalp, char *buffer, long length, int mode, int bufferid, 
             long offset)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	char			*cdb;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = buffer;
	scmd->size = length;
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;

	cdb = (char *)scmd->cdb.cmd_cdb;

	cdb[0] = 0x3B;
	cdb[1] = mode & 7;
	cdb[2] = bufferid;
	cdb[3] = offset >> 16;
	cdb[4] = (offset >> 8) & 0xff;
	cdb[5] = offset & 0xff;
	cdb[6] = length >> 16;
	cdb[7] = (length >> 8) & 0xff;
	cdb[8] = length & 0xff;

	usalp->cmdname = "write_buffer";

	if (usal_cmd(usalp) >= 0)
		return (1);
	return (0);
}

int
read_subchannel(SCSI *usalp, caddr_t bp, int track, int cnt, int msf, int subq, 
					 int fmt)

{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x42;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	if (msf)
		scmd->cdb.g1_cdb.res = 1;
	if (subq)
		scmd->cdb.g1_cdb.addr[0] = 0x40;
	scmd->cdb.g1_cdb.addr[1] = fmt;
	scmd->cdb.g1_cdb.res6 = track;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read subchannel";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_toc(SCSI *usalp, caddr_t bp, int track, int cnt, int msf, int fmt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x43;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	if (msf)
		scmd->cdb.g1_cdb.res = 1;
	scmd->cdb.g1_cdb.addr[0] = fmt & 0x0F;
	scmd->cdb.g1_cdb.res6 = track;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read toc";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_toc_philips(SCSI *usalp, caddr_t bp, int track, int cnt, int msf, int fmt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 4 * 60;		/* May last  174s on a TEAC CD-R55S */
	scmd->cdb.g1_cdb.cmd = 0x43;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	if (msf)
		scmd->cdb.g1_cdb.res = 1;
	scmd->cdb.g1_cdb.res6 = track;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	if (fmt & 1)
		scmd->cdb.g1_cdb.vu_96 = 1;
	if (fmt & 2)
		scmd->cdb.g1_cdb.vu_97 = 1;

	usalp->cmdname = "read toc";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_header(SCSI *usalp, caddr_t bp, long addr, int cnt, int msf)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x44;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	if (msf)
		scmd->cdb.g1_cdb.res = 1;
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read header";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_disk_info(SCSI *usalp, caddr_t bp, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 4 * 60;		/* Needs up to 2 minutes */
	scmd->cdb.g1_cdb.cmd = 0x51;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read disk info";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_track_info(SCSI *usalp, caddr_t bp, int type, int addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 4 * 60;		/* Needs up to 2 minutes */
	scmd->cdb.g1_cdb.cmd = 0x52;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
/*	scmd->cdb.cmd_cdb[1] = type & 0x03;*/
	scmd->cdb.cmd_cdb[1] = type;
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);	/* LBA/Track/Session */
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read track info";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
reserve_track(SCSI *usalp, Ulong size)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x53;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	i_to_4_byte(&scmd->cdb.g1_cdb.addr[3], size);

	usalp->cmdname = "reserve track";

	if (usal_cmd(usalp) < 0) 
		return (-1);

	return (0);

}

int
read_rzone_info(SCSI *usalp, caddr_t bp, int cnt)
{
	return (read_track_info(usalp, bp, TI_TYPE_LBA, 0, cnt));
}

int
reserve_tr_rzone(SCSI *usalp, long size /* number of blocks */)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)0;
	scmd->size = 0;
	scmd->flags = SCG_DISRE_ENA|SCG_CMD_RETRY;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x53;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);

	i_to_4_byte(&scmd->cdb.g1_cdb.addr[3], size);

	usalp->cmdname = "reserve_track_rzone";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_dvd_structure(SCSI *usalp, caddr_t bp, int cnt, int addr, int layer, 
                   int fmt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 4 * 60;		/* Needs up to 2 minutes ??? */
	scmd->cdb.g5_cdb.cmd = 0xAD;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	g5_cdbaddr(&scmd->cdb.g5_cdb, addr);
	g5_cdblen(&scmd->cdb.g5_cdb, cnt);
	scmd->cdb.g5_cdb.count[0] = layer;
	scmd->cdb.g5_cdb.count[1] = fmt;

	usalp->cmdname = "read dvd structure";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
send_dvd_structure(SCSI *usalp, caddr_t bp, int cnt, int layer, int fmt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 4 * 60;		/* Needs up to 2 minutes ??? */
	scmd->cdb.g5_cdb.cmd = 0xBF;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	g5_cdblen(&scmd->cdb.g5_cdb, cnt);

	scmd->cdb.cmd_cdb[7] = fmt;

	usalp->cmdname = "send dvd structure";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
send_opc(SCSI *usalp, caddr_t bp, int cnt, int doopc)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 60;
	scmd->cdb.g1_cdb.cmd = 0x54;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.reladr = doopc?1:0;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "send opc";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_track_info_philips(SCSI *usalp, caddr_t bp, int track, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0xE5;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, track);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read track info";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
scsi_close_tr_session(SCSI *usalp, int type, int track, BOOL immed)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 8 * 60;		/* Needs up to 4 minutes */
	scmd->cdb.g1_cdb.cmd = 0x5B;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.addr[0] = type;
	scmd->cdb.g1_cdb.addr[3] = track;

	if (immed)
		scmd->cdb.g1_cdb.reladr = 1;
/*		scmd->cdb.cmd_cdb[1] |= 0x01;*/
#ifdef	nono
	scmd->cdb.g1_cdb.reladr = 1;	/* IMM hack to test Mitsumi behaviour*/
#endif

	usalp->cmdname = "close track/session";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
read_master_cue(SCSI *usalp, caddr_t bp, int sheet, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x59;		/* Read master cue */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g1_cdb.addr[2] = sheet;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read master cue";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (0);
}

int
send_cue_sheet(SCSI *usalp, caddr_t bp, long size)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x5D;	/* Send CUE sheet */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, size);

	usalp->cmdname = "send_cue_sheet";

	if (usal_cmd(usalp) < 0)
		return (-1);
	return (size - scmd->resid);
}

int
read_buff_cap(SCSI *usalp, long *sp, long *fp)
{
	char	resp[12];
	Ulong	freespace;
	Ulong	bufsize;
	int	per;
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)resp;
	scmd->size = sizeof (resp);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x5C;		/* Read buffer cap */
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdblen(&scmd->cdb.g1_cdb, sizeof (resp));

	usalp->cmdname = "read buffer cap";

	if (usal_cmd(usalp) < 0)
		return (-1);

	bufsize   = a_to_u_4_byte(&resp[4]);
	freespace = a_to_u_4_byte(&resp[8]);
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

int
scsi_blank(SCSI *usalp, long addr, int blanktype, BOOL immed)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 160 * 60; /* full blank at 1x could take 80 minutes */
	scmd->cdb.g5_cdb.cmd = 0xA1;	/* Blank */
	scmd->cdb.g0_cdb.high_addr = blanktype;
	g1_cdbaddr(&scmd->cdb.g5_cdb, addr);

	if (immed)
		scmd->cdb.g5_cdb.res |= 8;
/*		scmd->cdb.cmd_cdb[1] |= 0x10;*/

	usalp->cmdname = "blank unit";

	return (usal_cmd(usalp));
}

int
scsi_format(SCSI *usalp, caddr_t addr, int size, BOOL background)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	int progress=0, ret=-1, pid=-1;
	unsigned char sense_table[18];
	int i;
	
	printf("scsi_format: preparing\n");

	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
	scmd->addr = addr;
	scmd->size = size;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->timeout = 160 * 60;     /* Do not know what to set */
	scmd->cdb.g5_cdb.cmd = 0x04;   /* Format Unit */
	scmd->cdb.cmd_cdb[1] = 0x11;  /* "FmtData" and "Format Code" */
	scmd->cdb.cmd_cdb[5] = 0;

	usalp->cmdname = "format unit";

	printf("scsi_format: running\n");
	ret = (usal_cmd(usalp));
	printf("scsi_format: post processing %d\n", ret);
	if (ret == -1) return ret;
	if (background) {
		if ((pid=fork()) == (pid_t)-1)
			perror ("- [unable to fork()]");
		else {
			if (!pid) {
			    while (1) {
				if (test_unit_ready(usalp) >= 0)
				    break;
				sleep(1);
			    }
			    return ret;
			}
		}
	}
	printf("Formating in progress: 0.00 %% done.");
	sleep(20);
	i = 0;
	while (progress < 0xfff0 && !(progress == 0 && i > 50)) {
		test_unit_ready(usalp);
		request_sense_b(usalp, (caddr_t)sense_table, 18);
		progress = sense_table[16]<<8|sense_table[17];
		printf("\rFormating in progress: %.2f %% done [%d].                           ", (float)(progress*100)/0x10000,progress);
		usleep(100000);
		i++;
		/*for (i=0; i < 18; i++) {
		    printf("%d ", sense_table[i]);
		}*/
	}
	sleep(10);
	printf("\rFormating in progress: 100.00 %% done.        \n");
	if (pid) exit (0);
	return ret;
}

/*
 * XXX First try to handle ATAPI:
 * XXX ATAPI cannot handle SCSI 6 byte commands.
 * XXX We try to simulate 6 byte mode sense/select.
 */
static BOOL	is_atapi;

BOOL
allow_atapi(SCSI *usalp, BOOL new)
{
	BOOL	old = is_atapi;
	Uchar	mode[256];

	if (new == old)
		return (old);

	usalp->silent++;
	/*
	 * If a bad drive has been reset before, we may need to fire up two
	 * test unit ready commands to clear status.
	 */
	(void) unit_ready(usalp);
	if (new &&
	    mode_sense_g1(usalp, mode, 8, 0x3F, 0) < 0) {	/* All pages current */
		new = FALSE;
	}
	usalp->silent--;

	is_atapi = new;
	return (old);
}

int
mode_select(SCSI *usalp, Uchar *dp, int cnt, int smp, int pf)
{
	if (is_atapi)
		return (mode_select_sg0(usalp, dp, cnt, smp, pf));
	return (mode_select_g0(usalp, dp, cnt, smp, pf));
}

int
mode_sense(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf)
{
	if (is_atapi)
		return (mode_sense_sg0(usalp, dp, cnt, page, pcf));
	return (mode_sense_g0(usalp, dp, cnt, page, pcf));
}

/*
 * Simulate mode select g0 with mode select g1.
 */
int
mode_select_sg0(SCSI *usalp, Uchar *dp, int cnt, int smp, int pf)
{
	Uchar	xmode[256+4];
	int	amt = cnt;

	if (amt < 1 || amt > 255) {
		/* XXX clear SCSI error codes ??? */
		return (-1);
	}

	if (amt < 4) {		/* Data length. medium type & VU */
		amt += 1;
	} else {
		amt += 4;
		movebytes(&dp[4], &xmode[8], cnt-4);
	}
	xmode[0] = 0;
	xmode[1] = 0;
	xmode[2] = dp[1];
	xmode[3] = dp[2];
	xmode[4] = 0;
	xmode[5] = 0;
	i_to_2_byte(&xmode[6], (unsigned int)dp[3]);

	if (usalp->verbose) usal_prbytes("Mode Parameters (un-converted)", dp, cnt);

	return (mode_select_g1(usalp, xmode, amt, smp, pf));
}

/*
 * Simulate mode sense g0 with mode sense g1.
 */
int
mode_sense_sg0(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf)
{
	Uchar	xmode[256+4];
	int	amt = cnt;
	int	len;

	if (amt < 1 || amt > 255) {
		/* XXX clear SCSI error codes ??? */
		return (-1);
	}

	fillbytes((caddr_t)xmode, sizeof (xmode), '\0');
	if (amt < 4) {		/* Data length. medium type & VU */
		amt += 1;
	} else {
		amt += 4;
	}
	if (mode_sense_g1(usalp, xmode, amt, page, pcf) < 0)
		return (-1);

	amt = cnt - usal_getresid(usalp);
/*
 * For tests: Solaris 8 & LG CD-ROM always returns resid == amt
 */
/*	amt = cnt;*/
	if (amt > 4)
		movebytes(&xmode[8], &dp[4], amt-4);
	len = a_to_u_2_byte(xmode);
	if (len == 0) {
		dp[0] = 0;
	} else if (len < 6) {
		if (len > 2)
			len = 2;
		dp[0] = len;
	} else {
		dp[0] = len - 3;
	}
	dp[1] = xmode[2];
	dp[2] = xmode[3];
	len = a_to_u_2_byte(&xmode[6]);
	dp[3] = len;

	if (usalp->verbose) usal_prbytes("Mode Sense Data (converted)", dp, amt);
	return (0);
}

int
mode_select_g0(SCSI *usalp, Uchar *dp, int cnt, int smp, int pf)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)dp;
	scmd->size = cnt;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_MODE_SELECT;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.high_addr = smp ? 1 : 0 | pf ? 0x10 : 0;
	scmd->cdb.g0_cdb.count = cnt;

	if (usalp->verbose) {
		fprintf(stderr, "%s ", smp?"Save":"Set ");
		usal_prbytes("Mode Parameters", dp, cnt);
	}

	usalp->cmdname = "mode select g0";

	return (usal_cmd(usalp));
}

int
mode_select_g1(SCSI *usalp, Uchar *dp, int cnt, int smp, int pf)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)dp;
	scmd->size = cnt;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x55;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.high_addr = smp ? 1 : 0 | pf ? 0x10 : 0;
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	if (usalp->verbose) {
		printf("%s ", smp?"Save":"Set ");
		usal_prbytes("Mode Parameters", dp, cnt);
	}

	usalp->cmdname = "mode select g1";

	return (usal_cmd(usalp));
}

int
mode_sense_g0(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)dp;
	scmd->size = 0xFF;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_MODE_SENSE;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
#ifdef	nonono
	scmd->cdb.g0_cdb.high_addr = 1<<4;	/* DBD Disable Block desc. */
#endif
	scmd->cdb.g0_cdb.mid_addr = (page&0x3F) | ((pcf<<6)&0xC0);
	scmd->cdb.g0_cdb.count = page ? 0xFF : 24;
	scmd->cdb.g0_cdb.count = cnt;

	usalp->cmdname = "mode sense g0";

	if (usal_cmd(usalp) < 0)
		return (-1);
	if (usalp->verbose) usal_prbytes("Mode Sense Data", dp, cnt - usal_getresid(usalp));
	return (0);
}

int
mode_sense_g1(SCSI *usalp, Uchar *dp, int cnt, int page, int pcf)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)dp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x5A;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
#ifdef	nonono
	scmd->cdb.g0_cdb.high_addr = 1<<4;	/* DBD Disable Block desc. */
#endif
	scmd->cdb.g1_cdb.addr[0] = (page&0x3F) | ((pcf<<6)&0xC0);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "mode sense g1";

	if (usal_cmd(usalp) < 0)
		return (-1);
	if (usalp->verbose) usal_prbytes("Mode Sense Data", dp, cnt - usal_getresid(usalp));
	return (0);
}

struct trackdesc {
	Uchar	res0;

#if defined(_BIT_FIELDS_LTOH)		/* Intel byteorder */
	Ucbit	control		: 4;
	Ucbit	adr		: 4;
#else					/* Motorola byteorder */
	Ucbit	adr		: 4;
	Ucbit	control		: 4;
#endif

	Uchar	track;
	Uchar	res3;
	Uchar	addr[4];
};

struct diskinfo {
	struct tocheader	hd;
	struct trackdesc	desc[1];
};

struct siheader {
	Uchar	len[2];
	Uchar	finished;
	Uchar	unfinished;
};

struct sidesc {
	Uchar	sess_number;
	Uchar	res1;
	Uchar	track;
	Uchar	res3;
	Uchar	addr[4];
};

struct sinfo {
	struct siheader	hd;
	struct sidesc	desc[1];
};

struct trackheader {
	Uchar	mode;
	Uchar	res[3];
	Uchar	addr[4];
};
#define	TRM_ZERO	0
#define	TRM_USER_ECC	1	/* 2048 bytes user data + 288 Bytes ECC/EDC */
#define	TRM_USER	2	/* All user data (2336 bytes) */


int
read_tochdr(SCSI *usalp, cdr_t *dp, int *fp, int *lp)
{
	struct	tocheader *tp;
	char	xb[256];
	int	len;

	tp = (struct tocheader *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc(usalp, xb, 0, sizeof (struct tocheader), 0, FMT_TOC) < 0) {
		if (usalp->silent == 0)
			errmsgno(EX_BAD, "Cannot read TOC header\n");
		return (-1);
	}
	len = a_to_u_2_byte(tp->len) + sizeof (struct tocheader)-2;
	if (len >= 4) {
		if (fp)
			*fp = tp->first;
		if (lp)
			*lp = tp->last;
		return (0);
	}
	return (-1);
}

int
read_cdtext(SCSI *usalp)
{
	struct	tocheader *tp;
	char	xb[256];
	int	len;
	char	xxb[10000];

	tp = (struct tocheader *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc(usalp, xb, 0, sizeof (struct tocheader), 0, FMT_CDTEXT) < 0) {
		if (usalp->silent == 0 || usalp->verbose > 0)
			errmsgno(EX_BAD, "Cannot read CD-Text header\n");
		return (-1);
	}
	len = a_to_u_2_byte(tp->len) + sizeof (struct tocheader)-2;
	printf("CD-Text len: %d\n", len);

	if (read_toc(usalp, xxb, 0, len, 0, FMT_CDTEXT) < 0) {
		if (usalp->silent == 0)
			errmsgno(EX_BAD, "Cannot read CD-Text\n");
		return (-1);
	}
	{
		FILE	*f = fileopen("cdtext.dat", "wctb");
		filewrite(f, xxb, len);
	}
	return (0);
}

int
read_trackinfo(SCSI *usalp, int track, long *offp, struct msf *msfp, int *adrp, 
					int *controlp, int *modep)
{
	struct	diskinfo *dp;
	char	xb[256];
	int	len;

	dp = (struct diskinfo *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc(usalp, xb, track, sizeof (struct diskinfo), 0, FMT_TOC) < 0) {
		if (usalp->silent <= 0)
			errmsgno(EX_BAD, "Cannot read TOC\n");
		return (-1);
	}
	len = a_to_u_2_byte(dp->hd.len) + sizeof (struct tocheader)-2;
	if (len <  (int)sizeof (struct diskinfo))
		return (-1);

	if (offp)
		*offp = a_to_4_byte(dp->desc[0].addr);
	if (adrp)
		*adrp = dp->desc[0].adr;
	if (controlp)
		*controlp = dp->desc[0].control;

	if (msfp) {
		usalp->silent++;
		if (read_toc(usalp, xb, track, sizeof (struct diskinfo), 1, FMT_TOC)
									>= 0) {
			msfp->msf_min = dp->desc[0].addr[1];
			msfp->msf_sec = dp->desc[0].addr[2];
			msfp->msf_frame = dp->desc[0].addr[3];
		} else if (read_toc(usalp, xb, track, sizeof (struct diskinfo), 0, FMT_TOC)
									>= 0) {
			/*
			 * Some drives (e.g. the Philips CDD-522) don't support
			 * to read the TOC in MSF mode.
			 */
			long off = a_to_4_byte(dp->desc[0].addr);

			lba_to_msf(off, msfp);
		} else {
			msfp->msf_min = 0;
			msfp->msf_sec = 0;
			msfp->msf_frame = 0;
		}
		usalp->silent--;
	}

	if (modep == NULL)
		return (0);

	if (track == 0xAA) {
		*modep = -1;
		return (0);
	}

	fillbytes((caddr_t)xb, sizeof (xb), '\0');

	usalp->silent++;
	if (read_header(usalp, xb, *offp, 8, 0) >= 0) {
		*modep = xb[0];
	} else if (read_track_info_philips(usalp, xb, track, 14) >= 0) {
		*modep = xb[0xb] & 0xF;
	} else {
		*modep = -1;
	}
	usalp->silent--;
	return (0);
}

int
read_B0(SCSI *usalp, BOOL isbcd, long *b0p, long *lop)
{
	struct	fdiskinfo *dp;
	struct	ftrackdesc *tp;
	char	xb[8192];
	char	*pe;
	int	len;
	long	l;

	dp = (struct fdiskinfo *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc_philips(usalp, xb, 1, sizeof (struct tocheader), 0, FMT_FULLTOC) < 0) {
		return (-1);
	}
	len = a_to_u_2_byte(dp->hd.len) + sizeof (struct tocheader)-2;
	if (len <  (int)sizeof (struct fdiskinfo))
		return (-1);
	if (read_toc_philips(usalp, xb, 1, len, 0, FMT_FULLTOC) < 0) {
		return (-1);
	}
	if (usalp->verbose) {
		usal_prbytes("TOC data: ", (Uchar *)xb,
			len > (int)sizeof (xb) - usal_getresid(usalp) ?
				sizeof (xb) - usal_getresid(usalp) : len);

		tp = &dp->desc[0];
		pe = &xb[len];

		while ((char *)tp < pe) {
			usal_prbytes("ENT: ", (Uchar *)tp, 11);
			tp++;
		}
	}
	tp = &dp->desc[0];
	pe = &xb[len];

	for (; (char *)tp < pe; tp++) {
		if (tp->sess_number != dp->hd.last)
			continue;
		if (tp->point != 0xB0)
			continue;
		if (usalp->verbose)
			usal_prbytes("B0: ", (Uchar *)tp, 11);
		if (isbcd) {
			l = msf_to_lba(from_bcd(tp->amin),
				from_bcd(tp->asec),
				from_bcd(tp->aframe), TRUE);
		} else {
			l = msf_to_lba(tp->amin,
				tp->asec,
				tp->aframe, TRUE);
		}
		if (b0p)
			*b0p = l;

		if (usalp->verbose)
			printf("B0 start: %ld\n", l);

		if (isbcd) {
			l = msf_to_lba(from_bcd(tp->pmin),
				from_bcd(tp->psec),
				from_bcd(tp->pframe), TRUE);
		} else {
			l = msf_to_lba(tp->pmin,
				tp->psec,
				tp->pframe, TRUE);
		}

		if (usalp->verbose)
			printf("B0 lout: %ld\n", l);
		if (lop)
			*lop = l;
		return (0);
	}
	return (-1);
}


/*
 * Return address of first track in last session (SCSI-3/mmc version).
 */
int
read_session_offset(SCSI *usalp, long *offp)
{
	struct	diskinfo *dp;
	char	xb[256];
	int	len;

	dp = (struct diskinfo *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc(usalp, (caddr_t)xb, 0, sizeof (struct tocheader), 0, FMT_SINFO) < 0)
		return (-1);

	if (usalp->verbose)
		usal_prbytes("tocheader: ",
		(Uchar *)xb, sizeof (struct tocheader) - usal_getresid(usalp));

	len = a_to_u_2_byte(dp->hd.len) + sizeof (struct tocheader)-2;
	if (len > (int)sizeof (xb)) {
		errmsgno(EX_BAD, "Session info too big.\n");
		return (-1);
	}
	if (read_toc(usalp, (caddr_t)xb, 0, len, 0, FMT_SINFO) < 0)
		return (-1);

	if (usalp->verbose)
		usal_prbytes("tocheader: ",
			(Uchar *)xb, len - usal_getresid(usalp));

	dp = (struct diskinfo *)xb;
	if (offp)
		*offp = a_to_u_4_byte(dp->desc[0].addr);
	return (0);
}

/*
 * Return address of first track in last session (pre SCSI-3 version).
 */
int
read_session_offset_philips(SCSI *usalp, long *offp)
{
	struct	sinfo *sp;
	char	xb[256];
	int	len;

	sp = (struct sinfo *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc_philips(usalp, (caddr_t)xb, 0, sizeof (struct siheader), 0, FMT_SINFO) < 0)
		return (-1);
	len = a_to_u_2_byte(sp->hd.len) + sizeof (struct siheader)-2;
	if (len > (int)sizeof (xb)) {
		errmsgno(EX_BAD, "Session info too big.\n");
		return (-1);
	}
	if (read_toc_philips(usalp, (caddr_t)xb, 0, len, 0, FMT_SINFO) < 0)
		return (-1);
	/*
	 * Old drives return the number of finished sessions in first/finished
	 * a descriptor is returned for each session.
	 * New drives return the number of the first and last session
	 * one descriptor for the last finished session is returned
	 * as in SCSI-3
	 * In all cases the lowest session number is set to 1.
	 */
	sp = (struct sinfo *)xb;
	if (offp)
		*offp = a_to_u_4_byte(sp->desc[sp->hd.finished-1].addr);
	return (0);
}

int
sense_secsize(SCSI *usalp, int current)
{
	Uchar	mode[0x100];
	Uchar	*p;
	Uchar	*ep;
	int	len;
	int	secsize = -1;

	usalp->silent++;
	(void) unit_ready(usalp);
	usalp->silent--;

	/* XXX Quick and dirty, musz verallgemeinert werden !!! */

	fillbytes(mode, sizeof (mode), '\0');
	usalp->silent++;

	len =	sizeof (struct scsi_mode_header) +
		sizeof (struct scsi_mode_blockdesc);
	/*
	 * Wenn wir hier get_mode_params() nehmen bekommen wir die Warnung:
	 * Warning: controller returns wrong page 1 for All pages page (3F).
	 */
	if (mode_sense(usalp, mode, len, 0x3F, current?0:2) < 0) {
		fillbytes(mode, sizeof (mode), '\0');
		if (mode_sense(usalp, mode, len, 0, current?0:2) < 0)	{ /* VU (block desc) */
			usalp->silent--;
			return (-1);
		}
	}
	if (mode[3] == 8) {
		if (usalp->debug) {
			printf("Density: 0x%X\n", mode[4]);
			printf("Blocks:  %ld\n", a_to_u_3_byte(&mode[5]));
			printf("Blocklen:%ld\n", a_to_u_3_byte(&mode[9]));
		}
		secsize = a_to_u_3_byte(&mode[9]);
	}
	fillbytes(mode, sizeof (mode), '\0');
	/*
	 * The ACARD TECH AEC-7720 ATAPI<->SCSI adaptor
	 * chokes if we try to transfer more than 0x40 bytes with
	 * mode_sense of all pages. So try to avoid to run this
	 * command if possible.
	 */
	if (usalp->debug &&
	    mode_sense(usalp, mode, 0xFE, 0x3F, current?0:2) >= 0) {	/* All Pages */

		ep = mode+mode[0];	/* Points to last byte of data */
		p = &mode[4];
		p += mode[3];
		printf("Pages: ");
		while (p < ep) {
			printf("0x%X ", *p&0x3F);
			p += p[1]+2;
		}
		printf("\n");
	}
	usalp->silent--;

	return (secsize);
}

int
select_secsize(SCSI *usalp, int secsize)
{
	struct scsi_mode_data md;
	int	count = sizeof (struct scsi_mode_header) +
			sizeof (struct scsi_mode_blockdesc);

	(void) test_unit_ready(usalp);	/* clear any error situation */

	fillbytes((caddr_t)&md, sizeof (md), '\0');
	md.header.blockdesc_len = 8;
	i_to_3_byte(md.blockdesc.lblen, secsize);

	return (mode_select(usalp, (Uchar *)&md, count, 0, usalp->inq->data_format >= 2));
}

BOOL
is_cddrive(SCSI *usalp)
{
	return (usalp->inq->type == INQ_ROMD || usalp->inq->type == INQ_WORM);
}

BOOL
is_unknown_dev(SCSI *usalp)
{
	return (usalp->dev == DEV_UNKNOWN);
}

#ifndef	DEBUG
#define	DEBUG
#endif
#ifdef	DEBUG

int
read_scsi(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	if (addr <= G0_MAXADDR && cnt < 256 && !is_atapi)
		return (read_g0(usalp, bp, addr, cnt));
	else
		return (read_g1(usalp, bp, addr, cnt));
}

int
read_g0(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (usalp->cap->c_bsize <= 0)
		raisecond("capacity_not_set", 0L);

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*usalp->cap->c_bsize;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_READ;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	g0_cdbaddr(&scmd->cdb.g0_cdb, addr);
	scmd->cdb.g0_cdb.count = cnt;
/*	scmd->cdb.g0_cdb.vu_56 = 1;*/

	usalp->cmdname = "read_g0";

	return (usal_cmd(usalp));
}

int
read_g1(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (usalp->cap->c_bsize <= 0)
		raisecond("capacity_not_set", 0L);

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*usalp->cap->c_bsize;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = SC_EREAD;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read_g1";

	return (usal_cmd(usalp));
}
#endif	/* DEBUG */

BOOL
getdev(SCSI *usalp, BOOL print)
{
		BOOL	got_inquiry = TRUE;
		char	vendor_info[8+1];
		char	prod_ident[16+1];
		char	prod_revision[4+1];
		int	inq_len = 0;
	register struct	usal_cmd	*scmd = usalp->scmd;
	register struct scsi_inquiry *inq = usalp->inq;


	fillbytes((caddr_t)inq, sizeof (*inq), '\0');
	usalp->dev = DEV_UNKNOWN;
	usalp->silent++;
	(void) unit_ready(usalp);
	if (scmd->error >= SCG_FATAL &&
				!(scmd->scb.chk && scmd->sense_count > 0)) {
		usalp->silent--;
		return (FALSE);
	}


/*	if (scmd->error < SCG_FATAL || scmd->scb.chk && scmd->sense_count > 0){*/

	if (inquiry(usalp, (caddr_t)inq, sizeof (*inq)) < 0) {
		got_inquiry = FALSE;
	} else {
		inq_len = sizeof (*inq) - usal_getresid(usalp);
	}
	if (!got_inquiry) {
		if (usalp->verbose) {
			printf(
		"error: %d scb.chk: %d sense_count: %d sense.code: 0x%x\n",
				scmd->error, scmd->scb.chk,
				scmd->sense_count, scmd->sense.code);
		}
			/*
			 * Folgende Kontroller kennen das Kommando
			 * INQUIRY nicht:
			 *
			 * ADAPTEC	ACB-4000, ACB-4010, ACB 4070
			 * SYSGEN	SC4000
			 *
			 * Leider reagieren ACB40X0 und ACB5500 identisch
			 * wenn drive not ready (code == not ready),
			 * sie sind dann nicht zu unterscheiden.
			 */

		if (scmd->scb.chk && scmd->sense_count == 4) {
			/* Test auf SYSGEN				 */
			(void) qic02(usalp, 0x12);	/* soft lock on  */
			if (qic02(usalp, 1) < 0) {	/* soft lock off */
				usalp->dev = DEV_ACB40X0;
/*				usalp->dev = acbdev();*/
			} else {
				usalp->dev = DEV_SC4000;
				inq->type = INQ_SEQD;
				inq->removable = 1;
			}
		}
	} else if (usalp->verbose) {
		int	i;
		int	len = inq->add_len + 5;
		Uchar	ibuf[256+5];
		Uchar	*ip = (Uchar *)inq;
		Uchar	c;

		if (len > (int)sizeof (*inq) &&
				inquiry(usalp, (caddr_t)ibuf, inq->add_len+5) >= 0) {
			len = inq->add_len+5 - usal_getresid(usalp);
			ip = ibuf;
		} else {
			len = sizeof (*inq);
		}
		printf("Inquiry Data   : ");
		for (i = 0; i < len; i++) {
			c = ip[i];
			if (c >= ' ' && c < 0177)
				printf("%c", c);
			else
				printf(".");
		}
		printf("\n");
	}

	strncpy(vendor_info, inq->vendor_info, sizeof (inq->vendor_info));
	strncpy(prod_ident, inq->prod_ident, sizeof (inq->prod_ident));
	strncpy(prod_revision, inq->prod_revision, sizeof (inq->prod_revision));

	vendor_info[sizeof (inq->vendor_info)] = '\0';
	prod_ident[sizeof (inq->prod_ident)] = '\0';
	prod_revision[sizeof (inq->prod_revision)] = '\0';

	switch (inq->type) {

	case INQ_DASD:
		if (inq->add_len == 0 && inq->vendor_info[0] != '\0') {
			Uchar	*p;
			/*
			 * NT-4.0 creates fake inquiry data for IDE disks.
			 * Unfortunately, it does not set add_len wo we
			 * check if vendor_info, prod_ident and prod_revision
			 * contains valid chars for a CCS inquiry.
			 */
			if (inq_len >= 36)
				inq->add_len = 31;

			for (p = (Uchar *)&inq->vendor_info[0];
					p < (Uchar *)&inq->prod_revision[4];
									p++) {
				if (*p < 0x20 || *p > 0x7E) {
					inq->add_len = 0;
					break;
				}
			}
		}
		if (inq->add_len == 0) {
			if (usalp->dev == DEV_UNKNOWN && got_inquiry) {
				usalp->dev = DEV_ACB5500;
				strcpy(inq->vendor_info,
					"ADAPTEC ACB-5500        FAKE");

			} else switch (usalp->dev) {

				case DEV_ACB40X0:
					strcpy(inq->vendor_info,
							"ADAPTEC ACB-40X0        FAKE");
					break;
				case DEV_ACB4000:
					strcpy(inq->vendor_info,
							"ADAPTEC ACB-4000        FAKE");
					break;
				case DEV_ACB4010:
					strcpy(inq->vendor_info,
							"ADAPTEC ACB-4010        FAKE");
					break;
				case DEV_ACB4070:
					strcpy(inq->vendor_info,
							"ADAPTEC ACB-4070        FAKE");
					break;
			}
		} else if (inq->add_len < 31) {
			usalp->dev = DEV_NON_CCS_DSK;

		} else if (strbeg("EMULEX", vendor_info)) {
			if (strbeg("MD21", prod_ident))
				usalp->dev = DEV_MD21;
			if (strbeg("MD23", prod_ident))
				usalp->dev = DEV_MD23;
			else
				usalp->dev = DEV_CCS_GENDISK;
		} else if (strbeg("ADAPTEC", vendor_info)) {
			if (strbeg("ACB-4520", prod_ident))
				usalp->dev = DEV_ACB4520A;
			if (strbeg("ACB-4525", prod_ident))
				usalp->dev = DEV_ACB4525;
			else
				usalp->dev = DEV_CCS_GENDISK;
		} else if (strbeg("SONY", vendor_info) &&
					strbeg("SMO-C501", prod_ident)) {
			usalp->dev = DEV_SONY_SMO;
		} else {
			usalp->dev = DEV_CCS_GENDISK;
		}
		break;

	case INQ_SEQD:
		if (usalp->dev == DEV_SC4000) {
			strcpy(inq->vendor_info,
				"SYSGEN  SC4000          FAKE");
		} else if (inq->add_len == 0 &&
					inq->removable &&
						inq->ansi_version == 1) {
			usalp->dev = DEV_MT02;
			strcpy(inq->vendor_info,
				"EMULEX  MT02            FAKE");
		}
		break;

/*	case INQ_OPTD:*/
	case INQ_ROMD:
	case INQ_WORM:
		if (strbeg("RXT-800S", prod_ident))
			usalp->dev = DEV_RXT800S;

		/*
		 * Start of CD-Recorders:
		 */
		if (strbeg("ACER", vendor_info)) {
			if (strbeg("CR-4020C", prod_ident))
				usalp->dev = DEV_RICOH_RO_1420C;

		} else if (strbeg("CREATIVE", vendor_info)) {
			if (strbeg("CDR2000", prod_ident))
				usalp->dev = DEV_RICOH_RO_1060C;

		} else if (strbeg("GRUNDIG", vendor_info)) {
			if (strbeg("CDR100IPW", prod_ident))
				usalp->dev = DEV_CDD_2000;

		} else if (strbeg("JVC", vendor_info)) {
			if (strbeg("XR-W2001", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			else if (strbeg("XR-W2010", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			else if (strbeg("R2626", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;

		} else if (strbeg("MITSBISH", vendor_info)) {

#ifdef	XXXX_REALLY
			/* It's MMC compliant */
			if (strbeg("CDRW226", prod_ident))
				usalp->dev = DEV_MMC_CDRW;
#else
			/* EMPTY */
#endif

		} else if (strbeg("MITSUMI", vendor_info)) {
			/* Don't know any product string */
			usalp->dev = DEV_CDD_522;

		} else if (strbeg("OPTIMA", vendor_info)) {
			if (strbeg("CD-R 650", prod_ident))
				usalp->dev = DEV_SONY_CDU_924;

		} else if (strbeg("PHILIPS", vendor_info) ||
				strbeg("IMS", vendor_info) ||
				strbeg("KODAK", vendor_info) ||
				strbeg("HP", vendor_info)) {

			if (strbeg("CDD521/00", prod_ident))
				usalp->dev = DEV_CDD_521_OLD;
			else if (strbeg("CDD521/02", prod_ident))
				usalp->dev = DEV_CDD_521_OLD;		/* PCD 200R? */
			else if (strbeg("CDD521", prod_ident))
				usalp->dev = DEV_CDD_521;

			if (strbeg("CDD522", prod_ident))
				usalp->dev = DEV_CDD_522;
			if (strbeg("PCD225", prod_ident))
				usalp->dev = DEV_CDD_522;
			if (strbeg("KHSW/OB", prod_ident))	/* PCD600 */
				usalp->dev = DEV_PCD_600;
			if (strbeg("CDR-240", prod_ident))
				usalp->dev = DEV_CDD_2000;

			if (strbeg("CDD20", prod_ident))
				usalp->dev = DEV_CDD_2000;
			if (strbeg("CDD26", prod_ident))
				usalp->dev = DEV_CDD_2600;

			if (strbeg("C4324/C4325", prod_ident))
				usalp->dev = DEV_CDD_2000;
			if (strbeg("CD-Writer 6020", prod_ident))
				usalp->dev = DEV_CDD_2600;

		} else if (strbeg("PINNACLE", vendor_info)) {
			if (strbeg("RCD-1000", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			if (strbeg("RCD5020", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			if (strbeg("RCD5040", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			if (strbeg("RCD 4X4", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;

		} else if (strbeg("PIONEER", vendor_info)) {
			if (strbeg("CD-WO DW-S114X", prod_ident))
				usalp->dev = DEV_PIONEER_DW_S114X;
			else if (strbeg("CD-WO DR-R504X", prod_ident))	/* Reoprt from philip@merge.com */
				usalp->dev = DEV_PIONEER_DW_S114X;
			else if (strbeg("DVD-R DVR-S101", prod_ident))
				usalp->dev = DEV_PIONEER_DVDR_S101;

		} else if (strbeg("PLASMON", vendor_info)) {
			if (strbeg("RF4100", prod_ident))
				usalp->dev = DEV_PLASMON_RF_4100;
			else if (strbeg("CDR4220", prod_ident))
				usalp->dev = DEV_CDD_2000;

		} else if (strbeg("PLEXTOR", vendor_info)) {
			if (strbeg("CD-R   PX-R24CS", prod_ident))
				usalp->dev = DEV_RICOH_RO_1420C;

		} else if (strbeg("RICOH", vendor_info)) {
			if (strbeg("RO-1420C", prod_ident))
				usalp->dev = DEV_RICOH_RO_1420C;
			if (strbeg("RO1060C", prod_ident))
				usalp->dev = DEV_RICOH_RO_1060C;

		} else if (strbeg("SAF", vendor_info)) {	/* Smart & Friendly */
			if (strbeg("CD-R2004", prod_ident) ||
			    strbeg("CD-R2006 ", prod_ident))
				usalp->dev = DEV_SONY_CDU_924;
			else if (strbeg("CD-R2006PLUS", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			else if (strbeg("CD-RW226", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;
			else if (strbeg("CD-R4012", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;

		} else if (strbeg("SANYO", vendor_info)) {
			if (strbeg("CD-WO CRD-R24S", prod_ident))
				usalp->dev = DEV_CDD_521;

		} else if (strbeg("SONY", vendor_info)) {
			if (strbeg("CD-R   CDU92", prod_ident) ||
			    strbeg("CD-R   CDU94", prod_ident))
				usalp->dev = DEV_SONY_CDU_924;

		} else if (strbeg("TEAC", vendor_info)) {
			if (strbeg("CD-R50S", prod_ident) ||
			    strbeg("CD-R55S", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;

		} else if (strbeg("TRAXDATA", vendor_info) ||
				strbeg("Traxdata", vendor_info)) {
			if (strbeg("CDR4120", prod_ident))
				usalp->dev = DEV_TEAC_CD_R50S;

		} else if (strbeg("T.YUDEN", vendor_info)) {
			if (strbeg("CD-WO EW-50", prod_ident))
				usalp->dev = DEV_TYUDEN_EW50;

		} else if (strbeg("WPI", vendor_info)) {	/* Wearnes */
			if (strbeg("CDR-632P", prod_ident))
				usalp->dev = DEV_CDD_2600;

		} else if (strbeg("YAMAHA", vendor_info)) {
			if (strbeg("CDR10", prod_ident))
				usalp->dev = DEV_YAMAHA_CDR_100;
			if (strbeg("CDR200", prod_ident))
				usalp->dev = DEV_YAMAHA_CDR_400;
			if (strbeg("CDR400", prod_ident))
				usalp->dev = DEV_YAMAHA_CDR_400;

		} else if (strbeg("MATSHITA", vendor_info)) {
			if (strbeg("CD-R   CW-7501", prod_ident))
				usalp->dev = DEV_MATSUSHITA_7501;
			if (strbeg("CD-R   CW-7502", prod_ident))
				usalp->dev = DEV_MATSUSHITA_7502;
		}
		if (usalp->dev == DEV_UNKNOWN) {
			/*
			 * We do not have Manufacturer strings for
			 * the following drives.
			 */
			if (strbeg("CDS615E", prod_ident))	/* Olympus */
				usalp->dev = DEV_SONY_CDU_924;
		}
		if (usalp->dev == DEV_UNKNOWN && inq->type == INQ_ROMD) {
			BOOL	cdrr	 = FALSE;
			BOOL	cdwr	 = FALSE;
			BOOL	cdrrw	 = FALSE;
			BOOL	cdwrw	 = FALSE;
			BOOL	dvd	 = FALSE;
			BOOL	dvdwr	 = FALSE;

			usalp->dev = DEV_CDROM;

			if (mmc_check(usalp, &cdrr, &cdwr, &cdrrw, &cdwrw,
								&dvd, &dvdwr))
				usalp->dev = DEV_MMC_CDROM;
			if (cdwr)
				usalp->dev = DEV_MMC_CDR;
			if (cdwrw)
				usalp->dev = DEV_MMC_CDRW;
			if (dvd)
				usalp->dev = DEV_MMC_DVD;
			if (dvdwr)
				usalp->dev = DEV_MMC_DVD_WR;
		}
		break;

	case INQ_PROCD:
		if (strbeg("BERTHOLD", vendor_info)) {
			if (strbeg("", prod_ident))
				usalp->dev = DEV_HRSCAN;
		}
		break;

	case INQ_SCAN:
		usalp->dev = DEV_MS300A;
		break;
	}
	usalp->silent--;
	if (!print)
		return (TRUE);

	if (usalp->dev == DEV_UNKNOWN && !got_inquiry) {
#ifdef	PRINT_INQ_ERR
		usal_printerr(usalp);
#endif
		return (FALSE);
	}

	printinq(usalp, stdout);
	return (TRUE);
}

void
printinq(SCSI *usalp, FILE *f)
{
	register struct scsi_inquiry *inq = usalp->inq;

	fprintf(f, "Device type    : ");
	usal_fprintdev(f, inq);
	fprintf(f, "Version        : %d\n", inq->ansi_version);
	fprintf(f, "Response Format: %d\n", inq->data_format);
	if (inq->data_format >= 2) {
		fprintf(f, "Capabilities   : ");
		if (inq->aenc)		fprintf(f, "AENC ");
		if (inq->termiop)	fprintf(f, "TERMIOP ");
		if (inq->reladr)	fprintf(f, "RELADR ");
		if (inq->wbus32)	fprintf(f, "WBUS32 ");
		if (inq->wbus16)	fprintf(f, "WBUS16 ");
		if (inq->sync)		fprintf(f, "SYNC ");
		if (inq->linked)	fprintf(f, "LINKED ");
		if (inq->cmdque)	fprintf(f, "CMDQUE ");
		if (inq->softreset)	fprintf(f, "SOFTRESET ");
		fprintf(f, "\n");
	}
	if (inq->add_len >= 31 ||
			inq->vendor_info[0] ||
			inq->prod_ident[0] ||
			inq->prod_revision[0]) {
		fprintf(f, "Vendor_info    : '%.8s'\n", inq->vendor_info);
		fprintf(f, "Identification : '%.16s'\n", inq->prod_ident);
		fprintf(f, "Revision       : '%.4s'\n", inq->prod_revision);
	}
}

void
printdev(SCSI *usalp)
{
	printf("Device seems to be: ");

	switch (usalp->dev) {

	case DEV_UNKNOWN:	printf("unknown");		break;
	case DEV_ACB40X0:	printf("Adaptec 4000/4010/4070"); break;
	case DEV_ACB4000:	printf("Adaptec 4000");		break;
	case DEV_ACB4010:	printf("Adaptec 4010");		break;
	case DEV_ACB4070:	printf("Adaptec 4070");		break;
	case DEV_ACB5500:	printf("Adaptec 5500");		break;
	case DEV_ACB4520A:	printf("Adaptec 4520A");	break;
	case DEV_ACB4525:	printf("Adaptec 4525");		break;
	case DEV_MD21:		printf("Emulex MD21");		break;
	case DEV_MD23:		printf("Emulex MD23");		break;
	case DEV_NON_CCS_DSK:	printf("Generic NON CCS Disk");	break;
	case DEV_CCS_GENDISK:	printf("Generic CCS Disk");	break;
	case DEV_SONY_SMO:	printf("Sony SMO-C501");	break;
	case DEV_MT02:		printf("Emulex MT02");		break;
	case DEV_SC4000:	printf("Sysgen SC4000");	break;
	case DEV_RXT800S:	printf("Maxtor RXT800S");	break;
	case DEV_HRSCAN:	printf("Berthold HR-Scanner");	break;
	case DEV_MS300A:	printf("Microtek MS300A");	break;

	case DEV_CDROM:		printf("Generic CD-ROM");	break;
	case DEV_MMC_CDROM:	printf("Generic mmc CD-ROM");	break;
	case DEV_MMC_CDR:	printf("Generic mmc CD-R");	break;
	case DEV_MMC_CDRW:	printf("Generic mmc CD-RW");	break;
	case DEV_MMC_DVD:	printf("Generic mmc2 DVD-ROM");	break;
	case DEV_MMC_DVD_WR:	printf("Generic mmc2 DVD-R/DVD-RW"); break;
	case DEV_CDD_521_OLD:	printf("Philips old CDD-521");	break;
	case DEV_CDD_521:	printf("Philips CDD-521");	break;
	case DEV_CDD_522:	printf("Philips CDD-522");	break;
	case DEV_PCD_600:	printf("Kodak PCD-600");	break;
	case DEV_CDD_2000:	printf("Philips CDD-2000");	break;
	case DEV_CDD_2600:	printf("Philips CDD-2600");	break;
	case DEV_YAMAHA_CDR_100:printf("Yamaha CDR-100");	break;
	case DEV_YAMAHA_CDR_400:printf("Yamaha CDR-400");	break;
	case DEV_PLASMON_RF_4100:printf("Plasmon RF-4100");	break;
	case DEV_SONY_CDU_924:	printf("Sony CDU-924S");	break;
	case DEV_RICOH_RO_1060C:printf("Ricoh RO-1060C");	break;
	case DEV_RICOH_RO_1420C:printf("Ricoh RO-1420C");	break;
	case DEV_TEAC_CD_R50S:	printf("Teac CD-R50S");		break;
	case DEV_MATSUSHITA_7501:printf("Matsushita CW-7501");	break;
	case DEV_MATSUSHITA_7502:printf("Matsushita CW-7502");	break;

	case DEV_PIONEER_DW_S114X: printf("Pioneer DW-S114X");	break;
	case DEV_PIONEER_DVDR_S101:printf("Pioneer DVD-R S101"); break;

	default:		printf("Missing Entry for dev %d",
						usalp->dev);	break;

	}
	printf(".\n");

}

BOOL
do_inquiry(SCSI *usalp, int print)
{
	if (getdev(usalp, print)) {
		if (print)
			printdev(usalp);
		return (TRUE);
	} else {
		return (FALSE);
	}
}

BOOL
recovery_needed(SCSI *usalp, cdr_t *dp)
{
		int err;
	register struct	usal_cmd	*scmd = usalp->scmd;

	usalp->silent++;
	err = test_unit_ready(usalp);
	usalp->silent--;

	if (err >= 0)
		return (FALSE);
	else if (scmd->error >= SCG_FATAL)	/* nicht selektierbar */
		return (FALSE);

	if (scmd->sense.code < 0x70)		/* non extended Sense */
		return (FALSE);

						/* XXX Old Philips code */
	return (((struct scsi_ext_sense *)&scmd->sense)->sense_code == 0xD0);
}

int
scsi_load(SCSI *usalp, cdr_t *dp)
{
	int	key;
	int	code;

	if ((dp->cdr_flags & CDR_CADDYLOAD) == 0) {
		if (scsi_start_stop_unit(usalp, 1, 1, dp && (dp->cdr_cmdflags&F_IMMED)) >= 0)
			return (0);
	}

	if (wait_unit_ready(usalp, 60))
		return (0);

	key = usal_sense_key(usalp);
	code = usal_sense_code(usalp);

	if (key == SC_NOT_READY && (code == 0x3A || code == 0x30)) {
		errmsgno(EX_BAD, "Cannot load media with %s drive!\n",
			(dp->cdr_flags & CDR_CADDYLOAD) ? "caddy" : "this");
		errmsgno(EX_BAD, "Try to load media by hand.\n");
	}
	return (-1);
}

int
scsi_unload(SCSI *usalp, cdr_t *dp)
{
	return (scsi_start_stop_unit(usalp, 0, 1, dp && (dp->cdr_cmdflags&F_IMMED)));
}

int 
scsi_cdr_write(SCSI *usalp, 
               caddr_t bp       /* address of buffer */, 
               long sectaddr    /* disk address (sector) to put */, 
               long size        /* number of bytes to transfer */, 
               int blocks       /* sector count */, 
               BOOL islast      /* last write for track */)
{
	return (write_xg1(usalp, bp, sectaddr, size, blocks));
}

struct cd_mode_page_2A *
mmc_cap(SCSI *usalp, Uchar *modep)
{
	int	len;
	int	val;
	Uchar	mode[0x100];
	struct	cd_mode_page_2A *mp;
	struct	cd_mode_page_2A *mp2;


retry:
	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	if (!get_mode_params(usalp, 0x2A, "CD capabilities",
			mode, (Uchar *)0, (Uchar *)0, (Uchar *)0, &len)) {

		if (usal_sense_key(usalp) == SC_NOT_READY) {
			if (wait_unit_ready(usalp, 60))
				goto retry;
		}
		return (NULL);		/* Pre SCSI-3/mmc drive		*/
	}

	if (len == 0)			/* Pre SCSI-3/mmc drive		*/
		return (NULL);

	mp = (struct cd_mode_page_2A *)
		(mode + sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len);

	/*
	 * Do some heuristics against pre SCSI-3/mmc VU page 2A
	 * We should test for a minimum p_len of 0x14, but some
	 * buggy CD-ROM readers ommit the write speed values.
	 */
	if (mp->p_len < 0x10)
		return (NULL);

	val = a_to_u_2_byte(mp->max_read_speed);
	if (val != 0 && val < 176)
		return (NULL);

	val = a_to_u_2_byte(mp->cur_read_speed);
	if (val != 0 && val < 176)
		return (NULL);

	len -= sizeof (struct scsi_mode_header) +
		((struct scsi_mode_header *)mode)->blockdesc_len;
	if (modep)
		mp2 = (struct cd_mode_page_2A *)modep;
	else
		mp2 = malloc(len);
	if (mp2)
		movebytes(mp, mp2, len);

	return (mp2);
}

void
mmc_getval(struct cd_mode_page_2A *mp, 
           BOOL *cdrrp  /* CD ROM */, 
           BOOL *cdwrp  /* CD-R writer */, 
           BOOL *cdrrwp /* CD-RW reader */, 
           BOOL *cdwrwp /* CD-RW writer */, 
           BOOL *dvdp   /* DVD reader */, 
           BOOL *dvdwp  /* DVD writer */)
{
	BOOL	isdvd;				/* Any DVD reader	*/
	BOOL	isdvd_wr;			/* DVD writer (R / RAM)	*/
	BOOL	iscd_wr;			/* CD  writer		*/

	iscd_wr = (mp->cd_r_write != 0) ||	/* SCSI-3/mmc CD-R	*/
		    (mp->cd_rw_write != 0);	/* SCSI-3/mmc CD-RW	*/

	if (cdrrp)
		*cdrrp = (mp->cd_r_read != 0);	/* SCSI-3/mmc CD	*/
	if (cdwrp)
		*cdwrp = (mp->cd_r_write != 0);	/* SCSI-3/mmc CD-R	*/

	if (cdrrwp)
		*cdrrwp = (mp->cd_rw_read != 0); /* SCSI-3/mmc CD	*/
	if (cdwrwp)
		*cdwrwp = (mp->cd_rw_write != 0); /* SCSI-3/mmc CD-RW	*/

	isdvd =					/* SCSI-3/mmc2 DVD 	*/
		(mp->dvd_ram_read + mp->dvd_r_read  +
		    mp->dvd_rom_read) != 0;

	isdvd_wr =				/* SCSI-3/mmc2 DVD writer*/
		(mp->dvd_ram_write + mp->dvd_r_write) != 0;

	if (dvdp)
		*dvdp = isdvd;
	if (dvdwp)
		*dvdwp = isdvd_wr;
}

BOOL
is_mmc(SCSI *usalp, BOOL *cdwp, BOOL *dvdwp)
{
	BOOL	cdwr	= FALSE;
	BOOL	cdwrw	= FALSE;

	if (cdwp)
		*cdwp = FALSE;
	if (dvdwp)
		*dvdwp = FALSE;

	if (!mmc_check(usalp, NULL, &cdwr, NULL, &cdwrw, NULL, dvdwp))
		return (FALSE);

	if (cdwp)
		*cdwp = cdwr | cdwrw;

	return (TRUE);
}

BOOL
mmc_check(SCSI *usalp, 
          BOOL *cdrrp   /* CD ROM */, 
          BOOL *cdwrp   /* CD-R writer */, 
          BOOL *cdrrwp  /* CD-RW reader */, 
          BOOL *cdwrwp  /* CD-RW writer */, 
          BOOL *dvdp    /* DVD reader */, 
          BOOL *dvdwp   /* DVD writer */)
{
	Uchar	mode[0x100];
	BOOL	was_atapi;
	struct	cd_mode_page_2A *mp;

	if (usalp->inq->type != INQ_ROMD)
		return (FALSE);

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	was_atapi = allow_atapi(usalp, TRUE);
	usalp->silent++;
	mp = mmc_cap(usalp, mode);
	usalp->silent--;
	allow_atapi(usalp, was_atapi);
	if (mp == NULL)
		return (FALSE);

	mmc_getval(mp, cdrrp, cdwrp, cdrrwp, cdwrwp, dvdp, dvdwp);

	return (TRUE);			/* Generic SCSI-3/mmc CD	*/
}

static void
print_speed(char *fmt, int val)
{
	printf("  %s: %5d kB/s", fmt, val);
	printf(" (CD %3ux,", val/176);
	printf(" DVD %2ux)\n", val/1385);
}

#define	DOES(what, flag)	printf("  Does %s%s\n", flag?"":"not ", what)
#define	IS(what, flag)		printf("  Is %s%s\n", flag?"":"not ", what)
#define	VAL(what, val)		printf("  %s: %d\n", what, val[0]*256 + val[1])
#define	SVAL(what, val)		printf("  %s: %s\n", what, val)

void
print_capabilities(SCSI *usalp)
{
	BOOL	was_atapi;
	Uchar	mode[0x100];
	struct	cd_mode_page_2A *mp;
static	const	char	*bclk[4] = {"32", "16", "24", "24 (I2S)"};
static	const	char	*load[8] = {"caddy", "tray", "pop-up", "reserved(3)",
				"disc changer", "cartridge changer",
				"reserved(6)", "reserved(7)" };
static	const	char	*rotctl[4] = {"CLV/PCAV", "CAV", "reserved(2)", "reserved(3)"};


	if (usalp->inq->type != INQ_ROMD)
		return;

	fillbytes((caddr_t)mode, sizeof (mode), '\0');

	was_atapi = allow_atapi(usalp, TRUE);	/* Try to switch to 10 byte mode cmds */
	usalp->silent++;
	mp = mmc_cap(usalp, mode);
	usalp->silent--;
	allow_atapi(usalp, was_atapi);
	if (mp == NULL)
		return;

	printf("\nDrive capabilities, per");
	if (mp->p_len >= 28)
		printf(" MMC-3");
	else if (mp->p_len >= 24)
		printf(" MMC-2");
	else
		printf(" MMC");
	printf(" page 2A:\n\n");

	DOES("read CD-R media", mp->cd_r_read);
	DOES("write CD-R media", mp->cd_r_write);
	DOES("read CD-RW media", mp->cd_rw_read);
	DOES("write CD-RW media", mp->cd_rw_write);
	DOES("read DVD-ROM media", mp->dvd_rom_read);
	DOES("read DVD-R media", mp->dvd_r_read);
	DOES("write DVD-R media", mp->dvd_r_write);
	DOES("read DVD-RAM media", mp->dvd_ram_read);
	DOES("write DVD-RAM media", mp->dvd_ram_write);
	DOES("support test writing", mp->test_write);
	printf("\n");
	DOES("read Mode 2 Form 1 blocks", mp->mode_2_form_1);
	DOES("read Mode 2 Form 2 blocks", mp->mode_2_form_2);
	DOES("read digital audio blocks", mp->cd_da_supported);
	if (mp->cd_da_supported)
		DOES("restart non-streamed digital audio reads accurately", mp->cd_da_accurate);
	DOES("support Buffer-Underrun-Free recording", mp->BUF);
	DOES("read multi-session CDs", mp->multi_session);
	DOES("read fixed-packet CD media using Method 2", mp->method2);
	DOES("read CD bar code", mp->read_bar_code);
	DOES("read R-W subcode information", mp->rw_supported);
	if (mp->rw_supported)
		DOES("return R-W subcode de-interleaved and error-corrected", mp->rw_deint_corr);
	DOES("read raw P-W subcode data from lead in", mp->pw_in_lead_in);
	DOES("return CD media catalog number", mp->UPC);
	DOES("return CD ISRC information", mp->ISRC);
	DOES("support C2 error pointers", mp->c2_pointers);
	DOES("deliver composite A/V data", mp->composite);
	printf("\n");
	DOES("play audio CDs", mp->audio_play);
	if (mp->audio_play) {
		VAL("Number of volume control levels", mp->num_vol_levels);
		DOES("support individual volume control setting for each channel", mp->sep_chan_vol);
		DOES("support independent mute setting for each channel", mp->sep_chan_mute);
		DOES("support digital output on port 1", mp->digital_port_1);
		DOES("support digital output on port 2", mp->digital_port_2);
		if (mp->digital_port_1 || mp->digital_port_2) {
			DOES("send digital data LSB-first", mp->LSBF);
			DOES("set LRCK high for left-channel data", mp->RCK);
			DOES("have valid data on falling edge of clock", mp->BCK);
			SVAL("Length of data in BCLKs", bclk[mp->length]);
		}
	}
	printf("\n");
	SVAL("Loading mechanism type", load[mp->loading_type]);
	DOES("support ejection of CD via START/STOP command", mp->eject);
	DOES("lock media on power up via prevent jumper", mp->prevent_jumper);
	DOES("allow media to be locked in the drive via PREVENT/ALLOW command", mp->lock);
	IS("currently in a media-locked state", mp->lock_state);
	DOES("support changing side of disk", mp->side_change);
	DOES("have load-empty-slot-in-changer feature", mp->sw_slot_sel);
	DOES("support Individual Disk Present feature", mp->disk_present_rep);
	printf("\n");
	print_speed("Maximum read  speed", a_to_u_2_byte(mp->max_read_speed));
	print_speed("Current read  speed", a_to_u_2_byte(mp->cur_read_speed));
	print_speed("Maximum write speed", a_to_u_2_byte(mp->max_write_speed));
	if (mp->p_len >= 28)
		print_speed("Current write speed", a_to_u_2_byte(mp->v3_cur_write_speed));
	else
		print_speed("Current write speed", a_to_u_2_byte(mp->cur_write_speed));
	if (mp->p_len >= 28) {
		SVAL("Rotational control selected", rotctl[mp->rot_ctl_sel]);
	}
	VAL("Buffer size in KB", mp->buffer_size);

	if (mp->p_len >= 24) {
		VAL("Copy management revision supported", mp->copy_man_rev);
	}

	if (mp->p_len >= 28) {
		struct cd_wr_speed_performance *pp;
		Uint	ndesc;
		Uint	i;
		Uint	n;

		ndesc = a_to_u_2_byte(mp->num_wr_speed_des);
		pp = mp->wr_speed_des;
		printf("  Number of supported write speeds: %d\n", ndesc);
		for (i = 0; i < ndesc; i++, pp++) {
			printf("  Write speed # %d:", i);
			n = a_to_u_2_byte(pp->wr_speed_supp);
			printf(" %5d kB/s", n);
			printf(" %s", rotctl[pp->rot_ctl_sel]);
			printf(" (CD %3ux,", n/176);
			printf(" DVD %2ux)\n", n/1385);
		}
	}

	/* Generic SCSI-3/mmc CD	*/
}
