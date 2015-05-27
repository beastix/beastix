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

/* @(#)scsi-sun.c	1.83 05/11/20 Copyright 1988,1995,2000-2004 J. Schilling */
/*
 *	SCSI user level command transport routines for
 *	the SCSI general driver 'usal'.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1988,1995,2000-2004 J. Schilling
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

#include <usal/usalio.h>

#include <libport.h>		/* Needed for gethostid() */
#ifdef	HAVE_SUN_DKIO_H
#	include <sun/dkio.h>

#	define	dk_cinfo	dk_conf
#	define	dki_slave	dkc_slave
#	define	DKIO_GETCINFO	DKIOCGCONF
#endif
#ifdef	HAVE_SYS_DKIO_H
#	include <sys/dkio.h>

#	define	DKIO_GETCINFO	DKIOCINFO
#endif

#define	TARGET(slave)	((slave) >> 3)
#define	LUN(slave)	((slave) & 07)

/*
 * Tht USCSI ioctl() is not usable on SunOS 4.x
 */
#ifdef	__SVR4
/*#define	VOLMGT_DEBUG*/
#include <volmgt.h>
#include <statdefs.h>
#	define	USE_USCSI
#endif

static	char	_usal_trans_version[] = "usal-1.83";	/* The version for /dev/usal	*/
static	char	_usal_utrans_version[] = "uscsi-1.83";	/* The version for USCSI	*/

#ifdef	USE_USCSI
static	int	usalo_uhelp(SCSI *usalp, FILE *f);
static	int	usalo_uopen(SCSI *usalp, char *device);
static	int	usalo_volopen(SCSI *usalp, char *devname);
static	int	usalo_openmedia(SCSI *usalp, char *mname);
static	int	usalo_uclose(SCSI *usalp);
static	int	usalo_ucinfo(int f, struct dk_cinfo *cp, int debug);
static	int	usalo_ugettlun(int f, int *tgtp, int *lunp);
static	long	usalo_umaxdma(SCSI *usalp, long amt);
static	int	usalo_openide(void);
static	BOOL	usalo_uhavebus(SCSI *usalp, int);
static	int	usalo_ufileno(SCSI *usalp, int, int, int);
static	int	usalo_uinitiator_id(SCSI *usalp);
static	int	usalo_uisatapi(SCSI *usalp);
static	int	usalo_ureset(SCSI *usalp, int what);
static	int	usalo_usend(SCSI *usalp);

static	int	have_volmgt = -1;

static usal_ops_t sun_uscsi_ops = {
	usalo_usend,
	usalo_version,		/* Shared with SCG driver */
	usalo_uhelp,
	usalo_uopen,
	usalo_uclose,
	usalo_umaxdma,
	usalo_getbuf,		/* Shared with SCG driver */
	usalo_freebuf,		/* Shared with SCG driver */
	usalo_uhavebus,
	usalo_ufileno,
	usalo_uinitiator_id,
	usalo_uisatapi,
	usalo_ureset,
};
#endif

/*
 * Need to move this into an usal driver ioctl.
 */
/*#define	MAX_DMA_SUN4M	(1024*1024)*/
#define	MAX_DMA_SUN4M	(124*1024)	/* Currently max working size */
/*#define	MAX_DMA_SUN4C	(126*1024)*/
#define	MAX_DMA_SUN4C	(124*1024)	/* Currently max working size */
#define	MAX_DMA_SUN3	(63*1024)
#define	MAX_DMA_SUN386	(56*1024)
#define	MAX_DMA_OTHER	(32*1024)

#define	ARCH_MASK	0xF0
#define	ARCH_SUN2	0x00
#define	ARCH_SUN3	0x10
#define	ARCH_SUN4	0x20
#define	ARCH_SUN386	0x30
#define	ARCH_SUN3X	0x40
#define	ARCH_SUN4C	0x50
#define	ARCH_SUN4E	0x60
#define	ARCH_SUN4M	0x70
#define	ARCH_SUNX	0x80

