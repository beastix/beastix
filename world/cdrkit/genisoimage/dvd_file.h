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

/* @(#)dvd_file.h	1.1 02/07/21 Copyright 2002 J. Schilling */
/*
 *	Defnitions for users of dvd_file.c
 *
 *	Copyright (c) 2002 J. Schilling
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

#ifndef	_DVD_FILE_H
#define	_DVD_FILE_H

extern	void		DVDFreeFileSet(title_set_info_t *title_set_info);
extern	title_set_info_t *DVDGetFileSet(char *dvd);
extern	int		DVDGetFilePad(title_set_info_t *title_set_info, char *name);

#endif	/* _DVD_FILE_H */
