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

/* @(#)scsi.c	1.20 05/05/01 Copyright 1997 J. Schilling */
/*
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

#ifdef	USE_SCG
#include <mconfig.h>

#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <schily.h>

#include "genisoimage.h"
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "wodim.h"
#include "../wodim/defaults.h"

/*
 * NOTICE:	You should not make BUF_SIZE more than
 *		the buffer size of the CD-Recorder.
 *
 * Do not set BUF_SIZE to be more than 126 KBytes
 * if you are running cdrecord on a sun4c machine.
 *
 * WARNING:	Philips CDD 521 dies if BUF_SIZE is to big.
 */
#define	BUF_SIZE	(62*1024)	/* Must be a multiple of 2048	   */

static	SCSI	*usalp;
static	long	bufsize;		/* The size of the transfer buffer */

int	readsecs(int startsecno, void *buffer, int sectorcount);
int	scsidev_open(char *path);
int	scsidev_close(void);

int
readsecs(int startsecno, void *buffer, int sectorcount)
{
	int	f;
	int	secsize;	/* The drive's SCSI sector size		*/
	long	amount;		/* The number of bytes to be transfered	*/
	long	secno;		/* The sector number to read from	*/
	long	secnum;		/* The number of sectors to read	*/
	char	*bp;
	long	amt;

	if (in_image == NULL) {
		/*
		 * We are using the standard CD-ROM sectorsize of 2048 bytes
		 * while the drive may be switched to 512 bytes per sector.
		 *
		 * XXX We assume that secsize is no more than SECTOR_SIZE
		 * XXX and that SECTOR_SIZE / secsize is not a fraction.
		 */
		secsize = usalp->cap->c_bsize;
		amount = sectorcount * SECTOR_SIZE;
		secno = startsecno * (SECTOR_SIZE / secsize);
		bp = buffer;

		while (amount > 0) {
			amt = amount;
			if (amount > bufsize)
				amt = bufsize;
			secnum = amt / secsize;

			if (read_scsi(usalp, bp, secno, secnum) < 0 ||
						usal_getresid(usalp) != 0) {
#ifdef	OLD
				return (-1);
#else
				comerr("Read error on old image\n");
#endif
			}

			amount	-= secnum * secsize;
			bp	+= secnum * secsize;
			secno	+= secnum;
		}
		return (SECTOR_SIZE * sectorcount);
	}

	f = fileno(in_image);

	if (lseek(f, (off_t)startsecno * SECTOR_SIZE, SEEK_SET) == (off_t)-1) {
#ifdef	USE_LIBSCHILY
		comerr("Seek error on old image\n");
#else
		fprintf(stderr, "Seek error on old image\n");
		exit(10);
#endif
	}
	if ((amt = read(f, buffer, (sectorcount * SECTOR_SIZE)))
			!= (sectorcount * SECTOR_SIZE)) {
#ifdef	USE_LIBSCHILY
		if (amt < 0)
			comerr("Read error on old image\n");
		comerrno(EX_BAD, "Short read on old image\n"); /* < secnt aber > 0 */
#else
		if (amt < 0)
			fprintf(stderr, "Read error on old image\n");
		else
			fprintf(stderr, "Short read on old image\n");
	
		exit(10);
#endif
	}
	return (sectorcount * SECTOR_SIZE);
}

int
scsidev_open(char *path)
{
	char	errstr[80];
	char	*buf;	/* ignored, bit OS/2 ASPI layer needs memory which */
			/* has been allocated by scsi_getbuf()		   */

	/*
	 * Call usal_remote() to force loading the remote SCSI transport library
	 * code that is located in librusal instead of the dummy remote routines
	 * that are located inside libusal.
	 */
	usal_remote();

	cdr_defaults(&path, NULL, NULL, NULL);
			/* path, debug, verboseopen */
	usalp = usal_open(path, errstr, sizeof (errstr), 0, 0);
	if (usalp == 0) {
		errmsg("%s%sCannot open SCSI driver.\n", errstr, errstr[0]?". ":"");
		return (-1);
	}

	bufsize = usal_bufsize(usalp, BUF_SIZE);
	if ((buf = usal_getbuf(usalp, bufsize)) == NULL) {
		errmsg("Cannot get SCSI I/O buffer.\n");
		usal_close(usalp);
		return (-1);
	}

	bufsize = (bufsize / SECTOR_SIZE) * SECTOR_SIZE;

	allow_atapi(usalp, TRUE);

	if (!wait_unit_ready(usalp, 60)) { /* Eat Unit att / Wait for drive */
		usalp->silent--;
		return (-1);
	}

	usalp->silent++;
	read_capacity(usalp);	/* Set Capacity/Sectorsize for I/O */
	usalp->silent--;

	return (1);
}

int
scsidev_close()
{
	if (in_image == NULL) {
		return (usal_close(usalp));
	} else {
		return (fclose(in_image));
	}
}

#endif	/* USE_SCG */
