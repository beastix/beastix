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

/* @(#)bswap.h	1.2 03/03/25 Copyright 2002 J. Schilling */
#ifndef _BSWAP_H
#define	_BSWAP_H

/*
 *	Allow to use B2N_* macros found in libdvdread in a portable way.
 *	These macros should better be avoided as in place conversion in
 *	general only works on processors like Motorola 68000 and Intel x86.
 *	Modern processors usually have alignement restrictions that may
 *	cause problems. The stripped down libdvdread for genisoimage is known
 *	not to have these alignement problems, so we may use the macros
 *	as they have been introduced by the authors of libdvdread.
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
#include <btorder.h>
#include <intcvt.h>

#if defined(WORDS_BIGENDIAN)

/* All bigendian systems are fine, just ignore the swaps. */
#define	B2N_16(x) (void)(x)
#define	B2N_32(x) (void)(x)
#define	B2N_64(x) (void)(x)

#else

/*
 * It is a bad idea to convert numbers in place.
 * In protocols, there is usually the additional problem that the
 * data is not properly aligned.
 */
#define	B2N_16(x) (x) = a_to_u_2_byte(&(x))
#define	B2N_32(x) (x) = a_to_u_4_byte(&(x))
#define	B2N_64(x) (x) = a_to_u_8_byte(&(x))

#endif

#endif /* _BSWAP_H */