/*
 * We are using a "real" /dev/usal?
 */
#define	scsi_xsend(usalp)	ioctl((usalp)->fd, SCGIO_CMD, (usalp)->scmd)
#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

struct usal_local {
	union {
		int	SCG_files[MAX_SCG];
#ifdef	USE_USCSI
		short	usal_files[MAX_SCG][MAX_TGT][MAX_LUN];
#endif
	} u;
};
#define	usallocal(p)	((struct usal_local *)((p)->local))
#define	usalfiles(p)	(usallocal(p)->u.SCG_files)

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
#ifdef	USE_USCSI
			if (usalp->ops == &sun_uscsi_ops)
				return (_usal_utrans_version);
#endif
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
	__usal_help(f, "usal", "Generic transport independent SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
#ifdef	USE_USCSI
	usalo_uhelp(usalp, f);
#endif
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
/*		int	tlun	= usal_lun(usalp);*/
	register int	f;
	register int	i;
	register int	nopen = 0;
	char		devname[32];

	if (busno >= MAX_SCG) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno '%d'", busno);
		return (-1);
	}

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
#ifdef	USE_USCSI
		usalp->ops = &sun_uscsi_ops;
		return (SCGO_OPEN(usalp, device));
#else
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
#endif
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE, "No memory for usal_local");
			return (0);
		}

		for (i = 0; i < MAX_SCG; i++) {
			usalfiles(usalp)[i] = -1;
		}
	}


	for (i = 0; i < MAX_SCG; i++) {
		/*
		 * Skip unneeded devices if not in SCSI Bus scan open mode
		 */
		if (busno >= 0 && busno != i)
			continue;
		snprintf(devname, sizeof (devname), "/dev/usal%d", i);
		f = open(devname, O_RDWR);
		if (f < 0) {
			if (errno != ENOENT && errno != ENXIO) {
				if (usalp->errstr)
					snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
						"Cannot open '%s'", devname);
				return (-1);
			}
		} else {
			nopen++;
		}
		usalfiles(usalp)[i] = f;
	}
#ifdef	USE_USCSI
	if (nopen <= 0) {
		if (usalp->local != NULL) {
			free(usalp->local);
			usalp->local = NULL;
		}
		usalp->ops = &sun_uscsi_ops;
		return (SCGO_OPEN(usalp, device));
	}
#endif
	return (nopen);
}

static int
usalo_close(SCSI *usalp)
{
	register int	i;

	if (usalp->local == NULL)
		return (-1);

	for (i = 0; i < MAX_SCG; i++) {
		if (usalfiles(usalp)[i] >= 0)
			close(usalfiles(usalp)[i]);
		usalfiles(usalp)[i] = -1;
	}
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long	maxdma = MAX_DMA_OTHER;
#if	!defined(__i386_) && !defined(i386)
	int	cpu_type;
#endif

#if	defined(__i386_) || defined(i386)
	maxdma = MAX_DMA_SUN386;
#else
	cpu_type = gethostid() >> 24;

	switch (cpu_type & ARCH_MASK) {

	case ARCH_SUN4C:
	case ARCH_SUN4E:
		maxdma = MAX_DMA_SUN4C;
		break;

	case ARCH_SUN4M:
	case ARCH_SUNX:
		maxdma = MAX_DMA_SUN4M;
		break;

	default:
		maxdma = MAX_DMA_SUN3;
	}
#endif

#ifndef	__SVR4
	/*
	 * SunOS 4.x allows esp hardware on VME boards and thus
	 * limits DMA on esp to 64k-1
	 */
	if (maxdma > MAX_DMA_SUN3)
		maxdma = MAX_DMA_SUN3;
#endif
	return (maxdma);
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	if (usalp->local == NULL)
		return (FALSE);

	return (busno < 0 || busno >= MAX_SCG) ? FALSE : (usalfiles(usalp)[busno] >= 0);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (usalp->local == NULL)
		return (-1);

	return ((busno < 0 || busno >= MAX_SCG) ? -1 : usalfiles(usalp)[busno]);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	int		id = -1;
#ifdef	DKIO_GETCINFO
	struct dk_cinfo	conf;
#endif

#ifdef	DKIO_GETCINFO
	if (usalp->fd < 0)
		return (id);
	if (ioctl(usalp->fd, DKIO_GETCINFO, &conf) < 0)
		return (id);
	if (TARGET(conf.dki_slave) != -1)
		id = TARGET(conf.dki_slave);
#endif
	return (id);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}
	return (ioctl(usalp->fd, SCGIORESET, 0));
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	usalp->bufbase = (void *)valloc((size_t)amt);
	return (usalp->bufbase);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static int
