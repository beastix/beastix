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

/* @(#)handlecond.c	1.22 04/05/09 Copyright 1985-2004 J. Schilling */
/*
 *	setup/clear a condition handler for a software signal
 */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/*
 *	A procedure frame is marked to have handlers if the
 *	previous freme pointer for this procedure is odd.
 *	The even base value, in this case actually points to a SIGBLK which
 *	holds the saved "real" frame pointer.
 *	The SIGBLK mentioned above may me the start of a chain of SIGBLK's,
 *	containing different handlers.
 *
 *	This will work on processors which support a frame pointer chain
 *	on the stack.
 *	On a processor which doesn't support this I think of a method
 *	where handlecond() has an own chain of frames, holding chains of
 *	SIGBLK's.
 *	In this case, a parameter has to be added to handlecond() and
 *	unhandlecond(). This parameter will be an opaque cookie which is zero
 *	on the first call to handlecond() in a procedure.
 *	A new cookie will be returned by handlecond() which must be used on
 *	each subsequent call to handlecond() and unhandlecond() in the same
 *	procedure.
 *
 *	Copyright (c) 1985-2004 J. Schilling
 */
#include <mconfig.h>
#include <sigblk.h>
#include <standard.h>
#include <stdxlib.h>
#include <strdefs.h>
#include <avoffset.h>
#include <utypes.h>
#include <schily.h>

#if	!defined(AV_OFFSET) || !defined(FP_INDIR)
#	ifdef	HAVE_SCANSTACK
#	undef	HAVE_SCANSTACK
#	endif
#endif

#ifdef	HAVE_SCANSTACK
#include <stkframe.h>
#else
extern	SIGBLK	*__roothandle;
#endif	/* HAVE_SCANSTACK */

#define	is_even(p)	((((long)(p)) & 1) == 0)
#define	even(p)		(((long)(p)) & ~1L)
#if defined(__sun) && defined(__i386)
/*
 * Solaris x86 has a broken frame.h which defines the frame ptr to int.
 */
#define	odd(p)		(((Intptr_t)(p)) | 1)
#else
#define	odd(p)		(void *)(((Intptr_t)(p)) | 1)
#endif

#ifdef	__future__
#define	even(p)		(((long)(p)) - 1) /* will this work with 64 bit ?? */
#endif

EXPORT	void	starthandlecond	__PR((SIGBLK *sp));

#ifdef	PROTOTYPES
EXPORT void
handlecond(const char	*signame,
		register SIGBLK	*sp,
		int		(*func)(const char *, long, long),
		long		arg1)
#else
EXPORT void
handlecond(signame, sp, func, arg1)
		char	*signame;
	register SIGBLK *sp;
		BOOL	(*func)();
		long	arg1;
