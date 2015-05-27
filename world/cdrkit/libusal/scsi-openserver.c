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

/* @(#)scsi-openserver.c	1.31 04/01/15 Copyright 1998 J. Schilling, Santa Cruz Operation */
/*
 *	Interface for the SCO SCSI implementation.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1998 J. Schilling, Santa Cruz Operation
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

#undef	sense

#include <sys/scsicmd.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-openserver.c-1.31";	/* The version for this transport*/

#define	MAX_SCG		16		/* Max # of cdrom devices */
#define	MAX_TGT		16		/* Not really needed	  */
#define	MAX_LUN		8		/* Not really needed	  */

#define	MAX_DMA		(64*1024)

#define	MAXPATH		256		/* max length of devicepath  */
#define	MAXLINE		80		/* max length of input line  */
#define	MAXSCSI		99		/* max number of mscsi lines */
#define	MAXDRVN		10		/* max length of drivername  */

#define	DEV_DIR		"/tmp"
#define	DEV_NAME	"usal.s%1dt%1dl%1d"

/*
 * ---------------------------------------------------------------------
 * We will only deal with cdroms by default! Only if you set a specific
 * environment variable, we will scan "all" devices !
 * Set LIBSCG_SCAN_ALL to any value to enable access to all your SCSI
 * devices.
 *
 * The upcoming support for USB will be for USB 1.1, so as this is not
 * tested yet, we will currently ignore drives connect to the USB stack
 * (usbha controller) regardless of having set LIBSCG_SCAN_ALL or not!
 */

#define	DEV_ROOT	"/dev/dsk/0s0"

#define	DEV_SDSK	"/dev/rdsk/%ds0"
#define	DEV_SROM	"/dev/rcd%d"
#define	DEV_STP		"/dev/xStp%d"
#define	DEV_SFLP	"/dev/rdsk/fp%dh"

#define	SCAN_DEV	"%s%s%d%d%d%d"

#define	SCSI_CFG	"/etc/sconf -r"		/* no. of configured devices  */
#define	SCSI_DEV	"/etc/sconf -g %d"	/* read line 'n' of mscsi tbl */

#define	DRV_ATAPI	"wd"		/* SCO OpenServer IDE driver	*/
#define	DRV_USB		"usbha"		/* SCO OpenServer USB driver	*/
#define	DRV_NOHA	"noha"		/* IDE/ATAPI device configured,	*/
					/* but missing !		*/


#define	T_DISK		"Sdsk"		/* SCO OpenServer SCSI disk	*/
#define	T_CDROM		"Srom"		/* SCO OpenServer SCSI cdrom	*/
#define	T_TAPE		"Stp"		/* SCO OpenServer SCSI tape	*/
#define	T_FLOPPY	"Sflp"		/* SCO OpenServer SCSI floppy	*/


/*
 * ---------------------------------------------------------------------
 * Environment variables to control certain functionality
 */

#define	SCAN_ALL	"LIBSCG_SCAN_ALL"	/* enable access for all devices */
#define	SCSI_USER_CMD	"LIBSCG_SCSIUSERCMD"	/* use old SCSIUSERCMD ioctl() */
#define	DMA_OVERRIDE	"LIBSCG_MAX_DMA"	/* override MAX_DMA value */
#define	ENABLE_USB	"LIBSCG_ENABLE_USB"	/* enable access of USB devices */

static	int	scan_all	= 0;	/* don't scan all devices by default */
static	int	scsiusercmd	= 0;	/* use SCSIUSERCMD2 ioctl by default */
static	int	enable_usb	= 0;	/* don't scan USB devices by default */
static	long	max_dma		= MAX_DMA; /* use MAX_DMA DMA buffer by default */


