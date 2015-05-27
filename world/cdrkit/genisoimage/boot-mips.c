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
 * Program boot-mips.c - Handle big-endian boot extensions to iso9660.
 *
 * Written by Steve McIntyre <steve@einval.com> June 2004
 *
 * Heavily inspired by / borrowed from genisovh:
 *
 * Copyright: (C) 2002 by Florian Lohoff <flo@rfc822.org>
 *            (C) 2004 by Thiemo Seufer <seufer@csv.ica.uni-stuttgart.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, Version 2, as published by the
 * Free Software Foundation.
 *
 * Format for volume header information
 *
 * The volume header is a block located at the beginning of all disk
 * media (sector 0).  It contains information pertaining to physical
 * device parameters and logical partition information.
 *
 * The volume header is manipulated by disk formatters/verifiers,
 * partition builders (e.g. fx, dvhtool, and mkfs), and disk drivers.
 *
 * Previous versions of IRIX wrote a copy of the volume header is
 * located at sector 0 of each track of cylinder 0.  These copies were
 * never used, and reduced the capacity of the volume header to hold large
 * files, so this practice was discontinued.
 * The volume header is constrained to be less than or equal to 512
 * bytes long.  A particular copy is assumed valid if no drive errors
 * are detected, the magic number is correct, and the 32 bit 2's complement
 * of the volume header is correct.  The checksum is calculated by initially
 * zeroing vh_csum, summing the entire structure and then storing the
 * 2's complement of the sum.  Thus a checksum to verify the volume header
 * should be 0.
 *
 * The error summary table, bad sector replacement table, and boot blocks are
 * located by searching the volume directory within the volume header.
 *
 * Tables are sized simply by the integral number of table records that
 * will fit in the space indicated by the directory entry.
 *
 * The amount of space allocated to the volume header, replacement blocks,
 * and other tables is user defined when the device is formatted.
 */

#include <inttypes.h>
#ifndef MIN
#define MIN(a,b) ( (a<b) ? a : b )
#endif

/*
 * device parameters are in the volume header to determine mapping
 * from logical block numbers to physical device addresses
 *
 * Linux doesn't care ...
 */
struct device_parameters {
	uint8_t dp_skew;	/* spiral addressing skew */
	uint8_t dp_gap1;	/* words of 0 before header */
	uint8_t dp_gap2;	/* words of 0 between hdr and data */
	uint8_t dp_spares_cyl;	/* This is for drives (such as SCSI
		that support zone oriented sparing, where the zone is larger
		than one track.  It gets subracteded from the cylinder size
		( dp_trks0 * dp_sec) when doing partition size calculations */
	uint16_t dp_cyls;	/* number of usable cylinders (i.e.,
		doesn't include cylinders reserved by the drive for badblocks,
		etc.). For drives with variable geometry, this number may be
		decreased so that:
		dp_cyls * ((dp_heads * dp_trks0) - dp_spares_cyl) <= actualcapacity
		This happens on SCSI drives such as the Wren IV and Toshiba 156
		Also see dp_cylshi below */
	uint16_t dp_shd0;	/* starting head vol 0 */
	uint16_t dp_trks0;	/* number of tracks / cylinder vol 0*/
	uint8_t dp_ctq_depth;	/* Depth of CTQ queue */
	uint8_t dp_cylshi;	/* high byte of 24 bits of cylinder count */
	uint16_t dp_unused;	/* not used */
	uint16_t dp_secs;	/* number of sectors/track */
	uint16_t dp_secbytes;	/* length of sector in bytes */
	uint16_t dp_interleave;	/* sector interleave */
	int32_t dp_flags;		/* controller characteristics */
	int32_t dp_datarate;		/* bytes/sec for kernel stats */
	int32_t dp_nretries;		/* max num retries on data error */
	int32_t dp_mspw;		/* ms per word to xfer, for iostat */
	uint16_t dp_xgap1;	/* Gap 1 for xylogics controllers */
	uint16_t dp_xsync;    /* sync delay for xylogics controllers */
	uint16_t dp_xrdly;    /* read delay for xylogics controllers */
	uint16_t dp_xgap2;    /* gap 2 for xylogics controllers */
	uint16_t dp_xrgate;   /* read gate for xylogics controllers */
	uint16_t dp_xwcont;   /* write continuation for xylogics */
};

/*
 * Device characterization flags
 * (dp_flags)
 */
#define	DP_SECTSLIP	0x00000001	/* sector slip to spare sector */
#define	DP_SECTFWD	0x00000002	/* forward to replacement sector */
#define	DP_TRKFWD	0x00000004	/* forward to replacement track */
#define	DP_MULTIVOL	0x00000008	/* multiple volumes per spindle */
#define	DP_IGNOREERRORS	0x00000010	/* transfer data regardless of errors */
#define DP_RESEEK	0x00000020	/* recalibrate as last resort */
#define	DP_CTQ_EN	0x00000040	/* enable command tag queueing */

