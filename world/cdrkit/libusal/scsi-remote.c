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

#define	USE_REMOTE
/* @(#)scsi-remote.c	1.18 06/01/12 Copyright 1990,2000-2003 J. Schilling */
/*
 *	Remote SCSI user level command transport routines
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1990,2000-2003 J. Schilling
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

#if !defined(HAVE_FORK) || !defined(HAVE_SOCKETPAIR) || !defined(HAVE_DUP2)
#undef	USE_RCMD_RSH
#endif
/*
 * We may work without getservbyname() if we restructure the code not to
 * use the port number if we only use _rcmdrsh().
 */
#if !defined(HAVE_GETSERVBYNAME)
#undef	USE_REMOTE				/* Cannot get rcmd() port # */
#endif
#if (!defined(HAVE_NETDB_H) || !defined(HAVE_RCMD)) && !defined(USE_RCMD_RSH)
#undef	USE_REMOTE				/* There is no rcmd() */
#endif

#ifdef	USE_REMOTE
#include <stdio.h>
#include <sys/types.h>
#include <fctldefs.h>
#ifdef	HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#include <errno.h>
#include <signal.h>
#ifdef	HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef	HAVE_PWD_H
#include <pwd.h>
#endif
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsitransp.h>

#if	defined(SIGDEFER) || defined(SVR4)
#define	signal	sigset
#endif

/*
 * On Cygwin, there are no privilleged ports.
 * On UNIX, rcmd() uses privilleged port that only work for root.
 */
#ifdef	IS_CYGWIN
#define	privport_ok()	(1)
#else
#ifdef	HAVE_GETPPRIV
#define	privport_ok()	ppriv_ok()
#else
#define	privport_ok()	(geteuid() == 0)
#endif
#endif

#define	CMD_SIZE	80

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

/*extern	BOOL	debug;*/
LOCAL	BOOL	debug = 1;

LOCAL	char	_usal_trans_version[] = "remote-1.18";	/* The version for remote SCSI	*/
LOCAL	char	_usal_auth_cdrkit[]	= "cdrkit-team";	/* The author for this module	*/

LOCAL	int	usalo_rsend		__PR((SCSI *usalp));
LOCAL	char *	usalo_rversion		__PR((SCSI *usalp, int what));
LOCAL	int	usalo_rhelp		__PR((SCSI *usalp, FILE *f));
LOCAL	int	usalo_ropen		__PR((SCSI *usalp, char *device));
LOCAL	int	usalo_rclose		__PR((SCSI *usalp));
LOCAL	long	usalo_rmaxdma		__PR((SCSI *usalp, long amt));
LOCAL	void *	usalo_rgetbuf		__PR((SCSI *usalp, long amt));
LOCAL	void	usalo_rfreebuf		__PR((SCSI *usalp));
LOCAL	BOOL	usalo_rhavebus		__PR((SCSI *usalp, int busno));
LOCAL	int	usalo_rfileno		__PR((SCSI *usalp, int busno, int tgt, int tlun));
LOCAL	int	usalo_rinitiator_id	__PR((SCSI *usalp));
LOCAL	int	usalo_risatapi		__PR((SCSI *usalp));
LOCAL	int	usalo_rreset		__PR((SCSI *usalp, int what));

/*
 * XXX We should rethink the fd parameter now that we introduced
 * XXX the rscsirchar() function and most access of remfd is done
 * XXX via usallocal(usalp)->remfd.
 */
