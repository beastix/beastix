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

/* @(#)wm_session.c	1.4 02/07/07 Copyright 1995, 1997 J. Schilling */
/*
 *	CDR write method abtraction layer
 *	session at once / disk at once writing intercace routines
 *
 *	Copyright (c) 1995, 1997 J. Schilling
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
#include <stdxlib.h>
#include <unixstd.h>
#include <standard.h>
#include <utypes.h>

#include <usal/scsitransp.h>
#include "wodim.h"

extern	int	debug;
extern	int	verbose;
extern	int	lverbose;

extern	char	*buf;			/* The transfer buffer */

int	write_session_data(SCSI *usalp, cdr_t *dp, track_t *trackp);
