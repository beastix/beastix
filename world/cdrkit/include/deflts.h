/*
 *	Copyright (c) 2006 Eduard Bloch
 *
 *	Config parsing code, with interface similar to basic libdeflt interface
 *	from J.  Schilling but with different semantics
 *
 *	get_value uses a static buffer (warning, non-reentrant)
 *	cfg_open and cfg_close maintain a static FILE pointer (warning, non-reentrant)
 *
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

#ifndef	_DEFLTS_H
#define	_DEFLTS_H

#ifdef	__cplusplus
extern "C" {
#endif

/* FIXME: use inline trough an INLINE macro wrapper */
extern int	cfg_open	(const char *name);
extern int	cfg_close	(void);

/* reset the position in FILE */
extern void	cfg_restart	(void);
/* returns the next value found after the current position */
extern char	*cfg_get_next	(const char *name);
/* equivalent to cfg_restart(); cfg_get_next(...) */
extern char *cfg_get(const char *key);
/* function wrapped by those above */
extern char *get_value(FILE *srcfile, const char *key, int dorewind);

#ifdef	__cplusplus
}
#endif

#endif
