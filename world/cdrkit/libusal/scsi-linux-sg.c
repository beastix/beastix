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

/* @(#)scsi-linux-sg.c	1.86 05/11/22 Copyright 1997 J. Schilling */
/*
 *	Interface for Linux generic SCSI implementation (sg).
 *
 *	This is the interface for the broken Linux SCSI generic driver.
 *	This is a hack, that tries to emulate the functionality
 *	of the usal driver.
 *
 *	Design flaws of the sg driver:
 *	-	cannot see if SCSI command could not be send
 *	-	cannot get SCSI status byte
 *	-	cannot get real dma count of tranfer
 *	-	cannot get number of bytes valid in auto sense data
 *	-	to few data in auto sense (CCS/SCSI-2/SCSI-3 needs >= 18)
 *
 *	This code contains support for the sg driver version 2 by
 *		H. Eiﬂfeld & J. Schilling
 *	Although this enhanced version has been announced to Linus and Alan,
 *	there was no reaction at all.
 *
 *	About half a year later there occured a version in the official
 *	Linux that was also called version 2. The interface of this version
 *	looks like a playground - the enhancements from this version are
 *	more or less useless for a portable real-world program.
 *
 *	With Linux 2.4 the official version of the sg driver is called 3.x
 *	and seems to be usable again. The main problem now is the curious
 *	interface that is provided to raise the DMA limit from 32 kB to a
 *	more reasonable value. To do this in a reliable way, a lot of actions
 *	are required.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1997 J. Schilling
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

#include <linux/version.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/utsname.h>

#ifndef LINUX_VERSION_CODE	/* Very old kernel? */
#	define LINUX_VERSION_CODE 0
#endif

#if LINUX_VERSION_CODE >= 0x01031a /* <linux/scsi.h> introduced in 1.3.26 */
#if LINUX_VERSION_CODE >= 0x020000 /* <scsi/scsi.h> introduced somewhere. */
/* Need to fine tune the ifdef so we get the transition point right. */
#include <scsi/scsi.h>
#else
#include <linux/scsi.h>
#endif
#else				/* LINUX_VERSION_CODE == 0 Very old kernel? */
#define	__KERNEL__		/* Some Linux Include files are inconsistent */
#include <linux/fs.h>		/* From ancient versions, really needed? */
#undef __KERNEL__
#include "block/blk.h"		/* From ancient versions, really needed? */
#include "scsi/scsi.h"
#endif

#if	defined(HAVE_BROKEN_SCSI_SG_H) || \
	defined(HAVE_BROKEN_SRC_SCSI_SG_H)
/*
 * Be very careful in case that the Linux Kernel maintainers
 * unexpectedly fix the bugs in the Linux Lernel include files.
 * Only introduce the attempt for a workaround in case the include
 * files are broken anyway.
 */
#define	__user
#endif
#include "scsi/sg.h"
#if	defined(HAVE_BROKEN_SCSI_SG_H) || \
	defined(HAVE_BROKEN_SRC_SCSI_SG_H)
#undef	__user
#endif

#undef sense			/* conflict in struct cdrom_generic_command */
#include <linux/cdrom.h>

#if	defined(CDROM_PACKET_SIZE) && defined(CDROM_SEND_PACKET)
#define	USE_OLD_ATAPI
#endif

#include <glob.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-linux-sg.c-1.86";	/* The version for this transport*/

#ifndef	SCSI_IOCTL_GET_BUS_NUMBER
#define	SCSI_IOCTL_GET_BUS_NUMBER 0x5386
#endif

/*
 * XXX There must be a better way than duplicating things from system include
 * XXX files. This is stolen from /usr/src/linux/drivers/scsi/scsi.h
 */
#ifndef	DID_OK
#define	DID_OK		0x00 /* NO error				*/
#define	DID_NO_CONNECT	0x01 /* Couldn't connect before timeout period	*/
#define	DID_BUS_BUSY	0x02 /* BUS stayed busy through time out period	*/
#define	DID_TIME_OUT	0x03 /* TIMED OUT for other reason		*/
#define	DID_BAD_TARGET	0x04 /* BAD target.				*/
#define	DID_ABORT	0x05 /* Told to abort for some other reason	*/
#define	DID_PARITY	0x06 /* Parity error				*/
#define	DID_ERROR	0x07 /* Internal error				*/
#define	DID_RESET	0x08 /* Reset by somebody.			*/
#define	DID_BAD_INTR	0x09 /* Got an interrupt we weren't expecting.	*/
#endif

/*
 *  These indicate the error that occurred, and what is available.
 */
#ifndef	DRIVER_BUSY
#define	DRIVER_BUSY	0x01
#define	DRIVER_SOFT	0x02
#define	DRIVER_MEDIA	0x03
#define	DRIVER_ERROR	0x04

#define	DRIVER_INVALID	0x05
#define	DRIVER_TIMEOUT	0x06
#define	DRIVER_HARD	0x07
#define	DRIVER_SENSE	0x08
#endif

/*
 * XXX Should add extra space in buscookies and usalfiles for a "PP bus"
 * XXX and for two or more "ATAPI busses".
 */
#define	MAX_SCG		1256	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

#ifdef	USE_OLD_ATAPI
/*
 * # of virtual buses (schilly_host number)
 */
#define	MAX_SCHILLY_HOSTS	MAX_SCG
typedef struct {
	Uchar   typ:4;
	Uchar   bus:4;
	Uchar   host:8;
} ata_buscookies;
#endif

