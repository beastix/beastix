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

/* @(#)ifo_read.c	1.5 04/03/04 joerg */
/*
 * Copyright (C) 2002 Olaf Beck <olaf_sc@yahoo.com>
 *		     Jörg Schilling <schilling@fokus.gmd.de>
 *		     (making the code portable)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * NOTE: This is a cut down version of libdvdread for genisoimage, due
 * to portability issues with the current libdvdread according to
 * the maintainer of genisoimage.
 * This cut down version only reads from a harddisk file structure
 * and it only implements the functions necessary inorder to make
 * genisoimage produce valid DVD-Video images.
 * DON'T USE THIS LIBRARY IN ANY OTHER PROGRAM GET THE REAL
 * LIBDVDREAD INSTEAD
 */
#ifdef DVD_VIDEO

#include <mconfig.h>
#include "genisoimage.h"
#include <fctldefs.h>
#include <utypes.h>
#include <schily.h>

#include "ifo_read.h"
#include "bswap.h"

#define MSGEREAD "Failed to read VIDEO_TS.IFO\n"
#define MSGESEEK "Failed to seek VIDEO_TS.IFO\n"
#define MSGEOPEN "Failed to open VIDEO_TS.IFO\n"

static	ifo_handle_t *ifoReadVTSI(int file, ifo_handle_t * ifofile);
static	ifo_handle_t *ifoReadVGMI(int file, ifo_handle_t * ifofile);
ifo_handle_t *ifoOpen(dvd_reader_t *dvd, int title);
static	void		ifoFree_TT_SRPT(ifo_handle_t *ifofile);
void		ifoClose(ifo_handle_t * ifofile);


static ifo_handle_t *
ifoReadVTSI(int file, ifo_handle_t *ifofile)
{
	off_t offset;
	UInt32_t sector;

	vtsi_mat_t * vtsi_mat;

	/* Make the VMG part NULL */
	ifofile->vmgi_mat = NULL;
	ifofile->tt_srpt = NULL;

	vtsi_mat = (vtsi_mat_t *)e_malloc(sizeof (vtsi_mat_t));
	if (!vtsi_mat) {
/*		fprintf(stderr, "Memmory allocation error\n");*/
		free(ifofile);
		return (0);
	}

	ifofile->vtsi_mat = vtsi_mat;

	/* Last sector of VTS i.e. last sector of BUP */

	offset = 12;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}

	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vtsi_mat->vts_last_sector = sector;

	/* Last sector of IFO */

	offset = 28;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}

	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vtsi_mat->vtsi_last_sector = sector;


	/* Star sector of VTS Menu VOB */

	offset = 192;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vtsi_mat->vtsm_vobs = sector;


	/* Start sector of VTS Title VOB */

	offset = 196;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vtsi_mat->vtstt_vobs = sector;

	return (ifofile);
}


static ifo_handle_t *
ifoReadVGMI(int file, ifo_handle_t *ifofile)
{
	off_t	offset;
	Uint	counter;
	UInt32_t sector;
	UInt16_t titles;

	vmgi_mat_t *vmgi_mat;
	tt_srpt_t *tt_srpt;

	/* Make the VTS part null */
	ifofile->vtsi_mat = NULL;

	vmgi_mat = (vmgi_mat_t *)e_malloc(sizeof (vmgi_mat_t));
	if (!vmgi_mat) {
/*		fprintf(stderr, "Memmory allocation error\n");*/
		free(ifofile);
		return (0);
	}

	ifofile->vmgi_mat = vmgi_mat;

	/* Last sector of VMG i.e. last sector of BUP */

	offset = 12;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}

	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vmgi_mat->vmg_last_sector = sector;

	/* Last sector of IFO */

	offset = 28;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vmgi_mat->vmgi_last_sector = sector;


	/* Number of VTS i.e. title sets */

	offset = 62;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &titles, sizeof (titles)) != sizeof (titles)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_16(titles);

	vmgi_mat->vmg_nr_of_title_sets = titles;


	/* Star sector of VMG Menu VOB */

	offset = 192;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vmgi_mat->vmgm_vobs = sector;


	/* Sector offset to TT_SRPT */

	offset = 196;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		ifoClose(ifofile);
		return (0);
	}


	if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		ifoClose(ifofile);
		return (0);
	}

	B2N_32(sector);

	vmgi_mat->tt_srpt = sector;

	tt_srpt = (tt_srpt_t *)e_malloc(sizeof (tt_srpt_t));
	if (!tt_srpt) {
/*		fprintf(stderr, "Memmory allocation error\n");*/
		ifoClose(ifofile);
		return (0);
	}

	ifofile->tt_srpt = tt_srpt;


	/* Number of titles in TT_SRPT */

	offset = 2048 * vmgi_mat->tt_srpt;

	if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGESEEK);
