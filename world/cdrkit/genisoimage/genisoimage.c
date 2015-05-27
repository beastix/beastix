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

/*
 *
 * Patched version with stuff from the Debian's cdrtools.
 * Replaced various warnings/disclaimers with more simple ones.
 *
 * Eduard Bloch <blade@debian.org>
*/
/* @(#)mkisofs.c	1.167 06/01/30 joerg */
/* Parts from @(#)mkisofs.c	1.206 07/02/26 joerg */
/*
 * Program genisoimage.c - generate iso9660 filesystem  based upon directory
 * tree on hard disk.
 *
 * Written by Eric Youngdale (1993).
 *
 * Copyright 1993 Yggdrasil Computing, Incorporated
 * Copyright (c) 1999,2000-2004 J. Schilling
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* APPLE_HYB James Pearson j.pearson@ge.ucl.ac.uk 22/2/2000 */

#include <mconfig.h>
#include "genisoimage.h"
#include <errno.h>
#include <timedefs.h>
#include <fctldefs.h>
#include <ctype.h>
#include "match.h"
#include "exclude.h"
#include <unls.h>	/* For UNICODE translation */
#include <schily.h>
#ifdef UDF
#include "udf.h"
#endif

#ifdef	NEED_O_BINARY
#include <io.h>					/* for setmode() prototype */
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "getopt.h"
extern int      optind;
extern char     *optarg;
#endif

#ifdef VMS
#include "vms.h"
#endif

#ifdef	no_more_needed
#ifdef __NetBSD__
#include <sys/resource.h>
#endif
#endif	/* no_more_needed */

#ifdef USE_ICONV
#include <locale.h>
#include <langinfo.h>
#endif

struct directory *root = NULL;
int		path_ind;

char	version_string[] = "genisoimage " CDRKIT_VERSION;

char		*outfile;
FILE		*discimage;
unsigned int	next_extent	= 0;
unsigned int	last_extent	= 0;
unsigned int	session_start	= 0;
unsigned int	path_table_size	= 0;
unsigned int	path_table[4]	= {0, };
unsigned int	path_blocks	= 0;


unsigned int	jpath_table_size = 0;
unsigned int	jpath_table[4]	= {0, };
unsigned int	jpath_blocks	= 0;

struct iso_directory_record root_record;
struct iso_directory_record jroot_record;

char	*extension_record = NULL;
int	extension_record_extent = 0;
int	extension_record_size = 0;

/* These variables are associated with command line options */
int	check_oldnames = 0;
int	check_session = 0;
int	use_eltorito = 0;
int	hard_disk_boot = 0;
int	not_bootable = 0;
int	no_emul_boot = 0;
int	load_addr = 0;
int	load_size = 0;
int	boot_info_table = 0;
int	use_alphaboot = 0;
int	use_sparcboot = 0;
int	use_hppaboot = 0;
int	use_mipsboot = 0;
int	use_mipselboot = 0;
int	use_sunx86boot = 0;
int	use_genboot = 0;
int	use_RockRidge = 0;
int	use_XA = 0;
int	osecsize = 0;	/* Output-sector size, 0 means default secsize 2048 */
int	use_Joliet = 0;
int	jlen = JMAX;	/* maximum Joliet file name length */
int	verbose = 1;
int	debug = 0;
int	gui = 0;
int	all_files = 1;	/* New default is to include all files */
int	follow_links = 0;
#ifdef	IS_CYGWIN
int	cache_inodes = 0; /* Do not cache inodes on Cygwin by default */
#else
int	cache_inodes = 1; /* Cache inodes if OS has unique inodes */
#endif
int	rationalize = 0;
int	rationalize_uid = 0;
int	rationalize_gid = 0;
int	rationalize_filemode = 0;
int	rationalize_dirmode = 0;
uid_t	uid_to_use = 0;		/* when rationalizing uid */
gid_t	gid_to_use = 0;		/* when rationalizing gid */
int	filemode_to_use = 0;	/* if non-zero, when rationalizing file mode */
int	dirmode_to_use = 0;	/* if non-zero, when rationalizing dir mode */
int	new_dir_mode = 0555;
int	generate_tables = 0;
int	dopad = 1;	/* Now default to do padding */
int	print_size = 0;
int	split_output = 0;
char	*icharset = NULL;	/* input charset to convert to UNICODE */
char	*ocharset = NULL;	/* output charset to convert from UNICODE */
char	*preparer = PREPARER_DEFAULT;
char	*publisher = PUBLISHER_DEFAULT;
char	*appid = APPID_DEFAULT;
char	*copyright = COPYRIGHT_DEFAULT;
char	*biblio = BIBLIO_DEFAULT;
char	*abstract = ABSTRACT_DEFAULT;
char	*volset_id = VOLSET_ID_DEFAULT;
char	*volume_id = VOLUME_ID_DEFAULT;
char	*system_id = SYSTEM_ID_DEFAULT;
char	*boot_catalog = BOOT_CATALOG_DEFAULT;
char	*boot_image = BOOT_IMAGE_DEFAULT;
char	*genboot_image = BOOT_IMAGE_DEFAULT;
int	ucs_level = 3;		/* We now have Unicode tables so use level 3 */
int	volume_set_size = 1;
int	volume_sequence_number = 1;

struct eltorito_boot_entry_info *first_boot_entry = NULL;
struct eltorito_boot_entry_info *last_boot_entry = NULL;
struct eltorito_boot_entry_info *current_boot_entry = NULL;

int	use_graft_ptrs;		/* Use graft points */
int	jhide_trans_tbl;	/* Hide TRANS.TBL from Joliet tree */
int	hide_rr_moved;		/* Name RR_MOVED .rr_moved in Rock Ridge tree */
int	omit_period = 0;	/* Violates iso9660, but these are a pain */
int	transparent_compression = 0; /* So far only works with linux */
int	omit_version_number = 0; /* May violate iso9660, but noone uses vers */
int	no_rr = 0;		/* Do not use RR attributes from old session */
int	force_rr = 0;		/* Force to use RR attributes from old session */
Uint	RR_relocation_depth = 6; /* Violates iso9660, but most systems work */
int	iso9660_level = 1;
int	iso9660_namelen = LEN_ISONAME; /* 31 characters, may be set to 37 */
int	full_iso9660_filenames = 0; /* Full 31 character iso9660 filenames */
int	relaxed_filenames = 0;	/* For Amiga.  Disc will not work with DOS */
int	allow_lowercase = 0;	/* Allow lower case letters */
int	allow_multidot = 0;	/* Allow more than on dot in filename */
int	iso_translate = 1;	/* 1 == enables '#', '-' and '~' removal */
int	allow_leading_dots = 0;	/* DOS cannot read names with leading dots */
int	allow_limited_size = 0;	/* Let the user to allow the trick explicitely */
#ifdef	VMS
int	use_fileversion = 1;	/* Use file version # from filesystem */
#else
int	use_fileversion = 0;	/* Use file version # from filesystem */
#endif
int	split_SL_component = 1;	/* circumvent a bug in the SunOS driver */
int	split_SL_field = 1;	/* circumvent a bug in the SunOS */
char	*trans_tbl = "TRANS.TBL"; /* default name for translation table */
int	stream_media_size = 0;	/* # of blocks on the media */
char	*stream_filename = NULL; /* Stream file, 0 to use default STREAM.IMG */

#ifdef APPLE_HYB
int	apple_hyb = 0;		/* create HFS hybrid flag */
int	apple_ext = 0;		/* create HFS extensions flag */
int	apple_both = 0;		/* common flag (for above) */
int	hfs_extra = 0;		/* extra HFS blocks added to end of ISO vol */
int	use_mac_name = 0;	/* use Mac name for ISO/Joliet/RR flag */
hce_mem	*hce;			/* libhfs/genisoimage extras */
char	*hfs_boot_file = 0;	/* name of HFS boot file */
int	gen_pt = 0;		/* generate HFS partition table */
char	*autoname = 0;		/* AutoStart filename */
char	*magic_filename = 0;	/* name of magic file */
int	probe = 0;		/* search files for HFS/Unix type */
int	nomacfiles = 0;		/* don't look for Mac/Unix files */
int	hfs_select = 0;		/* Mac/Unix types to select */
int	create_dt = 1;		/* create the Desktp files */
int	afe_size = 0;		/* Apple File Exchange block size */
int	hfs_last = MAG_LAST;	/* process magic file after map file */
char	*deftype = APPLE_TYPE_DEFAULT;	/* default Apple TYPE */
char	*defcreator = APPLE_CREATOR_DEFAULT;	/* default Apple CREATOR */
char	*hfs_volume_id = NULL;	/* HFS volume ID */
int	icon_pos = 0;		/* Keep icon position */
char	*hfs_icharset = NULL;	/* input HFS charset name */
char    *hfs_ocharset = NULL;	/* output HFS charset name */
int	hfs_lock = 1;		/* lock HFS volume (read-only) */
char	*hfs_bless = NULL;	/* name of folder to 'bless' (System Folder) */
char	*hfs_parms = NULL;	/* low level HFS parameters */

#ifdef PREP_BOOT
char	*prep_boot_image[4];
int	use_prep_boot = 0;
int	use_chrp_boot = 0;
#endif	/* PREP_BOOT */
#endif	/* APPLE_HYB */

#ifdef UDF
int	use_udf = 0;
#endif

#ifdef DVD_VIDEO
int	dvd_video = 0;
#endif

#ifdef SORTING
int	do_sort = 0;		/* sort file data */
#endif /* SORTING */

#ifdef USE_ICONV
int   iconv_possible;
#endif

struct unls_table *in_nls = NULL;  /* input UNICODE conversion table */
struct unls_table *out_nls = NULL; /* output UNICODE conversion table */
#ifdef APPLE_HYB
struct unls_table *hfs_inls = NULL; /* input HFS UNICODE conversion table */
struct unls_table *hfs_onls = NULL; /* output HFS UNICODE conversion table */
#endif /* APPLE_HYB */

struct rcopts {
	char		*tag;
	char		**variable;
};

struct rcopts rcopt[] = {
	{"PREP", &preparer},
	{"PUBL", &publisher},
	{"APPI", &appid},
	{"COPY", &copyright},
	{"BIBL", &biblio},
	{"ABST", &abstract},
	{"VOLS", &volset_id},
	{"VOLI", &volume_id},
	{"SYSI", &system_id},
#ifdef APPLE_HYB
	{"HFS_TYPE", &deftype},
	{"HFS_CREATOR", &defcreator},
#endif	/* APPLE_HYB */
	{NULL, NULL}
};

char		*merge_warn_msg=0; /* use as pointer and boolean */

/*
 * In case it isn't obvious, the option handling code was ripped off
 * from GNU-ld.
 */
struct ld_option {
	/* The long option information.  */
	struct option	opt;
	/* The short option with the same meaning ('\0' if none).  */
	char		shortopt;
	/* The name of the argument (NULL if none).  */
	const char	*arg;
	/*
	 * The documentation string.  If this is NULL, this is a synonym for
	 * the previous option.
	 */
	const char	*doc;
	enum {
		/* Use one dash before long option name.  */
		ONE_DASH,
		/* Use two dashes before long option name.  */
		TWO_DASHES,
		/* Don't mention this option in --help output.  */
		NO_HELP
	} control;
};

/*
 * Codes used for the long options with no short synonyms. Note that all these
 * values must not be ASCII or EBCDIC.
 */
#define	OPTION_HELP			1000
#define	OPTION_QUIET			1001
#define	OPTION_NOSPLIT_SL_COMPONENT	1002
#define	OPTION_NOSPLIT_SL_FIELD		1003
#define	OPTION_PRINT_SIZE		1004
#define	OPTION_SPLIT_OUTPUT		1005
#define	OPTION_ABSTRACT			1006
#define	OPTION_BIBLIO			1007
#define	OPTION_COPYRIGHT		1008
#define	OPTION_SYSID			1009
#define	OPTION_VOLSET			1010
#define	OPTION_VOLSET_SIZE		1011
#define	OPTION_VOLSET_SEQ_NUM		1012
#define	OPTION_I_HIDE			1013
#define	OPTION_J_HIDE			1014
#define	OPTION_LOG_FILE			1015
#define	OPTION_PVERSION			1016
#define	OPTION_NOBAK			1017
#define	OPTION_SPARCLABEL		1018
#define	OPTION_HARD_DISK_BOOT		1019
#define	OPTION_NO_EMUL_BOOT		1020
#define	OPTION_NO_BOOT			1021
#define	OPTION_BOOT_LOAD_ADDR		1022
#define	OPTION_BOOT_LOAD_SIZE		1023
#define	OPTION_BOOT_INFO_TABLE		1024
#define	OPTION_HIDE_TRANS_TBL		1025
#define	OPTION_HIDE_RR_MOVED		1026
#define	OPTION_GUI			1027
#define	OPTION_TRANS_TBL		1028
#define	OPTION_P_LIST			1029
#define	OPTION_I_LIST			1030
#define	OPTION_J_LIST			1031
#define	OPTION_X_LIST			1032
#define	OPTION_NO_RR			1033
#define	OPTION_JCHARSET			1034
#define	OPTION_PAD			1035
#define	OPTION_H_HIDE			1036
#define	OPTION_H_LIST			1037
#define	OPTION_CHECK_OLDNAMES		1038

#ifdef SORTING
#define	OPTION_SORT			1039
#endif /* SORTING */
#define	OPTION_UCS_LEVEL		1040
#define	OPTION_ISO_TRANSLATE		1041
#define	OPTION_ISO_LEVEL		1042
#define	OPTION_RELAXED_FILENAMES	1043
#define	OPTION_ALLOW_LOWERCASE		1044
#define	OPTION_ALLOW_MULTIDOT		1045
#define	OPTION_USE_FILEVERSION		1046
#define	OPTION_MAX_FILENAMES		1047
#define	OPTION_ALT_BOOT			1048
#define	OPTION_USE_GRAFT		1049

#define	OPTION_INPUT_CHARSET		1050
#define	OPTION_OUTPUT_CHARSET		1051

#define	OPTION_NOPAD			1052
#define	OPTION_UID			1053
#define	OPTION_GID			1054
#define	OPTION_FILEMODE			1055
#define	OPTION_DIRMODE			1056
#define	OPTION_NEW_DIR_MODE		1057
#define	OPTION_CACHE_INODES		1058
#define	OPTION_NOCACHE_INODES		1059

#define	OPTION_CHECK_SESSION		1060
#define	OPTION_FORCE_RR			1061

#define	OPTION_DEBUG			1062

#define	OPTION_JLONG			1063

#define	OPTION_STREAM_FILE_NAME		1064
#define	OPTION_STREAM_CD_SIZE		1065

#define	OPTION_XA			1066
#define	OPTION_XA_RATIONALIZED		1067

#define	OPTION_SUNX86BOOT		1068
#define	OPTION_SUNX86LABEL		1069

#define	OPTION_ALLOW_LEADING_DOTS	1070
#define	OPTION_PUBLISHER		1071

#ifdef		JIGDO_TEMPLATE
#define	OPTION_JTT_OUTPUT		1101
#define	OPTION_JTJ_OUTPUT		1102
#define	OPTION_JT_MIN_SIZE		1103
#define	OPTION_JT_PATH_MAP		1104
#define	OPTION_JT_MD5_LIST		1105
#define	OPTION_JT_INCLUDE		1106
#define	OPTION_JT_EXCLUDE		1107
#endif

#define	OPTION_BOOTALPHA		1200

#define	OPTION_HPPA_CMDLINE 		1210
#define	OPTION_HPPA_KERNEL_32   	1211
#define	OPTION_HPPA_KERNEL_64   	1212
#define	OPTION_HPPA_BOOTLOADER  	1213
#define	OPTION_HPPA_RAMDISK     	1214

#define	OPTION_BOOTMIPS     		1220

#define	OPTION_BOOTMIPSEL   		1230

