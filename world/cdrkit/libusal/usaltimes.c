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

/* @(#)usaltimes.c	1.1 00/08/25 Copyright 1995,2000 J. Schilling */
/*
 *	SCSI user level command timing
 *
 *	Copyright (c) 1995,2000 J. Schilling
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
#include <standard.h>
#include <timedefs.h>
#include <schily.h>

#include <usal/scsitransp.h>
#include "usaltimes.h"

void	__usal_times(SCSI *usalp);

/*
 * We don't like to make this a public interface to prevent bad users
 * from making our timing incorrect.
 */
void
__usal_times(SCSI *usalp)
{
	struct timeval	*stp = usalp->cmdstop;

	gettimeofday(stp, (struct timezone *)0);
	stp->tv_sec -= usalp->cmdstart->tv_sec;
	stp->tv_usec -= usalp->cmdstart->tv_usec;
	while (stp->tv_usec < 0) {
		stp->tv_sec -= 1;
		stp->tv_usec += 1000000;
	}
}
