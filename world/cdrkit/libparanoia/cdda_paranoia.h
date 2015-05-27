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

/* @(#)cdda_paranoia.h	1.20 04/02/20 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
/*
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 *
 */

#ifndef	_CDROM_PARANOIA_H
#define	_CDROM_PARANOIA_H

#ifndef _MCONFIG_H
#include <mconfig.h>
#endif
#ifndef _UTYPES_H
#include <utypes.h>
#endif

#ifndef	__GNUC__
#define	inline
#endif

#define	CD_FRAMESIZE_RAW		2352
#define	CD_FRAMEWORDS			(CD_FRAMESIZE_RAW/2)

/*
 * Second parameter of the callback function
 */
#define	PARANOIA_CB_READ		 0	/* Read off adjust ??? */
#define	PARANOIA_CB_VERIFY		 1	/* Verifying jitter */
#define	PARANOIA_CB_FIXUP_EDGE		 2	/* Fixed edge jitter */
#define	PARANOIA_CB_FIXUP_ATOM		 3	/* Fixed atom jitter */
#define	PARANOIA_CB_SCRATCH		 4	/* Unsupported */
#define	PARANOIA_CB_REPAIR		 5	/* Unsupported */
#define	PARANOIA_CB_SKIP		 6	/* Skip exhausted retry */
#define	PARANOIA_CB_DRIFT		 7	/* Drift detected */
#define	PARANOIA_CB_BACKOFF		 8	/* Unsupported */
#define	PARANOIA_CB_OVERLAP		 9	/* Dyn Overlap adjust */
#define	PARANOIA_CB_FIXUP_DROPPED	10	/* Fixed dropped bytes */
#define	PARANOIA_CB_FIXUP_DUPED		11	/* Fixed duplicate bytes */
#define	PARANOIA_CB_READERR		12	/* Hard read error */

/*
 * Cdparanoia modes to be set with paranoia_modeset()
 */
#define	PARANOIA_MODE_FULL		 0xFF
#define	PARANOIA_MODE_DISABLE		 0

#define	PARANOIA_MODE_VERIFY		 1	/* Verify data integrity in overlap area */
#define	PARANOIA_MODE_FRAGMENT		 2	/* unsupported */
#define	PARANOIA_MODE_OVERLAP		 4	/* Perform overlapped reads */
#define	PARANOIA_MODE_SCRATCH		 8	/* unsupported */
#define	PARANOIA_MODE_REPAIR		16	/* unsupported */
#define	PARANOIA_MODE_NEVERSKIP		32	/* Do not skip failed reads (retry maxretries) */


#ifndef	CDP_COMPILE
typedef	void    cdrom_paranoia;
#endif

/*
 * The interface from libcdparanoia to the high level caller
 */
extern cdrom_paranoia	*paranoia_init(void * d, int nsectors);
extern void	paranoia_dynoverlapset(cdrom_paranoia * p, int minoverlap, 
											  int maxoverlap);
extern void	paranoia_modeset(cdrom_paranoia * p, int mode);
extern long	paranoia_seek(cdrom_paranoia * p, long seek, int mode);
extern Int16_t	*paranoia_read(cdrom_paranoia * p, void (*callback) (long, int));
extern Int16_t	*paranoia_read_limited(cdrom_paranoia * p, 
												  void (*callback) (long, int), 
												  int maxretries);
extern void	paranoia_free(cdrom_paranoia * p);
extern void	paranoia_overlapset(cdrom_paranoia * p, long overlap);

#ifndef	HAVE_MEMMOVE
#define	memmove(dst, src, size)		movebytes((src), (dst), (size))
#endif


/*
 * The callback interface from libparanoia to the CD-ROM interface
 */
extern long	cdda_disc_firstsector(void *d);		/* -> long sector */
extern long	cdda_disc_lastsector(void *d);		/* -> long sector */
/* -> long sectors */
extern long	cdda_read(void *d, void *buffer, long beginsector, long sectors);	
extern int	cdda_sector_gettrack(void *d, long sector);	/* -> int trackno */
extern int	cdda_track_audiop(void *d, int track);	/* -> int Is audiotrack */
extern long	cdda_track_firstsector(void *d, int track);	/* -> long sector */
extern long	cdda_track_lastsector(void *d, int track);	/* -> long sector */
extern int	cdda_tracks(void *d);		/* -> int tracks */

#endif	/* _CDROM_PARANOIA_H */
