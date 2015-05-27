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

/* @(#)strdefs.h	1.8 03/03/09 Copyright 1996 J. Schilling */
/*
 *	Definitions for strings
 *
 *	Copyright (c) 1996 J. Schilling
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

#ifndef _STRDEFS_H
#define	_STRDEFS_H

#ifndef	_MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _PROTOTYP_H
#include <prototyp.h>
#endif

/*
 * It may be that IBM's AIX has problems when doing
 * #include <string.h>
 * #include <strings.h>
 * So I moved the #include <strings.h> to the top. As the file strings.h
 * defines strcasecmp() we may need it...
 *
 * Note that the only real problem appears if we use rubbish FSF based code that
 * #defines _NO_PROTO
 */
#ifdef	HAVE_STRINGS_H
#ifndef	_INCL_STRINGS_H
#include <strings.h>
#define	_INCL_STRINGS_H
#endif
#endif	/* HAVE_STRINGS_H */


#ifdef	HAVE_STRING_H
#ifndef	_INCL_STRING_H
#include <string.h>
#define	_INCL_STRING_H
#endif
#else	/* HAVE_STRING_H */

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef NULL
#define	NULL	0
#endif

extern void *memcpy(void *, const void *, int);
extern void *memmove(void *, const void *, int);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, int);

extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, int);

extern int memcmp(const void *, const void *, int);
extern int strcmp(const char *, const char *);
extern int strcoll(const char *, const char *);
extern int strncmp(const char *, const char *, int);
extern int strxfrm(char *, const char *, int);

extern void *memchr(const void *, int, int);
extern char *strchr(const char *, int);

extern int strcspn(const char *, const char *);
/*#pragma int_to_unsigned strcspn*/

extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);

extern int strspn(const char *, const char *);
/*#pragma int_to_unsigned strspn*/

extern char *strstr(const char *, const char *);
extern char *strtok(char *, const char *);
extern void *memset(void *, int, int);
extern char *strerror(int);

extern int strlen(const char *);
/*#pragma int_to_unsigned strlen*/

extern void *memccpy(void *, const void *, int, int);

extern int strcasecmp(const char *, const char *);
extern int strncasecmp(const char *, const char *, int);

/*#define	index	strchr*/
/*#define	rindex	strrchr*/

#ifdef	__cplusplus
}
#endif

#endif	/* HAVE_STRING_H */

#endif	/* _STRDEFS_H */
