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

/* @(#)scsiopen.c	1.95 04/01/14 Copyright 1995,2000 J. Schilling */
/*
 *	SCSI command functions for cdrecord
 *
 *	Copyright (c) 1995,2000 J. Schilling
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

/*
 * NOTICE:	The Philips CDD 521 has several firmware bugs.
 *		One of them is not to respond to a SCSI selection
 *		within 200ms if the general load on the
 *		SCSI bus is high. To deal with this problem
 *		most of the SCSI commands are send with the
 *		SCG_CMD_RETRY flag enabled.
 *
 *		Note that the only legal place to assign
 *		values to usal_scsibus() usal_target() and usal_lun()
 *		is usal_settarget().
 */
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <fctldefs.h>
#include <errno.h>
#include <strdefs.h>
#include <timedefs.h>

#include <utypes.h>
#include <btorder.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#if    defined(linux) || defined(__linux) || defined(__linux__)
extern BOOL check_linux_26();
#endif

#define	strbeg(s1, s2)	(strstr((s2), (s1)) == (s2))

extern	int	lverbose;

SCSI	*usal_open(char *scsidev, char *errs, int slen, int debug, int be_verbose);
int	usal_help(FILE *f);
static	int	usal_scandev(char *devp, char *errs, int slen, int *busp, 
									int *tgtp, int *lunp);
int	usal_close(SCSI * usalp);

void	usal_settimeout(SCSI * usalp, int timeout);

SCSI	*usal_smalloc(void);
void	usal_sfree(SCSI *usalp);

/*
 * Open a SCSI device.
 *
 * Possible syntax is:
 *
 * Preferred:
 *	dev=target,lun / dev=scsibus,target,lun
 *
 * Needed on some systems:
 *	dev=devicename:target,lun / dev=devicename:scsibus,target,lun
 *
 * On systems that don't support SCSI Bus scanning this syntax helps:
 *	dev=devicename:@ / dev=devicename:@,lun
 * or	dev=devicename (undocumented)
 *
 * NOTE: As the 'lun' is part of the SCSI command descriptor block, it
 *	 must always be known. If the OS cannot map it, it must be
 *	 specified on command line.
 */
