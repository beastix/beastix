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

/* @(#)scsi_scan.h	1.3 01/03/12 Copyright 1997 J. Schilling */
/*
 *	Interface to scan SCSI Bus.
 *
 *	Copyright (c) 1997 J. Schilling
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

#ifndef	_SCSI_SCAN_H
#define	_SCSI_SCAN_H

#include <usal/scsitransp.h>

extern	int	select_target(SCSI *usalp, FILE *f);
extern int list_devices(SCSI *usalp, FILE *f, int pickup_type);
extern SCSI * open_auto(int64_t need_size, int debug, int lverbose);

#endif	/* _SCSI_SCAN_H */
