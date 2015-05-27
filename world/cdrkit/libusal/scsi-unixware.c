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

/* @(#)scsi-unixware.c	1.36 04/01/15 Copyright 1998 J. Schilling, Santa Cruz Operation */
/*
 *	Interface for the SCO UnixWare SCSI implementation.
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
#undef	SC_PARITY
#undef	scb

#include <sys/sysmacros.h>	/* XXX Falsch, richtig -> sys/mkdev.h */
#include <sys/scsi.h>
#include <sys/sdi_edt.h>
#include <sys/sdi.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-unixware.c-1.36";	/* The version for this transport*/

/* Max. number of usal scsibusses.  The real limit would be		*/
/* MAX_HBA * MAX_BUS (which would be 32 * 8 on UnixWare 2.1/7.x),	*/
/* but given that we will hardly see such a beast, lets take 32		*/

#define	MAX_SCG		32

	/* maximum defines for UnixWare 2.x/7.x from <sys/sdi_edt.h> */

#define	MAX_TGT		MAX_EXTCS	/* Max # of target id's		*/
#define	MAX_LUN		MAX_EXLUS	/* Max # of lun's		*/

#define	MAX_DMA		(32*1024)
#ifdef	__WHAT_TODO__
#define	MAX_DMA		(16*1024)	/* On UnixWare 2.1.x w/ AHA2940 HBA */
					/* the max DMA size is 16KB.	    */
#endif

#define	MAXLINE		80
#define	MAXPATH		256

#define	DEV_DIR		"/tmp"
#define	DEV_NAME	"usal.s%1dt%1dl%1d"

#define	SCAN_HBA	"%d:%d,%d,%d:%7s : %n"
#define	SCAN_DEV	"%d,%d,%d:%7s : %n"

#define	PRIM_HBA	"/dev/hba/hba1"
#define	SCSI_CFG	"LC_ALL=C /etc/scsi/pdiconfig -l"

#define	SCAN_ALL	"LIBSCG_SCAN_ALL"

#define	SDI_VALID	0x01	/* Entry may be used (non disk)	   */
#define	SDI_ATAPI	0x02	/* Connected via IDE HBA	   */
#define	SDI_INITIATOR	0x04	/* This is the initiator target ID */

typedef struct usal2sdi {
	short	open;
	short	flags;
	short	fd;
	char	hba;
	char	bus;
	char	tgt;
	char	lun;

	dev_t	node;
	dev_t	major;
	dev_t	minor;
/*#define	SCG_DEBUG*/
#ifdef	SCG_DEBUG
	char	type[20];
	char	vend[40];
	char	devn[32];
#endif
} usal2sdi_t;

static	usal2sdi_t	sdidevs [MAX_SCG][MAX_TGT][MAX_LUN];
static	BOOL		sdiinit = FALSE;

struct usal_local {
	short	usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

static	int	unixware_init(SCSI *usalp);
static	int	do_usal_cmd(SCSI *usalp, struct usal_cmd *sp);
static	int	do_usal_sense(SCSI *usalp, struct usal_cmd *sp);
static	FILE	*xpopen(char *cmd, char *type);
static	int	xpclose(FILE *f);

/*
 * -------------------------------------------------------------------------
 * SCO UnixWare 2.1.x / UnixWare 7 provides a scsi pass-through mechanism,
 * which can be used to access any configured scsi device.
 *
 * NOTE: The libusal UnixWare passthrough routines have changed with
 *       cdrecord-1.8 to enable the -scanbus, -load, -eject option
 *	 regardless of the status of media and the addressing
 *       scheme is now the same as used on many other platforms like
 *       Solaris, Linux etc.
 *
 *      ===============================================================
 *	RUN 'cdrecord -scanbus' TO SEE THE DEVICE ADDRESSES YOU CAN USE
 *	===============================================================
 */

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 *
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
	__usal_help(f, "SDI_SEND", "Generic SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

/*
 * ---------------------------------------------------------------
 * This routine is introduced to create all device nodes necessary
 * to access any detected scsi device. It parses the output of
 * /etc/scsi/pdiconfig -l and creates passthru device node for each
 * found scsi device apart from the listed hba's.
 *
 */

static int
unixware_init(SCSI *usalp)
{
	FILE		*cmd;
	int		hba = 0, bus = 0, usal = 0, tgt = 0, lun = 0;
	int		nusal = -1, lhba = -1, lbus = 0;
	int		atapi, fd, nopen = 0, pos = 0, len = 0;
	int		s, t, l;
	int		scan_disks;
	char		lines[MAXLINE];
	char		class[MAXLINE];
	char		ident[MAXLINE];
	char		devnm[MAXPATH];
	char		dname[MAXPATH];
	struct stat 	stbuf;
	dev_t		ptdev, major, minor, node;
	char		**evsave;
extern	char		**environ;

	/* Check for validity of primary hostbus adapter node */

	if (stat(PRIM_HBA, &stbuf) < 0) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Can not stat() primary hba (%s)",
				PRIM_HBA);
		return (-1);
	}

