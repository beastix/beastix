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

/* @(#)gethostid.c	1.15 03/06/15 Copyright 1995-2003 J. Schilling */
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
#include <standard.h>
#include <stdxlib.h>
#include <utypes.h>
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>

#ifndef	HAVE_GETHOSTID
EXPORT	long	gethostid	__PR((void));
#endif


#if	!defined(HAVE_GETHOSTID)

#if	defined(SI_HW_SERIAL)

EXPORT long
gethostid()
{
	long	id;

	char	hbuf[257];
	sysinfo(SI_HW_SERIAL, hbuf, sizeof (hbuf));
	id = atoi(hbuf);
	return (id);
}
#else

#include <errno.h>
EXPORT long
gethostid()
{
	long	id = -1L;

#ifdef	ENOSYS
	seterrno(ENOSYS);
#else
	seterrno(EINVAL);
#endif
	return (id);
}
#endif

#endif
