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

/* @(#)vms.h	1.3 04/03/01 eric */
/*
 * Header file genisoimage.h - assorted structure definitions and typecasts.
 *
 *   Written by Eric Youngdale (1993).
 */

#ifdef VMS
#define	stat(X, Y)	VMS_stat(X, Y)
#define	lstat		VMS_stat

/* gmtime not available under VMS - make it look like we are in Greenwich */
#define	gmtime	localtime

extern int	vms_write_one_file(char *filename, off_t size, FILE * outfile);

#endif