LOCAL	void	rscsiabrt		__PR((int sig));
LOCAL	int	rscsigetconn		__PR((SCSI *usalp, char *host));
LOCAL	char	*rscsiversion		__PR((SCSI *usalp, int fd, int what));
LOCAL	int	rscsiopen		__PR((SCSI *usalp, int fd, char *fname));
LOCAL	int	rscsiclose		__PR((SCSI *usalp, int fd));
LOCAL	int	rscsimaxdma		__PR((SCSI *usalp, int fd, long amt));
LOCAL	int	rscsigetbuf		__PR((SCSI *usalp, int fd, long amt));
LOCAL	int	rscsifreebuf		__PR((SCSI *usalp, int fd));
LOCAL	int	rscsihavebus		__PR((SCSI *usalp, int fd, int bus));
LOCAL	int	rscsifileno		__PR((SCSI *usalp, int fd, int busno, int tgt, int tlun));
LOCAL	int	rscsiinitiator_id	__PR((SCSI *usalp, int fd));
LOCAL	int	rscsiisatapi		__PR((SCSI *usalp, int fd));
LOCAL	int	rscsireset		__PR((SCSI *usalp, int fd, int what));
LOCAL	int	rscsiscmd		__PR((SCSI *usalp, int fd, struct usal_cmd *sp));
LOCAL	int	rscsifillrbuf		__PR((SCSI *usalp));
LOCAL	int	rscsirchar		__PR((SCSI *usalp, char *cp));
LOCAL	int	rscsireadbuf		__PR((SCSI *usalp, int fd, char *buf, int count));
LOCAL	void	rscsivoidarg		__PR((SCSI *usalp, int fd, int count));
LOCAL	int	rscsicmd		__PR((SCSI *usalp, int fd, char *name, char *cbuf));
LOCAL	void	rscsisendcmd		__PR((SCSI *usalp, int fd, char *name, char *cbuf));
LOCAL	int	rscsigetline		__PR((SCSI *usalp, int fd, char *line, int count));
LOCAL	int	rscsireadnum		__PR((SCSI *usalp, int fd));
LOCAL	int	rscsigetstatus		__PR((SCSI *usalp, int fd, char *name));
LOCAL	int	rscsiaborted		__PR((SCSI *usalp, int fd));
#ifdef	USE_RCMD_RSH
LOCAL	int	_rcmdrsh		__PR((char **ahost, int inport,
						const char *locuser,
						const char *remuser,
						const char *cmd,
						const char *rsh));
#ifdef	HAVE_GETPPRIV
LOCAL	BOOL	ppriv_ok		__PR((void));
#endif
#endif

/*--------------------------------------------------------------------------*/

#define	READBUF_SIZE	128

struct usal_local {
	int	remfd;
	char	readbuf[READBUF_SIZE];
	char	*readbptr;
	int	readbcnt;
	BOOL	isopen;
	int	rsize;
	int	wsize;
	char	*v_version;
	char	*v_author;
	char	*v_sccs_id;
};


#define	usallocal(p)	((struct usal_local *)((p)->local))

usal_ops_t remote_ops = {
	usalo_rsend,		/* "S" end	*/
	usalo_rversion,		/* "V" ersion	*/
	usalo_rhelp,		/*     help	*/
	usalo_ropen,		/* "O" pen	*/
	usalo_rclose,		/* "C" lose	*/
	usalo_rmaxdma,		/* "D" MA	*/
	usalo_rgetbuf,		/* "M" alloc	*/
	usalo_rfreebuf,		/* "F" free	*/
	usalo_rhavebus,		/* "B" us	*/
	usalo_rfileno,		/* "T" arget	*/
	usalo_rinitiator_id,	/* "I" nitiator	*/
	usalo_risatapi,		/* "A" tapi	*/
	usalo_rreset,		/* "R" eset	*/
};

/*
 * Return our ops ptr.
 */
usal_ops_t *
usal_remote()
{
	return (&remote_ops);
}

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
LOCAL char *
usalo_rversion(usalp, what)
	SCSI	*usalp;
	int	what;
{
	int	f;

	if (usalp->local == NULL)
		return ((char *)0);

	f = usallocal(usalp)->remfd;
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (__sccsid);

		case SCG_RVERSION:
			if (usallocal(usalp)->v_version == NULL)
				usallocal(usalp)->v_version = rscsiversion(usalp, f, SCG_VERSION);
			return (usallocal(usalp)->v_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_RAUTHOR:
			if (usallocal(usalp)->v_author == NULL)
				usallocal(usalp)->v_author = rscsiversion(usalp, f, SCG_AUTHOR);
			return (usallocal(usalp)->v_author);
		case SCG_RSCCS_ID:
			if (usallocal(usalp)->v_sccs_id == NULL)
				usallocal(usalp)->v_sccs_id = rscsiversion(usalp, f, SCG_SCCS_ID);
			return (usallocal(usalp)->v_sccs_id);
		}
	}
	return ((char *)0);
}

LOCAL int
usalo_rhelp(usalp, f)
	SCSI	*usalp;
	FILE	*f;
{
	__usal_help(f, "RSCSI", "Remote SCSI",
		"REMOTE:", "rscsi@host:bus,target,lun", "REMOTE:rscsi@host:1,2,0", TRUE, FALSE);
	return (0);
}

