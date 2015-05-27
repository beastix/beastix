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

/* @(#)lowlevel.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
/* os dependent functions */

#ifndef LOWLEVEL
# define LOWLEVEL 1

# if defined(__linux__)
#  include <linux/version.h>
#  include <linux/major.h>

# endif /* defined __linux__ */

#endif /* ifndef LOWLEVEL */
