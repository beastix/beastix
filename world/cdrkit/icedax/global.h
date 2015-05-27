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

/* @(#)global.h	1.11 04/07/29 Copyright 1998-2004 Heiko Eissfeldt */
/* Global Variables */

#ifdef  MD5_SIGNATURES
#include "md5.h"
#endif
#ifdef	USE_PARANOIA
#include "cdda_paranoia.h"
#endif

typedef struct index_list
{
	struct index_list	*next;
	int			frameoffset;
}
index_list;

typedef struct global
{

	char			*dev_name;		/* device name */
	char			*aux_name;		/* device name */
	char			fname_base[200];

	int			have_forked;
	int			parent_died;
	int			audio;
	struct soundfile	*audio_out;
	int			cooked_fd;
	int			no_file;
	int			no_infofile;
	int			no_cddbfile;
	int			quiet;
	int			verbose;
	int			scsi_silent;
	int			scsi_verbose;
	int			scanbus;
	int			scandevs;
	int			multiname;
	int			sh_bits;
	int			Remainder;
	int			SkippedSamples;
	int			OutSampleSize;
	int			need_big_endian;
	int			need_hostorder;
	int			channels;
	unsigned long		iloop;
	unsigned long		nSamplesDoneInTrack;
	unsigned		overlap;
	int			useroverlap;
	unsigned		nsectors;
	unsigned		buffers;
	unsigned		shmsize;
	long			pagesize;
	int			in_lendian;
	int			outputendianess;
	int			findminmax;
	int			maxamp[2];
	int			minamp[2];
	unsigned		speed;
	int			userspeed;
	int			ismono;
	int			findmono;
	int			swapchannels;
	int			deemphasize;
	int			gui;
	long			playback_rate;
	int			target; /* SCSI Id to be used */
	int			lun;    /* SCSI Lun to be used */
	UINT4			cddb_id;
	int			cddbp;
	char *			cddbp_server;
	char *			cddbp_port;
	unsigned		cddb_revision;
	int			cddb_year;
	char			cddb_genre[60];
	int			illleadout_cd;
	int			reads_illleadout;
	unsigned char		*cdindex_id;
	unsigned char		*creator;
	unsigned char		*copyright_message;
	unsigned char		*disctitle;
	unsigned char		*tracktitle[100];
	unsigned char		*trackcreator[100];
	index_list		*trackindexlist[100];

	int			paranoia_selected;
#ifdef	USE_PARANOIA
	cdrom_paranoia  	*cdp;

	struct paranoia_parms_t
	{
	        Ucbit	disable_paranoia:1;
	        Ucbit	disable_extra_paranoia:1;
	        Ucbit	disable_scratch_detect:1;
	        Ucbit	disable_scratch_repair:1;
		int	retries;
		int	overlap;
		int	mindynoverlap;
		int	maxdynoverlap;
	}
	paranoia_parms;
#endif

	unsigned		md5blocksize;
#ifdef	MD5_SIGNATURES
	int			md5count;
	MD5_CTX			context;
	unsigned char		MD5_result[16];
#endif

#ifdef	ECHO_TO_SOUNDCARD
	int			soundcard_fd;
#endif
	int			echo;

	int			just_the_toc;
}
global_t;

extern global_t global;
