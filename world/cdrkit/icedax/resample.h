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

/* @(#)resample.h	1.3 02/08/02 Copyright 1998,1999 Heiko Eissfeldt */
#define SYNC_SIZE	600	/* has to be smaller than CD_FRAMESAMPLES */

extern int waitforsignal;	/* flag: wait for any audio response */
extern int any_signal;

extern short undersampling;	/* conversion factor */
extern short samples_to_do;	/* loop variable for conversion */
extern int Halved;		/* interpolate due to non integral divider */
extern int jitterShift;		/* track accumulated jitter */
long SaveBuffer(UINT4 *p, unsigned long SecsToDo, unsigned long *BytesDone);
unsigned char *synchronize(UINT4 *p, unsigned SamplesToDo, 
									unsigned TotSamplesDone);
void	handle_inputendianess(UINT4 *p, unsigned SamplesToDo);
