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

/* @(#)scsi_scan.c	1.19 04/04/16 Copyright 1997-2004 J. Schilling */
/*
 *	Scan SCSI Bus.
 *	Stolen from sformat. Need a more general form to
 *	re-use it in sformat too.
 *
 *	Copyright (c) 1997-2004 J. Schilling
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
#include <errno.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsi_scan.h"
#include "wodim.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

static	void	print_product(FILE *f, struct scsi_inquiry *ip);
int	select_target(SCSI *usalp, FILE *f);

#define MAXDEVCOUNT (256+26)

extern BOOL check_linux_26();

static void print_product(FILE *f, struct  scsi_inquiry *ip) {
	fprintf(f, "'%.8s' ", ip->vendor_info);
	fprintf(f, "'%.16s' ", ip->prod_ident);
	fprintf(f, "'%.4s' ", ip->prod_revision);
	if (ip->add_len < 31) {
		fprintf(f, "NON CCS ");
	}
	usal_fprintdev(f, ip);
}

SCSI * open_auto(int64_t need_size, int debug, int lverbose) {
	int res;
	SCSI * usalp = NULL;
	char  errstr[80];
	
#ifdef __linux__
	/* quick-and-dirty code but should do what is supposed to, more quickly */

		/*
		 * For Linux, try these strategies, in order:
		 * 1. stat /dev/cdrw or /dev/dvdrw, depending on size we need.
		 * 2. Read /proc/sys/dev/cdrom/info, look for a CD-R/DVD-R.
		 *    Will fail for kernel 2.4 or if cdrom module not loaded.
		 * 3. stat /dev/cdrom, just assume that it can write media.

     An example for procfs file contents, beware of the TABs

---
CD-ROM information, Id: cdrom.c 3.20 2003/12/17

drive name:		hdc     hda
drive speed:		40      40
drive # of slots:       1       1
Can close tray:         1       1
Can open tray:          1       1
Can lock tray:          1       1
Can change speed:       1       1
Can select disk:        0       0
Can read multisession:  1       1
Can read MCN:           1       1
Reports media changed:  1       1
Can play audio:         1       1
Can write CD-R:		0       1
Can write CD-RW:        0       1
Can read DVD:           1       1
Can write DVD-R:        0       1
Can write DVD-RAM:      0       1
Can read MRW:           0       1
Can write MRW:          0       1
Can write RAM:          0       1

---
*/
	struct stat statbuf;
	/* XXX Good guess? BD-RE recorders may not support CDRW anymore... */
	char *type="CD-R", *key="Can write CD-R:", *guessdev="/dev/cdrw", *result=NULL;
	FILE *fh;

	if( need_size > 360000*2048 ) {
		type="DVD-R";
		guessdev="/dev/dvdrw";
		key="Can write DVD-R:";
	}

	if(need_size>10240) /* don't bother with weird numbers */
		fprintf(stderr, "Looking for a %s drive to store %.2f MiB...\n", type, (float)need_size/1048576.0);
	if(0==stat(guessdev, &statbuf))
		result=guessdev;
	else if(0!= (fh = fopen("/proc/sys/dev/cdrom/info", "r")) ) {
		/* ok, going the hard way */
		char *nameline=NULL;
		static char buf[256];
		int kn = strlen(key);

		buf[255]='\0';

		while(fgets(buf, sizeof(buf), fh)) {
			if(0==strncmp(buf, "drive name:", 11))
				nameline=strdup(buf);
			if(nameline && 0==strncmp(buf, key, kn)) {
				int p=kn;
				char *descptr=nameline+11; /* start at the known whitespace */
				while(p<sizeof(buf) && buf[p]) {
					if(buf[p]=='1' || buf[p]=='0') {
						/* find the beginning of the descriptor */
						for(;isspace((Uchar) *descptr);descptr++)
							;
					}
					if(buf[p]=='1') {
						result=descptr-5;
						/* terminate on space/newline and stop there */
						for(;*descptr;descptr++) {
							if(isspace((Uchar) *descptr))
								*(descptr--)='\0';
						}
						strncpy(result, "/dev/", 5);
						break;
					}
					else { /* no hit, move to after word ending */
						for(; *descptr && ! isspace((Uchar) *descptr); descptr++)
							;
					}
					p++;
				}
			}

		}
		fclose(fh);
	}

	if(result)
		fprintf(stderr, "Detected %s drive: %s\n", type, result);
	if (0==stat("/dev/cdrom", &statbuf)) {
		result = "/dev/cdrom";
		fprintf(stderr, "Using /dev/cdrom of unknown capabilities\n");
	}
	if(result)
		return usal_open(result, errstr, sizeof(errstr), debug, lverbose);
#endif /* __linux__ */

	usalp = usal_open(NULL, errstr, sizeof(errstr), debug, lverbose);
	if(!usalp)
		return NULL;
	res = list_devices(usalp, stdout, 1);
	if(res>0)
		return usalp;
	else
		usal_close(usalp);
	return NULL;
}

