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

/* @(#)cd_misc.c	1.10 01/10/29 Copyright 1997 J. Schilling */
/*
 *	Misc CD support routines
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

#include <mconfig.h>
#include <standard.h>
#include <utypes.h>	/* Includes <sys/types.h> for caddr_t */
#include <stdio.h>
#include <schily.h>

#include "wodim.h"

int	from_bcd(int b);
int	to_bcd(int i);
long	msf_to_lba(int m, int s, int f, BOOL force_positive);
BOOL	lba_to_msf(long lba, msf_t *mp);
void	sec_to_msf(long sec, msf_t *mp);
void	print_min_atip(long li, long lo);

int 
from_bcd(int b)
{
	return ((b & 0x0F) + 10 * (((b)>> 4) & 0x0F));
}

int 
to_bcd(int i)
{
	return (i % 10 | ((i / 10) % 10) << 4);
}

long 
msf_to_lba(int m, int s, int f, BOOL force_positive)
{
	long	ret = m * 60 + s;

	ret *= 75;
	ret += f;
	if (m < 90 || force_positive)
		ret -= 150;
	else
		ret -= 450150;
	return (ret);
}

BOOL 
lba_to_msf(long lba, msf_t *mp)
{
	int	m;
	int	s;
	int	f;

#ifdef	__follow_redbook__
	if (lba >= -150 && lba < 405000) {	/* lba <= 404849 */
#else
	if (lba >= -150) {
#endif
		m = (lba + 150) / 60 / 75;
		s = (lba + 150 - m*60*75)  / 75;
		f = (lba + 150 - m*60*75 - s*75);

	} else if (lba >= -45150 && lba <= -151) {
		m = (lba + 450150) / 60 / 75;
		s = (lba + 450150 - m*60*75)  / 75;
		f = (lba + 450150 - m*60*75 - s*75);

	} else {
		mp->msf_min   = -1;
		mp->msf_sec   = -1;
		mp->msf_frame = -1;

		return (FALSE);
	}
	mp->msf_min   = m;
	mp->msf_sec   = s;
	mp->msf_frame = f;

	if (lba > 404849)			/* 404850 -> 404999: lead out */
		return (FALSE);
	return (TRUE);
}

void 
sec_to_msf(long sec, msf_t *mp)
{
	int	m;
	int	s;
	int	f;

	m = (sec) / 60 / 75;
	s = (sec - m*60*75)  / 75;
	f = (sec - m*60*75 - s*75);

	mp->msf_min   = m;
	mp->msf_sec   = s;
	mp->msf_frame = f;
}

void 
print_min_atip(long li, long lo)
{
	msf_t	msf;

	if (li < 0) {
		lba_to_msf(li, &msf);

		printf("  ATIP start of lead in:  %ld (%02d:%02d/%02d)\n",
			li, msf.msf_min, msf.msf_sec, msf.msf_frame);
	}
	if (lo > 0) {
		lba_to_msf(lo, &msf);
		printf("  ATIP start of lead out: %ld (%02d:%02d/%02d)\n",
			lo, msf.msf_min, msf.msf_sec, msf.msf_frame);
	}
}