#ifdef UDF
#define	OPTION_UDF			1500
#endif
#ifdef DVD_VIDEO
#define	OPTION_DVD			1501
#endif

#ifdef APPLE_HYB
#define	OPTION_CAP			2000
#define	OPTION_NETA			2001
#define	OPTION_DBL			2002
#define	OPTION_ESH			2003
#define	OPTION_FE			2004
#define	OPTION_SGI			2005
#define	OPTION_MBIN			2006
#define	OPTION_SGL			2007
/* aliases */
#define	OPTION_USH			2008
#define	OPTION_XIN			2009

#define	OPTION_DAVE			2010
#define	OPTION_SFM			2011
#define	OPTION_XDBL			2012
#define	OPTION_XHFS			2013

#define	OPTION_PROBE			2020
#define	OPTION_MACNAME			2021
#define	OPTION_NOMACFILES		2022
#define	OPTION_BOOT_HFS_FILE		2023
#define	OPTION_MAGIC_FILE		2024

#define	OPTION_HFS_LIST			2025

#define	OPTION_GEN_PT			2026

#define	OPTION_CREATE_DT		2027
#define	OPTION_HFS_HIDE			2028

#define	OPTION_AUTOSTART		2029
#define	OPTION_BSIZE			2030
#define	OPTION_HFS_VOLID		2031
#define	OPTION_PREP_BOOT		2032
#define	OPTION_ICON_POS			2033

#define	OPTION_HFS_TYPE			2034
#define	OPTION_HFS_CREATOR		2035

#define	OPTION_ROOT_INFO		2036

#define	OPTION_HFS_INPUT_CHARSET	2037
#define	OPTION_HFS_OUTPUT_CHARSET	2038

#define	OPTION_HFS_UNLOCK		2039
#define	OPTION_HFS_BLESS		2040
#define	OPTION_HFS_PARMS		2041

#define	OPTION_CHRP_BOOT		2042

#define	OPTION_RELOC_ROOT		2043
#define	OPTION_RELOC_OLD_ROOT		2044

#define	OPTION_MAP_FILE			2045

#define	OPTION_ALLOW_LIMITED_SIZE 2046

#endif	/* APPLE_HYB */

static int	save_pname = 0;

static const struct ld_option ld_options[] =
{
	{{"nobak", no_argument, NULL, OPTION_NOBAK},
	'\0', NULL, "Do not include backup files", ONE_DASH},
	{{"no-bak", no_argument, NULL, OPTION_NOBAK},
	'\0', NULL, "Do not include backup files", ONE_DASH},
	{{"abstract", required_argument, NULL, OPTION_ABSTRACT},
	'\0', "FILE", "Set Abstract filename", ONE_DASH},
	{{"appid", required_argument, NULL, 'A'},
	'A', "ID", "Set Application ID", ONE_DASH},
	{{"biblio", required_argument, NULL, OPTION_BIBLIO},
	'\0', "FILE", "Set Bibliographic filename", ONE_DASH},
	{{"cache-inodes", no_argument, NULL, OPTION_CACHE_INODES},
	'\0', NULL, "Cache inodes (needed to detect hard links)", ONE_DASH},
	{{"no-cache-inodes", no_argument, NULL, OPTION_NOCACHE_INODES},
	'\0', NULL, "Do not cache inodes (if filesystem has no unique unides)", ONE_DASH},
	{{"check-oldnames", no_argument, NULL, OPTION_CHECK_OLDNAMES},
	'\0', NULL, "Check all imported ISO9660 names from old session", ONE_DASH},
	{{"check-session", required_argument, NULL, OPTION_CHECK_SESSION},
	'\0', "FILE", "Check all ISO9660 names from previous session", ONE_DASH},
	{{"copyright", required_argument, NULL, OPTION_COPYRIGHT},
	'\0', "FILE", "Set Copyright filename", ONE_DASH},
	{{"debug", no_argument, NULL, OPTION_DEBUG},
	'\0', NULL, "Set debug flag", ONE_DASH},
	{{"eltorito-boot", required_argument, NULL, 'b'},
	'b', "FILE", "Set El Torito boot image name", ONE_DASH},
	{{"eltorito-alt-boot", no_argument, NULL, OPTION_ALT_BOOT},
	'\0', NULL, "Start specifying alternative El Torito boot parameters", ONE_DASH},
	{{"sparc-boot", required_argument, NULL, 'B'},
	'B', "FILES", "Set sparc boot image names", ONE_DASH},
	{{"sunx86-boot", required_argument, NULL, OPTION_SUNX86BOOT},
	'\0', "FILES", "Set sunx86 boot image names", ONE_DASH},
	{{"generic-boot", required_argument, NULL, 'G'},
	'G', "FILE", "Set generic boot image name", ONE_DASH},
	{{"sparc-label", required_argument, NULL, OPTION_SPARCLABEL},
	'\0', "label text", "Set sparc boot disk label", ONE_DASH},
	{{"sunx86-label", required_argument, NULL, OPTION_SUNX86LABEL},
	'\0', "label text", "Set sunx86 boot disk label", ONE_DASH},
	{{"eltorito-catalog", required_argument, NULL, 'c'},
	'c', "FILE", "Set El Torito boot catalog name", ONE_DASH},
	{{"cdrecord-params", required_argument, NULL, 'C'},
	'C', "PARAMS", "Magic paramters from cdrecord", ONE_DASH},
	{{"omit-period", no_argument, NULL, 'd'},
	'd', NULL, "Omit trailing periods from filenames (violates ISO9660)", ONE_DASH},
	{{"dir-mode", required_argument, NULL, OPTION_DIRMODE},
	'\0', "mode", "Make the mode of all directories this mode.", ONE_DASH},
	{{"disable-deep-relocation", no_argument, NULL, 'D'},
	'D', NULL, "Disable deep directory relocation (violates ISO9660)", ONE_DASH},
	{{"file-mode", required_argument, NULL, OPTION_FILEMODE},
	'\0', "mode", "Make the mode of all plain files this mode.", ONE_DASH},
	{{"follow-links", no_argument, NULL, 'f'},
	'f', NULL, "Follow symbolic links", ONE_DASH},
	{{"gid", required_argument, NULL, OPTION_GID},
	'\0', "gid", "Make the group owner of all files this gid.",
	ONE_DASH},
	{{"graft-points", no_argument, NULL, OPTION_USE_GRAFT},
	'\0', NULL, "Allow to use graft points for filenames", ONE_DASH},
	{{"root", required_argument, NULL, OPTION_RELOC_ROOT},
	'\0', "DIR", "Set root directory for all new files and directories", ONE_DASH},
	{{"old-root", required_argument, NULL, OPTION_RELOC_OLD_ROOT},
	'\0', "DIR", "Set root directory in previous session that is searched for files", ONE_DASH},
	{{"help", no_argument, NULL, OPTION_HELP},
	'\0', NULL, "Print option help", ONE_DASH},
	{{"hide", required_argument, NULL, OPTION_I_HIDE},
	'\0', "GLOBFILE", "Hide ISO9660/RR file", ONE_DASH},
	{{"hide-list", required_argument, NULL, OPTION_I_LIST},
	'\0', "FILE", "File with list of ISO9660/RR files to hide", ONE_DASH},
	{{"hidden", required_argument, NULL, OPTION_H_HIDE},
	'\0', "GLOBFILE", "Set hidden attribute on ISO9660 file", ONE_DASH},
	{{"hidden-list", required_argument, NULL, OPTION_H_LIST},
	'\0', "FILE", "File with list of ISO9660 files with hidden attribute", ONE_DASH},
	{{"hide-joliet", required_argument, NULL, OPTION_J_HIDE},
	'\0', "GLOBFILE", "Hide Joliet file", ONE_DASH},
	{{"hide-joliet-list", required_argument, NULL, OPTION_J_LIST},
	'\0', "FILE", "File with list of Joliet files to hide", ONE_DASH},
	{{"hide-joliet-trans-tbl", no_argument, NULL, OPTION_HIDE_TRANS_TBL},
	'\0', NULL, "Hide TRANS.TBL from Joliet tree", ONE_DASH},
	{{"hide-rr-moved", no_argument, NULL, OPTION_HIDE_RR_MOVED},
	'\0', NULL, "Rename RR_MOVED to .rr_moved in Rock Ridge tree", ONE_DASH},
	{{"gui", no_argument, NULL, OPTION_GUI},
	'\0', NULL, "Switch behaviour for GUI", ONE_DASH},
	{{NULL, required_argument, NULL, 'i'},
	'i', "ADD_FILES", "No longer supported", TWO_DASHES},
	{{"input-charset", required_argument, NULL, OPTION_INPUT_CHARSET},
	'\0', "CHARSET", "Local input charset for file name conversion", ONE_DASH},
	{{"output-charset", required_argument, NULL, OPTION_OUTPUT_CHARSET},
	'\0', "CHARSET", "Output charset for file name conversion", ONE_DASH},
	{{"iso-level", required_argument, NULL, OPTION_ISO_LEVEL},
	'\0', "LEVEL", "Set ISO9660 conformance level (1..3) or 4 for ISO9660 version 2", ONE_DASH},
	{{"joliet", no_argument, NULL, 'J'},
	'J', NULL, "Generate Joliet directory information", ONE_DASH},
	{{"joliet-long", no_argument, NULL, OPTION_JLONG},
	'\0', NULL, "Allow Joliet file names to be 103 Unicode characters", ONE_DASH},
	{{"jcharset", required_argument, NULL, OPTION_JCHARSET},
	'\0', "CHARSET", "Local charset for Joliet directory information", ONE_DASH},
	{{"full-iso9660-filenames", no_argument, NULL, 'l'},
	'l', NULL, "Allow full 31 character filenames for ISO9660 names", ONE_DASH},
	{{"max-iso9660-filenames", no_argument, NULL, OPTION_MAX_FILENAMES},
	'\0', NULL, "Allow 37 character filenames for ISO9660 names (violates ISO9660)", ONE_DASH},

	{{"allow-limited-size", no_argument, NULL, OPTION_ALLOW_LIMITED_SIZE},
	'\0', NULL, "Allow different file sizes in ISO9660/UDF on large files", ONE_DASH},

	{{"allow-leading-dots", no_argument, NULL, OPTION_ALLOW_LEADING_DOTS},
	'\0', NULL, "Allow ISO9660 filenames to start with '.' (violates ISO9660)", ONE_DASH},
	{{"ldots", no_argument, NULL, OPTION_ALLOW_LEADING_DOTS},
	'\0', NULL, "Allow ISO9660 filenames to start with '.' (violates ISO9660)", ONE_DASH},
	{{"allow-leading-dots", no_argument, NULL, 'L'},
	'L', NULL, "Allow ISO9660 filenames to start with '.' (violates ISO9660)", ONE_DASH},

	{{"log-file", required_argument, NULL, OPTION_LOG_FILE},
	'\0', "LOG_FILE", "Re-direct messages to LOG_FILE", ONE_DASH},
	{{"exclude", required_argument, NULL, 'm'},
	'm', "GLOBFILE", "Exclude file name", ONE_DASH},
	{{"exclude-list", required_argument, NULL, OPTION_X_LIST},
	'\0', "FILE", "File with list of file names to exclude", ONE_DASH},
	{{"pad", no_argument, NULL, OPTION_PAD},
	0, NULL, "Pad output to a multiple of 32k (default)", ONE_DASH},
	{{"no-pad", no_argument, NULL, OPTION_NOPAD},
	0, NULL, "Do not pad output to a multiple of 32k", ONE_DASH},
	{{"prev-session", required_argument, NULL, 'M'},
	'M', "FILE", "Set path to previous session to merge", ONE_DASH},
	{{"dev", required_argument, NULL, 'M'},
	'\0', "SCSIdev", "Set path to previous session to merge", ONE_DASH},
	{{"omit-version-number", no_argument, NULL, 'N'},
	'N', NULL, "Omit version number from ISO9660 filename (violates ISO9660)", ONE_DASH},
	{{"new-dir-mode", required_argument, NULL, OPTION_NEW_DIR_MODE},
	'\0', "mode", "Mode used when creating new directories.", ONE_DASH},
	{{"force-rr", no_argument, NULL, OPTION_FORCE_RR},
	0, NULL, "Inhibit automatic Rock Ridge detection for previous session", ONE_DASH},
	{{"no-rr", no_argument, NULL, OPTION_NO_RR},
	0, NULL, "Inhibit reading of Rock Ridge attributes from previous session", ONE_DASH},
	{{"no-split-symlink-components", no_argument, NULL, OPTION_NOSPLIT_SL_COMPONENT},
	0, NULL, "Inhibit splitting symlink components", ONE_DASH},
	{{"no-split-symlink-fields", no_argument, NULL, OPTION_NOSPLIT_SL_FIELD},
	0, NULL, "Inhibit splitting symlink fields", ONE_DASH},
	{{"output", required_argument, NULL, 'o'},
	'o', "FILE", "Set output file name", ONE_DASH},
	{{"path-list", required_argument, NULL, OPTION_P_LIST},
	'\0', "FILE", "File with list of pathnames to process", ONE_DASH},
	{{"preparer", required_argument, NULL, 'p'},
	'p', "PREP", "Set Volume preparer", ONE_DASH},
	{{"print-size", no_argument, NULL, OPTION_PRINT_SIZE},
	'\0', NULL, "Print estimated filesystem size and exit", ONE_DASH},
	{{"publisher", required_argument, NULL, OPTION_PUBLISHER},
	'\0', "PUB", "Set Volume publisher", ONE_DASH},
	{{"publisher", required_argument, NULL, 'P'},
	'P', "PUB", "Set Volume publisher", ONE_DASH},
	{{"quiet", no_argument, NULL, OPTION_QUIET},
	'\0', NULL, "Run quietly", ONE_DASH},
	{{"rational-rock", no_argument, NULL, 'r'},
	'r', NULL, "Generate rationalized Rock Ridge directory information", ONE_DASH},
	{{"rock", no_argument, NULL, 'R'},
	'R', NULL, "Generate Rock Ridge directory information", ONE_DASH},
	{{"sectype", required_argument, NULL, 's'},
	's', "TYPE", "Set output sector type to e.g. data/xa1/raw", ONE_DASH},

	{{"alpha-boot", required_argument, NULL, OPTION_BOOTALPHA},
	'\0', "FILE", "Set alpha boot image name (relative to image root)", ONE_DASH},

	{{"hppa-cmdline", required_argument, NULL, OPTION_HPPA_CMDLINE},
	'\0', "CMDLINE", "Set hppa boot command line (relative to image root)", ONE_DASH},
	{{"hppa-kernel-32", required_argument, NULL, OPTION_HPPA_KERNEL_32},
	'\0', "FILE", "Set hppa 32-bit image name (relative to image root)", ONE_DASH},
	{{"hppa-kernel-64", required_argument, NULL, OPTION_HPPA_KERNEL_64},
	'\0', "FILE", "Set hppa 64-bit image name (relative to image root)", ONE_DASH},
	{{"hppa-bootloader", required_argument, NULL, OPTION_HPPA_BOOTLOADER},
	'\0', "FILE", "Set hppa boot loader file name (relative to image root)", ONE_DASH},
	{{"hppa-ramdisk", required_argument, NULL, OPTION_HPPA_RAMDISK},
	'\0', "FILE", "Set hppa ramdisk file name (relative to image root)", ONE_DASH},

	{{"mips-boot", required_argument, NULL, OPTION_BOOTMIPS},
	'\0', "FILE", "Set mips boot image name (relative to image root)", ONE_DASH},

	{{"mipsel-boot", required_argument, NULL, OPTION_BOOTMIPSEL},
	'\0', "FILE", "Set mipsel boot image name (relative to image root)", ONE_DASH},

#ifdef JIGDO_TEMPLATE
	{{"jigdo-jigdo", required_argument, NULL, OPTION_JTJ_OUTPUT},
	'\0', "FILE", "Produce a jigdo .jigdo file as well as the .iso", ONE_DASH },
	{{"jigdo-template", required_argument, NULL, OPTION_JTT_OUTPUT},
	'\0', "FILE", "Produce a jigdo .template file as well as the .iso", ONE_DASH },
	{{"jigdo-min-file-size", required_argument, NULL, OPTION_JT_MIN_SIZE},
	'\0', "SIZE", "Minimum size for a file to be listed in the jigdo file", ONE_DASH },
	{{"jigdo-force-md5", required_argument, NULL, OPTION_JT_INCLUDE},
	'\0', "PATTERN", "Pattern(s) where files MUST match an externally-supplied MD5sum", ONE_DASH },
	{{"jigdo-exclude", required_argument, NULL, OPTION_JT_EXCLUDE},
	'\0', "PATTERN", "Pattern(s) to exclude from the jigdo file", ONE_DASH },
	{{"jigdo-map", required_argument, NULL, OPTION_JT_PATH_MAP},
	'\0', "PATTERN1=PATTERN2", "Pattern(s) to map paths (e.g. Debian=/mirror/debian)", ONE_DASH },
	{{"md5-list", required_argument, NULL, OPTION_JT_MD5_LIST},
	'\0', "FILE", "File containing MD5 sums of the files that should be checked", ONE_DASH },
#endif

#ifdef SORTING
	{ {"sort", required_argument, NULL, OPTION_SORT},
	'\0', "FILE", "Sort file content locations according to rules in FILE", ONE_DASH },
#endif /* SORTING */

	{{"split-output", no_argument, NULL, OPTION_SPLIT_OUTPUT},
	'\0', NULL, "Split output into files of approx. 1GB size", ONE_DASH},
	{{"stream-file-name", required_argument, NULL, OPTION_STREAM_FILE_NAME},
	'\0', "FILE_NAME", "Set the stream file ISO9660 name (incl. version)", ONE_DASH},
	{{"stream-media-size", required_argument, NULL, OPTION_STREAM_CD_SIZE},
	'\0', "#", "Set the size of your CD media in sectors", ONE_DASH},
	{{"sysid", required_argument, NULL, OPTION_SYSID},
	'\0', "ID", "Set System ID", ONE_DASH},
	{{"translation-table", no_argument, NULL, 'T'},
	'T', NULL, "Generate translation tables for systems that don't understand long filenames", ONE_DASH},
	{{"table-name", required_argument, NULL, OPTION_TRANS_TBL},
	'\0', "TABLE_NAME", "Translation table file name", ONE_DASH},
	{{"ucs-level", required_argument, NULL, OPTION_UCS_LEVEL},
	'\0', "LEVEL", "Set Joliet UCS level (1..3)", ONE_DASH},

#ifdef UDF
	{{"udf", no_argument, NULL, OPTION_UDF},
	'\0', NULL, "Generate UDF file system", ONE_DASH},
#endif

#ifdef DVD_VIDEO
	{{"dvd-video", no_argument, NULL, OPTION_DVD},
	'\0', NULL, "Generate DVD-Video compliant UDF file system", ONE_DASH},
#endif

	{{"uid", required_argument, NULL, OPTION_UID},
	'\0', "uid", "Make the owner of all files this uid.",
	ONE_DASH},
	{{"untranslated-filenames", no_argument, NULL, 'U'},
	/* CSTYLED */
	'U', NULL, "Allow Untranslated filenames (for HPUX & AIX - violates ISO9660). Forces -l, -d, -N, -allow-leading-dots, -relaxed-filenames, -allow-lowercase, -allow-multidot", ONE_DASH},
	{{"relaxed-filenames", no_argument, NULL, OPTION_RELAXED_FILENAMES},
	'\0', NULL, "Allow 7 bit ASCII except lower case characters (violates ISO9660)", ONE_DASH},
	{{"no-iso-translate", no_argument, NULL, OPTION_ISO_TRANSLATE},
	'\0', NULL, "Do not translate illegal ISO characters '~', '-' and '#' (violates ISO9660)", ONE_DASH},
	{{"allow-lowercase", no_argument, NULL, OPTION_ALLOW_LOWERCASE},
	'\0', NULL, "Allow lower case characters in addition to the current character set (violates ISO9660)", ONE_DASH},
	{{"allow-multidot", no_argument, NULL, OPTION_ALLOW_MULTIDOT},
	'\0', NULL, "Allow more than one dot in filenames (e.g. .tar.gz) (violates ISO9660)", ONE_DASH},
	{{"use-fileversion", no_argument, NULL, OPTION_USE_FILEVERSION},
	'\0', "LEVEL", "Use file version # from filesystem", ONE_DASH},
	{{"verbose", no_argument, NULL, 'v'},
	'v', NULL, "Verbose", ONE_DASH},
	{{"version", no_argument, NULL, OPTION_PVERSION},
	'\0', NULL, "Print the current version", ONE_DASH},
	{{"volid", required_argument, NULL, 'V'},
	'V', "ID", "Set Volume ID", ONE_DASH},
	{{"volset", required_argument, NULL, OPTION_VOLSET},
	'\0', "ID", "Set Volume set ID", ONE_DASH},
	{{"volset-size", required_argument, NULL, OPTION_VOLSET_SIZE},
	'\0', "#", "Set Volume set size", ONE_DASH},
	{{"volset-seqno", required_argument, NULL, OPTION_VOLSET_SEQ_NUM},
	'\0', "#", "Set Volume set sequence number", ONE_DASH},
	{{"old-exclude", required_argument, NULL, 'x'},
	'x', "FILE", "Exclude file name(depreciated)", ONE_DASH},
	{{"hard-disk-boot", no_argument, NULL, OPTION_HARD_DISK_BOOT},
	'\0', NULL, "Boot image is a hard disk image", ONE_DASH},
	{{"no-emul-boot", no_argument, NULL, OPTION_NO_EMUL_BOOT},
	'\0', NULL, "Boot image is 'no emulation' image", ONE_DASH},
	{{"no-boot", no_argument, NULL, OPTION_NO_BOOT},
	'\0', NULL, "Boot image is not bootable", ONE_DASH},
	{{"boot-load-seg", required_argument, NULL, OPTION_BOOT_LOAD_ADDR},
	'\0', "#", "Set load segment for boot image", ONE_DASH},
	{{"boot-load-size", required_argument, NULL, OPTION_BOOT_LOAD_SIZE},
	'\0', "#", "Set numbers of load sectors", ONE_DASH},
	{{"boot-info-table", no_argument, NULL, OPTION_BOOT_INFO_TABLE},
	'\0', NULL, "Patch boot image with info table", ONE_DASH},
	{{"XA", no_argument, NULL, OPTION_XA},
	'\0', NULL, "Generate XA directory attruibutes", ONE_DASH},
	{{"xa", no_argument, NULL, OPTION_XA_RATIONALIZED},
	'\0', NULL, "Generate rationalized XA directory attruibutes", ONE_DASH},
	{{"transparent-compression", no_argument, NULL, 'z'},
	'z', NULL, "Enable transparent compression of files", ONE_DASH},

#ifdef APPLE_HYB
	{{"hfs-type", required_argument, NULL, OPTION_HFS_TYPE},
	'\0', "TYPE", "Set HFS default TYPE", ONE_DASH},
	{{"hfs-creator", required_argument, NULL, OPTION_HFS_CREATOR},
	'\0', "CREATOR", "Set HFS default CREATOR", ONE_DASH},
	{{"apple", no_argument, NULL, 'g'},
	'g', NULL, "Add Apple ISO9660 extensions", ONE_DASH},
	{{"hfs", no_argument, NULL, 'h'},
	'h', NULL, "Create ISO9660/HFS hybrid", ONE_DASH},
	{{"map", required_argument, NULL, OPTION_MAP_FILE},
	'\0', "MAPPING_FILE", "Map file extensions to HFS TYPE/CREATOR", ONE_DASH},
	{{"map", required_argument, NULL, 'H'},
	'H', "MAPPING_FILE", "Map file extensions to HFS TYPE/CREATOR", ONE_DASH},
	{{"magic", required_argument, NULL, OPTION_MAGIC_FILE},
	'\0', "FILE", "Magic file for HFS TYPE/CREATOR", ONE_DASH},
	{{"probe", no_argument, NULL, OPTION_PROBE},
	'\0', NULL, "Probe all files for Apple/Unix file types", ONE_DASH},
	{{"mac-name", no_argument, NULL, OPTION_MACNAME},
	'\0', NULL, "Use Macintosh name for ISO9660/Joliet/RockRidge file name",
	ONE_DASH},
	{{"no-mac-files", no_argument, NULL, OPTION_NOMACFILES},
	'\0', NULL, "Do not look for Unix/Mac files (depreciated)", ONE_DASH},
	{{"boot-hfs-file", required_argument, NULL, OPTION_BOOT_HFS_FILE},
	'\0', "FILE", "Set HFS boot image name", ONE_DASH},
	{{"part", no_argument, NULL, OPTION_GEN_PT},
	'\0', NULL, "Generate HFS partition table", ONE_DASH},
	{{"cluster-size", required_argument, NULL, OPTION_BSIZE},
	'\0', "SIZE", "Cluster size for PC Exchange Macintosh files", ONE_DASH},
	{{"auto", required_argument, NULL, OPTION_AUTOSTART},
	'\0', "FILE", "Set HFS AutoStart file name", ONE_DASH},
	{{"no-desktop", no_argument, NULL, OPTION_CREATE_DT},
	'\0', NULL, "Do not create the HFS (empty) Desktop files", ONE_DASH},
	{{"hide-hfs", required_argument, NULL, OPTION_HFS_HIDE},
	'\0', "GLOBFILE", "Hide HFS file", ONE_DASH},
	{{"hide-hfs-list", required_argument, NULL, OPTION_HFS_LIST},
	'\0', "FILE", "List of HFS files to hide", ONE_DASH},
	{{"hfs-volid", required_argument, NULL, OPTION_HFS_VOLID},
	'\0', "HFS_VOLID", "Volume name for the HFS partition", ONE_DASH},
	{{"icon-position", no_argument, NULL, OPTION_ICON_POS},
	'\0', NULL, "Keep HFS icon position", ONE_DASH},
	{{"root-info", required_argument, NULL, OPTION_ROOT_INFO},
	'\0', "FILE", "finderinfo for root folder", ONE_DASH},
	{{"input-hfs-charset", required_argument, NULL, OPTION_HFS_INPUT_CHARSET},
	'\0', "CHARSET", "Local input charset for HFS file name conversion", ONE_DASH},
	{{"output-hfs-charset", required_argument, NULL, OPTION_HFS_OUTPUT_CHARSET},
	'\0', "CHARSET", "Output charset for HFS file name conversion", ONE_DASH},
	{{"hfs-unlock", no_argument, NULL, OPTION_HFS_UNLOCK},
	'\0', NULL, "Leave HFS Volume unlocked", ONE_DASH},
	{{"hfs-bless", required_argument, NULL, OPTION_HFS_BLESS},
	'\0', "FOLDER_NAME", "Name of Folder to be blessed", ONE_DASH},
	{{"hfs-parms", required_argument, NULL, OPTION_HFS_PARMS},
	'\0', "PARAMETERS", "Comma separated list of HFS parameters", ONE_DASH},
#ifdef PREP_BOOT
	{{"prep-boot", required_argument, NULL, OPTION_PREP_BOOT},
	'\0', "FILE", "PReP boot image file -- up to 4 are allowed", ONE_DASH},
	{{"chrp-boot", no_argument, NULL, OPTION_CHRP_BOOT},
	'\0', NULL, "Add CHRP boot header", ONE_DASH},
#endif	/* PREP_BOOT */
	{{"cap", no_argument, NULL, OPTION_CAP},
	'\0', NULL, "Look for AUFS CAP Macintosh files", TWO_DASHES},
	{{"netatalk", no_argument, NULL, OPTION_NETA},
	'\0', NULL, "Look for NETATALK Macintosh files", TWO_DASHES},
	{{"double", no_argument, NULL, OPTION_DBL},
	'\0', NULL, "Look for AppleDouble Macintosh files", TWO_DASHES},
	{{"ethershare", no_argument, NULL, OPTION_ESH},
	'\0', NULL, "Look for Helios EtherShare Macintosh files", TWO_DASHES},
	{{"exchange", no_argument, NULL, OPTION_FE},
	'\0', NULL, "Look for PC Exchange Macintosh files", TWO_DASHES},
	{{"sgi", no_argument, NULL, OPTION_SGI},
	'\0', NULL, "Look for SGI Macintosh files", TWO_DASHES},
	{{"macbin", no_argument, NULL, OPTION_MBIN},
	'\0', NULL, "Look for MacBinary Macintosh files", TWO_DASHES},
	{{"single", no_argument, NULL, OPTION_SGL},
	'\0', NULL, "Look for AppleSingle Macintosh files", TWO_DASHES},
	{{"ushare", no_argument, NULL, OPTION_USH},
	'\0', NULL, "Look for IPT UShare Macintosh files", TWO_DASHES},
	{{"xinet", no_argument, NULL, OPTION_XIN},
	'\0', NULL, "Look for XINET Macintosh files", TWO_DASHES},
	{{"dave", no_argument, NULL, OPTION_DAVE},
	'\0', NULL, "Look for DAVE Macintosh files", TWO_DASHES},
	{{"sfm", no_argument, NULL, OPTION_SFM},
	'\0', NULL, "Look for SFM Macintosh files", TWO_DASHES},
	{{"osx-double", no_argument, NULL, OPTION_XDBL},
	'\0', NULL, "Look for MacOS X AppleDouble Macintosh files", TWO_DASHES},
	{{"osx-hfs", no_argument, NULL, OPTION_XHFS},
	'\0', NULL, "Look for MacOS X HFS Macintosh files", TWO_DASHES},
#endif	/* APPLE_HYB */
};