#endif
{
	register SIGBLK	*this;
	register SIGBLK	*last = (SIGBLK *)NULL;
#ifdef	HAVE_SCANSTACK
	struct frame	*fp   = (struct frame *)NULL;
#endif
		int	slen;

	if (signame == NULL || (slen = strlen(signame)) == 0) {
		raisecond("handle_bad_name", (long)signame);
		abort();
	}

#ifdef	HAVE_SCANSTACK
	fp = (struct frame *)getfp();
	fp = (struct frame *)fp->fr_savfp;	/* point to frame of caller */
	if (is_even(fp->fr_savfp)) {
		/*
		 * Easy case: no handlers yet
		 * save real framepointer in sp->sb_savfp
		 */
		sp->sb_savfp   = (long **)fp->fr_savfp;
		this = (SIGBLK *)NULL;
	} else {
		this = (SIGBLK *)even(fp->fr_savfp);
	}
#else
	this = __roothandle;
#endif

	for (; this; this = this->sb_signext) {
		if (this == sp) {
			/*
			 * If a SIGBLK is reused, the name must not change.
			 */
			if (this->sb_signame != NULL &&
			    !streql(this->sb_signame, signame)) {
				raisecond("handle_reused_block", (long)signame);
				abort();
			}
			sp->sb_sigfun = func;
			sp->sb_sigarg = arg1;
			return;
		}
		if (this->sb_signame != NULL &&
		    streql(this->sb_signame, signame)) {
			if (last == (SIGBLK *)NULL) {
				/*
				 * 'this' is the first entry in chain
				 */
				if (this->sb_signext == (SIGBLK *)NULL) {
					/*
					 * only 'this' entry is in chain, copy
					 * saved real frame pointer into new sp
					 */
					sp->sb_savfp = this->sb_savfp;
				} else {
#ifdef	HAVE_SCANSTACK
					/*
					 * make second entry first link in chain
					 */
					this->sb_signext->sb_savfp =
								this->sb_savfp;
					fp->fr_savfp = odd(this->sb_signext);
#else
					/*
					 * Cannot happen if scanning the stack
					 * is not possible...
					 */
					raisecond("handle_is_empty", (long)0);
					abort();
#endif
				}
				continue;	/* don't trash 'last' ptr */
			} else {
				last->sb_signext = this->sb_signext;
			}
		}
		last = this;
	}
	sp->sb_signext = (SIGBLK *)NULL;
	sp->sb_signame = signame;
	sp->sb_siglen  = slen;
	sp->sb_sigfun  = func;
	sp->sb_sigarg  = arg1;
	/*
	 * If there is a chain append to end of the chain, else make it first
	 */
	if (last)
		last->sb_signext = sp;
#ifdef	HAVE_SCANSTACK
	else
		fp->fr_savfp = odd(sp);
#else
	/*
	 * Cannot happen if scanning the stack is not possible...
	 */
	else {
		raisecond("handle_is_empty", (long)0);
		abort();
	}
#endif
}

EXPORT void
starthandlecond(sp)
	register SIGBLK *sp;
{
#ifdef	HAVE_SCANSTACK
	struct frame	*fp = NULL;

	/*
	 * As the SCO OpenServer C-Compiler has a bug that may cause
	 * the first function call to getfp() been done before the
	 * new stack frame is created, we call getfp() twice.
	 */
	(void) getfp();
#endif

	sp->sb_signext = (SIGBLK *)NULL;
	sp->sb_signame = NULL;
	sp->sb_siglen  = 0;
	sp->sb_sigfun  = (handlefunc_t)NULL;
	sp->sb_sigarg  = 0;

#ifdef	HAVE_SCANSTACK
	fp = (struct frame *)getfp();
	fp = (struct frame *)fp->fr_savfp;	/* point to frame of caller */

	if (is_even(fp->fr_savfp)) {
		/*
		 * Easy case: no handlers yet
		 * save real framepointer in sp
		 */
		sp->sb_savfp = (long **)fp->fr_savfp;
		fp->fr_savfp = odd(sp);
	} else {
		raisecond("handle_not_empty", (long)0);
		abort();
	}
#else
	sp->sb_savfp	= (long **)__roothandle;
	__roothandle	= sp;
#endif
}

EXPORT void
unhandlecond(sp)
	register SIGBLK *sp;
{
#ifdef	HAVE_SCANSTACK
	register struct frame	*fp;
	register SIGBLK		*sps;

	/*
	 * As the SCO OpenServer C-Compiler has a bug that may cause
	 * the first function call to getfp() been done before the
	 * new stack frame is created, we call getfp() twice.
	 */
	(void) getfp();
	fp = (struct frame *)getfp();
	fp = (struct frame *)fp->fr_savfp;	/* point to frame of caller */

	if (!is_even(fp->fr_savfp)) {			/* if handlers	    */
		sps = (SIGBLK *)even(fp->fr_savfp);	/* point to SIGBLK  */
							/* real framepointer */
#if defined(__sun) && defined(__i386)
		fp->fr_savfp = (intptr_t)sps->sb_savfp;
#else
		fp->fr_savfp = (struct frame *)sps->sb_savfp;
#endif
	}
#else
	if (__roothandle == NULL) {
		raisecond("handle_is_empty", (long)0);
		abort();
	}
	/*
	 * Pop top level handler chain.
	 */
	__roothandle = (SIGBLK *)__roothandle->sb_savfp;
#endif
}
