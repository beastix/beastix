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

/* @(#)usleep.c	1.17 03/06/15 Copyright 1995-2003 J. Schilling */
/*
 *	Copyright (c) 1995-2003 J. Schilling
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
#define	usleep	__nothing_    /* prototype in unistd.h may be different */
#include <standard.h>
#include <stdxlib.h>
#include <timedefs.h>
#ifdef	HAVE_POLL_H
#	include <poll.h>
#else
#	ifdef	HAVE_SYS_POLL_H
#	include <sys/poll.h>
#	endif
#endif
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>
#undef	usleep

#ifndef	HAVE_USLEEP
EXPORT	int	usleep		__PR((int usec));
#endif

#ifdef	OPENSERVER
/*
 * Don't use the usleep() from libc on SCO's OPENSERVER.
 * It will kill our processes with SIGALRM.
 * SCO has a usleep() prototype in unistd.h, for this reason we
 * #define usleep to __nothing__ before including unistd.h
 */
#undef	HAVE_USLEEP
#endif

#ifdef apollo
/*
 * Apollo sys5.3 usleep is broken.  Define a version based on time_$wait.
 */
#include <apollo/base.h>
#include <apollo/time.h>
#undef HAVE_USLEEP
#endif

#if	!defined(HAVE_USLEEP)

EXPORT int
usleep(usec)
	int	usec;
{
#if defined(apollo)
	/*
	 * Need to check apollo before HAVE_SELECT, because Apollo has select,
	 * but it's time wait feature is also broken :-(
	 */
#define	HAVE_USLEEP
	/*
	 * XXX Do these vars need to be static on Domain/OS ???
	 */
	static time_$clock_t	DomainDelay;
	static status_$t	DomainStatus;

	/*
	 * DomainDelay is a 48 bit value that defines how many 4uS periods to
	 * delay.  Since the input value range is 32 bits, the upper 16 bits of
	 * DomainDelay must be zero.  So we just divide the input value by 4 to
	 * determine how many "ticks" to wait
	 */
	DomainDelay.c2.high16 = 0;
	DomainDelay.c2.low32 = usec / 4;
	time_$wait(time_$relative, DomainDelay, &DomainStatus);
#endif	/* Apollo */

#if	defined(HAVE_SELECT) && !defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	struct timeval tv;
	tv.tv_sec = usec / 1000000;
	tv.tv_usec = usec % 1000000;
	select(0, 0, 0, 0, &tv);
#endif

#if	defined(HAVE_POLL) && !defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	if (poll(0, 0, usec/1000) < 0)
		comerr("poll delay failed.\n");

#endif

#if	defined(HAVE_NANOSLEEP) && !defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	struct timespec ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;

	nanosleep(&ts, 0);
#endif


#if	!defined(HAVE_USLEEP)
#define	HAVE_USLEEP

	sleep((usec+500000)/1000000);
#endif

	return (0);
}
#endif
