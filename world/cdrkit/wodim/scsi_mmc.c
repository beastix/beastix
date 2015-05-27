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

/* @(#)scsi_mmc.c	1.13 05/05/16 Copyright 2002-2005 J. Schilling */
/*
 *	SCSI command functions for cdrecord
 *	covering MMC-3 level and above
 *
 *	Copyright (c) 2002-2005 J. Schilling
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
Includes code from http://libburnia.pykix.org/browser/libburn/trunk/libburn/mmc.c?format=txt 
*/

/*#define	DEBUG*/

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
#include <intcvt.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsidefs.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>

#include "scsimmc.h"
#include "wodim.h"

extern	int	xdebug;



int	get_configuration(SCSI *usalp, caddr_t bp, int cnt, int st_feature, 
								int rt);
static	int	get_conflen(SCSI *usalp, int st_feature, int rt);
int	get_curprofile(SCSI *usalp);
static	int	get_profiles(SCSI *usalp, caddr_t bp, int cnt);
int	print_profiles(SCSI *usalp);
int	get_proflist(SCSI *usalp, BOOL *wp, BOOL *cdp, BOOL *dvdp, BOOL *dvdplusp,
						 BOOL *ddcdp);
int	get_wproflist(SCSI *usalp, BOOL *cdp, BOOL *dvdp, BOOL *dvdplusp, 
						  BOOL *ddcdp);

/*
 * Get feature codes
 */
int
get_configuration(SCSI *usalp, caddr_t bp, int cnt, int st_feature, int rt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	fillbytes((caddr_t)scmd, sizeof (*scmd), '\0');
	scmd->addr = bp;
	scmd->size = cnt;
	scmd->flags = SCG_RECV_DATA|SCG_DISRE_ENA;
	scmd->cdb_len = SC_G1_CDBLEN;
	scmd->sense_len = CCS_SENSE_LEN;
	scmd->cdb.g1_cdb.cmd = 0x46;
	scmd->cdb.g1_cdb.lun = usal_lun(usalp);
	if (rt & 1)
		scmd->cdb.g1_cdb.reladr  = 1;
	if (rt & 2)
		scmd->cdb.g1_cdb.res  = 1;

	i_to_2_byte(scmd->cdb.g1_cdb.addr, st_feature);
	g1_cdblen(&scmd->cdb.g1_cdb, cnt);

	usalp->cmdname = "get_configuration";

	return (usal_cmd(usalp));
}

/*
 * Retrieve feature code list length
 */
static int
get_conflen(SCSI *usalp, int st_feature, int rt)
{
	Uchar	cbuf[8];
	int	flen;
	int	i;

	fillbytes(cbuf, sizeof (cbuf), '\0');
	usalp->silent++;
	i = get_configuration(usalp, (char *)cbuf, sizeof (cbuf), st_feature, rt);
	usalp->silent--;
	if (i < 0)
		return (-1);
	i = sizeof (cbuf) - usal_getresid(usalp);
	if (i < 4)
		return (-1);

	flen = a_to_u_4_byte(cbuf);
	if (flen < 4)
		return (-1);
	return (flen);
}

int
get_curprofile(SCSI *usalp)
{
	Uchar	cbuf[8];
	int	amt;
	int	flen;
	int	profile;
	int	i;

	fillbytes(cbuf, sizeof (cbuf), '\0');
	usalp->silent++;
	i = get_configuration(usalp, (char *)cbuf, sizeof (cbuf), 0, 0);
	usalp->silent--;
	if (i < 0)
		return (-1);

	amt = sizeof (cbuf) - usal_getresid(usalp);
	if (amt < 8)
		return (-1);
	flen = a_to_u_4_byte(cbuf);
	if (flen < 4)
		return (-1);

	profile = a_to_u_2_byte(&cbuf[6]);

	if (xdebug > 1)
		usal_prbytes("Features: ", cbuf, amt);

	if (xdebug > 0)
		printf("feature len: %d current profile 0x%04X (%s) len %d\n",
				flen, profile,
				mmc_obtain_profile_name(profile), amt);

	return (profile);
}

static int
get_profiles(SCSI *usalp, caddr_t bp, int cnt)
{
	int	amt;
	int	flen;
	int	i;

	flen = get_conflen(usalp, 0, 0);
	if (flen < 0)
		return (-1);
	if (cnt < flen)
		flen = cnt;

	fillbytes(bp, cnt, '\0');
	usalp->silent++;
	i = get_configuration(usalp, (char *)bp, flen, 0, 0);
	usalp->silent--;
	if (i < 0)
		return (-1);
	amt = flen - usal_getresid(usalp);

	flen = a_to_u_4_byte(bp);
	if ((flen+4) < amt)
		amt = flen+4;

	return (amt);
}

int
print_profiles(SCSI *usalp)
{
	Uchar	cbuf[1024];
	Uchar	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;

	flen = get_profiles(usalp, (caddr_t)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);

	p = cbuf;
	if (xdebug > 1)
		usal_prbytes("Features: ", cbuf, flen);

	curprofile = a_to_u_2_byte(&p[6]);
	if (xdebug > 0)
		printf("feature len: %d current profile 0x%04X (%s)\n",
				flen, curprofile,
				mmc_obtain_profile_name(curprofile));

	printf("Current: 0x%04X (%s)\n", curprofile,
			mmc_obtain_profile_name(curprofile));

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		if (xdebug > 0)
			printf("Profile: 0x%04X (%s)", profile,
					mmc_obtain_profile_name(profile));
		else
			printf("Profile: ");
		printf("0x%04X (%s) %s\n", profile,
				mmc_obtain_profile_name(profile), 
				p[2] & 1 ? "(current)":"");
		p += 4;
	}
	return (curprofile);
}

