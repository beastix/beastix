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

/* @(#)filereopen.c	1.15 04/08/08 Copyright 1986, 1995 J. Schilling */
/*
 *	open new file on old stream
 *
 *	Copyright (c) 1986, 1995 J. Schilling
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

#include "schilyio.h"

/*
 * Note that because of a definition in schilyio.h we are using fseeko()/ftello()
 * instead of fseek()/ftell() if available.
 */

LOCAL	char	*fmtab[] = {
			"",	/* 0	FI_NONE				*/
			"r",	/* 1	FI_READ				*/
			"r+",	/* 2	FI_WRITE		**1)	*/
			"r+",	/* 3	FI_READ  | FI_WRITE		*/
			"b",	/* 4	FI_NONE  | FI_BINARY		*/
			"rb",	/* 5	FI_READ  | FI_BINARY		*/
			"r+b",	/* 6	FI_WRITE | FI_BINARY	**1)	*/
			"r+b",	/* 7	FI_READ  | FI_WRITE | FI_BINARY	*/

/* + FI_APPEND	*/	"",	/* 0	FI_NONE				*/
/* ...		*/	"r",	/* 1	FI_READ				*/
			"a",	/* 2	FI_WRITE		**1)	*/
			"a+",	/* 3	FI_READ  | FI_WRITE		*/
			"b",	/* 4	FI_NONE  | FI_BINARY		*/
			"rb",	/* 5	FI_READ  | FI_BINARY		*/
			"ab",	/* 6	FI_WRITE | FI_BINARY	**1)	*/
			"a+b",	/* 7	FI_READ  | FI_WRITE | FI_BINARY	*/
		};
/*
 * NOTES:
 *	1)	there is no fopen() mode that opens for writing
 *		without creating/truncating at the same time.
 *
 *	"w"	will create/trunc files with fopen()
 *	"a"	will create files with fopen()
 */

EXPORT FILE *
filereopen(name, mode, fp)
	const char	*name;
	const char 	*mode;
	FILE		*fp;
{
	int	ret;
	int	omode = 0;
	int	flag = 0;

	if (!_cvmod(mode, &omode, &flag))
		return ((FILE *) NULL);

	/*
	 * create/truncate file if necessary
	 */
	if ((ret = _openfd(name, omode)) < 0)
		return ((FILE *) NULL);
	close(ret);

	fp = freopen(name,
		fmtab[flag & (FI_READ | FI_WRITE | FI_BINARY | FI_APPEND)], fp);

	if (fp != (FILE *) NULL) {
		set_my_flag(fp, 0); /* must clear it if fp is reused */

		if (flag & FI_APPEND) {
			(void) fseek(fp, (off_t)0, SEEK_END);
		}
		if (flag & FI_UNBUF) {
			setbuf(fp, NULL);
			add_my_flag(fp, _IOUNBUF);
		}
	}
	return (fp);
}