#define	OPTION_COUNT (sizeof ld_options / sizeof (ld_options[0]))

static	void	read_rcfile(char *appname);
static	void	susage(int excode);
static	void	usage(int excode);
int	iso9660_date(char *result, time_t crtime);
static	void	hide_reloc_dir(void);
static	char *get_pnames(int argc, char **argv, int opt, char *pname, 
								  int pnsize, FILE *fp);
char *findgequal(char *s);
static	char *escstrcpy(char *to, char *from);
void *e_malloc(size_t size);

static int
read_one_rcfile(char *filename)
{
	int linum = 0;
	char linebuffer[256];
	FILE *fp;

	if (!filename)
		return 0;

	fp = fopen(filename, "r");
	if (!fp) {
		if (errno == ENOENT)
			return 0;
#ifdef	USE_LIBSCHILY
		errmsg("Cannot open '%s'.\n", filename);
#else
		perror(filename);
#endif
		return 0;
	}
	if (verbose > 0)
		fprintf(stderr, "Using \"%s\"\n", filename);

	while (fgets(linebuffer, sizeof(linebuffer), fp)) {
		char *name, *p, *p1;
		struct rcopts *rco;

		++linum;
		/* skip any leading white space */
		for (p = linebuffer; *p == ' ' || *p == '\t'; p++)
			;
		/* Skip comments and blank lines */
		if (!*p || *p == '\n' || *p == '\r' || *p == '#')
			continue;
		/*
		 * The name should begin in the left margin.  Make sure it is
		 * in upper case.  Stop when we see white space or a comment.
		 */
		name = p;
		while (*p && (isalpha((unsigned char) *p) || *p == '_'))
			*p++ = toupper((unsigned char) *p);

		if (name == p) {
			fprintf(stderr, "%s:%d: name required\n", filename,
					linum);
			continue;
		}

		p1 = p;
		/* Skip past white space after the name */
		while (*p == ' ' || *p == '\t')
			p++;
		/* silently ignore errors in the rc file. */
		if (*p != '=') {
			fprintf(stderr, "%s:%d: equals sign required after '%.*s'\n",
						filename, linum,
						(int)(p1-name), name);
			continue;
		}
		*p1 = 0;

		/* Skip pas the = sign, and any white space following it */
		p++;
		while (*p == ' ' || *p == '\t')
			p++;

		/* Get rid of trailing newline */
		for (p1 = p; *p1 && *p1 != '\n' && *p1 != '\r'; p1++)
			;
		*p1 = 0;

		/* Figure out which option we have */
		for (rco = rcopt; rco->tag; rco++)
			if (strcmp(rco->tag, name) == 0) {
				/* memleak if we ever do this more than once */
				*rco->variable = strdup(p);
				break;
			}

		if (!rco->tag) {
			fprintf(stderr, "%s:%d: field name '%s' unknown\n",
				filename, linum,
				name);
		}
	}
	if (ferror(fp)) {
#ifdef	USE_LIBSCHILY
		errmsg("Read error on '%s'.\n", filename);
#else
		perror(filename);
#endif
		fclose(fp);
		return 0;
	}
	fclose(fp);
	return 1;
}