/*
 * Boot blocks, bad sector tables, and the error summary table, are located
 * via the volume_directory.
 */
#define VDNAMESIZE	8

struct volume_directory {
	int8_t  vd_name[VDNAMESIZE];	/* name */
	int32_t vd_lbn;			/* logical block number */
	int32_t vd_nbytes;		/* file length in bytes */
};

/*
 * partition table describes logical device partitions
 * (device drivers examine this to determine mapping from logical units
 * to cylinder groups, device formatters/verifiers examine this to determine
 * location of replacement tracks/sectors, etc)
 *
 * NOTE: pt_firstlbn SHOULD BE CYLINDER ALIGNED
 */
struct partition_table {		/* one per logical partition */
	int32_t pt_nblks;		/* # of logical blks in partition */
	int32_t pt_firstlbn;		/* first lbn of partition */
	int32_t pt_type;		/* use of partition */
};

#define	PTYPE_VOLHDR	0		/* partition is volume header */
#define	PTYPE_TRKREPL	1		/* partition is used for repl trks */
#define	PTYPE_SECREPL	2		/* partition is used for repl secs */
#define	PTYPE_RAW	3		/* partition is used for data */
#define	PTYPE_BSD42	4		/* partition is 4.2BSD file system */
#define	PTYPE_BSD	4		/* partition is 4.2BSD file system */
#define	PTYPE_SYSV	5		/* partition is SysV file system */
#define	PTYPE_VOLUME	6		/* partition is entire volume */
#define	PTYPE_EFS	7		/* partition is sgi EFS */
#define	PTYPE_LVOL	8		/* partition is part of a logical vol */
#define	PTYPE_RLVOL	9		/* part of a "raw" logical vol */
#define	PTYPE_XFS	10		/* partition is sgi XFS */
#define	PTYPE_XFSLOG	11		/* partition is sgi XFS log */
#define	PTYPE_XLV	12		/* partition is part of an XLV vol */
#define	PTYPE_XVM	13		/* partition is sgi XVM */
#define	PTYPE_LSWAP	0x82		/* partition is Linux swap */
#define	PTYPE_LINUX	0x83		/* partition is Linux native */
#define NPTYPES		16

#define	VHMAGIC		0xbe5a941	/* randomly chosen value */
#define	NPARTAB		16		/* 16 unix partitions */
#define	NVDIR		15		/* max of 15 directory entries */
#define BFNAMESIZE	16		/* max 16 chars in boot file name */

/* Partition types for ARCS */
#define NOT_USED        0       /* Not used 				*/
#define FAT_SHORT       1       /* FAT filesystem, 12-bit FAT entries 	*/
#define FAT_LONG        4       /* FAT filesystem, 16-bit FAT entries 	*/
#define EXTENDED        5       /* extended partition 			*/
#define HUGE            6       /* huge partition- MS/DOS 4.0 and later */

/* Active flags for ARCS */
#define BOOTABLE        0x00;
#define NOT_BOOTABLE    0x80;

struct volume_header {
	int32_t vh_magic; /* identifies volume header */
	int16_t vh_rootpt; /* root partition number */
	int16_t vh_swappt; /* swap partition number */
	int8_t vh_bootfile[BFNAMESIZE]; /* name of file to boot */
	struct device_parameters vh_dp; /* device parameters */
	struct volume_directory vh_vd[NVDIR]; /* other vol hdr contents */
	struct partition_table vh_pt[NPARTAB]; /* device partition layout */
	int32_t vh_csum; /* volume header checksum */
	int32_t vh_fill; /* fill out to 512 bytes */
    char pad[1536];  /* pad out to 2048 */
};

#include <mconfig.h>
#include "genisoimage.h"
#include <fctldefs.h>
#include <utypes.h>
#include <intcvt.h>
#include "match.h"
#include "diskmbr.h"
#include "bootinfo.h"
#include <schily.h>
#include "endianconv.h"

int     add_boot_mips_filename(char *filename);

static  int     boot_mips_write(FILE *outfile);

#define MAX_NAMES 15
static char *boot_mips_filename[MAX_NAMES] =
{
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL, NULL
};

static int boot_mips_num_files = 0;

#define SECTORS_PER_TRACK	32
#define BYTES_PER_SECTOR	512

