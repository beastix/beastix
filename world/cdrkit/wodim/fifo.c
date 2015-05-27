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

/* @(#)fifo.c	1.49 06/02/08 Copyright 1989,1997-2006 J. Schilling */
/*
 *	A "fifo" that uses shared memory between two processes
 *
 *	The actual code is a mixture of borrowed code from star's fifo.c
 *	and a proposal from Finn Arne Gangstad <finnag@guardian.no>
 *	who had the idea to use a ring buffer to handle average size chunks.
 *
 *	Copyright (c) 1989,1997-2006 J. Schilling
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

#ifndef	DEBUG
#define	DEBUG
#endif
/*#define	XDEBUG*/
#include <mconfig.h>

  /* We always wish FIFO unless it is disabled below */
#ifndef FIFO
#define FIFO
#endif

#if	defined(HAVE_OS_H) && \
	defined(HAVE_CLONE_AREA) && defined(HAVE_CREATE_AREA) && \
	defined(HAVE_DELETE_AREA)
#include <OS.h>
#	define	HAVE_BEOS_AREAS	/* BeOS/Zeta */
#endif
#if	!defined(HAVE_SMMAP) && !defined(HAVE_USGSHM) && \
	!defined(HAVE_DOSALLOCSHAREDMEM) && !defined(HAVE_BEOS_AREAS)
#undef	FIFO			/* We cannot have a FIFO on this platform */
#endif
#if	!defined(HAVE_FORK)
#undef	FIFO			/* We cannot have a FIFO on this platform */
#endif
#ifdef	FIFO
#if !defined(USE_MMAP) && !defined(USE_USGSHM)
#define	USE_MMAP
#endif
#ifndef	HAVE_SMMAP
#	undef	USE_MMAP
#	define	USE_USGSHM	/* now SYSV shared memory is the default*/
#endif
#ifdef	USE_MMAP		/* Only want to have one implementation */
#	undef	USE_USGSHM	/* mmap() is preferred			*/
#endif

#ifdef	HAVE_DOSALLOCSHAREDMEM	/* This is for OS/2 */
#	undef	USE_MMAP
#	undef	USE_USGSHM
#	define	USE_OS2SHM
#endif

#ifdef	HAVE_BEOS_AREAS		/* This is for BeOS/Zeta */
#	undef	USE_MMAP
#	undef	USE_USGSHM
#	undef	USE_OS2SHM
#	define	USE_BEOS_AREAS
#endif

#include <stdio.h>
#include <stdxlib.h>
#include <unixstd.h>	/* includes <sys/types.h> */
#include <utypes.h>
#include <fctldefs.h>
#if defined(HAVE_SMMAP) && defined(USE_MMAP)
#include <mmapdefs.h>
#endif
#include <waitdefs.h>
#include <standard.h>
#include <errno.h>
#include <signal.h>
#include <libport.h>
#include <schily.h>

#include "wodim.h"
#include "xio.h"

#ifdef DEBUG
#ifdef XDEBUG
FILE	*ef;
#define	USDEBUG1	if (debug) {if (s == owner_reader) fprintf(ef, "r"); else fprintf(ef, "w"); fflush(ef); }
#define	USDEBUG2	if (debug) {if (s == owner_reader) fprintf(ef, "R"); else fprintf(ef, "W"); fflush(ef); }
#else
#define	USDEBUG1
#define	USDEBUG2
#endif
#define EDEBUG(a)   if (debug) schily_error a
#else
#define	EDEBUG(a)
#define	USDEBUG1
#define	USDEBUG2
#endif

#define	palign(x, a)	(((char *)(x)) + ((a) - 1 - (((UIntptr_t)((x)-1))%(a))))

typedef enum faio_owner {
	owner_none,		/* Unused in real life			    */
	owner_writer,		/* owned by process that writes into FIFO   */
	owner_faio,		/* Intermediate state when buf still in use */
	owner_reader		/* owned by process that reads from FIFO    */
} fowner_t;

char	*onames[] = {
	"none",
	"writer",
	"faio",
	"reader",
};

typedef struct faio {
	int	len;
	volatile fowner_t owner;
	volatile int users;
	short	fd;
	short	saved_errno;
	char	*bufp;
} faio_t;

