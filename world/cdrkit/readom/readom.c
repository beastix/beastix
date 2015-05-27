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

/* @(#)readcd.c	1.80 06/02/05 Copyright 1987, 1995-2006 J. Schilling */
/*
 *	Skeleton for the use of the usal genearal SCSI - driver
 *
 *	Copyright (c) 1987, 1995-2004 J. Schilling
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
#include <unixstd.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <fctldefs.h>
#include <timedefs.h>
#include <signal.h>
#include <schily.h>
#ifdef	HAVE_PRIV_H
#include <priv.h>
#endif

#ifdef	NEED_O_BINARY
#include <io.h>					/* for setmode() prototype */
#endif

#include <usal/usalcmd.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsi_scan.h"
#include "scsimmc.h"
#define	qpto96	__nothing__
#include "wodim.h"
#include "defaults.h"
#undef	qpto96
#include "movesect.h"

char	cdr_version[] = "2.01.01a05";

#if	defined(PROTOTYPES)
#define	UINT_C(a)	(a##u)
#define	ULONG_C(a)	(a##ul)
#define	USHORT_C(a)	(a##uh)
#define	CONCAT(a, b)	a##b
#else
#define	UINT_C(a)	((unsigned)(a))
#define	ULONG_C(a)	((unsigned long)(a))
#define	USHORT_C(a)	((unsigned short)(a))
/* CSTYLED */
#define	CONCAT(a, b)	a/**/b
#endif

extern	BOOL	getlong(char *, long *, long, long);
extern	BOOL	getint(char *, int *, int, int);

typedef struct {
	long	start;
	long	end;
	long	sptr;		/* sectors per transfer */
	BOOL	askrange;
	char	*name;
} parm_t;

typedef struct {
	int	errors;
	int	c2_errors;
	int	c2_maxerrs;
	int	c2_errsecs;
	int	c2_badsecs;
	int	secsize;
	BOOL	ismmc;
} rparm_t;

struct exargs {
	SCSI	*usalp;
	int	old_secsize;
	int	flags;
	int	exflags;
	char	oerr[3];
} exargs;

BOOL	cvt_cyls(void);
BOOL	cvt_bcyls(void);
void	print_defect_list(void);
static	void	usage(int ret);
static	void	intr(int sig);
static	void	exscsi(int excode, void *arg);
static	void	excdr(int excode, void *arg);
static	int	prstats(void);
static	int	prstats_silent(void);
static	void	dorw(SCSI *usalp, char *filename, char *sectors);
static	void	doit(SCSI *usalp);
static	void	read_disk(SCSI *usalp, parm_t *parmp);
#ifdef	CLONE_WRITE
static	void	readcd_disk(SCSI *usalp, parm_t *parmp);
static	void	read_lin(SCSI *usalp, parm_t *parmp);
static	int	read_secheader(SCSI *usalp, long addr);
static	int	read_ftoc(SCSI *usalp, parm_t *parmp, BOOL do_sectype);
static	void	read_sectypes(SCSI *usalp, FILE *f);
static	void	get_sectype(SCSI *usalp, long addr, char *st);
#endif

static	void	readc2_disk(SCSI *usalp, parm_t *parmp);
static	int	fread_data(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
								  int cnt);
#ifdef	CLONE_WRITE
static	int	fread_2448(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
								  int cnt);
static	int	fread_2448_16(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
									  int cnt);
static	int	fread_2352(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
								  int cnt);
static	int	fread_lin(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
								 int cnt);
#endif
static	int	bits(int c);
static	int	bitidx(int c);
static	int	fread_c2(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, 
								int cnt);

static	int	fdata_null(rparm_t *rp, caddr_t bp, long addr, int cnt);
static	int	fdata_c2(rparm_t *rp, caddr_t bp, long addr, int cnt);

#ifdef	used
static	int read_scsi_g1(SCSI *usalp, caddr_t bp, long addr, int cnt);
#endif

int	write_scsi(SCSI *usalp, caddr_t bp, long addr, int cnt);
int	write_g0(SCSI *usalp, caddr_t bp, long addr, int cnt);
int	write_g1(SCSI *usalp, caddr_t bp, long addr, int cnt);

#ifdef	used
static	void	Xrequest_sense(SCSI *usalp);
#endif
static	int	read_retry(SCSI *usalp, caddr_t bp, long addr, long cnt,
								  int (*rfunc)(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt),
								  rparm_t *rp);
static	void	read_generic(SCSI *usalp, parm_t *parmp,
									 int (*rfunc)(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt),
									 rparm_t *rp,
									 int (*dfunc)(rparm_t *rp, caddr_t bp, long addr, int cnt));
static	void	write_disk(SCSI *usalp, parm_t *parmp);
static	int	choice(int n);
static	void	ra(SCSI *usalp);

int	read_da(SCSI *usalp, caddr_t bp, long addr, int cnt, int framesize, 
				  int subcode);
int	read_cd(SCSI *usalp, caddr_t bp, long addr, int cnt, int framesize, 
				  int data, int subch);

static	void	oldmode(SCSI *usalp, int *errp, int *retrp);
static	void	domode(SCSI *usalp, int err, int retr);

static	void	qpto96(Uchar *sub, Uchar *subq, int dop);
static	void	ovtime(SCSI *usalp);
static	void	add_bad(long addr);
static	void	print_bad(void);

struct timeval	starttime;
struct timeval	stoptime;
int	didintr;
int	exsig;

char	*Sbuf;
long	Sbufsize;

/*#define	MAX_RETRY	32*/
#define	MAX_RETRY	128

int	help;
int	xdebug;
int	lverbose;
int	quiet;
BOOL	is_suid;
BOOL	is_cdrom;
BOOL	is_dvd;
BOOL	do_write;
BOOL	c2scan;
BOOL	fulltoc;
BOOL	clonemode;
BOOL	noerror;
BOOL	nocorr;
BOOL	notrunc;
int	retries = MAX_RETRY;
int	maxtry = 0;
int	meshpoints;
BOOL	do_factor;

struct	scsi_format_data fmt;

/*XXX*/EXPORT	BOOL cvt_cyls(void) { return (FALSE); }
/*XXX*/EXPORT	BOOL cvt_bcyls(void) { return (FALSE); }
/*XXX*/EXPORT	void print_defect_list(void) {}

