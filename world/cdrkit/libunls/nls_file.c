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

/* @(#)nls_file.c	1.3 05/05/01 2000 J. Schilling */
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
/*
 *	Modifications to make the code portable Copyright (c) 2000 J. Schilling
 *
 * nls_file: create a charset table from a input table file
 * from the Unicode Organization (www.unicode.org).
 * The Unicode to charset table has only exact mappings.
 *
 * Only reads single byte to word matches.
 *
 * James Pearson (j.pearson@ge.ucl.ac.uk) 16-Aug-2000
 */

#include <mconfig.h>
#include <stdio.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <libport.h>
#include "nls.h"

#define	NUM	256

static void	free_mem(struct unls_unicode *, unsigned char **);

static void
free_mem(struct unls_unicode *charset2uni, unsigned char **page_uni2charset)
{
	int	i;

	if (charset2uni)
		free(charset2uni);

	if (page_uni2charset) {
		for (i = 0; i < NUM; i++) {
			if (page_uni2charset[i]) {
				free(page_uni2charset[i]);
			}
		}
		free(page_uni2charset);
	}
}

int
init_unls_file(char *filename)
{
	FILE	*fp;
	struct unls_unicode *charset2uni = NULL;
	unsigned char **page_uni2charset = NULL;
	char	buf[1024];
	char	*p;
	unsigned int	cp, uc;
	struct unls_table *table;
	int	i, ok = 0;

	/* give up if no filename is given */
	if (filename == NULL)
		return (-1);

	/*
	 * see if we already have a table with this name - built in tables
	 * have precedence of file tables - i.e. can't have the name of an
	 * existing table. Also, we may have already registered this file table
	 */
	if (find_unls(filename) != NULL)
		return (-1);

	if ((fp = fopen(filename, "r")) == NULL)
		return (-1);

	/* allocate memory for the forward conversion table */
	if ((charset2uni = (struct unls_unicode *)
			malloc(sizeof (struct unls_unicode) * NUM)) == NULL) {
		free_mem(charset2uni, page_uni2charset);
		return (-1);
	}

	/* any unknown character should be mapped to NULL */
	memset(charset2uni, 0, sizeof (struct unls_unicode) * NUM);

	/*
	 * some source files don't set the control characters 0x00 - 0x1f
	 * so set these by default
	 */
	for (i = 0; i < 32; i++) {
		charset2uni[i].unls_low = i;
	}

	/* also set DELETE (0x7f) by default */
	charset2uni[0x7f].unls_low = 0x7f;

	/* read each line of the file */
	while (fgets(buf, sizeof (buf), fp) != NULL) {
		/* cut off any comments */
		if ((p = strchr(buf, '#')) != NULL)
			*p = '\0';

		/* look for two hex values */
		if (sscanf(buf, "%x%x", &cp, &uc) == 2) {
			/* if they are not in range - fail */
			if (cp > 0xff || uc > 0xffff) {
				continue;
			}

			/* set the Unicode value for the given code point */
			charset2uni[cp].unls_low  = uc & 0xff;
			charset2uni[cp].unls_high = (uc >> 8) & 0xff;

			/* make sure we find at least one pair ... */
			ok = 1;
		}
	}

	fclose(fp);

	if (!ok) {
		/* we haven't found anything ... */
		free_mem(charset2uni, page_uni2charset);
		return (-1);
	}

	/* allocate memory for the reverse table */
	if ((page_uni2charset = (unsigned char **)
			malloc(sizeof (unsigned char *) * NUM)) == NULL) {
		free_mem(charset2uni, page_uni2charset);
		return (-1);
	}

	memset(page_uni2charset, 0, sizeof (unsigned char *) * NUM);

	/* loop through the forward table, setting the reverse value */
	for (i = 0; i < NUM; i++) {
		uc = charset2uni[i].unls_high;
		cp = charset2uni[i].unls_low;
		/* if the page doesn't exist, create a page */
		if (page_uni2charset[uc] == NULL) {
			if ((page_uni2charset[uc] =
				(unsigned char *) malloc(NUM)) == NULL) {
				free_mem(charset2uni, page_uni2charset);
				return (-1);
			}
			memset(page_uni2charset[uc], 0, NUM);
		}

		/* set the reverse point in the page */
		page_uni2charset[uc][cp] = i;
	}

	/* set up the table */
	if ((table = (struct unls_table *)malloc(sizeof (struct unls_table)))
							== NULL) {
		free_mem(charset2uni, page_uni2charset);
		return (-1);
	}

	/* give the table the file name, so we can find it again if needed */
	table->unls_name = strdup(filename);
	table->unls_uni2cs = page_uni2charset;
	table->unls_cs2uni = charset2uni;
	table->unls_next = NULL;

	/* register the table */
	return (register_unls(table));
}