struct faio_stats {
	long	puts;
	long	gets;
	long	empty;
	long	full;
	long	done;
	long	cont_low;
	int	users;
} *sp;

#define	MIN_BUFFERS	3

#define	MSECS	1000
#define	SECS	(1000*MSECS)

/*
 * Note: WRITER_MAXWAIT & READER_MAXWAIT need to be greater than the SCSI
 * timeout for commands that write to the media. This is currently 200s
 * if we are in SAO mode.
 */
/* microsecond delay between each buffer-ready probe by writing process */
#define	WRITER_DELAY	(20*MSECS)
#define	WRITER_MAXWAIT	(240*SECS)	/* 240 seconds max wait for data */

/* microsecond delay between each buffer-ready probe by reading process */
#define	READER_DELAY	(80*MSECS)
#define	READER_MAXWAIT	(240*SECS)	/* 240 seconds max wait for reader */

static	char	*buf;
static	char	*bufbase;
static	char	*bufend;
static	long	buflen;			/* The size of the FIFO buffer */

extern	int	debug;
extern	int	lverbose;

void	init_fifo(long);
#ifdef	USE_MMAP
static	char	*mkshare(int size);
#endif
#ifdef	USE_USGSHM
static	char	*mkshm(int size);
#endif
#ifdef	USE_OS2SHM
static	char	*mkos2shm(int size);
#endif
#ifdef	USE_BEOS_AREAS
static	char	*mkbeosshm(int size);
static	void	beosshm_child(void);
#endif

BOOL	init_faio(track_t *trackp, int);
BOOL	await_faio(void);
void	kill_faio(void);
int	wait_faio(void);
static	void	faio_reader(track_t *trackp);
static	void	faio_read_track(track_t *trackp);
static	void	faio_wait_on_buffer(faio_t *f, fowner_t s, unsigned long delay,
											  unsigned long max_wait);
static	int	faio_read_segment(int fd, faio_t *f, track_t *track, long secno, 
											int len);
static	faio_t	*faio_ref(int n);
int	faio_read_buf(int f, char *bp, int size);
int	faio_get_buf(int f, char **bpp, int size);
void	fifo_stats(void);
int	fifo_percent(BOOL addone);


void
init_fifo(long fs)
{
	int	pagesize;

	if (fs == 0L)
		return;

	pagesize = getpagesize();
	buflen = roundup(fs, pagesize) + pagesize;
	EDEBUG(("fs: %ld buflen: %ld\n", fs, buflen));

#if	defined(USE_MMAP)
	buf = mkshare(buflen);
#endif
#if	defined(USE_USGSHM)
	buf = mkshm(buflen);
#endif
#if	defined(USE_OS2SHM)
	buf = mkos2shm(buflen);
#endif
#if	defined(USE_BEOS_AREAS)
	buf = mkbeosshm(buflen);
#endif

	bufbase = buf;
	bufend = buf + buflen;
	EDEBUG(("buf: %p bufend: %p, buflen: %ld\n", buf, bufend, buflen));
	buf = palign(buf, pagesize);
	buflen -= buf - bufbase;
	EDEBUG(("buf: %p bufend: %p, buflen: %ld (align %ld)\n", buf, bufend, buflen, (long)(buf - bufbase)));

	/*
	 * Dirty the whole buffer. This can die with various signals if
	 * we're trying to lock too much memory
	 */
	fillbytes(buf, buflen, '\0');

#ifdef	XDEBUG
	if (debug)
		ef = fopen("/tmp/ef", "w");
#endif
}