usalo_send(SCSI *usalp)
{
	usalp->scmd->target = usal_target(usalp);
	return (ioctl(usalp->fd, SCGIO_CMD, usalp->scmd));
}

/*--------------------------------------------------------------------------*/
/*
 *	This is Sun USCSI interface code ...
 */
#ifdef	USE_USCSI
#include <sys/scsi/impl/uscsi.h>

/*
 * Bit Mask definitions, for use accessing the status as a byte.
 */
#define	STATUS_MASK			0x3E
#define	STATUS_GOOD			0x00
#define	STATUS_CHECK			0x02

#define	STATUS_RESERVATION_CONFLICT	0x18
#define	STATUS_TERMINATED		0x22

#ifdef	nonono
#define	STATUS_MASK			0x3E
#define	STATUS_GOOD			0x00
#define	STATUS_CHECK			0x02

#define	STATUS_MET			0x04
#define	STATUS_BUSY			0x08
#define	STATUS_INTERMEDIATE		0x10
#define	STATUS_SCSI2			0x20
#define	STATUS_INTERMEDIATE_MET		0x14
#define	STATUS_RESERVATION_CONFLICT	0x18
#define	STATUS_TERMINATED		0x22
#define	STATUS_QFULL			0x28
#define	STATUS_ACA_ACTIVE		0x30
#endif

static int
usalo_uhelp(SCSI *usalp, FILE *f)
{
	__usal_help(f, "USCSI", "SCSI transport for targets known by Solaris drivers",
		"USCSI:", "bus,target,lun", "USCSI:1,2,0", TRUE, TRUE);
	return (0);
}

static int
usalo_uopen(SCSI *usalp, char *device)
{
		int	busno	= usal_scsibus(usalp);
		int	tgt	= usal_target(usalp);
		int	tlun	= usal_lun(usalp);
	register int	f;
	register int	b;
	register int	t;
	register int	l;
	register int	nopen = 0;
	char		devname[32];

	if (have_volmgt < 0)
		have_volmgt = volmgt_running();

	if (usalp->overbose) {
		fprintf((FILE *)usalp->errfile,
				"Warning: Using USCSI interface.\n");
	}
#ifndef	SEEK_HOLE
	/*
	 * SEEK_HOLE first appears in Solaris 11 Build 14, volmgt supports
	 * medialess drives since Build 21. Using SEEK_HOLD as indicator
	 * seems to be the best guess.
	 */
	if (usalp->overbose > 0 && have_volmgt) {
		fprintf((FILE *)usalp->errfile,
		"Warning: Volume management is running, medialess managed drives are invisible.\n");
	}
#endif

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr) {
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
					busno, tgt, tlun);
		}
		return (-1);
	}
	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL) {
			if (usalp->errstr)
				snprintf(usalp->errstr, SCSI_ERRSTR_SIZE, "No memory for usal_local");
			return (0);
		}

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++)
					usallocal(usalp)->u.usal_files[b][t][l] = (short)-1;
			}
		}
	}

	if (device != NULL && strcmp(device, "USCSI") == 0)
		goto uscsiscan;

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2))
		goto openbydev;