#define ETCDIR "/etc"
#define RCFILENAME "genisoimagerc"
#define OLD_RCFILENAME "mkisofsrc"

static void
read_rcfile(char *appname)
{
	char *p;
	char filename[1000];

	if (read_one_rcfile(getenv("GENISOIMAGERC")))
		return;
	if (read_one_rcfile(getenv("MKISOFSRC")))
		return;
	if (read_one_rcfile("." RCFILENAME))
		return;
	if (read_one_rcfile("." OLD_RCFILENAME))
		return;

	p = getenv("HOME");
	if (p && strlen(p) + 1 + sizeof(RCFILENAME) < sizeof(filename)) {
		strcpy(filename, p);
		p = filename + strlen(filename);
		*p++ = PATH_SEPARATOR;
		strcpy(p, "." RCFILENAME);
		if (read_one_rcfile(filename))
			return;
		strcpy(p, "." OLD_RCFILENAME);
		if (read_one_rcfile(filename))
			return;
	}

	if (read_one_rcfile(ETCDIR SPATH_SEPARATOR RCFILENAME))
		return;

	if (appname &&
	    strlen(appname) + 1 + sizeof(RCFILENAME) < sizeof(filename)) {
		strcpy(filename, appname);
		p = strrchr(filename, PATH_SEPARATOR);
		if (p) {
			strcpy(p + 1, RCFILENAME);
			if (read_one_rcfile(filename))
				return;
		}
	}
}

char	*path_table_l = NULL;
char	*path_table_m = NULL;

char	*jpath_table_l = NULL;
char	*jpath_table_m = NULL;

int	goof = 0;

#ifndef TRUE
#define	TRUE 1
#endif

#ifndef FALSE
#define	FALSE 0
#endif

static void
susage(int excode)
{
	const char	*program_name = "genisoimage";

	fprintf(stderr, "Usage: %s [options] -o file directory ...\n", program_name);
	fprintf(stderr, "\nUse %s -help\n", program_name);
	fprintf(stderr, "to get a list of valid options.\n");
	fprintf(stderr, "\nReport problems to debburn-devel@lists.alioth.debian.org.\n");

	exit(excode);
}

static void
usage(int excode)
{
	const char	*program_name = "genisoimage";

#if 0
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,
		"genisoimage [-o outfile] [-R] [-V volid] [-v] [-a] \
[-T]\n [-l] [-d] [-V] [-D] [-L] [-p preparer]"
		"[-P publisher] [ -A app_id ] [-z] \n \
[-b boot_image_name] [-c boot_catalog-name] \
[-x path -x path ...] path\n");
#endif

	int	i;

/*	const char **targets, **pp;*/

	fprintf(stderr, "Usage: %s [options] file...\n", program_name);

	fprintf(stderr, "Options:\n");
	for (i = 0; i < (int)OPTION_COUNT; i++) {
		if (ld_options[i].doc != NULL) {
			int	comma;
			int	len;
			int	j;

			fprintf(stderr, "  ");

			comma = FALSE;
			len = 2;

			j = i;
			do {
				if (ld_options[j].shortopt != '\0' &&
					ld_options[j].control != NO_HELP) {
					fprintf(stderr, "%s-%c",
						comma ? ", " : "",
						ld_options[j].shortopt);
					len += (comma ? 2 : 0) + 2;
					if (ld_options[j].arg != NULL) {
						if (ld_options[j].opt.has_arg != optional_argument) {
							fprintf(stderr, " ");
							++len;
						}
						fprintf(stderr, "%s",
							ld_options[j].arg);
						len += strlen(ld_options[j].arg);
					}
					comma = TRUE;
				}
				++j;
			}
			while (j < (int)OPTION_COUNT && ld_options[j].doc == NULL);

			j = i;
			do {
				if (ld_options[j].opt.name != NULL &&
					ld_options[j].control != NO_HELP) {
					fprintf(stderr, "%s-%s%s",
						comma ? ", " : "",
						ld_options[j].control == TWO_DASHES ? "-" : "",
						ld_options[j].opt.name);
					len += ((comma ? 2 : 0)
						+ 1
						+ (ld_options[j].control == TWO_DASHES ? 1 : 0)
						+ strlen(ld_options[j].opt.name));
					if (ld_options[j].arg != NULL) {
						fprintf(stderr, " %s",
							ld_options[j].arg);
						len += 1 +
						    strlen(ld_options[j].arg);
					}
					comma = TRUE;
				}
				++j;
			}
			while (j < (int)OPTION_COUNT && ld_options[j].doc == NULL);

			if (len >= 30) {
				fprintf(stderr, "\n");
				len = 0;
			}
			for (; len < 30; len++)
				fputc(' ', stderr);

			fprintf(stderr, "%s\n", ld_options[i].doc);
		}
	}
	fprintf(stderr, 
        "\nReport problems to debburn-devel@lists.alioth.debian.org.\n");
	exit(excode);
}


/*
 * Fill in date in the iso9660 format
 *
 * The standards  state that the timezone offset is in multiples of 15
 * minutes, and is what you add to GMT to get the localtime.  The U.S.
 * is always at a negative offset, from -5h to -8h (can vary a little
 * with DST,  I guess).  The Linux iso9660 filesystem has had the sign
 * of this wrong for ages (genisoimage had it wrong too for the longest time).
 */
int
iso9660_date(char *result, time_t crtime)
{
	struct tm	*local;

	local = localtime(&crtime);
	result[0] = local->tm_year;
	result[1] = local->tm_mon + 1;
	result[2] = local->tm_mday;
	result[3] = local->tm_hour;
	result[4] = local->tm_min;
	result[5] = local->tm_sec;

	/*
	 * Must recalculate proper timezone offset each time, as some files use
	 * daylight savings time and some don't...
	 */
	result[6] = local->tm_yday;	/* save yday 'cause gmtime zaps it */
	local = gmtime(&crtime);
	local->tm_year -= result[0];
	local->tm_yday -= result[6];
	local->tm_hour -= result[3];
	local->tm_min -= result[4];
	if (local->tm_year < 0) {
		local->tm_yday = -1;
	} else {
		if (local->tm_year > 0)
			local->tm_yday = 1;
	}

	result[6] = -(local->tm_min + 60 *
			(local->tm_hour + 24 * local->tm_yday)) / 15;

	return (0);
}

/* hide "./rr_moved" if all its contents are hidden */
static void
hide_reloc_dir()
{
	struct directory_entry *s_entry;

	for (s_entry = reloc_dir->contents; s_entry; s_entry = s_entry->next) {
		if (strcmp(s_entry->name, ".") == 0 ||
				strcmp(s_entry->name, "..") == 0)
			continue;

		if ((s_entry->de_flags & INHIBIT_ISO9660_ENTRY) == 0)
			return;
	}

	/* all entries are hidden, so hide this directory */
	reloc_dir->dir_flags |= INHIBIT_ISO9660_ENTRY;
	reloc_dir->self->de_flags |= INHIBIT_ISO9660_ENTRY;
}

/*
 * get pathnames from the command line, and then from given file
 */
static char *
get_pnames(int argc, char **argv, int opt, char *pname, int pnsize, FILE *fp)
{
	int	len;

	/* we may of already read the first line from the pathnames file */
	if (save_pname) {
		save_pname = 0;
		return (pname);
	}

	if (opt < argc)
		return (argv[opt]);

	if (fp == NULL)
		return ((char *) 0);

	if (fgets(pname, pnsize, fp)) {
		/* Discard newline */
		len = strlen(pname);
		if (pname[len - 1] == '\n') {
			pname[len - 1] = '\0';
		}
		return (pname);
	}
	return ((char *) 0);
}

extern char	*cdrecord_data;

int main(int argc, char *argv[])
{
	struct directory_entry de;

#ifdef HAVE_SBRK
	unsigned long	mem_start;

#endif
	struct stat	statbuf;
	char		*merge_image = NULL;
	char		*reloc_root = NULL;
	char		*reloc_old_root = NULL;
	struct iso_directory_record *mrootp = NULL;
	struct output_fragment *opnt;
	int		longind;
	char		shortopts[OPTION_COUNT * 3 + 2];
	struct option	longopts[OPTION_COUNT + 1];
	int		c;
	int		n;
	char		*log_file = 0;
	char		*node = NULL;
	char		*pathnames = 0;
	FILE		*pfp = NULL;
	char		pname[2*PATH_MAX + 1 + 1];	/* may be too short */
	char		*arg;				/* if '\\' present  */
	char		nodename[PATH_MAX + 1];
	int		no_path_names = 1;
	int		warn_violate = 0;
	int		have_cmd_line_pathspec = 0;
	int		rationalize_all = 0;
	char		*mkisofs_call = 0; /* use as pointer and boolean */

#ifdef APPLE_HYB
	char		*afpfile = "";	/* mapping file for TYPE/CREATOR */
	int		hfs_ct = 0;
	char		*root_info = 0;
#endif	/* APPLE_HYB */

  /* abusing arg */
  mkisofs_call=strstr(argv[0], "mkisofs");
  if(mkisofs_call && '\0' == mkisofs_call[7]) /* lame cheater detected */
     argv[0]="genisoimage";

#ifdef __EMX__
	/* This gives wildcard expansion with Non-Posix shells with EMX */
	_wildcard(&argc, &argv);
#endif
	save_args(argc, argv);

	if (argc < 2) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD, "Missing pathspec.\n");
#endif
		susage(1);
	}
	/* Get the defaults from the .genisoimagerc file */
	read_rcfile(argv[0]);

	outfile = NULL;

	/*
	 * Copy long option initialization from GNU-ld.
	 */
	/*
	 * Starting the short option string with '-' is for programs that
	 * expect options and other ARGV-elements in any order and that care
	 * about the ordering of the two.  We describe each non-option
	 * ARGV-element as if it were the argument of an option with
	 * character code 1.
	 */
	{
		int		i,
				is,
				il;

		shortopts[0] = '-';
		is = 1;
		il = 0;
		for (i = 0; i < (int)OPTION_COUNT; i++) {
			if (ld_options[i].shortopt != '\0') {
				shortopts[is] = ld_options[i].shortopt;
				++is;
				if (ld_options[i].opt.has_arg ==
					required_argument ||
					ld_options[i].opt.has_arg ==
							optional_argument) {
					shortopts[is] = ':';
					++is;
					if (ld_options[i].opt.has_arg ==
							optional_argument) {
						shortopts[is] = ':';
						++is;
					}
				}
			}
			if (ld_options[i].opt.name != NULL) {
				longopts[il] = ld_options[i].opt;
				++il;
			}
		}
		shortopts[is] = '\0';
		longopts[il].name = NULL;
	}

	while ((c = getopt_long_only(argc, argv, shortopts,
						longopts, &longind)) != EOF)
		switch (c) {
		case 1:
			/* A filename that we take as input. */
			optind--;
			have_cmd_line_pathspec = 1;
			goto parse_input_files;

		case OPTION_USE_GRAFT:
			use_graft_ptrs = 1;
			break;
		case 'C':
			/*
			 * This is a temporary hack until cdrecord gets the
			 * proper hooks in it.
			 */
			cdrecord_data = optarg;
			break;
		case OPTION_GUI:
			gui++;
			break;
		case 'i':
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD, "-i option no longer supported.\n");
#else
			fprintf(stderr, "-i option no longer supported.\n");
			exit(1);
#endif
			break;
		case OPTION_ISO_LEVEL:
			iso9660_level = atoi(optarg);

			switch (iso9660_level) {

			case 1:
				/*
				 * Only on file section
				 * 8.3 d or d1 characters for files
				 * 8   d or d1 characters for directories
				 */
				break;
			case 2:
				/*
				 * Only on file section
				 */
				break;
			case 3:
				/*
				 * No restrictions
				 */
				break;
			case 4:
				/*
				 * This is ISO-9660:1988 (ISO-9660 version 2)
				 */
				iso9660_namelen = MAX_ISONAME_V2; /* allow 207 chars */
				full_iso9660_filenames++;	/* 31+ chars	*/
				omit_version_number++;
				RR_relocation_depth = 32767;

				/*
				 * From -U ...
				 */
				omit_period++;			/* trailing dot */
				allow_leading_dots++;
				relaxed_filenames++;		/* all chars	*/
				allow_lowercase++;		/* even lowcase	*/
				allow_multidot++;		/* > 1 dots	*/
				break;

			default:
				comerrno(EX_BAD, "Illegal iso9660 Level %d, use 1..3 or 4.\n",
							iso9660_level);
			}
			break;
		case 'J':
			use_Joliet++;
			break;
		case OPTION_JLONG:
			use_Joliet++;
			jlen = JLONGMAX;
			break;
		case OPTION_JCHARSET:
			use_Joliet++;
			/* FALLTHROUGH */
		case OPTION_INPUT_CHARSET:
			icharset = optarg;
			break;
		case OPTION_OUTPUT_CHARSET:
			ocharset = optarg;
			break;
#ifdef JIGDO_TEMPLATE
		case OPTION_JTT_OUTPUT:
			jtemplate_out = optarg;
			break;
		case OPTION_JTJ_OUTPUT:
			jjigdo_out = optarg;
			break;
		case OPTION_JT_MD5_LIST:
			jmd5_list = optarg;
			break;
		case OPTION_JT_MIN_SIZE:
			jte_min_size = atoi(optarg);
			if (jte_min_size < MIN_JIGDO_FILE_SIZE) {
				fprintf(stderr, "Jigdo min size %d too small; using default %d instead\n", jte_min_size, MIN_JIGDO_FILE_SIZE);
				jte_min_size = MIN_JIGDO_FILE_SIZE;
			}
			break;
		case OPTION_JT_INCLUDE:
			if (jte_add_include(optarg)) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				         "Failed to build jigdo-include list\n");
#else
				fprintf(stderr,
				        "Failed to build jigdo-include list\n");
				exit(1);
#endif
			}
			break;
		case OPTION_JT_EXCLUDE:
			if (jte_add_exclude(optarg)) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				         "Failed to build jigdo-exclude list\n");
#else
				fprintf(stderr,
				        "Failed to build jigdo-exclude list\n");
				exit(1);
#endif
			}
			break;
		case OPTION_JT_PATH_MAP:
			if (jte_add_mapping(optarg)) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				         "Failed to build jigdo mapping list\n");
#else
				fprintf(stderr,
				        "Failed to build jigdo mapping list\n");
				exit(1);
#endif
			}
			break;
#endif /* JIGDO_TEMPLATE */
		case OPTION_NOBAK:
			all_files = 0;
			break;
		case 'b':
			do_sort++;		/* We sort bootcat/botimage */
			use_eltorito++;
			boot_image = optarg;	/* pathname of the boot image */
						/* on disk */
			if (boot_image == NULL) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Required Eltorito boot image pathname missing\n");
#else
				fprintf(stderr,
				"Required Eltorito boot image pathname missing\n");
				exit(1);
