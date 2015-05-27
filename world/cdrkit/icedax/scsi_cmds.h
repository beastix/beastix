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

/* @(#)scsi_cmds.h	1.11 03/03/02 Copyright 1998,1999 Heiko Eissfeldt */
/* header file for scsi_cmds.c */

extern unsigned char *cmd;
struct TOC;
int SCSI_emulated_ATAPI_on(SCSI *usalp);
unsigned char *Inquiry(SCSI *usalp);
int TestForMedium(SCSI *usalp);
void SpeedSelectSCSIMMC(SCSI *usalp, unsigned speed);
void SpeedSelectSCSIYamaha(SCSI *usalp, unsigned speed);
void SpeedSelectSCSISony(SCSI *usalp, unsigned speed);
void SpeedSelectSCSIPhilipsCDD2600(SCSI *usalp, unsigned speed);
void SpeedSelectSCSINEC(SCSI *usalp, unsigned speed);
void SpeedSelectSCSIToshiba(SCSI *usalp, unsigned speed);
subq_chnl *ReadSubQSCSI(SCSI *usalp, unsigned char sq_format, 
								unsigned char ltrack);
subq_chnl *ReadSubChannelsSony(SCSI *usalp, unsigned lSector);
subq_chnl *ReadSubChannelsFallbackMMC(SCSI *usalp, unsigned lSector);
subq_chnl *ReadStandardSub(SCSI *usalp, unsigned lSector);
int ReadCddaMMC12(SCSI *usalp, UINT4 *p, unsigned lSector, 
						unsigned SectorBurstVal);
int ReadCdda12Matsushita(SCSI *usalp, UINT4 *p, unsigned lSector, 
								 unsigned SectorBurstVal);
int ReadCdda12(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SecorBurstVal);
int ReadCdda10(SCSI *usalp, UINT4 *p, unsigned lSector, unsigned SecorBurstVal);
int ReadStandard(SCSI *usalp, UINT4 *p, unsigned lSector, 
					  unsigned SctorBurstVal);
int ReadStandardData(SCSI *usalp, UINT4 *p, unsigned lSector, 
							unsigned SctorBurstVal);
int ReadCddaFallbackMMC(SCSI *usalp, UINT4 *p, unsigned lSector, 
								unsigned SctorBurstVal);
int ReadCddaSubSony(SCSI *usalp, UINT4 *p, unsigned lSector, 
						  unsigned SectorBurstVal);
int ReadCddaSub96Sony(SCSI *usalp, UINT4 *p, unsigned lSector, 
							 unsigned SectorBurstVal);
int ReadCddaSubMMC12(SCSI *usalp, UINT4 *p, unsigned lSector, 
							unsigned SectorBurstVal);
unsigned ReadTocSony(SCSI *usalp);
unsigned ReadTocMMC(SCSI *usalp);
unsigned ReadTocSCSI(SCSI *usalp);
unsigned ReadFirstSessionTOCSony(SCSI *usalp);
unsigned ReadFirstSessionTOCMMC(SCSI *usalp);
void ReadTocTextSCSIMMC(SCSI *usalp);
int Play_atSCSI(SCSI *usalp, unsigned int from_sector, unsigned int sectors);
int StopPlaySCSI(SCSI *usalp);
void EnableCddaModeSelect(SCSI *usalp, int fAudioMode, unsigned uSectorsize);
int set_sectorsize(SCSI *usalp, unsigned int secsize);
unsigned int
get_orig_sectorsize(SCSI *usalp, unsigned char *m4, unsigned char *m10,
                    unsigned char *m11);
int heiko_mmc(SCSI *usalp);
void init_scsibuf(SCSI *usalp, unsigned amt);
int	myscsierr(SCSI *usalp);

extern int accepts_fua_bit;
extern unsigned char density;
extern unsigned char orgmode4;
extern unsigned char orgmode10, orgmode11;

