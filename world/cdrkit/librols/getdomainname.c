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

/* @(#)getdomainname.c	1.16 03/06/15 Copyright 1995-2003 J. Schilling */
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
#ifdef	HAVE_SYS_SYSTEMINFO_H
#include <sys/systeminfo.h>
#endif
#include <libport.h>

#ifndef	HAVE_GETDOMAINNAME
EXPORT	int	getdomainname	__PR((char *name, int namelen));
#endif


/*#undef	HAVE_GETDOMAINNAME*/
/*#undef	SI_SRPC_DOMAIN*/

#if	!defined(HAVE_GETDOMAINNAME) && defined(SI_SRPC_DOMAIN)
#define	FUNC_GETDOMAINNAME

EXPORT int
getdomainname(name, namelen)
	char	*name;
	int	namelen;
{
	if (sysinfo(SI_SRPC_DOMAIN, name, namelen) < 0)
		return (-1);
	return (0);
}
#endif

#if	!defined(HAVE_GETDOMAINNAME) && !defined(FUNC_GETDOMAINNAME)
#define	FUNC_GETDOMAINNAME

#include <stdio.h>
#include <strdefs.h>
#include <schily.h>

EXPORT int
getdomainname(name, namelen)
	char	*name;
	int	namelen;
{
	FILE	*f;
	char	name1[1024];
	char	*p;
	char	*p2;

	name[0] = '\0';

	f = fileopen("/etc/resolv.conf", "r");

	if (f == NULL)
		return (-1);

	while (fgetline(f, name1, sizeof (name1)) >= 0) {
		if ((p = strchr(name1, '#')) != NULL)
			*p = '\0';

		/*
		 * Skip leading whitespace.
		 */
		p = name1;
		while (*p != '\0' && (*p == ' ' || *p == '\t'))
			p++;

		if (strncmp(p, "domain", 6) == 0) {
			p += 6;
			while (*p != '\0' && (*p == ' ' || *p == '\t'))
				p++;
			if ((p2 = strchr(p, ' ')) != NULL)
				*p2 = '\0';
			if ((p2 = strchr(p, '\t')) != NULL)
				*p2 = '\0';

			strncpy(name, p, namelen);

			fclose(f);
			return (0);
		}
	}
	fclose(f);
	return (0);
}
#endif
