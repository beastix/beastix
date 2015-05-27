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

/* @(#)librmt.h	1.16 03/06/15 Copyright 1996 J. Schilling */
/*
 *	Prototypes for rmt client subroutines
 *
 *	Copyright (c) 1995,1996,2000-2002 J. Schilling
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

#ifndef	_LIBRMT_H
#define	_LIBRMT_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _PROTOTYP_H
#include <prototyp.h>
#endif

#ifndef	_INCL_SYS_TYPES_H
#include <sys/types.h>
#define	_INCL_SYS_TYPES_H
#endif

#include <rmtio.h>

/*
 * remote.c
 */
extern	void		rmtinit(int (*errmsgn)(int, const char *, ...),
								  void (*eexit)(int));
extern	int		rmtdebug(int dlevel);
extern	char		*rmtfilename(char *name);
extern	char		*rmthostname(char *hostname, int hnsize, char *rmtspec);
extern	int		rmtgetconn(char *host, int trsize, int excode);
extern	int		rmtopen(int fd, char *fname, int fmode);
extern	int		rmtclose(int fd);
extern	int		rmtread(int fd, char *buf, int count);
extern	int		rmtwrite(int fd, char *buf, int count);
extern	off_t		rmtseek(int fd, off_t offset, int whence);
extern	int		rmtioctl(int fd, int cmd, int count);
#ifdef	MTWEOF
extern	int		rmtstatus(int fd, struct mtget *mtp);
#endif
extern	int		rmtxstatus(int fd, struct rmtget *mtp);
#ifdef	MTWEOF
extern	void		_rmtg2mtg(struct mtget *mtp, struct rmtget *rmtp);
extern	int		_mtg2rmtg(struct rmtget *rmtp, struct mtget *mtp);
#endif

#endif	/* _LIBRMT_H */
