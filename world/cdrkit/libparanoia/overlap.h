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

/* @(#)overlap.h	1.7 04/02/18 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
/*
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 */

#ifndef	_OVERLAP_H_
#define	_OVERLAP_H_

extern	void	paranoia_resetcache(cdrom_paranoia *p);
extern	void	paranoia_resetall(cdrom_paranoia *p);
extern	void	i_paranoia_trim(cdrom_paranoia *p, long beginword, long endword);
extern	void	offset_adjust_settings(cdrom_paranoia *p,
												  void (*callback) (long, int));
extern	void	offset_add_value(cdrom_paranoia *p, offsets *o, long value,
										  void (*callback) (long, int));

#endif
