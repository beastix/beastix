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

/* @(#)nls_iconv.c	1.0 02/04/20 2002 J. Schilling  */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 *	Modifications to make the code portable Copyright (c) 2000 J. Schilling
 *
 * nls_iconv: create a pseudo-charset table to use iconv() provided by C
 * library or libiconv by Bruno Haible
 * The Unicode to charset table has only exact mappings.
 *
 *
 * Jungshik Shin (jshin@mailaps.org) 04-Feb-2002
 */

#ifdef USE_ICONV
#include <mconfig.h>
#include <stdio.h>
#include <stdxlib.h>
#include <strdefs.h>
#include "nls.h"
#include <iconv.h>

static void	inc_use_count(void);
static void	dec_use_count(void);


static void
inc_use_count()
{
	MOD_INC_USE_COUNT;
}

static void
dec_use_count()
{
	MOD_DEC_USE_COUNT;
}

int
init_nls_iconv(char *charset)
{
	iconv_t iconv_d;  /* iconv conversion descriptor */
	struct unls_table *table;

	/* give up if no charset is given */
	if (charset == NULL)
		return -1;

	/* see if we already have a table with this name - built in tables
	   have precedence over iconv() - i.e. can't have the name of an
	   existing table. Also, we may have already registered this file
	   table */
	if (find_unls(charset) != NULL)
		return -1;

	if ((iconv_d = iconv_open("UCS-2BE", charset)) == (iconv_t) -1)
		return -1;


	/* set up the table */
	if ((table = (struct unls_table *)malloc(sizeof (struct unls_table)))
							== NULL) {
		return -1;
	}

	/* give the table the file name, so we can find it again if needed */
	table->unls_name = strdup(charset);
	table->unls_uni2cs = NULL;
	table->unls_cs2uni = NULL;
	table->unls_next = NULL;
	table->iconv_d = iconv_d;

	/* register the table */
	return register_unls(table);
}
#endif
