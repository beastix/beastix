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

/* @(#)scsihelp.c	1.4 04/01/14 Copyright 2002 J. Schilling */
/*
 *	usal Library
 *	Help subsystem
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

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <schily.h>

#include <usal/scsitransp.h>

void	__usal_help(FILE *f, char *name, char *tcomment, char *tind, char *tspec,
					  char *texample, BOOL mayscan, BOOL bydev);

void
__usal_help(FILE *f, char *name, char *tcomment, char *tind, char *tspec, 
			  char *texample, BOOL mayscan, BOOL bydev)
{
	fprintf(f, "\nTransport name:		%s\n", name);
	fprintf(f, "Transport descr.:	%s\n", tcomment);
	fprintf(f, "Transp. layer ind.:	%s\n", tind);
	fprintf(f, "Target specifier:	%s\n", tspec);
	fprintf(f, "Target example:		%s\n", texample);
	fprintf(f, "SCSI Bus scanning:	%ssupported\n", mayscan? "":"not ");
	fprintf(f, "Open via UNIX device:	%ssupported\n", bydev? "":"not ");
}