#else
		printf(stderr, MSGESEEK);
#endif
		return (0);
	}

	if (read(file, &titles, sizeof (titles)) != sizeof (titles)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		return (0);
	}

	B2N_16(titles);

	tt_srpt->nr_of_srpts = titles;

	tt_srpt->title = (title_info_t *)e_malloc(sizeof (title_info_t) * tt_srpt->nr_of_srpts);
	if (!tt_srpt->title) {
/*		fprintf(stderr, "Memmory allocation error\n");*/
		ifoClose(ifofile);
		return (0);
	}

	/* Start sector of each title in TT_SRPT */

	for (counter = 0; counter < tt_srpt->nr_of_srpts; counter++) {
		offset = (2048 * vmgi_mat->tt_srpt) + 8 + (counter * 12) + 8;
		if (lseek(file, offset, SEEK_SET) != offset) {
#ifdef	USE_LIBSCHILY
			errmsg(MSGESEEK);
#else
			printf(stderr, MSGESEEK);
#endif
			ifoClose(ifofile);
			return (0);
		}

		if (read(file, &sector, sizeof (sector)) != sizeof (sector)) {
#ifdef	USE_LIBSCHILY
			errmsg(MSGEREAD);
#else
			printf(stderr, MSGEREAD);
#endif
			ifoClose(ifofile);
			return (0);
		}

		B2N_32(sector);

		tt_srpt->title[counter].title_set_sector = sector;

	}
	return (ifofile);
}

ifo_handle_t *
ifoOpen(dvd_reader_t *dvd, int title)
{
	/* The main ifofile structure */
	ifo_handle_t *ifofile;

	/* File handles and offset */
	int file;
	off_t offset;
	char full_path[ PATH_MAX + 1 ];

	/* Identifier of the IFO */
	char identifier[13];

	identifier[0] = '\0';

	ifofile = (ifo_handle_t *)e_malloc(sizeof (ifo_handle_t));

	memset(ifofile, 0, sizeof (ifo_handle_t));

	if (title) {
		snprintf(full_path, sizeof (full_path),
				"%s/VIDEO_TS/VTS_%02d_0.IFO", dvd->path_root, title);
	} else {
		snprintf(full_path, sizeof (full_path),
				"%s/VIDEO_TS/VIDEO_TS.IFO", dvd->path_root);
	}

	if ((file = open(full_path, O_RDONLY | O_BINARY)) == -1) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEOPEN);
#else
		printf(stderr, MSGEOPEN);
#endif
		free(ifofile);
		return (0);
	}

	offset = 0;

	/* Determine if we have a VMGI or VTSI */

	if (read(file, identifier, sizeof (identifier)) != sizeof (identifier)) {
#ifdef	USE_LIBSCHILY
		errmsg(MSGEREAD);
#else
		printf(stderr, MSGEREAD);
#endif
		return (0);
	}

	if ((strstr("DVDVIDEO-VMG", identifier) != 0) && (title == 0)) {
		ifofile = ifoReadVGMI(file, ifofile);
		close(file);
		return (ifofile);
	} else if ((strstr("DVDVIDEO-VTS", identifier) != 0) && (title != 0)) {
		ifofile = ifoReadVTSI(file, ifofile);
		close(file);
		return (ifofile);
	} else {
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD, "Giving up this is not a valid IFO file\n");
#else
		fprintf(stderr, "Giving up this is not a valid IFO file\n");
#endif
		close(file);
		free(ifofile);
		ifofile = 0;
		return (0);
	}
}

static void
ifoFree_TT_SRPT(ifo_handle_t *ifofile)
{
	if (!ifofile)
		return;

	if (ifofile->tt_srpt) {
		if (ifofile->tt_srpt->title) {
			free(ifofile->tt_srpt->title);
		}
		free(ifofile->tt_srpt);
		ifofile->tt_srpt = 0;
	}
}

void
ifoClose(ifo_handle_t *ifofile)
{

	if (!ifofile)
		return;

	ifoFree_TT_SRPT(ifofile);

	if (ifofile->vmgi_mat) {
		free(ifofile->vtsi_mat);
	}

	if (ifofile->vtsi_mat) {
		free(ifofile->vtsi_mat);
	}

	free(ifofile);
	ifofile = 0;
}
#endif /* DVD_VIDEO */