	if (!S_ISCHR(stbuf.st_mode)) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Primary hba (%s) not a character device",
				PRIM_HBA);
		return (-1);
	}

	major = getmajor(stbuf.st_rdev);

	/*
	 * Check whether we want to scan all devices
	 */
	if (getenv(SCAN_ALL) != NULL) {
		scan_disks = 1;
	} else {
		scan_disks = 0;
	}

	/* read pdiconfig output and get all attached scsi devices ! */

	evsave = environ;
	environ = 0;
	if ((cmd = xpopen(SCSI_CFG, "r")) == NULL) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Error popen() for \"%s\"",
				SCSI_CFG);
		environ = evsave;
		return (-1);
	}
	environ = evsave;


	for (;;) {
		if (fgets(lines, MAXLINE, cmd) == NULL)
			break;

		memset(class, '\0', sizeof (class));
		memset(ident, '\0', sizeof (ident));

		if (lines[0] == ' ') {
			sscanf(lines, SCAN_DEV, &bus, &tgt, &lun, class, &pos);
			hba = lhba;
		} else {
			sscanf(lines, SCAN_HBA, &hba, &bus, &tgt, &lun, class, &pos);
			nusal++;
			lhba = hba;
			atapi = 0;
		}

		/* We can't sscanf() the ident string of the device	*/
		/* as it may contain characters sscanf() will		*/
		/* recognize as a delimiter. So do a strcpy() instead !	*/

		len = strlen(lines) - pos - 1; /* don't copy the '\n' */

		strncpy(ident, &lines[pos], len);

		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"SDI -> %d:%d,%d,%d: %-7s : %s\n",
				hba, bus, tgt, lun, class, ident);
		}
		if (bus != lbus) {
			nusal++;
			lbus = bus;
		}

		usal = nusal;

		/* check whether we have a HBA or a SCSI device, don't 	*/
		/* let HBA's be valid device for cdrecord, but mark	*/
		/* them as a controller (initiator = 1).		*/

		/* Don't detect disks, opening a mounted disk can hang	*/
		/* the disk subsystem !!! So unless we set an		*/
		/* environment variable LIBSCG_SCAN_ALL, we will ignore	*/
		/* disks						*/

		if (strstr(class, "HBA") == NULL) {
			if (strstr(class, "DISK") != NULL) {
				if (scan_disks)
					sdidevs[usal][tgt][lun].flags |= SDI_VALID;
				else
					sdidevs[usal][tgt][lun].flags &= ~SDI_VALID;
			} else {
				sdidevs[usal][tgt][lun].flags |= SDI_VALID;
			}
		} else {
			sdidevs[usal][tgt][lun].flags |= SDI_INITIATOR;
		}


		/* There is no real flag that shows a HBA as an ATAPI	*/
		/* controller, so as we know the driver is called 'ide'	*/
		/* we can check the ident string for the occurence of it*/

		if (strstr(ident, "(ide,") != NULL) {
			atapi = 1;
		}

		/*
		 * Fill the sdidevs array with all we know now.
		 * Do not overwrite fields that may contain old state like
		 * sdidevs[usal][tgt][lun].open
		 */

		if (atapi)
			sdidevs[usal][tgt][lun].flags |= SDI_ATAPI;
		else
			sdidevs[usal][tgt][lun].flags &= ~SDI_ATAPI;

		sdidevs[usal][tgt][lun].hba = hba;
		sdidevs[usal][tgt][lun].bus = bus;
		sdidevs[usal][tgt][lun].tgt = tgt;
		sdidevs[usal][tgt][lun].lun = lun;

#ifdef	SCG_DEBUG
		strcpy(sdidevs[usal][tgt][lun].type, class);
		strcpy(sdidevs[usal][tgt][lun].vend, ident);

		snprintf(sdidevs[usal][tgt][lun].devn,
				sizeof (sdidevs[usal][tgt][lun].devn),
				DEV_NAME, usal, tgt, lun);