SCSI *
usal_open(char *scsidev, char *errs, int slen, int debug, int be_verbose)
{
	char	devname[256];
	char	*devp = NULL;
	char	*sdev = NULL;
	int	x1;
	int	bus = 0;
	int	tgt = 0;
	int	lun = 0;
	int	n = 0;
	SCSI	*usalp;

	if (errs)
		errs[0] = '\0';
	usalp = usal_smalloc();
	if (usalp == NULL) {
		if (errs)
			snprintf(errs, slen, "No memory for SCSI structure");
		return ((SCSI *)0);
	}
	usalp->debug = debug;
	usalp->overbose = be_verbose;
	devname[0] = '\0';
	if (scsidev != NULL && scsidev[0] != '\0') {
		sdev = scsidev;
		if ((strncmp(scsidev, "HELP", 4) == 0) ||
		    (strncmp(scsidev, "help", 4) == 0)) {

			return ((SCSI *)0);
		}
		if (strncmp(scsidev, "REMOTE", 6) == 0) {
			/*
			 * REMOTE:user@host:scsidev or
			 * REMOTE(transp):user@host:scsidev
			 * e.g.: REMOTE(/usr/bin/ssh):user@host:scsidev
			 *
			 * We must send the complete device spec to the remote
			 * site to allow parsing on both sites.
			 */
			strncpy(devname, scsidev, sizeof (devname)-1);
			devname[sizeof (devname)-1] = '\0';
			if (sdev[6] == '(' || sdev[6] == ':')
				sdev = strchr(sdev, ':');
			else
				sdev = NULL;

			if (sdev == NULL) {
				/*
				 * This seems to be an illegal remote dev spec.
				 * Give it a chance with a standard parsing.
				 */
				sdev = scsidev;
				devname[0] = '\0';
			} else {
				/*
				 * Now try to go past user@host spec.
				 */
				if (sdev)
					sdev = strchr(&sdev[1], ':');
				if (sdev)
					sdev++;	/* Device name follows ... */
				else
					goto nulldevice;
			}
		}
		if ((devp = strchr(sdev, ':')) == NULL) {
			if (strchr(sdev, ',') == NULL) {
				/* Notation form: 'devname' (undocumented)  */
				/* Forward complete name to usal__open()	    */
				/* Fetch bus/tgt/lun values from OS	    */
				/* We may come here too with 'USCSI'	    */
				n = -1;
				lun  = -2;	/* Lun must be known	    */
				if (devname[0] == '\0') {
					strncpy(devname, scsidev,
							sizeof (devname)-1);
					devname[sizeof (devname)-1] = '\0';
				}
			} else {
				/* Basic notation form: 'bus,tgt,lun'	    */
				devp = sdev;
			}
		} else {
			/* Notation form: 'devname:bus,tgt,lun'/'devname:@' */
			/* We may come here too with 'USCSI:'		    */
			if (devname[0] == '\0') {
				/*
				 * Copy over the part before the ':'
				 */
				x1 = devp - scsidev;
				if (x1 >= (int)sizeof (devname))
					x1 = sizeof (devname)-1;
				strncpy(devname, scsidev, x1);
				devname[x1] = '\0';
			}
			devp++;
			/* Check for a notation in the form 'devname:@'	    */
			if (devp[0] == '@') {
				if (devp[1] == '\0') {
					lun = -2;
				} else if (devp[1] == ',') {
					if (*astoi(&devp[2], &lun) != '\0') {
						errno = EINVAL;
						if (errs)
							snprintf(errs, slen,
								"Invalid lun specifier '%s'",
										&devp[2]);
						return ((SCSI *)0);
					}
				}
				n = -1;
				/*
				 * Got device:@ or device:@,lun
				 * Make sure not to call usal_scandev()
				 */
				devp = NULL;
			} else if (devp[0] == '\0') {
				/*
				 * Got USCSI: or ATAPI:
				 * Make sure not to call usal_scandev()
				 */
				devp = NULL;
			} else if (strchr(sdev, ',') == NULL) {
				/* We may come here with 'ATAPI:/dev/hdc'   */
				strncpy(devname, scsidev,
						sizeof (devname)-1);
				devname[sizeof (devname)-1] = '\0';
				n = -1;
				lun  = -2;	/* Lun must be known	    */
				/*
				 * Make sure not to call usal_scandev()
				 */
				devp = NULL;
			}
		}
	}
nulldevice:

/*fprintf(stderr, "10 scsidev '%s' sdev '%s' devp '%s' b: %d t: %d l: %d\n", scsidev, sdev, devp, bus, tgt, lun);*/

	if (devp != NULL) {
		n = usal_scandev(devp, errs, slen, &bus, &tgt, &lun);
		if (n < 0) {
			errno = EINVAL;
			return ((SCSI *)0);
		}
	}
	if (n >= 1 && n <= 3) {	/* Got bus,target,lun or target,lun or tgt*/
		usal_settarget(usalp, bus, tgt, lun);
	} else if (n == -1) {	/* Got device:@, fetch bus/lun from OS	*/
		usal_settarget(usalp, -2, -2, lun);
	} else if (devp != NULL) {
		/*
		 * XXX May this happen after we allow tgt to repesent tgt,0 ?
		 */
		fprintf(stderr, "WARNING: device not valid, trying to use default target...\n");
		usal_settarget(usalp, 0, 6, 0);
	}
	if (be_verbose && scsidev != NULL) {
		fprintf(stderr, "scsidev: '%s'\n", scsidev);
		if (devname[0] != '\0')
			fprintf(stderr, "devname: '%s'\n", devname);
		fprintf(stderr, "scsibus: %d target: %d lun: %d\n",
					usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
	}
	if (debug > 0) {
		fprintf(stderr, "usal__open(%s) %d,%d,%d\n",
			devname,
			usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
	}
	if (usal__open(usalp, devname) <= 0) {
		if (errs && usalp->errstr)
			snprintf(errs, slen, "%s", usalp->errstr);
		usal_sfree(usalp);
		return ((SCSI *)0);
	}
	return (usalp);
}

int
usal_help(FILE *f)
{
	SCSI	*usalp;

	usalp = usal_smalloc();
	if (usalp != NULL) {
extern	usal_ops_t usal_std_ops;

		usalp->ops = &usal_std_ops;

		printf("Supported SCSI transports for this platform:\n");
		SCGO_HELP(usalp, f);
		usal_remote()->usalo_help(usalp, f);
		usal_sfree(usalp);
	}
	return (0);
}

/*
 * Convert target,lun or scsibus,target,lun syntax.
 * Check for bad syntax and invalid values.
 * This is definitely better than using scanf() as it checks for syntax errors.
 */
static int
usal_scandev(char *devp, char *errs, int slen, int *busp, int *tgtp, int *lunp)
{
	int	x1, x2, x3;
	int	n = 0;
	char	*p = devp;

	x1 = x2 = x3 = 0;
	*busp = *tgtp = *lunp = 0;

	if (*p != '\0') {
		p = astoi(p, &x1);
		if (*p == ',') {
			p++;
			n++;
		} else {
			if (errs)
				snprintf(errs, slen, "Invalid bus or target specifier in '%s'", devp);
			return (-1);
		}
	}
	if (*p != '\0') {
		p = astoi(p, &x2);
		if (*p == ',' || *p == '\0') {
			if (*p != '\0')
				p++;
			n++;
		} else {
			if (errs)
				snprintf(errs, slen, "Invalid target or lun specifier in '%s'", devp);
			return (-1);
		}
	}
	if (*p != '\0') {
		p = astoi(p, &x3);
		if (*p == '\0') {
			n++;
		} else {
			if (errs)
				snprintf(errs, slen, "Invalid lun specifier in '%s'", devp);
			return (-1);
		}
	}
	if (n == 3) {
		*busp = x1;
		*tgtp = x2;
		*lunp = x3;
	}
	if (n == 2) {
		*tgtp = x1;
		*lunp = x2;
	}
	if (n == 1) {
		*tgtp = x1;
	}

	if (x1 < 0 || x2 < 0 || x3 < 0) {
		if (errs)
			snprintf(errs, slen, "Invalid value for bus, target or lun (%d,%d,%d)",
				*busp, *tgtp, *lunp);
		return (-1);
	}
	return (n);
}

int
usal_close(SCSI *usalp)
{
	usal__close(usalp);
	usal_sfree(usalp);
	return (0);
}

char * usal_natname(SCSI *usalp, int busno, int tgt, int tlun) {
	return usalp->ops->usalo_natname(usalp, busno, tgt, tlun);
}

int usal_fileno(SCSI *usalp, int busno, int tgt, int tlun) {
	return usalp->ops->usalo_fileno(usalp, busno, tgt, tlun);
}

void
usal_settimeout(SCSI *usalp, int timeout)
{
#ifdef	nonono
	if (timeout >= 0)
		usalp->deftimeout = timeout;
#else
	usalp->deftimeout = timeout;
#endif
}

SCSI *
usal_smalloc()
{
	SCSI	*usalp;
extern	usal_ops_t usal_dummy_ops;

	usalp = (SCSI *)malloc(sizeof (*usalp));
	if (usalp == NULL)
		return ((SCSI *)0);

	fillbytes(usalp, sizeof (*usalp), 0);
	usalp->ops	= &usal_dummy_ops;
	usal_settarget(usalp, -1, -1, -1);
	usalp->fd	= -1;
	usalp->deftimeout = 20;
	usalp->running	= FALSE;

	usalp->cmdstart = (struct timeval *)malloc(sizeof (struct timeval));
	if (usalp->cmdstart == NULL)
		goto err;
	usalp->cmdstop = (struct timeval *)malloc(sizeof (struct timeval));
	if (usalp->cmdstop == NULL)
		goto err;
	usalp->scmd = (struct usal_cmd *)malloc(sizeof (struct usal_cmd));
	if (usalp->scmd == NULL)
		goto err;
	usalp->errstr = malloc(SCSI_ERRSTR_SIZE);
	if (usalp->errstr == NULL)
		goto err;
	usalp->errptr = usalp->errbeg = usalp->errstr;
	usalp->errstr[0] = '\0';
	usalp->errfile = (void *)stderr;
	usalp->inq = (struct scsi_inquiry *)malloc(sizeof (struct scsi_inquiry));
	if (usalp->inq == NULL)
		goto err;
	usalp->cap = (struct scsi_capacity *)malloc(sizeof (struct scsi_capacity));
	if (usalp->cap == NULL)
		goto err;

	return (usalp);
err:
	usal_sfree(usalp);
	return ((SCSI *)0);
}

void
usal_sfree(SCSI *usalp)
{
	if (usalp->cmdstart)
		free(usalp->cmdstart);
	if (usalp->cmdstop)
		free(usalp->cmdstop);
	if (usalp->scmd)
		free(usalp->scmd);
	if (usalp->inq)
		free(usalp->inq);
	if (usalp->cap)
		free(usalp->cap);
	if (usalp->local)
		free(usalp->local);
	usal_freebuf(usalp);
	if (usalp->errstr)
		free(usalp->errstr);
	free(usalp);
}
