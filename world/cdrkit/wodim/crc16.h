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

/* @(#)crc16.h	1.3 02/03/04 Copyright 1998-2002 J. Schilling */
/*
 *	Q-subchannel CRC definitions
 *
 *	Copyright (c) 1998-2002 J. Schilling
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

#ifndef	_CRC16_H
#define	_CRC16_H

extern	UInt16_t	calcCRC(Uchar *buf, Uint bsize);
extern	UInt16_t	fillcrc(Uchar *buf, Uint bsize);
extern	UInt16_t	flip_crc_error_corr(Uchar *b, Uint bsize, Uint p_crc);

#endif
