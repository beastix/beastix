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

/* @(#)gethostname.c	1.15 03/10/04 Copyright 1995 J. Schilling */
/*
 *	Copyright (c) 1995 J. Schilling
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
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>

#ifndef	HAVE_GETHOSTNAME
EXPORT	int	gethostname	__PR((char *name, int namelen));


#ifdef	SI_HOSTNAME

EXPORT int
gethostname(name, namelen)
	char	*name;
	int	namelen;
{
	if (sysinfo(SI_HOSTNAME, name, namelen) < 0)
		return (-1);
	return (0);
}
#else

#if	defined(HAVE_UNAME) && defined(HAVE_SYS_UTSNAME_H)
#include <sys/utsname.h>
#include <strdefs.h>

EXPORT int
gethostname(name, namelen)
	char	*name;
	int	namelen;
{
	struct utsname	uts;

	if (uname(&uts) < 0)
		return (-1);

	strncpy(name, uts.nodename, namelen);
	return (0);
}
#endif

#endif

#endif	/* HAVE_GETHOSTNAME */