static void
usage(int ret)
{
	fprintf(stderr, "Usage:\treadom [options]\n");
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\t-version	print version information and exit\n");
	fprintf(stderr, "\tdev=target	SCSI target to use\n");
	fprintf(stderr, "\tf=filename	Name of file to read/write\n");
	fprintf(stderr, "\tsectors=range	Range of sectors to read/write\n");
	fprintf(stderr, "\tspeed=#		set speed of drive (MMC only)\n");
	fprintf(stderr, "\tts=#		set maximum transfer size for a single SCSI command\n");
	fprintf(stderr, "\t-w		Switch to write mode\n");
	fprintf(stderr, "\t-c2scan		Do a C2 error scan\n");
#ifdef	CLONE_WRITE
	fprintf(stderr, "\t-fulltoc	Retrieve the full TOC\n");
	fprintf(stderr, "\t-clone		Retrieve the full TOC and all data\n");
#endif
	fprintf(stderr, "\ttimeout=#	set the default SCSI command timeout to #.\n");
	fprintf(stderr, "\tdebug=#,-d	Set to # or increment misc debug level\n");
	fprintf(stderr, "\tkdebug=#,kd=#	do Kernel debugging\n");
	fprintf(stderr, "\t-quiet,-q	be more quiet in error retry mode\n");
	fprintf(stderr, "\t-verbose,-v	increment general verbose level by one\n");
	fprintf(stderr, "\t-Verbose,-V	increment SCSI command transport verbose level by one\n");
	fprintf(stderr, "\t-silent,-s	do not print status of failed SCSI commands\n");
	fprintf(stderr, "\t-scanbus	scan the SCSI bus and exit\n");
	fprintf(stderr, "\t-noerror	do not abort on error\n");
#ifdef	CLONE_WRITE
	fprintf(stderr, "\t-nocorr		do not apply error correction in drive\n");
#endif
	fprintf(stderr, "\t-notrunc	do not truncate outputfile in read mode\n");
	fprintf(stderr, "\tretries=#	set retry count (default is %d)\n", retries);
	fprintf(stderr, "\t-overhead	meter SCSI command overhead times\n");
	fprintf(stderr, "\tmeshpoints=#	print read-speed at # locations\n");
	fprintf(stderr, "\t-factor		try to use speed factor with meshpoints=# if possible\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "sectors=0-0 will read nothing, sectors=0-1 will read one sector starting from 0\n");
	exit(ret);
}	

/* CSTYLED */
char	opts[]   = "debug#,d+,kdebug#,kd#,timeout#,quiet,q,verbose+,v+,Verbose+,V+,x+,xd#,silent,s,help,h,version,scanbus,dev*,sectors*,w,c2scan,fulltoc,clone,noerror,nocorr,notrunc,retries#,factor,f*,speed#,ts&,overhead,meshpoints#";

int
main(int argc, char *argv[])
{
	char	*dev = NULL;
	int	fcount;
	int	cac;
	char	* const *cav;
	int	scsibus	= -1;
	int	target	= -1;
	int	lun	= -1;
	int	silent	= 0;
	int	verbose	= 0;
	int	kdebug	= 0;
	int	debug	= 0;
	int	deftimeout = 40;
	int	pversion = 0;
	int	scanbus = 0;
	int	speed	= -1;
	int	dooverhead = 0;
	SCSI	*usalp;
	char	*filename = NULL;
	char	*sectors = NULL;

	save_args(argc, argv);

	cac = --argc;
	cav = ++argv;

	if (getallargs(&cac, &cav, opts,
			&debug, &debug,
			&kdebug, &kdebug,
			&deftimeout,
			&quiet, &quiet,
			&lverbose, &lverbose,
			&verbose, &verbose,
			&xdebug, &xdebug,
			&silent, &silent,
			&help, &help, &pversion,
			&scanbus, &dev, &sectors, &do_write,
			&c2scan,
			&fulltoc, &clonemode,
			&noerror, &nocorr,
			&notrunc, &retries, &do_factor, &filename,
			&speed, getnum, &Sbufsize,
			&dooverhead, &meshpoints) < 0) {
		errmsgno(EX_BAD, "Bad flag: %s.\n", cav[0]);
		usage(EX_BAD);
	}
	if (help)
		usage(0);
	if (pversion) {
		printf("readcd %s is not what you see here. This line is only a fake for too clever\n"
				"GUIs and other frontend applications. In fact, this program is:\n", cdr_version);

		printf("readom " CDRKIT_VERSION " (" HOST_SYSTEM ")\n"
				"Copyright (C) 1987, 1995-2006 Joerg Schilling\n"
				"Copyright (C) 2006 Cdrkit maintainers\n"
				"(modified version of <censored> -- "
				"don't bother Joerg Schilling with problems)\n");
		exit(0);
	}

	fcount = 0;
	cac = argc;
	cav = argv;

	while (getfiles(&cac, &cav, opts) > 0) {
		fcount++;
		if (fcount == 1) {
			if (*astoi(cav[0], &target) != '\0') {
				errmsgno(EX_BAD,
					"Target '%s' is not a Number.\n",
								cav[0]);
				usage(EX_BAD);
				/* NOTREACHED */
			}
		}
		if (fcount == 2) {
			if (*astoi(cav[0], &lun) != '\0') {
				errmsgno(EX_BAD,
					"Lun is '%s' not a Number.\n",
								cav[0]);
				usage(EX_BAD);
				/* NOTREACHED */
			}
		}
		if (fcount == 3) {
			if (*astoi(cav[0], &scsibus) != '\0') {
				errmsgno(EX_BAD,
					"Scsibus is '%s' not a Number.\n",
								cav[0]);
				usage(EX_BAD);
				/* NOTREACHED */
			}
		}
		cac--;
		cav++;
	}
/*fprintf(stderr, "dev: '%s'\n", dev);*/
	if (!scanbus)
		cdr_defaults(&dev, NULL, NULL, NULL);
	if (debug) {
		printf("dev: '%s'\n", dev);
	}
	if (!scanbus && dev == NULL &&
	    scsibus == -1 && (target == -1 || lun == -1)) {
		errmsgno(EX_BAD, "No SCSI device specified.\n");
		usage(EX_BAD);
	}
	if (dev || scanbus) {
		char	errstr[80];

		/*
		 * Call usal_remote() to force loading the remote SCSI transport
		 * library code that is located in librusal instead of the dummy
		 * remote routines that are located inside libusal.
		 */
		usal_remote();
		if (dev != NULL &&
		    ((strncmp(dev, "HELP", 4) == 0) ||
		    (strncmp(dev, "help", 4) == 0))) {
			usal_help(stderr);
			exit(0);
		}
		if ((usalp = usal_open(dev, errstr, sizeof (errstr), debug, lverbose)) == (SCSI *)0) {
			int	err = geterrno();

			errmsgno(err, "%s%sCannot open SCSI driver.\n", errstr, errstr[0]?". ":"");
			errmsgno(EX_BAD, "For possible targets try 'wodim -scanbus'.%s\n",
						geteuid() ? " Make sure you are root.":"");
			errmsgno(EX_BAD, "For possible transport specifiers try 'wodim dev=help'.\n");
			exit(err);
		}
	} else {
		if (scsibus == -1 && target >= 0 && lun >= 0)
			scsibus = 0;

		usalp = usal_smalloc();
		usalp->debug = debug;
		usalp->kdebug = kdebug;

		usal_settarget(usalp, scsibus, target, lun);
		if (usal__open(usalp, NULL) <= 0)
			comerr("Cannot open SCSI driver.\n");
	}
	usalp->silent = silent;
	usalp->verbose = verbose;
	usalp->debug = debug;
	usalp->kdebug = kdebug;
	usal_settimeout(usalp, deftimeout);

	if (Sbufsize == 0)
		Sbufsize = 256*1024L;
	Sbufsize = usal_bufsize(usalp, Sbufsize);
	if ((Sbuf = usal_getbuf(usalp, Sbufsize)) == NULL)
		comerr("Cannot get SCSI I/O buffer.\n");

#ifdef	HAVE_PRIV_SET
	is_suid = priv_ineffect(PRIV_FILE_DAC_READ) &&
		    !priv_ineffect(PRIV_PROC_SETID);
	/*
	 * Give up privs we do not need anymore.
	 * We no longer need:
	 *	file_dac_read,net_privaddr
	 * We still need:
	 *	sys_devices
	 */
	priv_set(PRIV_OFF, PRIV_EFFECTIVE,
		PRIV_FILE_DAC_READ, PRIV_NET_PRIVADDR, NULL);
	priv_set(PRIV_OFF, PRIV_PERMITTED,
		PRIV_FILE_DAC_READ, PRIV_NET_PRIVADDR, NULL);
	priv_set(PRIV_OFF, PRIV_INHERITABLE,
		PRIV_FILE_DAC_READ, PRIV_NET_PRIVADDR, PRIV_SYS_DEVICES, NULL);
#endif
	/*
	 * This is only for OS that do not support fine grained privs.
	 */
	if (!is_suid)
		is_suid = geteuid() != getuid();
	/*
	 * We don't need root privilleges anymore.
	 */
#ifdef	HAVE_SETREUID
	if (setreuid(-1, getuid()) < 0)
#else
#ifdef	HAVE_SETEUID
	if (seteuid(getuid()) < 0)
#else
	if (setuid(getuid()) < 0)
#endif
#endif
		comerr("Panic cannot set back effective uid.\n");

	/* code to use SCG */

	if (scanbus) {
		select_target(usalp, stdout);
		exit(0);
	}
	do_inquiry(usalp, FALSE);
	allow_atapi(usalp, TRUE);    /* Try to switch to 10 byte mode cmds */
	if (is_mmc(usalp, NULL, NULL)) {
		int	rspeed;
		int	wspeed;
		/*
		 * At this point we know that we have a SCSI-3/mmc compliant drive.
		 * Unfortunately ATAPI drives violate the SCSI spec in returning
		 * a response data format of '1' which from the SCSI spec would
		 * tell us not to use the "PF" bit in mode select. As ATAPI drives
		 * require the "PF" bit to be set, we 'correct' the inquiry data.
		 */
		if (usalp->inq->data_format < 2)
			usalp->inq->data_format = 2;

		if ((rspeed = get_curprofile(usalp)) >= 0) {
			if (rspeed >= 0x08 && rspeed < 0x10)
				is_cdrom = TRUE;
			if (rspeed >= 0x10 && rspeed < 0x20)
				is_dvd = TRUE;
		} else {
			BOOL	dvd;

			mmc_check(usalp, NULL, NULL, NULL, NULL, &dvd, NULL);
			if (dvd == FALSE) {
				is_cdrom = TRUE;
			} else {
				char	xb[32];

				if (read_dvd_structure(usalp, (caddr_t)xb, 32, 0, 0, 0) >= 0) {
				/*
				 * If read DVD structure is supported and works, then
				 * we must have a DVD media in the drive. Signal to
				 * use the DVD driver.
				 */
					is_dvd = TRUE;
				} else {
					is_cdrom = TRUE;
				}
			}
		}

		if (speed > 0)
			speed *= 177;
		if (speed > 0xFFFF || speed < 0)
			speed = 0xFFFF;
		scsi_set_speed(usalp, speed, speed, ROTCTL_CLV);
		if (scsi_get_speed(usalp, &rspeed, &wspeed) >= 0) {
			fprintf(stderr, "Read  speed: %5d kB/s (CD %3dx, DVD %2dx).\n",
				rspeed, rspeed/176, rspeed/1385);
			fprintf(stderr, "Write speed: %5d kB/s (CD %3dx, DVD %2dx).\n",
				wspeed, wspeed/176, wspeed/1385);
		}
	}
	exargs.usalp	   = usalp;
	exargs.old_secsize = -1;
/*	exargs.flags	   = flags;*/
	exargs.oerr[2]	   = 0;

	/*
	 * Install exit handler before we change the drive status.
	 */
	on_comerr(exscsi, &exargs);
	signal(SIGINT, intr);
	signal(SIGTERM, intr);

	if (dooverhead) {
		ovtime(usalp);
		comexit(0);
	}

	if (is_suid) {
		if (usalp->inq->type != INQ_ROMD)
			comerrno(EX_BAD, "Not root. Will only work on CD-ROM in suid/priv mode\n");
	}

	if (filename || sectors || c2scan || meshpoints || fulltoc || clonemode) {
		dorw(usalp, filename, sectors);
	} else {
		doit(usalp);
	}
	comexit(0);
	return (0);
}

/*
 * XXX Leider kann man vim Signalhandler keine SCSI Kommandos verschicken
 * XXX da meistens das letzte SCSI Kommando noch laeuft.
 * XXX Eine Loesung waere ein Abort Callback in SCSI *.
 */
static void
intr(int sig)
{
	didintr++;
	exsig = sig;
/*	comexit(sig);*/
}

/* ARGSUSED */
static void
exscsi(int excode, void *arg)
{
	struct exargs	*exp = (struct exargs *)arg;
		int	i;

	/*
	 * Try to restore the old sector size.
	 */
	if (exp != NULL && exp->exflags == 0) {
		for (i = 0; i < 10*100; i++) {
			if (!exp->usalp->running)
				break;
			if (i == 10) {
				errmsgno(EX_BAD,
					"Waiting for current SCSI command to finish.\n");
			}
			usleep(100000);
		}

		if (!exp->usalp->running) {
			if (exp->oerr[2] != 0) {
				domode(exp->usalp, exp->oerr[0], exp->oerr[1]);
			}
			if (exp->old_secsize > 0 && exp->old_secsize != 2048)
				select_secsize(exp->usalp, exp->old_secsize);
		}
		exp->exflags++;	/* Make sure that it only get called once */
	}
}

static void
excdr(int excode, void *arg)
{
	exscsi(excode, arg);

#ifdef	needed
	/* Do several other restores/statistics here (see cdrecord.c) */
#endif
}

/*
 * Return milliseconds since start time.
 */
static int
prstats(void)
{
	int	sec;
	int	usec;
	int	tmsec;

	if (gettimeofday(&stoptime, (struct timezone *)0) < 0)
		comerr("Cannot get time\n");

	sec = stoptime.tv_sec - starttime.tv_sec;
	usec = stoptime.tv_usec - starttime.tv_usec;
	tmsec = sec*1000 + usec/1000;
#ifdef	lint
	tmsec = tmsec;	/* Bisz spaeter */
#endif
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}

	fprintf(stderr, "Time total: %d.%03dsec\n", sec, usec/1000);
	return (1000*sec + (usec / 1000));
}

