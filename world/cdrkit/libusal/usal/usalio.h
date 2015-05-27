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

/* @(#)usalio.h	2.16 00/11/07 Copyright 1986 J. Schilling */
/*
 *	Definitions for the SCSI general driver 'usal'
 *
 *	Copyright (c) 1986 J. Schilling
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

#ifndef	_SCG_SCGIO_H
#define	_SCG_SCGIO_H

#ifndef	_SCG_SCGCMD_H
#include <usal/usalcmd.h>
#endif

#if	defined(SVR4)
#include <sys/ioccom.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#if	defined(__STDC__) || defined(SVR4)
#define	SCGIOCMD	_IOWR('G', 1, struct usal_cmd)	/* do a SCSI cmd   */
#define	SCGIORESET	_IO('G', 2)			/* reset SCSI bus  */
#define	SCGIOGDISRE	_IOR('G', 4, int)		/* get sc disre Val*/
#define	SCGIOSDISRE	_IOW('G', 5, int)		/* set sc disre Val*/
#define	SCGIOIDBG	_IO('G', 100)			/* Inc Debug Val   */
#define	SCGIODDBG	_IO('G', 101)			/* Dec Debug Val   */
#define	SCGIOGDBG	_IOR('G', 102, int)		/* get Debug Val   */
#define	SCGIOSDBG	_IOW('G', 103, int)		/* set Debug Val   */
#define	SCIOGDBG	_IOR('G', 104, int)		/* get sc Debug Val*/
#define	SCIOSDBG	_IOW('G', 105, int)		/* set sc Debug Val*/
#else
#define	SCGIOCMD	_IOWR(G, 1, struct usal_cmd)	/* do a SCSI cmd   */
#define	SCGIORESET	_IO(G, 2)			/* reset SCSI bus  */
#define	SCGIOGDISRE	_IOR(G, 4, int)			/* get sc disre Val*/
#define	SCGIOSDISRE	_IOW(G, 5, int)			/* set sc disre Val*/
#define	SCGIOIDBG	_IO(G, 100)			/* Inc Debug Val   */
#define	SCGIODDBG	_IO(G, 101)			/* Dec Debug Val   */
#define	SCGIOGDBG	_IOR(G, 102, int)		/* get Debug Val   */
#define	SCGIOSDBG	_IOW(G, 103, int)		/* set Debug Val   */
#define	SCIOGDBG	_IOR(G, 104, int)		/* get sc Debug Val*/
#define	SCIOSDBG	_IOW(G, 105, int)		/* set sc Debug Val*/
#endif

#define	SCGIO_CMD	SCGIOCMD	/* backward ccompatibility */

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCGIO_H */