#ifdef	USE_MMAP
static char *
mkshare(int size)
{
	int	f;
	char	*addr;

#ifdef	MAP_ANONYMOUS	/* HP/UX */
	f = -1;
	addr = mmap(0, mmap_sizeparm(size),
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, f, 0);
#else
	if ((f = open("/dev/zero", O_RDWR)) < 0)
		comerr("Cannot open '/dev/zero'.\n");
	addr = mmap(0, mmap_sizeparm(size),
			PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
#endif
	if (addr == (char *)-1)
		comerr("Cannot get mmap for %d Bytes on /dev/zero.\n", size);
	if (f >= 0)
		close(f);

	if (debug) errmsgno(EX_BAD, "shared memory segment attached at: %p size %d\n",
				(void *)addr, size);

	return (addr);
}
#endif

#ifdef	USE_USGSHM
#include <sys/ipc.h>
#include <sys/shm.h>
static char *
mkshm(int size)
{
	int	id;
	char	*addr;
	/*
	 * Unfortunately, a declaration of shmat() is missing in old
	 * implementations such as AT&T SVr0 and SunOS.
	 * We cannot add this definition here because the return-type
	 * changed on newer systems.
	 *
	 * We will get a warning like this:
	 *
	 * warning: assignment of pointer from integer lacks a cast
	 * or
	 * warning: illegal combination of pointer and integer, op =
	 */
/*	extern	char *shmat();*/

	if ((id = shmget(IPC_PRIVATE, size, IPC_CREAT|0600)) == -1)
		comerr("shmget failed\n");

	if (debug) errmsgno(EX_BAD, "shared memory segment allocated: %d\n", id);

	if ((addr = shmat(id, (char *)0, 0600)) == (char *)-1)
		comerr("shmat failed\n");

	if (debug) errmsgno(EX_BAD, "shared memory segment attached at: %p size %d\n",
				(void *)addr, size);

	if (shmctl(id, IPC_RMID, 0) < 0)
		comerr("shmctl failed to detach shared memory segment\n");

#ifdef	SHM_LOCK
	/*
	 * Although SHM_LOCK is standard, it seems that all versions of AIX
	 * ommit this definition.
	 */
	if (shmctl(id, SHM_LOCK, 0) < 0)
		comerr("shmctl failed to lock shared memory segment\n");
#endif

	return (addr);
}
#endif

#ifdef	USE_OS2SHM
static char *
mkos2shm(int size)
{
	char	*addr;

	/*
	 * The OS/2 implementation of shm (using shm.dll) limits the size of one shared
	 * memory segment to 0x3fa000 (aprox. 4MBytes). Using OS/2 native API we have
	 * no such restriction so I decided to use it allowing fifos of arbitrary size.
	 */
	if (DosAllocSharedMem(&addr, NULL, size, 0X100L | 0x1L | 0x2L | 0x10L))
		comerr("DosAllocSharedMem() failed\n");

	if (debug) errmsgno(EX_BAD, "shared memory allocated attached at: %p size %d\n",
				(void *)addr, size);

	return (addr);
}
#endif

#ifdef	USE_BEOS_AREAS
static	area_id	faio_aid;
static	void	*faio_addr;
static	char	faio_name[32];

static char *
mkbeosshm(int size)
{
	snprintf(faio_name, sizeof (faio_name), "cdrecord FIFO %lld",
		(Llong)getpid());

	faio_aid = create_area(faio_name, &faio_addr,
			B_ANY_ADDRESS,
			size,
			B_NO_LOCK, B_READ_AREA|B_WRITE_AREA);
	if (faio_addr == NULL) {
		comerrno(faio_aid,
			"Cannot get create_area for %d Bytes FIFO.\n", size);
	}
	if (debug) errmsgno(EX_BAD, "shared memory allocated attached at: %p size %d\n",
				(void *)faio_addr, size);
	return (faio_addr);
}

static void
beosshm_child()
{
	/*
	 * Delete the area created by fork that is copy-on-write.
	 */
	delete_area(area_for(faio_addr));
	/*
	 * Clone (share) the original one.
	 */
	faio_aid = clone_area(faio_name, &faio_addr,
			B_ANY_ADDRESS, B_READ_AREA|B_WRITE_AREA,
			faio_aid);
	if (bufbase != faio_addr) {
		errmsgno(EX_BAD, "Panic FIFO addr.\n");
		return (FALSE);
	}
}
#endif

static	int	faio_buffers;
static	int	faio_buf_size;
static	int	buf_idx = 0;		/* Initialize to fix an Amiga bug   */
static	int	buf_idx_reader = 0;	/* Separate var to allow vfork()    */
					/* buf_idx_reader is for the process */
					/* that fills the FIFO		    */
static	pid_t	faio_pid = -1;
static	BOOL	faio_didwait;

#ifdef AMIGA
/*
 * On Amiga fork will be replaced by the speciall vfork() like call ix_vfork,
 * which lets the parent asleep. The child process later wakes up the parent
 * process by calling ix_fork_resume().
 */
#define	fork()		 ix_vfork()
#define	__vfork_resume() ix_vfork_resume()

#else	/* !AMIGA */
#define	__vfork_resume()
#endif


/*#define	faio_ref(n)	(&((faio_t *)buf)[n])*/


BOOL
init_faio(track_t *trackp, int bufsize)
{
	int	n;
	faio_t	*f;
	int	pagesize;
	char	*base;

	if (buflen == 0L)
		return (FALSE);

	pagesize = getpagesize();
	faio_buf_size = bufsize;
	f = (faio_t *)buf;

	/*
	 * Compute space for buffer headers.
	 * Round bufsize up to pagesize to make each FIFO segment
	 * properly page aligned.
	 */
	bufsize = roundup(bufsize, pagesize);
	faio_buffers = (buflen - sizeof (*sp)) / bufsize;
	EDEBUG(("bufsize: %d buffers: %d hdrsize %ld\n", bufsize, faio_buffers, (long)faio_buffers * sizeof (struct faio)));

	/*
	 * Reduce buffer space by header space.
	 */
	n = sizeof (*sp) + faio_buffers * sizeof (struct faio);
	n = roundup(n, pagesize);
	faio_buffers = (buflen-n) / bufsize;
	EDEBUG(("bufsize: %d buffers: %d hdrsize %ld\n", bufsize, faio_buffers, (long)faio_buffers * sizeof (struct faio)));

	if (faio_buffers < MIN_BUFFERS) {
		errmsgno(EX_BAD,
			"write-buffer too small, minimum is %dk. Disabling.\n",
						MIN_BUFFERS*bufsize/1024);
		return (FALSE);
	}

	if (debug)
		printf("Using %d buffers of %d bytes.\n", faio_buffers, faio_buf_size);

	f = (faio_t *)buf;
	base = buf + roundup(sizeof (*sp) + faio_buffers * sizeof (struct faio),
				pagesize);

	for (n = 0; n < faio_buffers; n++, f++, base += bufsize) {
		/* Give all the buffers to the file reader process */
		f->owner = owner_writer;
		f->users = 0;
		f->bufp = base;
		f->fd = -1;
	}
	sp = (struct faio_stats *)f;	/* point past headers */
	sp->gets = sp->puts = sp->done = 0L;
	sp->users = 1;

	faio_pid = fork();
	if (faio_pid < 0)
		comerr("fork(2) failed");

	if (faio_pid == 0) {
		/*
		 * child (background) process that fills the FIFO.
		 */
		raisepri(1);		/* almost max priority */

#ifdef USE_OS2SHM
		DosGetSharedMem(buf, 3); /* PAG_READ|PAG_WRITE */
#endif
#ifdef	USE_BEOS_AREAS
		beosshm_child();
#endif
		/* Ignoring SIGALRM cures the SCO usleep() bug */
/*		signal(SIGALRM, SIG_IGN);*/
		__vfork_resume();	/* Needed on some platforms */
		faio_reader(trackp);
		/* NOTREACHED */
	} else {
#ifdef	__needed__
		Uint	t;
#endif

		faio_didwait = FALSE;

		/*
		 * XXX We used to close all track files in the foreground
		 * XXX process. This was not correct before we used "xio"
		 * XXX and with "xio" it will start to fail because we need
		 * XXX the fd handles for the faio_get_buf() function.
		 */
#ifdef	__needed__
		/* close all file-descriptors that only the child will use */
		for (t = 1; t <= trackp->tracks; t++) {
			if (trackp[t].xfp != NULL)
				xclose(trackp[t].xfp);
		}
#endif
	}

	return (TRUE);
}

BOOL
await_faio()
{
	int	n;
	int	lastfd = -1;
	faio_t	*f;

	/*
	 * Wait until the reader is active and has filled the buffer.
	 */
	if (lverbose || debug) {
		printf("Waiting for reader process to fill input buffer ... ");
		flush();
	}

	faio_wait_on_buffer(faio_ref(faio_buffers - 1), owner_reader,
			    500*MSECS, 0);

	if (lverbose || debug)
		printf("input buffer ready.\n");

	sp->empty = sp->full = 0L;	/* set correct stat state */
	sp->cont_low = faio_buffers;	/* set cont to max value  */

	f = faio_ref(0);
	for (n = 0; n < faio_buffers; n++, f++) {
		if (f->fd != lastfd &&
			f->fd == STDIN_FILENO && f->len == 0) {
			errmsgno(EX_BAD, "Premature EOF on stdin.\n");
			kill(faio_pid, SIGKILL);
			return (FALSE);
		}
		lastfd = f->fd;
	}
	return (TRUE);
}

void
kill_faio()
{
	if (faio_pid > 0)
		kill(faio_pid, SIGKILL);
  faio_pid=-1;
}

int
wait_faio()
{
	if (faio_pid > 0 && !faio_didwait)
		return (wait(0));
	faio_didwait = TRUE;
	return (0);
}

static void
faio_reader(track_t *trackp)
{
	/* This function should not return, but _exit. */
	Uint	trackno;

	if (debug)
		printf("\nfaio_reader starting\n");

	for (trackno = 1; trackno <= trackp->tracks; trackno++) {
		if (debug)
			printf("\nfaio_reader reading track %u\n", trackno);
		faio_read_track(&trackp[trackno]);
	}
	sp->done++;
	if (debug)
		printf("\nfaio_reader all tracks read, exiting\n");

	/* Prevent hang if buffer is larger than all the tracks combined */
	if (sp->gets == 0)
		faio_ref(faio_buffers - 1)->owner = owner_reader;

#ifdef	USE_OS2SHM
	DosFreeMem(buf);
	sleep(30000);	/* XXX If calling _exit() here the parent process seems to be blocked */
			/* XXX This should be fixed soon */
#endif
	if (debug)
		fprintf(stderr, "\nfaio_reader _exit(0)\n");
	_exit(0);
}

#ifndef	faio_ref
static faio_t *
faio_ref(int n)
{
	return (&((faio_t *)buf)[n]);
}
#endif


static void
faio_read_track(track_t *trackp)
{
	int	fd = -1;
	int	bytespt = trackp->secsize * trackp->secspt;
	int	secspt = trackp->secspt;
	int	l;
	long	secno = trackp->trackstart;
	tsize_t	tracksize = trackp->tracksize;
	tsize_t	bytes_read = (tsize_t)0;
	long	bytes_to_read;

	if (trackp->xfp != NULL)
		fd = xfileno(trackp->xfp);

	if (bytespt > faio_buf_size) {
		comerrno(EX_BAD,
		"faio_read_track fatal: secsize %d secspt %d, bytespt(%d) > %d !!\n",
			trackp->secsize, trackp->secspt, bytespt,
			faio_buf_size);
	}

	do {
		bytes_to_read = bytespt;
		if (tracksize > 0) {
			if ((tracksize - bytes_read) > bytespt) {
				bytes_to_read = bytespt;
			} else {
				bytes_to_read = tracksize - bytes_read;
			}
		}
		l = faio_read_segment(fd, faio_ref(buf_idx_reader), trackp, secno, bytes_to_read);
		if (++buf_idx_reader >= faio_buffers)
			buf_idx_reader = 0;
		if (l <= 0)
			break;
		bytes_read += l;
		secno += secspt;
	} while (tracksize < 0 || bytes_read < tracksize);

	xclose(trackp->xfp);	/* Don't keep files open longer than neccesary */
}

static void
#ifdef	PROTOTYPES
faio_wait_on_buffer(faio_t *f, fowner_t s,
			unsigned long delay,
			unsigned long max_wait)
#else
faio_wait_on_buffer(faio_t *f, fowner_t *s, unsigned long delay, unsigned long max_wait)
#endif
{
	unsigned long max_loops;

	if (f->owner == s)
		return;		/* return immediately if the buffer is ours */

	if (s == owner_reader)
		sp->empty++;
	else
		sp->full++;

	max_loops = max_wait / delay + 1;

	while (max_wait == 0 || max_loops--) {
		USDEBUG1;
		usleep(delay);
		USDEBUG2;

		if (f->owner == s)
			return;
	}
	if (debug) {
		errmsgno(EX_BAD,
		"%lu microseconds passed waiting for %d current: %d idx: %ld\n",
		max_wait, s, f->owner, (long)(f - faio_ref(0))/sizeof (*f));
	}
	comerrno(EX_BAD, "faio_wait_on_buffer for %s timed out.\n",
	(s > owner_reader || s < owner_none) ? "bad_owner" : onames[s-owner_none]);
}

static int
faio_read_segment(int fd, faio_t *f, track_t *trackp, long secno, int len)
{
	int l;

	faio_wait_on_buffer(f, owner_writer, WRITER_DELAY, WRITER_MAXWAIT);

	f->fd = fd;
	l = fill_buf(fd, trackp, secno, f->bufp, len);
	f->len = l;
	f->saved_errno = geterrno();
	f->owner = owner_reader;
	f->users = sp->users;

	sp->puts++;

	return (l);
}

int
faio_read_buf(int fd, char *bp, int size)
{
	char *bufp;

	int len = faio_get_buf(fd, &bufp, size);
	if (len > 0) {
		movebytes(bufp, bp, len);
	}
	return (len);
}

int
faio_get_buf(int fd, char **bpp, int size)
{
	faio_t	*f;
	int	len;

again:
	f = faio_ref(buf_idx);
	if (f->owner == owner_faio) {
		f->owner = owner_writer;
		if (++buf_idx >= faio_buffers)
			buf_idx = 0;
		f = faio_ref(buf_idx);
	}

	if ((sp->puts - sp->gets) < sp->cont_low && sp->done == 0) {
		EDEBUG(("gets: %ld puts: %ld cont: %ld low: %ld\n", sp->gets, sp->puts, sp->puts - sp->gets, sp->cont_low));
		sp->cont_low = sp->puts - sp->gets;
	}
	faio_wait_on_buffer(f, owner_reader, READER_DELAY, READER_MAXWAIT);
	len = f->len;

	if (f->fd != fd) {
		if (f->len == 0) {
			/*
			 * If the tracksize for this track was known, and
			 * the tracksize is 0 mod bytespt, this happens.
			 */
			goto again;
		}
		comerrno(EX_BAD,
		"faio_get_buf fatal: fd=%d, f->fd=%d, f->len=%d f->errno=%d\n",
		fd, f->fd, f->len, f->saved_errno);
	}
	if (size < len) {
		comerrno(EX_BAD,
		"unexpected short read-attempt in faio_get_buf. size = %d, len = %d\n",
		size, len);
	}

	if (len < 0)
		seterrno(f->saved_errno);

	sp->gets++;

	*bpp = f->bufp;
	if (--f->users <= 0)
		f->owner = owner_faio;
	return (len);
}

void
fifo_stats()
{
	if (sp == NULL)	/* We might not use a FIFO */
		return;

	errmsgno(EX_BAD, "fifo had %ld puts and %ld gets.\n",
		sp->puts, sp->gets);
	errmsgno(EX_BAD, "fifo was %ld times empty and %ld times full, min fill was %ld%%.\n",
		sp->empty, sp->full, (100L*sp->cont_low)/faio_buffers);
}

int
fifo_percent(BOOL addone)
{
	int	percent;

	if (sp == NULL)	/* We might not use a FIFO */
		return (-1);

	if (sp->done)
		return (100);
	percent = (100*(sp->puts + 1 - sp->gets)/faio_buffers);
	if (percent > 100)
		return (100);
	return (percent);
}
#else	/* FIFO */

#include <standard.h>
#include <utypes.h>	/* includes sys/types.h */
#include <schily.h>

#include "wodim.h"

void	init_fifo(long);
BOOL	init_faio(track_t *track, int);
BOOL	await_faio(void);
void	kill_faio(void);
int	wait_faio(void);
int	faio_read_buf(int f, char *bp, int size);
int	faio_get_buf(int f, char **bpp, int size);
void	fifo_stats(void);
int	fifo_percent(BOOL addone);


void init_fifo(long fs)
{
	errmsgno(EX_BAD, "Fifo not supported.\n");
}

BOOL init_faio(track_t *track, 
               int bufsize /* The size of a single transfer buffer */)
{
	return (FALSE);
}

BOOL await_faio()
{
	return (TRUE);
}

void kill_faio()
{
}

int wait_faio()
{
	return (0);
}

int faio_read_buf(int fd, char *bp, int size)
{
	return (0);
}

int faio_get_buf(int fd, char **bpp, int size)
{
	return (0);
}

void fifo_stats()
{
}

int fifo_percent(BOOL addone)
{
	return (-1);
}

#endif	/* FIFO */
