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

/* @(#)sndconfig.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
#define NONBLOCKING_AUDIO
int set_snd_device(const char *devicename);
int init_soundcard(double rate, int bits);

int open_snd_device(void);
int close_snd_device(void);
int write_snd_device(char *buffer, unsigned length);
