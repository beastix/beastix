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

/* @(#)interface.h	1.14 06/02/19 Copyright 1998-2001 Heiko Eissfeldt, Copyright 2005-2006 J. Schilling */

/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Heiko Eissfeldt
 *
 * header file interface.h for cdda2wav */

#ifndef CD_FRAMESIZE
#define CD_FRAMESIZE 2048
#endif

#ifndef CD_FRAMESIZE_RAW
#define CD_FRAMESIZE_RAW 2352
#endif

#define CD_FRAMESAMPLES (CD_FRAMESIZE_RAW / 4)

extern unsigned interface;

extern int trackindex_disp;
#ifndef NSECTORS
#define NSECTORS 75
#endif

/* interface types */
#define GENERIC_SCSI	0
#define COOKED_IOCTL	1

/* constants for sub-q-channel info */
#define GET_ALL			0
#define GET_POSITIONDATA	1
#define GET_CATALOGNUMBER	2
#define GET_TRACK_ISRC		3

typedef struct subq_chnl {
    unsigned char reserved;
    unsigned char audio_status;
    unsigned short subq_length;
    unsigned char format;
    unsigned char control_adr;
    unsigned char track;
    unsigned char index;
    unsigned char data[40];	/* this has subq_all, subq_position,
				   subq_catalog or subq_track_isrc format */
} subq_chnl;

typedef struct subq_all {
    unsigned char abs_min;
    unsigned char abs_sec;
    unsigned char abs_frame;
    unsigned char abs_reserved;
    unsigned char trel_min;
    unsigned char trel_sec;
    unsigned char trel_frame;
    unsigned char trel_reserved;
    unsigned char mc_valid;     /* MSB */
    unsigned char media_catalog_number[13];
    unsigned char zero;
    unsigned char aframe;
    unsigned char tc_valid;	/* MSB */
    unsigned char track_ISRC[15];
} subq_all;

typedef struct subq_position {
    unsigned char abs_reserved;
    unsigned char abs_min;
    unsigned char abs_sec;
    unsigned char abs_frame;
    unsigned char trel_reserved;
    unsigned char trel_min;
    unsigned char trel_sec;
    unsigned char trel_frame;
} subq_position;

typedef struct subq_catalog {
    unsigned char mc_valid;	/* MSB */
    unsigned char media_catalog_number[13];
    unsigned char zero;
    unsigned char aframe;
} subq_catalog;

typedef struct subq_track_isrc {
    unsigned char tc_valid;	/* MSB */
    unsigned char track_isrc[15];
} subq_track_isrc;

#if	!defined	NO_SCSI_STUFF

struct TOC;

/* cdrom access function pointer */
extern void     (*EnableCdda)(SCSI *usalp, int Switch, unsigned uSectorsize);
extern unsigned (*doReadToc)(SCSI *usalp);
extern void	(*ReadTocText)(SCSI *usalp);
extern unsigned (*ReadLastAudio)(SCSI *usalp);
extern int      (*ReadCdRom)(SCSI *usalp, UINT4 *p, unsigned lSector, 
									  unsigned SectorBurstVal);
extern int      (*ReadCdRomSub)(SCSI *usalp, UINT4 *p, unsigned lSector, 
										  unsigned SectorBurstVal);
extern int      (*ReadCdRomData)(SCSI *usalp, unsigned char *p, unsigned lSector,
											unsigned SectorBurstVal);
extern subq_chnl *(*ReadSubQ)(SCSI *usalp, unsigned char sq_format, 
										unsigned char track);
extern subq_chnl *(*ReadSubChannels)(SCSI *usalp, unsigned lSector);
extern void     (*SelectSpeed)(SCSI *usalp, unsigned speed);
extern int	(*Play_at)(SCSI *usalp, unsigned from_sector, unsigned sectors);
extern int	(*StopPlay)(SCSI *usalp);
extern void	(*trash_cache)(UINT4 *p, unsigned lSector, unsigned SectorBurstVal);

SCSI    *get_scsi_p(void);
#endif

extern unsigned char *bufferTOC;
extern subq_chnl *SubQbuffer;


void SetupInterface(void);
int	Toshiba3401(void);

void	priv_init(void);
void	priv_on(void);
void	priv_off(void);
