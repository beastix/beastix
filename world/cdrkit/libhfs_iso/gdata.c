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

/* @(#)gdata.c	1.1 01/01/21 Copyright 2001 J. Schilling */

#include <mconfig.h>
#include "internal.h"

char *hfs_error = "no error";	/* static error string */
hfsvol *hfs_mounts;		/* linked list of mounted volumes */
hfsvol *hfs_curvol;		/* current volume */
