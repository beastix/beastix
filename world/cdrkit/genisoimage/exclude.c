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

/* @(#)exclude.c	1.9 04/03/04 joerg */
/*
 * 9-Dec-93 R.-D. Marzusch, marzusch@odiehh.hanse.de:
 * added 'exclude' option (-x) to specify pathnames NOT to be included in
 * CD image.
 */

#include <mconfig.h>
#include <stdio.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <standard.h>
#include <schily.h>


/* this allows for 1000 entries to be excluded ... */
#define	MAXEXCL		1000

static char		*excl[MAXEXCL];

void	exclude(char *fn);
int	is_excluded(char *fn);


void
exclude(char *fn)
{
	register int	i;

	for (i = 0; excl[i] && i < MAXEXCL; i++)
		;

	if (i == MAXEXCL) {
		fprintf(stderr,
			"Can't exclude '%s' - too many entries in table\n",
								fn);
		return;
	}
	excl[i] = (char *) malloc(strlen(fn) + 1);
	if (excl[i] == NULL) {
#ifdef	USE_LIBSCHILY
		errmsg("Can't allocate memory for excluded filename\n");
#else
		fprintf(stderr,
			"Can't allocate memory for excluded filename\n");
#endif
		return;
	}
	strcpy(excl[i], fn);
}

int
is_excluded(char *fn)
{
	register int	i;

	/*
	 * very dumb search method ...
	 */
	for (i = 0; excl[i] && i < MAXEXCL; i++) {
		if (strcmp(excl[i], fn) == 0) {
			return (1);	/* found -> excluded filenmae */
		}
	}
	return (0);	/* not found -> not excluded */
}
