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

/* @(#)nls_config.c	1.5 05/05/01 2000,2001 J. Schilling */
/*
 *	Modifications to make the code portable Copyright (c) 2000 J. Schilling
 *	This file contains code taken from nls_base.c to avoid loops
 *	in dependency reported by tsort.
 *
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

#include <mconfig.h>
#include "nls.h"

int
init_unls()
{
#ifdef CONFIG_NLS_ISO8859_1
	init_unls_iso8859_1();
#endif
#ifdef CONFIG_NLS_ISO8859_2
	init_unls_iso8859_2();
#endif
#ifdef CONFIG_NLS_ISO8859_3
	init_unls_iso8859_3();
#endif
#ifdef CONFIG_NLS_ISO8859_4
	init_unls_iso8859_4();
#endif
#ifdef CONFIG_NLS_ISO8859_5
	init_unls_iso8859_5();
#endif
#ifdef CONFIG_NLS_ISO8859_6
	init_unls_iso8859_6();
#endif
#ifdef CONFIG_NLS_ISO8859_7
	init_unls_iso8859_7();
#endif
#ifdef CONFIG_NLS_ISO8859_8
	init_unls_iso8859_8();
#endif
#ifdef CONFIG_NLS_ISO8859_9
	init_unls_iso8859_9();
#endif
#ifdef CONFIG_NLS_ISO8859_14
	init_unls_iso8859_14();
#endif
#ifdef CONFIG_NLS_ISO8859_15
	init_unls_iso8859_15();
#endif
#ifdef CONFIG_NLS_CODEPAGE_437
	init_unls_cp437();
#endif
#ifdef CONFIG_NLS_CODEPAGE_737
	init_unls_cp737();
#endif
#ifdef CONFIG_NLS_CODEPAGE_775
	init_unls_cp775();
#endif
#ifdef CONFIG_NLS_CODEPAGE_850
	init_unls_cp850();
#endif
#ifdef CONFIG_NLS_CODEPAGE_852
	init_unls_cp852();
#endif
#ifdef CONFIG_NLS_CODEPAGE_855
	init_unls_cp855();
#endif
#ifdef CONFIG_NLS_CODEPAGE_857
	init_unls_cp857();
#endif
#ifdef CONFIG_NLS_CODEPAGE_860
	init_unls_cp860();
#endif
#ifdef CONFIG_NLS_CODEPAGE_861
	init_unls_cp861();
#endif
#ifdef CONFIG_NLS_CODEPAGE_862
	init_unls_cp862();
#endif
#ifdef CONFIG_NLS_CODEPAGE_863
	init_unls_cp863();
#endif
#ifdef CONFIG_NLS_CODEPAGE_864
	init_unls_cp864();
#endif
#ifdef CONFIG_NLS_CODEPAGE_865
	init_unls_cp865();
#endif
#ifdef CONFIG_NLS_CODEPAGE_866
	init_unls_cp866();
#endif
#ifdef CONFIG_NLS_CODEPAGE_869
	init_unls_cp869();
#endif
#ifdef CONFIG_NLS_CODEPAGE_874
	init_unls_cp874();
#endif
#ifdef CONFIG_NLS_CODEPAGE_1250
	init_unls_cp1250();
#endif
#ifdef CONFIG_NLS_CODEPAGE_1251
	init_unls_cp1251();
#endif
#ifdef CONFIG_NLS_KOI8_R
	init_unls_koi8_r();
#endif
#ifdef CONFIG_NLS_KOI8_U
	init_unls_koi8_u();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10000
	init_unls_cp10000();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10006
	init_unls_cp10006();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10007
	init_unls_cp10007();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10029
	init_unls_cp10029();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10079
	init_unls_cp10079();
#endif
#ifdef CONFIG_NLS_CODEPAGE_10081
	init_unls_cp10081();
#endif
	return (0);
}