/*
 * ---------------------------------------------------------------------
 * There are two scsi passthrough ioctl() on SCO OpenServer 5.0.[45],
 * while there is only one available on SCO OpenServer 5.0.[02].
 *
 * The SCSIUSERCMD ioctl is available on all OpenServer 5
 *
 * The SCSIUSERCMD2 ioctl which executes the usercmd and reads the sense
 * in one go, is only available from 5.0.4 onwards.
 *
 * By default we will use the SCSIUSERCMD2 ioctl(), in order to execute
 * the SCSIUSERCMD ioctl() instead set the environment variable
 * LIBSCG_SCSIUSERCMD to any value. Using the olderSCSIUSERCMD ioctl() will
 * if the SCSI commands returns a CHECK CONDITION status, run a seperate
 * REQUEST_SENSE command immediately. But we need to remember that in a
 * multi-tasking environment, there might be other code which has accessed
 * the device in between these two steps and therefore the sense code
 * is no longer valid !!!
 *
 * NOTE: There are problems with the usage of AHA 154X controllers
 * and SCSIUSERCMD2 such as nonsense (weird) output on cdrecord -scanbus
 *
 */


typedef struct usal2sdi {

	int	valid;
	int	open;
	int	atapi;
	int	fd;
	int	lmscsi;

} usal2sdi_t;

static	usal2sdi_t	sdidevs [MAX_SCG][MAX_TGT][MAX_LUN];

typedef struct amscsi {
	char	typ[MAXDRVN];
	char	drv[MAXDRVN];
	int	hba;
	int	bus;
	int	usal;
	int	tgt;
	int	lun;
	char	dev[MAXPATH];

} amscsi_t;

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

static	int	sort_mscsi(const void *l1, const void *l2);
static	int	openserver_init(SCSI *usalp);
static	void	cp_usal2sco(struct scsicmd2 *sco, struct usal_cmd *usal);
static	void	cp_sco2usal(struct scsicmd2 *sco, struct usal_cmd *usal);