LOCAL int
usalo_ropen(usalp, device)
	SCSI	*usalp;
	char	*device;
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
		int	tlun	= usal_lun(usalp);
	register int	f;
	register int	nopen = 0;
	char		devname[128];
	char		*p;

	if (usalp->overbose)
		fprintf(stderr, "Warning: Using remote SCSI interface.\n");

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			"Illegal value for busno, target or lun '%d,%d,%d'",
			busno, tgt, tlun);

		return (-1);
	}
	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);
		usallocal(usalp)->remfd = -1;
		usallocal(usalp)->readbptr = usallocal(usalp)->readbuf;
		usallocal(usalp)->readbcnt = 0;
		usallocal(usalp)->isopen = FALSE;
		usallocal(usalp)->rsize = 0;
		usallocal(usalp)->wsize = 0;
		usallocal(usalp)->v_version = NULL;
		usallocal(usalp)->v_author  = NULL;
		usallocal(usalp)->v_sccs_id = NULL;
	}

	if (device == NULL || (strncmp(device, "REMOTE", 6) != 0) ||
				(device = strchr(device, ':')) == NULL) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal remote device syntax");
		return (-1);
	}
	device++;
	/*
	 * Save non user@host:device
	 */
	snprintf(devname, sizeof (devname), "%s", device);

	if ((p = strchr(devname, ':')) != NULL)
		*p++ = '\0';

	f = rscsigetconn(usalp, devname);
	if (f < 0) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Cannot get connection to remote host");
		return (-1);
	}
	usallocal(usalp)->remfd = f;
	debug = usalp->debug;
	if (rscsiopen(usalp, f, p) >= 0) {
		nopen++;
		usallocal(usalp)->isopen = TRUE;
	}
	return (nopen);
}

LOCAL int
usalo_rclose(usalp)
	SCSI	*usalp;
{
	register int	f;
		int	ret;

	if (usalp->local == NULL)
		return (-1);

	if (usallocal(usalp)->v_version != NULL) {
		free(usallocal(usalp)->v_version);
		usallocal(usalp)->v_version = NULL;
	}
	if (usallocal(usalp)->v_author != NULL) {
		free(usallocal(usalp)->v_author);
		usallocal(usalp)->v_author  = NULL;
	}
	if (usallocal(usalp)->v_sccs_id != NULL) {
		free(usallocal(usalp)->v_sccs_id);
		usallocal(usalp)->v_sccs_id = NULL;
	}

	f = usallocal(usalp)->remfd;
	if (f < 0 || !usallocal(usalp)->isopen)
		return (0);
	ret = rscsiclose(usalp, f);
	usallocal(usalp)->isopen = FALSE;
	close(f);
	usallocal(usalp)->remfd = -1;
	return (ret);
}

LOCAL long
usalo_rmaxdma(usalp, amt)
	SCSI	*usalp;
	long	amt;
{
	if (usalp->local == NULL)
		return (-1L);

	return (rscsimaxdma(usalp, usallocal(usalp)->remfd, amt));
}

LOCAL void *
usalo_rgetbuf(usalp, amt)
	SCSI	*usalp;
	long	amt;
{
	int	ret;

	if (usalp->local == NULL)
		return ((void *)0);

	ret = rscsigetbuf(usalp, usallocal(usalp)->remfd, amt);
	if (ret < 0)
		return ((void *)0);

#ifdef	HAVE_VALLOC
	usalp->bufbase = (void *)valloc((size_t)amt);
#else
	usalp->bufbase = (void *)malloc((size_t)amt);
#endif
	if (usalp->bufbase == NULL) {
		usalo_rfreebuf(usalp);
		return ((void *)0);
	}
	return (usalp->bufbase);
}

LOCAL void
usalo_rfreebuf(usalp)
	SCSI	*usalp;
{
	int	f;

	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;

	if (usalp->local == NULL)
		return;

	f = usallocal(usalp)->remfd;
	if (f < 0 || !usallocal(usalp)->isopen)
		return;
	rscsifreebuf(usalp, f);
}

