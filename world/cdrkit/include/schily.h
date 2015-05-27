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

/* @(#)schily.h	1.54 06/01/12 Copyright 1985-2006 J. Schilling */
/*
 *	Definitions for libschily
 *
 *	This file should be included past:
 *
 *	mconfig.h / config.h
 *	standard.h
 *	stdio.h
 *	stdlib.h	(better use stdxlib.h)
 *	unistd.h	(better use unixstd.h) needed LARGEFILE support
 *	string.h
 *	sys/types.h
 *
 *	If you need stdio.h, you must include it before schily.h
 *
 *	NOTE: If you need ctype.h and did not include stdio.h you need to
 *	include ctype.h past schily.h as OpenBSD does not follow POSIX and
 *	defines EOF in ctype.h
 *
 *	Copyright (c) 1985-2006 J. Schilling
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

#ifndef _SCHILY_H
#define	_SCHILY_H

#ifndef _STANDARD_H
#include <standard.h>
#endif
#ifndef _CCOMDEFS_H
#include <ccomdefs.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if	defined(_INCL_SYS_TYPES_H) || defined(off_t)
#	ifndef	FOUND_OFF_T
#	define	FOUND_OFF_T
#	endif
#endif
#if	defined(_INCL_SYS_TYPES_H) || defined(size_t)
#	ifndef	FOUND_SIZE_T
#	define	FOUND_SIZE_T
#	endif
#endif

#ifdef	__never_def__
/*
 * It turns out that we cannot use the folloginw definition because there are
 * some platforms that do not behave application friendly. These are mainly
 * BSD-4.4 based systems (which #undef a definition when size_t is available.
 * We actually removed this code because of a problem with QNX Neutrino.
 * For this reason, it is important not to include <sys/types.h> directly but
 * via the Schily SING include files so we know whether it has been included
 * before we come here.
 */
#if	defined(_SIZE_T)	|| defined(_T_SIZE_)	|| defined(_T_SIZE) || \
	defined(__SIZE_T)	|| defined(_SIZE_T_)	|| \
	defined(_GCC_SIZE_T)	|| defined(_SIZET_)	|| \
	defined(__sys_stdtypes_h) || defined(___int_size_t_h) || defined(size_t)

#ifndef	FOUND_SIZE_T
#	define	FOUND_SIZE_T	/* We already included a size_t definition */
#endif
#endif
#endif	/* __never_def__ */

#if	defined(HAVE_LARGEFILES)
#	define	_fcons		_fcons64
#	define	fdup		fdup64
#	define	fileluopen	fileluopen64
#	define	fileopen	fileopen64
#	define	filemopen	filemopen64
#	define	filepos		filepos64
#	define	filereopen	filereopen64
#	define	fileseek	fileseek64
#	define	filesize	filesize64
#	define	filestat	filestat64
#	define	_openfd		_openfd64
#endif

#ifdef	EOF	/* stdio.h has been included */
extern	int	_cvmod(const char *, int *, int *);
extern	FILE	*_fcons(FILE *, int, int);
extern	FILE	*fdup(FILE *);
extern	int	fdown(FILE *);
extern	int	fexecl(const char *, FILE *, FILE *, FILE *, const char *, ...);
extern	int	fexecle(const char *, FILE *, FILE *, FILE *, const char *, ...);
		/* 6th arg not const, fexecv forces av[ac] = NULL */
extern	int	fexecv(const char *, FILE *, FILE *, FILE *, int, char **);
extern	int	fexecve(const char *, FILE *, FILE *, FILE *, char * const *, 
							  char * const *);
extern	int	fspawnv(FILE *, FILE *, FILE *, int, char * const *);
extern	int	fspawnl(FILE *, FILE *, FILE *, const char *, const char *, ...);
extern	int	fspawnv_nowait(FILE *, FILE *, FILE *, const char *, int, 
										char *const*);
extern	int	fgetline(FILE *, char *, int);
extern	int	fgetstr(FILE *, char *, int);
extern	void	file_raise(FILE *, int);
extern	int	fileclose(FILE *);
extern	FILE	*fileluopen(int, const char *);
extern	FILE	*fileopen(const char *, const char *);
#ifdef	_INCL_SYS_TYPES_H
extern	FILE	*filemopen(const char *, const char *, mode_t);
#endif
#ifdef	FOUND_OFF_T
extern	off_t	filepos(FILE *);
#endif
extern	int	fileread(FILE *, void *, int);
extern	int	ffileread(FILE *, void *, int);
extern	FILE	*filereopen(const char *, const char *, FILE *);
#ifdef	FOUND_OFF_T
extern	int	fileseek(FILE *, off_t);
extern	off_t	filesize(FILE *);
#endif
#ifdef	S_IFMT
extern	int	filestat(FILE *, struct stat *);
#endif
extern	int	filewrite(FILE *, void *, int);
extern	int	ffilewrite(FILE *, void *, int);
extern	int	flush(void);
extern	int	fpipe(FILE **);
extern	int	getbroken(FILE *, char *, char, char **, int);
extern	int	ofindline(FILE *, char, const char *, int, char **, int);
extern	int	peekc(FILE *);

#ifdef	__never_def__
/*
 * We cannot define this or we may get into problems with DOS based systems.
 */
