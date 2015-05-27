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

/* @(#)isodebug.c	1.10 05/05/01 Copyright 1996-2004 J. Schilling */
/*
 *	Copyright (c) 1996-2004 J. Schilling
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
#include <sys/types.h>
#include <sys/stat.h>
#include <statdefs.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <standard.h>
#include <utypes.h>
#include <intcvt.h>
#include <schily.h>

#include "../scsi.h"
#include "../../wodim/defaults.h"

#define	_delta(from, to)	((to) - (from) + 1)

#define	VD_BOOT		0
#define	VD_PRIMARY	1
#define	VD_SUPPLEMENT	2
#define	VD_PARTITION	3
#define	VD_TERM		255

#define	VD_ID		"CD001"

struct	iso9660_voldesc {
	char	vd_type		[_delta(1, 1)];
	char	vd_id		[_delta(2, 6)];
	char	vd_version	[_delta(7, 7)];
	char	vd_fill		[_delta(8, 2048)];
};

struct	iso9660_boot_voldesc {
	char	vd_type		[_delta(1, 1)];
	char	vd_id		[_delta(2, 6)];
	char	vd_version	[_delta(7, 7)];
	char	vd_bootsys	[_delta(8, 39)];
	char	vd_bootid	[_delta(40, 71)];
	char	vd_bootcode	[_delta(72, 2048)];
};

struct	iso9660_pr_voldesc {
	char	vd_type			[_delta(1,	1)];
	char	vd_id			[_delta(2,	6)];
	char	vd_version		[_delta(7,	7)];
	char	vd_unused1		[_delta(8,	8)];
	char	vd_system_id		[_delta(9,	40)];
	char	vd_volume_id		[_delta(41,	72)];
	char	vd_unused2		[_delta(73,	80)];
	char	vd_volume_space_size	[_delta(81,	88)];
	char	vd_unused3		[_delta(89,	120)];
	char	vd_volume_set_size	[_delta(121,	124)];
	char	vd_volume_seq_number	[_delta(125,	128)];
	char	vd_lbsize		[_delta(129,	132)];
	char	vd_path_table_size	[_delta(133,	140)];
	char	vd_pos_path_table_l	[_delta(141,	144)];
	char	vd_opt_pos_path_table_l	[_delta(145,	148)];
	char	vd_pos_path_table_m	[_delta(149,	152)];
	char	vd_opt_pos_path_table_m	[_delta(153,	156)];
	char	vd_root_dir		[_delta(157,	190)];
	char	vd_volume_set_id	[_delta(191,	318)];
	char	vd_publisher_id		[_delta(319,	446)];
	char	vd_data_preparer_id	[_delta(447,	574)];
	char	vd_application_id	[_delta(575,	702)];
	char	vd_copyr_file_id	[_delta(703,	739)];
	char	vd_abstr_file_id	[_delta(740,	776)];
	char	vd_bibl_file_id		[_delta(777,	813)];
	char	vd_create_time		[_delta(814,	830)];
	char	vd_mod_time		[_delta(831,	847)];
	char	vd_expiry_time		[_delta(848,	864)];
	char	vd_effective_time	[_delta(865,	881)];
	char	vd_file_struct_vers	[_delta(882,	882)];
	char	vd_reserved1		[_delta(883,	883)];
	char	vd_application_use	[_delta(884,	1395)];
	char	vd_fill			[_delta(1396,	2048)];
};

#define	GET_UBYTE(a)	a_to_u_byte(a)
#define	GET_SBYTE(a)	a_to_byte(a)
#define	GET_SHORT(a)	a_to_u_2_byte(&((unsigned char *) (a))[0])
#define	GET_BSHORT(a)	a_to_u_2_byte(&((unsigned char *) (a))[2])
#define	GET_INT(a)	a_to_4_byte(&((unsigned char *) (a))[0])
#define	GET_LINT(a)	la_to_4_byte(&((unsigned char *) (a))[0])
#define	GET_BINT(a)	a_to_4_byte(&((unsigned char *) (a))[4])

#define	infile	in_image
FILE		*infile = NULL;

static void	usage(int excode);
static char	*isodinfo(FILE *f);

static void
usage(int excode)
{
	errmsgno(EX_BAD, "Usage: %s [options] image\n",
						get_progname());

	fprintf(stderr, "Options:\n");
	fprintf(stderr, "\t-help,-h	Print this help\n");
	fprintf(stderr, "\t-version	Print version info and exit\n");
	fprintf(stderr, "\t-i filename	Filename to read ISO-9660 image from\n");
	fprintf(stderr, "\tdev=target	SCSI target to use as CD/DVD-Recorder\n");
	fprintf(stderr, "\nIf neither -i nor dev= are speficied, <image> is needed.\n");
	exit(excode);
}

static char *
isodinfo(FILE *f)
{
static	struct iso9660_voldesc		vd;
	struct iso9660_pr_voldesc	*vp;
#ifndef	USE_SCG
	struct stat			sb;
	mode_t				mode;
#endif
	BOOL				found = FALSE;
	off_t				sec_off = 16L;

#ifndef	USE_SCG
	/*
	 * First check if a bad guy tries to call isosize()
	 * with an unappropriate file descriptor.
	 * return -1 in this case.
	 */
	if (isatty(fileno(f)))
		return (NULL);
	if (fstat(fileno(f), &sb) < 0)
		return (NULL);
	mode = sb.st_mode & S_IFMT;
	if (!S_ISREG(mode) && !S_ISBLK(mode) && !S_ISCHR(mode))
		return (NULL);