LOCAL BOOL
usalo_rhavebus(usalp, busno)
	SCSI	*usalp;
	int	busno;
{
	if (usalp->local == NULL || busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	return (rscsihavebus(usalp, usallocal(usalp)->remfd, busno));
}

LOCAL int
usalo_rfileno(usalp, busno, tgt, tlun)
	SCSI	*usalp;
	int	busno;
	int	tgt;
	int	tlun;
{
	int	f;

	if (usalp->local == NULL ||
	    busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	f = usallocal(usalp)->remfd;
	if (f < 0 || !usallocal(usalp)->isopen)
		return (-1);
	return (rscsifileno(usalp, f, busno, tgt, tlun));
}

LOCAL int
usalo_rinitiator_id(usalp)
	SCSI	*usalp;
{
	if (usalp->local == NULL)
		return (-1);

	return (rscsiinitiator_id(usalp, usallocal(usalp)->remfd));
}

LOCAL int
usalo_risatapi(usalp)
	SCSI	*usalp;
{
	if (usalp->local == NULL)
		return (-1);

	return (rscsiisatapi(usalp, usallocal(usalp)->remfd));
}

LOCAL int
usalo_rreset(usalp, what)
	SCSI	*usalp;
	int	what;
{
	if (usalp->local == NULL)
		return (-1);

	return (rscsireset(usalp, usallocal(usalp)->remfd, what));
}

LOCAL int
usalo_rsend(usalp)
	SCSI		*usalp;
{
	struct usal_cmd	*sp = usalp->scmd;
	int		ret;

	if (usalp->local == NULL)
		return (-1);

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}
	ret = rscsiscmd(usalp, usallocal(usalp)->remfd, usalp->scmd);

	return (ret);
}

/*--------------------------------------------------------------------------*/
LOCAL void
rscsiabrt(sig)
	int	sig;
{
	rscsiaborted((SCSI *)0, -1);
}

LOCAL int
rscsigetconn(usalp, host)
	SCSI	*usalp;
	char	*host;
{
	static	struct servent	*sp = 0;
	static	struct passwd	*pw = 0;
		char		*name = "root";
		char		*p;
		char		*rscsi;
		char		*rsh;
		int		rscsisock;
		char		*rscsipeer;
		char		rscsiuser[128];


	signal(SIGPIPE, rscsiabrt);
	if (sp == 0) {
		sp = getservbyname("shell", "tcp");
		if (sp == 0) {
			comerrno(EX_BAD, "shell/tcp: unknown service\n");
			/* NOTREACHED */
		}
		pw = getpwuid(getuid());
		if (pw == 0) {
			comerrno(EX_BAD, "who are you? No passwd entry found.\n");
			/* NOTREACHED */
		}
	}
	if ((p = strchr(host, '@')) != NULL) {
		size_t d = p - host;

		if (d > sizeof (rscsiuser))
			d = sizeof (rscsiuser);
		snprintf(rscsiuser, sizeof (rscsiuser), "%.*s", (int)d, host);
		name = rscsiuser;
		host = &p[1];
	} else {
		name = pw->pw_name;
	}
	if (usalp->debug > 0)
		errmsgno(EX_BAD, "locuser: '%s' rscsiuser: '%s' host: '%s'\n",
						pw->pw_name, name, host);
	rscsipeer = host;

	if ((rscsi = getenv("RSCSI")) == NULL)
		rscsi = "/usr/sbin/netscsid";
	rsh = getenv("RSH");

#ifdef	USE_RCMD_RSH
	if (!privport_ok() || rsh != NULL)
		rscsisock = _rcmdrsh(&rscsipeer, (unsigned short)sp->s_port,
					pw->pw_name, name, rscsi, rsh);
	else
#endif
#ifdef	HAVE_RCMD
		rscsisock = rcmd(&rscsipeer, (unsigned short)sp->s_port,
					pw->pw_name, name, rscsi, 0);
#else
		rscsisock = _rcmdrsh(&rscsipeer, (unsigned short)sp->s_port,
					pw->pw_name, name, rscsi, rsh);
#endif

	return (rscsisock);
}

LOCAL char *
rscsiversion(usalp, fd, what)
	SCSI	*usalp;
	int	fd;
	int	what;
{
	char	cbuf[CMD_SIZE];
	char	*p;
	int	ret;

	snprintf(cbuf, sizeof (cbuf), "V%d\n", what);
	ret = rscsicmd(usalp, fd, "version", cbuf);
	p = malloc(ret);
	if (p == NULL)
		return (p);
	rscsireadbuf(usalp, fd, p, ret);
	return (p);
}

LOCAL int
rscsiopen(usalp, fd, fname)
	SCSI	*usalp;
	int	fd;
	char	*fname;
{
	char	cbuf[CMD_SIZE];
	int	ret;
	int	bus;
	int	chan;
	int	tgt;
	int	lun;

	snprintf(cbuf, sizeof (cbuf), "O%s\n", fname?fname:"");
	ret = rscsicmd(usalp, fd, "open", cbuf);
	if (ret < 0)
		return (ret);

	bus = rscsireadnum(usalp, fd);
	chan = rscsireadnum(usalp, fd);
	tgt = rscsireadnum(usalp, fd);
	lun = rscsireadnum(usalp, fd);

	usal_settarget(usalp, bus, tgt, lun);
	return (ret);
}

LOCAL int
rscsiclose(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	return (rscsicmd(usalp, fd, "close", "C\n"));
}

LOCAL int
rscsimaxdma(usalp, fd, amt)
	SCSI	*usalp;
	int	fd;
	long	amt;
{
	char	cbuf[CMD_SIZE];

	snprintf(cbuf, sizeof (cbuf), "D%ld\n", amt);
	return (rscsicmd(usalp, fd, "maxdma", cbuf));
}

LOCAL int
rscsigetbuf(usalp, fd, amt)
	SCSI	*usalp;
	int	fd;
	long	amt;
{
	char	cbuf[CMD_SIZE];
	int	size;
	int	ret;

	snprintf(cbuf, sizeof (cbuf), "M%ld\n", amt);
	ret = rscsicmd(usalp, fd, "getbuf", cbuf);
	if (ret < 0)
		return (ret);

	size = ret + 1024;	/* Add protocol overhead */

#ifdef	SO_SNDBUF
	if (size > usallocal(usalp)->wsize) while (size > 512 &&
		setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
					(char *)&size, sizeof (size)) < 0) {
		size -= 512;
	}
	if (size > usallocal(usalp)->wsize) {
		usallocal(usalp)->wsize = size;
		if (usalp->debug > 0)
			errmsgno(EX_BAD, "sndsize: %d\n", size);
	}
#endif
#ifdef	SO_RCVBUF
	if (size > usallocal(usalp)->rsize) while (size > 512 &&
		setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
					(char *)&size, sizeof (size)) < 0) {
		size -= 512;
	}
	if (size > usallocal(usalp)->rsize) {
		usallocal(usalp)->rsize = size;
		if (usalp->debug > 0)
			errmsgno(EX_BAD, "rcvsize: %d\n", size);
	}