#endif
		snprintf(devnm, sizeof (devnm),
				DEV_NAME, usal, tgt, lun);

		minor = SDI_MINOR(hba, tgt, lun, bus);
		node  = makedevice(major, minor);

		sdidevs[usal][tgt][lun].major = major;
		sdidevs[usal][tgt][lun].minor = minor;
		sdidevs[usal][tgt][lun].node  = node;

		if (usalp->debug > 0) {

			fprintf((FILE *)usalp->errfile,
			"h = %d; b = %d, s = %d, t = %d, l = %d, a = %d, ma = %d, mi = %2d, dev = '%s', id = '%s'\n",
			hba, bus, usal, tgt, lun,
			(sdidevs[usal][tgt][lun].flags & SDI_ATAPI) != 0,
			sdidevs[usal][tgt][lun].major,
			sdidevs[usal][tgt][lun].minor,
			devnm,
			ident);
		}


	}

	if (xpclose(cmd) == -1) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Error pclose() for \"%s\"",
				SCSI_CFG);
		return (-1);
	}


	/* create all temporary device nodes */

	for (s = 0; s < MAX_SCG; s++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {

				if ((sdidevs[s][t][l].flags & SDI_VALID) == 0) {
					if (sdidevs[s][t][l].fd >= 0) {
						close(sdidevs[s][t][l].fd);
					}
					sdidevs[s][t][l].fd = -1;
					sdidevs[s][t][l].open = 0;
					continue;
				}

				/* Make pass-through interface device node */

				snprintf(devnm,
					sizeof (devnm),
					DEV_NAME, s, t, l);

				snprintf(dname, sizeof (dname),
					"%s/%s", DEV_DIR, devnm);

				ptdev = sdidevs[s][t][l].node;

				if (mknod(dname, S_IFCHR | 0700, ptdev) < 0) {
					if (errno == EEXIST) {
						unlink(dname);

						if (mknod(dname, S_IFCHR | 0700, ptdev) < 0) {
							if (usalp->errstr)
								snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
									"mknod() error for \"%s\"", dname);
							return (-1);
						}
					} else {
						if (usalp->errstr)
							snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
								"mknod() error for \"%s\"", dname);
						return (-1);
					}
				}

				/* Open pass-through device node */

				if ((fd = open(dname, O_RDONLY)) < 0) {
					if (errno == EBUSY && sdidevs[s][t][l].open > 0) {
						/*
						 * Device has already been opened, just
						 * return the saved file desc.
						 */
						fd = sdidevs[s][t][l].fd;
					} else {
						if (usalp->errstr)
							snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
								"can not open pass-through %s", dname);
						return (-1);
					}
				}

				/*
				 * If for whatever reason we may open a pass through
				 * device more than once, this will waste fs's as we
				 * do not check for sdidevs[s][t][l].fd == -1.
				 */
				sdidevs[s][t][l].fd   = fd;
				sdidevs[s][t][l].open++;
				nopen++;
				usallocal(usalp)->usalfiles[s][t][l] = (short) fd;

				if (usalp->debug > 0) {

					fprintf((FILE *)usalp->errfile,
						"s = %d, t = %d, l = %d, dev = %s, fd = %d\n",
						s, t, l,
						devnm,
						sdidevs[s][t][l].fd);
				}

			}
		}
	}

	return (nopen);
}


static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
	int	tlun	= usal_lun(usalp);
	int	b, t, l;

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

	if (!sdiinit) {
		sdiinit = TRUE;
		memset(sdidevs, 0, sizeof (sdidevs));	/* init tmp_structure */
		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++) {

					sdidevs[b][t][l].flags = 0;
					sdidevs[b][t][l].fd = -1;
					sdidevs[b][t][l].open = 0;
				}
			}
		}
	}

	if (*device != '\0') {		/* we don't allow old dev usage */
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			"Open by 'devname' no longer supported on this OS");
		return (-1);
	} else {			/* this is the new stuff	 */
					/* it will do the initialisation */
					/* and return the number of	 */
					/* detected devices to be used	 */
					/* with the new addressing	 */
					/* scheme.			 */

		return (unixware_init(usalp));
	}

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
				if (f >= 0) {
					if (sdidevs[b][t][l].open > 0)
						sdidevs[b][t][l].open--;
					if (sdidevs[b][t][l].open <= 0) {
						if (sdidevs[b][t][l].fd >= 0)
							close(sdidevs[b][t][l].fd);
						sdidevs[b][t][l].fd    = -1;
						sdidevs[b][t][l].flags &= ~SDI_VALID;
					}
				}
				usallocal(usalp)->usalfiles[b][t][l] = (short)-1;
			}
		}
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (MAX_DMA);
}


