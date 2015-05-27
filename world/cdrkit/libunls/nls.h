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

/* @(#)nls.h	1.7 05/05/01 2000 J. Schilling */
/*
 *	Modifications to make the code portable Copyright (c) 2000 J. Schilling
 *	Thanks to Georgy Salnikov <sge@nmr.nioch.nsc.ru>
 *
 *	Code taken from the Linux kernel.
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

#ifndef	_NLS_H
#define	_NLS_H

#include <unls.h>

#ifndef	NULL
#define	NULL ((void *)0)
#endif

#define	MOD_INC_USE_COUNT
#define	MOD_DEC_USE_COUNT

#define	CONFIG_NLS_CODEPAGE_437
#define	CONFIG_NLS_CODEPAGE_737
#define	CONFIG_NLS_CODEPAGE_775
#define	CONFIG_NLS_CODEPAGE_850
#define	CONFIG_NLS_CODEPAGE_852
#define	CONFIG_NLS_CODEPAGE_855
#define	CONFIG_NLS_CODEPAGE_857
#define	CONFIG_NLS_CODEPAGE_860
#define	CONFIG_NLS_CODEPAGE_861
#define	CONFIG_NLS_CODEPAGE_862
#define	CONFIG_NLS_CODEPAGE_863
#define	CONFIG_NLS_CODEPAGE_864
#define	CONFIG_NLS_CODEPAGE_865
#define	CONFIG_NLS_CODEPAGE_866
#define	CONFIG_NLS_CODEPAGE_869
#define	CONFIG_NLS_CODEPAGE_874
#define	CONFIG_NLS_CODEPAGE_1250
#define	CONFIG_NLS_CODEPAGE_1251
#define	CONFIG_NLS_ISO8859_1
#define	CONFIG_NLS_ISO8859_2
#define	CONFIG_NLS_ISO8859_3
#define	CONFIG_NLS_ISO8859_4
#define	CONFIG_NLS_ISO8859_5
#define	CONFIG_NLS_ISO8859_6
#define	CONFIG_NLS_ISO8859_7
#define	CONFIG_NLS_ISO8859_8
#define	CONFIG_NLS_ISO8859_9
#define	CONFIG_NLS_ISO8859_14
#define	CONFIG_NLS_ISO8859_15
#define	CONFIG_NLS_KOI8_R
#define	CONFIG_NLS_KOI8_U

#define	CONFIG_NLS_CODEPAGE_10000
#define	CONFIG_NLS_CODEPAGE_10006
#define	CONFIG_NLS_CODEPAGE_10007
#define	CONFIG_NLS_CODEPAGE_10029
#define	CONFIG_NLS_CODEPAGE_10079
#define	CONFIG_NLS_CODEPAGE_10081

extern int init_unls_iso8859_1(void);
extern int init_unls_iso8859_2(void);
extern int init_unls_iso8859_3(void);
extern int init_unls_iso8859_4(void);
extern int init_unls_iso8859_5(void);
extern int init_unls_iso8859_6(void);
extern int init_unls_iso8859_7(void);
extern int init_unls_iso8859_8(void);
extern int init_unls_iso8859_9(void);
extern int init_unls_iso8859_14(void);
extern int init_unls_iso8859_15(void);
extern int init_unls_cp437(void);
extern int init_unls_cp737(void);
extern int init_unls_cp775(void);
extern int init_unls_cp850(void);
extern int init_unls_cp852(void);
extern int init_unls_cp855(void);
extern int init_unls_cp857(void);
extern int init_unls_cp860(void);
extern int init_unls_cp861(void);
extern int init_unls_cp862(void);
extern int init_unls_cp863(void);
extern int init_unls_cp864(void);
extern int init_unls_cp865(void);
extern int init_unls_cp866(void);
extern int init_unls_cp869(void);
extern int init_unls_cp874(void);
extern int init_unls_cp1250(void);
extern int init_unls_cp1251(void);
extern int init_unls_koi8_r(void);
extern int init_unls_koi8_u(void);

extern int init_unls_cp10000(void);
extern int init_unls_cp10006(void);
extern int init_unls_cp10007(void);
extern int init_unls_cp10029(void);
extern int init_unls_cp10079(void);
extern int init_unls_cp10081(void);
extern int init_unls_file(char *name);

#ifdef USE_ICONV
extern int init_nls_iconv(char *name);
#endif

#endif	/* _NLS_H */