#endif
	return (ret);
}

LOCAL int
rscsifreebuf(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	return (rscsicmd(usalp, fd, "freebuf", "F\n"));
}

LOCAL int
rscsihavebus(usalp, fd, busno)
	SCSI	*usalp;
	int	fd;
	int	busno;
{
	char	cbuf[2*CMD_SIZE];

	snprintf(cbuf, sizeof (cbuf), "B%d\n%d\n",
		busno,
		0);
	return (rscsicmd(usalp, fd, "havebus", cbuf));
}

LOCAL int
rscsifileno(usalp, fd, busno, tgt, tlun)
	SCSI	*usalp;
	int	fd;
	int	busno;
	int	tgt;
	int	tlun;
{
	char	cbuf[3*CMD_SIZE];

	snprintf(cbuf, sizeof (cbuf), "T%d\n%d\n%d\n%d\n",
		busno,
		0,
		tgt,
		tlun);
	return (rscsicmd(usalp, fd, "fileno", cbuf));
}

LOCAL int
rscsiinitiator_id(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	return (rscsicmd(usalp, fd, "initiator id", "I\n"));
}

LOCAL int
rscsiisatapi(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	return (rscsicmd(usalp, fd, "isatapi", "A\n"));
}

LOCAL int
rscsireset(usalp, fd, what)
	SCSI	*usalp;
	int	fd;
	int	what;
{
	char	cbuf[CMD_SIZE];

	snprintf(cbuf, sizeof (cbuf), "R%d\n", what);
	return (rscsicmd(usalp, fd, "reset", cbuf));
}

LOCAL int
rscsiscmd(usalp, fd, sp)
	SCSI	*usalp;
	int	fd;
	struct usal_cmd  *sp;
{
	char	cbuf[1600];
	int	ret;
	int	amt = 0;
	int	voidsize = 0;