uscsiscan:
	if (busno >= 0 && tgt >= 0 && tlun >= 0) {

		if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN)
			return (-1);

		snprintf(devname, sizeof (devname), "/dev/rdsk/c%dt%dd%ds2",
			busno, tgt, tlun);
		f = open(devname, O_RDONLY | O_NDELAY);
		if (f < 0 && geterrno() == EBUSY)
			f = usalo_volopen(usalp, devname);
		if (f < 0) {
			snprintf(usalp->errstr,
				    SCSI_ERRSTR_SIZE,
				"Cannot open '%s'", devname);
			return (0);
		}
		usallocal(usalp)->u.usal_files[busno][tgt][tlun] = f;
		return (1);
	} else {

		for (b = 0; b < MAX_SCG; b++) {
			for (t = 0; t < MAX_TGT; t++) {
				for (l = 0; l < MAX_LUN; l++) {
					snprintf(devname, sizeof (devname),
						"/dev/rdsk/c%dt%dd%ds2",
						b, t, l);
					f = open(devname, O_RDONLY | O_NDELAY);
					if (f < 0 && geterrno() == EBUSY) {
						f = usalo_volopen(usalp, devname);
						/*
						 * Hack to mark inaccessible
						 * drives with fd == -2
						 */
						if (f < 0 &&
						    usallocal(usalp)->u.usal_files[b][t][l] < 0)
							usallocal(usalp)->u.usal_files[b][t][l] = f;
					}
					if (f < 0 && errno != ENOENT &&
						    errno != ENXIO &&
						    errno != ENODEV) {
						if (usalp->errstr)
							snprintf(usalp->errstr,
							    SCSI_ERRSTR_SIZE,
							    "Cannot open '%s'", devname);
					}
					if (f < 0 && l == 0)
						break;
					if (f >= 0) {
						nopen ++;
						if (usallocal(usalp)->u.usal_files[b][t][l] == -1)
							usallocal(usalp)->u.usal_files[b][t][l] = f;
						else
							close(f);
					}
				}
			}
		}
	}
openbydev:
	if (nopen == 0) {
		int	target;
		int	lun;

		if (device != NULL && strncmp(device, "USCSI:", 6) == 0)
			device += 6;
		if (device == NULL || device[0] == '\0')
			return (0);

		f = open(device, O_RDONLY | O_NDELAY);
		if (f < 0)
			f = usalo_volopen(usalp, device);
		if (f < 0) {
			snprintf(usalp->errstr,
				    SCSI_ERRSTR_SIZE,
				"Cannot open '%s'", device);
			return (0);
		}

		if (busno < 0)
			busno = 0;	/* Use Fake number if not specified */

		if (usalo_ugettlun(f, &target, &lun) >= 0) {
			if (tgt >= 0 && tlun >= 0) {
				if (tgt != target || tlun != lun) {
					close(f);
					return (0);
				}
			}
			tgt = target;
			tlun = lun;
		} else {
			if (tgt < 0 || tlun < 0) {
				close(f);
				return (0);
			}
		}

		if (usallocal(usalp)->u.usal_files[busno][tgt][tlun] == -1)
			usallocal(usalp)->u.usal_files[busno][tgt][tlun] = f;
		usal_settarget(usalp, busno, tgt, tlun);

		return (++nopen);
	}
	return (nopen);
}