#endif
			}
			get_boot_entry();
			current_boot_entry->boot_image = boot_image;
			break;
		case OPTION_ALT_BOOT:
			/*
			 * Start new boot entry parameter list.
			 */
			new_boot_entry();
			break;
		case OPTION_BOOTALPHA:
			use_alphaboot++;
			/* list of pathnames of boot images */
			add_boot_alpha_filename(optarg);
			break;
		case OPTION_HPPA_CMDLINE:
			use_hppaboot++;
			add_boot_hppa_cmdline(optarg);
			break;
		case OPTION_HPPA_KERNEL_32:
			use_hppaboot++;
			add_boot_hppa_kernel_32(optarg);
			break;
		case OPTION_HPPA_KERNEL_64:
			use_hppaboot++;
			add_boot_hppa_kernel_64(optarg);
			break;
		case OPTION_HPPA_BOOTLOADER:
			use_hppaboot++;
			add_boot_hppa_bootloader(optarg);
			break;
		case OPTION_HPPA_RAMDISK:
			use_hppaboot++;
			/* list of pathnames of boot images */
			add_boot_hppa_ramdisk(optarg);
			break;
		case OPTION_BOOTMIPS:
			use_mipsboot++;
			/* list of pathnames of boot images */
			add_boot_mips_filename(optarg);
			break;
		case OPTION_BOOTMIPSEL:
			use_mipselboot++;
			add_boot_mipsel_filename(optarg);
			break;
		case 'B':
			if (use_sunx86boot)
				comerrno(EX_BAD,
				"-sparc-boot and -sunx86-boot are mutual exclusive.\n");
			use_sparcboot++;
			/* list of pathnames of boot images */
			scan_sparc_boot(optarg);
			break;
		case OPTION_SUNX86BOOT:
			if (use_sparcboot)
				comerrno(EX_BAD,
				"-sparc-boot and -sunx86-boot are mutual exclusive.\n");
			use_sunx86boot++;
			/* list of pathnames of boot images */
			scan_sunx86_boot(optarg);
			break;
		case 'G':
			use_genboot++;
			/* pathname of the boot image on disk */
			genboot_image = optarg;
			if (genboot_image == NULL) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Required generic boot image pathname missing\n");
#else
				fprintf(stderr,
				"Required generic boot image pathname missing\n");
				exit(1);
#endif
			}
			break;
		case OPTION_SPARCLABEL:
			/* Sun disk label string */
			sparc_boot_label(optarg);
			break;
		case OPTION_SUNX86LABEL:
			/* Sun disk label string */
			sunx86_boot_label(optarg);
			break;
		case 'c':
			use_eltorito++;
			/* pathname of the boot image on cd */
			boot_catalog = optarg;
			if (boot_catalog == NULL) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Required boot catalog pathname missing\n");
#else
				fprintf(stderr,
				"Required boot catalog pathname missing\n");
				exit(1);
#endif
			}
			break;
		case OPTION_ABSTRACT:
			abstract = optarg;
			if (strlen(abstract) > 37) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Abstract filename string too long\n");
#else
				fprintf(stderr,
				"Abstract filename string too long\n");
				exit(1);
#endif
			}
			break;
		case 'A':
			appid = optarg;
			if (strlen(appid) > 128) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Application-id string too long\n");
#else
				fprintf(stderr,
				"Application-id string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_BIBLIO:
			biblio = optarg;
			if (strlen(biblio) > 37) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Bibliographic filename string too long\n");
#else
				fprintf(stderr,
				"Bibliographic filename string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_CACHE_INODES:
			cache_inodes = 1;
			break;
		case OPTION_NOCACHE_INODES:
			cache_inodes = 0;
			break;
		case OPTION_CHECK_OLDNAMES:
			check_oldnames++;
			break;
		case OPTION_CHECK_SESSION:
			check_session++;
			check_oldnames++;
			merge_image = optarg;
			outfile = "/dev/null";
			/*
			 * cdrecord_data is handled specially in multi.c
			 * as we cannot write to all strings.
			 * If genisoimage is called with -C xx,yy
			 * our default is overwritten.
			 */
/*			cdrecord_data = "0,0";*/
			break;
		case OPTION_COPYRIGHT:
			copyright = optarg;
			if (strlen(copyright) > 37) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Copyright filename string too long\n");
#else
				fprintf(stderr,
				"Copyright filename string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_DEBUG:
			debug++;
			break;
		case 'd':
			omit_period++;
			warn_violate++;
			break;
		case 'D':
			RR_relocation_depth = 32767;
			break;
		case 'f':
			follow_links++;
			break;
		case 'l':
			full_iso9660_filenames++;
			break;
		case OPTION_MAX_FILENAMES:
			iso9660_namelen = MAX_ISONAME_V1; /* allow 37 chars */
			full_iso9660_filenames++;
			omit_version_number++;
			warn_violate++;
			break;
		case 'L':
			/* FALLTHRU */
		case OPTION_ALLOW_LEADING_DOTS:
			allow_leading_dots++;
			warn_violate++;
			break;
		case OPTION_LOG_FILE:
			log_file = optarg;
			break;
		case 'M':
			merge_image = optarg;
			break;
		case OPTION_RELOC_ROOT:
			reloc_root = optarg;
			break;
		case OPTION_RELOC_OLD_ROOT:
			reloc_old_root = optarg;
			break;
		case 'N':
			omit_version_number++;
			warn_violate++;
			break;
		case OPTION_FORCE_RR:
			force_rr++;
			break;
		case OPTION_NO_RR:
			no_rr++;
			break;
		case 'o':
			outfile = optarg;
			break;
		case OPTION_PAD:
			dopad++;
			break;
		case OPTION_NOPAD:
			dopad = 0;
			break;
		case OPTION_P_LIST:
			pathnames = optarg;
			break;
		case 'p':
			preparer = optarg;
			if (strlen(preparer) > 128) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Preparer string too long\n");
#else
				fprintf(stderr, "Preparer string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_PRINT_SIZE:
			print_size++;
			break;
		case 'P':
			/* FALLTHRU */
		case OPTION_PUBLISHER:
			publisher = optarg;
			if (strlen(publisher) > 128) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
						"Publisher string too long\n");
#else
				fprintf(stderr, "Publisher string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_QUIET:
			verbose = 0;
			break;
		case 'R':
			use_RockRidge++;
			break;
		case 'r':
			rationalize_all++;
			use_RockRidge++;
			break;
		case OPTION_XA:
			use_XA++;
			break;
		case OPTION_XA_RATIONALIZED:
			rationalize_all++;
			use_XA++;
			break;

		case 's':
			if (strcmp(optarg, "data") == 0)
				osecsize = 2048;
			else if (strcmp(optarg, "xa1") == 0)
				osecsize = 2056;
			else if (strcmp(optarg, "raw") == 0) {
				osecsize = 2352;
				comerrno(EX_BAD,
					"Unsupported sector type '%s'.\n",
					optarg);
			}
			break;
		case 'S':
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD, "Option -%c is reserved for future use.\n", c);
#else
			fprintf(stderr, "Option -%c is reserved for future use.\n", c);
#endif
			susage(1);
			/* NOTREACHED */

		case OPTION_NEW_DIR_MODE:
			rationalize++;
		{
			char	*end = 0;

			new_dir_mode = strtol(optarg, &end, 8);
			if (!end || *end != 0 ||
			    new_dir_mode < 0 || new_dir_mode > 07777) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Bad mode for -new-dir-mode\n");
#else
				fprintf(stderr, "Bad mode for -new-dir-mode\n");
				exit(1);
#endif
			}
			break;
		}

		case OPTION_UID:
			rationalize++;
			use_RockRidge++;
			rationalize_uid++;
		{
			char	*end = 0;

			uid_to_use = strtol(optarg, &end, 0);
			if (!end || *end != 0) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Bad value for -uid\n");
#else
				fprintf(stderr, "Bad value for -uid\n");
				exit(1);
#endif
			}
			break;
		}

		case OPTION_GID:
			rationalize++;
			use_RockRidge++;
			rationalize_gid++;
		{
			char	*end = 0;

			gid_to_use = strtol(optarg, &end, 0);
			if (!end || *end != 0) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Bad value for -gid\n");
#else
				fprintf(stderr, "Bad value for -gid\n");
				exit(1);
#endif
			}
			break;
		}

		case OPTION_FILEMODE:
			rationalize++;
			use_RockRidge++;
			rationalize_filemode++;
		{
			char	*end = 0;

			filemode_to_use = strtol(optarg, &end, 8);
			if (!end || *end != 0 ||
			    filemode_to_use < 0 || filemode_to_use > 07777) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Bad mode for -file-mode\n");
#else
				fprintf(stderr, "Bad mode for -file-mode\n");
				exit(1);
#endif
			}
			break;
		}

		case OPTION_DIRMODE:
			rationalize++;
			use_RockRidge++;
			rationalize_dirmode++;
		{
			char	*end = 0;

			dirmode_to_use = strtol(optarg, &end, 8);
			if (!end || *end != 0 ||
			    dirmode_to_use < 0 || dirmode_to_use > 07777) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD, "Bad mode for -dir-mode\n");
#else
				fprintf(stderr, "Bad mode for -dir-mode\n");
				exit(1);
#endif
			}
			break;
		}

#ifdef SORTING
		case OPTION_SORT:
			do_sort++;
			add_sort_list(optarg);
			break;
#endif /* SORTING */

		case OPTION_SPLIT_OUTPUT:
			split_output++;
			break;

		case OPTION_STREAM_FILE_NAME:
			stream_filename = optarg;
			break;

		case OPTION_STREAM_CD_SIZE:
			stream_media_size = atoi(optarg);
			break;

		case OPTION_SYSID:
			system_id = optarg;
			if (strlen(system_id) > 32) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
						"System ID string too long\n");
#else
				fprintf(stderr, "System ID string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_TRANS_TBL:
			trans_tbl = optarg;
			/* FALLTHRU */
		case 'T':
			generate_tables++;
			break;
		case OPTION_UCS_LEVEL:
			ucs_level = atoi(optarg);
			if (ucs_level < 1 || ucs_level > 3)
				comerrno(EX_BAD, "Illegal UCS Level %d, use 1..3.\n",
							ucs_level);
			break;
#ifdef UDF
		case OPTION_UDF:
			use_udf++;
			break;
#endif

#ifdef DVD_VIDEO
		case OPTION_DVD:
			use_udf++;
			dvd_video++;
			break;
#endif
		case OPTION_USE_FILEVERSION:
			use_fileversion++;
			break;
		case 'U':
			/*
			 * Minimal (only truncation of 31+ characters)
			 * translation of filenames.
			 *
			 * Forces -l, -d, -N, -allow-leading-dots,
			 * -relaxed-filenames,
			 * -allow-lowercase, -allow-multidot
			 *
			 * This is for HP-UX, which does not recognize ANY
			 * extentions (Rock Ridge, Joliet), causing pain when
			 * loading software. pfs_mount can be used to read the
			 * extensions, but the untranslated filenames can be
			 * read by the "native" cdfs mounter. Completely
			 * violates iso9660.
			 */
			full_iso9660_filenames++;	/* 31 chars	*/
			omit_period++;			/* trailing dot */
			allow_leading_dots++;
			omit_version_number++;
			relaxed_filenames++;		/* all chars	*/
			allow_lowercase++;		/* even lowcase	*/
			allow_multidot++;		/* > 1 dots	*/
			warn_violate++;
			break;

		case OPTION_RELAXED_FILENAMES:
			relaxed_filenames++;
			warn_violate++;
			break;
		case OPTION_ALLOW_LOWERCASE:
			allow_lowercase++;
			warn_violate++;
			break;
		case OPTION_ALLOW_MULTIDOT:
			allow_multidot++;
			warn_violate++;
			break;
		case OPTION_ISO_TRANSLATE:
			iso_translate = 0;
			warn_violate++;
			break;
		case 'V':
			volume_id = optarg;
			if (strlen(volume_id) > 32) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
					"Volume ID string too long\n");
#else
				fprintf(stderr,
					"Volume ID string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_VOLSET:
			volset_id = optarg;
			if (strlen(volset_id) > 128) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Volume set ID string too long\n");
#else
				fprintf(stderr,
				"Volume set ID string too long\n");
				exit(1);
#endif
			}
			break;
		case OPTION_VOLSET_SIZE:
			volume_set_size = atoi(optarg);
			if (volume_set_size <= 0) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Illegal Volume Set Size %s\n", optarg);
#else
				fprintf(stderr,
				"Illegal Volume Set Size %s\n", optarg);
				exit(1);
#endif
			}
			if (volume_set_size > 1) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Volume Set Size > 1 not yet supported\n");
#else
				fprintf(stderr,
				"Volume Set Size > 1 not yet supported\n");
				exit(1);
#endif
			}
			break;
		case OPTION_VOLSET_SEQ_NUM:
			volume_sequence_number = atoi(optarg);
			if (volume_sequence_number > volume_set_size) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Volume set sequence number too big\n");
#else
				fprintf(stderr,
				"Volume set sequence number too big\n");
				exit(1);
#endif
			}
			break;
		case 'v':
			verbose++;
			break;
		case 'z':
#ifdef VMS
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD,
			"Transparent compression not supported with VMS\n");
#else
			fprintf(stderr,
			"Transparent compression not supported with VMS\n");
			exit(1);
#endif
#else
			transparent_compression++;
#endif
			break;
		case 'x':
		case 'm':
			/*
			 * Somehow two options to do basically the same thing
			 * got added somewhere along the way.  The 'match'
			 * code supports limited globbing, so this is the one
			 * that got selected. Unfortunately the 'x' switch is
			 * probably more intuitive.
			 */
			add_match(optarg);
			break;
		case OPTION_X_LIST:
			add_list(optarg);
			break;
		case OPTION_I_HIDE:
			i_add_match(optarg);
			break;
		case OPTION_I_LIST:
			i_add_list(optarg);
			break;
		case OPTION_H_HIDE:
			h_add_match(optarg);
			break;
		case OPTION_H_LIST:
			h_add_list(optarg);
			break;
		case OPTION_J_HIDE:
			j_add_match(optarg);
			break;
		case OPTION_J_LIST:
			j_add_list(optarg);
			break;
		case OPTION_HIDE_TRANS_TBL:
			jhide_trans_tbl++;
			break;
		case OPTION_HIDE_RR_MOVED:
			hide_rr_moved++;
			break;
		case OPTION_HELP:
			usage(0);
			break;
		case OPTION_PVERSION:
      if(mkisofs_call)
         printf("mkisofs 2.01 is not what you see here. This line is only a fake for too clever\n"
               "GUIs and other frontend applications. In fact, this program is:\n");
			printf("%s (%s)\n", version_string, HOST_SYSTEM);
			exit(0);
			break;
		case OPTION_NOSPLIT_SL_COMPONENT:
			split_SL_component = 0;
			break;
		case OPTION_NOSPLIT_SL_FIELD:
			split_SL_field = 0;
			break;
		case OPTION_HARD_DISK_BOOT:
			use_eltorito++;
			hard_disk_boot++;
			get_boot_entry();
			current_boot_entry->hard_disk_boot = 1;
			break;
		case OPTION_NO_EMUL_BOOT:
			use_eltorito++;
			no_emul_boot++;
			get_boot_entry();
			current_boot_entry->no_emul_boot = 1;
			break;
		case OPTION_NO_BOOT:
			use_eltorito++;
			not_bootable++;
			get_boot_entry();
			current_boot_entry->not_bootable = 1;
			break;
		case OPTION_BOOT_LOAD_ADDR:
			use_eltorito++;
			{
				long	val;
				char	*ptr;

				val = strtol(optarg, &ptr, 0);
				if (*ptr || val < 0 || val >= 0x10000) {
#ifdef	USE_LIBSCHILY
					comerrno(EX_BAD, "Boot image load address invalid.\n");
#else
					fprintf(stderr, "Boot image load address invalid.\n");
					exit(1);
#endif
				}
				load_addr = val;
			}
			get_boot_entry();
			current_boot_entry->load_addr = load_addr;
			break;
		case OPTION_BOOT_LOAD_SIZE:
			use_eltorito++;
			{
				long	val;
				char	*ptr;

				val = strtol(optarg, &ptr, 0);
				if (*ptr || val < 0 || val >= 0x10000) {
#ifdef	USE_LIBSCHILY
					comerrno(EX_BAD,
					"Boot image load size invalid.\n");
#else
					fprintf(stderr,
					"Boot image load size invalid.\n");
					exit(1);
#endif
				}
				load_size = val;
			}
			get_boot_entry();
			current_boot_entry->load_size = load_size;
			break;
		case OPTION_BOOT_INFO_TABLE:
			use_eltorito++;
			boot_info_table++;
			get_boot_entry();
			current_boot_entry->boot_info_table = 1;
			break;