	ret = snprintf(cbuf, sizeof (cbuf), "S%d\n%d\n%d\n%d\n%d\n",
		sp->size, sp->flags,
		sp->cdb_len, sp->sense_len,
		sp->timeout);
	movebytes(sp->cdb.cmd_cdb, &cbuf[ret], sp->cdb_len);
	ret += sp->cdb_len;

	if ((sp->flags & SCG_RECV_DATA) == 0 && sp->size > 0) {
		amt = sp->size;
		if ((ret + amt) <= sizeof (cbuf)) {
			movebytes(sp->addr, &cbuf[ret], amt);
			ret += amt;
			amt = 0;
		}
	}
	errno = 0;
	if (_nixwrite(fd, cbuf, ret) != ret)
		rscsiaborted(usalp, fd);

	if (amt > 0) {
		if (_nixwrite(fd, sp->addr, amt) != amt)
			rscsiaborted(usalp, fd);
	}

	ret = rscsigetstatus(usalp, fd, "sendcmd");
	if (ret < 0)
		return (ret);

	sp->resid = sp->size - ret;
	sp->error = rscsireadnum(usalp, fd);
	sp->ux_errno = rscsireadnum(usalp, fd);
	*(Uchar *)&sp->scb = rscsireadnum(usalp, fd);
	sp->sense_count = rscsireadnum(usalp, fd);

	if (sp->sense_count > SCG_MAX_SENSE) {
		voidsize = sp->sense_count - SCG_MAX_SENSE;
		sp->sense_count = SCG_MAX_SENSE;
	}
	if (sp->sense_count > 0) {
		rscsireadbuf(usalp, fd, (char *)sp->u_sense.cmd_sense, sp->sense_count);
		rscsivoidarg(usalp, fd, voidsize);
	}

	if ((sp->flags & SCG_RECV_DATA) != 0 && ret > 0)
		rscsireadbuf(usalp, fd, sp->addr, ret);

	return (0);
}

LOCAL int
rscsifillrbuf(usalp)
	SCSI	*usalp;
{
	usallocal(usalp)->readbptr = usallocal(usalp)->readbuf;

	return (usallocal(usalp)->readbcnt =
			_niread(usallocal(usalp)->remfd,
			    usallocal(usalp)->readbuf, READBUF_SIZE));
}

LOCAL int
rscsirchar(usalp, cp)
	SCSI	*usalp;
	char	*cp;
{
	if (--(usallocal(usalp)->readbcnt) < 0) {
		if (rscsifillrbuf(usalp) <= 0)
			return (usallocal(usalp)->readbcnt);
		--(usallocal(usalp)->readbcnt);
	}
	*cp = *(usallocal(usalp)->readbptr)++;
	return (1);
}

LOCAL int
rscsireadbuf(usalp, fd, buf, count)
	SCSI	*usalp;
	int	fd;
	char	*buf;
	int	count;
{
	register int	n = count;
	register int	amt = 0;
	register int	cnt;

	if (usallocal(usalp)->readbcnt > 0) {
		cnt = usallocal(usalp)->readbcnt;
		if (cnt > n)
			cnt = n;
		movebytes(usallocal(usalp)->readbptr, buf, cnt);
		usallocal(usalp)->readbptr += cnt;
		usallocal(usalp)->readbcnt -= cnt;
		amt += cnt;
	}
	while (amt < n) {
		if ((cnt = _niread(fd, &buf[amt], n - amt)) <= 0) {
			return (rscsiaborted(usalp, fd));
		}
		amt += cnt;
	}
	return (amt);
}

LOCAL void
rscsivoidarg(usalp, fd, n)
	SCSI	*usalp;
	int	fd;
	register int	n;
{
	register int	i;
	register int	amt;
		char	buf[512];

	for (i = 0; i < n; i += amt) {
		amt = sizeof (buf);
		if ((n - i) < amt)
			amt = n - i;
		rscsireadbuf(usalp, fd, buf, amt);
	}
}

LOCAL int
rscsicmd(usalp, fd, name, cbuf)
	SCSI	*usalp;
	int	fd;
	char	*name;
	char	*cbuf;
{
	rscsisendcmd(usalp, fd, name, cbuf);
	return (rscsigetstatus(usalp, fd, name));
}

LOCAL void
rscsisendcmd(usalp, fd, name, cbuf)
	SCSI	*usalp;
	int	fd;
	char	*name;
	char	*cbuf;
{
	int	buflen = strlen(cbuf);

	errno = 0;
	if (_nixwrite(fd, cbuf, buflen) != buflen)
		rscsiaborted(usalp, fd);
}

