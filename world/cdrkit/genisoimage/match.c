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

/* @(#)match.c	1.18 04/05/23 joerg */
/*
 * 27-Mar-96: Jan-Piet Mens <jpm@mens.de>
 * added 'match' option (-m) to specify regular expressions NOT to be included
 * in the CD image.
 *
 * Re-written 13-Apr-2000 James Pearson
 * now uses a generic set of routines
 */

#include <mconfig.h>
#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <standard.h>
#include <schily.h>
#include <libport.h>
#include "match.h"

struct match {
	struct match *next;
	char	 *name;
};

typedef struct match match;

static match *mats[MAX_MAT];

static char *mesg[MAX_MAT] = {
	"excluded",
	"excluded ISO-9660",
	"excluded Joliet",
	"hidden attribute ISO-9660",
#ifdef APPLE_HYB
	"excluded HFS",
#endif /* APPLE_HYB */
};

#ifdef SORTING
struct sort_match {
	struct sort_match	*next;
	char			*name;
	int			val;
};

typedef struct sort_match sort_match;

static sort_match	*s_mats;

int
add_sort_match(char *fn, int val)
{
	sort_match *s_mat;

	s_mat = (sort_match *)malloc(sizeof (sort_match));
	if (s_mat == NULL) {
#ifdef	USE_LIBSCHILY
		errmsg("Can't allocate memory for sort filename\n");
#else
		fprintf(stderr, "Can't allocate memory for sort filename\n");
#endif
		return (0);
	}

	if ((s_mat->name = strdup(fn)) == NULL) {
#ifdef	USE_LIBSCHILY
		errmsg("Can't allocate memory for sort filename\n");
#else
		fprintf(stderr, "Can't allocate memory for sort filename\n");
#endif
		return (0);
	}

	/* need to reserve the minimum value for other uses */
	if (val == NOT_SORTED)
		val++;

	s_mat->val = val;
	s_mat->next = s_mats;
	s_mats = s_mat;

	return (1);
}

void
add_sort_list(char *file)
{
	FILE	*fp;
	char	name[4096];
	char	*p;
	int	val;

	if ((fp = fopen(file, "r")) == NULL) {
#ifdef	USE_LIBSCHILY
		comerr("Can't open sort file list %s\n", file);
#else
		fprintf(stderr, "Can't open hidden/exclude file list %s\n", file);
		exit(1);
#endif
	}

	while (fgets(name, sizeof (name), fp) != NULL) {
		/*
		 * look for the last space or tab character
		 */
		if ((p = strrchr(name, ' ')) == NULL)
			p = strrchr(name, '\t');
		else if (strrchr(p, '\t') != NULL)	/* Tab after space? */
			p = strrchr(p, '\t');

		if (p == NULL) {
#ifdef	USE_LIBSCHILY
			comerrno(EX_BAD, "Incorrect sort file format\n\t%s", name);
#else
			fprintf(stderr, "Incorrect sort file format\n\t%s", name);
#endif
			continue;
		} else {
			*p = '\0';
			val = atoi(++p);
		}
		if (!add_sort_match(name, val)) {
			fclose(fp);
			return;
		}
	}

	fclose(fp);
}

int
sort_matches(char *fn, int val)
{
	register sort_match	*s_mat;

	for (s_mat = s_mats; s_mat; s_mat = s_mat->next) {
		if (fnmatch(s_mat->name, fn, FNM_FILE_NAME) != FNM_NOMATCH) {
			return (s_mat->val); /* found sort value */
		}
	}
	return (val); /* not found - default sort value */
}

void
del_sort()
{
	register sort_match * s_mat, *s_mat1;

	s_mat = s_mats;
	while (s_mat) {
		s_mat1 = s_mat->next;

		free(s_mat->name);
		free(s_mat);

		s_mat = s_mat1;
	}

	s_mats = 0;
}

#endif /* SORTING */


int
gen_add_match(char *fn, int n)
{
	match	*mat;

	if (n >= MAX_MAT)
		return (0);

	mat = (match *)malloc(sizeof (match));
	if (mat == NULL) {
#ifdef	USE_LIBSCHILY
		errmsg("Can't allocate memory for %s filename\n", mesg[n]);
#else
		fprintf(stderr, "Can't allocate memory for %s filename\n", mesg[n]);
#endif
		return (0);
	}

	if ((mat->name = strdup(fn)) == NULL) {
#ifdef	USE_LIBSCHILY
		errmsg("Can't allocate memory for %s filename\n", mesg[n]);
#else
		fprintf(stderr, "Can't allocate memory for %s filename\n", mesg[n]);
#endif
		return (0);
	}

	mat->next = mats[n];
	mats[n] = mat;

	return (1);
}

void
gen_add_list(char *file, int n)
{
	FILE	*fp;
	char	name[4096];
	int	len;

	if ((fp = fopen(file, "r")) == NULL) {
#ifdef	USE_LIBSCHILY
		comerr("Can't open %s file list %s\n", mesg[n], file);
#else
		fprintf(stderr, "Can't open %s file list %s\n", mesg[n], file);
		exit(1);
#endif
	}

	while (fgets(name, sizeof (name), fp) != NULL) {
		/*
		 * strip of '\n'
		 */
		len = strlen(name);
		if (name[len - 1] == '\n') {
			name[len - 1] = '\0';
		}
		if (!gen_add_match(name, n)) {
			fclose(fp);
			return;
		}
	}

	fclose(fp);
}

int
gen_matches(char *fn, int n)
{
	register match * mat;

	if (n >= MAX_MAT)
		return (0);

	for (mat = mats[n]; mat; mat = mat->next) {
		if (fnmatch(mat->name, fn, FNM_FILE_NAME) != FNM_NOMATCH) {
			return (1);	/* found -> excluded filename */
		}
	}
	return (0);			/* not found -> not excluded */
}

int
gen_ishidden(int n)
{
	if (n >= MAX_MAT)
		return (0);

	return ((int)(mats[n] != 0));
}

void
gen_del_match(int n)
{
	register match	*mat;
	register match 	*mat1;

	if (n >= MAX_MAT)
		return;

	mat = mats[n];

	while (mat) {
		mat1 = mat->next;

		free(mat->name);
		free(mat);

		mat = mat1;
	}

	mats[n] = 0;
}