#ifdef APPLE_HYB
		case OPTION_HFS_TYPE:
			deftype = optarg;
			hfs_ct++;
			if (strlen(deftype) != 4) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"HFS default TYPE string has illegal length.\n");
#else
				fprintf(stderr,
				"HFS default TYPE string has illegal length.\n");
				exit(1);
#endif
			}
			break;
		case OPTION_HFS_CREATOR:
			defcreator = optarg;
			hfs_ct++;
			if (strlen(defcreator) != 4) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"HFS default CREATOR string has illegal length.\n");
#else
				fprintf(stderr,
				"HFS default CREATOR string has illegal length.\n");
				exit(1);
#endif
			}
			break;
		case 'H':
			/* FALLTHRU */
		case OPTION_MAP_FILE:
			afpfile = optarg;
			hfs_last = MAP_LAST;
			break;
		case 'h':
			apple_hyb = 1;
			break;
		case 'g':
			apple_ext = 1;
			break;
		case OPTION_PROBE:
			probe = 1;
			break;
		case OPTION_MACNAME:
			use_mac_name = 1;
			break;
		case OPTION_NOMACFILES:
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
			"Warning: -no-mac-files no longer used ... ignoring\n");
#else
			fprintf(stderr,
			"Warning: -no-mac-files no longer used ... ignoring\n");
#endif
			break;
		case OPTION_BOOT_HFS_FILE:
			hfs_boot_file = optarg;
			/* FALLTHRU */
		case OPTION_GEN_PT:
			gen_pt = 1;
			break;
		case OPTION_MAGIC_FILE:
#ifndef USE_MAGIC
			fprintf(stderr, "This program has been compiled without magic library support.\n"
				"Ignoring the -magic option.\n");
#endif
			magic_filename = optarg;
			hfs_last = MAG_LAST;
			break;
		case OPTION_AUTOSTART:
			autoname = optarg;
			/* gen_pt = 1; */
			break;
		case OPTION_BSIZE:
			afe_size = atoi(optarg);
			hfs_select |= DO_FEU;
			hfs_select |= DO_FEL;
			break;
		case OPTION_HFS_VOLID:
			hfs_volume_id = optarg;
			break;
		case OPTION_ROOT_INFO:
			root_info = optarg;
			/* FALLTHRU */
		case OPTION_ICON_POS:
			icon_pos = 1;
			break;
		/* Mac/Unix types to include */
		case OPTION_CAP:
			hfs_select |= DO_CAP;
			break;
		case OPTION_NETA:
			hfs_select |= DO_NETA;
			break;
		case OPTION_DBL:
			hfs_select |= DO_DBL;
			break;
		case OPTION_ESH:
		case OPTION_USH:
			hfs_select |= DO_ESH;
			break;
		case OPTION_FE:
			hfs_select |= DO_FEU;
			hfs_select |= DO_FEL;
			break;
		case OPTION_SGI:
		case OPTION_XIN:
			hfs_select |= DO_SGI;
			break;
		case OPTION_MBIN:
			hfs_select |= DO_MBIN;
			break;
		case OPTION_SGL:
			hfs_select |= DO_SGL;
			break;
		case OPTION_DAVE:
			hfs_select |= DO_DAVE;
			break;
		case OPTION_SFM:
			hfs_select |= DO_SFM;
			break;
		case OPTION_XDBL:
			hfs_select |= DO_XDBL;
			break;
		case OPTION_XHFS:
#ifdef IS_MACOS_X
			hfs_select |= DO_XHFS;
#else /* IS_MACOS_X */
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD,
			"Warning: --osx-hfs only works on MacOS X ... ignoring\n");
#else /* USE_LIBSCHILY */
			fprintf(stderr,
			"Warning: --osx-hfs only works on MacOS X ... ignoring\n");
#endif /* USE_LIBSCHILY */
#endif /* IS_MACOS_X */
			break;
		case OPTION_CREATE_DT:
			create_dt = 0;
			break;
		case OPTION_HFS_HIDE:
			hfs_add_match(optarg);
			break;
		case OPTION_HFS_LIST:
			hfs_add_list(optarg);
			break;
		case OPTION_HFS_INPUT_CHARSET:
			use_mac_name = 1;
			hfs_icharset = optarg;
			break;
		case OPTION_HFS_OUTPUT_CHARSET:
			hfs_ocharset = optarg;
			break;
		case OPTION_HFS_UNLOCK:
			hfs_lock = 0;
			break;
		case OPTION_HFS_BLESS:
			hfs_bless = optarg;
			break;
		case OPTION_HFS_PARMS:
			hfs_parms = strdup(optarg);
			break;
#ifdef PREP_BOOT
		case OPTION_PREP_BOOT:
			use_prep_boot++;
			if (use_prep_boot > 4 - use_chrp_boot) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Maximum of 4 PRep+CHRP partition entries are allowed\n");
#else
				fprintf(stderr,
				"Maximum of 4 PRep+CHRP partition entries are allowed\n");
#endif
				exit(1);
			}
			/* pathname of the boot image on cd */
			prep_boot_image[use_prep_boot - 1] = optarg;
			if (prep_boot_image[use_prep_boot - 1] == NULL) {
#ifdef	USE_LIBSCHILY
				comerrno(EX_BAD,
				"Required PReP boot image pathname missing\n");
#else
				fprintf(stderr,
				"Required PReP boot image pathname missing\n");
#endif
				exit(1);
			}
			break;
		case OPTION_CHRP_BOOT:
			if (use_chrp_boot)
				break;		/* silently allow duplicates */
			use_chrp_boot = 1;
			if (use_prep_boot > 3) {
#ifdef USE_LIBSCHILY
				comerrno(EX_BAD,
				"Maximum of 4 PRep+CHRP partition entries are allowed\n");
#else
				fprintf(stderr,
				"Maximum of 4 PRep+CHRP partition entries are allowed\n");
#endif
				exit(1);
			}
			break;
#endif	/* PREP_BOOT */
#endif	/* APPLE_HYB */
		case OPTION_ALLOW_LIMITED_SIZE:
			allow_limited_size++;
      use_udf++;
			break;
		default:
			susage(1);
		}
	/*
	 * "--" was found, the next argument is a pathspec
	 */
	if (argc != optind)
		have_cmd_line_pathspec = 1;

parse_input_files:
	path_ind = optind;

	/*
	 * XXX This is a hack until we have a decent separate name handling
	 * XXX for UDF filenames.
	 */
	if (dvd_video && use_Joliet) {
		use_Joliet = 0;
		fprintf(stderr, "Warning: Disabling Joliet support for DVD-Video.\n");
	}
	if (use_udf && !use_Joliet)
		jlen = 255;

	if (preparer) {
		if (strlen(preparer) > 128) {
			comerrno(EX_BAD, "Preparer string too long\n");
		}
	}
	if (publisher) {
		if (strlen(publisher) > 128) {
			comerrno(EX_BAD,
				"Publisher string too long\n");
		}
	}
	if (stream_filename) {
		if (strlen(stream_filename) > MAX_ISONAME)
			comerrno(EX_BAD,
					"stream-file-name too long (%d), max is %d.\n",
					strlen(stream_filename), MAX_ISONAME);
		if (strchr(stream_filename, '/'))
			comerrno(EX_BAD, "Illegal character '/' in stream-file-name.\n");
		iso9660_level = 4;
	} else {
		stream_filename = "STREAM.IMG;1";
	}
	if (system_id) {
		if (strlen(system_id) > 32) {
			comerrno(EX_BAD,
					"System ID string too long\n");
		}
	}


	if (use_RockRidge && (iso9660_namelen > MAX_ISONAME_V2_RR))
		iso9660_namelen = MAX_ISONAME_V2_RR;

	if (warn_violate) /* this one is enough for quiet mode, print others warnings only in more verbose modes */
		fprintf(stderr, "Warning: creating filesystem that does not conform to ISO-9660.\n");
	if (iso9660_level > 3 && verbose>0)
		fprintf(stderr, "Warning: Creating ISO-9660:1999 (version 2) filesystem.\n");
	if (iso9660_namelen > LEN_ISONAME && verbose>0)
		fprintf(stderr, "Warning: ISO-9660 filenames longer than %d may cause buffer overflows in the OS.\n",
			LEN_ISONAME);
	if (use_Joliet && !use_RockRidge && verbose>0) {
		fprintf(stderr,
                      "Warning: creating filesystem with Joliet extensions but without Rock Ridge\n"
                      "         extensions. It is highly recommended to add Rock Ridge.\n");
	}
	if (transparent_compression && verbose>0) {
		fprintf(stderr, "Warning: using transparent compression. This is a nonstandard Rock Ridge\n");
		fprintf(stderr, "         extension. The resulting filesystem can only be transparently\n");
		fprintf(stderr, "         read on Linux. On other operating systems you need to call\n");
		fprintf(stderr, "         mkzftree by hand to decompress the files.\n");
	}
	if (transparent_compression && !use_RockRidge && verbose>0) {
		fprintf(stderr, "Warning: transparent decompression is a Linux Rock Ridge extension, but\n");
		fprintf(stderr, "         creating filesystem without Rock Ridge attributes; files\n");
		fprintf(stderr, "         will not be transparently decompressed.\n");
	}
    if(follow_links && verbose>0)
        fprintf(stderr,
                "Warning: -follow-links does not always work correctly; be careful.\n");

	init_unls();		/* Initialize UNICODE tables */

	/* initialize code tables from a file - if they exists */
	init_unls_file(icharset);
	init_unls_file(ocharset);
#ifdef APPLE_HYB
	init_unls_file(hfs_icharset);
	init_unls_file(hfs_ocharset);
#endif /* APPLE_HYB */

#ifdef USE_ICONV
	iconv_possible = !(iso9660_level >= 4 || ((ocharset &&
		strcmp(ocharset, icharset ? icharset : "")) &&
		use_RockRidge) || apple_ext || apple_hyb);

	setlocale(LC_CTYPE, "");
	
  	if (icharset == NULL && iconv_possible) {
		char *charset = nl_langinfo(CODESET);
		/* set to detected value but only if it is not pure US-ASCII */
  	if(charset) { /* workaround for SunOS, iconv is case-sensitive */
 			char *t;
 			charset = strdup(charset);
 			for(t=charset;*t!='\0';t++)
 				*t=tolower(*t);
 		}

		if(strcmp(charset, "ansi_x3.4-1968") != 0)
			icharset = charset;

		if(icharset && verbose > 0)
			fprintf(stderr, "I: -input-charset not specified, using %s (detected in locale settings)\n",
			icharset);
	}

	if(iconv_possible) {
		/*
		 * don't care if initialization fails
		 */
		init_nls_iconv(icharset);
		init_nls_iconv(ocharset);
	}
#endif

	if (icharset == NULL) {
#if	(defined(__CYGWIN32__) || defined(__CYGWIN__) || defined(__DJGPP__)) && !defined(IS_CYGWIN_1)
		in_nls = load_unls("cp437");
#else
		in_nls = load_unls("iso8859-1");
#endif
	} else {
		if (strcmp(icharset, "default") == 0)
			in_nls = load_unls_default();
		else
			in_nls = load_unls(icharset);
	}
	/*
	 * set the output charset to the same as the input or the given output
	 * charset
	 */
	if (ocharset == NULL) {
		out_nls = in_nls;
	} else {
		if (strcmp(ocharset, "default") == 0)
			out_nls = load_unls_default();
		else
			out_nls = load_unls(ocharset);
	}
	if (in_nls == NULL || out_nls == NULL) { /* Unknown charset specified */
		fprintf(stderr, "Unknown charset\nKnown charsets are:\n");
		list_unls();	/* List all known charset names */
#ifdef USE_ICONV
        fprintf(stderr, "\nAdditional input charsets are available for Joliet through the iconv support."
                "\nRun \"iconv -l\" to display them. Iconv charsets cannot be used with HFS, Apple"
                "\nextension, ISO9660 version 2 or Rock Ridge.\n");
#endif
		exit(1);
	}


#ifdef APPLE_HYB
	if (hfs_icharset == NULL || strcmp(hfs_icharset, "mac-roman")) {
		hfs_inls = load_unls("cp10000");
	} else {
		if (strcmp(hfs_icharset, "default") == 0)
			hfs_inls = load_unls_default();
		else
			hfs_inls = load_unls(hfs_icharset);
	}
	if (hfs_ocharset == NULL) {
		hfs_onls = hfs_inls;
	} else {
		if (strcmp(hfs_ocharset, "default") == 0)
			hfs_onls = load_unls_default();
		else if (strcmp(hfs_ocharset, "mac-roman") == 0)
			hfs_onls = load_unls("cp10000");
		else
			hfs_onls = load_unls(hfs_ocharset);
	}

	if (hfs_inls == NULL || hfs_onls == NULL) {
		fprintf(stderr, "Unknown HFS charset\nKnown charsets are:\n");
		list_unls();
		exit(1);
	}
#endif /* APPLE_HYB */

	if (merge_image != NULL) {
		if (open_merge_image(merge_image) < 0) {
			/* Complain and die. */
#ifdef	USE_LIBSCHILY
			comerr("Unable to open previous session image '%s'.\n",
				merge_image);
#else
			fprintf(stderr,
				"Unable to open previous session image '%s'.\n",
				merge_image);
			exit(1);
#endif
		}
	}
	/* We don't need root privilleges anymore. */
#ifdef	HAVE_SETREUID
	if (setreuid(-1, getuid()) < 0)
#else
#ifdef	HAVE_SETEUID
	if (seteuid(getuid()) < 0)
#else
	if (setuid(getuid()) < 0)
#endif
#endif
#ifdef	USE_LIBSCHILY
		comerr("Panic cannot set back effective uid.\n");
#else
	{
		perror("Panic cannot set back effective uid.");
		exit(1);
	}
#endif


#ifdef	no_more_needed
#ifdef __NetBSD__
	{
		int		resource;
		struct rlimit	rlp;

		if (getrlimit(RLIMIT_DATA, &rlp) == -1)
			perror("Warning: getrlimit failed");
		else {
			rlp.rlim_cur = 33554432;
			if (setrlimit(RLIMIT_DATA, &rlp) == -1)
				perror("Warning: setrlimit failed");
		}
	}
#endif
#endif	/* no_more_needed */
#ifdef HAVE_SBRK
	mem_start = (unsigned long) sbrk(0);
#endif

	/*
	 * if the -hide-joliet option has been given, set the Joliet option
	 */
	if (!use_Joliet && j_ishidden())
		use_Joliet++;

#ifdef APPLE_HYB
	if (apple_hyb && apple_ext) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have both -apple and -hfs options");
#else
		fprintf(stderr, "Can't have both -apple and -hfs options");
		exit(1);
#endif
	}
	/*
	 * if -probe, -macname, any hfs selection and/or mapping file is given,
	 * but no HFS option, then select apple_hyb
	 */
	if (!apple_hyb && !apple_ext) {
		if (*afpfile || probe || use_mac_name || hfs_select ||
				hfs_boot_file || magic_filename ||
				hfs_ishidden() || gen_pt || autoname ||
				afe_size || icon_pos || hfs_ct ||
				hfs_icharset || hfs_ocharset) {
			apple_hyb = 1;
		}
	}
	if (apple_ext && hfs_boot_file) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have -hfs-boot-file with -apple\n");