int list_devices(SCSI *usalp, FILE *f, int pickup_first) {
	int	initiator;
	int	i;
	int	bus;
	int	tgt;
	int	lun = 0;
	BOOL	have_tgt;

	int fd, ndevs=0;
  	struct stat statbuf;
	char *lines[MAXDEVCOUNT];
	char buf[256], perms[8], *p;


	usalp->silent++;

	/* XXX should be done before opening usal fprintf(stderr, "Beginning native device scan. This may take a while if devices are busy...\n"); */

	for (bus = 0; bus < 1256; bus++) {
		usal_settarget(usalp, bus, 0, 0);

		if (!usal_havebus(usalp, bus))
			continue;

		initiator = usal_initiator_id(usalp);
		//fprintf(f, "scsibus%d:\n", bus);

		for (tgt = 0; tgt < 16; tgt++) {
			usal_settarget(usalp, bus, tgt, lun);
			have_tgt = unit_ready(usalp) || usalp->scmd->error != SCG_FATAL;

			if (!have_tgt && tgt > 7) {
				if (usalp->scmd->ux_errno == EINVAL)
					break;
				continue;
			}

			fd=usal_fileno(usalp, bus, tgt, lun);
			strcpy(perms,"------");
			if(fd>=0 && 0==fstat(fd, &statbuf)) {
				if(statbuf.st_mode&S_IRUSR) perms[0]= 'r';
				if(statbuf.st_mode&S_IWUSR) perms[1]= 'w';
				if(statbuf.st_mode&S_IRGRP) perms[2]= 'r';
				if(statbuf.st_mode&S_IWGRP) perms[3]= 'w';
				if(statbuf.st_mode&S_IROTH) perms[4]= 'r';
				if(statbuf.st_mode&S_IWOTH) perms[5]= 'w';
			}
			getdev(usalp, FALSE);
			if(usalp->inq->type == INQ_ROMD || usalp->inq->type == INQ_WORM) {
				char *p;

				for(p=usalp->inq->vendor_info + 7 ; p >= usalp->inq->vendor_info; p--) {
					if(isspace((unsigned char)*p))
						*p='\0';
					else
						break;
				}
				for(p=usalp->inq->prod_ident + 15 ; p >= usalp->inq->prod_ident; p--) {
					if(isspace((unsigned char)*p))
						*p='\0';
					else
						break;
				}
				snprintf(buf, sizeof(buf), "%2d  dev='%s'\t%s : '%.8s' '%.16s'\n", ndevs, usal_natname(usalp, bus, tgt, lun), perms, usalp->inq->vendor_info, usalp->inq->prod_ident);
				/* alternative use, only select the first device */
				if(pickup_first) {
					printf("Using drive: %s\n", usal_natname(usalp, bus, tgt, lun));
					return 1;
				}
				lines[ndevs++]=strdup(buf);
			}

		}
	}
	usalp->silent--;

	/* should have been returned before if there was a recorder */
	if(pickup_first)
		return 0;

	/* now start the output */

	fprintf(stdout, "%s: Overview of accessible drives (%d found) :\n"
			"-------------------------------------------------------------------------\n",
			get_progname(), ndevs);
	for(i=0;i<ndevs;i++) {
		fprintf(stdout, "%s", lines[i]);
		free(lines[i]);
	}
	fprintf(stdout,	"-------------------------------------------------------------------------\n");

	return ndevs;
}

int select_target(SCSI *usalp, FILE *f) {
	int	initiator;
#ifdef	FMT
	int	cscsibus = usal_scsibus(usalp);
	int	ctarget  = usal_target(usalp);
	int	clun	 = usal_lun(usalp);
#endif
	int	n;
	int	low	= -1;
	int	high	= -1;
	int	amt	= 0;
	int	bus;
	int	tgt;
	int	lun = 0;
	BOOL	have_tgt;

	usalp->silent++;

	for (bus = 0; bus < 1256; bus++) {
		usal_settarget(usalp, bus, 0, 0);

		if (!usal_havebus(usalp, bus))
			continue;

		initiator = usal_initiator_id(usalp);
		fprintf(f, "scsibus%d:\n", bus);

		for (tgt = 0; tgt < 16; tgt++) {
			n = bus*100 + tgt;

			usal_settarget(usalp, bus, tgt, lun);
			have_tgt = unit_ready(usalp) || usalp->scmd->error != SCG_FATAL;

			if (!have_tgt && tgt > 7) {
				if (usalp->scmd->ux_errno == EINVAL)
					break;
				continue;
			}

#ifdef	FMT
			if (print_disknames(bus, tgt, -1) < 8)
				fprintf(f, "\t");
			else
				fprintf(f, " ");
#else
			fprintf(f, "\t");
#endif
			if (fprintf(f, "%d,%d,%d", bus, tgt, lun) < 8)
				fprintf(f, "\t");
			else
				fprintf(f, " ");
			fprintf(f, "%3d) ", n);
			if (tgt == initiator) {
				fprintf(f, "HOST ADAPTOR\n");
				continue;
			}
			if (!have_tgt) {
				/*
				 * Hack: fd -> -2 means no access
				 */
				fprintf(f, "%c\n", usalp->fd == -2 ? '?':'*');
				continue;
			}
			amt++;
			if (low < 0)
				low = n;
			high = n;

			getdev(usalp, FALSE);
			print_product(f, usalp->inq);
		}
	}
	usalp->silent--;

	if (low < 0) {
		errmsgno(EX_BAD, "No target found.\n");
		return (0);
	}
	n = -1;
#ifdef	FMT
	getint("Select target", &n, low, high);
	bus = n/100;
	tgt = n%100;
	usal_settarget(usalp, bus, tgt, lun);
	return (select_unit(usalp));

	usal_settarget(usalp, cscsibus, ctarget, clun);
#endif
	return (amt);
}

