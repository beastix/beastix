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

/* @(#)record.h	1.1 00/03/05 joerg */
/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996, 1997 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

void r_packcatkey(CatKeyRec *, unsigned char *, int *);
void r_unpackcatkey(unsigned char *, CatKeyRec *);

void r_packextkey(ExtKeyRec *, unsigned char *, int *);
void r_unpackextkey(unsigned char *, ExtKeyRec *);

int r_comparecatkeys(unsigned char *, unsigned char *);
int r_compareextkeys(unsigned char *, unsigned char *);

void r_packcatdata(CatDataRec *, unsigned char *, int *);
void r_unpackcatdata(unsigned char *, CatDataRec *);

void r_packextdata(ExtDataRec *, unsigned char *, int *);
void r_unpackextdata(unsigned char *, ExtDataRec *);

void r_makecatkey(CatKeyRec *, long, char *);
void r_makeextkey(ExtKeyRec *, int, long, unsigned int);

void r_unpackdirent(long, char *, CatDataRec *, hfsdirent *);
void r_packdirent(CatDataRec *, hfsdirent *);