#else
		fprintf(stderr, "Can't have -hfs-boot-file with -apple\n");
		exit(1);
#endif
	}
	if (apple_ext && autoname) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have -auto with -apple\n");
#else
		fprintf(stderr, "Can't have -auto with -apple\n");
		exit(1);
#endif
	}
	if (apple_hyb && (use_sparcboot || use_sunx86boot)) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have -hfs with -sparc-boot/-sunx86-boot\n");
#else
		fprintf(stderr, "Can't have -hfs with -sparc-boot/-sunx86-boot\n");
		exit(1);
#endif
	}
	if (apple_hyb && use_genboot) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have -hfs with -generic-boot\n");
#else
		fprintf(stderr, "Can't have -hfs with -generic-boot\n");
		exit(1);
#endif
	}
#ifdef PREP_BOOT
	if (apple_ext && use_prep_boot) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD, "Can't have -prep-boot with -apple\n");
#else
		fprintf(stderr, "Can't have -prep-boot with -apple\n");
		exit(1);
#endif
	}
#endif	/* PREP_BOOT */

	if (apple_hyb || apple_ext)
		apple_both = 1;

	if (probe)
		/* we need to search for all types of Apple/Unix files */
		hfs_select = ~0;

	if (apple_both && verbose && !(hfs_select || *afpfile || magic_filename)) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
		"Warning: no Apple/Unix files will be decoded/mapped\n");
#else
		fprintf(stderr,
		"Warning: no Apple/Unix files will be decoded/mapped\n");
#endif
	}
	if (apple_both && verbose && !afe_size &&
					(hfs_select & (DO_FEU | DO_FEL))) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
		"Warning: assuming PC Exchange cluster size of 512 bytes\n");
#else
		fprintf(stderr,
		"Warning: assuming PC Exchange cluster size of 512 bytes\n");
#endif
		afe_size = 512;
	}
	if (apple_both) {
		/* set up the TYPE/CREATOR mappings */
		hfs_init(afpfile, 0, hfs_select);
	}
	if (apple_ext && !use_RockRidge) {
#ifdef	nonono
		/* use RockRidge to set the SystemUse field ... */
		use_RockRidge++;
		rationalize_all++;
#else
		/* EMPTY */
#endif
	}
	if (apple_ext && !(use_XA || use_RockRidge)) {
		comerrno(EX_BAD, "Need either -XA/-xa or -R/-r for -apple to become active.\n");
	}

#endif	/* APPLE_HYB */

	if (rationalize_all) {
		rationalize++;
		rationalize_uid++;
		rationalize_gid++;
		rationalize_filemode++;
		rationalize_dirmode++;
	}

	if (verbose > 1) {
		fprintf(stderr, "%s (%s)\n", version_string, HOST_SYSTEM);
	}
	if (cdrecord_data == NULL && !check_session && merge_image != NULL) {
#ifdef	USE_LIBSCHILY
		comerrno(EX_BAD,
		"Multisession usage bug: Must specify -C if -M is used.\n");
#else
		fprintf(stderr,
		"Multisession usage bug: Must specify -C if -M is used.\n");
		exit(1);
#endif
	}
	if (cdrecord_data != NULL && merge_image == NULL) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
		"Warning: -C specified without -M: old session data will not be merged.\n");
#else
		fprintf(stderr,
		"Warning: -C specified without -M: old session data will not be merged.\n");
#endif
	}
#ifdef APPLE_HYB
	if (merge_image != NULL && apple_hyb) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD,
		"Warning: files from previous sessions will not be included in the HFS volume.\n");
#else
		fprintf(stderr,
		"Warning: files from previous sessions will not be included in the HFS volume.\n");
#endif
	}
#endif	/* APPLE_HYB */

	/*
	 * see if we have a list of pathnames to process
	 */
	if (pathnames) {
		/* "-" means take list from the standard input */
		if (strcmp(pathnames, "-")) {
			if ((pfp = fopen(pathnames, "r")) == NULL) {
#ifdef	USE_LIBSCHILY
				comerr("Unable to open pathname list %s.\n",
								pathnames);
#else
				fprintf(stderr,
					"Unable to open pathname list %s.\n",
								pathnames);
				exit(1);
#endif
			}
		} else
			pfp = stdin;
	}

	/* The first step is to scan the directory tree, and take some notes */

	if ((arg = get_pnames(argc, argv, optind, pname,
					sizeof (pname), pfp)) == NULL) {
		if (check_session == 0 && !stream_media_size) {
#ifdef	USE_LIBSCHILY
			errmsgno(EX_BAD, "Missing pathspec.\n");
#endif
			susage(1);
		}
	}

	/*
	 * if we don't have a pathspec, then save the pathspec found
	 * in the pathnames file (stored in pname) - we don't want
	 * to skip this pathspec when we read the pathnames file again
	 */
	if (!have_cmd_line_pathspec && !stream_media_size) {
		save_pname = 1;
	}
	if (stream_media_size) {
		if (use_XA || use_RockRidge || use_udf || use_Joliet)
			comerrno(EX_BAD,
			"Cannot use XA, Rock Ridge, UDF or Joliet with -stream-media-size\n");
		if (merge_image)
			comerrno(EX_BAD,
			"Cannot use multi session with -stream-media-size\n");
		if (use_eltorito || use_sparcboot || use_sunx86boot ||
		    use_genboot || use_prep_boot || hfs_boot_file)
			comerrno(EX_BAD,
			"Cannot use boot options with -stream-media-size\n");
		if (apple_hyb)
			comerrno(EX_BAD,
			"Cannot use Apple hybrid options with -stream-media-size\n");
	}

	if (use_RockRidge) {
		/* BEGIN CSTYLED */
#if 1
		extension_record = generate_rr_extension_record("RRIP_1991A",
			"THE ROCK RIDGE INTERCHANGE PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
			"PLEASE CONTACT DISC PUBLISHER FOR SPECIFICATION SOURCE.  SEE PUBLISHER IDENTIFIER IN PRIMARY VOLUME DESCRIPTOR FOR CONTACT INFORMATION.",
			&extension_record_size);
#else
		extension_record = generate_rr_extension_record("IEEE_P1282",
			"THE IEEE P1282 PROTOCOL PROVIDES SUPPORT FOR POSIX FILE SYSTEM SEMANTICS",
			"PLEASE CONTACT THE IEEE STANDARDS DEPARTMENT, PISCATAWAY, NJ, USA FOR THE P1282 SPECIFICATION.",
			&extension_record_size);
#endif
		/* END CSTYLED */
	}
	if (log_file) {
		FILE		*lfp;
		int		i;

		/* open log file - test that we can open OK */
		if ((lfp = fopen(log_file, "w")) == NULL) {
#ifdef	USE_LIBSCHILY
			comerr("Can't open logfile: '%s'.\n", log_file);
#else
			fprintf(stderr, "Can't open logfile: '%s'.\n", log_file);
			exit(1);
#endif
		}
		fclose(lfp);

		/* redirect all stderr message to log_file */
		fprintf(stderr, "re-directing all messages to %s\n", log_file);
		fflush(stderr);

		/* associate stderr with the log file */
		if (freopen(log_file, "w", stderr) == NULL) {
#ifdef	USE_LIBSCHILY
			comerr("Can't open logfile: '%s'.\n", log_file);
#else
			fprintf(stderr, "Can't open logfile: '%s'.\n", log_file);
			exit(1);
#endif
		}
		if (verbose > 1) {
			for (i = 0; i < argc; i++)
				fprintf(stderr, "%s ", argv[i]);

			fprintf(stderr, "\n%s (%s)\n",
				version_string, HOST_SYSTEM);
		}
	}
	/* Find name of root directory. */
	if (arg != NULL)
		node = findgequal(arg);
	if (!use_graft_ptrs)
		node = NULL;
	if (node == NULL) {
		if (use_graft_ptrs && arg != NULL)
			node = escstrcpy(nodename, arg);
		else
			node = arg;
	} else {
		/*
		 * Remove '\\' escape chars which are located
		 * before '\\' and '=' chars
		 */
		node = escstrcpy(nodename, ++node);
	}

	/*
	 * See if boot catalog file exists in root directory, if not we will
	 * create it.
	 */
	if (use_eltorito)
		init_boot_catalog(node);

	/*
	 * Find the device and inode number of the root directory. Record this
	 * in the hash table so we don't scan it more than once.
	 */
	stat_filter(node, &statbuf);
	add_directory_hash(statbuf.st_dev, STAT_INODE(statbuf));

	memset(&de, 0, sizeof (de));

	/*
	 * PO:
	 * Isn't root NULL at this time anyway?
	 * I think it is created by the first call to
	 * find_or_create_directory() below.
	 */
	de.filedir = root;	/* We need this to bootstrap */

	if (cdrecord_data != NULL && merge_image == NULL) {
		/*
		 * in case we want to add a new session, but don't want to
		 * merge old one
		 */
		get_session_start(NULL);
	}
	if (merge_image != NULL) {
		char	sector[SECTOR_SIZE];

		errno = 0;
		mrootp = merge_isofs(merge_image);
		if (mrootp == NULL) {
			/* Complain and die. */
#ifdef	USE_LIBSCHILY
			if (errno == 0)
				errno = -1;
			comerr("Unable to find previous session PVD '%s'.\n",
				merge_image);
#else
			fprintf(stderr,
				"Unable to find previous session PVD '%s'.\n",
				merge_image);
			exit(1);
#endif
		}
		memcpy(de.isorec.extent, mrootp->extent, 8);

		/*
		 * Look for RR Attributes in '.' entry of root dir.
		 * This is the first ISO directory entry in the root dir.
		 */
		c = isonum_733((unsigned char *)mrootp->extent);
#ifdef  USE_SCG
		readsecs(c, sector, 1);
#else
    lseek(fileno(in_image), c*2048, SEEK_SET);
    read(fileno(in_image), sector, sizeof (sector));
#endif
		c = rr_flags((struct iso_directory_record *)sector);
		if (c & RR_FLAG_XA)
			fprintf(stderr, "XA signatures found\n");
		if (c & RR_FLAG_AA)
			fprintf(stderr, "AA signatures found\n");
		if (c & ~(RR_FLAG_XA|RR_FLAG_AA)) {
			if (c & RR_FLAG_SP) {
				fprintf(stderr, "Rock Ridge signatures found\n");
			} else {
				fprintf(stderr, "Bad Rock Ridge signatures found (SU record missing)\n");
				if (!force_rr)
					no_rr++;
			}
		} else {
			fprintf(stderr, "NO Rock Ridge present\n");
			if ((c & (RR_FLAG_XA|RR_FLAG_AA)) == 0) {
				if (!force_rr)
					no_rr++;
			}
		}
		if (no_rr)
			fprintf(stderr, "Disabling Rock Ridge / XA / AA\n");
	}
	/*
	 * Create an empty root directory. If we ever scan it for real,
	 * we will fill in the contents.
	 */
	find_or_create_directory(NULL, "", &de, TRUE);

#ifdef APPLE_HYB
	/* may need to set window layout of the volume */
	if (root_info)
		set_root_info(root_info);
#endif /* APPLE_HYB */

	if(optind < argc-1)
		merge_warn_msg="NOTE: multiple source directories have been specified and merged into the root\n"
			"of the filesystem. Check your program arguments. genisoimage is not tar.\n";

	/*
	 * Scan the actual directory (and any we find below it) for files to
	 * write out to the output image.  Note - we take multiple source
	 * directories and keep merging them onto the image.
	 */
if (check_session == 0)
	while ((arg = get_pnames(argc, argv, optind, pname,
					sizeof (pname), pfp)) != NULL) {
		struct directory *graft_dir;
		struct stat	st;
		char		*short_name;
		int		status;
		char		graft_point[PATH_MAX + 1];

		/*
		 * We would like a syntax like:
		 *
		 *	/tmp=/usr/tmp/xxx
		 *
		 * where the user can specify a place to graft each component
		 * of the tree.  To do this, we may have to create directories
		 * along the way, of course. Secondly, I would like to allow
		 * the user to do something like:
		 *
		 *	/home/baz/RMAIL=/u3/users/baz/RMAIL
		 *
		 * so that normal files could also be injected into the tree
		 * at an arbitrary point.
		 *
		 * The idea is that the last component of whatever is being
		 * entered would take the name from the last component of
		 * whatever the user specifies.
		 *
		 * The default will be that the file is injected at the root of
		 * the image tree.
		 */
		node = findgequal(arg);
		if (!use_graft_ptrs)
			node = NULL;
		/*
		 * Remove '\\' escape chars which are located
		 * before '\\' and '=' chars ---> below in escstrcpy()
		 */

		short_name = NULL;

		if (node != NULL || reloc_root) {
			char		*pnt;
			char		*xpnt;
			size_t		len;

			/* insert -root prefix */
			if (reloc_root != NULL) {
				strcpy(graft_point, reloc_root);
				len = strlen(graft_point);
				if (graft_point[len] != '/')
					graft_point[len++] = '/';
			} else {
				len = 0;
			}

			if (node) {
				*node = '\0';
				escstrcpy(&graft_point[len], arg);
				*node = '=';
			}

			/*
			 * Remove unwanted "./" & "/" sequences from start...
			 */
			do {
				xpnt = graft_point;
				while (xpnt[0] == '.' && xpnt[1] == '/')
					xpnt += 2;
				while (*xpnt == PATH_SEPARATOR) {
					xpnt++;
				}
				strcpy(graft_point, xpnt);
			} while (xpnt > graft_point);

			if (node) {
				node = escstrcpy(nodename, ++node);
			} else {
				node = arg;
			}

			graft_dir = root;
			xpnt = graft_point;

			/*
			 * If "node" points to a directory, then graft_point
			 * needs to point to a directory too.
			 */
			if (follow_links)
				status = stat_filter(node, &st);
			else
				status = lstat_filter(node, &st);
			if (status == 0 && S_ISDIR(st.st_mode)) {
				len = strlen(graft_point);

				if ((len <= (sizeof (graft_point) -1)) &&
				    graft_point[len-1] != '/') {
					graft_point[len++] = '/';
					graft_point[len] = '\0';
				}
			}
			if (debug)
				fprintf(stderr, "GRAFT:'%s'\n", xpnt);
			/*
			 * Loop down deeper and deeper until we find the
			 * correct insertion spot.
			 * Canonicalize the filename while parsing it.
			 */
			for (;;) {
				do {
					while (xpnt[0] == '.' && xpnt[1] == '/')
						xpnt += 2;
					while (xpnt[0] == '/')
						xpnt += 1;
					if (xpnt[0] == '.' && xpnt[1] == '.' && xpnt[2] == '/') {
						if (graft_dir && graft_dir != root) {
							graft_dir = graft_dir->parent;
							xpnt += 2;
						}
					}
				} while ((xpnt[0] == '/') || (xpnt[0] == '.' && xpnt[1] == '/'));
				pnt = strchr(xpnt, PATH_SEPARATOR);
				if (pnt == NULL) {
					if (*xpnt != '\0') {
						short_name = xpnt;
					}
					break;
				}
				*pnt = '\0';
				if (debug) {
					fprintf(stderr, "GRAFT Point:'%s' in '%s : %s' (%s)\n",
						xpnt,
						graft_dir->whole_name,
						graft_dir->de_name,
						graft_point);
				}
				graft_dir = find_or_create_directory(graft_dir,
					graft_point,
					NULL, TRUE);
				*pnt = PATH_SEPARATOR;
				xpnt = pnt + 1;
			}
		} else {
			graft_dir = root;
			if (use_graft_ptrs)
				node = escstrcpy(nodename, arg);
			else
				node = arg;
		}

		/*
		 * Now see whether the user wants to add a regular file, or a
		 * directory at this point.
		 */
		if (follow_links)
			status = stat_filter(node, &st);
		else
			status = lstat_filter(node, &st);
		if (status != 0) {
			/*
			 * This is a fatal error - the user won't be getting
			 * what they want if we were to proceed.
			 */
#ifdef	USE_LIBSCHILY
			comerr("Invalid node - '%s'.\n", node);
#else
			fprintf(stderr, "Invalid node - '%s'.\n", node);
			exit(1);
#endif
		} else {
			if (S_ISDIR(st.st_mode)) {
				if (debug) {
					fprintf(stderr, "graft_dir: '%s : %s', node: '%s', (scan)\n",
						graft_dir->whole_name,
						graft_dir->de_name, node);
				}
				if (!scan_directory_tree(graft_dir,
								node, &de)) {
					exit(1);
				}
				if (debug) {
					fprintf(stderr, "scan done\n");
				}
			} else {
				if (short_name == NULL) {
					short_name = strrchr(node,
							PATH_SEPARATOR);
					if (short_name == NULL ||
							short_name < node) {
						short_name = node;
					} else {
						short_name++;
					}
				}
				if (debug) {
					fprintf(stderr, "graft_dir: '%s : %s', node: '%s', short_name: '%s'\n",
						graft_dir->whole_name,
						graft_dir->de_name, node,
						short_name);
				}
#ifdef APPLE_HYB
				if (!insert_file_entry(graft_dir, node,
								short_name, 0))
#else
				if (!insert_file_entry(graft_dir, node,
								short_name))
#endif	/* APPLE_HYB */
				{
					/*
					 * Should we ignore this?
					 */
/*					exit(1);*/
					/* EMPTY */
				}
			}
		}

		optind++;
		no_path_names = 0;
	}

	if (pfp && pfp != stdin)
		fclose(pfp);

	/*
	 * exit if we don't have any pathnames to process
	 * - not going to happen at the moment as we have to have at least one
	 * path on the command line
	 */
	if (no_path_names && !check_session && !stream_media_size) {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD, "No pathnames found.\n");
