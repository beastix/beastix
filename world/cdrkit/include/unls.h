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

/* @(#)unls.h	1.6 05/04/21 Copyright 2000-2005 J. Schilling */
/*
 *	Definitions fur users of libunls
 *
 *	Copyright (c) 2000-2005 J. Schilling
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

#ifndef _UNLS_H
#define	_UNLS_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _PROTOTYP_H
#include <prototyp.h>
#endif

#ifdef USE_ICONV
#include <iconv.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

struct unls_unicode {
	unsigned char	unls_low;		/* Low Byte */
	unsigned char	unls_high;		/* High Byte */
};

struct unls_table {
	char		*unls_name;		/* UNLS charset name	*/
	unsigned char 	**unls_uni2cs;		/* Unicode -> Charset	*/
	struct unls_unicode *unls_cs2uni;	/* Charset -> Unicode	*/
	struct unls_table *unls_next;		/* Next table		*/
#ifdef USE_ICONV
    iconv_t iconv_d;
#endif
};

extern int		init_unls(void);
extern int		register_unls(struct unls_table *);
extern int		unregister_unls(struct unls_table *);
extern struct unls_table *find_unls(char *);
extern void		list_unls(void);
extern struct unls_table *load_unls(char *);
extern void 		unload_unls(struct unls_table *);
extern struct unls_table *load_unls_default(void);
extern int		init_unls_file(char * name);

#ifdef USE_ICONV
extern int             init_nls_iconv(char * name);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _UNLS_H */