static int
usalo_volopen(SCSI *usalp, char *devname)
{
	int	oerr = geterrno();
	int	f = -1;
	char	*name   = NULL;	/* Volume symbolic device name		*/
	char	*symdev = NULL;	/* /dev/... name based on "name" 	*/
	char	*mname  = NULL;	/* Volume media name based on "name"	*/

	if (!have_volmgt)
		return (-1);

#ifdef	VOLMGT_DEBUG
	usalp->debug++;
#endif
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"usalo_volopen(%s)\n", devname);
	}

	/*
	 * We come here because trying to open "devname" did not work.
	 * First try to translate between a symbolic name and a /dev/...
	 * based device name. Then translate back to a symbolic name.
	 */
	symdev = volmgt_symdev(devname);
	if (symdev)
		name = volmgt_symname(symdev);
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"volmgt_symdev(%s)=%s -> %s\n", devname, symdev, name);
	}

	/*
	 * If "devname" is not a symbolic device name, then it must be
	 * a /dev/... based device name. Try to translate it into a
	 * symbolic name. Then translate back to a /dev/... name.
	 */
	if (name == NULL) {
		name = volmgt_symname(devname);
		if (name)
			symdev = volmgt_symdev(name);
	}
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"volmgt_symdev(%s)=%s -> %s\n", devname, symdev, name);
	}

	/*
	 * If we have been able to translate to a symbolic device name,
	 * translate this name into a volume management media name that
	 * may be used for opening.
	 */
	if (name)
		mname = media_findname(name);
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"symdev %s name %s mname %s\n", symdev, name, mname);
	}

	/*
	 * Das scheint nur mit dem normierten /dev/rdsk/ *s2 Namen zu gehen.
	 */
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
			"volmgt_inuse(%s) %d\n", symdev, volmgt_inuse(symdev));
	}
	if (mname)
		f = usalo_openmedia(usalp, mname);
	else if (name)
		f = -2;	/* Mark inaccessible drives with fd == -2 */

	/*
	 * Besonderen Fehlertext oder fprintf/errfile bei non-scanbus Open und
	 * wenn errrno == EBUSY && kein Mapping?
	 */
	if (name)
		free(name);
	if (symdev)
		free(symdev);
	if (mname)
		free(mname);
	seterrno(oerr);
#ifdef	VOLMGT_DEBUG
	usalp->debug--;
#endif
	return (f);
}

static int
usalo_openmedia(SCSI *usalp, char *mname)
{
	int	f = -1;
	char	*device = NULL;
	struct	stat sb;

	if (mname == NULL)
		return (-1);

	/*
	 * Check whether the media name refers to a directory.
	 * In this case, the medium is partitioned and we need to
	 * check all partitions.
	 */
	if (stat(mname, &sb) >= 0) {
		if (S_ISDIR(sb.st_mode)) {
			char    name[128];
			int	i;

			/*
			 * First check partition '2', the whole disk.
			 */
			snprintf(name, sizeof (name), "%s/s2", mname);
			f = open(name, O_RDONLY | O_NDELAY);
			if (f >= 0)
				return (f);
			/*
			 * Now try all other partitions.
			 */
			for (i = 0; i < 16; i++) {
				if (i == 2)
					continue;
				snprintf(name, sizeof (name),
							"%s/s%d", mname, i);
				if (stat(name, &sb) >= 0)
					break;
			}
			if (i < 16) {
				device = mname;
			}
		} else {
			device = mname;
		}
	}
	if (device)
		f = open(device, O_RDONLY | O_NDELAY);
	return (f);
}

static int
usalo_uclose(SCSI *usalp)
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
				f = usallocal(usalp)->u.usal_files[b][t][l];
				if (f >= 0)
					close(f);
				usallocal(usalp)->u.usal_files[b][t][l] = (short)-1;
			}
		}
	}
	return (0);
}

