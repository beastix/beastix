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

/* @(#)aifc.c	1.5 01/10/27 Copyright 1998,1999 Heiko Eissfeldt */
/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Heiko Eissfeldt
 *
 *
 * ---------------------------------------------------------------------
 *  definitions for aifc pcm output
 * ---------------------------------------------------------------------
 */

#include "config.h"
#include "mytype.h"
#include <stdio.h>
#include <standard.h>
#include <unixstd.h>
#include <strdefs.h>
#include <schily.h>
#include "byteorder.h"
#include "sndfile.h"

typedef UINT4 FOURCC;    /* a four character code */
typedef struct CHUNKHDR {
  FOURCC	ckid;          /* chunk ID */
  UINT4		dwSize;	/* chunk size */
} CHUNKHDR;

#define mmioFOURCC(ch0, ch1, ch2, ch3) \
  ((UINT4)(unsigned char)(ch3) | ((UINT4)(unsigned char)(ch2) << 8) | \
  ((UINT4)(unsigned char)(ch1) << 16) | ((UINT4)(unsigned char)(ch0) << 24))

#define FOURCC_FORM     mmioFOURCC ('F', 'O', 'R', 'M')
#define FOURCC_AIFC     mmioFOURCC ('A', 'I', 'F', 'C')
#define FOURCC_FVER     mmioFOURCC ('F', 'V', 'E', 'R')
#define FOURCC_COMM     mmioFOURCC ('C', 'O', 'M', 'M')
#define FOURCC_NONE     mmioFOURCC ('N', 'O', 'N', 'E')
#define FOURCC_SSND     mmioFOURCC ('S', 'S', 'N', 'D')

#define NO_COMPRESSION	"not compressed"

/* brain dead construction from apple involving bigendian 80-bit doubles.
   Definitely designed not to be portable. Alignment is a nightmare too. */
typedef struct AIFCHDR {
  CHUNKHDR	formChk;
  FOURCC	formType;

  CHUNKHDR	fverChk;		/* Version chunk */
  UINT4		timestamp;		/* timestamp identifies version */

  CHUNKHDR	commChk;		/* Common chunk */
  /* from now on, alignment prevents us from using the original types :-(( */
  unsigned char	numChannels[2];		/* Audio Channels */
  unsigned char	numSampleFrames[4];	/* # of samples */
  unsigned char	samplesize[2];		/* bits per sample */
  unsigned char	sample_rate[10];	/* sample rate in extended float */
  unsigned char	compressionType[4];	/* AIFC extension */
  unsigned char	compressionNameLen;	/* AIFC extension */
  	char	compressionName[sizeof(NO_COMPRESSION)];	/* AIFC extension */

  unsigned char ssndChkid[4];		/* Sound data chunk */
  unsigned char	dwSize[4];		/* size of chunk */
  unsigned char	offset[4];		/* start of 1st sample */
  unsigned char	blocksize[4];		/* aligned sound data block size */

} AIFCHDR;

static AIFCHDR AifcHdr;

/* Prototypes */
static int Format_samplerate(unsigned long rate, unsigned char the_rate[10]);
static int InitSound(int audio, long channels, unsigned long rate, 
							long nBitsPerSample, unsigned long expected_bytes );
static int ExitSound(int audio, unsigned long nBytesDone);
static unsigned long GetHdrSize(void);
static unsigned long InSizeToOutSize(unsigned long BytesToDo);

struct soundfile aifcsound =
{
	InitSound,		/* init header method */
	ExitSound,		/* exit header method */
	GetHdrSize,		/* report header size method */
	/* get sound samples out */
	(int (*)(int audio, unsigned char *buf, unsigned long BytesToDo))write,
	InSizeToOutSize,	/* compressed? output file size */
	1						/* needs big endian samples */
};

/* format the sample rate into an
   bigendian 10-byte IEEE-754 floating point number
 */
static int Format_samplerate(unsigned long rate, unsigned char the_rate[10])
{
  int i;

  /* normalize rate */
  for (i = 0; (rate & 0xffff) != 0; rate <<= 1, i++) {
    if ((rate & 0x8000) != 0) {
      break;
    }
  }

  /* set exponent and sign */
  the_rate[1] = 14-i;
  the_rate[0] = 0x40;		/* LSB = sign */

  /* 16-bit part of mantisse for sample rate */
  the_rate[3] = rate & 0xff;
  the_rate[2] = (rate >> 8) & 0xff;

  /* initialize lower digits of mantisse */
  the_rate[4] = the_rate[5] = the_rate[6] =
  the_rate[7] = the_rate[8] = the_rate[9] = 0;

  return 0;
}


