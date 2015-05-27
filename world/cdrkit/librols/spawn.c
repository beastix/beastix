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

/* @(#)spawn.c	1.17 04/09/25 Copyright 1985, 1989, 1995-2003 J. Schilling */
/*
 *	Spawn another process/ wait for child process
 *
 *	Copyright (c) 1985, 1989, 1995-2003 J. Schilling
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
#include <standard.h>
#define	fspawnl	__nothing__	/* prototype in schily.h is wrong */
#define	spawnl	__nothing__	/* prototype in schily.h is wrong */
#include <schily.h>
#undef	fspawnl
#undef	spawnl
#include <unixstd.h>
#include <stdxlib.h>
#include <vadefs.h>
#include <waitdefs.h>
#include <errno.h>

#define	MAX_F_ARGS	16

EXPORT	int	fspawnl	__PR((FILE *, FILE *, FILE *, ...));

EXPORT int
fspawnv(in, out, err, argc, argv)
	FILE	*in;
	FILE	*out;
	FILE	*err;
	int	argc;
	char	* const argv[];
{
	int	pid;

	if ((pid = fspawnv_nowait(in, out, err, argv[0], argc, argv)) < 0)
		return (pid);

	return (wait_chld(pid));
}

/* VARARGS3 */
#ifdef	PROTOTYPES
EXPORT int
fspawnl(FILE *in, FILE *out, FILE *err, ...)
#else
EXPORT int
fspawnl(in, out, err, va_alist)
	FILE	*in;
	FILE	*out;
	FILE	*err;
	va_dcl
#endif
{
	va_list	args;
	int	ac = 0;
	char	*xav[MAX_F_ARGS];
	char	**av;
	char	**pav;
	char	*p;
	int	ret;

#ifdef	PROTOTYPES
	va_start(args, err);
#else
	va_start(args);
#endif
	while (va_arg(args, char *) != NULL)
		ac++;
	va_end(args);

	if (ac < MAX_F_ARGS) {
		pav = av = xav;
	} else {
		pav = av = (char **)malloc((ac+1)*sizeof (char *));
		if (av == 0)
			return (-1);
	}

#ifdef	PROTOTYPES
	va_start(args, err);
#else
	va_start(args);
#endif
	do {
		p = va_arg(args, char *);
		*pav++ = p;
	} while (p != NULL);
	va_end(args);

	ret =  fspawnv(in, out, err, ac, av);
	if (av != xav)
		free(av);
	return (ret);
}

EXPORT int
fspawnv_nowait(in, out, err, name, argc, argv)
	FILE		*in;
	FILE		*out;
	FILE		*err;
	const char	*name;
	int		argc;
	char		* const argv[];
{
	int	pid = -1;	/* Initialization needed to make GCC happy */
	int	i;

	for (i = 1; i < 64; i *= 2) {
		if ((pid = fork()) >= 0)
			break;
		sleep(i);
	}
	if (pid != 0)
		return (pid);
				/*
				 * silly: fexecv must set av[ac] = NULL
				 * so we have to cast argv tp (char **)
				 */
	fexecv(name, in, out, err, argc, (char **)argv);
	exit(geterrno());
	/* NOTREACHED */
}

EXPORT int
wait_chld(pid)
	int	pid;
{
	int	died;
	WAIT_T	status;

	do {
		do {
			died = wait(&status);
		} while (died < 0 && geterrno() == EINTR);
		if (died < 0)
			return (died);
	} while (died != pid);

	if (WCOREDUMP(status))
		unlink("core");

	return (WEXITSTATUS(status));
}
