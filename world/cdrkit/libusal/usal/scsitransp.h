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

/* @(#)scsitransp.h	1.54 03/05/03 Copyright 1995 J. Schilling */
/*
 *	Definitions for commands that use functions from scsitransp.c
 *
 *	Copyright (c) 1995 J. Schilling
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

#ifndef	_SCG_SCSITRANSP_H
#define	_SCG_SCSITRANSP_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct usal_scsi	SCSI;

typedef struct {
	int	scsibus;	/* SCSI bus #    for next I/O		*/
	int	target;		/* SCSI target # for next I/O		*/
	int	lun;		/* SCSI lun #    for next I/O		*/
} usal_addr_t;

#ifndef	_SCG_SCGOPS_H
#include <usal/usalops.h>
#endif

typedef	int	(*usal_cb_t)(void *);

struct usal_scsi {
	usal_ops_t *ops;		/* Ptr to low level SCSI transport ops	*/
	int	fd;		/* File descriptor for next I/O		*/
	usal_addr_t	addr;	/* SCSI address for next I/O		*/
	int	flags;		/* Libusal flags (see below)		*/
	int	dflags;		/* Drive specific flags (see below)	*/
	int	kdebug;		/* Kernel debug value for next I/O	*/
	int	debug;		/* Debug value for SCSI library		*/
	int	silent;		/* Be silent if value > 0		*/
	int	verbose;	/* Be verbose if value > 0		*/
	int	overbose;	/* Be verbose in open() if value > 0	*/
	int	disre_disable;
	int	deftimeout;
	int	noparity;	/* Do not use SCSI parity fo next I/O	*/
	int	dev;		/* from scsi_cdr.c			*/
	struct usal_cmd *scmd;
	char	*cmdname;
	char	*curcmdname;
	BOOL	running;
	int	error;		/* libusal error number			*/

	long	maxdma;		/* Max DMA limit for this open instance	*/
	long	maxbuf;		/* Cur DMA buffer limit for this inst.	*/
				/* This is the size behind bufptr	*/
	struct timeval	*cmdstart;
	struct timeval	*cmdstop;
	const char	**nonstderrs;
	void	*local;		/* Local data from the low level code	*/
	void	*bufbase;	/* needed for scsi_freebuf()		*/
	void	*bufptr;	/* DMA buffer pointer for appl. use	*/
	char	*errstr;	/* Error string for scsi_open/sendmcd	*/
	char	*errbeg;	/* Pointer to begin of not flushed data	*/
	char	*errptr;	/* Actual write pointer into errstr	*/
	void	*errfile;	/* FILE to write errors to. NULL for not*/
				/* writing and leaving errs in errstr	*/
	usal_cb_t cb_fun;
	void	*cb_arg;

	struct scsi_inquiry *inq;
	struct scsi_capacity *cap;
};

/*
 * Macros for accessing members of the usal address structure.
 * usal_settarget() is the only function that is allowed to modify
 * the values of the SCSI address.
 */
#define	usal_scsibus(usalp)	(usalp)->addr.scsibus
#define	usal_target(usalp)	(usalp)->addr.target
#define	usal_lun(usalp)		(usalp)->addr.lun

/*
 * Flags for struct SCSI:
 */
/* NONE yet */

/*
 * Drive specific flags for struct SCSI:
 */
#define	DRF_MODE_DMA_OVR	0x0001		/* Drive gives DMA overrun */
						/* on mode sense	   */

#define	SCSI_ERRSTR_SIZE	4096

/*
 * Libusal error codes:
 */
#define	SCG_ERRBASE		1000000
#define	SCG_NOMEM		1000001

/*
 * Function codes for usal_version():
 */
#define	SCG_VERSION		0	/* libusal or transport version */
#define	SCG_AUTHOR		1	/* Author of above */
#define	SCG_SCCS_ID		2	/* SCCS id of above */
#define	SCG_RVERSION		10	/* Remote transport version */
#define	SCG_RAUTHOR		11	/* Remote transport author */
#define	SCG_RSCCS_ID		12	/* Remote transport SCCS ID */
#define	SCG_KVERSION		20	/* Kernel transport version */

/*
 * Function codes for usal_reset():
 */
#define	SCG_RESET_NOP		0	/* Test if reset is supported */
#define	SCG_RESET_TGT		1	/* Reset Target only */
#define	SCG_RESET_BUS		2	/* Reset complete SCSI Bus */

/*
 * Helpers for the error buffer in SCSI*
 */
#define	usal_errsize(usalp)	((usalp)->errptr - (usalp)->errstr)
#define	usal_errrsize(usalp)	(SCSI_ERRSTR_SIZE - usal_errsize(usalp))

/*
 * From scsitransp.c:
 */