struct usal_local {
	int	usalfile;		/* Used for SG_GET_BUFSIZE ioctl()*/
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
  char *filenames[MAX_SCG][MAX_TGT][MAX_LUN];
	short	buscookies[MAX_SCG];
	int	pgbus;
	int	pack_id;		/* Should be a random number	*/
	int	drvers;
	short	isold;
	short	flags;
	long	xbufsize;
	char	*xbuf;
	char	*SCSIbuf;
#ifdef	USE_OLD_ATAPI
	ata_buscookies	bc[MAX_SCHILLY_HOSTS];
#endif
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

/*
 * Flag definitions
 */

#ifdef	SG_BIG_BUFF
#define	MAX_DMA_LINUX	SG_BIG_BUFF	/* Defined in include/scsi/sg.h	*/
#else
#define	MAX_DMA_LINUX	(4*1024)	/* Old Linux versions		*/
#endif

#ifndef	SG_MAX_SENSE
#	define	SG_MAX_SENSE	16	/* Too small for CCS / SCSI-2	*/
#endif					/* But cannot be changed	*/

#if	!defined(__i386) && !defined(i386) && !defined(mc68000)
#define	MISALIGN
#endif
/*#define	MISALIGN*/
/*#undef	SG_GET_BUFSIZE*/

#if	defined(USE_PG) && !defined(USE_PG_ONLY)
#include "scsi-linux-pg.c"
#endif
#ifdef	USE_OLD_ATAPI
#include "scsi-linux-ata.c"
#endif


#ifdef	MISALIGN
static	int	sg_getint(int *ip);
#endif
static	int	usalo_send(SCSI *usalp);
#ifdef	SG_IO
static	int	sg_rwsend(SCSI *usalp);
#endif
static	void	sg_clearnblock(int f);
static	BOOL	sg_setup(SCSI *usalp, int f, int busno, int tgt, int tlun, 
								int ataidx, char *origname);
static	void	sg_initdev(SCSI *usalp, int f);
static	int	sg_mapbus(SCSI *usalp, int busno, int ino);
static	BOOL	sg_mapdev(SCSI *usalp, int f, int *busp, int *tgtp, int *lunp,
								 int *chanp, int *inop, int ataidx);
#if defined(SG_SET_RESERVED_SIZE) && defined(SG_GET_RESERVED_SIZE)
static	long	sg_raisedma(SCSI *usalp, long newmax);
#endif
static	void	sg_settimeout(int f, int timeout);

int    sg_open_excl(char *device, int mode, BOOL beQuiet);

static BOOL get_max_secs(char *dirpath, int *outval);


BOOL check_linux_26() {
	int gen, tmp;
	struct utsname buf;
	return ( 0==uname( &buf ) && sscanf(buf.release, "%d.%d", &gen, &tmp)>1 && tmp>=6);
}

int sg_open_excl(char *device, int mode, BOOL beQuiet)
{
       int f;
       int i=0;
       long interval = beQuiet ? 400000 : 1000000;

       f = open(device, mode|O_EXCL);
       /* try to reopen locked/busy devices up to five times */
       for (i = 0; (i < 5) && (f == -1 && errno == EBUSY); i++) {
	       if(!beQuiet)
		       fprintf(stderr, "Error trying to open %s exclusively (%s)... %s\n",
				       device, strerror(errno), 
				       (i<4)?"retrying in 1 second.":"giving up.");
	       usleep(interval + interval * rand()/(RAND_MAX+1.0));
	       f = open(device, mode|O_EXCL);
       }
       if(i==5 && !beQuiet) {
	       FILE *g = fopen("/proc/mounts", "r");
	       if(g) {
		       char buf[80];
		       unsigned int len=strlen(device);
		       while(!feof(g) && !ferror(g)) {
			       if(fgets(buf, 79, g) && 0==strncmp(device, buf, len)) {
				       fprintf(stderr, "WARNING: %s seems to be mounted!\n", device);
			       }
		       }
		       fclose(g);
	       }
       }
       return f;
}

#if 0
// Dead code, that sysfs parts may become deprecated soon
void map_sg_to_block(char *device, int len) {
	char globpat[100];
	glob_t globbuf;
	snprintf(globpat, 100, "/sys/class/scsi_generic/%s/device/block:*", device+5);
	memset(&globbuf, 0, sizeof(glob_t));
	if(0==glob(globpat, GLOB_DOOFFS | GLOB_NOSORT, NULL, &globbuf)) {
		char *p = strrchr(globbuf.gl_pathv[0], ':');
		if(p) snprintf(device, len, "/dev/%s", p+1);
	}
	globfree(&globbuf);
}
#endif

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_version(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
#ifdef	USE_PG
#error pg-junk
		/*
		 * If we only have a Parallel port or only opened a handle
		 * for PP, just return PP version.
		 */
		if (usallocal(usalp)->pgbus == 0 ||
		    (usal_scsibus(usalp) >= 0 &&
		    usal_scsibus(usalp) == usallocal(usalp)->pgbus))
			return (pg_version(usalp, what));
#endif
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
		case SCG_KVERSION:
			{
				static	char kv[16];
				int	n;

				if (usallocal(usalp)->drvers >= 0) {
					n = usallocal(usalp)->drvers;
					snprintf(kv, sizeof (kv),
					"%d.%d.%d",
					n/10000, (n%10000)/100, n%100);

					return (kv);
				}
			}
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "sg", "Generic transport independent SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
#ifdef	USE_PG
	pg_help(usalp, f);
#endif
#ifdef	USE_OLD_ATAPI
	usalo_ahelp(usalp, f);
#endif
	__usal_help(f, "ATA", "ATA Packet specific SCSI transport using sg interface",
		"ATA:", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

#define in_scanmode (busno < 0 && tgt < 0 && tlun < 0)

/*
 * b/t/l is chopped of the device string.
 */
static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
	int	tlun	= usal_lun(usalp);
	register int	f;
	register int	i;
	register int	b;
	register int	t;
	register int	l;
	register int	nopen = 0;
	char		devname[64];
	int fake_atabus=0;

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

	struct stat statbuf;
	if(check_linux_26() && 0!=stat("/sys/kernel", &statbuf)) {
		static int warn_sysfs=1;
		if(warn_sysfs) {
			warn_sysfs=0;
			fprintf(stderr, "\nWarning, sysfs is not mounted on /sys!\n"
					"It is recommended to mount sysfs to allow better device configuration\n");
			sleep(5);
		}
	}

	if (device != NULL && *device != '\0') {
		fake_atabus=0;
		if(0==strncmp(device, "OLDATAPI", 8)) {
			device+=3;
			usalp->ops = &ata_ops;
			return (SCGO_OPEN(usalp, device));
		}
		else if(0==strncmp(device, "ATAPI", 5)) {
			if(check_linux_26()) {
				device+=5;
				fake_atabus=1;
				fprintf(stderr, "WARNING: the ATAPI: method is considered deprecated on modern kernels!\n"
						"Mapping device specification to ATA: method now.\n"
						"To force the old ATAPI: method, replace ATAPI: with OLDATAPI:\n");
			}
			else {
				usalp->ops = &ata_ops;
				return (SCGO_OPEN(usalp, device));
			}
		}
		else if(0==strncmp(device, "ATA", 3)) {
			fprintf(stderr, "WARNING: the ATA: method is considered deprecated on modern kernels!\n"
					"Use --devices to display the native names.\n");
			fake_atabus=1;
			device+=3;
		}
		if(device[0]==':')
			device++;

	}
	else if( ! in_scanmode ) {
			fprintf(stderr, "WARNING: the deprecated pseudo SCSI syntax found as device specification.\n"
					"Support for that may cease in the future versions of wodim. For now,\n"
					"the device will be mapped to a block device file where possible.\n"
					"Run \"wodim --devices\" for details.\n" );
			sleep(5);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);

		usallocal(usalp)->usalfile = -1;
		usallocal(usalp)->pgbus = -2;
		usallocal(usalp)->SCSIbuf = (char *)-1;
		usallocal(usalp)->pack_id = 5;
		usallocal(usalp)->drvers = -1;
		usallocal(usalp)->isold = -1;
		usallocal(usalp)->flags = 0;
		usallocal(usalp)->xbufsize = 0L;
		usallocal(usalp)->xbuf = NULL;

		for (b = 0; b < MAX_SCG; b++) {
			usallocal(usalp)->buscookies[b] = (short)-1;
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
					usallocal(usalp)->filenames[b][t][l] = NULL;
			}
		}
	}

