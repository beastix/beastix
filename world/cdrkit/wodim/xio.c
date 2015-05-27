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

/* @(#)xio.c	1.11 04/07/11 Copyright 2003-2004 J. Schilling */
/*
 *	EXtended I/O functions for cdrecord
 *
 *	Copyright (c) 2003-2004 J. Schilling
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
#include <unixstd.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <standard.h>
#include <fctldefs.h>

#ifdef	NEED_O_BINARY
#include <io.h>					/* for setmode() prototype */
#endif

#include "xio.h"

static xio_t	x_stdin = {
	NULL,		/* x_next	*/
	NULL,		/* x_name	*/
	0,		/* x_off	*/
	STDIN_FILENO,	/* x_file	*/
	999,		/* x_refcnt	*/
	O_RDONLY,	/* x_oflag	*/
	0		/* x_omode	*/
};

static xio_t	*x_root = &x_stdin;
static xio_t	**x_tail = NULL;


static	xio_t	*xnewnode(char *name);
void	*xopen(char *name, int oflag, int mode);
int	xclose(void *vp);

static xio_t *
xnewnode(char *name)
{
	xio_t	*xp;

	if ((xp = malloc(sizeof (xio_t))) == NULL)
		return ((xio_t *) NULL);

	xp->x_next = (xio_t *) NULL;
	xp->x_name = strdup(name);
	if (xp->x_name == NULL) {
		free(xp);
		return ((xio_t *) NULL);
	}
	xp->x_off = 0;
	xp->x_file = -1;
	xp->x_refcnt = 1;
	xp->x_oflag = 0;
	xp->x_omode = 0;
	return (xp);
}

void *
xopen(char *name, int oflag, int mode)
{
	int	f;
	xio_t	*xp;
	xio_t	*pp = x_root;

	if (x_tail == NULL)
		x_tail = &x_stdin.x_next;
	if (name == NULL) {
		xp = &x_stdin;
		xp->x_refcnt++;
#ifdef	NEED_O_BINARY
		if ((oflag & O_BINARY) != 0) {
			setmode(STDIN_FILENO, O_BINARY);
		}
#endif
		return (xp);
	}
	for (; pp; pp = pp->x_next) {
		if (pp->x_name == NULL)	/* stdin avoid core dump in strcmp() */
			continue;
		if ((strcmp(pp->x_name, name) == 0) &&
		    (pp->x_oflag == oflag) && (pp->x_omode == mode)) {
			break;
		}
	}
	if (pp) {
		pp->x_refcnt++;
		return ((void *)pp);
	}
	if ((f = open(name, oflag, mode)) < 0)
		return (NULL);

	if ((xp = xnewnode(name)) == NULL) {
		close(f);
		return (NULL);
	}
	xp->x_file = f;
	xp->x_oflag = oflag;
	xp->x_omode = mode;
	*x_tail = xp;
	x_tail = &xp->x_next;
	return ((void *)xp);
}

int
xclose(void *vp)
{
	xio_t	*xp = vp;
	xio_t	*pp = x_root;
	int	ret = 0;

	if (xp == &x_stdin)
		return (ret);
	if (x_tail == NULL)
		x_tail = &x_stdin.x_next;

	if (--xp->x_refcnt <= 0) {
		ret = close(xp->x_file);
		while (pp) {
			if (pp->x_next == xp)
				break;
			if (pp->x_next == NULL)
				break;
			pp = pp->x_next;
		}
		if (pp->x_next == xp) {
			if (x_tail == &xp->x_next)
				x_tail = &pp->x_next;
			pp->x_next = xp->x_next;
		}

		free(xp->x_name);
		free(xp);
	}
	return (ret);
}