/*
 * -------------------------------------------------------------------------
 * SCO OpenServer does not have a generic scsi device driver, which can
 * be used to access any configured scsi device. But we can use the "Sxxx"
 * scsi peripherial drivers passthrough ioctl() (SCSIUSERCMD / SCSIUSERCMD2)
 * to send scsi user comands to any target device controlled by the
 * corresponding target driver.
 *
 * This passthrough implementation for libusal currently allows to
 * handle the following devices classes:
 *
 *	1. DISK		handled by Sdsk
 *	2. CD-ROM	handled by Srom
 *	3. TAPES	handled by Stp
 *	4. FLOPPY	handled by Sflp
 *
 * NOTE: The libusal OpenServer passthrough routines have changed with
 *       cdrecord-1.8 to enable the -scanbus option. Therefore the
 *	 addressing scheme is now the same as used on many other platforms
 *	 like Solaris, Linux etc.
 *
 *   ===============================================================
 *   RUN 'cdrecord -scanbus' TO SEE THE DEVICE ADDRESSES YOU CAN USE
 *   ===============================================================
 *
 */

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_version(SCSI *usalp, int what)
{
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
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "SCSIUSERCMD/SCSIUSERCMD2", "Generic SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

/*
 * ---------------------------------------------------------------
 * This routine sorts the amscsi_t lines on the following columns
 * in ascending order:
 *
 *	1. drv  - driver name
 *	2. bus  - scsibus per controller
 *	3. tgt  - target id of device
 *	4. lun  - lun of the device
 *
 */


static int
sort_mscsi(const void *l1, const void *l2)
{
	amscsi_t	*t1 = (amscsi_t *) l1;
	amscsi_t	*t2 = (amscsi_t *) l2;

	if (strcmp(t1->drv, t2->drv) == 0) {
		if (t1->bus < t2->bus) {
			return (-1);

		} else if (t1->bus > t2->bus) {
			return (1);

		} else if (t1->tgt < t2->tgt) {
			return (-1);

		} else if (t1->tgt > t2->tgt) {
			return (1);

		} else if (t1->lun < t2->lun) {
			return (-1);

		} else if (t1->lun > t2->lun) {
			return (1);
		} else {
			return (0);
		}
	} else {
		return (strcmp(t1->drv, t2->drv));
	}
}

/*
 * ---------------------------------------------------------------
 * This routine is introduced to find all scsi devices which are
 * currently configured into the kernel. This is done by reading
 * the dynamic kernel mscsi tables and parse the resulting lines.
 * As the output of 'sconf' is not directly usable the information
 * found is to be sorted and re-arranged to be used with the libusal
 * routines.
 *
 * NOTE: One problem is currently still not solved ! If you don't
 *       have a media in your CD-ROM/CD-Writer we are not able to
 *       do an open() and therefore will set the drive to be not
 *       available (valid=0).
 *
 *       This will for example cause cdrecord to not list the drive
 *	 in the -scanbus output.
 *
 */

static int
openserver_init(SCSI *usalp)
{
	FILE		*cmd;
	int		nusal  = -1, lhba  = -1, lbus = -1;
	int		nSrom = -1, nSdsk = -1, nStp = -1, nSflp = -1;
	int		atapi, fd, nopen = 0;
	int		pos = 0, len = 0, nlm = 0;
	int		s = 0, t = 0, l = 0;
	int		ide_rootdisk = 0;
	long		dma_override = 0;
	int		mscsi;
	char		sconf[MAXLINE];
	char		lines[MAXLINE];
	char		drvid[MAXDRVN];
	amscsi_t	cmtbl[MAXSCSI];
	char		dname[MAXPATH];
	char		**evsave;
extern	char		**environ;


	for (s = 0; s < MAX_SCG; s++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				sdidevs[s][t][l].valid	=  0;
				sdidevs[s][t][l].open	= -1;
				sdidevs[s][t][l].atapi	= -1;
				sdidevs[s][t][l].fd	= -1;
				sdidevs[s][t][l].lmscsi	= -1;
			}
		}
	}

	/* Check whether we want to use the older SCSIUSERCMD ioctl() */

	if (getenv(SCSI_USER_CMD) != NULL) {
		scsiusercmd = 1;
	}

	/*
	 * Check whether we want to scan all devices
	 */
	if (getenv(SCAN_ALL) != NULL) {
		scan_all = 1;
	}

	/*
	 * Check whether we want to use USB devices
	 */
	if (getenv(ENABLE_USB) != NULL) {
		enable_usb = 1;
	}

	/*
	 * Check whether we want to override the MAX_DMA value
	 */
	if (getenv(DMA_OVERRIDE) != NULL) {
		dma_override = atol(getenv(DMA_OVERRIDE));
		if ((dma_override >= 1) && (dma_override <= (256)))
			max_dma = dma_override * 1024;
	}


	/* read sconf -r and get number of kernel mscsi lines ! */

	evsave = environ;
	environ = 0;
	if ((cmd = popen(SCSI_CFG, "r")) == NULL) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Error popen() for \"%s\"",
				SCSI_CFG);
		environ = evsave;
		return (-1);
	}
	environ = evsave;

	if (fgets(lines, MAXLINE, cmd) == NULL) {
		errno = EIO;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Error reading popen() for \"%s\"",
				SCSI_CFG);
		return (-1);
	} else
		nlm = atoi(lines);

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
		fprintf((FILE *)usalp->errfile, "mscsi lines = %d\n", nlm);
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
	}

	if (pclose(cmd) == -1) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Error pclose() for \"%s\"",
				SCSI_CFG);
		return (-1);
	}

	for (l = 0; l < nlm; l++) {

		/* initialize cmtbl entry */

		cmtbl[l].hba = -1;
		cmtbl[l].bus = -1;
		cmtbl[l].tgt = -1;
		cmtbl[l].lun = -1;
		cmtbl[l].usal = -1;

		memset(cmtbl[l].typ, '\0', MAXDRVN);
		memset(cmtbl[l].drv, '\0', MAXDRVN);
		memset(cmtbl[l].dev, '\0', MAXDRVN);

		/* read sconf -g 'n' and get line of kernel mscsi table!  */
		/* the order the lines will be received in will determine */
		/* the device name we can use to open the device	  */

		snprintf(sconf, sizeof (sconf),
			SCSI_DEV, l + 1); /* enumeration starts with 1 */

		evsave = environ;
		environ = 0;
		if ((cmd = popen(sconf, "r")) == NULL) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Error popen() for \"%s\"",
					sconf);
			environ = evsave;
			return (-1);
		}
		environ = evsave;

		if (fgets(lines, MAXLINE, cmd) == NULL)
			break;

		if (pclose(cmd) == -1) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
					"Error pclose() for \"%s\"",
					sconf);
			return (-1);
		}

		sscanf(lines, SCAN_DEV, cmtbl[l].typ,
					cmtbl[l].drv,
					&cmtbl[l].hba,
					&cmtbl[l].bus,
					&cmtbl[l].tgt,
					&cmtbl[l].lun);

		if (strstr(cmtbl[l].typ, T_DISK) != NULL) {
			snprintf(cmtbl[l].dev,  sizeof (cmtbl[l].dev),
					DEV_SDSK, ++nSdsk);
		}

		if (strstr(cmtbl[l].typ, T_CDROM) != NULL) {
			snprintf(cmtbl[l].dev,  sizeof (cmtbl[l].dev),
					DEV_SROM, ++nSrom);
		}

		if (strstr(cmtbl[l].typ, T_TAPE) != NULL) {
			snprintf(cmtbl[l].dev, sizeof (cmtbl[l].dev),
					DEV_STP, ++nStp);
		}

		if (strstr(cmtbl[l].typ, T_FLOPPY) != NULL) {
			snprintf(cmtbl[l].dev, sizeof (cmtbl[l].dev),
					DEV_SFLP, ++nSflp);
		}

		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"%-4s = %5s(%d,%d,%d,%d) -> %s\n",
				cmtbl[l].typ,
				cmtbl[l].drv,
				cmtbl[l].hba,
				cmtbl[l].bus,
				cmtbl[l].tgt,
				cmtbl[l].lun,
				cmtbl[l].dev);
		}

	}

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
		fprintf((FILE *)usalp->errfile, "%2d DISK  \n", nSdsk + 1);
		fprintf((FILE *)usalp->errfile, "%2d CD-ROM\n", nSrom + 1);
		fprintf((FILE *)usalp->errfile, "%2d TAPE  \n", nStp  + 1);
		fprintf((FILE *)usalp->errfile, "%2d FLOPPY\n", nSflp + 1);
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
	}

	/* ok, now let's sort this array of scsi devices	*/

	qsort((void *) cmtbl, nlm, sizeof (amscsi_t), sort_mscsi);

	if (usalp->debug > 0) {
		for (l = 0; l < nlm; l++)
		fprintf((FILE *)usalp->errfile,
			"%-4s = %5s(%d,%d,%d,%d) -> %s\n",
			cmtbl[l].typ,
			cmtbl[l].drv,
			cmtbl[l].hba,
			cmtbl[l].bus,
			cmtbl[l].tgt,
			cmtbl[l].lun,
			cmtbl[l].dev);
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
	}

	/* find root disk controller to make it usal 0		*/

	/*
	 * if we have disk(s) found in the mscsi table, we still
	 * don't know if the rootdisk is among these, there can
	 * be a IDE rootdisk as well, but it's not listed in
	 * the mscsi table.
	 */

	t = 0;
	if (nSdsk > 0) {
		for (l = 0; l < nlm; l++)
			if (strcmp(cmtbl[l].dev, DEV_ROOT) == 0)
				t = l;
	} else {

		/*
		 * we haven't found a disk in mscsi, so we definitely
		 * have an IDE disk on a wd adapter as IDE disks are
		 * not listed as SCSI disks in the kernel mscsi table
		 */
		ide_rootdisk = 1;

	}

	if (!(ide_rootdisk) && (usalp->debug > 0)) {
		fprintf((FILE *)usalp->errfile,
			"root = %5s(%d,%d,%d,%d) -> %s\n",
			cmtbl[t].drv,
			cmtbl[t].hba,
			cmtbl[t].bus,
			cmtbl[t].tgt,
			cmtbl[t].lun,
			cmtbl[t].dev);
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
	}

	/* calculate usal from drv, hba and bus 			*/

	strcpy(drvid, "");

	for (l = 0, s = t; l < nlm; l++, s = (t + l) % nlm) {

		if (strcmp(drvid, cmtbl[s].drv) != 0) {
			strcpy(drvid, cmtbl[s].drv);
			lhba = cmtbl[s].hba;
			lbus = cmtbl[s].bus;
			cmtbl[s].usal = ++nusal;

		} else if (cmtbl[s].hba != lhba) {
			lhba = cmtbl[s].hba;
			lbus = cmtbl[s].bus;
			cmtbl[s].usal = ++nusal;

		} else if (cmtbl[s].bus != lbus) {
			lbus = cmtbl[s].bus;
			cmtbl[s].usal = ++nusal;
		} else {
			cmtbl[s].usal = nusal;
		}
		sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].open   = 0;

		/* check whether we want to open all devices or it's a CDROM */

		if ((scan_all) || (strcmp(cmtbl[s].typ, T_CDROM) == 0))
			sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].valid = 1;

		/* check whether we have an IDE/ATAPI device */

		if (strcmp(cmtbl[s].drv, DRV_ATAPI) == 0)
			sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].atapi = 1;

		/* don't open a USB device if enable_usb is not set */

		if (strcmp(cmtbl[s].drv, DRV_USB) == 0)
			sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].valid = enable_usb;

		/* don't open a IDE/ATAPI device which is missing but configured */

		if (strcmp(cmtbl[s].drv, DRV_NOHA) == 0)
			sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].valid = 0;


		sdidevs[cmtbl[s].usal][cmtbl[s].tgt][cmtbl[s].lun].lmscsi = s;

	}


	/* open all yet valid device nodes */

	for (s = 0; s < MAX_SCG; s++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {

				if (sdidevs[s][t][l].valid == 0)
					continue;

				/* Open pass-through device node */

				mscsi = sdidevs[s][t][l].lmscsi;

				strcpy(dname, cmtbl[mscsi].dev);

	/*
	 * ------------------------------------------------------------------
	 * NOTE: If we can't open the device, we will set the device invalid!
	 * ------------------------------------------------------------------
	 */
				errno = 0;
				if ((fd = open(dname, (O_RDONLY | O_NONBLOCK))) < 0) {
					sdidevs[s][t][l].valid = 0;
					if (usalp->debug > 0) {
						fprintf((FILE *)usalp->errfile,
							"%5s(%d,%d,%d,%d) -> %s open() failed: errno = %d (%s)\n",
							cmtbl[mscsi].drv,
							cmtbl[mscsi].hba,
							cmtbl[mscsi].bus,
							cmtbl[mscsi].tgt,
							cmtbl[mscsi].lun,
							cmtbl[mscsi].dev,
							errno,
							strerror(errno));
					}
					continue;
				}

				if (usalp->debug > 0) {
					fprintf((FILE *)usalp->errfile,
						"%d,%d,%d => %5s(%d,%d,%d,%d) -> %d : %s \n",
						s, t, l,
						cmtbl[mscsi].drv,
						cmtbl[mscsi].hba,
						cmtbl[mscsi].bus,
						cmtbl[mscsi].tgt,
						cmtbl[mscsi].lun,
						cmtbl[mscsi].usal,
						cmtbl[mscsi].dev);
				}

				sdidevs[s][t][l].fd   = fd;
				sdidevs[s][t][l].open = 1;
				nopen++;
				usallocal(usalp)->usalfiles[s][t][l] = (short) fd;
			}
		}
	}

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
		fprintf((FILE *)usalp->errfile, "nopen = %d devices   \n", nopen);
		fprintf((FILE *)usalp->errfile, "-------------------- \n");
	}

	return (nopen);
}