/*
 * Return milliseconds since start time, but be silent this time.
 */
static int
prstats_silent(void)
{
	int	sec;
	int	usec;
	int	tmsec;

	if (gettimeofday(&stoptime, (struct timezone *)0) < 0)
		comerr("Cannot get time\n");

	sec = stoptime.tv_sec - starttime.tv_sec;
	usec = stoptime.tv_usec - starttime.tv_usec;
	tmsec = sec*1000 + usec/1000;
#ifdef	lint
	tmsec = tmsec;	/* Bisz spaeter */
#endif
	if (usec < 0) {
		sec--;
		usec += 1000000;
	}

	return (1000*sec + (usec / 1000));
}

static void
dorw(SCSI *usalp, char *filename, char *sectors)
{
	parm_t	params;
	char	*p = NULL;

	params.start = 0;
	params.end = -1;
	params.sptr = -1;
	params.askrange = FALSE;
	params.name = NULL;

	if (filename)
		params.name = filename;
	if (meshpoints > 0) {
		if (params.name == NULL)
			params.name = "/dev/null";
	}
	if (sectors)
		p = astol(sectors, &params.start);
	if (p && *p == '-')
		p = astol(++p, &params.end);
	if (p && *p != '\0')
		comerrno(EX_BAD, "Not a valid sector range '%s'\n", sectors);

	if (!wait_unit_ready(usalp, 60))
		comerrno(EX_BAD, "Device not ready.\n");

#ifdef	CLONE_WRITE
	if (fulltoc) {
		if (params.name == NULL)
			params.name = "/dev/null";
		read_ftoc(usalp, &params, FALSE);
	} else if (clonemode) {
		if (!is_mmc(usalp, NULL, NULL))
			comerrno(EX_BAD, "Unsupported device for clone mode.\n");
		noerror = TRUE;
		if (retries == MAX_RETRY)
			retries = 10;
		if (params.name == NULL)
			params.name = "/dev/null";

		if (read_ftoc(usalp, &params, TRUE) < 0)
			comerrno(EX_BAD, "Read fulltoc problems.\n");
		readcd_disk(usalp, &params);
	} else
#endif
	if (c2scan) {
		noerror = TRUE;
		if (retries == MAX_RETRY)
			retries = 10;
		if (params.name == NULL)
			params.name = "/dev/null";
		readc2_disk(usalp, &params);
	} else if (do_write)
		write_disk(usalp, &params);
	else
		read_disk(usalp, &params);
}

static void
doit(SCSI *usalp)
{
	int	i = 0;
	parm_t	params;

	params.start = 0;
	params.end = -1;
	params.sptr = -1;
	params.askrange = TRUE;
	params.name = "/dev/null";

	for (;;) {
		if (!wait_unit_ready(usalp, 60))
			comerrno(EX_BAD, "Device not ready.\n");

		printf("0:read 1:veri   2:erase   3:read buffer 4:cache 5:ovtime 6:cap\n");
		printf("7:wne  8:floppy 9:verify 10:checkcmds  11:read disk 12:write disk\n");
		printf("13:scsireset 14:seektest 15: readda 16: reada 17: c2err\n");
#ifdef	CLONE_WRITE
		printf("18:readom 19: lin 20: full toc\n");
#endif

		getint("Enter selection:", &i, 0, 20);
		if (didintr)
			return;

		switch (i) {

		case 5:		ovtime(usalp);		break;
		case 11:	read_disk(usalp, 0);	break;
		case 12:	write_disk(usalp, 0);	break;
		case 15:	ra(usalp);		break;
/*		case 16:	reada_disk(usalp, 0, 0);	break;*/
		case 17:	readc2_disk(usalp, &params);	break;
#ifdef	CLONE_WRITE
		case 18:	readcd_disk(usalp, 0);	break;
		case 19:	read_lin(usalp, 0);	break;
		case 20:	read_ftoc(usalp, 0, FALSE);	break;
#endif
		}
	}
}