#endif

	vp = (struct iso9660_pr_voldesc *) &vd;

	do {
#ifdef	USE_SCG
		readsecs(sec_off, &vd, 1);
#else
		if (lseek(fileno(f), (off_t)(sec_off * 2048L), SEEK_SET) == -1)
			return (NULL);
		read(fileno(f), &vd, sizeof (vd));
#endif
		sec_off++;

		if (GET_UBYTE(vd.vd_type) == VD_PRIMARY) {
			found = TRUE;
/*			break;*/
		}

	} while (GET_UBYTE(vd.vd_type) != VD_TERM);

	if (GET_UBYTE(vd.vd_type) != VD_TERM)
		return (NULL);

#ifdef	USE_SCG
	readsecs(sec_off, &vd, 1);
#else
	if (lseek(fileno(f), (off_t)(sec_off * 2048L), SEEK_SET) == -1)
		return (NULL);
	read(fileno(f), &vd, sizeof (vd));
#endif
	sec_off++;

	return ((char *)&vd);
}

int
main(int argc, char *argv[])
{
	int	cac;
	char	* const *cav;
	char	*opts = "help,h,version,i*,dev*";
	BOOL	help = FALSE;
	BOOL	prvers = FALSE;
	char	*filename = NULL;
	char	*devname = NULL;
	char	*p;
	char	*eol;

	save_args(argc, argv);

	cac = argc - 1;
	cav = argv + 1;
	if (getallargs(&cac, &cav, opts, &help, &help, &prvers,
			&filename, &devname) < 0) {
		errmsgno(EX_BAD, "Bad Option: '%s'\n", cav[0]);
		usage(EX_BAD);
	}
	if (help)
		usage(0);
	if (prvers) {
		printf("isodebug %s (%s)\n", CDRKIT_VERSION, HOST_SYSTEM);
		exit(0);
	}
	cac = argc - 1;
	cav = argv + 1;
	if (filename == NULL && devname == NULL) {
		if (getfiles(&cac, &cav, opts) != 0) {
			filename = cav[0];
			cac--, cav++;
		}
	}
	if (getfiles(&cac, &cav, opts) != 0) {
		errmsgno(EX_BAD, "Bad Argument: '%s'\n", cav[0]);
		usage(EX_BAD);
	}
	if (filename != NULL && devname != NULL) {
		errmsgno(EX_BAD, "Only one of -i or dev= allowed\n");
		usage(EX_BAD);
	}
#ifdef	USE_SCG
	if (filename == NULL && devname == NULL)
		cdr_defaults(&devname, NULL, NULL, NULL);
#endif
	if (filename == NULL && devname == NULL) {
		errmsgno(EX_BAD, "ISO-9660 image not specified\n");
		usage(EX_BAD);
	}

	if (filename != NULL)
		infile = fopen(filename, "rb");
	else
		filename = devname;

	if (infile != NULL) {
		/* EMPTY */;
#ifdef	USE_SCG
	} else if (scsidev_open(filename) < 0) {
#else
	} else {
#endif
		comerr("Cannot open '%s'\n", filename);
	}

	p = isodinfo(infile);
	if (p == NULL) {
		printf("No ISO-9660 image debug info.\n");
	} else if (strncmp(p, "MKI ", 4) == 0) {
		eol = strchr(p, '\n');
		if (eol)
			*eol = '\0';
		printf("ISO-9660 image created at %s\n", &p[4]);
		if (eol) {
			printf("\nCmdline: '%s'\n", &eol[1]);
		}
	}
	return (0);
}