	if (device != NULL && *device != '\0')
	{
		/* open ONE directly */
		b = -1;
		if (device && strncmp(device, "/dev/hd", 7) == 0 && device[8]=='\0') {
			b = device[7] - 'a';
			if (b < 0 || b > 25)
				b = -1;
		}
		if(b>=0 && fake_atabus)
			b+=1000;

		f = sg_open_excl(device, O_RDWR | O_NONBLOCK, FALSE);

		if (f < 0) {
			/*
			 * The pg driver has the same rules to decide whether
			 * to use openbydev. If we cannot open the device, it
			 * makes no sense to try the /dev/pg* driver.
			 */
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'",
						device);
			return (0);
		}
		sg_clearnblock(f);
		/* get some fake SCSI data */
		sg_mapdev(usalp, f, &busno, &tgt, &tlun, 0, 0, b);
		usal_settarget(usalp, busno, tgt, tlun);
		if (sg_setup(usalp, f, busno, tgt, tlun, b, device))
			return (++nopen);
	}
	else {
		/* scan and maybe keep one open, sg_setup decides */
#define HDX 0
#define SCD 1
#define SG 2
		int h;
retry_scan_open:
		for(h=HDX; h <= (fake_atabus ? HDX : SG) ; h++) {
			char *pattern;
			unsigned int first, last;
			switch(h) {
				case(HDX): 
					{
						pattern="/dev/hd%c";
						first='a';
						last='z';
						break;
					}
				case(SCD):
					{
						if(!check_linux_26())
							continue;
						pattern="/dev/scd%d";
						first=0;
						last=255;
						break;
					}
				case(SG):
					{
						if(check_linux_26())
							continue; 
#if 0
						/*
						 * Don't touch it on 2.6 until we have a proper locking scheme
						 */
							if(nopen<=0)
								fprintf(stderr, "Warning, using /dev/sg* for SG_IO operation. This method is considered harmful.\n");
							else if(found_scd)
								continue;
#endif
						pattern="/dev/sg%d";
						first=0;
						last=255;
						break;
					}
			}
			for(i=first; i<=last; i++) {
				snprintf(devname, sizeof (devname), pattern, i);
				f = sg_open_excl(devname, O_RDWR | O_NONBLOCK, in_scanmode);
				if (f < 0) {
					if (usalp->errstr)
						snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
								"Cannot open '%s'", devname);
				} else {
					if(h == HDX) { // double-check the capabilities on ATAPI devices
						int	iparm;

						if (ioctl(f, SG_GET_TIMEOUT, &iparm) < 0) {
							if (usalp->errstr)
								snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
										"SCSI unsupported with '%s'", devname);
							close(f);
							continue;
						}
					}
					sg_clearnblock(f);	/* Be very proper about this */
					
					/* construct the fake bus number hint, keep it readable */
					b=-1;
					if(h==HDX) {
						b=i-'a';
						if(!fake_atabus)
							b+=1000;
					}

					/* sg_setup returns false in scan mode, true if one single target was specified and opened */
					if (sg_setup(usalp, f, busno, tgt, tlun, b, devname))
						return (++nopen);

					if (in_scanmode)
						nopen++;
				}
			}

			if (nopen > 0 && usalp->errstr)
				usalp->errstr[0] = '\0';

			/* that's crap, should not be reached in non-scan mode.
			 * Let's see whether it can be mapped to an atapi
			 * device to emulate some old cludge's behaviour. */
			if(!in_scanmode && busno < 1000 && busno >=0) {
				fake_atabus=1;
				fprintf(stderr, "Unable to open this SCSI ID. Trying to map to old ATA syntax."
						"This workaround will disappear in the near future. Fix your configuration.");
				goto retry_scan_open;
			}
		}
	}

	if (usalp->debug > 0) for (b = 0; b < MAX_SCG; b++) {
		fprintf((FILE *)usalp->errfile,
			"Bus: %d cookie: %X\n",
			b, usallocal(usalp)->buscookies[b]);
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				if (usallocal(usalp)->usalfiles[b][t][l] != (short)-1) {
					fprintf((FILE *)usalp->errfile,
						"file (%d,%d,%d): %d\n",
						b, t, l, usallocal(usalp)->usalfiles[b][t][l]);
				}
			}
		}
	}

	return (nopen);
}

static int
usalo_close(SCSI *usalp)
{
	register int	f;
	register int	b;
	register int	t;
	register int	l;

	if (usalp->local == NULL)
		return (-1);

	for (b = 0; b < MAX_SCG; b++) {
		if (b == usallocal(usalp)->pgbus)
			continue;
		usallocal(usalp)->buscookies[b] = (short)-1;
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				f = usallocal(usalp)->usalfiles[b][t][l];
				if (f >= 0)
					close(f);
				usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
				if(usallocal(usalp)->filenames[b][t][l]) {
					free(usallocal(usalp)->filenames[b][t][l]);
					usallocal(usalp)->filenames[b][t][l]=NULL;
				}
			}
		}
	}
	if (usallocal(usalp)->xbuf != NULL) {
		free(usallocal(usalp)->xbuf);
		usallocal(usalp)->xbufsize = 0L;
		usallocal(usalp)->xbuf = NULL;
	}
#ifdef	USE_PG
	pg_close(usalp);
#endif
	return (0);
}

/*
 * The Linux kernel becomes more and more unmaintainable.
 * Every year, a new incompatible SCSI transport interface is added.
 * Each of them has it's own contradictory constraints.
 * While you cannot have O_NONBLOCK set during operation, at least one
 * of the drivers requires O_NONBLOCK to be set during open().
 * This is used to clear O_NONBLOCK immediately after open() succeeded.
 */
static void
sg_clearnblock(int f)
{
	int	n;

	n = fcntl(f, F_GETFL);
	n &= ~O_NONBLOCK;
	fcntl(f, F_SETFL, n);
}

/*!
 *
 * Return: TRUE when single target is chosen and was opened successfully, FALSE otherwise (on scans, etc).
 */