static void
read_disk(SCSI *usalp, parm_t *parmp)
{
	rparm_t	rp;

	read_capacity(usalp);
	print_capacity(usalp, stderr);

	rp.errors = 0;
	rp.c2_errors = 0;
	rp.c2_maxerrs = 0;
	rp.c2_errsecs = 0;
	rp.c2_badsecs = 0;
	rp.secsize = usalp->cap->c_bsize;

	read_generic(usalp, parmp, fread_data, &rp, fdata_null);
}

#ifdef	CLONE_WRITE
static void
readcd_disk(SCSI *usalp, parm_t *parmp)
{
	rparm_t	rp;
	int	osecsize = 2048;
	int	oerr = 0;
	int	oretr = 10;
	int	(*funcp)(SCSI *_usalp, rparm_t *_rp, caddr_t bp, long addr, int cnt);

	usalp->silent++;
	if (read_capacity(usalp) >= 0)
		osecsize = usalp->cap->c_bsize;
	usalp->silent--;
	if (osecsize != 2048)
		select_secsize(usalp, 2048);

	read_capacity(usalp);
	print_capacity(usalp, stderr);

	rp.errors = 0;
	rp.c2_errors = 0;
	rp.c2_maxerrs = 0;
	rp.c2_errsecs = 0;
	rp.c2_badsecs = 0;
	rp.secsize = 2448;
	rp.ismmc = is_mmc(usalp, NULL, NULL);
	funcp = fread_2448;

	wait_unit_ready(usalp, 10);
	if (fread_2448(usalp, &rp, Sbuf, 0, 0) < 0) {
		errmsgno(EX_BAD, "read 2448 failed\n");
		if (rp.ismmc &&
		    fread_2448_16(usalp, &rp, Sbuf, 0, 0) >= 0) {
			errmsgno(EX_BAD, "read 2448_16 : OK\n");

			funcp = fread_2448_16;
		}
	}

	oldmode(usalp, &oerr, &oretr);
	exargs.oerr[0] = oerr;
	exargs.oerr[1] = oretr;
	exargs.oerr[2] = 0xFF;
	if (parmp == NULL)		/* XXX Nur am Anfang!!! */
		domode(usalp, -1, -1);
	else
		domode(usalp, nocorr?0x21:0x20, 10);

	read_generic(usalp, parmp, funcp, &rp, fdata_null);
	if (osecsize != 2048)
		select_secsize(usalp, osecsize);
	domode(usalp, oerr, oretr);
}

/* ARGSUSED */
static void
read_lin(SCSI *usalp, parm_t *parmp)
{
	parm_t	parm;
	rparm_t	rp;

	read_capacity(usalp);
	print_capacity(usalp, stderr);

	parm.start = ULONG_C(0xF0000000);
	parm.end =   ULONG_C(0xFF000000);
	parm.name = "DDD";

	rp.errors = 0;
	rp.c2_errors = 0;
	rp.c2_maxerrs = 0;
	rp.c2_errsecs = 0;
	rp.c2_badsecs = 0;
	rp.secsize = 2448;
	rp.ismmc = is_mmc(usalp, NULL, NULL);
	domode(usalp, -1, -1);
	read_generic(usalp, &parm, fread_lin, &rp, fdata_null);
}

static int
read_secheader(SCSI *usalp, long addr)
{
	rparm_t	rp;
	int	osecsize = 2048;
	int	ret = 0;

	usalp->silent++;
	if (read_capacity(usalp) >= 0)
		osecsize = usalp->cap->c_bsize;
	usalp->silent--;
	if (osecsize != 2048)
		select_secsize(usalp, 2048);

	read_capacity(usalp);

	rp.errors = 0;
	rp.c2_errors = 0;
	rp.c2_maxerrs = 0;
	rp.c2_errsecs = 0;
	rp.c2_badsecs = 0;
	rp.secsize = 2352;
	rp.ismmc = is_mmc(usalp, NULL, NULL);

	wait_unit_ready(usalp, 10);

	fillbytes(Sbuf, 2352, '\0');
	if (fread_2352(usalp, &rp, Sbuf, addr, 1) < 0) {
		ret = -1;
	}
	if (osecsize != 2048)
		select_secsize(usalp, osecsize);
	return (ret);
}

/* ARGSUSED */
static int
read_ftoc(SCSI *usalp, parm_t *parmp, BOOL do_sectype)
{
	FILE	*f;
	int	i;
	char	filename[1024];
	struct	tocheader *tp;
	char	*p;
	char	xb[256];
	int	len;
	char	xxb[10000];


	strcpy(filename, "toc.dat");
	if (strcmp(parmp->name, "/dev/null") != 0) {

		len = strlen(parmp->name);
		if (len > (sizeof (filename)-5)) {
			len = sizeof (filename)-5;
		}
		snprintf(filename, sizeof (filename), "%.*s.toc", len, parmp->name);
	}

	tp = (struct tocheader *)xb;

	fillbytes((caddr_t)xb, sizeof (xb), '\0');
	if (read_toc(usalp, xb, 0, sizeof (struct tocheader), 0, FMT_FULLTOC) < 0) {
		if (usalp->silent == 0 || usalp->verbose > 0)
			errmsgno(EX_BAD, "Cannot read TOC header\n");
		return (-1);
	}
	len = a_to_u_2_byte(tp->len) + sizeof (struct tocheader)-2;
	fprintf(stderr, "TOC len: %d. First Session: %d Last Session: %d.\n", len, tp->first, tp->last);

	if (read_toc(usalp, xxb, 0, len, 0, FMT_FULLTOC) < 0) {
		if (len & 1) {
			/*
			 * Work around a bug in some operating systems that do not
			 * handle odd byte DMA correctly for ATAPI drives.
			 */
			wait_unit_ready(usalp, 30);
			read_toc(usalp, xb, 0, sizeof (struct tocheader), 0, FMT_FULLTOC);
			wait_unit_ready(usalp, 30);
			if (read_toc(usalp, xxb, 0, len+1, 0, FMT_FULLTOC) >= 0) {
				goto itworked;
			}
		}
		if (usalp->silent == 0)
			errmsgno(EX_BAD, "Cannot read full TOC\n");
		return (-1);
	}

itworked:
	f = fileopen(filename, "wctb");

	if (f == NULL)
		comerr("Cannot open '%s'.\n", filename);
	filewrite(f, xxb, len);
	if (do_sectype)
		read_sectypes(usalp, f);
	fflush(f);
	fclose(f);

	p = &xxb[4];
	for (; p < &xxb[len]; p += 11) {
		for (i = 0; i < 11; i++)
			fprintf(stderr, "%02X ", p[i] & 0xFF);
		fprintf(stderr, "\n");
	}
	/*
	 * List all lead out start times to give information about multi
	 * session disks.
	 */
	p = &xxb[4];
	for (; p < &xxb[len]; p += 11) {
		if ((p[3] & 0xFF) == 0xA2) {
			fprintf(stderr, "Lead out %d: %ld\n", p[0], msf_to_lba(p[8], p[9], p[10], TRUE));
		}
	}
	return (0);
}

static void
read_sectypes(SCSI *usalp, FILE *f)
{
	char	sect;

	sect = SECT_AUDIO;
	get_sectype(usalp, 4, &sect);
	if (f != NULL)
		filewrite(f, &sect, 1);
	if (xdebug)
		usal_prbytes("sec 0", (Uchar *)Sbuf, 16);

	sect = SECT_AUDIO;
	get_sectype(usalp, usalp->cap->c_baddr-4, &sect);
	if (f != NULL)
		filewrite(f, &sect, 1);
	if (xdebug) {
		usal_prbytes("sec E", (Uchar *)Sbuf, 16);
		fprintf(stderr, "baddr: %ld\n", (long)usalp->cap->c_baddr);
	}
}

