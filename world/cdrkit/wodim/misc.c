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

/* @(#)misc.c	1.4 04/03/02 Copyright 1998, 2001-2004 J. Schilling */
/*
 *	Misc support functions
 *
 *	Copyright (c) 1998, 2001-2004 J. Schilling
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
#include <timedefs.h>
#include <stdio.h>
#include <standard.h>
#include <schily.h>

void	timevaldiff(struct timeval *start, struct timeval *stop);
void	prtimediff(const char *fmt, struct timeval *start, struct timeval *stop);

void
timevaldiff(struct timeval *start, struct timeval *stop)
{
	struct timeval tv;

	tv.tv_sec = stop->tv_sec - start->tv_sec;
	tv.tv_usec = stop->tv_usec - start->tv_usec;
	while (tv.tv_usec > 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	while (tv.tv_usec < 0) {
		tv.tv_usec += 1000000;
		tv.tv_sec -= 1;
	}
	*stop = tv;
}

void
prtimediff(const char *fmt, struct timeval *start, struct timeval *stop)
{
	struct timeval tv;

	tv.tv_sec = stop->tv_sec - start->tv_sec;
	tv.tv_usec = stop->tv_usec - start->tv_usec;
	while (tv.tv_usec > 1000000) {
		tv.tv_usec -= 1000000;
		tv.tv_sec += 1;
	}
	while (tv.tv_usec < 0) {
		tv.tv_usec += 1000000;
		tv.tv_sec -= 1;
	}
	/*
	 * We need to cast timeval->* to long because
	 * of the broken sys/time.h in Linux.
	 */
	printf("%s%4ld.%03lds\n", fmt, (long)tv.tv_sec, (long)tv.tv_usec/1000);
	flush();
}