static BOOL
sg_setup(SCSI *usalp, int f, int busno, int tgt, int tlun, int ataidx, char *origname)
{
	int	n;
	int	Chan;
	int	Ino;
	int	Bus;
	int	Target;
	int	Lun;
	BOOL	onetarget = FALSE;

#ifdef	SG_GET_VERSION_NUM
	if (usallocal(usalp)->drvers < 0) {
		usallocal(usalp)->drvers = 0;
		if (ioctl(f, SG_GET_VERSION_NUM, &n) >= 0) {
			usallocal(usalp)->drvers = n;
			if (usalp->overbose) {
				fprintf((FILE *)usalp->errfile,
					"Linux sg driver version: %d.%d.%d\n",
					n/10000, (n%10000)/100, n%100);
			}
		}
	}
#endif
	if (usal_scsibus(usalp) >= 0 && usal_target(usalp) >= 0 && usal_lun(usalp) >= 0)
		onetarget = TRUE;

	sg_mapdev(usalp, f, &Bus, &Target, &Lun, &Chan, &Ino, ataidx);
	/*
	 * For old kernels try to make the best guess.
	 */
	Ino |= Chan << 8;
	n = sg_mapbus(usalp, Bus, Ino);
	if (Bus == -1) {
		Bus = n;
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"SCSI Bus: %d (mapped from %d)\n", Bus, Ino);
		}
	}

	if (Bus < 0 || Bus >= MAX_SCG || Target < 0 || Target >= MAX_TGT ||
						Lun < 0 || Lun >= MAX_LUN) {
		return (FALSE);
	}

	if (usallocal(usalp)->usalfiles[Bus][Target][Lun] == (short)-1)
		usallocal(usalp)->usalfiles[Bus][Target][Lun] = (short)f;

	if (usallocal(usalp)->filenames[Bus][Target][Lun] == NULL)
		usallocal(usalp)->filenames[Bus][Target][Lun] = strdup(origname);

	if (onetarget) {
		if (Bus == busno && Target == tgt && Lun == tlun) {
			sg_initdev(usalp, f);
			usallocal(usalp)->usalfile = f;	/* remember file for ioctl's */
			return (TRUE);
		} else {
			usallocal(usalp)->usalfiles[Bus][Target][Lun] = (short)-1;
			close(f);
		}
	} else {
		/*
		 * SCSI bus scanning may cause other generic SCSI activities to
		 * fail because we set the default timeout and clear command
		 * queues (in case of the old sg driver interface).
		 */
		sg_initdev(usalp, f);
		if (usallocal(usalp)->usalfile < 0)
			usallocal(usalp)->usalfile = f;	/* remember file for ioctl's */
	}
	return (FALSE);
}

static void
sg_initdev(SCSI *usalp, int f)
{
	struct sg_rep {
		struct sg_header	hd;
		unsigned char		rbuf[100];
	} sg_rep;
	int	n;
	int	i;
	struct stat sb;

	sg_settimeout(f, usalp->deftimeout);

	/*
	 * If it's a block device, don't read.... pre Linux-2.4 /dev/sg*
	 * definitely is a character device and we only need to clear the
	 * queue for old /dev/sg* versions. If somebody ever implements
	 * raw disk access for Linux, this test may fail.
	 */
	if (fstat(f, &sb) >= 0 && S_ISBLK(sb.st_mode))
		return;

	/* Eat any unwanted garbage from prior use of this device */

	n = fcntl(f, F_GETFL);	/* Be very proper about this */
	fcntl(f, F_SETFL, n|O_NONBLOCK);

	fillbytes((caddr_t)&sg_rep, sizeof (struct sg_header), '\0');
	sg_rep.hd.reply_len = sizeof (struct sg_header);

	/*
	 * This is really ugly.
	 * We come here if 'f' is related to a raw device. If Linux
	 * will ever have raw devices for /dev/hd* we may get problems.
	 * As long as there is no clean way to find out whether the
	 * filedescriptor 'f' is related to an old /dev/sg* or to
	 * /dev/hd*, we must assume that we found an old /dev/sg* and
	 * clean it up. Unfortunately, reading from /dev/hd* will
	 * Access the medium.
	 */
	for (i = 0; i < 1000; i++) {	/* Read at least 32k from /dev/sg* */
		int	ret;

		ret = read(f, &sg_rep, sizeof (sg_rep));
		if (ret > 0)
			continue;
		if (ret == 0 || errno == EAGAIN || errno == EIO)
			break;
		if (ret < 0 && i > 10)	/* Stop on repeated unknown error */
			break;
	}
	fcntl(f, F_SETFL, n);
}

static int
sg_mapbus(SCSI *usalp, int busno, int ino)
{
	register int	i;

	if (busno >= 0 && busno < MAX_SCG) {
		/*
		 * SCSI_IOCTL_GET_BUS_NUMBER worked.
		 * Now we have the problem that Linux does not properly number
		 * SCSI busses. The Bus number that Linux creates really is
		 * the controller (card) number. I case of multi SCSI bus
		 * cards we are lost.
		 */
		if (usallocal(usalp)->buscookies[busno] == (short)-1) {
			usallocal(usalp)->buscookies[busno] = ino;
			return (busno);
		}
		/*
		 * if (usallocal(usalp)->buscookies[busno] != (short)ino)
			errmsgno(EX_BAD, "Warning Linux Bus mapping botch.\n");
			*/
		return (busno);

	} else for (i = 0; i < MAX_SCG; i++) {
		if (usallocal(usalp)->buscookies[i] == (short)-1) {
			usallocal(usalp)->buscookies[i] = ino;
			return (i);
		}

		if (usallocal(usalp)->buscookies[i] == ino)
			return (i);
	}
	return (0);
}

static BOOL
sg_mapdev(SCSI *usalp, int f, int *busp, int *tgtp, int *lunp, int *chanp, 
			 int *inop, int ataidx)
{
	struct	sg_id {
		long	l1; /* target | lun << 8 | channel << 16 | low_ino << 24 */
		long	l2; /* Unique id */
	} sg_id;
	int	Chan;
	int	Ino;
	int	Bus;
	int	Target;
	int	Lun;

	if (ataidx >= 0) {
		/*
		 * The badly designed /dev/hd* interface maps everything
		 * to 0,0,0 so we need to do the mapping ourselves.
		 */
		*busp = (ataidx/1000) * 1000;
		*tgtp = ataidx%1000;
		*lunp = 0;
		if (chanp)
			*chanp = 0;
		if (inop)
			*inop = 0;
		return (TRUE);
	}
	if (ioctl(f, SCSI_IOCTL_GET_IDLUN, &sg_id))
		return (FALSE);
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"l1: 0x%lX l2: 0x%lX\n", sg_id.l1, sg_id.l2);
	}
	if (ioctl(f, SCSI_IOCTL_GET_BUS_NUMBER, &Bus) < 0) {
		Bus = -1;
	}

	Target	= sg_id.l1 & 0xFF;
	Lun	= (sg_id.l1 >> 8) & 0xFF;
	Chan	= (sg_id.l1 >> 16) & 0xFF;
	Ino	= (sg_id.l1 >> 24) & 0xFF;
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"Bus: %d Target: %d Lun: %d Chan: %d Ino: %d\n",
			Bus, Target, Lun, Chan, Ino);
	}
	*busp = Bus;
	*tgtp = Target;
	*lunp = Lun;
	if (chanp)
		*chanp = Chan;
	if (inop)
		*inop = Ino;
	return (TRUE);
}

