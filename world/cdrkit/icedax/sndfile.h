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

/* @(#)sndfile.h	1.4 06/02/19 Copyright 1998,1999 Heiko Eissfeldt, Copyright 2006 J. Schilling */

/*
 * generic soundfile structure
 */

#ifndef	_SNDFILE_H
#define	_SNDFILE_H

#include <utypes.h>

struct soundfile {
	int	(* InitSound)(int audio, long channels, Ulong rate,
							  long nBitsPerSample,
							  Ulong expected_bytes);
	int	(* ExitSound)(int audio, Ulong nBytesDone);
	Ulong	(* GetHdrSize)(void);
	int	(* WriteSound)(int audio, unsigned char *buf, Ulong BytesToDo);
	Ulong	(* InSizeToOutSize)(Ulong BytesToDo);

	int	need_big_endian;
};

#endif	/* _SNDFILE_H */
