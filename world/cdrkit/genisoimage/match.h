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

/* @(#)match.h	1.9 04/05/23 joerg */
/*
 * 27th March 1996. Added by Jan-Piet Mens for matching regular expressions
 *                  in paths.
 *
 */

#include "fnmatch.h"

#ifdef	SORTING
#include <limits.h>
#define	NOT_SORTED INT_MIN

#ifdef	MAX				/* May be defined in param.h */
#undef	MAX
#endif
#define	MAX(A, B)	(A) > (B) ? (A) : (B)
#endif

#define	EXCLUDE		0		/* Exclude file completely */
#define	I_HIDE		1		/* ISO9660/Rock Ridge hide */
#define	J_HIDE		2		/* Joliet hide */
#define	H_HIDE		3		/* ISO9660 hidden bit set */

#ifdef	APPLE_HYB
#define	HFS_HIDE	4		/* HFS hide */
#define	MAX_MAT		5
#else
#define	MAX_MAT		4
#endif /* APPLE_HYB */

extern int	gen_add_match(char *fn, int n);
extern int	gen_matches(char *fn, int n);
extern void	gen_add_list(char *fn, int n);
extern int	gen_ishidden(int n);
extern void	gen_del_match(int n);

#ifdef SORTING
extern int	add_sort_match(char *fn, int val);
extern void	add_sort_list(char *fn);
extern int	sort_matches(char *fn, int val);
extern void	del_sort(void);
#endif /* SORTING */

/*
 * The following are for compatiblity with the separate routines - the
 * main code should be changed to call the generic routines directly
 */

/* filenames to be excluded */
#define	add_match(FN)	gen_add_match((FN), EXCLUDE)
#define	add_list(FN)	gen_add_list((FN), EXCLUDE)
#define	matches(FN)	gen_matches((FN), EXCLUDE)

/* ISO9660/Rock Ridge filenames to be hidden */
#define	i_add_match(FN)	gen_add_match((FN), I_HIDE)
#define	i_add_list(FN)	gen_add_list((FN), I_HIDE)
#define	i_matches(FN)	gen_matches((FN), I_HIDE)
#define	i_ishidden()	gen_ishidden(I_HIDE)

/* Joliet filenames to be hidden */
#define	j_add_match(FN)	gen_add_match((FN), J_HIDE)
#define	j_add_list(FN)	gen_add_list((FN), J_HIDE)
#define	j_matches(FN)	gen_matches((FN), J_HIDE)
#define	j_ishidden()	gen_ishidden(J_HIDE)

/* ISO9660 "hidden" files */
#define	h_add_match(FN)	gen_add_match((FN), H_HIDE)
#define	h_add_list(FN)	gen_add_list((FN), H_HIDE)
#define	h_matches(FN)	gen_matches((FN), H_HIDE)
#define	h_ishidden()	gen_ishidden(H_HIDE)

#ifdef APPLE_HYB
/* HFS filenames to be hidden */
#define	hfs_add_match(FN) gen_add_match((FN), HFS_HIDE)
#define	hfs_add_list(FN)  gen_add_list((FN), HFS_HIDE)
#define	hfs_matches(FN)	  gen_matches((FN), HFS_HIDE)
#define	hfs_ishidden()	  gen_ishidden(HFS_HIDE)
#endif /* APPLE_HYB */