static void
get_sectype(SCSI *usalp, long addr, char *st)
{
	char	*synchdr = "\0\377\377\377\377\377\377\377\377\377\377\0";
	int	sectype = SECT_AUDIO;
	int	i;
	long	raddr = addr;
#define	_MAX_TRY_	20

	usalp->silent++;
	for (i = 0; i < _MAX_TRY_ && read_secheader(usalp, raddr) < 0; i++) {
		if (addr == 0)
			raddr++;
		else
			raddr--;
	}
	usalp->silent--;
	if (i >= _MAX_TRY_) {
		fprintf(stderr, "Sectype (%ld) is CANNOT\n", addr);
		return;
	} else if (i > 0) {
		fprintf(stderr, "Sectype (%ld) needed %d retries\n", addr, i);
	}
#undef	_MAX_TRY_

	if (cmpbytes(Sbuf, synchdr, 12) < 12) {
		if (xdebug)
			fprintf(stderr, "Sectype (%ld) is AUDIO\n", addr);
		if (st)
			*st = SECT_AUDIO;
		return;
	}
	if (xdebug)
		fprintf(stderr, "Sectype (%ld) is DATA\n", addr);
	if (Sbuf[15] == 0) {
		if (xdebug)
			fprintf(stderr, "Sectype (%ld) is MODE 0\n", addr);
		sectype = SECT_MODE_0;

	} else if (Sbuf[15] == 1) {
		if (xdebug)
			fprintf(stderr, "Sectype (%ld) is MODE 1\n", addr);
		sectype = SECT_ROM;

	} else if (Sbuf[15] == 2) {
		if (xdebug)
			fprintf(stderr, "Sectype (%ld) is MODE 2\n", addr);

		if ((Sbuf[16+2]  & 0x20) == 0 &&
		    (Sbuf[16+4+2]  & 0x20) == 0) {
			if (xdebug)
				fprintf(stderr, "Sectype (%ld) is MODE 2 form 1\n", addr);
			sectype = SECT_MODE_2_F1;

		} else if ((Sbuf[16+2]  & 0x20) != 0 &&
		    (Sbuf[16+4+2]  & 0x20) != 0) {
			if (xdebug)
				fprintf(stderr, "Sectype (%ld) is MODE 2 form 2\n", addr);
			sectype = SECT_MODE_2_F2;
		} else {
			if (xdebug)
				fprintf(stderr, "Sectype (%ld) is MODE 2 formless\n", addr);
			sectype = SECT_MODE_2;
		}
	} else {
		fprintf(stderr, "Sectype (%ld) is UNKNOWN\n", addr);
	}
	if (st)
		*st = sectype;
	if (xdebug)
		fprintf(stderr, "Sectype (%ld) is 0x%02X\n", addr, sectype);
}

#endif	/* CLONE_WRITE */

char	zeroblk[512];

static void
readc2_disk(SCSI *usalp, parm_t *parmp)
{
	rparm_t	rp;
	int	osecsize = 2048;
	int	oerr = 0;
	int	oretr = 10;

	usalp->silent++;
	if (read_capacity(usalp) >= 0)
		osecsize = usalp->cap->c_bsize;
	usalp->silent--;
	if (osecsize != 2048)
		select_secsize(usalp, 2048);

	read_capacity(usalp);
	print_capacity(usalp, stderr);

	rp.errors = 0;
	rp.c2_errors = 0;
	rp.c2_maxerrs = 0;
	rp.c2_errsecs = 0;
	rp.c2_badsecs = 0;
	rp.secsize = 2352 + 294;
	rp.ismmc = is_mmc(usalp, NULL, NULL);

	oldmode(usalp, &oerr, &oretr);
	exargs.oerr[0] = oerr;
	exargs.oerr[1] = oretr;
	exargs.oerr[2] = 0xFF;
	domode(usalp, 0x21, 10);


	read_generic(usalp, parmp, fread_c2, &rp, fdata_c2);
	if (osecsize != 2048)
		select_secsize(usalp, osecsize);
	domode(usalp, oerr, oretr);

	printf("Total of %d hard read errors.\n", rp.errors);
	printf("C2 errors total: %d bytes in %d sectors on disk\n", rp.c2_errors, rp.c2_errsecs);
	printf("C2 errors rate: %f%% \n", (100.0*rp.c2_errors)/usalp->cap->c_baddr/2352);
	printf("C2 errors on worst sector: %d, sectors with 100+ C2 errors: %d\n", rp.c2_maxerrs, rp.c2_badsecs);
}

/* ARGSUSED */
static int
fread_data(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	return (read_g1(usalp, bp, addr, cnt));
}

#ifdef	CLONE_WRITE
static int
fread_2448(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	if (rp->ismmc) {
		return (read_cd(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC */
			(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3),
			/* plus all subchannels RAW */
			1));
	} else {
		return (read_da(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC + all subch */
			0x02));
	}
}

