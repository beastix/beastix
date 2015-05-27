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

/* @(#)rscsi.c	1.29 05/05/16 Copyright 1994,2000-2002 J. Schilling*/
/*
 *	Remote SCSI server
 *
 *	Copyright (c) 1994,2000-2002 J. Schilling
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

/*#define	FORCE_DEBUG*/

#include <mconfig.h>

#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>	/* includes <sys/types.h> */
#include <utypes.h>
#include <fctldefs.h>
#include <statdefs.h>
#include <strdefs.h>
#ifdef	HAVE_SYS_SOCKET_H
#define	USE_REMOTE
#include <sys/socket.h>
#endif
#ifdef	 HAVE_SYS_PARAM_H
#include <sys/param.h>	/* BSD-4.2 & Linux need this for MAXHOSTNAMELEN */
#endif
#include <errno.h>
#include <pwd.h>

#include <standard.h>
#include <deflts.h>
#include <patmatch.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsitransp.h>

#include <netinet/in.h>
#ifdef	HAVE_ARPA_INET_H
#include <arpa/inet.h>		/* BeOS does not have <arpa/inet.h> */
#endif				/* but inet_ntaoa() is in <netdb.h> */
#ifdef	HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef	USE_REMOTE
static	void	checkuser(void);
static	char	*getpeer(void);
static	BOOL	checktarget(void);
static	void	dorscsi(void);
static	void	scsiversion(void);
static	void	openscsi(void);
static	void	closescsi(void);
static	void	maxdma(void);
static	void	getbuf(void);
static	void	freebuf(void);
static	void	havebus(void);
static	void	scsifileno(void);
static	void	initiator_id(void);
static	void	isatapi(void);
static	void	scsireset(void);
static	void	sendcmd(void);

static	int	fillrdbuf(void);
static	int	readchar(char *cp);

static	void	readbuf(char *buf, int n);
static	void	voidarg(int n);
static	void	readarg(char *buf, int n);
static	char *preparebuffer(int size);
static	int	checkscsi(char *decive);
static	void	rscsirespond(int ret, int err);
static	void	rscsireply(int ret);
static	void	rscsierror(int err, char *str, char *xstr);

#define	CMD_SIZE	80

static	SCSI	*scsi_ptr = NULL;
static	char	*Sbuf;
static	long	Sbufsize;

static	char	*username;
static	char	*peername;

static	char	*debug_name;
static	FILE	*debug_file;

#define	DEBUG(fmt)		if (debug_file) fprintf(debug_file, fmt)
#define	DEBUG1(fmt,a)		if (debug_file) fprintf(debug_file, fmt, a)
#define	DEBUG2(fmt,a1,a2)	if (debug_file) fprintf(debug_file, fmt, a1, a2)
#define	DEBUG3(fmt,a1,a2,a3)	if (debug_file) fprintf(debug_file, fmt, a1, a2, a3)
#define	DEBUG4(fmt,a1,a2,a3,a4)	if (debug_file) fprintf(debug_file, fmt, a1, a2, a3, a4)
#define	DEBUG5(fmt,a1,a2,a3,a4,a5)	if (debug_file) fprintf(debug_file, fmt, a1, a2, a3, a4, a5)
#define	DEBUG6(fmt,a1,a2,a3,a4,a5,a6)	if (debug_file) fprintf(debug_file, fmt, a1, a2, a3, a4, a5, a6)
#endif	/* USE_REMOTE */

