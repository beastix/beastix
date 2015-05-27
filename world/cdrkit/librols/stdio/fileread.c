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

/* @(#)fileread.c	1.14 04/08/08 Copyright 1986, 1995-2003 J. Schilling */
/*
 *	Copyright (c) 1986, 1995-2003 J. Schilling
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

static	char	_readerr[]	= "file_read_err";

#ifdef	HAVE_USG_STDIO

EXPORT int
fileread(f, buf, len)
	register FILE	*f;
	void	*buf;
	int	len;
{
	int	cnt;
	register int	n;

	down2(f, _IOREAD, _IORW);

	if (f->_flag & _IONBF) {
		cnt = _niread(fileno(f), buf, len);
		if (cnt < 0) {
			f->_flag |= _IOERR;
			if (!(my_flag(f) & _IONORAISE))
				raisecond(_readerr, 0L);
		}
		if (cnt == 0 && len)
			f->_flag |= _IOEOF;
		return (cnt);
	}
	cnt = 0;
	while (len > 0) {
		if (f->_cnt <= 0) {
			if (usg_filbuf(f) == EOF)
				break;
			f->_cnt++;
			f->_ptr--;
		}
		n = f->_cnt >= len ? len : f->_cnt;
		buf = (void *)movebytes(f->_ptr, buf, n);
		f->_ptr += n;
		f->_cnt -= n;
		cnt += n;
		len -= n;
	}
	if (!ferror(f))
		return (cnt);
	if (!(my_flag(f) & _IONORAISE))
		raisecond(_readerr, 0L);
	return (-1);
}

#else

EXPORT int
fileread(f, buf, len)
	register FILE	*f;
	void	*buf;
	int	len;
{
	int	cnt;

	down2(f, _IOREAD, _IORW);

	if (my_flag(f) & _IOUNBUF)
		return (_niread(fileno(f), buf, len));
	cnt = fread(buf, 1, len, f);

	if (!ferror(f))
		return (cnt);
	if (!(my_flag(f) & _IONORAISE))
		raisecond(_readerr, 0L);
	return (-1);
}

#endif	/* HAVE_USG_STDIO */