int
get_proflist(SCSI *usalp, BOOL *wp, BOOL *cdp, BOOL *dvdp, BOOL *dvdplusp, 
             BOOL *ddcdp)
{
	Uchar	cbuf[1024];
	Uchar	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;
	BOOL	wr	= FALSE;
	BOOL	cd	= FALSE;
	BOOL	dvd	= FALSE;
	BOOL	dvdplus	= FALSE;
	BOOL	ddcd	= FALSE;

	flen = get_profiles(usalp, (caddr_t)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);

	p = cbuf;
	if (xdebug > 1)
		usal_prbytes("Features: ", cbuf, flen);

	curprofile = a_to_u_2_byte(&p[6]);
	if (xdebug > 0)
		printf("feature len: %d current profile 0x%04X (%s)\n",
				flen, curprofile, 
				mmc_obtain_profile_name(curprofile));

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		p += 4;
		if (profile >= 0x0008 && profile < 0x0010)
			cd = TRUE;
		if (profile > 0x0008 && profile < 0x0010)
			wr = TRUE;

		if (profile >= 0x0010 && profile < 0x0018)
			dvd = TRUE;
		if (profile > 0x0010 && profile < 0x0018)
			wr = TRUE;

		if (profile >= 0x0018 && profile < 0x0020)
			dvdplus = TRUE;
		if (profile > 0x0018 && profile < 0x0020)
			wr = TRUE;

		if (profile >= 0x0020 && profile < 0x0028)
			ddcd = TRUE;
		if (profile > 0x0020 && profile < 0x0028)
			wr = TRUE;
	}
	if (wp)
		*wp	= wr;
	if (cdp)
		*cdp	= cd;
	if (dvdp)
		*dvdp	= dvd;
	if (dvdplusp)
		*dvdplusp = dvdplus;
	if (ddcdp)
		*ddcdp	= ddcd;

	return (curprofile);
}

int
get_wproflist(SCSI *usalp, BOOL *cdp, BOOL *dvdp, BOOL *dvdplusp, BOOL *ddcdp)
{
	Uchar	cbuf[1024];
	Uchar	*p;
	int	flen;
	int	curprofile;
	int	profile;
	int	i;
	int	n;
	BOOL	cd	= FALSE;
	BOOL	dvd	= FALSE;
	BOOL	dvdplus	= FALSE;
	BOOL	ddcd	= FALSE;

	flen = get_profiles(usalp, (caddr_t)cbuf, sizeof (cbuf));
	if (flen < 0)
		return (-1);
	p = cbuf;
	curprofile = a_to_u_2_byte(&p[6]);

	p += 8;		/* Skip feature header	*/
	n = p[3];	/* Additional length	*/
	n /= 4;
	p += 4;

	for (i = 0; i < n; i++) {
		profile = a_to_u_2_byte(p);
		p += 4;
		if (profile > 0x0008 && profile < 0x0010)
			cd = TRUE;
		if (profile > 0x0010 && profile < 0x0018)
			dvd = TRUE;
		if (profile > 0x0018 && profile < 0x0020)
			dvdplus = TRUE;
		if (profile > 0x0020 && profile < 0x0028)
			ddcd = TRUE;
	}
	if (cdp)
		*cdp	= cd;
	if (dvdp)
		*dvdp	= dvd;
	if (dvdplusp)
		*dvdplusp = dvdplus;
	if (ddcdp)
		*ddcdp	= ddcd;

	return (curprofile);
}


char *mmc_obtain_profile_name(int profile_number) {
  static char *texts[0x53] = {NULL};
  static char *reserved="Reserved/Unknown";
  int i, max_pno = 0x53;
  
  if (texts[0] == NULL) {
    for (i = 0; i<max_pno; i++)
      texts[i] = reserved;
    /* mmc5r04c.pdf , Table 88, human readable profile names */
    texts[0x01] = "Non-removable disk";
    texts[0x02] = "Removable disk";
    texts[0x03] = "MO erasable";
    texts[0x04] = "Optical write once";
    texts[0x05] = "AS-MO";
    texts[0x08] = "CD-ROM";
    texts[0x09] = "CD-R";
    texts[0x0a] = "CD-RW";
    texts[0x10] = "DVD-ROM";
    texts[0x11] = "DVD-R sequential recording";
    texts[0x12] = "DVD-RAM";
    texts[0x13] = "DVD-RW restricted overwrite";
    texts[0x14] = "DVD-RW sequential recording";
    texts[0x15] = "DVD-R/DL sequential recording";
    texts[0x16] = "DVD-R/DL layer jump recording";
    texts[0x1a] = "DVD+RW";
    texts[0x1b] = "DVD+R";
    texts[0x2a] = "DVD+RW/DL";
    texts[0x2b] = "DVD+R/DL";
    texts[0x40] = "BD-ROM";
    texts[0x41] = "BD-R sequential recording";
    texts[0x42] = "BD-R random recording";
    texts[0x43] = "BD-RE";
    texts[0x50] = "HD-DVD-ROM";
    texts[0x51] = "HD-DVD-R";
    texts[0x52] = "HD-DVD-RAM";
  }
  if (profile_number<0 || profile_number>=max_pno)
    return "";
  return texts[profile_number];
}