int
main(int argc, char *argv[])
{
	save_args(argc, argv);
#ifndef	USE_REMOTE
	comerrno(EX_BAD, "No remote SCSI support on this platform.\n");
#else
	argc--, argv++;
	if (argc > 0 && strcmp(*argv, "-c") == 0) {
		/*
		 * Skip params in case we have been installed as shell.
		 */
		argc--, argv++;
		argc--, argv++;
	}
	/*
	 * WARNING you are only allowed to change the defaults configuration
	 * filename if you also change the documentation and add a statement
	 * that makes clear where the official location of the file is, why you
	 * did choose a nonstandard location and that the nonstandard location
	 * only refers to inofficial rscsi versions.
	 *
	 * I was forced to add this because some people change cdrecord without
	 * rational reason and then publish the result. As those people
	 * don't contribute work and don't give support, they are causing extra
	 * work for me and this way slow down the development.
	 */
	if (cfg_open("/etc/netscsid.conf") < 0) {
		rscsierror(geterrno(), errmsgstr(geterrno()),
			"Remote configuration error: Cannot open /etc/netscsid.conf");
/*		rscsirespond(-1, geterrno());*/
		exit(EX_BAD);
	}
	debug_name=cfg_get("DEBUG");
#ifdef	FORCE_DEBUG
	if (debug_name == NULL && argc <= 0)
		debug_name = "/tmp/RSCSI";
#endif
#ifdef	NONONO
	/*
	 * Should we allow root to shoot himself into the foot?
	 * Allowing to write arbitrary files may be a security risk.
	 */
	if (argc > 0 && getuid() == 0)
		debug_name = *argv;
#endif

	/*
	 * XXX If someone sets up debugging and allows the debug file to be
	 * XXX replaced by a symlink to e.g. /etc/passwd this would be a
	 * XXX security risk. But /etc/rscsi.conf is only writable by root
	 * XXX and for this reason a possible security risk would have been
	 * XXX introduced by the administrator.
	 */
    if (debug_name != NULL) {
        /* Try to be careful when opening debug files, might be
         * created in an unsafe location 
         * */
        int fd = open(debug_name, O_CREAT | O_EXCL | O_TRUNC | O_RDWR, 0600);
        if (fd > -1) 
            debug_file = fdopen(fd, "w");
        else {
            rscsirespond(-1, geterrno());
            exit(EX_BAD);
        }
    } 
		
	if (argc > 0) {
		if (debug_file == 0) {
			rscsirespond(-1, geterrno());
			exit(EX_BAD);
		}
		(void) setbuf(debug_file, (char *)0);
	}
	checkuser();		/* Check if we are called by a bad guy	*/
	peername = getpeer();	/* Get host name of caller		*/
	dorscsi();
#endif	/* USE_REMOTE */
	return (0);
}

#ifdef	USE_REMOTE
static void
checkuser()
{
	uid_t	uid = getuid();
	char	*uname;
	struct passwd *pw;

	if (uid == 0) {
		username = "root";
		DEBUG("rscsid: user id 0, name root\n");
		return;
	}
	pw = getpwuid(uid);
	if (pw == NULL)
		goto notfound;

	username = pw->pw_name;
	DEBUG2("rscsid: user id %ld, name %s\n", (long)uid, username);

	cfg_restart();
	while ((uname = cfg_get_next("USER")) != NULL) {
		if (0==strcmp(username, uname))
			return;
	}
notfound:
	DEBUG2("rscsid: Illegal user '%s' id %ld for RSCSI server\n",
						username, (long)uid);
	rscsierror(0, "Illegal user id for RSCSI server", NULL);
	exit(EX_BAD);
}

#ifndef	NI_MAXHOST
#ifdef	MAXHOSTNAMELEN			/* XXX remove this and sys/param.h */
#define	NI_MAXHOST	MAXHOSTNAMELEN
#else
#define	NI_MAXHOST	64
#endif
#endif

