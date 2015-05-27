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

/* @(#)isort.c	1.14 04/02/20 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
/*
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 *
 * sorted vector abstraction for paranoia
 *
 */

/*
 * Old isort got a bit complex.  This re-constrains complexity to
 * give a go at speed through a more alpha-6-like mechanism.
 */

#include <mconfig.h>
#include <stdxlib.h>
#include <standard.h>
#include <utypes.h>
#include <strdefs.h>
#include "p_block.h"
#include "isort.h"
#include "pmalloc.h"

sort_info *sort_alloc(long size);
void sort_unsortall(sort_info * i);
void sort_free(sort_info * i);
void sort_sort(sort_info * i, long sortlo, long sorthi);
void sort_setup(sort_info * i, Int16_t * vector, long *abspos, long size, 
					 long sortlo, long sorthi);
sort_link *sort_getmatch(sort_info * i, long post, long overlap, int value);
sort_link *sort_nextmatch(sort_info * i, sort_link * prev);


sort_info *sort_alloc(long size)
{
	sort_info	*ret = _pcalloc(1, sizeof (sort_info));

	ret->vector = NULL;
	ret->sortbegin = -1;
	ret->size = -1;
	ret->maxsize = size;

	ret->head = _pcalloc(65536, sizeof (sort_link *));
	ret->bucketusage = _pmalloc(65536 * sizeof (long));
	ret->revindex = _pcalloc(size, sizeof (sort_link));
	ret->lastbucket = 0;

	return (ret);
}

void sort_unsortall(sort_info *i)
{
	if (i->lastbucket > 2000) {	/* a guess */
		memset(i->head, 0, 65536 * sizeof (sort_link *));
	} else {
		long	b;

		for (b = 0; b < i->lastbucket; b++)
			i->head[i->bucketusage[b]] = NULL;
	}

	i->lastbucket = 0;
	i->sortbegin = -1;
}

void sort_free(sort_info *i)
{
	_pfree(i->revindex);
	_pfree(i->head);
	_pfree(i->bucketusage);
	_pfree(i);
}

void sort_sort(sort_info *i, long sortlo, long sorthi)
{
	long	j;

	for (j = sorthi - 1; j >= sortlo; j--) {
		sort_link	**hv = i->head + i->vector[j] + 32768;
		sort_link	 *l = i->revindex + j;

		if (*hv == NULL) {
			i->bucketusage[i->lastbucket] = i->vector[j] + 32768;
			i->lastbucket++;
		}
		l->next = *hv;
		*hv = l;
	}
	i->sortbegin = 0;
}

/*
 * size *must* be less than i->maxsize
 */
void sort_setup(sort_info *i, Int16_t *vector, long *abspos, long size, 
                long sortlo, long sorthi)
{
	if (i->sortbegin != -1)
		sort_unsortall(i);

	i->vector = vector;
	i->size = size;
	i->abspos = abspos;

	i->lo = min(size, max(sortlo - *abspos, 0));
	i->hi = max(0, min(sorthi - *abspos, size));
}

sort_link *sort_getmatch(sort_info *i, long post, long overlap, int value)
{
	sort_link	*ret;

	if (i->sortbegin == -1)
		sort_sort(i, i->lo, i->hi);
	/*
	 * Now we reuse lo and hi
	 */
	post = max(0, min(i->size, post));
	i->val = value + 32768;
	i->lo = max(0, post - overlap);		/* absolute position */
	i->hi = min(i->size, post + overlap);	/* absolute position */

	ret = i->head[i->val];
	while (ret) {
		if (ipos(i, ret) < i->lo) {
			ret = ret->next;
		} else {
			if (ipos(i, ret) >= i->hi)
				ret = NULL;
			break;
		}
	}
/*	i->head[i->val]=ret; */
	return (ret);
}

sort_link *sort_nextmatch(sort_info *i, sort_link *prev)
{
	sort_link	*ret = prev->next;

	if (!ret || ipos(i, ret) >= i->hi)
		return (NULL);
	return (ret);
}