static int
fread_2448_16(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{

	if (rp->ismmc) {
		track_t trackdesc;
		int	ret;
		int	i;
		char	*p;

		trackdesc.isecsize = 2368;
		trackdesc.secsize = 2448;
		ret = read_cd(usalp, bp, addr, cnt, 2368,
			/* Sync + all headers + user data + EDC/ECC */
			(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3),
			/* subchannels P/Q */
			2);
		if (ret < 0)
			return (ret);

		scatter_secs(&trackdesc, bp, cnt);
		for (i = 0, p = bp+2352; i < cnt; i++) {
#ifdef	more_than_q_sub
			if ((p[15] & 0x80) != 0)
				printf("P");
#endif
			/*
			 * As the drives don't return P-sub, we check
			 * whether the index equals 0.
			 */
			qpto96((Uchar *)p, (Uchar *)p, p[2] == 0);
			p += 2448;
		}
		return (ret);
	} else {
		comerrno(EX_BAD, "Cannot fread_2448_16 on non MMC drives\n");

		return (read_da(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC + all subch */
			0x02));
	}
}

static int
fread_2352(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	if (rp->ismmc) {
		return (read_cd(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC */
			(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3),
			/* NO subchannels */
			0));
	} else {
		comerrno(EX_BAD, "Cannot fread_2352 on non MMC drives\n");

		return (read_da(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC + all subch */
			0x02));
	}
}

static int
fread_lin(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	if (addr != ULONG_C(0xF0000000))
		addr = ULONG_C(0xFFFFFFFF);

	return (read_cd(usalp, bp, addr, cnt, rp->secsize,
		/* Sync + all headers + user data + EDC/ECC */
		(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3),
		/* plus all subchannels RAW */
		1));
}
#endif	/* CLONE_WRITE */

static int
bits(int c)
{
	int	n = 0;

	if (c & 0x01)
		n++;
	if (c & 0x02)
		n++;
	if (c & 0x04)
		n++;
	if (c & 0x08)
		n++;
	if (c & 0x10)
		n++;
	if (c & 0x20)
		n++;
	if (c & 0x40)
		n++;
	if (c & 0x80)
		n++;
	return (n);
}

static int
bitidx(int c)
{
	if (c & 0x80)
		return (0);
	if (c & 0x40)
		return (1);
	if (c & 0x20)
		return (2);
	if (c & 0x10)
		return (3);
	if (c & 0x08)
		return (4);
	if (c & 0x04)
		return (5);
	if (c & 0x02)
		return (6);
	if (c & 0x01)
		return (7);
	return (-1);
}

static int
fread_c2(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	if (rp->ismmc) {
		return (read_cd(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC + C2 */
/*			(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3 | 2 << 1),*/
			(1 << 7 | 3 << 5 | 1 << 4 | 1 << 3 | 1 << 1),
			/* without subchannels */
			0));
	} else {
		return (read_da(usalp, bp, addr, cnt, rp->secsize,
			/* Sync + all headers + user data + EDC/ECC + C2 */
			0x04));
	}
}

/* ARGSUSED */
static int
fdata_null(rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	return (0);
}

static int
fdata_c2(rparm_t *rp, caddr_t bp, long addr, int cnt)
{
	int	i;
	int	j;
	int	k;
	char	*p;

	p = &bp[2352];

	for (i = 0; i < cnt; i++, p += (2352+294)) {
/*		usal_prbytes("XXX ", p, 294);*/
		if ((j = cmpbytes(p, zeroblk, 294)) < 294) {
			printf("C2 in sector: %3ld first at byte: %4d (0x%02X)", addr+i,
				j*8 + bitidx(p[j]), p[j]&0xFF);
			for (j = 0, k = 0; j < 294; j++)
				k += bits(p[j]);
			printf(" total: %4d errors\n", k);
/*			usal_prbytes("XXX ", p, 294);*/
			rp->c2_errors += k;
			if (k > rp->c2_maxerrs)
				rp->c2_maxerrs = k;
			rp->c2_errsecs++;
			if (k >= 100)
				rp->c2_badsecs += 1;
		}
	}
	return (0);
}

#ifdef	used
static int
read_scsi_g1(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
/*	scmd->size = cnt*512;*/
	scmd->size = cnt*usalp->cap->c_bsize;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x28;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "read extended";

	return (usal_cmd(usalp));
}
#endif

#define	G0_MAXADDR	0x1FFFFFL

int
write_scsi(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	if (addr <= G0_MAXADDR)
		return (write_g0(usalp, bp, addr, cnt));
	else
		return (write_g1(usalp, bp, addr, cnt));
}

int
write_g0(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (usalp->cap->c_bsize <= 0)
		raisecond("capacity_not_set", 0L);

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*usalp->cap->c_bsize;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g0_cdb.cmd = SC_WRITE;
	scmd->cdb.g0_cdb.lun = usal_lun(usalp);
	g0_cdbaddr(&scmd->cdb.g0_cdb, addr);
	scmd->cdb.g0_cdb.count = (Uchar)cnt;

	usalp->cmdname = "write_g0";

	return (usal_cmd(usalp));
}

int
write_g1(SCSI *usalp, caddr_t bp, long addr, int cnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (usalp->cap->c_bsize <= 0)
		raisecond("capacity_not_set", 0L);

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*usalp->cap->c_bsize;
	scmd->flags = SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = SC_EWRITE;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	g1_cdbaddr(&scmd->cdb.g1_cdb, addr);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "write_g1";

	return (usal_cmd(usalp));
}

#ifdef	used
static void
Xrequest_sense(SCSI *usalp)
{
	char	sense_buf[32];
	struct	usal_cmd ocmd;
	int	sense_count;
	char	*cmdsave;
	register struct	usal_cmd	*scmd = usalp->scmd;

	cmdsave = usalp->cmdname;

	movebytes(scmd, &ocmd, sizeof (*scmd));

	fillbytes((caddr_t)sense_buf, sizeof (sense_buf), '\0');

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = (caddr_t)sense_buf;
	scmd->size = sizeof (sense_buf);
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G0_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x3;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	scmd->cdb.g0_cdb.count = sizeof (sense_buf);

	usalp->cmdname = "request sense";

	usal_cmd(usalp);

	sense_count = sizeof (sense_buf) - usal_getresid(usalp);
	movebytes(&ocmd, scmd, sizeof (*scmd));
	scmd->sense_count = sense_count;
	movebytes(sense_buf, (Uchar *)&scmd->sense, scmd->sense_count);

	usalp->cmdname = cmdsave;
	usal_printerr(usalp);
	usal_printresult(usalp);	/* XXX restore key/code in future */
}
#endif

static int
read_retry(SCSI *usalp, caddr_t bp, long addr, long cnt, 
			  int (*rfunc)(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt), 
			  rparm_t *rp)
{
/*	int	secsize = usalp->cap->c_bsize;*/
	int	secsize = rp->secsize;
	int	try = 0;
	int	err;
	char	dummybuf[8192];

	if (secsize > sizeof (dummybuf)) {
		errmsgno(EX_BAD, "Cannot retry, sector size %d too big.\n", secsize);
		return (-1);
	}

	errmsgno(EX_BAD, "Retrying from sector %ld.\n", addr);
	while (cnt > 0) {
		fprintf(stderr, ".");

		do {
			if (didintr)
				comexit(exsig);		/* XXX besseres Konzept?!*/
			wait_unit_ready(usalp, 120);
			if (try >= 10) {		/* First 10 retries without seek */
				if ((try % 8) == 0) {
					fprintf(stderr, "+");	/* Read last sector */
					usalp->silent++;
					(*rfunc)(usalp, rp, dummybuf, usalp->cap->c_baddr, 1);
					usalp->silent--;
				} else if ((try % 4) == 0) {
					fprintf(stderr, "-");	/* Read first sector */
					usalp->silent++;
					(*rfunc)(usalp, rp, dummybuf, 0, 1);
					usalp->silent--;
				} else {
					fprintf(stderr, "~");	/* Read random sector */
					usalp->silent++;
					(*rfunc)(usalp, rp, dummybuf, choice(usalp->cap->c_baddr), 1);
					usalp->silent--;
				}
				if (didintr)
					comexit(exsig);		/* XXX besseres Konzept?!*/
				wait_unit_ready(usalp, 120);
			}
			if (didintr)
				comexit(exsig);		/* XXX besseres Konzept?!*/

			fillbytes(bp, secsize, 0);

			usalp->silent++;
			err = (*rfunc)(usalp, rp, bp, addr, 1);
			usalp->silent--;

			if (err < 0) {
				err = usalp->scmd->ux_errno;
/*				fprintf(stderr, "\n");*/
/*				errmsgno(err, "Cannot read source disk\n");*/
			} else {
				if (usal_getresid(usalp)) {
					fprintf(stderr, "\nresid: %d\n", usal_getresid(usalp));
					return (-1);
				}
				break;
			}
		} while (++try < retries);

		if (try >= retries) {
			fprintf(stderr, "\n");
			errmsgno(err, "Error on sector %ld not corrected. Total of %d errors.\n",
					addr, ++rp->errors);

			if (usalp->silent <= 1 && lverbose > 0)
				usal_printerr(usalp);

			add_bad(addr);

			if (!noerror)
				return (-1);
			errmsgno(EX_BAD, "-noerror set, continuing ...\n");
		} else {
			if (try >= maxtry)
				maxtry = try;

			if (try > 1) {
				fprintf(stderr, "\n");
				errmsgno(EX_BAD,
				"Error on sector %ld corrected after %d tries. Total of %d errors.\n",
					addr, try, rp->errors);
			}
		}
		try = 0;
		cnt -= 1;
		addr += 1;
		bp += secsize;
	}
	return (0);
}

static void
read_generic(SCSI *usalp, parm_t *parmp, 
				 int (*rfunc)(SCSI *usalp, rparm_t *rp, caddr_t bp, long addr, int cnt),
				 rparm_t *rp,
				 int (*dfunc)(rparm_t *rp, caddr_t bp, long addr, int cnt))
{
	char	filename[512];
	char	*defname = NULL;
	FILE	*f;
	long	addr = 0L;
	long	old_addr = 0L;
	long	num;
	long	end = 0L;
	long	start = 0L;
	long	cnt = 0L;
	long	next_point = 0L;
	long	secs_per_point = 0L;
	double  speed;
	int	msec;
	int	old_msec = 0;
	int	err = 0;
	BOOL	askrange = FALSE;
	BOOL	isrange = FALSE;
	int	secsize = rp->secsize;
	int	i = 0;

	if (is_suid) {
		if (usalp->inq->type != INQ_ROMD)
			comerrno(EX_BAD, "Not root. Will only read from CD in suid/priv mode\n");
	}

	if (parmp == NULL || parmp->askrange)
		askrange = TRUE;
	if (parmp != NULL && !askrange && (parmp->start <= parmp->end))
		isrange = TRUE;

	filename[0] = '\0';

	usalp->silent++;
	if (read_capacity(usalp) >= 0)
		end = usalp->cap->c_baddr + 1;
	usalp->silent--;

	if ((end <= 0 && isrange) || (askrange && usal_yes("Ignore disk size? ")))
		end = 10000000;	/* Hack to read empty (e.g. blank=fast) disks */

	if (parmp) {
		if (parmp->name)
			defname = parmp->name;
		if (defname != NULL) {
			fprintf(stderr, "Copy from SCSI (%d,%d,%d) disk to file '%s'\n",
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp),
					defname);
		}

		addr = start = parmp->start;
		if (parmp->end != -1 && parmp->end < end)
			end = parmp->end;
		cnt = Sbufsize / secsize;
	}

	if (defname == NULL) {
		defname = "disk.out";
		fprintf(stderr, "Copy from SCSI (%d,%d,%d) disk to file\n",
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
		fprintf(stderr, "Enter filename [%s]: ", defname); flush();
		(void) getline(filename, sizeof (filename));
	}

	if (askrange) {
		addr = start;
		getlong("Enter starting sector for copy:", &addr, start, end-1);
/*		getlong("Enter starting sector for copy:", &addr, -300, end-1);*/
		start = addr;
	}

	if (askrange) {
		num = end - addr;
		getlong("Enter number of sectors to copy:", &num, 1L, num);
		end = addr + num;
	}

	if (askrange) {
/* XXX askcnt */
		cnt = Sbufsize / secsize;
		getlong("Enter number of sectors per copy:", &cnt, 1L, cnt);
	}

	if (filename[0] == '\0')
		strncpy(filename, defname, sizeof (filename));
	filename[sizeof (filename)-1] = '\0';
	if (streql(filename, "-")) {
		f = stdout;
#ifdef	NEED_O_BINARY
		setmode(STDOUT_FILENO, O_BINARY);
#endif
	} else if ((f = fileopen(filename, notrunc?"wcub":"wctub")) == NULL)
		comerr("Cannot open '%s'.\n", filename);

	fprintf(stderr, "end:  %8ld\n", end);
	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		comerr("Cannot get start time\n");

	if (meshpoints > 0) {
		if ((end-start) < meshpoints)
			secs_per_point = 1;
		else
			secs_per_point = (end-start) / meshpoints;
		next_point = start + secs_per_point;
		old_addr = start;
	}

	for (; addr < end; addr += cnt) {
		if (didintr)
			comexit(exsig);		/* XXX besseres Konzept?!*/

		if ((addr + cnt) > end)
			cnt = end - addr;

		if (meshpoints > 0) {
			if (addr > next_point) {

				msec = prstats_silent();
				if ((msec - old_msec) == 0)		/* Avoid division by zero */
					msec = old_msec + 1;
				speed = ((addr - old_addr)/(1000.0/secsize)) / (0.001*(msec - old_msec));
				if (do_factor) {
					if (is_cdrom)
						speed /= 176.400;
					else if (is_dvd)
						speed /= 1385.0;
				}
				fprintf(stderr, "addr: %8ld cnt: %ld", addr, cnt);
				printf("%8ld %8.2f\n", addr, speed);
				fprintf(stderr, "\r");
				next_point += secs_per_point;
				old_addr = addr;
				old_msec = msec;
				i++;
				if (meshpoints < 100)
					flush();
				else if (i % (meshpoints/100) == 0)
					flush();
			}
		}
		fprintf(stderr, "addr: %8ld cnt: %ld\r", addr, cnt);

		usalp->silent++;
		if ((*rfunc)(usalp, rp, Sbuf, addr, cnt) < 0) {
			usalp->silent--;
			err = usalp->scmd->ux_errno;
			if (quiet) {
				fprintf(stderr, "\n");
			} else if (usalp->silent == 0) {
				usal_printerr(usalp);
			}
			errmsgno(err, "Cannot read source disk\n");

			if (read_retry(usalp, Sbuf, addr, cnt, rfunc, rp) < 0)
				goto out;
		} else {
			usalp->silent--;
			if (usal_getresid(usalp)) {
				fprintf(stderr, "\nresid: %d\n", usal_getresid(usalp));
				goto out;
			}
		}
		(*dfunc)(rp, Sbuf, addr, cnt);
		if (filewrite(f, Sbuf, cnt * secsize) < 0) {
			err = geterrno();
			fprintf(stderr, "\n");
			errmsgno(err, "Cannot write '%s'\n", filename);
			break;
		}
	}
	fprintf(stderr, "addr: %8ld", addr);
out:
	fprintf(stderr, "\n");
	msec = prstats();
	if (msec == 0)		/* Avoid division by zero */
		msec = 1;
#ifdef	OOO
	fprintf(stderr, "Read %.2f kB at %.1f kB/sec.\n",
		(double)(addr - start)/(1024.0/usalp->cap->c_bsize),
		(double)((addr - start)/(1024.0/usalp->cap->c_bsize)) / (0.001*msec));
#else
	fprintf(stderr, "Read %.2f kB at %.1f kB/sec.\n",
		(double)(addr - start)/(1024.0/secsize),
		(double)((addr - start)/(1024.0/secsize)) / (0.001*msec));
#endif
	print_bad();
}

static void
write_disk(SCSI *usalp, parm_t *parmp)
{
	char	filename[512];
	char	*defname = "disk.out";
	FILE	*f;
	long	addr = 0L;
	long	cnt;
	long	amt;
	long	end;
	int	msec;
	int	start;

	if (is_suid)
		comerrno(EX_BAD, "Not root. Will not write in suid/priv mode\n");

	filename[0] = '\0';
	if (read_capacity(usalp) >= 0) {
		end = usalp->cap->c_baddr + 1;
		print_capacity(usalp, stderr);
	}

	if (end <= 1)
		end = 10000000;	/* Hack to write empty disks */

	if (parmp) {
		if (parmp->name)
			defname = parmp->name;
		fprintf(stderr, "Copy from file '%s' to SCSI (%d,%d,%d) disk\n",
					defname,
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));

		addr = start = parmp->start;
		if (parmp->end != -1 && parmp->end < end)
			end = parmp->end;
		cnt = Sbufsize / usalp->cap->c_bsize;
	} else {
		fprintf(stderr, "Copy from file to SCSI (%d,%d,%d) disk\n",
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
		fprintf(stderr, "Enter filename [%s]: ", defname); flush();
		(void) getline(filename, sizeof (filename));
		fprintf(stderr, "Notice: reading from file always starts at file offset 0.\n");

		getlong("Enter starting sector for copy:", &addr, 0L, end-1);
		start = addr;
		cnt = end - addr;
		getlong("Enter number of sectors to copy:", &end, 1L, end);
		end = addr + cnt;

		cnt = Sbufsize / usalp->cap->c_bsize;
		getlong("Enter number of sectors per copy:", &cnt, 1L, cnt);
/*		fprintf(stderr, "end:  %8ld\n", end);*/
	}

	if (filename[0] == '\0')
		strncpy(filename, defname, sizeof (filename));
	filename[sizeof (filename)-1] = '\0';
	if (streql(filename, "-")) {
		f = stdin;
#ifdef	NEED_O_BINARY
		setmode(STDIN_FILENO, O_BINARY);
#endif
	} else if ((f = fileopen(filename, "rub")) == NULL)
		comerr("Cannot open '%s'.\n", filename);

	fprintf(stderr, "end:  %8ld\n", end);
	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		comerr("Cannot get start time\n");

	for (; addr < end; addr += cnt) {
		if (didintr)
			comexit(exsig);		/* XXX besseres Konzept?!*/

		if ((addr + cnt) > end)
			cnt = end - addr;

		fprintf(stderr, "addr: %8ld cnt: %ld\r", addr, cnt);

		if ((amt = fileread(f, Sbuf, cnt * usalp->cap->c_bsize)) < 0)
			comerr("Cannot read '%s'\n", filename);
		if (amt == 0)
			break;
		if ((amt / usalp->cap->c_bsize) < cnt)
			cnt = amt / usalp->cap->c_bsize;
		if (write_scsi(usalp, Sbuf, addr, cnt) < 0)
			comerrno(usalp->scmd->ux_errno,
					"Cannot write destination disk\n");
	}
	fprintf(stderr, "addr: %8ld\n", addr);
	msec = prstats();
	if (msec == 0)		/* Avoid division by zero */
		msec = 1;
	fprintf(stderr, "Wrote %.2f kB at %.1f kB/sec.\n",
		(double)(addr - start)/(1024.0/usalp->cap->c_bsize),
		(double)((addr - start)/(1024.0/usalp->cap->c_bsize)) / (0.001*msec));
}

static int
choice(int n)
{
#if	defined(HAVE_DRAND48)
	extern	double	drand48(void);

	return (drand48() * n);
#else
#	if	defined(HAVE_RAND)
	extern	int	rand(void);

	return (rand() % n);
#	else
	return (0);
#	endif
#endif
}

static void
ra(SCSI *usalp)
{
/*	char	filename[512];*/
	FILE	*f;
/*	long	addr = 0L;*/
/*	long	cnt;*/
/*	long	end;*/
/*	int	msec;*/
/*	int	start;*/
/*	int	err = 0;*/

	select_secsize(usalp, 2352);
	read_capacity(usalp);
	print_capacity(usalp, stderr);
	fillbytes(Sbuf, 50*2352, 0);
	if (read_g1(usalp, Sbuf, 0, 50) < 0)
		errmsg("read CD\n");
	f = fileopen("DDA", "wctb");
/*	filewrite(f, Sbuf, 50 * 2352 - usal_getresid(usalp));*/
	filewrite(f, Sbuf, 50 * 2352);
	fclose(f);
}

#define	g5x_cdblen(cdb, len)	((cdb)->count[0] = ((len) >> 16L)& 0xFF,\
				(cdb)->count[1] = ((len) >> 8L) & 0xFF,\
				(cdb)->count[2] = (len) & 0xFF)