LOCAL int
rscsigetline(usalp, fd, line, count)
	SCSI	*usalp;
	int	fd;
	char	*line;
	int	count;
{
	register char	*cp;

	for (cp = line; cp < &line[count]; cp++) {
		if (rscsirchar(usalp, cp) != 1)
			return (rscsiaborted(usalp, fd));

		if (*cp == '\n') {
			*cp = '\0';
			return (cp - line);
		}
	}
	return (rscsiaborted(usalp, fd));
}

LOCAL int
rscsireadnum(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	char	cbuf[CMD_SIZE];

	rscsigetline(usalp, fd, cbuf, sizeof (cbuf));
	return (atoi(cbuf));
}

LOCAL int
rscsigetstatus(usalp, fd, name)
	SCSI	*usalp;
	int	fd;
	char	*name;
{
	char	cbuf[CMD_SIZE];
	char	code;
	int	number;
	int	count;
	int	voidsize = 0;

	rscsigetline(usalp, fd, cbuf, sizeof (cbuf));
	code = cbuf[0];
	number = atoi(&cbuf[1]);

	if (code == 'E' || code == 'F') {
		rscsigetline(usalp, fd, cbuf, sizeof (cbuf));
		if (code == 'F')	/* should close file ??? */
			rscsiaborted(usalp, fd);

		rscsigetline(usalp, fd, cbuf, sizeof (cbuf));
		count = atoi(cbuf);
		if (count > 0) {
			if (usalp->errstr == NULL) {
				voidsize = count;
				count = 0;
			} else if (count > SCSI_ERRSTR_SIZE) {
				voidsize = count - SCSI_ERRSTR_SIZE;
				count = SCSI_ERRSTR_SIZE;
			}
			rscsireadbuf(usalp, fd, usalp->errstr, count);
			rscsivoidarg(usalp, fd, voidsize);
		}
		if (usalp->debug > 0)
			errmsgno(number, "Remote status(%s): %d '%s'.\n",
							name, number, cbuf);
		errno = number;
		return (-1);
	}
	if (code != 'A') {
		/* XXX Hier kommt evt Command not found ... */
		if (usalp->debug > 0)
			errmsgno(EX_BAD, "Protocol error (got %s).\n", cbuf);
		return (rscsiaborted(usalp, fd));
	}
	return (number);
}

LOCAL int
rscsiaborted(usalp, fd)
	SCSI	*usalp;
	int	fd;
{
	if ((usalp && usalp->debug > 0) || debug)
		errmsgno(EX_BAD, "Lost connection to remote host ??\n");
	/* if fd >= 0 */
	/* close file */
	if (errno == 0)
		errno = EIO;
	return (-1);
}

/*--------------------------------------------------------------------------*/
#ifdef	USE_RCMD_RSH
/*
 * If we make a separate file for libschily, we would need these include files:
 *
 * socketpair():	sys/types.h + sys/socket.h
 * dup2():		unixstd.h (hat auch sys/types.h)
 * strrchr():		strdefs.h
 *
 * and make sure that we use sigset() instead of signal() if possible.
 */