#endif
		susage(1);
	}
	/*
	 * Now merge in any previous sessions.  This is driven on the source
	 * side, since we may need to create some additional directories.
	 */
	if (merge_image != NULL) {
		if (merge_previous_session(root, mrootp,
					reloc_root, reloc_old_root) < 0) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD, "Cannot merge previous session.\n");
#else
			fprintf(stderr, "Cannot merge previous session.\n");
			exit(1);
#endif
		}
		close_merge_image();

		/*
		 * set up parent_dir and filedir in relocated entries which
		 * were read from previous session so that
		 * finish_cl_pl_entries can do its job
		 */
		match_cl_re_entries();
	}
#ifdef APPLE_HYB
	/* free up any HFS filename mapping memory */
	if (apple_both)
		clean_hfs();
#endif	/* APPLE_HYB */

	/* hide "./rr_moved" if all its contents have been hidden */
	if (reloc_dir && i_ishidden())
		hide_reloc_dir();

	/* insert the boot catalog if required */
	if (use_eltorito)
		insert_boot_cat();

	/*
	 * Free up any matching memory
	 */
	for (n = 0; n < MAX_MAT; n++)
		gen_del_match(n);

#ifdef SORTING
	del_sort();
#endif /* SORTING */

	/*
	 * Sort the directories in the required order (by ISO9660).  Also,
	 * choose the names for the 8.3 filesystem if required, and do any
	 * other post-scan work.
	 */
	goof += sort_tree(root);

	if (goof) {
		fprintf(stderr, "ISO9660/Rock Ridge tree sort failed.\n");
		if(merge_warn_msg)
			fprintf(stderr, merge_warn_msg);
		exit(1);
	}
#ifdef UDF
	if (use_Joliet || use_udf) {
#else
	if (use_Joliet) {
#endif
		goof += joliet_sort_tree(root);
	}
	if (goof) {
		fprintf(stderr, "Joliet tree sort failed. The -joliet-long switch may help you.\n");
		if(merge_warn_msg)
			fprintf(stderr, merge_warn_msg);
		exit(1);
	}
	/*
	 * Fix a couple of things in the root directory so that everything is
	 * self consistent. Fix this up so that the path tables get done right.
	 */
	root->self = root->contents;

	/* OK, ready to write the file.  Open it up, and generate the thing. */
	if (print_size) {
		discimage = fopen("/dev/null", "wb");
		if (!discimage) {
#ifdef	USE_LIBSCHILY
			comerr("Unable to open /dev/null\n");
#else
			fprintf(stderr, "Unable to open /dev/null\n");
			exit(1);
#endif
		}
	} else if (outfile) {
		discimage = fopen(outfile, "wb");
		if (!discimage) {
#ifdef	USE_LIBSCHILY
			comerr("Unable to open disc image file '%s'.\n", outfile);
#else
			fprintf(stderr, "Unable to open disc image file '%s'.\n", outfile);
			exit(1);
#endif
		}
		if (jtemplate_out || jjigdo_out) {
			if (!jtemplate_out || !jjigdo_out || !jmd5_list) {
#ifdef USE_LIBSCHILY
				comerr("Bad options - need to specify output names for jigdo and template file, and also the md5-list input file!\n");
#else
				fprintf(stderr, "Bad options - need to specify output names for jigdo and template file, and also the md5-list input file!\n");
				exit(1);
#endif
			}
			jtjigdo = fopen(jjigdo_out, "wb");
			jttemplate = fopen(jtemplate_out, "wb");
			if (!jtjigdo || !jttemplate) {
#ifdef USE_LIBSCHILY
				comerr("Unable to open jigdo template image file\n");
#else
				fprintf(stderr, "Unable to open jigdo template image file\n");
				exit(1);
#endif
			}
			write_jt_header(jttemplate, jtjigdo);
		}
	} else {
		discimage = stdout;

#ifdef	NEED_O_BINARY
		setmode(fileno(stdout), O_BINARY);
#endif
	}

	/* Now assign addresses on the disc for the path table. */

	path_blocks = ISO_BLOCKS(path_table_size);
	if (path_blocks & 1)
		path_blocks++;

	jpath_blocks = ISO_BLOCKS(jpath_table_size);
	if (jpath_blocks & 1)
		jpath_blocks++;

	/*
	 * Start to set up the linked list that we use to track the contents
	 * of the disc.
	 */
#ifdef APPLE_HYB
#ifdef PREP_BOOT
	if (apple_hyb || use_prep_boot || use_chrp_boot)
#else	/* PREP_BOOT */
	if (apple_hyb)
#endif	/* PREP_BOOT */
		outputlist_insert(&hfs_desc);
#endif	/* APPLE_HYB */
	if (use_sparcboot || use_sunx86boot)
		outputlist_insert(&sunlabel_desc);
	if (use_alphaboot)
		outputlist_insert(&alphaboot_desc);
	if (use_hppaboot)
		outputlist_insert(&hppaboot_desc);
	if (use_alphaboot || use_hppaboot)
		outputlist_insert(&alpha_hppa_boot_desc);
	if (use_mipsboot)
		outputlist_insert(&mipsboot_desc);
	if (use_mipselboot)
		outputlist_insert(&mipselboot_desc);
	if (use_genboot)
		outputlist_insert(&genboot_desc);
	outputlist_insert(&startpad_desc);

	/* PVD for disc. */
	outputlist_insert(&voldesc_desc);

	/* SVD for El Torito. MUST be immediately after the PVD! */
	if (use_eltorito) {
		outputlist_insert(&torito_desc);
	}
	/* Enhanced PVD for disc. neded if we write ISO-9660:1999 */
	if (iso9660_level > 3)
		outputlist_insert(&xvoldesc_desc);

	/* SVD for Joliet. */
	if (use_Joliet) {
		outputlist_insert(&joliet_desc);
	}
	/* Finally the last volume descriptor. */
	outputlist_insert(&end_vol);

#ifdef UDF
	if (use_udf) {
		outputlist_insert(&udf_vol_recognition_area_frag);
	}
#endif

	/* Insert the version descriptor. */
	outputlist_insert(&version_desc);

#ifdef UDF
	if (use_udf) {
		/*
		 * Most of the space before sector 256 is wasted when
		 * UDF is turned on. The waste could be reduced by
		 * putting the ISO9660/Joliet structures before the
		 * pad_to_sector_256; the problem is that they might
		 * overshoot sector 256, so there would have to be some
		 * ugly logic to detect this case and rearrange things
		 * appropriately. I don't know if it's worth it.
		 */
		outputlist_insert(&udf_pad_to_sector_32_frag);
		outputlist_insert(&udf_main_seq_frag);
		outputlist_insert(&udf_main_seq_copy_frag);
		outputlist_insert(&udf_integ_seq_frag);
		outputlist_insert(&udf_pad_to_sector_256_frag);
		outputlist_insert(&udf_anchor_vol_desc_frag);
		outputlist_insert(&udf_file_set_desc_frag);
		outputlist_insert(&udf_dirtree_frag);
		outputlist_insert(&udf_file_entries_frag);
	}
#endif

	/* Now start with path tables and directory tree info. */
	if (!stream_media_size)
		outputlist_insert(&pathtable_desc);
	else
		outputlist_insert(&strpath_desc);

	if (use_Joliet) {
		outputlist_insert(&jpathtable_desc);
	}

	if (!stream_media_size)
		outputlist_insert(&dirtree_desc);

	if (use_Joliet) {
		outputlist_insert(&jdirtree_desc);
	}
	outputlist_insert(&dirtree_clean);

	if (extension_record) {
		outputlist_insert(&extension_desc);
	}

	if (!stream_media_size) {
		outputlist_insert(&files_desc);
	} else {
		outputlist_insert(&strfile_desc);
		outputlist_insert(&strdir_desc);
	}

	/*
	 * Allow room for the various headers we will be writing.
	 * There will always be a primary and an end volume descriptor.
	 */
	last_extent = session_start;

	/*
	 * Calculate the size of all of the components of the disc, and assign
	 * extent numbers.
	 */
	for (opnt = out_list; opnt; opnt = opnt->of_next) {
		opnt->of_start_extent = last_extent;
		if (opnt->of_size != NULL) {
			(*opnt->of_size) (last_extent);
		}
	}

	/*
	 * Generate the contents of any of the sections that we want to
	 * generate. Not all of the fragments will do anything here
	 * - most will generate the data on the fly when we get to the write
	 * pass.
	 */
	for (opnt = out_list; opnt; opnt = opnt->of_next) {
		if (opnt->of_generate != NULL) {
			(*opnt->of_generate) ();
		}
	}

	/*
	 * Padding just after the ISO-9660 filesystem.
	 *
	 * files_desc does not have an of_size function. For this
	 * reason, we must insert us after the files content has been
	 * generated.
	 */
#ifdef UDF
	if (use_udf) {
		/* Single anchor volume descriptor pointer at end */
		outputlist_insert(&udf_end_anchor_vol_desc_frag);
		if (udf_end_anchor_vol_desc_frag.of_size != NULL) {
			(*udf_end_anchor_vol_desc_frag.of_size) (last_extent);
		}
		if (dopad) {
			/*
			 * Pad with anchor volume descriptor pointer
			 * blocks instead of zeroes.
			 */
			outputlist_insert(&udf_padend_avdp_frag);
			if (udf_padend_avdp_frag.of_size != NULL) {
				(*udf_padend_avdp_frag.of_size) (last_extent);
			}
		}
	} else
#endif
	if (dopad && !(use_sparcboot || use_sunx86boot)) {
		outputlist_insert(&endpad_desc);
		if (endpad_desc.of_size != NULL) {
			(*endpad_desc.of_size) (last_extent);
		}
	}
	c = 0;
	if (use_sparcboot) {
		if (dopad) {
			/* Padding before the boot partitions. */
			outputlist_insert(&interpad_desc);
			if (interpad_desc.of_size != NULL) {
				(*interpad_desc.of_size) (last_extent);
			}
		}
		c = make_sun_label();
		last_extent += c;
		outputlist_insert(&sunboot_desc);
		if (dopad) {
			outputlist_insert(&endpad_desc);
			if (endpad_desc.of_size != NULL) {
				(*endpad_desc.of_size) (last_extent);
			}
		}
	} else if (use_sunx86boot) {
		if (dopad) {
			/* Padding before the boot partitions. */
			outputlist_insert(&interpad_desc);
			if (interpad_desc.of_size != NULL) {
				(*interpad_desc.of_size) (last_extent);
			}
		}
		c = make_sunx86_label();
		last_extent += c;
		outputlist_insert(&sunboot_desc);
		if (dopad) {
			outputlist_insert(&endpad_desc);
			if (endpad_desc.of_size != NULL) {
				(*endpad_desc.of_size) (last_extent);
			}
		}
	}
	if (print_size > 0) {
		if (verbose > 0)
			fprintf(stderr,
			"Total extents scheduled to be written = %u\n",
			(last_extent - session_start));
		printf("%u\n", (last_extent - session_start));
		exit(0);
	}
	/*
	 * Now go through the list of fragments and write the data that
	 * corresponds to each one.
	 */
	for (opnt = out_list; opnt; opnt = opnt->of_next) {
		Uint	oext;

		oext = last_extent_written;
		if (opnt->of_start_extent != 0 &&
		    opnt->of_start_extent != last_extent_written) {
			/*
			 * Consistency check.
			 * XXX Should make sure that all entries have
			 * XXXX of_start_extent set up correctly.
			 */
			comerrno(EX_BAD,
			"Implementation botch: %s should start at %u but starts at %u.\n",
			opnt->of_name, opnt->of_start_extent, last_extent_written);
		}
		if (opnt->of_write != NULL) {
			if (verbose > 1)
				fprintf(stderr, "Writing:   %-40sStart Block %u\n",
					opnt->of_name, last_extent_written);
			(*opnt->of_write) (discimage);
			if (verbose > 1)
				fprintf(stderr, "Done with: %-40sBlock(s)    %u\n",
					opnt->of_name, last_extent_written-oext);
		}
	}
	if (last_extent != last_extent_written) {
		comerrno(EX_BAD,
		"Implementation botch: FS should end at %u but ends at %u.\n",
				last_extent, last_extent_written);
	}

	if (jttemplate) {
		write_jt_footer();
		fclose(jttemplate);
	}
	if (jtjigdo)
		fclose(jtjigdo);

	if (verbose > 0) {
#ifdef HAVE_SBRK
		fprintf(stderr, "Max brk space used %x\n",
			(unsigned int)(((unsigned long) sbrk(0)) - mem_start));
#endif
		fprintf(stderr, "%u extents written (%u MB)\n",
			last_extent, last_extent >> 9);
	}
#ifdef VMS
	return (1);
#else
	return (0);
#endif
}

/*
 * Find unescaped equal sign in graft pointer string.
 */
char *
findgequal(char *s)
{
	char	*p = s;

	while ((p = strchr(p, '=')) != NULL) {
		if (p > s && p[-1] != '\\')
			return (p);
		p++;
	}
	return (NULL);
}

/*
 * Find unescaped equal sign in string.
 */
static char *
escstrcpy(char *to, char *from)
{
	char	*p = to;

	if (debug)
		fprintf(stderr, "FROM: '%s'\n", from);

	while ((*p = *from++) != '\0') {
		if (*p == '\\') {
			if ((*p = *from++) == '\0')
				break;
			if (*p != '\\' && *p != '=') {
				p[1] = p[0];
				*p++ = '\\';
			}
		}
		p++;
	}
	if (debug)
		fprintf(stderr, "ESC:  '%s'\n", to);
	return (to);
}

void *
e_malloc(size_t size)
{
	void		*pt = 0;

	if ((size > 0) && ((pt = malloc(size)) == NULL)) {
#ifdef	USE_LIBSCHILY
		comerr("Not enough memory\n");
#else
		fprintf(stderr, "Not enough memory\n");
		exit(1);
#endif
	}
	/*
	 * Not all code is clean yet.
	 * Filling all allocated data with zeroes will help
	 * to avoid core dumps.
	 */
  if (size > 0) /* a workaround for gcc bug gcc.gnu.org/PR25639 */
     memset(pt, 0, size);
	return (pt);
}
