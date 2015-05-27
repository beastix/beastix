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

/* @(#)drv_simul.c	1.48 05/05/16 Copyright 1998-2005 J. Schilling */
/*
 *	Simulation device driver
 *
 *	Copyright (c) 1998-2005 J. Schilling
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

#ifndef	DEBUG
#define	DEBUG
#endif
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <errno.h>
#include <strdefs.h>
#include <timedefs.h>
#include <utypes.h>
#include <btorder.h>
#include <schily.h>

/*#include <usalio.h>*/
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include <libport.h>

#include "wodim.h"

extern	int	silent;
extern	int	verbose;
extern	int	lverbose;

static	int	simul_load(SCSI *usalp, cdr_t *);
static	int	simul_unload(SCSI *usalp, cdr_t *);
static	cdr_t	*identify_simul(SCSI *usalp, cdr_t *, struct scsi_inquiry *);
static	int	init_simul(SCSI *usalp, cdr_t *dp);
static	int	getdisktype_simul(SCSI *usalp, cdr_t *dp);
static	int	speed_select_simul(SCSI *usalp, cdr_t *dp, int *speedp);
static	int	next_wr_addr_simul(SCSI *usalp, track_t *trackp, long *ap);
static	int	cdr_write_simul(SCSI *usalp, caddr_t bp, long sectaddr, long size, 
										 int blocks, BOOL islast);
