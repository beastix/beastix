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

/* @(#)scsihack.c	1.44 06/01/30 Copyright 1997,2000,2001 J. Schilling */
/*
 *	Interface for other generic SCSI implementations.
 *	Emulate the functionality of /dev/usal? with the local
 *	SCSI user land implementation.
 *
 *	To add a new hack, add something like:
 *
 *	#ifdef	__FreeBSD__
 *	#define	SCSI_IMPL
 *	#include some code
 *	#endif
 *
 *	Warning: you may change this source or add new SCSI tranport
 *	implementations, but if you do that you need to change the
 *	_usal_version and _usal_auth* string that are returned by the
 *	SCSI transport code.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *	If your version has been integrated into the main steam release,
 *	the return value will be set to "schily".
 *
 *	Copyright (c) 1997,2000,2001 J. Schilling
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

#ifdef	HAVE_SYS_PARAM_H
#include <sys/param.h>	/* Include various defs needed with some OS */
#endif
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <errno.h>
#include <timedefs.h>
#include <sys/ioctl.h>
#include <fctldefs.h>
#include <strdefs.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsitransp.h>
#include "usaltimes.h"

#ifndef	HAVE_ERRNO_DEF
extern	int	errno;
#endif

static	int	usalo_send(SCSI *usalp);
static	char *usalo_version(SCSI *usalp, int what);
static	int	usalo_help(SCSI *usalp, FILE *f);
static	int	usalo_open(SCSI *usalp, char *device);
static	int	usalo_close(SCSI *usalp);
static	long	usalo_maxdma(SCSI *usalp, long amt);
static	void *usalo_getbuf(SCSI *usalp, long amt);
static	void	usalo_freebuf(SCSI *usalp);

static	BOOL	usalo_havebus(SCSI *usalp, int busno);
static	int	usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun);
static char * usalo_natname(SCSI *usalp, int busno, int tgt, int tlun);
static	int	usalo_initiator_id(SCSI *usalp);
static	int	usalo_isatapi(SCSI *usalp);
static	int	usalo_reset(SCSI *usalp, int what);

static	char	_usal_auth_cdrkit[]	= "cdrkit-team";	/* The author for this module	*/

usal_ops_t usal_std_ops = {
	usalo_send,
	usalo_version,
	usalo_help,
	usalo_open,
	usalo_close,
	usalo_maxdma,
	usalo_getbuf,
	usalo_freebuf,
	usalo_havebus,
	usalo_fileno,
	usalo_initiator_id,
	usalo_isatapi,
	usalo_reset,
	usalo_natname,
};

/*#undef sun*/
/*#undef __sun*/
/*#undef __sun__*/

#if defined(sun) || defined(__sun) || defined(__sun__)
#define	SCSI_IMPL		/* We have a SCSI implementation for Sun */

#include "scsi-sun.c"

#endif	/* Sun */


#ifdef	linux
#define	SCSI_IMPL		/* We have a SCSI implementation for Linux */

#ifdef	not_needed		/* We now have a local vrersion of pg.h  */
#ifndef	HAVE_LINUX_PG_H		/* If we are compiling on an old version */
#	undef	USE_PG_ONLY	/* there is no 'pg' driver and we cannot */
#	undef	USE_PG		/* include <linux/pg.h> which is needed  */
#endif				/* by the pg transport code.		 */
#endif

#ifdef	USE_PG_ONLY
#include "scsi-linux-pg.c"
#else
#include "scsi-linux-sg.c"
#endif

#endif	/* linux */

#if	defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)  || defined(__FreeBSD_kernel__) 
#define	SCSI_IMPL		/* We have a SCSI implementation for *BSD */

#include "scsi-bsd.c"

#endif	/* *BSD */

#if	defined(__bsdi__)	/* We have a SCSI implementation for BSD/OS 3.x (and later?) */
# include <sys/param.h>
# if (_BSDI_VERSION >= 199701)
#  define	SCSI_IMPL

#  include "scsi-bsd-os.c"

# endif	/* BSD/OS >= 3.0 */
#endif /* BSD/OS */

#ifdef	__sgi
#define	SCSI_IMPL		/* We have a SCSI implementation for SGI */

