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

/* @(#)isort.h	1.10 04/02/18 J. Schilling from cdparanoia-III-alpha9.8 */
/*
 *	Modifications to make the code portable Copyright (c) 2002 J. Schilling
 */
/*
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 */

#ifndef	_ISORT_H_
#define	_ISORT_H_

typedef struct sort_link {
	struct sort_link *next;
} sort_link;

typedef struct sort_info {
	Int16_t		*vector;	/* vector */
					/* vec storage doesn't belong to us */

	long		*abspos;	/* pointer for side effects */
	long		size;		/* vector size */

	long		maxsize;	/* maximum vector size */

	long		sortbegin;	/* range of contiguous sorted area */
	long		lo;
	long		hi;		/* current post, overlap range */
	int		val;		/* ...and val */

	/*
	 * sort structs
	 */
	sort_link	**head;		/* sort buckets (65536) */

	long		*bucketusage;	/* of used buckets (65536) */
	long		lastbucket;
	sort_link	*revindex;

} sort_info;

extern sort_info	*sort_alloc(long size);
extern void		sort_unsortall(sort_info * i);
extern void		sort_setup(sort_info * i, Int16_t * vector, long *abspos, 
								  long size, long sortlo, long sorthi);
extern void		sort_free(sort_info * i);
extern sort_link	*sort_getmatch(sort_info * i, long post, long overlap, 
											int value);
extern sort_link	*sort_nextmatch(sort_info * i, sort_link * prev);

#define	is(i)		((i)->size)
#define	ib(i)		(*(i)->abspos)
#define	ie(i)		((i)->size + *(i)->abspos)
#define	iv(i)		((i)->vector)
#define	ipos(i, l)	((l) - (i)->revindex)

#endif
