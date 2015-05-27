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

/* @(#)toc.h	1.9 06/02/19 Copyright 1998,1999 Heiko Eissfeldt, Copyright 2006 J. Schilling */

#define	MAXTRK	100	/* maximum of audio tracks (without a hidden track) */

extern	unsigned cdtracks;
extern	int	have_multisession;
extern	int	have_CD_extra;
extern	int	have_CD_text;
extern	int	have_CDDB;

#if	!defined(HAVE_NETDB_H)
#undef	USE_REMOTE
#else
#define	USE_REMOTE	1
extern	int		request_titles(void);
#endif

extern	int		ReadToc(void);
extern	void		Check_Toc(void);
extern	int		TOC_entries(unsigned tracks, 
										unsigned char *a, unsigned char *b,
										int bvalid);
extern	void		toc_entry(unsigned nr, unsigned flag, unsigned tr,
									 unsigned char *ISRC,
									 unsigned long lba, int m, int s, int f);
extern	int		patch_real_end(unsigned long sector);
extern	int		no_disguised_audiotracks(void);

extern	int		Get_Track(unsigned long sector);
extern	long		FirstTrack(void);
extern	long		LastTrack(void);
extern	long		FirstAudioTrack(void);
extern	long		FirstDataTrack(void);
extern	long		LastAudioTrack(void);
extern	long		Get_EndSector(unsigned long p_track);
extern	long		Get_StartSector(unsigned long p_track);
extern	long		Get_AudioStartSector(unsigned long p_track);
extern	long		Get_LastSectorOnCd(unsigned long p_track);
extern	int		CheckTrackrange(unsigned long from, unsigned long upto);

extern	int		Get_Preemphasis(unsigned long p_track);
extern	int		Get_Channels(unsigned long p_track);
extern	int		Get_Copyright(unsigned long p_track);
extern	int		Get_Datatrack(unsigned long p_track);
extern	int		Get_Tracknumber(unsigned long p_track);
extern	unsigned char *Get_MCN(void);
extern	unsigned char *Get_ISRC(unsigned long p_track);

extern	unsigned	find_an_off_sector(unsigned lSector, unsigned SectorBurstVal);
extern	void		DisplayToc(void);
extern	unsigned	FixupTOC(unsigned no_tracks);
extern	void		calc_cddb_id(void);
extern	void		calc_cdindex_id(void);
extern	void		Read_MCN_ISRC(void);
extern	unsigned	ScanIndices(unsigned trackval, unsigned indexval, int bulk);
extern	int		handle_cdtext(void);
extern	int		lba_2_msf(long lba, int *m, int *s, int *f);