int
read_da(SCSI *usalp, caddr_t bp, long addr, int cnt, int framesize, int subcode)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (usalp->cap->c_bsize <= 0)
		raisecond("capacity_not_set", 0L);

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*framesize;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xd8;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	g5_cdbaddr(&scmd->cdb.g5_cdb, addr);
	g5_cdblen(&scmd->cdb.g5_cdb, cnt);
	scmd->cdb.g5_cdb.res10 = subcode;

	usalp->cmdname = "read_da";

	return (usal_cmd(usalp));
}

int
read_cd(SCSI *usalp, caddr_t bp, long addr, int cnt, int framesize, int data, 
		  int subch)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt*framesize;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G5_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g5_cdb.cmd = 0xBE;
	scmd->cdb.g5_cdb.lun = usal_lun(usalp);
	scmd->cdb.g5_cdb.res = 0;	/* expected sector type field ALL */
	g5_cdbaddr(&scmd->cdb.g5_cdb, addr);
	g5x_cdblen(&scmd->cdb.g5_cdb, cnt);

	scmd->cdb.g5_cdb.count[3] = data & 0xFF;
	scmd->cdb.g5_cdb.res10 = subch & 0x07;

	usalp->cmdname = "read_cd";

	return (usal_cmd(usalp));
}

static void
oldmode(SCSI *usalp, int *errp, int *retrp)
{
	Uchar	mode[0x100];
	Uchar	cmode[0x100];
	Uchar	*p;
	int	i;
	int	len;

	fillbytes(mode, sizeof (mode), '\0');
	fillbytes(cmode, sizeof (cmode), '\0');

	if (!get_mode_params(usalp, 0x01, "CD error recovery parameter",
			mode, (Uchar *)0, (Uchar *)cmode, (Uchar *)0, &len)) {
		return;
	}
	if (xdebug)
		usal_prbytes("Mode Sense Data", mode, len);

	mode[0] = 0;
	mode[2] = 0; /* ??? ist manchmal 0x80 */
	p = mode;
	p += mode[3] + 4;
	*p &= 0x3F;

	if (xdebug)
		usal_prbytes("Mode page 1:", p, 0x10);

	i = p[2];
	if (errp != NULL)
		*errp = i;

	i = p[3];
	if (retrp != NULL)
		*retrp = i;
}