#if defined(SG_SET_RESERVED_SIZE) && defined(SG_GET_RESERVED_SIZE)
/*
 * The way Linux does DMA resouce management is a bit curious.
 * It totally deviates from all other OS and forces long ugly code.
 * If we are opening all drivers for a SCSI bus scan operation, we need
 * to set the limit for all open devices.
 * This may use up all kernel memory ... so do the job carefully.
 *
 * A big problem is that SG_SET_RESERVED_SIZE does not return any hint
 * on whether the request did fail. The only way to find if it worked
 * is to use SG_GET_RESERVED_SIZE to read back the current values.
 */
static long
sg_raisedma(SCSI *usalp, long newmax)
{
	register int	b;
	register int	t;
	register int	l;
	register int	f;
		int	val;
		int	old;

	/*
	 * First try to raise the DMA limit to a moderate value that
	 * most likely does not use up all kernel memory.
	 */
	val = 126*1024;

	if (val > MAX_DMA_LINUX) {
		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++) {
					if ((f = SCGO_FILENO(usalp, b, t, l)) < 0)
						continue;
					old = 0;
					if (ioctl(f, SG_GET_RESERVED_SIZE, &old) < 0)
						continue;
					if (val > old)
						ioctl(f, SG_SET_RESERVED_SIZE, &val);
				}
			}
		}
	}

	/*
	 * Now to raise the DMA limit to what we really need.
	 */
	if (newmax > val) {
		val = newmax;
		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++) {
					if ((f = SCGO_FILENO(usalp, b, t, l)) < 0)
						continue;
					old = 0;
					if (ioctl(f, SG_GET_RESERVED_SIZE, &old) < 0)
						continue;
					if (val > old)
						ioctl(f, SG_SET_RESERVED_SIZE, &val);
				}
			}
		}
	}

	/*
	 * To make sure we did not fail (the ioctl does not report errors)
	 * we need to check the DMA limits. We return the smallest value.
	 */
	for (b = 0; b < MAX_SCG; b++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				if ((f = SCGO_FILENO(usalp, b, t, l)) < 0)
					continue;
				if (ioctl(f, SG_GET_RESERVED_SIZE, &val) < 0)
					continue;
				if (usalp->debug > 0) {
					fprintf((FILE *)usalp->errfile,
						"Target (%d,%d,%d): DMA max %d old max: %ld\n",
						b, t, l, val, newmax);
				}
				if (val < newmax)
					newmax = val;
			}
		}
	}
	return ((long)newmax);
}
#endif

static void freadstring(char *fn, char *out, int len) {
	FILE *fd=fopen(fn, "r");
	out[0]='\0';
	if(!fd) return;
	fgets(out, len, fd);
	fclose(fd);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	struct stat stbuf;
	long maxdma = MAX_DMA_LINUX;

#if defined(SG_SET_RESERVED_SIZE) && defined(SG_GET_RESERVED_SIZE)
	/*
	 * Use the curious new kernel interface found on Linux >= 2.2.10
	 * This interface first appeared in 2.2.6 but it was not working.
	 */
	if (usallocal(usalp)->drvers >= 20134)
		maxdma = sg_raisedma(usalp, amt);
#endif
	/*
	 * First try the modern kernel 2.6.1x way to detect the real maximum
	 * DMA for this specific device, then try the other methods.
	 */
	if(0==fstat(usallocal(usalp)->usalfile, &stbuf)) {
		/* that's ugly, there are so many symlinks in sysfs but none from
		 * major:minor to the relevant directory */
		long int major, minor, i;
		major=stbuf.st_rdev>>8;
		minor=stbuf.st_rdev&0xFF;
		if (usalp->debug > 0)
			fprintf(stderr, "Looking for data for major:minor: %d:%d\n", major, minor);
		glob_t globbuf;
		memset(&globbuf, 0, sizeof(glob_t));
		/* *dev files contain the major:minor strings to compare */
		glob("/sys/class/scsi_generic/*/device/block*/queue/max_sectors_kb", GLOB_DOOFFS | GLOB_NOSORT, NULL, &globbuf);
		glob("/sys/block/*/device/block*/queue/max_sectors_kb", GLOB_DOOFFS | GLOB_NOSORT | GLOB_APPEND, NULL, &globbuf);
		for(i=0;i<globbuf.gl_pathc; i++) {
			FILE *fd;
			char *cut, *ende;
			char buf[64];
			cut=strstr(globbuf.gl_pathv[i], "/device/")+4;
			*cut='\0';
			freadstring(globbuf.gl_pathv[i], buf, sizeof(buf));
			if(strtol(buf, &ende, 10) == major && ende && atoi(ende) == minor) {
				*cut='i';
				freadstring(globbuf.gl_pathv[i], buf, sizeof(buf));
				return(1024*atoi(buf));
			}

		}
		globfree(&globbuf);
	}
#ifdef	SG_GET_BUFSIZE
	/*
	 * We assume that all /dev/sg instances use the same
	 * maximum buffer size.
	 */
	maxdma = ioctl(usallocal(usalp)->usalfile, SG_GET_BUFSIZE, 0);
#endif
	if (maxdma < 0) {
#ifdef	USE_PG
		/*
		 * If we only have a Parallel port, just return PP maxdma.
		 */
		if (usallocal(usalp)->pgbus == 0)
			return (pg_maxdma(usalp, amt));
#endif
		if (usallocal(usalp)->usalfile >= 0)
			maxdma = MAX_DMA_LINUX;
	}
#ifdef	USE_PG
	if (usal_scsibus(usalp) == usallocal(usalp)->pgbus)
		return (pg_maxdma(usalp, amt));
	if ((usal_scsibus(usalp) < 0) && (pg_maxdma(usalp, amt) < maxdma))
		return (pg_maxdma(usalp, amt));
#endif
	return (maxdma);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	char	*ret;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"usalo_getbuf: %ld bytes\n", amt);
	}
	/*
	 * For performance reason, we allocate pagesize()
	 * bytes before the SCSI buffer to avoid
	 * copying the whole buffer contents when
	 * setting up the /dev/sg data structures.
	 */
	ret = valloc((size_t)(amt+getpagesize()));
	if (ret == NULL)
		return (ret);
	usalp->bufbase = ret;
	ret += getpagesize();
	usallocal(usalp)->SCSIbuf = ret;
	return ((void *)ret);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	register int	t;
	register int	l;

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (usalp->local == NULL)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++)
			if (usallocal(usalp)->usalfiles[busno][t][l] >= 0)
				return (TRUE);
	}
	return (FALSE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (usalp->local == NULL)
		return (-1);

	return ((int)usallocal(usalp)->usalfiles[busno][tgt][tlun]);
}