#include "scsi-sgi.c"

#endif	/* SGI */

#ifdef	__hpux
#define	SCSI_IMPL		/* We have a SCSI implementation for HP-UX */

#include "scsi-hpux.c"

#endif	/* HP-UX */

#if	defined(_IBMR2) || defined(_AIX)
#define	SCSI_IMPL		/* We have a SCSI implementation for AIX */

#include "scsi-aix.c"

#endif	/* AIX */

#if	defined(__NeXT__) || defined(IS_MACOS_X)
#if	defined(HAVE_BSD_DEV_SCSIREG_H)
/*
 *	This is the
 */
#define	SCSI_IMPL		/* We found a SCSI implementation for NextStep and Mac OS X */

#include "scsi-next.c"
#else

#define	SCSI_IMPL		/* We found a SCSI implementation for Mac OS X (Darwin-1.4) */

#include "scsi-mac-iokit.c"

#endif	/* HAVE_BSD_DEV_SCSIREG_H */

#endif	/* NEXT / Mac OS X */

#if	defined(__osf__)
#define	SCSI_IMPL		/* We have a SCSI implementation for OSF/1 */

#include "scsi-osf.c"

#endif	/* OSF/1 */

#ifdef	VMS
#define	SCSI_IMPL		/* We have a SCSI implementation for VMS */

#include "scsi-vms.c"

#endif	/* VMS */

#ifdef	OPENSERVER
#define	SCSI_IMPL		/* We have a SCSI implementation for SCO OpenServer */

#include "scsi-openserver.c"

#endif  /* SCO */

#ifdef	UNIXWARE
#define	SCSI_IMPL		/* We have a SCSI implementation for SCO UnixWare */

#include "scsi-unixware.c"

#endif  /* UNIXWARE */

#ifdef	__OS2
#define	SCSI_IMPL		/* We have a SCSI implementation for OS/2 */

#include "scsi-os2.c"

#endif  /* OS/2 */

#ifdef	__BEOS__
#define	SCSI_IMPL		/* Yep, BeOS does that funky scsi stuff */
#include "scsi-beos.c"
#endif

#ifdef	__CYGWIN32__
#define	SCSI_IMPL		/* Yep, we support WNT and W9? */
#include "scsi-wnt.c"
#endif

#ifdef	apollo
#define	SCSI_IMPL		/* We have a SCSI implementation for Apollo Domain/OS */
#include "scsi-apollo.c"
#endif

#ifdef	AMIGA			/* We have a SCSI implementation for AmigaOS */
#define	SCSI_IMPL
#include "scsi-amigaos.c"
#endif

#if	defined(__QNXNTO__) || defined(__QNX__)
#define	SCSI_IMPL		/* We have a SCSI implementation for QNX */
#include "scsi-qnx.c"
#endif	/* QNX */

#ifdef	__DJGPP__		/* We have a SCSI implementation for MS-DOS/DJGPP */
#define	SCSI_IMPL
#include "scsi-dos.c"
#endif

#ifdef	__NEW_ARCHITECTURE
#define	SCSI_IMPL		/* We have a SCSI implementation for XXX */
/*
 * Add new hacks here
 */
#include "scsi-new-arch.c"
#endif


#ifndef	SCSI_IMPL
/*
 * To make scsihack.c compile on all architectures.
 * This does not mean that you may use it, but you can see
 * if other problems exist.
 */
#define	usalo_dversion		usalo_version
#define	usalo_dhelp		usalo_help
#define	usalo_dopen		usalo_open
#define	usalo_dclose		usalo_close
#define	usalo_dmaxdma		usalo_maxdma
#define	usalo_dgetbuf		usalo_getbuf
#define	usalo_dfreebuf		usalo_freebuf
#define	usalo_dhavebus		usalo_havebus
#define	usalo_dfileno		usalo_fileno
#define	usalo_dinitiator_id	usalo_initiator_id
#define	usalo_disatapi		usalo_isatapi
#define	usalo_dreset		usalo_reset
#define	usalo_dsend		usalo_send
#endif	/* SCSI_IMPL */