static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
	int	tlun	= usal_lun(usalp);
	int	f, b, t, l;
	int	nopen = 0;
	char	devname[64];

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

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}

	if (*device != '\0') {		/* we don't allow old dev usage */
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			"Open by 'devname' no longer supported on this OS");
		return (-1);
	}

	return (openserver_init(usalp));

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
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {

				f = usallocal(usalp)->usalfiles[b][t][l];
				if (f >= 0)
					close(f);

				sdidevs[b][t][l].fd    = -1;
				sdidevs[b][t][l].open  =  0;
				sdidevs[b][t][l].valid =  0;

				usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (max_dma);
}


static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = valloc((size_t)(amt));

	return (usalp->bufbase);
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
	    tgt   < 0 || tgt   >= MAX_TGT ||
	    tlun  < 0 || tlun  >= MAX_LUN)
		return (-1);

	if (usalp->local == NULL)
		return (-1);

	return ((int)usallocal(usalp)->usalfiles[busno][tgt][tlun]);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);

	/*
	 * We don't know the initiator ID yet, but we can if we parse the
	 * output of the command 'cat /dev/string/cfg | grep "%adapter"'
	 *
	 * Sample line:
	 *
	 *	%adapter 0xE800-0xE8FF 11 - type=alad ha=0 bus=0 id=7 fts=sto
	 *
	 * This tells us that the alad controller 0 has an id of 7 !
	 * The parsing should be done in openserver_init().
	 *
	 */
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (sdidevs[usal_scsibus(usalp)][usal_target(usalp)][usal_lun(usalp)].atapi);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	errno = EINVAL;
	return (-1);		/* no scsi reset available */
}