extern	char	*usal_version(SCSI *usalp, int what);
extern	int	usal__open(SCSI *usalp, char *device);
extern	int	usal__close(SCSI *usalp);
extern	BOOL	usal_havebus(SCSI *usalp, int);
extern	int	usal_initiator_id(SCSI *usalp);
extern	int	usal_isatapi(SCSI *usalp);
extern	int	usal_reset(SCSI *usalp, int what);
extern	void	*usal_getbuf(SCSI *usalp, long);
extern	void	usal_freebuf(SCSI *usalp);
extern	long	usal_bufsize(SCSI *usalp, long);
extern	void	usal_setnonstderrs(SCSI *usalp, const char **);
extern	BOOL	usal_yes(char *);
extern	int	usal_cmd(SCSI *usalp);
extern	void	usal_vhead(SCSI *usalp);
extern	int	usal_svhead(SCSI *usalp, char *buf, int maxcnt);
extern	int	usal_vtail(SCSI *usalp);
extern	int	usal_svtail(SCSI *usalp, int *retp, char *buf, int maxcnt);
extern	void	usal_vsetup(SCSI *usalp);
extern	int	usal_getresid(SCSI *usalp);
extern	int	usal_getdmacnt(SCSI *usalp);
extern	BOOL	usal_cmd_err(SCSI *usalp);
extern	void	usal_printerr(SCSI *usalp);
#ifdef	EOF	/* stdio.h has been included */
extern	void	usal_fprinterr(SCSI *usalp, FILE *f);
#endif
extern	int	usal_sprinterr(SCSI *usalp, char *buf, int maxcnt);
extern	int	usal__sprinterr(SCSI *usalp, char *buf, int maxcnt);
extern	void	usal_printcdb(SCSI *usalp);
extern	int	usal_sprintcdb(SCSI *usalp, char *buf, int maxcnt);
extern	void	usal_printwdata(SCSI *usalp);
extern	int	usal_sprintwdata(SCSI *usalp, char *buf, int maxcnt);
extern	void	usal_printrdata(SCSI *usalp);
extern	int	usal_sprintrdata(SCSI *usalp, char *buf, int maxcnt);
extern	void	usal_printresult(SCSI *usalp);
extern	int	usal_sprintresult(SCSI *usalp, char *buf, int maxcnt);
extern	void	usal_printstatus(SCSI *usalp);
extern	int	usal_sprintstatus(SCSI *usalp, char *buf, int maxcnt);
#ifdef	EOF	/* stdio.h has been included */
extern	void	usal_fprbytes(FILE *, char *, unsigned char *, int);
extern	void	usal_fprascii(FILE *, char *, unsigned char *, int);
#endif
extern	void	usal_prbytes(char *, unsigned char *, int);
extern	void	usal_prascii(char *, unsigned char *, int);
extern	int	usal_sprbytes(char *buf, int maxcnt, char *, unsigned char *, int);
extern	int	usal_sprascii(char *buf, int maxcnt, char *, unsigned char *, int);
#ifdef	EOF	/* stdio.h has been included */
extern	void	usal_fprsense(FILE *f, unsigned char *, int);
#endif
extern	void	usal_prsense(unsigned char *, int);
extern	int	usal_sprsense(char *buf, int maxcnt, unsigned char *, int);
extern	int	usal_cmd_status(SCSI *usalp);
extern	int	usal_sense_key(SCSI *usalp);
extern	int	usal_sense_code(SCSI *usalp);
extern	int	usal_sense_qual(SCSI *usalp);
#ifdef	_SCG_SCSIREG_H
#ifdef	EOF	/* stdio.h has been included */
extern	void	usal_fprintdev(FILE *, struct scsi_inquiry *);
#endif
extern	void	usal_printdev(struct scsi_inquiry *);
#endif
extern	int	usal_printf(SCSI *usalp, const char *form, ...);
extern	int	usal_errflush(SCSI *usalp);
#ifdef	EOF	/* stdio.h has been included */
extern	int	usal_errfflush(SCSI *usalp, FILE *f);
#endif

/*
 * From scsierrmsg.c:
 */
extern	const char	*usal_sensemsg(int, int, int, const char **, char *, 
											  int maxcnt);
#ifdef	_SCG_SCSISENSE_H
extern	int		usal__errmsg(SCSI *usalp, char *obuf, int maxcnt,
										struct scsi_sense *, struct scsi_status *, int);
#endif

/*
 * From scsiopen.c:
 */
#ifdef	EOF	/* stdio.h has been included */
extern	int	usal_help(FILE *f);
#endif
extern	SCSI	*usal_open(char *scsidev, char *errs, int slen, int odebug, 
								 int be_verbose);
extern	int	usal_close(SCSI * usalp);
extern	void	usal_settimeout(SCSI * usalp, int timeout);
extern	SCSI	*usal_smalloc(void);
extern	void	usal_sfree(SCSI *usalp);

/*
 * From usalsettarget.c:
 */
extern	int	usal_settarget(SCSI *usalp, int scsibus, int target, int lun);

/*
 * From scsi-remote.c:
 */
extern	usal_ops_t *usal_remote(void);

/*
 * From scsihelp.c:
 */
#ifdef	EOF	/* stdio.h has been included */
extern	void	__usal_help(FILE *f, char *name, char *tcomment, char *tind,
								  char *tspec, char *texample, BOOL mayscan, 
								  BOOL bydev);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SCG_SCSITRANSP_H */
