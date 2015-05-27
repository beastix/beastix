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

/* @(#)scsidefs.h	1.28 04/09/04 Copyright 1988 J. Schilling */
/*
 *	Definitions for SCSI devices i.e. for error strings in scsierrs.c
 *
 *	Copyright (c) 1988 J. Schilling
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

#ifndef	_SCG_SCSIDEFS_H
#define	_SCG_SCSIDEFS_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Disks
 */
#ifdef	DEV_UNKNOWN
/*
 * True64 defines DEV_UNKNOWN in /usr/include/sys/devio.h as "UNKNOWN"
 */
#undef	DEV_UNKNOWN
#endif
#define	DEV_UNKNOWN		0
#define	DEV_ACB40X0		1
#define	DEV_ACB4000		2
#define	DEV_ACB4010		3
#define	DEV_ACB4070		4
#define	DEV_ACB5500		5
#define	DEV_ACB4520A		6
#define	DEV_ACB4525		7
#define	DEV_MD21		8
#define	DEV_MD23		9
#define	DEV_NON_CCS_DSK		10
#define	DEV_CCS_GENDISK		11

/*
 * Tapes
 */
#define	DEV_MT02		100
#define	DEV_SC4000		101

/*
 * Printer
 */
#define	DEV_PRT			200

/*
 * Processors
 */
#define	DEV_PROC		300

/*
 * Worm
 */
#define	DEV_WORM		400
#define	DEV_RXT800S		401

/*
 * CD-ROM
 */
#define	DEV_CDROM		500
#define	DEV_MMC_CDROM		501
#define	DEV_MMC_CDR		502
#define	DEV_MMC_CDRW		503
#define	DEV_MMC_DVD		504
#define	DEV_MMC_DVD_WR		505

#define	DEV_CDD_521_OLD		510
#define	DEV_CDD_521		511
#define	DEV_CDD_522		512
#define	DEV_PCD_600		513
#define	DEV_CDD_2000		514
#define	DEV_CDD_2600		515
#define	DEV_TYUDEN_EW50		516
#define	DEV_YAMAHA_CDR_100	520
#define	DEV_YAMAHA_CDR_400	521
#define	DEV_PLASMON_RF_4100	530
#define	DEV_SONY_CDU_924	540
#define	DEV_RICOH_RO_1420C	550
#define	DEV_RICOH_RO_1060C	551
#define	DEV_TEAC_CD_R50S	560
#define	DEV_MATSUSHITA_7501	570
#define	DEV_MATSUSHITA_7502	571
#define	DEV_PIONEER_DW_S114X	580
#define	DEV_PIONEER_DVDR_S101	581

/*
 * Scanners
 */
#define	DEV_HRSCAN		600
#define	DEV_MS300A		601

/*
 * Optical memory
 */
#define	DEV_SONY_SMO		700


#define	old_acb(d)	(((d) == DEV_ACB40X0) || \
			    ((d) == DEV_ACB4000) || ((d) == DEV_ACB4010) || \
			    ((d) == DEV_ACB4070) || ((d) == DEV_ACB5500))

#define	is_ccs(d)	(!old_acb(d))

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCSIDEFS_H */