static int
usalo_initiator_id(SCSI *usalp)
{
#ifdef	USE_PG
	if (usal_scsibus(usalp) == usallocal(usalp)->pgbus)
		return (pg_initiator_id(usalp));
#endif
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return -1;
#if 0
	/*
	 * Who exactly needs this information? Just for some bitching in wodim?
	 * Is this an _abstraction_ layer or spam layer?
	 */
#ifdef	USE_PG
	if (usal_scsibus(usalp) == usallocal(usalp)->pgbus)
		return (pg_isatapi(usalp));
#endif

	/*
	 * The /dev/hd* interface always returns TRUE for SG_EMULATED_HOST.
	 * So this is completely useless.
	 */
	if (usallocal(usalp)->flags & LF_ATA)
		return (-1);

#ifdef	SG_EMULATED_HOST
	{
	int	emulated = FALSE;

	/*
	 * XXX Should we use this at all?
	 * XXX The badly designed /dev/hd* interface always
	 * XXX returns TRUE, even when used with e.g. /dev/sr0.
	 */
	if (ioctl(usalp->fd, SG_EMULATED_HOST, &emulated) >= 0)
		return (emulated != 0);
	}
#endif
	return (-1);
#endif
}

static int
usalo_reset(SCSI *usalp, int what)
{
#ifdef	SG_SCSI_RESET
	int	f = usalp->fd;
	int	func = -1;
#endif
#ifdef	USE_PG
	if (usal_scsibus(usalp) == usallocal(usalp)->pgbus)
		return (pg_reset(usalp, what));
#endif
	/*
	 * Do we have a SCSI reset in the Linux sg driver?
	 */
#ifdef	SG_SCSI_RESET
	/*
	 * Newer Linux sg driver seem to finally implement it...
	 */
#ifdef	SG_SCSI_RESET_NOTHING
	func = SG_SCSI_RESET_NOTHING;
	if (ioctl(f, SG_SCSI_RESET, &func) >= 0) {
		if (what == SCG_RESET_NOP)
			return (0);
#ifdef	SG_SCSI_RESET_DEVICE
		if (what == SCG_RESET_TGT) {
			func = SG_SCSI_RESET_DEVICE;
			if (ioctl(f, SG_SCSI_RESET, &func) >= 0)
				return (0);
		}
#endif
#ifdef	SG_SCSI_RESET_BUS
		if (what == SCG_RESET_BUS) {
			func = SG_SCSI_RESET_BUS;
			if (ioctl(f, SG_SCSI_RESET, &func) >= 0)
				return (0);
		}
#endif
	}
#endif
#endif
	return (-1);
}

static void
sg_settimeout(int f, int tmo)
{
#ifndef HZ
	static int HZ=0;
	if (!HZ)
		HZ = sysconf(_SC_CLK_TCK);
#endif
	tmo *= HZ;
	if (tmo)
		tmo += HZ/2;

	if (ioctl(f, SG_SET_TIMEOUT, &tmo) < 0)
		comerr("Cannot set SG_SET_TIMEOUT.\n");
}

/*
 * Get misaligned int.
 * Needed for all recent processors (sparc/ppc/alpha)
 * because the /dev/sg design forces us to do misaligned
 * reads of integers.
 */
#ifdef	MISALIGN
static int
sg_getint(int *ip)
{
		int	ret;
	register char	*cp = (char *)ip;
	register char	*tp = (char *)&ret;
	register int	i;

	for (i = sizeof (int); --i >= 0; )
		*tp++ = *cp++;

	return (ret);
}
#define	GETINT(a)	sg_getint(&(a))
#else
#define	GETINT(a)	(a)
#endif

#ifdef	SG_IO
static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	int		ret;
	sg_io_hdr_t	sg_io;
	struct timeval	to;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		return (0);
	}
	if (usallocal(usalp)->isold > 0) {
		return (sg_rwsend(usalp));
	}
	fillbytes((caddr_t)&sg_io, sizeof (sg_io), '\0');

	sg_io.interface_id = 'S';

	if (sp->flags & SCG_RECV_DATA) {
		sg_io.dxfer_direction = SG_DXFER_FROM_DEV;
	} else if (sp->size > 0) {
		sg_io.dxfer_direction = SG_DXFER_TO_DEV;
	} else {
		sg_io.dxfer_direction = SG_DXFER_NONE;
	}
	sg_io.cmd_len = sp->cdb_len;
	if (sp->sense_len > SG_MAX_SENSE)
		sg_io.mx_sb_len = SG_MAX_SENSE;
	else
		sg_io.mx_sb_len = sp->sense_len;
	sg_io.dxfer_len = sp->size;
	sg_io.dxferp = sp->addr;
	sg_io.cmdp = sp->cdb.cmd_cdb;
	sg_io.sbp = sp->u_sense.cmd_sense;
	sg_io.timeout = sp->timeout*1000;
	sg_io.flags |= SG_FLAG_DIRECT_IO;

	ret = ioctl(usalp->fd, SG_IO, &sg_io);
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"ioctl ret: %d\n", ret);
	}

	if (ret < 0) {
		sp->ux_errno = geterrno();
		/*
		 * Check if SCSI command cound not be send at all.
		 * Linux usually returns EINVAL for an unknoen ioctl.
		 * In case somebody from the Linux kernel team learns that the
		 * corect errno would be ENOTTY, we check for this errno too.
		 */
		if ((sp->ux_errno == ENOTTY || sp->ux_errno == EINVAL) &&
		    usallocal(usalp)->isold < 0) {
			usallocal(usalp)->isold = 1;
			return (sg_rwsend(usalp));
		}
		if (sp->ux_errno == ENXIO ||
		    sp->ux_errno == EINVAL || sp->ux_errno == EACCES) {
			return (-1);
		}
	}

	sp->u_scb.cmd_scb[0] = sg_io.status;
	sp->sense_count = sg_io.sb_len_wr;

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"host_status: %02X driver_status: %02X\n",
				sg_io.host_status, sg_io.driver_status);
	}

	switch (sg_io.host_status) {

	case DID_OK:
			/*
			 * If there is no DMA overrun and there is a
			 * SCSI Status byte != 0 then the SCSI cdb transport
			 * was OK and sp->error must be SCG_NO_ERROR.
			 */
			if ((sg_io.driver_status & DRIVER_SENSE) != 0) {
				if (sp->ux_errno == 0)
					sp->ux_errno = EIO;

				if (sp->u_sense.cmd_sense[0] != 0 &&
				    sp->u_scb.cmd_scb[0] == 0) {
					/*
					 * The Linux SCSI system up to 2.4.xx
					 * trashes the status byte in the
					 * kernel. This is true at least for
					 * ide-scsi emulation. Until this gets
					 * fixed, we need this hack.
					 */
					sp->u_scb.cmd_scb[0] = ST_CHK_COND;
					if (sp->sense_count == 0)
						sp->sense_count = SG_MAX_SENSE;

					if ((sp->u_sense.cmd_sense[2] == 0) &&
					    (sp->u_sense.cmd_sense[12] == 0) &&
					    (sp->u_sense.cmd_sense[13] == 0)) {
						/*
						 * The Linux SCSI system will
						 * send a request sense for
						 * even a dma underrun error.
						 * Clear CHECK CONDITION state
						 * in case of No Sense.
						 */
						sp->u_scb.cmd_scb[0] = 0;
						sp->u_sense.cmd_sense[0] = 0;
						sp->sense_count = 0;
					}
				}
			}
			break;

	case DID_NO_CONNECT:	/* Arbitration won, retry NO_CONNECT? */
			sp->error = SCG_RETRYABLE;
			break;
	case DID_BAD_TARGET:
			sp->error = SCG_FATAL;
			break;

	case DID_TIME_OUT:
		__usal_times(usalp);

		if (sp->timeout > 1 && usalp->cmdstop->tv_sec == 0) {
			sp->u_scb.cmd_scb[0] = 0;
			sp->error = SCG_FATAL;	/* a selection timeout */
		} else {
			sp->error = SCG_TIMEOUT;
		}
		break;

	default:
		to.tv_sec = sp->timeout;
		to.tv_usec = 500000;
		__usal_times(usalp);

		if (usalp->cmdstop->tv_sec < to.tv_sec ||
		    (usalp->cmdstop->tv_sec == to.tv_sec &&
			usalp->cmdstop->tv_usec < to.tv_usec)) {

			sp->ux_errno = 0;
			sp->error = SCG_TIMEOUT;	/* a timeout */
		} else {
			sp->error = SCG_RETRYABLE;
		}
		break;
	}
	if (sp->error && sp->ux_errno == 0)
		sp->ux_errno = EIO;

	sp->resid = sg_io.resid;
	return (0);
}
#else
#	define	sg_rwsend	usalo_send
#endif

