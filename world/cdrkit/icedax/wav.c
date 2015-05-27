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

/* @(#)wav.c	1.4 01/10/27 Copyright 1998,1999 Heiko Eissfeldt */
/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Heiko Eissfeldt
 *
 *
 */
#include "config.h"
#include <stdio.h>
#include <unixstd.h>
#include "byteorder.h"
#include "sndfile.h"

/***
 * ---------------------------------------------------------------------
 *  definitions for RIFF-output (from windows MMSYSTEM)
 * ---------------------------------------------------------------------
 */

typedef unsigned int FOURCC;	/* a four character code */

typedef struct CHUNKHDR {
  FOURCC ckid;		/* chunk ID */
  unsigned int dwSize; 	/* chunk size */
} CHUNKHDR;

/* flags for 'wFormatTag' field of WAVEFORMAT */
#define WAVE_FORMAT_PCM 1

/* specific waveform format structure for PCM data */
typedef struct pcmwaveformat_tag {
  unsigned short wFormatTag;	/* format type */
  unsigned short nChannels;	/* number of channels (i.e. mono, stereo, etc.) */
  unsigned int nSamplesPerSec; /* sample rate */
  unsigned int nAvgBytesPerSec;/* for buffer size estimate */
  unsigned short nBlockAlign;	/* block size of data */
  unsigned short wBitsPerSample;
} PCMWAVEFORMAT;
typedef PCMWAVEFORMAT *PPCMWAVEFORMAT;


/* MMIO macros */
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
  ((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) | \
  ((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24))

#define FOURCC_RIFF	mmioFOURCC ('R', 'I', 'F', 'F')
#define FOURCC_LIST	mmioFOURCC ('L', 'I', 'S', 'T')
#define FOURCC_WAVE	mmioFOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT	mmioFOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA	mmioFOURCC ('d', 'a', 't', 'a')


/* simplified Header for standard WAV files */
typedef struct WAVEHDR {
  CHUNKHDR chkRiff;
  FOURCC fccWave;
  CHUNKHDR chkFmt;
  unsigned short wFormatTag;	/* format type */
  unsigned short nChannels;	/* number of channels (i.e. mono, stereo, etc.) */
  unsigned int nSamplesPerSec; /* sample rate */
  unsigned int nAvgBytesPerSec;/* for buffer estimation */
  unsigned short nBlockAlign;	/* block size of data */
  unsigned short wBitsPerSample;
  CHUNKHDR chkData;
} WAVEHDR;

#define IS_STD_WAV_HEADER(waveHdr) ( \
  waveHdr.chkRiff.ckid == FOURCC_RIFF && \
  waveHdr.fccWave == FOURCC_WAVE && \
  waveHdr.chkFmt.ckid == FOURCC_FMT && \
  waveHdr.chkData.ckid == FOURCC_DATA && \
  waveHdr.wFormatTag == WAVE_FORMAT_PCM)

static WAVEHDR waveHdr;

static int _InitSound(int audio, long channels, unsigned long rate, 
							 long nBitsPerSample, unsigned long expected_bytes);

static int _InitSound(int audio, long channels, unsigned long rate, 
                      long nBitsPerSample, unsigned long expected_bytes)
{
  unsigned long nBlockAlign = channels * ((nBitsPerSample + 7) / 8);
  unsigned long nAvgBytesPerSec = nBlockAlign * rate;
  unsigned long temp = expected_bytes + sizeof(WAVEHDR) - sizeof(CHUNKHDR);

  waveHdr.chkRiff.ckid    = cpu_to_le32(FOURCC_RIFF);
  waveHdr.fccWave         = cpu_to_le32(FOURCC_WAVE);
  waveHdr.chkFmt.ckid     = cpu_to_le32(FOURCC_FMT);
  waveHdr.chkFmt.dwSize   = cpu_to_le32(sizeof (PCMWAVEFORMAT));
  waveHdr.wFormatTag      = cpu_to_le16(WAVE_FORMAT_PCM);
  waveHdr.nChannels       = cpu_to_le16(channels);
  waveHdr.nSamplesPerSec  = cpu_to_le32(rate);
  waveHdr.nBlockAlign     = cpu_to_le16(nBlockAlign);
  waveHdr.nAvgBytesPerSec = cpu_to_le32(nAvgBytesPerSec);
  waveHdr.wBitsPerSample  = cpu_to_le16(nBitsPerSample);
  waveHdr.chkData.ckid    = cpu_to_le32(FOURCC_DATA);
  waveHdr.chkRiff.dwSize  = cpu_to_le32(temp);
  waveHdr.chkData.dwSize  = cpu_to_le32(expected_bytes);

  return write (audio, &waveHdr, sizeof (waveHdr));
}

static int _ExitSound(int audio, unsigned long nBytesDone);

static int _ExitSound(int audio, unsigned long nBytesDone)
{
  unsigned long temp = nBytesDone + sizeof(WAVEHDR) - sizeof(CHUNKHDR);

  waveHdr.chkRiff.dwSize = cpu_to_le32(temp);
  waveHdr.chkData.dwSize = cpu_to_le32(nBytesDone);

  /* goto beginning */
  if (lseek(audio, 0L, SEEK_SET) == -1) {
    return 0;
  }
  return write (audio, &waveHdr, sizeof (waveHdr));
}

static unsigned long _GetHdrSize(void);

static unsigned long _GetHdrSize(void)
{
  return sizeof( waveHdr );
}

static unsigned long InSizeToOutSize(unsigned long BytesToDo);

static unsigned long InSizeToOutSize(unsigned long BytesToDo)
{
	return BytesToDo;
}

struct soundfile wavsound =
{
	_InitSound,		/* init header method */
	_ExitSound,		/* exit header method */
	_GetHdrSize,		/* report header size method */
	/* get sound samples out */
	(int (*)(int audio, unsigned char *buf, unsigned long BytesToDo)) write,
	InSizeToOutSize,	/* compressed? output file size */
	0			/* needs big endian samples */
};