static void
domode(SCSI *usalp, int err, int retr)
{
	Uchar	mode[0x100];
	Uchar	cmode[0x100];
	Uchar	*p;
	int	i;
	int	len;

	fillbytes(mode, sizeof (mode), '\0');
	fillbytes(cmode, sizeof (cmode), '\0');

	if (!get_mode_params(usalp, 0x01, "CD error recovery parameter",
			mode, (Uchar *)0, (Uchar *)cmode, (Uchar *)0, &len)) {
		return;
	}
	if (xdebug || (err == -1 && retr == -1)) {
		usal_prbytes("Mode Sense Data", mode, len);
	}

	mode[0] = 0;
	mode[2] = 0; /* ??? ist manchmal 0x80 */
	p = mode;
	p += mode[3] + 4;
	*p &= 0x3F;

	if (xdebug || (err == -1 && retr == -1))
		usal_prbytes("Mode page 1:", p, 0x10);

	i = p[2];
	if (err == -1) {
		getint("Error handling? ", &i, 0, 255);
		p[2] = i;
	} else {
		if (xdebug)
			fprintf(stderr, "Error handling set from %02X to %02X\n",
		p[2], err);
		p[2] = err;
	}

	i = p[3];
	if (retr == -1) {
		getint("Retry count? ", &i, 0, 255);
		p[3] = i;
	} else {
		if (xdebug)
			fprintf(stderr, "Retry count set from %d to %d\n",
		p[3] & 0xFF, retr);
		p[3] = retr;
	}

	if (xdebug || (err == -1 && retr == -1))
		usal_prbytes("Mode Select Data", mode, len);
	mode_select(usalp, mode, len, 0, usalp->inq->data_format >= 2);
}


/*--------------------------------------------------------------------------*/
static	void	qpto96(Uchar *sub, Uchar *subq, int dop);
/*EXPORT	void	qpto96		__PR((Uchar *sub, Uchar *subq, int dop));*/
/*
 * Q-Sub auf 96 Bytes blähen und P-Sub addieren
 *
 * OUT: sub, IN: subqptr
 */
static void
/*EXPORT void*/
qpto96(Uchar *sub, Uchar *subqptr, int dop)
{
	Uchar	tmp[16];
	Uchar	*p;
	int	c;
	int	i;

	if (subqptr == sub) {
		movebytes(subqptr, tmp, 12);
		subqptr = tmp;
	}
	fillbytes(sub, 96, '\0');

	/* CSTYLED */
	if (dop) for (i = 0, p = sub; i < 96; i++) {
		*p++ |= 0x80;
	}
	for (i = 0, p = sub; i < 12; i++) {
		c = subqptr[i] & 0xFF;
/*printf("%02X\n", c);*/
		if (c & 0x80)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x40)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x20)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x10)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x08)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x04)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x02)
			*p++ |= 0x40;
		else
			p++;
		if (c & 0x01)
			*p++ |= 0x40;
		else
			p++;
	}
}

/*--------------------------------------------------------------------------*/

static void
ovtime(SCSI *usalp)
{
	register int	i;

	usalp->silent++;
	(void) test_unit_ready(usalp);
	usalp->silent--;
	if (test_unit_ready(usalp) < 0)
		return;

	printf("Doing 1000 'TEST UNIT READY' operations.\n");

	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		comerr("Cannot get start time\n");

	for (i = 1000; --i >= 0; ) {
		(void) test_unit_ready(usalp);

		if (didintr)
			return;
	}

	prstats();

	/*
	 * ATAPI drives do not like seek_g0()
	 */
	usalp->silent++;
	i = seek_g0(usalp, 0L);
	usalp->silent--;

	if (i >= 0) {
		printf("Doing 1000 'SEEK_G0 (0)' operations.\n");

		if (gettimeofday(&starttime, (struct timezone *)0) < 0)
			comerr("Cannot get start time\n");

		for (i = 1000; --i >= 0; ) {
			(void) seek_g0(usalp, 0L);

			if (didintr)
				return;
		}

		prstats();
	}

	usalp->silent++;
	i = seek_g1(usalp, 0L);
	usalp->silent--;
	if (i < 0)
		return;

	printf("Doing 1000 'SEEK_G1 (0)' operations.\n");

	if (gettimeofday(&starttime, (struct timezone *)0) < 0)
		comerr("Cannot get start time\n");

	for (i = 1000; --i >= 0; ) {
		(void) seek_g1(usalp, 0L);

		if (didintr)
			return;
	}

	prstats();
}

#define	BAD_INC		16
long	*badsecs;
int	nbad;
int	maxbad;

static void
add_bad(long addr)
{
	if (maxbad == 0) {
		maxbad = BAD_INC;
		badsecs = malloc(maxbad * sizeof (long));
		if (badsecs == NULL)
			comerr("No memory for bad sector list\n.");
	}
	if (nbad >= maxbad) {
		maxbad += BAD_INC;
		badsecs = realloc(badsecs, maxbad * sizeof (long));
		if (badsecs == NULL)
			comerr("No memory to grow bad sector list\n.");
	}
	badsecs[nbad++] = addr;
}

static void
print_bad(void)
{
	int	i;

	if (nbad == 0)
		return;

	fprintf(stderr, "Max corected retry count was %d (limited to %d).\n", maxtry, retries);
	fprintf(stderr, "The following %d sector(s) could not be read correctly:\n", nbad);
	for (i = 0; i < nbad; i++)
		fprintf(stderr, "%ld\n", badsecs[i]);
}