static void
cp_usal2sco(struct scsicmd2 *sco, struct usal_cmd *usal)
{
	sco->cmd.data_ptr = (char *) usal->addr;
	sco->cmd.data_len = usal->size;
	sco->cmd.cdb_len  = usal->cdb_len;

	sco->sense_len    = usal->sense_len;
	sco->sense_ptr    = usal->u_sense.cmd_sense;

	if (!(usal->flags & SCG_RECV_DATA) && (usal->size > 0))
		sco->cmd.is_write = 1;

	if (usal->cdb_len == SC_G0_CDBLEN)
		memcpy(sco->cmd.cdb, &usal->cdb.g0_cdb, usal->cdb_len);

	if (usal->cdb_len == SC_G1_CDBLEN)
		memcpy(sco->cmd.cdb, &usal->cdb.g1_cdb, usal->cdb_len);

	if (usal->cdb_len == SC_G5_CDBLEN)
		memcpy(sco->cmd.cdb, &usal->cdb.g5_cdb, usal->cdb_len);
}


static void
cp_sco2usal(struct scsicmd2 *sco, struct usal_cmd *usal)
{
	usal->size	= sco->cmd.data_len;

	memset(&usal->scb, 0, sizeof (usal->scb));

	if (sco->sense_len > SCG_MAX_SENSE)
		usal->sense_count = SCG_MAX_SENSE;
	else
		usal->sense_count = sco->sense_len;

	usal->resid = 0;

	usal->u_scb.cmd_scb[0] = sco->cmd.target_sts;

}


