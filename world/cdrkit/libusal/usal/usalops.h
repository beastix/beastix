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

/* @(#)usalops.h	1.5 02/10/19 Copyright 2000 J. Schilling */
/*
 *	Copyright (c) 2000 J. Schilling
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

#ifndef	_SCG_SCGOPS_H
#define	_SCG_SCGOPS_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct usal_ops {
	int	(*usalo_send)(SCSI *usalp);

	char *	(*usalo_version)(SCSI *usalp, int what);
#ifdef	EOF	/* stdio.h has been included */
	int	(*usalo_help)(SCSI *usalp, FILE *f);
#else
	int	(*usalo_help)(SCSI *usalp, void *f);
#endif
	int	(*usalo_open)(SCSI *usalp, char *device);
	int	(*usalo_close)(SCSI *usalp);
	long	(*usalo_maxdma)(SCSI *usalp, long amt);
	void *	(*usalo_getbuf)(SCSI *usalp, long amt);
	void	(*usalo_freebuf)(SCSI *usalp);


	BOOL	(*usalo_havebus)(SCSI *usalp, int busno);
	int	(*usalo_fileno)(SCSI *usalp, int busno, int tgt, int tlun);

	int	(*usalo_initiator_id)(SCSI *usalp);
	int	(*usalo_isatapi)(SCSI *usalp);
	int	(*usalo_reset)(SCSI *usalp, int what);

	char *	(*usalo_natname)(SCSI *usalp, int busno, int tgt, int tlun);
} usal_ops_t;

#define	SCGO_SEND(usalp)				(*(usalp)->ops->usalo_send)(usalp)
#define	SCGO_VERSION(usalp, what)		(*(usalp)->ops->usalo_version)(usalp, what)
#define	SCGO_HELP(usalp, f)			(*(usalp)->ops->usalo_help)(usalp, f)
#define	SCGO_OPEN(usalp, device)			(*(usalp)->ops->usalo_open)(usalp, device)
#define	SCGO_CLOSE(usalp)			(*(usalp)->ops->usalo_close)(usalp)
#define	SCGO_MAXDMA(usalp, amt)			(*(usalp)->ops->usalo_maxdma)(usalp, amt)
#define	SCGO_GETBUF(usalp, amt)			(*(usalp)->ops->usalo_getbuf)(usalp, amt)
#define	SCGO_FREEBUF(usalp)			(*(usalp)->ops->usalo_freebuf)(usalp)
#define	SCGO_HAVEBUS(usalp, busno)		(*(usalp)->ops->usalo_havebus)(usalp, busno)
#define	SCGO_FILENO(usalp, busno, tgt, tlun)	(*(usalp)->ops->usalo_fileno)(usalp, busno, tgt, tlun)
#define	SCGO_INITIATOR_ID(usalp)			(*(usalp)->ops->usalo_initiator_id)(usalp)
#define	SCGO_ISATAPI(usalp)			(*(usalp)->ops->usalo_isatapi)(usalp)
#define	SCGO_RESET(usalp, what)			(*(usalp)->ops->usalo_reset)(usalp, what)

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCGOPS_H */