static int
usalo_ucinfo(int f, struct dk_cinfo *cp, int debug)
{
	fillbytes(cp, sizeof (*cp), '\0');

	if (ioctl(f, DKIOCINFO, cp) < 0)
		return (-1);

	if (debug <= 0)
		return (0);

	fprintf(stderr, "cname:		'%s'\n", cp->dki_cname);
	fprintf(stderr, "ctype:		0x%04hX %hd\n", cp->dki_ctype, cp->dki_ctype);
	fprintf(stderr, "cflags:		0x%04hX\n", cp->dki_flags);
	fprintf(stderr, "cnum:		%hd\n", cp->dki_cnum);
#ifdef	__EVER__
	fprintf(stderr, "addr:		%d\n", cp->dki_addr);
	fprintf(stderr, "space:		%d\n", cp->dki_space);
	fprintf(stderr, "prio:		%d\n", cp->dki_prio);
	fprintf(stderr, "vec:		%d\n", cp->dki_vec);
#endif
	fprintf(stderr, "dname:		'%s'\n", cp->dki_dname);
	fprintf(stderr, "unit:		%d\n", cp->dki_unit);
	fprintf(stderr, "slave:		%d %04o Tgt: %d Lun: %d\n",
				cp->dki_slave, cp->dki_slave,
				TARGET(cp->dki_slave), LUN(cp->dki_slave));
	fprintf(stderr, "partition:	%hd\n", cp->dki_partition);
	fprintf(stderr, "maxtransfer:	%d (%d)\n",
				cp->dki_maxtransfer,
				cp->dki_maxtransfer * DEV_BSIZE);
	return (0);
}

static int
usalo_ugettlun(int f, int *tgtp, int *lunp)
{
	struct dk_cinfo ci;

	if (usalo_ucinfo(f, &ci, 0) < 0)
		return (-1);
	if (tgtp)
		*tgtp = TARGET(ci.dki_slave);
	if (lunp)
		*lunp = LUN(ci.dki_slave);
	return (0);
}

static long
usalo_umaxdma(SCSI *usalp, long amt)
{
	register int	b;
	register int	t;
	register int	l;
	long		maxdma = -1L;
	int		f;
	struct dk_cinfo ci;
	BOOL		found_ide = FALSE;

	if (usalp->local == NULL)
		return (-1L);

	for (b = 0; b < MAX_SCG; b++) {
		for (t = 0; t < MAX_TGT; t++) {
			for (l = 0; l < MAX_LUN; l++) {
				if ((f = usallocal(usalp)->u.usal_files[b][t][l]) < 0)
					continue;
				if (usalo_ucinfo(f, &ci, usalp->debug) < 0)
					continue;
				if (maxdma < 0)
					maxdma = (long)(ci.dki_maxtransfer * DEV_BSIZE);
				if (maxdma > (long)(ci.dki_maxtransfer * DEV_BSIZE))
					maxdma = (long)(ci.dki_maxtransfer * DEV_BSIZE);
				if (streql(ci.dki_cname, "ide"))
					found_ide = TRUE;
			}
		}
	}

#if	defined(__i386_) || defined(i386)
	/*
	 * At least on Solaris 9 x86, DKIOCINFO returns a wrong value
	 * for dki_maxtransfer if the target is an ATAPI drive.
	 * Without DMA, it seems to work if we use 256 kB DMA size for ATAPI,
	 * but if we allow DMA, only 68 kB will work (for more we get a silent
	 * DMA residual count == DMA transfer count).
	 * For this reason, we try to figure out the correct value for 'ide'
	 * by retrieving the (correct) value from a ide hard disk.
	 */
	if (found_ide) {
		if ((f = usalo_openide()) >= 0) {
#ifdef	sould_we
			long omaxdma = maxdma;
#endif

			if (usalo_ucinfo(f, &ci, usalp->debug) >= 0) {
				if (maxdma < 0)
					maxdma = (long)(ci.dki_maxtransfer * DEV_BSIZE);
				if (maxdma > (long)(ci.dki_maxtransfer * DEV_BSIZE))
					maxdma = (long)(ci.dki_maxtransfer * DEV_BSIZE);
			}
			close(f);
#ifdef	sould_we
			/*
			 * The kernel returns 56 kB but we tested that 68 kB works.
			 */
			if (omaxdma > maxdma && maxdma == (112 * DEV_BSIZE))
				maxdma = 136 * DEV_BSIZE;
#endif
		} else {
			/*
			 * No IDE disk on this system?
			 */
			if (maxdma == (512 * DEV_BSIZE))
				maxdma = MAX_DMA_SUN386;
		}
	}
#endif
	/*
	 * The Sun tape driver does not support to retrieve the max DMA count.
	 * Use the knwoledge about default DMA sizes in this case.
	 */
	if (maxdma < 0)
		maxdma = usalo_maxdma(usalp, amt);

	return (maxdma);
}