static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	struct scsicmd2	scsi_cmd;
	int		i;
	Uchar		sense_buf[SCG_MAX_SENSE];

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	memset(&scsi_cmd, 0, sizeof (scsi_cmd));
	memset(sense_buf, 0, sizeof (sense_buf));
	scsi_cmd.sense_ptr = sense_buf;
	scsi_cmd.sense_len = sizeof (sense_buf);
	cp_usal2sco(&scsi_cmd, sp);

	errno = 0;
	sp->ux_errno = 0;
	sp->error = SCG_NO_ERROR;
	for (;;) {
		int		ioctlStatus;
		struct scsicmd	s_cmd;

		if (scsiusercmd) {	/* Use SCSIUSERCMD ioctl() */
			if (usalp->debug > 1) {
				fprintf((FILE *)usalp->errfile, "calling SCSIUSERCMD ioctl()\n");
			}

			if ((ioctlStatus = ioctl(usalp->fd, SCSIUSERCMD, &(scsi_cmd.cmd))) < 0) {
				if (usalp->debug > 1) {
					fprintf((FILE *)usalp->errfile, "returning from SCSIUSERCMD ioctl()\n");
				}
				if (errno == EINTR)
					continue;

				cp_sco2usal(&scsi_cmd, sp);
				sp->ux_errno = errno;
				if (errno == 0)
					sp->ux_errno = EIO;
				sp->error    = SCG_RETRYABLE;

				return (0);
			}

			if (usalp->debug > 1) {
				fprintf((FILE *)usalp->errfile, "returning from SCSIUSERCMD ioctl()\n");
			}
			cp_sco2usal(&scsi_cmd, sp);
			sp->ux_errno = errno;

			if (scsi_cmd.cmd.target_sts & 0x02) { /* Check Condition & get Sense */

				if (sp->sense_len > SCG_MAX_SENSE)
					sp->sense_len = SCG_MAX_SENSE;

				memset((caddr_t)&s_cmd, 0, sizeof (s_cmd));

				s_cmd.data_ptr  = (caddr_t) sp->u_sense.cmd_sense;
				s_cmd.data_len  = sp->sense_len;
				s_cmd.is_write  = 0;
				s_cmd.cdb[0]    = SC_REQUEST_SENSE;

				while (((ioctlStatus = ioctl(usalp->fd, SCSIUSERCMD, &s_cmd)) < 0) &&
					(errno == EINTR))
						;

				sp->sense_count = sp->sense_len;
				sp->ux_errno    = errno;

				if (errno == 0)
					sp->ux_errno = EIO;
				sp->error = SCG_NO_ERROR;
			}

			if (usalp->debug > 0) {
				if (errno != 0)
					fprintf((FILE *)usalp->errfile, "ux_errno: %d (%s) \n", sp->ux_errno, strerror(sp->ux_errno));
				if (sp->u_scb.cmd_scb[0] != 0)
					fprintf((FILE *)usalp->errfile, "tgt_stat: %d \n", sp->u_scb.cmd_scb[0]);
			}
			break;

		} else {		/* Use SCSIUSERCMD2 ioctl() */
			if (usalp->debug > 1) {
				fprintf((FILE *)usalp->errfile, "calling SCSIUSERCMD2 ioctl()\n");
			}

			if ((ioctlStatus = ioctl(usalp->fd, SCSIUSERCMD2, &scsi_cmd)) < 0) {
				if (usalp->debug > 1) {
					fprintf((FILE *)usalp->errfile, "returning from SCSIUSERCMD2 ioctl()\n");
				}
				if (errno == EINTR)
					continue;

				cp_sco2usal(&scsi_cmd, sp);
				sp->ux_errno = errno;
				if (errno == 0)
					sp->ux_errno = EIO;
				sp->error    = SCG_RETRYABLE;

				return (0);
			}
			if (usalp->debug > 1) {
				fprintf((FILE *)usalp->errfile, "returning from SCSIUSERCMD2 ioctl()\n");
			}

			cp_sco2usal(&scsi_cmd, sp);
			sp->ux_errno = errno;

			if (scsi_cmd.cmd.target_sts & 0x02) { /* Check Condition */
				if (errno == 0)
					sp->ux_errno = EIO;
				sp->error    = SCG_NO_ERROR;
			}

			if (usalp->debug > 0) {
				if (errno != 0)
					fprintf((FILE *)usalp->errfile, "ux_errno: %d (%s) \n", sp->ux_errno, strerror(sp->ux_errno));
				if (sp->u_scb.cmd_scb[0] != 0)
					fprintf((FILE *)usalp->errfile, "tgt_stat: %d \n", sp->u_scb.cmd_scb[0]);
			}
			break;

		}
	}

	return (0);
}

#define	sense	u_sense.Sense