extern	int	spawnv(FILE *, FILE *, FILE *, int, char * const *);
extern	int	spawnl(FILE *, FILE *, FILE *, const char *, const char *, ...);
extern	int	spawnv_nowait(FILE *, FILE *, FILE *, const char *, int, 
									  char *const*);
#endif	/* __never_def__m */
#endif	/* EOF */

extern	int	_niread(int, void *, int);
extern	int	_niwrite(int, void *, int);
extern	int	_nixread(int, void *, int);
extern	int	_nixwrite(int, void *, int);
extern	int	_openfd(const char *, int);
extern	int	on_comerr(void (*fun)(int, void *), void *arg);
/*PRINTFLIKE1*/
extern	void	comerr(const char *, ...) __printflike__(1, 2);
/*PRINTFLIKE2*/
extern	void	comerrno(int, const char *, ...) __printflike__(2, 3);
/*PRINTFLIKE1*/
extern	int	errmsg(const char *, ...) __printflike__(1, 2);
/*PRINTFLIKE2*/
extern	int	errmsgno(int, const char *, ...) __printflike__(2, 3);
#ifdef	FOUND_SIZE_T
/*PRINTFLIKE3*/
extern	int	serrmsg(char *, size_t, const char *, ...) __printflike__(3, 4);
/*PRINTFLIKE4*/
extern	int	serrmsgno(int, char *, size_t, const char *, ...) __printflike__(4, 5);
#endif
extern	void	comexit(int);
extern	char	*errmsgstr(int);
/*PRINTFLIKE1*/
extern	int	schily_error(const char *, ...) __printflike__(1, 2);
extern	char	*fillbytes(void *, int, char);
extern	char	*findbytes(const void *, int, char);
extern	int	findline(const char *, char, const char *, int, char **, int);
extern	int	getline(char *, int);
extern	int	getstr(char *, int);
extern	int	breakline(char *, char, char **, int);
extern	int	getallargs(int *, char * const**, const char *, ...);
extern	int	getargs(int *, char * const**, const char *, ...);
extern	int	getfiles(int *, char * const**, const char *);
extern	char	*astoi(const char *, int *);
extern	char	*astol(const char *, long *);
extern	char	*astolb(const char *, long *, int base);
#ifdef	_UTYPES_H
extern	char	*astoll(const char *, Llong *);
extern	char	*astollb(const char *, Llong *, int);
extern	char	*astoull(const char *, Ullong *);
extern	char	*astoullb(const char *, Ullong *, int);
#endif

/*extern	void	handlecond __PR((const char *, SIGBLK *, int(*)(const char *, long, long), long));*/
/*extern	void	unhandlecond __PR((SIGBLK *));*/

extern	int		patcompile(const unsigned char *, int, int *);
extern	unsigned char	*patmatch(const unsigned char *, const int *,
											 const unsigned char *, int, int, int, int[]);
extern	unsigned char	*patlmatch(const unsigned char *, const int *,
											  const unsigned char *, int, int, int, int[]);

extern	char	*movebytes(const void *, void *, int);

extern	void	save_args(int, char **);
extern	int	saved_ac(void);
extern	char	**saved_av(void);
extern	char	*saved_av0(void);
#ifndef	seterrno
extern	int	seterrno(int);
#endif
extern	void	set_progname(const char *);
extern	char	*get_progname(void);

extern	void	setfp(void * const *);
extern	int	wait_chld(int);		/* for fspawnv_nowait() */
extern	int	geterrno(void);
extern	void	raisecond(const char *, long);
extern	char	*strcatl(char *, ...);
extern	int	streql(const char *, const char *);
#ifdef	va_arg
extern	int	format(void (*)(char, long), long, const char *, va_list);
#else
extern	int	format(void (*)(char, long), long, const char *, void *);
#endif

extern	int	ftoes(char *, double, int, int);
extern	int	ftofs(char *, double, int, int);

extern	void	swabbytes(void *, int);
extern	char	**getmainfp(void);
extern	char	**getavp(void);
extern	char	*getav0(void);
extern	void	**getfp(void);
extern	int	flush_reg_windows(int);
extern	int	cmpbytes(const void *, const void *, int);
extern	int	cmpnullbytes(const void *, int);

#ifdef	nonono
#if	defined(HAVE_LARGEFILES)
/*
 * To allow this, we need to figure out how to do autoconfiguration for off64_t
 */
extern	FILE	*_fcons64(FILE *, int, int);
extern	FILE	*fdup64(FILE *);
extern	FILE	*fileluopen64(int, const char *);
extern	FILE	*fileopen64(const char *, const char *);
#ifdef	FOUND_OFF_T
extern	off64_t	filepos64(FILE *);
#endif
extern	FILE	*filereopen64(const char *, const char *, FILE *);
#ifdef	FOUND_OFF_T
extern	int	fileseek64(FILE *, off64_t);
extern	off64_t	filesize64(FILE *);
#endif
#ifdef	S_IFMT
extern	int	filestat64(FILE *, struct stat *);
#endif
extern	int	_openfd64(const char *, int);
#endif
#endif

#ifdef	__cplusplus
}
#endif

#if defined(_JOS) || defined(JOS)
#	ifndef	_JOS_IO_H
#	include <jos_io.h>
#	endif
#endif

#endif	/* _SCHILY_H */