#if	defined(__i386_) || defined(i386)
static int
usalo_openide()
{
	char	buf[20];
	int	b;
	int	t;
	int	f = -1;

	for (b = 0; b < 5; b++) {
		for (t = 0; t < 2; t++) {
			snprintf(buf, sizeof (buf),
				"/dev/rdsk/c%dd%dp0", b, t);
			if ((f = open(buf, O_RDONLY | O_NDELAY)) >= 0)
				goto out;
		}
	}
out:
	return (f);
}
#endif

static BOOL
usalo_uhavebus(SCSI *usalp, int busno)
{
	register int	t;
	register int	l;

	if (usalp->local == NULL || busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++)
			if (usallocal(usalp)->u.usal_files[busno][t][l] >= 0)
				return (TRUE);
	}
	return (FALSE);
}

static int
usalo_ufileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (usalp->local == NULL ||
	    busno < 0 || busno >= MAX_SCG ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	return ((int)usallocal(usalp)->u.usal_files[busno][tgt][tlun]);
}

static int
usalo_uinitiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_uisatapi(SCSI *usalp)
{
	char		devname[32];
	char		symlinkname[MAXPATHLEN];
	int		len;
	struct dk_cinfo ci;

	if (ioctl(usalp->fd, DKIOCINFO, &ci) < 0)
		return (-1);

	snprintf(devname, sizeof (devname), "/dev/rdsk/c%dt%dd%ds2",
		usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));

	symlinkname[0] = '\0';
	len = readlink(devname, symlinkname, sizeof (symlinkname));
	if (len > 0)
		symlinkname[len] = '\0';

	if (len >= 0 && strstr(symlinkname, "ide") != NULL)
		return (TRUE);
	else
		return (FALSE);
}

static int
usalo_ureset(SCSI *usalp, int what)
{
	struct uscsi_cmd req;

	if (what == SCG_RESET_NOP)
		return (0);

	fillbytes(&req, sizeof (req), '\0');

	if (what == SCG_RESET_TGT) {
		req.uscsi_flags = USCSI_RESET | USCSI_SILENT;	/* reset target */
	} else if (what != SCG_RESET_BUS) {
		req.uscsi_flags = USCSI_RESET_ALL | USCSI_SILENT; /* reset bus */
	} else {
		errno = EINVAL;
		return (-1);
	}

	return (ioctl(usalp->fd, USCSICMD, &req));
}