static char *
getpeer()
{
#ifdef	HAVE_GETNAMEINFO
#ifdef	HAVE_SOCKADDR_STORAGE
	struct sockaddr_storage sa;
#else
	char			sa[256];
#endif
#else
	struct	sockaddr sa;
	struct hostent	*he;
#endif
	struct	sockaddr *sap;
	struct	sockaddr_in *s;
	socklen_t	 sasize = sizeof (sa);
static	char		buffer[NI_MAXHOST];

	sap = (struct  sockaddr *)&sa;
	if (getpeername(STDIN_FILENO, sap, &sasize) < 0) {
		int		errsav = geterrno();
		struct stat	sb;

		if (fstat(STDIN_FILENO, &sb) >= 0) {
			if (S_ISFIFO(sb.st_mode)) {
				DEBUG("rmt: stdin is a PIPE\n");
				return ("PIPE");
			}
			DEBUG1("rscsid: stdin st_mode %0llo\n", (Llong)sb.st_mode);
		}

		DEBUG1("rscsid: peername %s\n", errmsgstr(errsav));
		return ("ILLEGAL_SOCKET");
	} else {
		s = (struct sockaddr_in *)&sa;
#ifdef	AF_INET6
		if (s->sin_family != AF_INET && s->sin_family != AF_INET6) {
#else
		if (s->sin_family != AF_INET) {
#endif
#ifdef	AF_UNIX
			/*
			 * AF_UNIX is not defined on BeOS
			 */
			if (s->sin_family == AF_UNIX) {
				DEBUG("rmt: stdin is a PIPE (UNIX domain socket)\n");
				return ("PIPE");
			}
#endif
			DEBUG1("rmt: stdin NOT_IP socket (sin_family: %d)\n",
							s->sin_family);
			return ("NOT_IP");
		}
               
#ifdef	HAVE_GETNAMEINFO
		buffer[0] = '\0';
		if (debug_file &&
		    getnameinfo(sap, sasize, buffer, sizeof (buffer), NULL, 0,
		    NI_NUMERICHOST) == 0) {
			DEBUG1("rmt: peername %s\n", buffer);
		}
		buffer[0] = '\0';
		if (getnameinfo(sap, sasize, buffer, sizeof (buffer), NULL, 0,
		    0) == 0) {
			DEBUG1("rmt: peername %s\n", buffer);
			return (buffer);
		}
		return ("CANNOT_MAP_ADDRESS");
#else	/* HAVE_GETNAMEINFO */
#ifdef	HAVE_INET_NTOA
		(void) snprintf(buffer, sizeof(buffer), "%s", inet_ntoa(s->sin_addr));
#else
		(void) snprintf(buffer, sizeof(buffer), "%x", s->sin_addr.s_addr);
#endif
		DEBUG1("rscsid: peername %s\n", buffer);
		he = gethostbyaddr((char *)&s->sin_addr.s_addr, 4, AF_INET);
		DEBUG1("rscsid: peername %s\n", he!=NULL?he->h_name:buffer);
		if (he != NULL)
			return (he->h_name);
		return (buffer);
#endif	/* HAVE_GETNAMEINFO */
	}
}

static BOOL
checktarget()
{
	char	*target;
	char	*user;
	char	*host;
	char	*p;
	int	bus;
	int	chan;
	int	tgt;
	int	lun;

	if (peername == NULL)
		return (FALSE);
  cfg_restart();
	while ((target = cfg_get_next("ACCESS")) != NULL) {
		p = target;
		while (*p == '\t')
			p++;
		user = p;
		if ((p = strchr(p, '\t')) != NULL)
			*p++ = '\0';
		else
			continue;
		if (0!=strcmp(username, user))
			continue;

		while (*p == '\t')
			p++;
		host = p;
		if ((p = strchr(p, '\t')) != NULL)
			*p++ = '\0';
		else
			continue;
		if (0!=strcmp(peername, host))
			continue;

		p = astoi(p, &bus);
		if (*p != '\t')
			continue;
		p = astoi(p, &chan);
		if (*p != '\t')
			continue;
		p = astoi(p, &tgt);
		if (*p != '\t')
			continue;
		p = astoi(p, &lun);

		if (*p != '\t' && *p != '\n' && *p != '\r' && *p != '\0') 
			continue;
		DEBUG6("ACCESS %s %s %d.%d,%d,%d\n", user, host, bus, chan, tgt, lun);

		if (bus != -1 && bus != usal_scsibus(scsi_ptr))
			continue;
		if (tgt != -1 && tgt != usal_target(scsi_ptr))
			continue;
		if (lun != -1 && lun != usal_lun(scsi_ptr))
			continue;
		return (TRUE);
	}
	return (FALSE);
}

static void
dorscsi()
{
	char	c;

	while (readchar(&c) == 1) {
		seterrno(0);

		switch (c) {

		case 'V':		/* "V" ersion	*/
			scsiversion();
			break;
		case 'O':		/* "O" pen	*/
			openscsi();
			break;
		case 'C':		/* "C" lose	*/
			closescsi();
			break;
		case 'D':		/* "D" MA	*/
			maxdma();
			break;
		case 'M':		/* "M" alloc	*/
			getbuf();
			break;
		case 'F':		/* "F" free	*/
			freebuf();
			break;
		case 'B':		/* "B" us	*/
			havebus();
			break;
		case 'T':		/* "T" arget	*/
			scsifileno();
			break;
		case 'I':		/* "I" nitiator	*/
			initiator_id();
			break;
		case 'A':		/* "A" tapi	*/
			isatapi();
			break;
		case 'R':		/* "R" eset	*/
			scsireset();
			break;
		case 'S':		/* "S" end	*/
			sendcmd();
			break;

		default:
			DEBUG1("rscsid: garbage command '%c'\n", c);
			rscsierror(0, "Garbage command", NULL);
			exit(EX_BAD);
		}
	}
	exit(0);
}

static void
scsiversion()
{
	int	ret;
	char	*str;
	char	what[CMD_SIZE];

	readarg(what, sizeof(what));
	DEBUG1("rscsid: V %s\n", what);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	str = usal_version(scsi_ptr, atoi(what));
	ret = strlen(str);
	ret++;	/* Include null char */
	rscsirespond(ret, geterrno());
	_nixwrite(STDOUT_FILENO, str, ret);
}

static void
openscsi()
{
	char	device[CMD_SIZE];
	char	errstr[80];
	int	debug = 0;
	int	lverbose = 0;
	int	ret = 0;
	char	rbuf[1600];

	if (scsi_ptr != NULL)
		(void) usal_close(scsi_ptr);

	readarg(device, sizeof(device));
	DEBUG1("rscsid: O %s\n", device);
	if (strncmp(device, "REMOTE", 6) == 0) {
		scsi_ptr = NULL;
		seterrno(EINVAL);
	} else if (!checkscsi(device)) {
		scsi_ptr = NULL;
		seterrno(EACCES);
	} else {
		scsi_ptr = usal_open(device, errstr, sizeof(errstr), debug, lverbose);
		if (scsi_ptr == NULL) {
			ret = -1;
		} else {
			scsi_ptr->silent = 1;
			scsi_ptr->verbose = 0;
			scsi_ptr->debug = 0;
			scsi_ptr->kdebug = 0;
		}
	}
	if (ret < 0) {
		/*
		 * XXX This is currently the only place where we use the
		 * XXX extended error string.
		 */
		rscsierror(geterrno(), errmsgstr(geterrno()), errstr);
/*		rscsirespond(ret, geterrno());*/
		return;
	}
	DEBUG4("rscsid:>A 0 %d.%d,%d,%d\n", 
		usal_scsibus(scsi_ptr),
		0,
		usal_target(scsi_ptr),
		usal_lun(scsi_ptr));

	ret = snprintf(rbuf, sizeof(rbuf), "A0\n%d\n%d\n%d\n%d\n",
		usal_scsibus(scsi_ptr),
		0,
		usal_target(scsi_ptr),
		usal_lun(scsi_ptr));
	(void) _nixwrite(STDOUT_FILENO, rbuf, ret);
}

static void
closescsi()
{
	int	ret;
	char	device[CMD_SIZE];

	readarg(device, sizeof(device));
	DEBUG1("rscsid: C %s\n", device);
	ret = usal_close(scsi_ptr);
	rscsirespond(ret, geterrno());
	scsi_ptr = NULL;
}

static void
maxdma()
{
	int	ret;
	char	amt[CMD_SIZE];

	readarg(amt, sizeof(amt));
	DEBUG1("rscsid: D %s\n", amt);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	ret = usal_bufsize(scsi_ptr, atol(amt));
	rscsirespond(ret, geterrno());
}

static void
getbuf()
{
	int	ret = 0;
	char	amt[CMD_SIZE];

	readarg(amt, sizeof(amt));
	DEBUG1("rscsid: M %s\n", amt);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	ret = usal_bufsize(scsi_ptr, atol(amt));
	if (preparebuffer(ret) == NULL)
		ret = -1;
	rscsirespond(ret, geterrno());
}

static void
freebuf()
{
	int	ret = 0;
	char	dummy[CMD_SIZE];

	readarg(dummy, sizeof(dummy));
	DEBUG1("rscsid: F %s\n", dummy);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	usal_freebuf(scsi_ptr);
	Sbuf = NULL;
	rscsirespond(ret, geterrno());
}

static void
havebus()
{
	int	ret;
	char	bus[CMD_SIZE];
	char	chan[CMD_SIZE];

	readarg(bus, sizeof(bus));
	readarg(chan, sizeof(chan));
	DEBUG2("rscsid: B %s.%s\n", bus, chan);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	ret = usal_havebus(scsi_ptr, atol(bus));
	rscsirespond(ret, geterrno());
}

static void
scsifileno()
{
	int	ret;
	char	bus[CMD_SIZE];
	char	chan[CMD_SIZE];
	char	tgt[CMD_SIZE];
	char	lun[CMD_SIZE];

	readarg(bus, sizeof(bus));
	readarg(chan, sizeof(chan));
	readarg(tgt, sizeof(tgt));
	readarg(lun, sizeof(lun));
	DEBUG4("rscsid: T %s.%s,%s,%s\n", bus, chan, tgt, lun);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	seterrno(0);
	ret = usal_settarget(scsi_ptr, atoi(bus), atoi(tgt), atoi(lun));
	if (!checktarget()) {
		usal_settarget(scsi_ptr, -1, -1, -1);
		ret = -1;
	}
	if (geterrno() != 0)
		rscsirespond(ret, geterrno());
	else
		rscsireply(ret);
}

static void
initiator_id()
{
	int	ret;
	char	dummy[CMD_SIZE];

	readarg(dummy, sizeof(dummy));
	DEBUG1("rscsid: I %s\n", dummy);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	seterrno(0);
	ret = usal_initiator_id(scsi_ptr);
	if (geterrno() != 0)
		rscsirespond(ret, geterrno());
	else
		rscsireply(ret);
}

static void
isatapi()
{
	int	ret;
	char	dummy[CMD_SIZE];

	readarg(dummy, sizeof(dummy));
	DEBUG1("rscsid: A %s\n", dummy);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	seterrno(0);
	ret = usal_isatapi(scsi_ptr);
	if (geterrno() != 0)
		rscsirespond(ret, geterrno());
	else
		rscsireply(ret);
}

static void
scsireset()
{
	int	ret;
	char	what[CMD_SIZE];

	readarg(what, sizeof(what));
	DEBUG1("rscsid: R %s\n", what);
	if (scsi_ptr == NULL) {
		rscsirespond(-1, EBADF);
		return;
	}
	ret = usal_reset(scsi_ptr, atoi(what));
	rscsirespond(ret, geterrno());
}

static void
sendcmd()
{
	register struct	usal_cmd	*scmd;
	int	n;
	int	ret;
	char	count[CMD_SIZE];
	char	flags[CMD_SIZE];
	char	cdb_len[CMD_SIZE];
	char	sense_len[CMD_SIZE];
	char	timeout[CMD_SIZE];
	int	csize;
	int	cflags;
	int	clen;
	int	ctimeout;
	char	rbuf[1600];
	char	*p;

	/*
	 *	S count\n
	 *	flags\n
	 *	cdb_len\n
	 *	sense_len\n
	 *	timeout\n
	 *	<data if available>
	 *
	 *	Timeout:
	 *	-	sss	(e.g. 10)
	 *	-	sss.uuu	(e.g. 10.23)
	 */
	readarg(count, sizeof(count));
	readarg(flags, sizeof(flags));
	readarg(cdb_len, sizeof(cdb_len));
	readarg(sense_len, sizeof(sense_len));
	readarg(timeout, sizeof(timeout));
	DEBUG5("rscsid: S %s %s %s %s %s", count, flags, cdb_len, sense_len, timeout);
	csize = atoi(count);
	cflags = atoi(flags);
	clen = atoi(cdb_len);

	p = strchr(timeout, '.');
	if (p)
		*p = '\0';
	ctimeout = atoi(timeout);

	if (scsi_ptr == NULL || clen > SCG_MAX_CMD || csize > Sbufsize) {
		DEBUG("\n");
		voidarg(clen);
		if ((cflags & SCG_RECV_DATA) == 0 && csize > 0)
			voidarg(csize);
		rscsirespond(-1, scsi_ptr==NULL ? EBADF : EINVAL);
		return;
	}

	scmd = scsi_ptr->scmd;
	fillbytes((caddr_t)scmd, sizeof(*scmd), '\0');
	scmd->addr = (caddr_t)Sbuf;
	scmd->size = csize;
	scmd->flags = cflags;
	scmd->cdb_len = clen;
	scmd->sense_len = atoi(sense_len);
	scmd->timeout = ctimeout;
	readbuf((char *)scmd->cdb.cmd_cdb, clen);
	DEBUG6(" 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		scmd->cdb.cmd_cdb[0],
		scmd->cdb.cmd_cdb[1],
		scmd->cdb.cmd_cdb[2],
		scmd->cdb.cmd_cdb[3],
		scmd->cdb.cmd_cdb[4],
		scmd->cdb.cmd_cdb[5]);

	if ((cflags & SCG_RECV_DATA) == 0 && csize > 0)
		readbuf(Sbuf, scmd->size);

	scsi_ptr->cmdname = "";

	ret = usal_cmd(scsi_ptr);

	n = 0;
	if ((csize - scmd->resid) > 0)
		n = csize - scmd->resid;

	/*
	 *	A count\n
	 *	error\n
	 *	errno\n
	 *	scb\n
	 *	sense_count\n
	 *	<data if available>
	 */
	DEBUG5("rscsid:>A %d %d %d %d %d\n",
		n,
		scmd->error,
		scmd->ux_errno,
		*(Uchar *)&scmd->scb,
		scmd->sense_count);

	ret = snprintf(rbuf, sizeof(rbuf), "A%d\n%d\n%d\n%d\n%d\n",
		n,
		scmd->error,
		scmd->ux_errno,
		*(Uchar *)&scmd->scb,
		scmd->sense_count);

	if (scmd->sense_count > 0) {
		movebytes(scmd->u_sense.cmd_sense, &rbuf[ret], scmd->sense_count);
		ret += scmd->sense_count;
	}
	if ((cflags & SCG_RECV_DATA) == 0)
		n = 0;
	if (n > 0 && ((ret + n) <= sizeof(rbuf))) {
		movebytes(Sbuf, &rbuf[ret], n);
		ret += n;
		n = 0;
	}
	(void) _nixwrite(STDOUT_FILENO, rbuf, ret);

	if (n > 0)
		(void) _nixwrite(STDOUT_FILENO, Sbuf, n);
}

#define	READB_SIZE	128
static	char		readb[READB_SIZE];
static	char		*readbptr;
static	int		readbcnt;

static int
fillrdbuf()
{
	readbptr = readb;

	return (readbcnt = _niread(STDIN_FILENO, readb, READB_SIZE));
}

static int
readchar(char *cp)
{
	if (--readbcnt < 0) {
		if (fillrdbuf() <= 0)
			return (readbcnt);
		--readbcnt;
	}
	*cp = *readbptr++;
	return (1);
}

static void
readbuf(register char *buf, register int n)
{
	register int	i = 0;
	register int	amt;

	if (readbcnt > 0) {
		amt = readbcnt;
		if (amt > n)
			amt = n;
		movebytes(readbptr, buf, amt);
		readbptr += amt;
		readbcnt -= amt;
		i += amt;
	}

	for (; i < n; i += amt) {
		amt = _niread(STDIN_FILENO, &buf[i], n - i);
		if (amt <= 0) {
			DEBUG("rscsid: premature eof\n");
			rscsierror(0, "Premature eof", NULL);
			exit(EX_BAD);
		}
	}
}

static void
voidarg(register int n)
{
	register int	i;
	register int	amt;
		 char	buf[512];

	for (i = 0; i < n; i += amt) {
		amt = sizeof(buf);
		if ((n - i) < amt)
			amt = n - i;
		readbuf(buf, amt);
	}
}

static void
readarg(char *buf, int n)
{
	int	i;

	for (i = 0; i < n; i++) {
		if (readchar(&buf[i]) != 1)
			exit(0);
		if (buf[i] == '\n')
			break;
	}
	buf[i] = '\0';
}

static char *
preparebuffer(int size)
{
	Sbufsize = size;
	if ((Sbuf = usal_getbuf(scsi_ptr, Sbufsize)) == NULL) {
		Sbufsize = 0L;
		return (Sbuf);
	}
	size = Sbufsize + 1024;	/* Add protocol overhead */

#ifdef	SO_SNDBUF
	while (size > 512 &&
	       setsockopt(STDOUT_FILENO, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof (size)) < 0)
		size -= 512;
	DEBUG1("rscsid: sndsize: %d\n", size);
#endif
#ifdef	SO_RCVBUF
	while (size > 512 &&
	       setsockopt(STDIN_FILENO, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof (size)) < 0)
		size -= 512;
	DEBUG1("rscsid: rcvsize: %d\n", size);
#endif
	return (Sbuf);
}

static int
checkscsi(char *device)
{
#ifdef	CHECKTAPE
	if (strncmp(device, "/dev/rst", 8) == 0 ||
	    strncmp(device, "/dev/nrst", 9) == 0 ||
	    strcmp(device, "/dev/zero") == 0 ||
	    strcmp(device, "/dev/null") == 0)
		return (1);
	return (0);
#else
	return (1);
#endif
}

static void
rscsirespond(int ret, int err)
{
	if (ret < 0) {
		rscsierror(err, errmsgstr(err), NULL);
	} else {
		rscsireply(ret);
	}
}

static void
rscsireply(int ret)
{
	char	rbuf[CMD_SIZE];

	DEBUG1("rscsid:>A %d\n", ret);
	(void) snprintf(rbuf, sizeof(rbuf), "A%d\n", ret);
	(void) _nixwrite(STDOUT_FILENO, rbuf, strlen(rbuf));
}

static void
rscsierror(int err, char *str, char *xstr)
{
	char	rbuf[1600];
	int	xlen = 0;
	int	n;

	if (xstr != NULL)
		xlen = strlen(xstr) + 1;

	DEBUG3("rscsid:>E %d (%s) [%s]\n", err, str, xstr?xstr:"");
	n = snprintf(rbuf, sizeof(rbuf), "E%d\n%s\n%d\n", err, str, xlen);

	if (xlen > 0 && ((xlen + n) <= sizeof(rbuf))) {
		movebytes(xstr, &rbuf[n], xlen);
		n += xlen;
		xlen = 0;
	}
	(void) _nixwrite(STDOUT_FILENO, rbuf, n);
	if (xlen > 0)
		(void) _nixwrite(STDOUT_FILENO, xstr, xlen);
}
#endif	/* USE_REMOTE */