static int
sg_rwsend(SCSI *usalp)
{
	int		f = usalp->fd;
	struct usal_cmd	*sp = usalp->scmd;
	struct sg_rq	*sgp;
	struct sg_rq	*sgp2;
	int	i;
	int	pack_len;
	int	reply_len;
	int	amt = sp->cdb_len;
	struct sg_rq {
		struct sg_header	hd;
		unsigned char		buf[MAX_DMA_LINUX+SCG_MAX_CMD];
	} sg_rq;
#ifdef	SG_GET_BUFSIZE		/* We may use a 'sg' version 2 driver	*/
	char	driver_byte;
	char	host_byte;
	char	msg_byte;
	char	status_byte;
#endif

	if (f < 0) {
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		return (0);
	}
#ifdef	USE_PG
	if (usal_scsibus(usalp) == usallocal(usalp)->pgbus)
		return (pg_send(usalp));
#endif
	if (sp->timeout != usalp->deftimeout)
		sg_settimeout(f, sp->timeout);


	sgp2 = sgp = &sg_rq;
	if (sp->addr == usallocal(usalp)->SCSIbuf) {
		sgp = (struct sg_rq *)
			(usallocal(usalp)->SCSIbuf - (sizeof (struct sg_header) + amt));
		sgp2 = (struct sg_rq *)
			(usallocal(usalp)->SCSIbuf - (sizeof (struct sg_header)));
	} else {
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"DMA addr: 0x%8.8lX size: %d - using copy buffer\n",
				(long)sp->addr, sp->size);
		}
		if (sp->size > (int)(sizeof (sg_rq.buf) - SCG_MAX_CMD)) {

			if (usallocal(usalp)->xbuf == NULL) {
				usallocal(usalp)->xbufsize = usalp->maxbuf;
				usallocal(usalp)->xbuf =
					malloc(usallocal(usalp)->xbufsize +
						SCG_MAX_CMD +
						sizeof (struct sg_header));
				if (usalp->debug > 0) {
					fprintf((FILE *)usalp->errfile,
						"Allocated DMA copy buffer, addr: 0x%8.8lX size: %ld\n",
						(long)usallocal(usalp)->xbuf,
						usalp->maxbuf);
				}
			}
			if (usallocal(usalp)->xbuf == NULL ||
				sp->size > usallocal(usalp)->xbufsize) {
				errno = ENOMEM;
				return (-1);
			}
			sgp2 = sgp = (struct sg_rq *)usallocal(usalp)->xbuf;
		}
	}

	/*
	 * This is done to avoid misaligned access of sgp->some_int
	 */
	pack_len = sizeof (struct sg_header) + amt;
	reply_len = sizeof (struct sg_header);
	if (sp->flags & SCG_RECV_DATA) {
		reply_len += sp->size;
	} else {
		pack_len += sp->size;
	}

#ifdef	MISALIGN
	/*
	 * sgp->some_int may be misaligned if (sp->addr == SCSIbuf)
	 * This is no problem on Intel porocessors, however
	 * all other processors don't like it.
	 * sizeof (struct sg_header) + amt is usually not a multiple of
	 * sizeof (int). For this reason, we fill in the values into sg_rq
	 * which is always corectly aligned and then copy it to the real
	 * location if this location differs from sg_rq.
	 * Never read/write directly to sgp->some_int !!!!!
	 */
	fillbytes((caddr_t)&sg_rq, sizeof (struct sg_header), '\0');

	sg_rq.hd.pack_len = pack_len;
	sg_rq.hd.reply_len = reply_len;
	sg_rq.hd.pack_id = usallocal(usalp)->pack_id++;
/*	sg_rq.hd.result = 0;	not needed because of fillbytes() */

	if ((caddr_t)&sg_rq != (caddr_t)sgp)
		movebytes((caddr_t)&sg_rq, (caddr_t)sgp, sizeof (struct sg_header));
#else
	fillbytes((caddr_t)sgp, sizeof (struct sg_header), '\0');

	sgp->hd.pack_len = pack_len;
	sgp->hd.reply_len = reply_len;
	sgp->hd.pack_id = usallocal(usalp)->pack_id++;
/*	sgp->hd.result = 0;	not needed because of fillbytes() */
#endif
	if (amt == 12)
		sgp->hd.twelve_byte = 1;


	for (i = 0; i < amt; i++) {
		sgp->buf[i] = sp->cdb.cmd_cdb[i];
	}
	if (!(sp->flags & SCG_RECV_DATA)) {
		if ((void *)sp->addr != (void *)&sgp->buf[amt])
			movebytes(sp->addr, &sgp->buf[amt], sp->size);
		amt += sp->size;
	}
#ifdef	SG_GET_BUFSIZE
	sgp->hd.want_new  = 1;			/* Order new behaviour	*/
	sgp->hd.cdb_len	  = sp->cdb_len;	/* Set CDB length	*/
	if (sp->sense_len > SG_MAX_SENSE)
		sgp->hd.sense_len = SG_MAX_SENSE;
	else
		sgp->hd.sense_len = sp->sense_len;
