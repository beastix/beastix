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

/* @(#)exitcodes.h	1.1 02/11/21 Copyright 2002 Heiko Eissfeldt */
/* header file for system wide exit codes. */
#ifndef	exitcodes_h
#define	exitcodes_h

#define NO_ERROR	0
#define SYNTAX_ERROR	1
#define PERM_ERROR	2
#define READ_ERROR	3
#define WRITE_ERROR	4
#define SOUND_ERROR	5
#define STAT_ERROR	6
#define SIGPIPE_ERROR	7
#define SETSIG_ERROR	8
#define SHMMEM_ERROR	9
#define NOMEM_ERROR	10
#define MEDIA_ERROR	11
#define DEVICEOPEN_ERROR	12
#define RACE_ERROR	13
#define DEVICE_ERROR	14
#define INTERNAL_ERROR	15
#define SEMAPHORE_ERROR	16
#define SETUPSCSI_ERROR	17
#define PIPE_ERROR	18
#endif