int add_boot_mips_filename(char *filename)
{
    if (boot_mips_num_files < MAX_NAMES)
    {
        boot_mips_filename[boot_mips_num_files] = filename;
        boot_mips_num_files++;
    }

    else
    {
#ifdef	USE_LIBSCHILY
        comerrno(EX_BAD, "Too many MIPS boot files!\n");
#else
        fprintf(stderr, "Too many MIPS boot files!\n");
        exit(1);
#endif
    }
    return 0;
}

static void vh_calc_checksum(struct volume_header *vh)
{
	uint32_t newsum = 0;
	unsigned char *buffer = (unsigned char *)vh;
	unsigned int i;

	vh->vh_csum = 0;

	for(i = 0; i < sizeof(struct volume_header); i += 4)
        newsum -= read_be32(&buffer[i]);

    write_be32(newsum, (unsigned char *)&vh->vh_csum);
}

static char *file_base_name(char *path)
{
    char *endptr = path;
    char *ptr = path;
    
    while (*ptr != '\0')
    {
        if ('/' == *ptr)
            endptr = ++ptr;
        else
            ++ptr;
    }
    return endptr;
}

static int boot_mips_write(FILE *outfile)
{
	struct directory_entry	*boot_file;	/* Boot file we need to search for */
    unsigned long length = 0;
    unsigned long extent = 0;
	int i;
	struct volume_header vh;
    unsigned long long iso_size = 0;
    char *filename = NULL;

	memset(&vh, 0, sizeof(vh));

    iso_size = last_extent * 2048;

    write_be32(VHMAGIC, (unsigned char *)&vh.vh_magic);

	/* Values from an IRIX cd */
    write_be16(BYTES_PER_SECTOR, (unsigned char *)&vh.vh_dp.dp_secbytes);
    write_be16(SECTORS_PER_TRACK, (unsigned char *)&vh.vh_dp.dp_secs);
    write_be32(DP_RESEEK|DP_IGNOREERRORS|DP_TRKFWD, (unsigned char *)&vh.vh_dp.dp_flags);
    write_be16(1, (unsigned char *)&vh.vh_dp.dp_trks0);

    write_be16((iso_size + BYTES_PER_SECTOR - 1) / (SECTORS_PER_TRACK * BYTES_PER_SECTOR),
               (unsigned char *)&vh.vh_dp.dp_cyls);

	for(i = 0; i < boot_mips_num_files; i++)
    {
        boot_file = search_tree_file(root, boot_mips_filename[i]);
        
        if (!boot_file) {
#ifdef	USE_LIBSCHILY
            comerrno(EX_BAD, "Uh oh, I cant find the MIPS boot file '%s'!\n",
                     boot_mips_filename[i]);
#else
            fprintf(stderr, "Uh oh, I cant find the MIPS boot file '%s'!\n",
                    boot_mips_filename[i]);
            exit(1);
#endif
        }

        extent = get_733(boot_file->isorec.extent) * 4;
        length = ((get_733(boot_file->isorec.size) + 2047) / 2048) * 2048;
        filename = file_base_name(boot_mips_filename[i]);

        strncpy(vh.vh_vd[i].vd_name, filename, MIN(VDNAMESIZE, strlen(filename)));
        write_be32(extent, (unsigned char *)&vh.vh_vd[i].vd_lbn);
        write_be32(length, (unsigned char *)&vh.vh_vd[i].vd_nbytes);
        
        fprintf(stderr, "Found mips boot image %s, using extent %lu (0x%lX), #blocks %lu (0x%lX)\n",
                filename, extent, extent, length, length);
	}

	/* Create volume partition on whole cd iso */
    write_be32((iso_size + (BYTES_PER_SECTOR - 1))/ BYTES_PER_SECTOR, (unsigned char *)&vh.vh_pt[10].pt_nblks);
    write_be32(0, (unsigned char *)&vh.vh_pt[10].pt_firstlbn);
    write_be32(PTYPE_VOLUME, (unsigned char *)&vh.vh_pt[10].pt_type);

	/* Create volume header partition, also on WHOLE cd iso */
    write_be32((iso_size + (BYTES_PER_SECTOR - 1))/ BYTES_PER_SECTOR, (unsigned char *)&vh.vh_pt[8].pt_nblks);
    write_be32(0, (unsigned char *)&vh.vh_pt[8].pt_firstlbn);
    write_be32(PTYPE_VOLHDR, (unsigned char *)&vh.vh_pt[8].pt_type);

	/* Create checksum */
	vh_calc_checksum(&vh);

    jtwrite(&vh, sizeof(vh), 1, 0, FALSE);
    xfwrite(&vh, sizeof(vh), 1, outfile, 0, FALSE);
    last_extent_written++;

	return 0;
}

struct output_fragment mipsboot_desc = {NULL, oneblock_size, NULL, boot_mips_write, "MIPS boot block"};