static int
usalo_usend(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	struct uscsi_cmd req;
	int		ret;
static	uid_t		cureuid = 0;	/* XXX Hack until we have uid management */

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	fillbytes(&req, sizeof (req), '\0');

	req.uscsi_flags = USCSI_SILENT | USCSI_DIAGNOSE | USCSI_RQENABLE;

	if (sp->flags & SCG_RECV_DATA) {
		req.uscsi_flags |= USCSI_READ;
	} else if (sp->size > 0) {
		req.uscsi_flags |= USCSI_WRITE;
	}
	req.uscsi_buflen	= sp->size;
	req.uscsi_bufaddr	= sp->addr;
	req.uscsi_timeout	= sp->timeout;
	req.uscsi_cdblen	= sp->cdb_len;
	req.uscsi_rqbuf		= (caddr_t) sp->u_sense.cmd_sense;
	req.uscsi_rqlen		= sp->sense_len;
	req.uscsi_cdb		= (caddr_t) &sp->cdb;

	if (cureuid != 0)
		seteuid(0);
again:
	errno = 0;
	ret = ioctl(usalp->fd, USCSICMD, &req);

	if (ret < 0 && geterrno() == EPERM) {	/* XXX Hack until we have uid management */
		cureuid = geteuid();
		if (seteuid(0) >= 0)
			goto again;
	}
	if (cureuid != 0)
		seteuid(cureuid);

	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile, "ret: %d errno: %d (%s)\n", ret, errno, errmsgstr(errno));
		fprintf((FILE *)usalp->errfile, "uscsi_flags:     0x%x\n", req.uscsi_flags);
		fprintf((FILE *)usalp->errfile, "uscsi_status:    0x%x\n", req.uscsi_status);
		fprintf((FILE *)usalp->errfile, "uscsi_timeout:   %d\n", req.uscsi_timeout);
		fprintf((FILE *)usalp->errfile, "uscsi_bufaddr:   0x%lx\n", (long)req.uscsi_bufaddr);
								/*
								 * Cast auf int OK solange sp->size
								 * auch ein int bleibt.
								 */
		fprintf((FILE *)usalp->errfile, "uscsi_buflen:    %d\n", (int)req.uscsi_buflen);
		fprintf((FILE *)usalp->errfile, "uscsi_resid:     %d\n", (int)req.uscsi_resid);
		fprintf((FILE *)usalp->errfile, "uscsi_rqlen:     %d\n", req.uscsi_rqlen);
		fprintf((FILE *)usalp->errfile, "uscsi_rqstatus:  0x%x\n", req.uscsi_rqstatus);
		fprintf((FILE *)usalp->errfile, "uscsi_rqresid:   %d\n", req.uscsi_rqresid);
		fprintf((FILE *)usalp->errfile, "uscsi_rqbuf ptr: 0x%lx\n", (long)req.uscsi_rqbuf);
		fprintf((FILE *)usalp->errfile, "uscsi_rqbuf:     ");
		if (req.uscsi_rqbuf != NULL && req.uscsi_rqlen > req.uscsi_rqresid) {
			int	i;
			int	len = req.uscsi_rqlen - req.uscsi_rqresid;

			for (i = 0; i < len; i++) {
				fprintf((FILE *)usalp->errfile, "0x%02X ", ((char *)req.uscsi_rqbuf)[i]);
			}
			fprintf((FILE *)usalp->errfile, "\n");
		} else {
			fprintf((FILE *)usalp->errfile, "<data not available>\n");
		}
	}
	if (ret < 0) {
		sp->ux_errno = geterrno();
		/*
		 * Check if SCSI command cound not be send at all.
		 */
		if (sp->ux_errno == ENOTTY && usalo_uisatapi(usalp) == TRUE) {
			if (usalp->debug > 0) {
				fprintf((FILE *)usalp->errfile,
					"ENOTTY atapi: %d\n", usalo_uisatapi(usalp));
			}
			sp->error = SCG_FATAL;
			return (0);
		}
		if (errno == ENXIO) {
			sp->error = SCG_FATAL;
			return (0);
		}
		if (errno == ENOTTY || errno == EINVAL || errno == EACCES) {
			return (-1);
		}
	} else {
		sp->ux_errno = 0;
	}
	ret			= 0;
	sp->sense_count		= req.uscsi_rqlen - req.uscsi_rqresid;
	sp->resid		= req.uscsi_resid;
	sp->u_scb.cmd_scb[0]	= req.uscsi_status;

	if ((req.uscsi_status & STATUS_MASK) == STATUS_GOOD) {
		sp->error = SCG_NO_ERROR;
		return (0);
	}
	if (req.uscsi_rqstatus == 0 &&
	    ((req.uscsi_status & STATUS_MASK) == STATUS_CHECK)) {
		sp->error = SCG_NO_ERROR;
		return (0);
	}
	if (req.uscsi_status & (STATUS_TERMINATED |
	    STATUS_RESERVATION_CONFLICT)) {
		sp->error = SCG_FATAL;
	}
	if (req.uscsi_status != 0) {
		/*
		 * This is most likely wrong. There seems to be no way
		 * to produce SCG_RETRYABLE with USCSI.
		 */
		sp->error = SCG_RETRYABLE;
	}

	return (ret);
}
#endif	/* USE_USCSI */