static	int	open_track_simul(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	close_track_simul(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	open_session_simul(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	int	fixate_simul(SCSI *usalp, cdr_t *dp, track_t *trackp);
static	void	tv_sub(struct timeval *tvp1, struct timeval *tvp2);

static int simul_load(SCSI *usalp, cdr_t *dp)
{
	return (0);
}

static int simul_unload(SCSI *usalp, cdr_t *dp)
{
	return (0);
}

cdr_t	cdr_cdr_simul = {
	0, 0,
	CDR_TAO|CDR_SAO|CDR_PACKET|CDR_RAW|CDR_RAW16|CDR_RAW96P|CDR_RAW96R|CDR_SRAW96P|CDR_SRAW96R|CDR_TRAYLOAD|CDR_SIMUL,
	CDR_CDRW_ALL,
	40, 372,
	"cdr_simul",
	"simulation CD-R driver for timing/speed tests",
	0,
	(dstat_t *)0,
	identify_simul,
	drive_attach,
	init_simul,
	getdisktype_simul,
	simul_load,
	simul_unload,
	buf_dummy,
	cmd_dummy,					/* recovery_needed */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_simul,
	select_secsize,
	next_wr_addr_simul,
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	cdr_write_simul,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy, /* send_cue */
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_simul,
	close_track_simul,
	open_session_simul,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	fixate_simul,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

cdr_t	cdr_dvd_simul = {
	0, 0,
	CDR_TAO|CDR_SAO|CDR_PACKET|CDR_RAW|CDR_RAW16|CDR_RAW96P|CDR_RAW96R|CDR_SRAW96P|CDR_SRAW96R|CDR_DVD|CDR_TRAYLOAD|CDR_SIMUL,
	CDR_CDRW_ALL,
	2, 1000,
	"dvd_simul",
	"simulation DVD-R driver for timing/speed tests",
	0,
	(dstat_t *)0,
	identify_simul,
	drive_attach,
	init_simul,
	getdisktype_simul,
	simul_load,
	simul_unload,
	buf_dummy,
	cmd_dummy,					/* recovery_needed */
	(int(*)(SCSI *, cdr_t *, int))cmd_dummy,	/* recover	*/
	speed_select_simul,
	select_secsize,
	next_wr_addr_simul,
	(int(*)(SCSI *, Ulong))cmd_ill,	/* reserve_track	*/
	cdr_write_simul,
	(int(*)(track_t *, void *, BOOL))cmd_dummy,	/* gen_cue */
	(int(*)(SCSI *usalp, cdr_t *, track_t *))cmd_dummy, /* send_cue */
	(int(*)(SCSI *, cdr_t *, track_t *))cmd_dummy, /* leadin */
	open_track_simul,
	close_track_simul,
	open_session_simul,
	cmd_dummy,
	cmd_dummy,					/* abort	*/
	read_session_offset,
	fixate_simul,
	cmd_dummy,					/* stats	*/
	blank_dummy,
	format_dummy,
	(int(*)(SCSI *, caddr_t, int, int))NULL,	/* no OPC	*/
	cmd_dummy,					/* opt1		*/
	cmd_dummy,					/* opt2		*/
};

static cdr_t *
identify_simul(SCSI *usalp, cdr_t *dp, struct scsi_inquiry *ip)
{
	return (dp);
}

static	long	simul_nwa;
static	int	simul_speed = 1;
static	int	simul_dummy;
static	int	simul_isdvd;
static	int	simul_bufsize = 1024;
static	Uint	sleep_rest;
static	Uint	sleep_max;
static	Uint	sleep_min;

static int
init_simul(SCSI *usalp, cdr_t *dp)
{
	return (speed_select_simul(usalp, dp, NULL));
}

static int
getdisktype_simul(SCSI *usalp, cdr_t *dp)
{
	dstat_t	*dsp = dp->cdr_dstat;

	if (strcmp(dp->cdr_drname, cdr_cdr_simul.cdr_drname) == 0) {
		dsp->ds_maxblocks = 333000;
		simul_isdvd = FALSE;
	} else {
		dsp->ds_maxblocks = 2464153;	/* 4.7 GB  */
/*		dsp->ds_maxblocks = 1927896;*/	/* 3.95 GB */
		dsp->ds_flags |= DSF_DVD;
		simul_isdvd = TRUE;
	}
	return (drive_getdisktype(usalp, dp));
}


static int
speed_select_simul(SCSI *usalp, cdr_t *dp, int *speedp)
{
	long	val;
	char	*p;
	BOOL	dummy = (dp->cdr_cmdflags & F_DUMMY) != 0;

	if (speedp)
		simul_speed = *speedp;
	simul_dummy = dummy;

	if ((p = getenv("CDR_SIMUL_BUFSIZE")) != NULL) {
		if (getnum(p, &val) == 1)
			simul_bufsize = val / 1024;
	}

	/*
	 * sleep_max is the time to empty the drive's buffer in µs.
	 * sector size is from 2048 bytes to 2352 bytes.
	 * If sector size is 2048 bytes, 1k takes 6.666 ms.
	 * If sector size is 2352 bytes, 1k takes 5.805 ms.
	 * We take the 6 ms as an average between both values.
	 * simul_bufsize is the number of kilobytes in drive buffer.
	 */
	sleep_max = 6 * 1000 * simul_bufsize / simul_speed;

	/*
	 * DVD single speed is 1385 * 1000 Bytes/s (676.27 sectors/s)
	 */
	if ((dp->cdr_flags & CDR_DVD) != 0)
		sleep_max = 739 * simul_bufsize / simul_speed;

	if (lverbose) {
		printf("Simulation drive buffer size: %d KB\n", simul_bufsize);
		printf("Maximum reserve time in drive buffer: %d.%3.3d ms for speed %dx\n",
					sleep_max / 1000,
					sleep_max % 1000,
					simul_speed);
	}
	return (0);
}

static int
next_wr_addr_simul(SCSI *usalp, track_t *trackp, long *ap)
{
	/*
	 * This will most likely not 100% correct for TAO CDs
	 * but it is better than returning 0 in all cases.
	 */
	if (ap)
		*ap = simul_nwa;
	return (0);
}


static int
cdr_write_simul(SCSI *usalp, caddr_t bp /* address of buffer */, 
                long sectaddr   /* disk address (sector) to put */, 
                long size       /* number of bytes to transfer */, 
                int blocks      /* sector count */, 
                BOOL islast     /* last write for track */)
{
	Uint	sleep_time;
	Uint	sleep_diff;

	struct timeval	tv1;
    static	struct timeval	tv2;

	if (lverbose > 1 && islast)
		printf("\nWriting last record for this track.\n");

	simul_nwa += blocks;

	gettimeofday(&tv1, (struct timezone *)0);
	if (tv2.tv_sec != 0) {		/* Already did gettimeofday(&tv2) */
		tv_sub(&tv1, &tv2);
		if (sleep_rest != 0) {
			sleep_diff = tv1.tv_sec * 1000000 + tv1.tv_usec;

			if (sleep_min > (sleep_rest - sleep_diff))
				sleep_min = (sleep_rest - sleep_diff);

			if (sleep_diff > sleep_rest) {
				printf("Buffer underrun: actual delay was %d.%3.3d ms, max delay was %d.%3.3d ms.\n",
						sleep_diff / 1000,
						sleep_diff % 1000,
						sleep_rest / 1000,
						sleep_rest % 1000);
				if (!simul_dummy)
					return (-1);
			}
			/*
			 * If we spent time outside the write function
			 * subtract this time.
			 */
			sleep_diff = tv1.tv_sec * 1000000 + tv1.tv_usec;
			if (sleep_rest >= sleep_diff)
				sleep_rest -= sleep_diff;
			else
				sleep_rest = 0;
		}
	}
	/*
	 * Speed 1 ist 150 Sektoren/s
	 * Bei DVD 767.27 Sektoren/s
	 */
	sleep_time = 1000000 * blocks / 75 / simul_speed;
	if (simul_isdvd)
		sleep_time = 1000000 * blocks / 676 / simul_speed;

	sleep_time += sleep_rest;

	if (sleep_time > sleep_max) {
		int	mod;
		long	rsleep;

		sleep_rest = sleep_max;
		sleep_time -= sleep_rest;
		mod = sleep_time % 20000;
		sleep_rest += mod;
		sleep_time -= mod;
		if (sleep_time > 0) {
			gettimeofday(&tv1, (struct timezone *)0);
			usleep(sleep_time);
			gettimeofday(&tv2, (struct timezone *)0);
			tv2.tv_sec -= tv1.tv_sec;
			tv2.tv_usec -= tv1.tv_usec;
			rsleep = tv2.tv_sec * 1000000 + tv2.tv_usec;
			sleep_rest -= rsleep - sleep_time;
		}
	} else {
		sleep_rest = sleep_time;
	}

	gettimeofday(&tv2, (struct timezone *)0);
	return (size);
}

static int
open_track_simul(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	sleep_min = 999 * 1000000;
	return (0);
}

static int
close_track_simul(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	if (lverbose) {
		printf("Remaining reserve time in drive buffer: %d.%3.3d ms\n",
					sleep_rest / 1000,
					sleep_rest % 1000);
		printf("Minimum reserve time in drive buffer: %d.%3.3d ms\n",
					sleep_min / 1000,
					sleep_min % 1000);
	}
	usleep(sleep_rest);
	sleep_rest = 0;
	return (0);
}

static int
open_session_simul(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	simul_nwa = 0L;
	return (0);
}

static int
fixate_simul(SCSI *usalp, cdr_t *dp, track_t *trackp)
{
	return (0);
}

static void
tv_sub(struct timeval *tvp1, struct timeval *tvp2)
{
	tvp1->tv_sec -= tvp2->tv_sec;
	tvp1->tv_usec -= tvp2->tv_usec;

	while (tvp1->tv_usec < 0) {
		tvp1->tv_usec += 1000000;
		tvp1->tv_sec -= 1;
	}
}