static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = (void *) valloc((size_t)(amt));

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
	register int	t;
	register int	l;
	register int	busno;

	busno = usal_scsibus(usalp);

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++)
			if ((sdidevs[busno][t][l].flags & SDI_INITIATOR) != 0) {
				if (usalp->debug > 0) {
					fprintf((FILE *)usalp->errfile,
						"usalo_initiator_id: id = %d\n", t);
				}
				return (t);
			}
	}

	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	/* if the new address method is used we know if this is ATAPI */

	return ((sdidevs[usal_scsibus(usalp)][usal_target(usalp)][usal_lun(usalp)].flags & SDI_ATAPI) != 0);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	int	f = usalp->fd;

	errno = EINVAL;

#if defined(SDI_TRESET) || defined(SDI_BRESET)
	if (what == SCG_RESET_NOP) {
		errno = 0;
		return (0);
	}

#ifdef	SDI_TRESET
	if (what == SCG_RESET_TGT) {
		errno = 0;
		if (ioctl(f, SDI_TRESET, 0) >= 0)
			return (0);
	}
#endif

#ifdef	SDI_BRESET
	if (what == SCG_RESET_BUS) {
		errno = 0;
		if (ioctl(f, SDI_BRESET, 0) >= 0)
			return (0);
	}
#endif

#endif	/* defined(SDI_TRESET) || defined(SDI_BRESET) */

	return (-1);
}

static int
do_usal_cmd(SCSI *usalp, struct usal_cmd *sp)
{
	int			ret;
	int			i;
	struct sb		scsi_cmd;
	struct scb		*scbp;

	memset(&scsi_cmd,  0, sizeof (scsi_cmd));

	scsi_cmd.sb_type = ISCB_TYPE;
	scbp = &scsi_cmd.SCB;

	scbp->sc_cmdpt = (caddr_t) sp->cdb.cmd_cdb;
	scbp->sc_cmdsz = sp->cdb_len;

	scbp->sc_datapt = sp->addr;
	scbp->sc_datasz = sp->size;

	if (!(sp->flags & SCG_RECV_DATA) && (sp->size > 0))
		scbp->sc_mode = SCB_WRITE;
	else
		scbp->sc_mode = SCB_READ;

	scbp->sc_time = sp->timeout;

	sp->error = SCG_NO_ERROR;
	errno = 0;
	for (;;) {
		if ((ret = ioctl(usalp->fd, SDI_SEND, &scsi_cmd)) < 0) {
			if (errno == EAGAIN) {
				sleep(1);
				errno = 0;
				continue;
			}
			sp->ux_errno = errno;
			if (errno == 0)
				sp->ux_errno = EIO;
			sp->error = SCG_RETRYABLE;

#ifdef	__needed__
			if (errno == ENOTTY || errno == EINVAL ||
			    errno == EACCES) {
				return (-1);
			}
#endif
			return (ret);
		}
		break;
	}
	sp->ux_errno = errno;
	sp->resid = scbp->sc_resid;
	memset(&sp->u_scb.Scb, 0, sizeof (sp->u_scb.Scb));
	sp->u_scb.cmd_scb[0] = scbp->sc_status;

	if (sp->u_scb.cmd_scb[0] & 0x02) {
		if (sp->ux_errno == 0)
			sp->ux_errno = EIO;
	}

	switch (scbp->sc_comp_code) {

		case SDI_ASW	 : /* Job completed normally		*/
		case SDI_LINKF0	 : /* Linked command done without flag	*/
		case SDI_LINKF1	 : /* Linked command done with flag	*/

				sp->error = SCG_NO_ERROR;
				break;

		case SDI_CKSTAT	 : /* Check the status byte		*/

				sp->error = SCG_NO_ERROR;
				break;

		case SDI_NOALLOC : /* This block is not allocated	*/
		case SDI_NOTEQ	 : /* Addressed device not present	*/
		case SDI_OOS	 : /* Device is out of service		*/
		case SDI_NOSELE	 : /* The SCSI bus select failed	*/
		case SDI_SBRESC	 : /* SCSI bus reservation conflict	*/

				sp->error = SCG_FATAL;
				if (sp->ux_errno == 0)
					sp->ux_errno = EIO;
				break;

		case SDI_QFLUSH	 : /* Job was flushed			*/
		case SDI_ABORT	 : /* Command was aborted		*/
		case SDI_RESET	 : /* Reset was detected on the bus	*/
		case SDI_CRESET	 : /* Reset was caused by this unit	*/
		case SDI_V2PERR	 : /* vtop failed			*/
		case SDI_HAERR	 : /* Host adapter error		*/
		case SDI_MEMERR	 : /* Memory fault			*/
		case SDI_SBUSER	 : /* SCSI bus error			*/
		case SDI_SCBERR	 : /* SCB error				*/
		case SDI_MISMAT	 : /* parameter mismatch		*/

		case SDI_PROGRES : /* Job in progress			*/
		case SDI_UNUSED	 : /* Job not in use			*/

		case SDI_ONEIC	 : /* More than one immediate request	*/
		case SDI_SFBERR	 : /* SFB error				*/
		case SDI_TCERR	 : /* Target protocol error detected	*/
		default:
				sp->error = SCG_RETRYABLE;
				if (sp->ux_errno == 0)
					sp->ux_errno = EIO;
				break;

		case SDI_TIME	 : /* Job timed out			*/
		case SDI_TIME_NOABORT : /* Job timed out, but could not be aborted */

				sp->error = SCG_TIMEOUT;
				if (sp->ux_errno == 0)
					sp->ux_errno = EIO;
				break;
	}
	return (0);
}


