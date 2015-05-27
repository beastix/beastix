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

/* @(#)findbytes.c	1.2 03/06/15 Copyright 2000-2003 J. Schilling */
/*
 *	Find a byte with specific value in memory.
 *
 *	Copyright (c) 2000-2003 J. Schilling
 *
 *	Based on a strlen() idea from Torbjorn Granlund (tege@sics.se) and
 *	Dan Sahlin (dan@sics.se) and the memchr() suggestion
 *	from Dick Karpinski (dick@cca.ucsf.edu).
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
#include <stdxlib.h>
#include <utypes.h>
#include <align.h>
#include <standard.h>
#include <strdefs.h>
#include <schily.h>

#ifdef	PROTOTYPES
EXPORT char *
findbytes(const void *vp, int cnt, char val)
#else
EXPORT char *
findbytes(vp, cnt, val)
		const	void	*vp;
	register	int	cnt;
			char	val;
#endif
{
	register	Uchar	uval = (Uchar)val;
	register const	Uchar	*cp  = (Uchar *)vp;
	register const	Ulong	*lp;
	register	Ulong	lval;
	register	Ulong	lmask;
	register	Ulong	magic_mask;

	/*
	 * Test byte-wise until cp is properly aligned for a long pointer.
	 */
	while (--cnt >= 0 && !laligned(cp)) {
		if (*cp++ == uval)
			return ((char *)--cp);
	}
	cnt++;

	/*
	 * The magic mask is a long word where all carry bits a clear.
	 * This are bits 8, 16, 24 ...
	 * In addition, the top bit is not set (e.g bit 31 or 63). The magic
	 * mask will look this way:
	 *
	 * bits:  01111110 11111110 ... 11111110 11111111
	 * bytes: AAAAAAAA BBBBBBBB ... CCCCCCCC DDDDDDDD
	 *
	 * If we add anything to this magic number, no carry bit will change if
	 * it is the first carry bit left to a 0 byte. Adding anything != 0
	 * to the magic number will just turn the carry bit left to the byte
	 * but does not propagate any further.
	 */
#if SIZE_LONG == 4
	magic_mask = 0x7EFEFEFFL;
#else
#if SIZE_LONG == 8
	magic_mask = 0x7EFEFEFEFEFEFEFFL;
#else
	/*
	 * #error will not work for all compilers (e.g. sunos4)
	 * The following line will abort compilation on all compilers
	 * if none of the above is defines. And that's  what we want.
	 */
	error	SIZE_LONG has unknown value
#endif
#endif

	lmask = val & 0xFF;
	lmask |= lmask << 8;
	lmask |= lmask << 16;
#if SIZE_LONG > 4
	lmask |= lmask << 32;
#endif
#if SIZE_LONG > 8
	error	SIZE_LONG has unknown value
#endif
	for (lp = (const Ulong *)cp; cnt >= sizeof (long); cnt -= sizeof (long)) {
		/*
		 * We are not looking for 0 bytes so we need to xor with the
		 * long mask of repeated bytes. If any of the bytes matches our
		 * wanted char, we will create a 0 byte in the current long.
		 * But how will we find if at least one byte in a long is zero?
		 *
		 * If we add 'magic_mask' and any of the holes in the magic
		 * mask do not change, we most likely found a 0 byte in the
		 * long word. It is only a most likely match because if bits
		 * 24..30 (ot bits 56..62) are 0 but bit 31 (or bit 63) is set
		 * we will believe that we found a match but there is none.
		 * This will happen if there is 0x80nnnnnn / 0x80nnnnnnnnnnnnnn
		 */
		lval = (*lp++ ^ lmask);		   /* create 0 byte on match */
		lval = (lval + magic_mask) ^ ~lval; /* set bits unchanged by +*/
		if ((lval & ~magic_mask) != 0) {   /* a magic hole was set   */
			/*
			 * If any of the hole bits did not change by addition,
			 * we most likely had a match.
			 * If this was a correct match, find the matching byte.
			 */
			cp = (const Uchar *)(lp - 1);

			if (cp[0] == uval)
				return ((char *)cp);
			if (cp[1] == uval)
				return ((char *)&cp[1]);
			if (cp[2] == uval)
				return ((char *)&cp[2]);
			if (cp[3] == uval)
				return ((char *)&cp[3]);
#if SIZE_LONG > 4
			if (cp[4] == uval)
				return ((char *)&cp[4]);
			if (cp[5] == uval)
				return ((char *)&cp[5]);
			if (cp[6] == uval)
				return ((char *)&cp[6]);
			if (cp[7] == uval)
				return ((char *)&cp[7]);
#endif
#if SIZE_LONG > 8
			error	SIZE_LONG has unknown value
#endif
		}
	}

	for (cp = (const Uchar *)lp; --cnt >= 0; ) {
		if (*cp++ == uval)
			return ((char *)--cp);
	}
	return ((char *)NULL);
}