static int InitSound(int audio, long channels, unsigned long rate, 
                     long nBitsPerSample, unsigned long expected_bytes)
{
  UINT4 tmp;

  fillbytes(&AifcHdr, sizeof(AifcHdr), '\0');
  AifcHdr.formChk.ckid	= cpu_to_be32(FOURCC_FORM);
  AifcHdr.formChk.dwSize= cpu_to_be32(expected_bytes +
                        offset_of(AIFCHDR,blocksize)+sizeof(AifcHdr.blocksize)
					- offsetof(AIFCHDR,commChk));
  AifcHdr.formType	= cpu_to_be32(FOURCC_AIFC);

  AifcHdr.fverChk.ckid	= cpu_to_be32(FOURCC_FVER);
  AifcHdr.fverChk.dwSize= cpu_to_be32(offsetof(AIFCHDR,commChk)
					- offsetof(AIFCHDR,timestamp));

  AifcHdr.compressionType[0]='N';
  AifcHdr.compressionType[1]='O';
  AifcHdr.compressionType[2]='N';
  AifcHdr.compressionType[3]='E';
  AifcHdr.compressionNameLen = sizeof(NO_COMPRESSION)-1;
  strcpy(AifcHdr.compressionName, NO_COMPRESSION);
  AifcHdr.timestamp	= cpu_to_be32(UINT4_C(0xA2805140)); /* AIFC Version 1 */

  AifcHdr.commChk.ckid	= cpu_to_be32(FOURCC_COMM);
  AifcHdr.commChk.dwSize= cpu_to_be32(offset_of(AIFCHDR,ssndChkid)
					- offset_of(AIFCHDR,numChannels));

  AifcHdr.numChannels[1]= channels;

  tmp = cpu_to_be32(expected_bytes/(channels * (nBitsPerSample/8)));
  AifcHdr.numSampleFrames[0] = tmp >> 24;
  AifcHdr.numSampleFrames[1] = tmp >> 16;
  AifcHdr.numSampleFrames[2] = tmp >> 8;
  AifcHdr.numSampleFrames[3] = tmp >> 0;
  AifcHdr.samplesize[1]	= nBitsPerSample;
  Format_samplerate(rate, AifcHdr.sample_rate);

  memcpy(AifcHdr.ssndChkid, "SSND", 4);
  tmp = cpu_to_be32(expected_bytes + offset_of(AIFCHDR,blocksize)+sizeof(AifcHdr.blocksize) - offset_of(AIFCHDR, offset));
  AifcHdr.dwSize[0] = tmp >> 24;
  AifcHdr.dwSize[1] = tmp >> 16;
  AifcHdr.dwSize[2] = tmp >> 8;
  AifcHdr.dwSize[3] = tmp >> 0;

  return write (audio, &AifcHdr, sizeof (AifcHdr));
}

static int ExitSound(int audio, unsigned long nBytesDone)
{
  UINT4 tmp;

  AifcHdr.formChk.dwSize= cpu_to_be32(nBytesDone + sizeof(AIFCHDR)
					- offsetof(AIFCHDR,commChk));
  tmp = cpu_to_be32(nBytesDone/(
	AifcHdr.numChannels[1] * AifcHdr.samplesize[1]/ULONG_C(8)));
  AifcHdr.numSampleFrames[0] = tmp >> 24;
  AifcHdr.numSampleFrames[1] = tmp >> 16;
  AifcHdr.numSampleFrames[2] = tmp >> 8;
  AifcHdr.numSampleFrames[3] = tmp >> 0;

  /* If an odd number of bytes has been written,
     extend the chunk with one dummy byte. This is a requirement for AIFC. */
  if ((nBytesDone & 1) && (lseek(audio, 1L, SEEK_CUR) == -1)) {
    return 0;
  }

  /* goto beginning */
  if (lseek(audio, 0L, SEEK_SET) == -1) {
    return 0;
  }
  return write (audio, &AifcHdr, sizeof (AifcHdr));
}

static unsigned long GetHdrSize()
{
  return sizeof( AifcHdr );
}

static unsigned long InSizeToOutSize(unsigned long  BytesToDo)
{
        return BytesToDo;
}