static int
do_usal_sense(SCSI *usalp, struct usal_cmd *sp)
{
	int		ret;
	struct usal_cmd	s_cmd;

	memset((caddr_t)&s_cmd, 0, sizeof (s_cmd));

	s_cmd.addr	= (caddr_t) sp->u_sense.cmd_sense;
	s_cmd.size	= sp->sense_len;
	s_cmd.flags	= SCG_RECV_DATA|SCG_DISRE_ENA;
	s_cmd.cdb_len	= SC_G0_CDBLEN;
	s_cmd.sense_len	= CCS_SENSE_LEN;

	s_cmd.cdb.g0_cdb.cmd   = SC_REQUEST_SENSE;
	s_cmd.cdb.g0_cdb.lun   = sp->cdb.g0_cdb.lun;
	s_cmd.cdb.g0_cdb.count = sp->sense_len;

	ret = do_usal_cmd(usalp, &s_cmd);

	if (ret < 0)
		return (ret);

	sp->sense_count = sp->sense_len - s_cmd.resid;
	return (ret);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	int	ret;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	ret = do_usal_cmd(usalp, sp);
	if (ret < 0)
		return (ret);

	if (sp->u_scb.cmd_scb[0] & S_CKCON)
		ret = do_usal_sense(usalp, sp);

	return (ret);
}

#define	sense	u_sense.Sense
#undef	SC_PARITY
#define	SC_PARITY	0x09
#define	scb		u_scb.Scb

/*--------------------------------------------------------------------------*/
#include <unixstd.h>
#include <waitdefs.h>
/*
 * Simplified version of popen()
 * This version of popen() is not usable more than once at a time.
 * Needed because /etc/scsi/pdiconfig will not work if euid != uid
 */
static pid_t	po_pid;

static FILE *
xpopen(char *cmd, char *type)
{
	FILE	*ret;
	FILE	*pp[2];

	if (po_pid != 0)
		return ((FILE *)NULL);

	if (*type != 'r')
		return ((FILE *)NULL);

	if (fpipe(pp) == 0)
		return ((FILE *)NULL);


	if ((po_pid = fork()) == 0) {
		setuid(0);

		fclose(pp[0]);
		(void) fexecl("/bin/sh", stdin, pp[1], stderr,
					"sh", "-c", cmd, (char *)0);
		_exit(1);
	}
	fclose(pp[1]);

	if (po_pid == (pid_t)-1) {
		fclose(pp[0]);
		return ((FILE *)NULL);
	}
	return (pp[0]);
}

static int
xpclose(FILE *f)
{
	int	ret = 0;

	if (po_pid == 0)
		return (-1);

	fclose(f);

	if (waitpid(po_pid, &ret, 0) < 0)
		ret = -1;

	po_pid = 0;
	return (ret);
}