#include <waitdefs.h>
LOCAL int
_rcmdrsh(ahost, inport, locuser, remuser, cmd, rsh)
	char		**ahost;
	int		inport;		/* port is ignored */
	const char	*locuser;
	const char	*remuser;
	const char	*cmd;
	const char	*rsh;
{
	struct passwd	*pw;
	int	pp[2];
	int	pid;

	if (rsh == 0)
		rsh = "rsh";

	/*
	 * Verify that 'locuser' is present on local host.
	 */
	if ((pw = getpwnam(locuser)) == NULL) {
		errmsgno(EX_BAD, "Unknown user: %s\n", locuser);
		return (-1);
	}
	/* XXX Check the existence for 'ahost' here? */

	/*
	 * rcmd(3) creates a single socket to be used for communication.
	 * We need a bi-directional pipe to implement the same interface.
	 * On newer OS that implement bi-directional we could use pipe(2)
	 * but it makes no sense unless we find an OS that implements a
	 * bi-directional pipe(2) but no socketpair().
	 */
	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pp) == -1) {
		errmsg("Cannot create socketpair.\n");
		return (-1);
	}

	pid = fork();
	if (pid < 0) {
		return (-1);
	} else if (pid == 0) {
		const char	*p;
		const char	*av0;
		int		xpid;

		(void) close(pp[0]);
		if (dup2(pp[1], 0) == -1 ||	/* Pipe becomes 'stdin'  */
		    dup2(0, 1) == -1) {		/* Pipe becomes 'stdout' */

			errmsg("dup2 failed.\n");
			_exit(EX_BAD);
			/* NOTREACHED */
		}
		(void) close(pp[1]);		/* We don't need this anymore*/

		/*
		 * Become 'locuser' to tell the rsh program the local user id.
		 */
		if (getuid() != pw->pw_uid &&
		    setuid(pw->pw_uid) == -1) {
			errmsg("setuid(%lld) failed.\n",
							(Llong)pw->pw_uid);
			_exit(EX_BAD);
			/* NOTREACHED */
		}
		if (getuid() != geteuid() &&
#ifdef	HAVE_SETREUID
		    setreuid(-1, pw->pw_uid) == -1) {
#else
#ifdef	HAVE_SETEUID
		    seteuid(pw->pw_uid) == -1) {
#else
		    setuid(pw->pw_uid) == -1) {
#endif
#endif
			errmsg("seteuid(%lld) failed.\n",
							(Llong)pw->pw_uid);
			_exit(EX_BAD);
			/* NOTREACHED */
		}
		if (getuid() != geteuid() &&
		    seteuid(pw->pw_uid) == -1) {
			errmsg("seteuid(%lld) failed.\n",
							(Llong)pw->pw_uid);
			_exit(EX_BAD);
			/* NOTREACHED */
		}

		/*
		 * Fork again to completely detach from parent
		 * and avoid the need to wait(2).
		 */
		if ((xpid = fork()) == -1) {
			errmsg("rcmdsh: fork to lose parent failed.\n");
			_exit(EX_BAD);
			/* NOTREACHED */
		}
		if (xpid > 0) {
			_exit(0);
			/* NOTREACHED */
		}

		/*
		 * Always use remote shell programm (even for localhost).
		 * The client command may call getpeername() for security
		 * reasons and this would fail on a simple pipe.
		 */


		/*
		 * By default, 'rsh' handles terminal created signals
		 * but this is not what we like.
		 * For this reason, we tell 'rsh' to ignore these signals.
		 * Ignoring these signals is important to allow 'star' / 'sdd'
		 * to e.g. implement SIGQUIT as signal to trigger intermediate
		 * status printing.
		 *
		 * For now (late 2002), we know that the following programs
		 * are broken and do not implement signal handling correctly:
		 *
		 *	rsh	on SunOS-5.0...SunOS-5.9
		 *	ssh	from ssh.com
		 *	ssh	from openssh.org
		 *
		 * Sun already did accept a bug report for 'rsh'. For the ssh
		 * commands we need to send out bug reports. Meanwhile it could
		 * help to call setsid() if we are running under X so the ssh
		 * X pop up for passwd reading will work.
		 */
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
#ifdef	SIGTSTP
		signal(SIGTSTP, SIG_IGN); /* We would not be able to continue*/
#endif

		av0 = rsh;
		if ((p = strrchr(rsh, '/')) != NULL)
			av0 = ++p;
		execlp(rsh, av0, *ahost, "-l", remuser, cmd, (char *)NULL);

		errmsg("execlp '%s' failed.\n", rsh);
		_exit(EX_BAD);
		/* NOTREACHED */
	} else {
		(void) close(pp[1]);
		/*
		 * Wait for the intermediate child.
		 * The real 'rsh' program is completely detached from us.
		 */
		wait(0);
		return (pp[0]);
	}
	return (-1);	/* keep gcc happy */
}

#ifdef	HAVE_GETPPRIV
#include <priv.h>

LOCAL BOOL
ppriv_ok()
{
	priv_set_t	*privset;
	BOOL		net_privaddr = FALSE;


	if ((privset = priv_allocset()) == NULL) {
		return (FALSE);
	}
	if (getppriv(PRIV_EFFECTIVE, privset) == -1) {
		priv_freeset(privset);
		return (FALSE);
	}
	if (priv_ismember(privset, PRIV_NET_PRIVADDR)) {
		net_privaddr = TRUE;
	}
	priv_freeset(privset);

	return (net_privaddr);
}
#endif	/* HAVE_GETPPRIV */

#endif	/* USE_RCMD_RSH */

#endif	/* USE_REMOTE */
