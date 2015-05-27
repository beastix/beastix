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

/* @(#)dvd_reader.c	1.3 04/03/04 joerg */
/*
 * Copyright (C) 2001, 2002 Billy Biggs <vektor@dumbterm.net>,
 *                          Håkan Hjort <d95hjort@dtek.chalmers.se>,
 *                          Olaf Beck <olaf_sc@yahoo.com>
 *			    (I only did the cut down no other contribs)
 *			    Jörg Schilling <schilling@fokus.gmd.de>
 *			    (making the code portable)
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

#include "genisoimage.h"
#include <fctldefs.h>
#include <schily.h>

#include "dvd_reader.h"

struct dvd_file_s {
	/* Basic information. */
	dvd_reader_t	*dvd;

	/* Calculated at open-time, size in blocks. */
	ssize_t		filesize;
};


void		DVDCloseFile(dvd_file_t *dvd_file);
static	dvd_file_t *DVDOpenFilePath(dvd_reader_t *dvd, char *filename);
static	dvd_file_t *DVDOpenVOBPath(dvd_reader_t *dvd, int title, int menu);
dvd_file_t *DVDOpenFile(dvd_reader_t *dvd, int titlenum, 
								dvd_read_domain_t domain);
static	dvd_reader_t *DVDOpenPath(const char *path_root);
dvd_reader_t *DVDOpen(const char *path);
void		DVDClose(dvd_reader_t *dvd);
ssize_t		DVDFileSize(dvd_file_t *dvd_file);


/*
 * Free a DVD file
 */
void
DVDCloseFile(dvd_file_t *dvd_file)
{
	free(dvd_file);
	dvd_file = 0;
}


/*
 * Stat a IFO or BUP file from a DVD directory tree.
 */
static dvd_file_t *
DVDOpenFilePath(dvd_reader_t *dvd, char *filename)
{

	char		full_path[PATH_MAX + 1];
	dvd_file_t	*dvd_file;
	struct stat	fileinfo;

	/* Get the full path of the file. */

	snprintf(full_path, sizeof (full_path),
				"%s/%s", dvd->path_root, filename);


	dvd_file = (dvd_file_t *) e_malloc(sizeof (dvd_file_t));
	if (!dvd_file)
		return (0);
	dvd_file->dvd = dvd;
	dvd_file->filesize = 0;

	if (stat(full_path, &fileinfo) < 0) {
		free(dvd_file);
		return (0);
	}
	dvd_file->filesize = fileinfo.st_size / DVD_VIDEO_LB_LEN;

	return (dvd_file);
}


/*
 * Stat a VOB file from a DVD directory tree.
 */
static dvd_file_t *
DVDOpenVOBPath(dvd_reader_t *dvd, int title, int menu)
{

	char		filename[PATH_MAX + 1];
	struct stat	fileinfo;
	dvd_file_t	*dvd_file;
	int		i;

	dvd_file = (dvd_file_t *) e_malloc(sizeof (dvd_file_t));
	if (!dvd_file)
		return (0);
	dvd_file->dvd = dvd;
	dvd_file->filesize = 0;

	if (menu) {
		if (title == 0) {
			snprintf(filename, sizeof (filename),
				"%s/VIDEO_TS/VIDEO_TS.VOB", dvd->path_root);
		} else {
			snprintf(filename, sizeof (filename),
				"%s/VIDEO_TS/VTS_%02i_0.VOB", dvd->path_root, title);
		}
		if (stat(filename, &fileinfo) < 0) {
			free(dvd_file);
			return (0);
		}
		dvd_file->filesize = fileinfo.st_size / DVD_VIDEO_LB_LEN;
	} else {
		for (i = 0; i < 9; ++i) {

			snprintf(filename, sizeof (filename),
				"%s/VIDEO_TS/VTS_%02i_%i.VOB", dvd->path_root, title, i + 1);
			if (stat(filename, &fileinfo) < 0) {
					break;
			}

			dvd_file->filesize += fileinfo.st_size / DVD_VIDEO_LB_LEN;
		}
	}

	return (dvd_file);
}

/*
 * Stat a DVD file from a DVD directory tree
 */
EXPORT dvd_file_t *
DVDOpenFile(dvd_reader_t *dvd, int titlenum, dvd_read_domain_t domain)
{
	char		filename[MAX_UDF_FILE_NAME_LEN];

	switch (domain) {

	case DVD_READ_INFO_FILE:
		if (titlenum == 0) {
			snprintf(filename, sizeof (filename),
					"/VIDEO_TS/VIDEO_TS.IFO");
		} else {
			snprintf(filename, sizeof (filename),
					"/VIDEO_TS/VTS_%02i_0.IFO", titlenum);
		}
		break;

	case DVD_READ_INFO_BACKUP_FILE:
		if (titlenum == 0) {
			snprintf(filename, sizeof (filename),
					"/VIDEO_TS/VIDEO_TS.BUP");
		} else {
			snprintf(filename, sizeof (filename),
					"/VIDEO_TS/VTS_%02i_0.BUP", titlenum);
		}
		break;

	case DVD_READ_MENU_VOBS:
		return (DVDOpenVOBPath(dvd, titlenum, 1));

	case DVD_READ_TITLE_VOBS:
		if (titlenum == 0)
			return (0);
		return (DVDOpenVOBPath(dvd, titlenum, 0));

	default:
#ifdef	USE_LIBSCHILY
		errmsgno(EX_BAD, "Invalid domain for file open.\n");
#else
		fprintf(stderr, "Invalid domain for file open.\n");
#endif
		return (0);
	}
	return (DVDOpenFilePath(dvd, filename));
}



/*
 * Stat a DVD directory structure
 */
static dvd_reader_t *
DVDOpenPath(const char *path_root)
{
	dvd_reader_t	*dvd;

	dvd = (dvd_reader_t *) e_malloc(sizeof (dvd_reader_t));
	if (!dvd)
		return (0);
	dvd->path_root = strdup(path_root);

	return (dvd);
}


/*
 * Stat a DVD structure - this one only works with directory structures
 */
dvd_reader_t *
DVDOpen(const char *path)
{
	struct stat	fileinfo;
	int		ret;

	if (!path)
		return (0);

	ret = stat(path, &fileinfo);
	if (ret < 0) {
	/* If we can't stat the file, give up */
#ifdef	USE_LIBSCHILY
		errmsg("Can't stat %s\n", path);
#else
		fprintf(stderr, "Can't stat %s\n", path);
		perror("");
#endif
		return (0);
	}


	if (S_ISDIR(fileinfo.st_mode)) {
		return (DVDOpenPath(path));
	}

	/* If it's none of the above, screw it. */
#ifdef	USE_LIBSCHILY
	errmsgno(EX_BAD, "Could not open %s\n", path);
#else
	fprintf(stderr, "Could not open %s\n", path);
#endif
	return (0);
}

/*
 * Free a DVD structure - this one will only close a directory tree
 */
void
DVDClose(dvd_reader_t *dvd)
{
	if (dvd) {
		if (dvd->path_root) free(dvd->path_root);
		free(dvd);
		dvd = 0;
	}
}



/*
 * Return the size of a DVD file
 */
ssize_t
DVDFileSize(dvd_file_t *dvd_file)
{
	return (dvd_file->filesize);
}

#endif /* DVD_VIDEO */