#endif
	i = sizeof (struct sg_header) + amt;
	if ((amt = write(f, sgp, i)) < 0) {			/* write */
		sg_settimeout(f, usalp->deftimeout);
		return (-1);
	} else if (amt != i) {
		errmsg("usalo_send(%s) wrote %d bytes (expected %d).\n",
						usalp->cmdname, amt, i);
	}

	if (sp->addr == usallocal(usalp)->SCSIbuf) {
		movebytes(sgp, sgp2, sizeof (struct sg_header));
		sgp = sgp2;
	}
	sgp->hd.sense_buffer[0] = 0;
	if ((amt = read(f, sgp, reply_len)) < 0) {		/* read */
		sg_settimeout(f, usalp->deftimeout);
		return (-1);
	}

	if (sp->flags & SCG_RECV_DATA && ((void *)sgp->buf != (void *)sp->addr)) {
		movebytes(sgp->buf, sp->addr, sp->size);
	}
	sp->ux_errno = GETINT(sgp->hd.result);		/* Unaligned read */
	sp->error = SCG_NO_ERROR;

#ifdef	SG_GET_BUFSIZE
	if (sgp->hd.grant_new) {
		sp->sense_count = sgp->hd.sense_len;
		pack_len    = GETINT(sgp->hd.sg_cmd_status);	/* Unaligned read */
		driver_byte = (pack_len  >> 24) & 0xFF;
		host_byte   = (pack_len  >> 16) & 0xFF;
		msg_byte    = (pack_len  >> 8) & 0xFF;
		status_byte =  pack_len  & 0xFF;

		switch (host_byte) {

		case DID_OK:
				if ((driver_byte & DRIVER_SENSE ||
				    sgp->hd.sense_buffer[0] != 0) &&
				    status_byte == 0) {
					/*
					 * The Linux SCSI system up to 2.4.xx
					 * trashes the status byte in the
					 * kernel. This is true at least for
					 * ide-scsi emulation. Until this gets
					 * fixed, we need this hack.
					 */
					status_byte = ST_CHK_COND;
					if (sgp->hd.sense_len == 0)
						sgp->hd.sense_len = SG_MAX_SENSE;

					if ((sp->u_sense.cmd_sense[2] == 0) &&
					    (sp->u_sense.cmd_sense[12] == 0) &&
					    (sp->u_sense.cmd_sense[13] == 0)) {
						/*
						 * The Linux SCSI system will
						 * send a request sense for
						 * even a dma underrun error.
						 * Clear CHECK CONDITION state
						 * in case of No Sense.
						 */
						sp->u_scb.cmd_scb[0] = 0;
						sp->u_sense.cmd_sense[0] = 0;
						sp->sense_count = 0;
					}
				}
				break;

		case DID_NO_CONNECT:	/* Arbitration won, retry NO_CONNECT? */
				sp->error = SCG_RETRYABLE;
				break;

		case DID_BAD_TARGET:
				sp->error = SCG_FATAL;
				break;

		case DID_TIME_OUT:
				sp->error = SCG_TIMEOUT;
				break;

		default:
				sp->error = SCG_RETRYABLE;

				if ((driver_byte & DRIVER_SENSE ||
				    sgp->hd.sense_buffer[0] != 0) &&
				    status_byte == 0) {
					status_byte = ST_CHK_COND;
					sp->error = SCG_NO_ERROR;
				}
				if (status_byte != 0 && sgp->hd.sense_len == 0) {
					sgp->hd.sense_len = SG_MAX_SENSE;
					sp->error = SCG_NO_ERROR;
				}
				break;

		}
		if ((host_byte != DID_OK || status_byte != 0) && sp->ux_errno == 0)
			sp->ux_errno = EIO;
		sp->u_scb.cmd_scb[0] = status_byte;
		if (status_byte & ST_CHK_COND) {
			sp->sense_count = sgp->hd.sense_len;
			movebytes(sgp->hd.sense_buffer, sp->u_sense.cmd_sense, sp->sense_count);
		}
	} else
#endif
	{
		if (GETINT(sgp->hd.result) == EBUSY) {	/* Unaligned read */
			struct timeval to;

			to.tv_sec = sp->timeout;
			to.tv_usec = 500000;
			__usal_times(usalp);

			if (sp->timeout > 1 && usalp->cmdstop->tv_sec == 0) {
				sp->u_scb.cmd_scb[0] = 0;
				sp->ux_errno = EIO;
				sp->error = SCG_FATAL;	/* a selection timeout */
			} else if (usalp->cmdstop->tv_sec < to.tv_sec ||
			    (usalp->cmdstop->tv_sec == to.tv_sec &&
				usalp->cmdstop->tv_usec < to.tv_usec)) {

				sp->ux_errno = EIO;
				sp->error = SCG_TIMEOUT;	/* a timeout */
			} else {
				sp->error = SCG_RETRYABLE;	/* may be BUS_BUSY */
			}
		}

		if (sp->flags & SCG_RECV_DATA)
			sp->resid = (sp->size + sizeof (struct sg_header)) - amt;
		else
			sp->resid = 0;	/* sg version1 cannot return DMA resid count */

		if (sgp->hd.sense_buffer[0] != 0) {
			sp->scb.chk = 1;
			sp->sense_count = SG_MAX_SENSE;
			movebytes(sgp->hd.sense_buffer, sp->u_sense.cmd_sense, sp->sense_count);
			if (sp->ux_errno == 0)
				sp->ux_errno = EIO;
		}
	}

	if (usalp->verbose > 0 && usalp->debug > 0) {
#ifdef	SG_GET_BUFSIZE
		fprintf((FILE *)usalp->errfile,
				"status: 0x%08X pack_len: %d, reply_len: %d pack_id: %d result: %d wn: %d gn: %d cdb_len: %d sense_len: %d sense[0]: %02X\n",
				GETINT(sgp->hd.sg_cmd_status),
				GETINT(sgp->hd.pack_len),
				GETINT(sgp->hd.reply_len),
				GETINT(sgp->hd.pack_id),
				GETINT(sgp->hd.result),
				sgp->hd.want_new,
				sgp->hd.grant_new,
				sgp->hd.cdb_len,
				sgp->hd.sense_len,
				sgp->hd.sense_buffer[0]);
#else
		fprintf((FILE *)usalp->errfile,
				"pack_len: %d, reply_len: %d pack_id: %d result: %d sense[0]: %02X\n",
				GETINT(sgp->hd.pack_len),
				GETINT(sgp->hd.reply_len),
				GETINT(sgp->hd.pack_id),
				GETINT(sgp->hd.result),
				sgp->hd.sense_buffer[0]);
#endif
#ifdef	DEBUG
		fprintf((FILE *)usalp->errfile, "sense: ");
		for (i = 0; i < 16; i++)
			fprintf((FILE *)usalp->errfile, "%02X ", sgp->hd.sense_buffer[i]);
		fprintf((FILE *)usalp->errfile, "\n");
#endif
	}

	if (sp->timeout != usalp->deftimeout)
		sg_settimeout(f, usalp->deftimeout);
	return (0);
};

#define HAVE_NAT_NAMES
static char * usalo_natname(SCSI *usalp, int busno, int tgt, int tlun) {
	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN)
		return "BADID";
	return usallocal(usalp)->filenames[busno][tgt][tlun];
}