static	int	usalo_dsend(SCSI *usalp);
static	char *usalo_dversion(SCSI *usalp, int what);
static	int	usalo_dhelp(SCSI *usalp, FILE *f);
static	int	usalo_nohelp(SCSI *usalp, FILE *f);
static	int	usalo_ropen(SCSI *usalp, char *device);
static	int	usalo_dopen(SCSI *usalp, char *device);
static	int	usalo_dclose(SCSI *usalp);
static	long	usalo_dmaxdma(SCSI *usalp, long amt);
static	void *usalo_dgetbuf(SCSI *usalp, long amt);
static	void	usalo_dfreebuf(SCSI *usalp);
static	BOOL	usalo_dhavebus(SCSI *usalp, int busno);
static	int	usalo_dfileno(SCSI *usalp, int busno, int tgt, int tlun);
static	int	usalo_dinitiator_id(SCSI *usalp);
static	int	usalo_disatapi(SCSI *usalp);
static	int	usalo_dreset(SCSI *usalp, int what);

usal_ops_t usal_remote_ops = {
	usalo_dsend,
	usalo_dversion,
	usalo_nohelp,
	usalo_ropen,
	usalo_dclose,
	usalo_dmaxdma,
	usalo_dgetbuf,
	usalo_dfreebuf,
	usalo_dhavebus,
	usalo_dfileno,
	usalo_dinitiator_id,
	usalo_disatapi,
	usalo_dreset,
  usalo_natname,
};

usal_ops_t usal_dummy_ops = {
	usalo_dsend,
	usalo_dversion,
	usalo_dhelp,
	usalo_dopen,
	usalo_dclose,
	usalo_dmaxdma,
	usalo_dgetbuf,
	usalo_dfreebuf,
	usalo_dhavebus,
	usalo_dfileno,
	usalo_dinitiator_id,
	usalo_disatapi,
	usalo_dreset,
  usalo_natname,
};

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_dversion[] = "scsihack.c-1.44";	/* The version for this transport*/

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_dversion(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_dversion);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (_sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_dhelp(SCSI *usalp, FILE *f)
{
	printf("None.\n");
	return (0);
}

static int
usalo_nohelp(SCSI *usalp, FILE *f)
{
	return (0);
}

static int
usalo_ropen(SCSI *usalp, char *device)
{
	comerrno(EX_BAD, "No remote SCSI transport available.\n");
	return (-1);	/* Keep lint happy */
}

#ifndef	SCSI_IMPL
static int
usalo_dopen(SCSI *usalp, char *device)
{
	comerrno(EX_BAD, "No local SCSI transport implementation for this architecture.\n");
	return (-1);	/* Keep lint happy */
}
#else
static int
usalo_dopen(SCSI *usalp, char *device)
{
	comerrno(EX_BAD, "SCSI open usage error.\n");
	return (-1);	/* Keep lint happy */
}
#endif	/* SCSI_IMPL */

static int
usalo_dclose(SCSI *usalp)
{
	errno = EINVAL;
	return (-1);
}

static long
usalo_dmaxdma(SCSI *usalp, long amt)
{
	errno = EINVAL;
	return	(0L);
}

static void *
usalo_dgetbuf(SCSI *usalp, long amt)
{
	errno = EINVAL;
	return ((void *)0);
}

static void
usalo_dfreebuf(SCSI *usalp)
{
}

static BOOL
usalo_dhavebus(SCSI *usalp, int busno)
{
	return (FALSE);
}

static int
usalo_dfileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	return (-1);
}

#ifndef HAVE_NAT_NAMES /* to be defined in included source if supported */
static char * usalo_natname(SCSI *usalp, int busno, int tgt, int tlun) {
   static char namebuf[81];
   snprintf(namebuf, 80, "%d,%d,%d", usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
   return namebuf;
}
#endif

static int
usalo_dinitiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_disatapi(SCSI *usalp)
{
	return (FALSE);
}

static int
usalo_dreset(SCSI *usalp, int what)
{
	errno = EINVAL;
	return (-1);
}

static int
usalo_dsend(SCSI *usalp)
{
	errno = EINVAL;
	return (-1);
}
