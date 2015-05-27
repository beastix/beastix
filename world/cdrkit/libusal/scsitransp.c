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

/* @(#)scsitransp.c	1.91 04/06/17 Copyright 1988,1995,2000-2004 J. Schilling */
/*#ifndef lint*/
static	char sccsid[] =
	"@(#)scsitransp.c	1.91 04/06/17 Copyright 1988,1995,2000-2004 J. Schilling";
/*#endif*/
/*
 *	SCSI user level command transport routines (generic part).
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1988,1995,2000-2004 J. Schilling
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

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <errno.h>
#include <timedefs.h>
#include <strdefs.h>
#include <schily.h>

#include <usal/usalcmd.h>
#include <usal/scsireg.h>
#include <usal/scsitransp.h>
#include "usaltimes.h"

#include <stdarg.h>


/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_version[]		= CDRKIT_VERSION;	/* The global libusal version	*/
static	char	_usal_auth_cdrkit[]	= "Cdrkit";	/* The author for this module	*/

#define	DEFTIMEOUT	20	/* Default timeout for SCSI command transport */

char	*usal_version(SCSI *usalp, int what);
int	usal__open(SCSI *usalp, char *device);
int	usal__close(SCSI *usalp);
BOOL	usal_havebus(SCSI *usalp, int);
int	usal_initiator_id(SCSI *usalp);
int	usal_isatapi(SCSI *usalp);
int	usal_reset(SCSI *usalp, int what);
void	*usal_getbuf(SCSI *usalp, long);
void	usal_freebuf(SCSI *usalp);
long	usal_bufsize(SCSI *usalp, long);
void	usal_setnonstderrs(SCSI *usalp, const char **);
BOOL	usal_yes(char *);
#ifdef	nonono
static	void	usal_sighandler(int);
#endif
int	usal_cmd(SCSI *usalp);
void	usal_vhead(SCSI *usalp);
int	usal_svhead(SCSI *usalp, char *buf, int maxcnt);
int	usal_vtail(SCSI *usalp);
int	usal_svtail(SCSI *usalp, int *retp, char *buf, int maxcnt);
void	usal_vsetup(SCSI *usalp);
int	usal_getresid(SCSI *usalp);
int	usal_getdmacnt(SCSI *usalp);
BOOL	usal_cmd_err(SCSI *usalp);
void	usal_printerr(SCSI *usalp);
void	usal_fprinterr(SCSI *usalp, FILE *f);
int	usal_sprinterr(SCSI *usalp, char *buf, int maxcnt);
int	usal__sprinterr(SCSI *usalp, char *buf, int maxcnt);
void	usal_printcdb(SCSI *usalp);
int	usal_sprintcdb(SCSI *usalp, char *buf, int maxcnt);
void	usal_printwdata(SCSI *usalp);
int	usal_sprintwdata(SCSI *usalp, char *buf, int maxcnt);
void	usal_printrdata(SCSI *usalp);
int	usal_sprintrdata(SCSI *usalp, char *buf, int maxcnt);
void	usal_printresult(SCSI *usalp);
int	usal_sprintresult(SCSI *usalp, char *buf, int maxcnt);
void	usal_printstatus(SCSI *usalp);
int	usal_sprintstatus(SCSI *usalp, char *buf, int maxcnt);
void	usal_fprbytes(FILE *, char *, unsigned char *, int);
void	usal_fprascii(FILE *, char *, unsigned char *, int);
void	usal_prbytes(char *, unsigned char *, int);
void	usal_prascii(char *, unsigned char *, int);
int	usal_sprbytes(char *buf, int maxcnt, char *, unsigned char *, int);
int	usal_sprascii(char *buf, int maxcnt, char *, unsigned char *, int);
void	usal_fprsense(FILE *f, unsigned char *, int);
int	usal_sprsense(char *buf, int maxcnt, unsigned char *, int);
void	usal_prsense(unsigned char *, int);
int	usal_cmd_status(SCSI *usalp);
int	usal_sense_key(SCSI *usalp);
int	usal_sense_code(SCSI *usalp);
int	usal_sense_qual(SCSI *usalp);
unsigned char *usal_sense_table(SCSI *usalp);
void	usal_fprintdev(FILE *, struct scsi_inquiry *);
void	usal_printdev(struct scsi_inquiry *);
int	usal_printf(SCSI *usalp, const char *form, ...);
int	usal_errflush(SCSI *usalp);
int	usal_errfflush(SCSI *usalp, FILE *f);

/*
 * Return version information for the SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 *
 * If usalp is NULL, return general library version information.
 * If usalp is != NULL, return version information for the low level transport.
 */
char *
usal_version(SCSI *usalp, int what)
{
	if (usalp == (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (sccsid);
		default:
			return ((char *)0);
		}
	}
	return (SCGO_VERSION(usalp, what));
}

/*
 * Call low level SCSI open routine from transport abstraction layer.
 */
int
usal__open(SCSI *usalp, char *device)
{
	int	ret;
	usal_ops_t *ops;
extern	usal_ops_t usal_std_ops;

	usalp->ops = &usal_std_ops;

	if (device && strncmp(device, "REMOTE", 6) == 0) {
		ops = usal_remote();
		if (ops != NULL)
			usalp->ops = ops;
	}

	ret = SCGO_OPEN(usalp, device);
	if (ret < 0)
		return (ret);

	/*
	 * Now make usalp->fd valid if possible.
	 * Note that usal_scsibus(usalp)/usal_target(usalp)/usal_lun(usalp) may have
	 * changed in SCGO_OPEN().
	 */
	usal_settarget(usalp, usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp));
	return (ret);
}

/*
 * Call low level SCSI close routine from transport abstraction layer.
 */
int
usal__close(SCSI *usalp)
{
	return (SCGO_CLOSE(usalp));
}

/*
 * Retrieve max DMA count for this target.
 */
long
usal_bufsize(SCSI *usalp, long amt)
{
	long	maxdma;

	maxdma = SCGO_MAXDMA(usalp, amt);
	if (amt <= 0 || amt > maxdma)
		amt = maxdma;

	usalp->maxdma = maxdma;	/* Max possible  */
	usalp->maxbuf = amt;	/* Current value */

	return (amt);
}

/*
 * Allocate a buffer that may be used for DMA.
 */
void *
usal_getbuf(SCSI *usalp, long amt)
{
	void	*buf;

	if (amt <= 0 || amt > usal_bufsize(usalp, amt))
		return ((void *)0);

	buf = SCGO_GETBUF(usalp, amt);
	usalp->bufptr = buf;
	return (buf);
}

/*
 * Free DMA buffer.
 */
void
usal_freebuf(SCSI *usalp)
{
	SCGO_FREEBUF(usalp);
	usalp->bufptr = NULL;
}

/*
 * Check if 'busno' is a valid SCSI bus number.
 */
BOOL
usal_havebus(SCSI *usalp, int busno)
{
	return (SCGO_HAVEBUS(usalp, busno));
}

/*
 * Return SCSI initiator ID for current SCSI bus if available.
 */
int
usal_initiator_id(SCSI *usalp)
{
	return (SCGO_INITIATOR_ID(usalp));
}

/*
 * Return a hint whether current SCSI target refers to a ATAPI device.
 */
int
usal_isatapi(SCSI *usalp)
{
	return (SCGO_ISATAPI(usalp));
}

/*
 * Reset SCSI bus or target.
 */
int
usal_reset(SCSI *usalp, int what)
{
	return (SCGO_RESET(usalp, what));
}

/*
 * Set up nonstd error vector for curren target.
 * To clear additional error table, call usal_setnonstderrs(usalp, NULL);
 * Note: do not use this when scanning the SCSI bus.
 */
void
usal_setnonstderrs(SCSI *usalp, const char **vec)
{
	usalp->nonstderrs = vec;
}

/*
 * Simple Yes/No answer checker.
 */
BOOL
usal_yes(char *msg)
{
	char okbuf[10];

	printf("%s", msg);
	flush();
	if (getline(okbuf, sizeof (okbuf)) == EOF)
		exit(EX_BAD);
	if (streql(okbuf, "y") || streql(okbuf, "yes") ||
	    streql(okbuf, "Y") || streql(okbuf, "YES"))
		return (TRUE);
	else
		return (FALSE);
}

#ifdef	nonono
static void
usal_sighandler(int sig)
{
	printf("\n");
	if (scsi_running) {
		printf("Running command: %s\n", scsi_command);
		printf("Resetting SCSI - Bus.\n");
		if (usal_reset(usalp) < 0)
			errmsg("Cannot reset SCSI - Bus.\n");
	}
	if (usal_yes("EXIT ? "))
		exit(sig);
}
#endif

/*
 * Send a SCSI command.
 * Do error checking and reporting depending on the values of
 * usalp->verbose, usalp->debug and usalp->silent.
 */
int
usal_cmd(SCSI *usalp)
{
		int		ret;
	register struct	usal_cmd	*scmd = usalp->scmd;

	/*
	 * Reset old error messages in usalp->errstr
	 */
	usalp->errptr = usalp->errbeg = usalp->errstr;

	scmd->kdebug = usalp->kdebug;
	if (scmd->timeout == 0 || scmd->timeout < usalp->deftimeout)
		scmd->timeout = usalp->deftimeout;
	if (usalp->disre_disable)
		scmd->flags &= ~SCG_DISRE_ENA;
	if (usalp->noparity)
		scmd->flags |= SCG_NOPARITY;

	scmd->u_sense.cmd_sense[0] = 0;		/* Paranioa */
	if (scmd->sense_len > SCG_MAX_SENSE)
		scmd->sense_len = SCG_MAX_SENSE;
	else if (scmd->sense_len < 0)
		scmd->sense_len = 0;

	if (usalp->verbose) {
		usal_vhead(usalp);
		usal_errflush(usalp);
	}

	if (usalp->running) {
		if (usalp->curcmdname) {
			fprintf(stderr, "Currently running '%s' command.\n",
							usalp->curcmdname);
		}
		raisecond("SCSI ALREADY RUNNING !!", 0L);
	}
	usalp->cb_fun = NULL;
	gettimeofday(usalp->cmdstart, (struct timezone *)0);
	usalp->curcmdname = usalp->cmdname;
	usalp->running = TRUE;
	ret = SCGO_SEND(usalp);
	usalp->running = FALSE;
	__usal_times(usalp);
	if (ret < 0) {
		/*
		 * Old /dev/usal versions will not allow to access targets > 7.
		 * Include a workaround to make this non fatal.
		 */
		if (usal_target(usalp) < 8 || geterrno() != EINVAL)
			comerr("Cannot send SCSI cmd via ioctl\n");
		if (scmd->ux_errno == 0)
			scmd->ux_errno = geterrno();
		if (scmd->error == SCG_NO_ERROR)
			scmd->error = SCG_FATAL;
		if (usalp->debug > 0) {
			errmsg("ret < 0 errno: %d ux_errno: %d error: %d\n",
					geterrno(), scmd->ux_errno, scmd->error);
		}
	}

	ret = usal_vtail(usalp);
	usal_errflush(usalp);
	if (usalp->cb_fun != NULL)
		(*usalp->cb_fun)(usalp->cb_arg);
	return (ret);
}

/*
 * Fill the head of verbose printing into the SCSI error buffer.
 * Action depends on SCSI verbose status.
 */
void
usal_vhead(SCSI *usalp)
{
	usalp->errptr += usal_svhead(usalp, usalp->errptr, usal_errrsize(usalp));
}

/*
 * Fill the head of verbose printing into a buffer.
 * Action depends on SCSI verbose status.
 */
int
usal_svhead(SCSI *usalp, char *buf, int maxcnt)
{
	register char	*p = buf;
	register int	amt;

	if (usalp->verbose <= 0)
		return (0);

	amt = snprintf(p, maxcnt,
		"\nExecuting '%s' command on Bus %d Target %d, Lun %d timeout %ds\n",
								/* XXX Really this ??? */
/*		usalp->cmdname, usal_scsibus(usalp), usal_target(usalp), usalp->scmd->cdb.g0_cdb.lun,*/
		usalp->cmdname, usal_scsibus(usalp), usal_target(usalp), usal_lun(usalp),
		usalp->scmd->timeout);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;

	amt = usal_sprintcdb(usalp, p, maxcnt);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;

	if (usalp->verbose > 1) {
		amt = usal_sprintwdata(usalp, p, maxcnt);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	return (p - buf);
}

/*
 * Fill the tail of verbose printing into the SCSI error buffer.
 * Action depends on SCSI verbose status.
 */
int
usal_vtail(SCSI *usalp)
{
	int	ret;

	usalp->errptr += usal_svtail(usalp, &ret, usalp->errptr, usal_errrsize(usalp));
	return (ret);
}

/*
 * Fill the tail of verbose printing into a buffer.
 * Action depends on SCSI verbose status.
 */
int
usal_svtail(SCSI *usalp, int *retp, char *buf, int maxcnt)
{
	register char	*p = buf;
	register int	amt;
	int	ret;

	ret = usal_cmd_err(usalp) ? -1 : 0;
	if (retp)
		*retp = ret;
	if (ret) {
		if (usalp->silent <= 0 || usalp->verbose) {
			amt = usal__sprinterr(usalp, p, maxcnt);
			if (amt < 0)
				return (amt);
			p += amt;
			maxcnt -= amt;
		}
	}
	if ((usalp->silent <= 0 || usalp->verbose) && usalp->scmd->resid) {
		if (usalp->scmd->resid < 0) {
			/*
			 * An operating system that does DMA the right way
			 * will not allow DMA overruns - it will stop DMA
			 * before bad things happen.
			 * A DMA residual count < 0 (-1) is a hint for a DMA
			 * overrun but does not affect the transfer count.
			 */
			amt = snprintf(p, maxcnt, "DMA overrun, ");
			if (amt < 0)
				return (amt);
			p += amt;
			maxcnt -= amt;
		}
		amt = snprintf(p, maxcnt, "resid: %d\n", usalp->scmd->resid);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	if (usalp->verbose > 0 || (ret < 0 && usalp->silent <= 0)) {
		amt = usal_sprintresult(usalp, p, maxcnt);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	return (p - buf);
}

/*
 * Set up SCSI error buffer with verbose print data.
 * Action depends on SCSI verbose status.
 */
void
usal_vsetup(SCSI *usalp)
{
	usal_vhead(usalp);
	usal_vtail(usalp);
}

/*
 * Return the residual DMA count for last command.
 * If this count is < 0, then a DMA overrun occured.
 */
int
usal_getresid(SCSI *usalp)
{
	return (usalp->scmd->resid);
}

/*
 * Return the actual DMA count for last command.
 */
int
usal_getdmacnt(SCSI *usalp)
{
	register struct usal_cmd *scmd = usalp->scmd;

	if (scmd->resid < 0)
		return (scmd->size);

	return (scmd->size - scmd->resid);
}

/*
 * Test if last SCSI command got an error.
 */
BOOL
usal_cmd_err(SCSI *usalp)
{
	register struct usal_cmd *cp = usalp->scmd;

	if (cp->error != SCG_NO_ERROR ||
				cp->ux_errno != 0 ||
				*(Uchar *)&cp->scb != 0 ||
				cp->u_sense.cmd_sense[0] != 0)	/* Paranioa */
		return (TRUE);
	return (FALSE);
}

/*
 * Used to print error messges if the command itself has been run silently.
 *
 * print the following SCSI codes:
 *
 * -	command transport status
 * -	CDB
 * -	SCSI status byte
 * -	Sense Bytes
 * -	Decoded Sense data
 * -	DMA status
 * -	SCSI timing
 *
 * to SCSI errfile.
 */
void
usal_printerr(SCSI *usalp)
{
	usal_fprinterr(usalp, (FILE *)usalp->errfile);
}

/*
 * print the following SCSI codes:
 *
 * -	command transport status
 * -	CDB
 * -	SCSI status byte
 * -	Sense Bytes
 * -	Decoded Sense data
 * -	DMA status
 * -	SCSI timing
 *
 * to a file.
 */
void
usal_fprinterr(SCSI *usalp, FILE *f)
{
	char	errbuf[SCSI_ERRSTR_SIZE];
	int	amt;

	amt = usal_sprinterr(usalp, errbuf, sizeof (errbuf));
	if (amt > 0) {
		filewrite(f, errbuf, amt);
		fflush(f);
	}
}

/*
 * print the following SCSI codes:
 *
 * -	command transport status
 * -	CDB
 * -	SCSI status byte
 * -	Sense Bytes
 * -	Decoded Sense data
 * -	DMA status
 * -	SCSI timing
 *
 * into a buffer.
 */
int
usal_sprinterr(SCSI *usalp, char *buf, int maxcnt)
{
	int	amt;
	int	osilent = usalp->silent;
	int	overbose = usalp->verbose;

	usalp->silent = 0;
	usalp->verbose = 0;
	amt = usal_svtail(usalp, NULL, buf, maxcnt);
	usalp->silent = osilent;
	usalp->verbose = overbose;
	return (amt);
}

/*
 * print the following SCSI codes:
 *
 * -	command transport status
 * -	CDB
 * -	SCSI status byte
 * -	Sense Bytes
 * -	Decoded Sense data
 *
 * into a buffer.
 */
int
usal__sprinterr(SCSI *usalp, char *buf, int maxcnt)
{
	register struct usal_cmd *cp = usalp->scmd;
	register char		*err;
		char		*cmdname = "SCSI command name not set by caller";
		char		errbuf[64];
	register char		*p = buf;
	register int		amt;

	switch (cp->error) {

	case SCG_NO_ERROR :	err = "no error"; break;
	case SCG_RETRYABLE:	err = "retryable error"; break;
	case SCG_FATAL    :	err = "fatal error"; break;
				/*
				 * We need to cast timeval->* to long because
				 * of the broken sys/time.h in Linux.
				 */
	case SCG_TIMEOUT  :	snprintf(errbuf, sizeof (errbuf),
					"cmd timeout after %ld.%03ld (%d) s",
					(long)usalp->cmdstop->tv_sec,
					(long)usalp->cmdstop->tv_usec/1000,
								cp->timeout);
				err = errbuf;
				break;
	default:		snprintf(errbuf, sizeof (errbuf),
					"error: %d", cp->error);
				err = errbuf;
	}

	if (usalp->cmdname != NULL && usalp->cmdname[0] != '\0')
		cmdname = usalp->cmdname;
	/*amt = serrmsgno(cp->ux_errno, p, maxcnt, "%s: scsi sendcmd: %s\n", cmdname, err);
	if (amt < 0)
		return (amt);
    */
  amt=snprintf(p, maxcnt, "Errno: %d (%s), %s scsi sendcmd: %s\n", cp->ux_errno, strerror(cp->ux_errno), cmdname, err);
  if(amt>=maxcnt || amt<0)
     return (amt);
	p += amt;
	maxcnt -= amt;

	amt = usal_sprintcdb(usalp, p, maxcnt);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;

	if (cp->error <= SCG_RETRYABLE) {
		amt = usal_sprintstatus(usalp, p, maxcnt);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}

	if (cp->scb.chk) {
		amt = usal_sprsense(p, maxcnt, (Uchar *)&cp->sense, cp->sense_count);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
		amt = usal__errmsg(usalp, p, maxcnt, &cp->sense, &cp->scb, -1);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	return (p - buf);
}

/*
 * XXX Do we need this function?
 *
 * print the SCSI Command descriptor block to XXX stderr.
 */
void
usal_printcdb(SCSI *usalp)
{
	usal_prbytes("CDB: ", usalp->scmd->cdb.cmd_cdb, usalp->scmd->cdb_len);
}

/*
 * print the SCSI Command descriptor block into a buffer.
 */
int
usal_sprintcdb(SCSI *usalp, char *buf, int maxcnt)
{
	int	cnt;

	cnt = usal_sprbytes(buf, maxcnt, "CDB: ", usalp->scmd->cdb.cmd_cdb, usalp->scmd->cdb_len);
	if (cnt < 0)
		cnt = 0;
	return (cnt);
}

/*
 * XXX Do we need this function?
 * XXX usal_printrdata() is used.
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print the SCSI send data to stderr.
 */
void
usal_printwdata(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;

	if (scmd->size > 0 && (scmd->flags & SCG_RECV_DATA) == 0) {
		fprintf(stderr, "Sending %d (0x%X) bytes of data.\n",
			scmd->size, scmd->size);
		usal_prbytes("Write Data: ",
			(Uchar *)scmd->addr,
			scmd->size > 100 ? 100 : scmd->size);
	}
}

/*
 * print the SCSI send data into a buffer.
 */
int
usal_sprintwdata(SCSI *usalp, char *buf, int maxcnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	register char		*p = buf;
	register int		amt;

	if (scmd->size > 0 && (scmd->flags & SCG_RECV_DATA) == 0) {
		amt = snprintf(p, maxcnt,
			"Sending %d (0x%X) bytes of data.\n",
			scmd->size, scmd->size);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
		amt = usal_sprbytes(p, maxcnt, "Write Data: ",
			(Uchar *)scmd->addr,
			scmd->size > 100 ? 100 : scmd->size);
		if (amt < 0)
			return (amt);
		p += amt;
	}
	return (p - buf);
}

/*
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print the SCSI received data to stderr.
 */
void
usal_printrdata(SCSI *usalp)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	register int		trcnt = usal_getdmacnt(usalp);

	if (scmd->size > 0 && (scmd->flags & SCG_RECV_DATA) != 0) {
		fprintf(stderr, "Got %d (0x%X), expecting %d (0x%X) bytes of data.\n",
			trcnt, trcnt,
			scmd->size, scmd->size);
		usal_prbytes("Received Data: ",
			(Uchar *)scmd->addr,
			trcnt > 100 ? 100 : trcnt);
	}
}

/*
 * print the SCSI received data into a buffer.
 */
int
usal_sprintrdata(SCSI *usalp, char *buf, int maxcnt)
{
	register struct	usal_cmd	*scmd = usalp->scmd;
	register char		*p = buf;
	register int		amt;
	register int		trcnt = usal_getdmacnt(usalp);

	if (scmd->size > 0 && (scmd->flags & SCG_RECV_DATA) != 0) {
		amt = snprintf(p, maxcnt,
			"Got %d (0x%X), expecting %d (0x%X) bytes of data.\n",
			trcnt, trcnt,
			scmd->size, scmd->size);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
		amt = usal_sprbytes(p, maxcnt,
			"Received Data: ",
			(Uchar *)scmd->addr,
			trcnt > 100 ? 100 : trcnt);
		if (amt < 0)
			return (amt);
		p += amt;
	}
	return (p - buf);
}

/*
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print the SCSI timings and (depending on verbose) received data to stderr.
 */
void
usal_printresult(SCSI *usalp)
{
	fprintf(stderr, "cmd finished after %ld.%03lds timeout %ds\n",
		(long)usalp->cmdstop->tv_sec,
		(long)usalp->cmdstop->tv_usec/1000,
		usalp->scmd->timeout);
	if (usalp->verbose > 1)
		usal_printrdata(usalp);
	flush();
}

/*
 * print the SCSI timings and (depending on verbose) received data into a buffer.
 */
int
usal_sprintresult(SCSI *usalp, char *buf, int maxcnt)
{
	register char		*p = buf;
	register int		amt;

	amt = snprintf(p, maxcnt,
		"cmd finished after %ld.%03lds timeout %ds\n",
		(long)usalp->cmdstop->tv_sec,
		(long)usalp->cmdstop->tv_usec/1000,
		usalp->scmd->timeout);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;
	if (usalp->verbose > 1) {
		amt = usal_sprintrdata(usalp, p, maxcnt);
		if (amt < 0)
			return (amt);
		p += amt;
	}
	return (p - buf);
}

/*
 * XXX Do we need this function?
 *
 * print the SCSI status byte in human readable form to the SCSI error file.
 */
void
usal_printstatus(SCSI *usalp)
{
	char	errbuf[SCSI_ERRSTR_SIZE];
	int	amt;

	amt = usal_sprintstatus(usalp, errbuf, sizeof (errbuf));
	if (amt > 0) {
		filewrite((FILE *)usalp->errfile, errbuf, amt);
		fflush((FILE *)usalp->errfile);
	}
}

/*
 * print the SCSI status byte in human readable form into a buffer.
 */
int
usal_sprintstatus(SCSI *usalp, char *buf, int maxcnt)
{
	register struct usal_cmd *cp = usalp->scmd;
		char	*err;
		char	*err2 = "";
	register char	*p = buf;
	register int	amt;

	amt = snprintf(p, maxcnt, "status: 0x%x ", *(Uchar *)&cp->scb);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;
#ifdef	SCSI_EXTENDED_STATUS
	if (cp->scb.ext_st1) {
		amt = snprintf(p, maxcnt, "0x%x ", ((Uchar *)&cp->scb)[1]);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	if (cp->scb.ext_st2) {
		amt = snprintf(p, maxcnt, "0x%x ", ((Uchar *)&cp->scb)[2]);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
#endif
	switch (*(Uchar *)&cp->scb & 036) {

	case 0  : err = "GOOD STATUS";			break;
	case 02 : err = "CHECK CONDITION";		break;
	case 04 : err = "CONDITION MET/GOOD";		break;
	case 010: err = "BUSY";				break;
	case 020: err = "INTERMEDIATE GOOD STATUS";	break;
	case 024: err = "INTERMEDIATE CONDITION MET/GOOD"; break;
	case 030: err = "RESERVATION CONFLICT";		break;
	default : err = "Reserved";			break;
	}
#ifdef	SCSI_EXTENDED_STATUS
	if (cp->scb.ext_st1 && cp->scb.ha_er)
		err2 = " host adapter detected error";
#endif
	amt = snprintf(p, maxcnt, "(%s%s)\n", err, err2);
	if (amt < 0)
		return (amt);
	p += amt;
	return (p - buf);
}

/*
 * print some bytes in hex to a file.
 */
void
usal_fprbytes(FILE *f, char *s, register Uchar *cp, register int n)
{
	fprintf(f, "%s", s);
	while (--n >= 0)
		fprintf(f, " %02X", *cp++);
	fprintf(f, "\n");
}

/*
 * print some bytes in ascii to a file.
 */
void
usal_fprascii(FILE *f, char *s, register Uchar *cp, register int n)
{
	register int	c;

	fprintf(f, "%s", s);
	while (--n >= 0) {
		c = *cp++;
		if (c >= ' ' && c < 0177)
			fprintf(f, "%c", c);
		else
			fprintf(f, ".");
	}
	fprintf(f, "\n");
}

/*
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print some bytes in hex to stderr.
 */
void
usal_prbytes(char *s, register Uchar *cp, register int n)
{
	usal_fprbytes(stderr, s, cp, n);
}

/*
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print some bytes in ascii to stderr.
 */
void
usal_prascii(char *s, register Uchar *cp, register int n)
{
	usal_fprascii(stderr, s, cp, n);
}

/*
 * print some bytes in hex into a buffer.
 */
int
usal_sprbytes(char *buf, int maxcnt, char *s, register Uchar *cp, register int n)
{
	register char	*p = buf;
	register int	amt;

	amt = snprintf(p, maxcnt, "%s", s);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;

	while (--n >= 0) {
		amt = snprintf(p, maxcnt, " %02X", *cp++);
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	amt = snprintf(p, maxcnt, "\n");
	if (amt < 0)
		return (amt);
	p += amt;
	return (p - buf);
}

/*
 * print some bytes in ascii into a buffer.
 */
int
usal_sprascii(char *buf, int maxcnt, char *s, register Uchar *cp, register int n)
{
	register char	*p = buf;
	register int	amt;
	register int	c;

	amt = snprintf(p, maxcnt, "%s", s);
	if (amt < 0)
		return (amt);
	p += amt;
	maxcnt -= amt;

	while (--n >= 0) {
		c = *cp++;
		if (c >= ' ' && c < 0177)
			amt = snprintf(p, maxcnt, "%c", c);
		else
			amt = snprintf(p, maxcnt, ".");
		if (amt < 0)
			return (amt);
		p += amt;
		maxcnt -= amt;
	}
	amt = snprintf(p, maxcnt, "\n");
	if (amt < 0)
		return (amt);
	p += amt;
	return (p - buf);
}

/*
 * print the SCSI sense data for last command in hex to a file.
 */
void
usal_fprsense(FILE *f, Uchar *cp, int n)
{
	usal_fprbytes(f, "Sense Bytes:", cp, n);
}

/*
 * XXX We need to check if we should write to stderr or better to usal->errfile
 *
 * print the SCSI sense data for last command in hex to stderr.
 */
void
usal_prsense(Uchar *cp, int n)
{
	usal_fprsense(stderr, cp, n);
}

/*
 * print the SCSI sense data for last command in hex into a buffer.
 */
int
usal_sprsense(char *buf, int maxcnt, Uchar *cp, int n)
{
	return (usal_sprbytes(buf, maxcnt, "Sense Bytes:", cp, n));
}

/*
 * Return the SCSI status byte for last command.
 */
int
usal_cmd_status(SCSI *usalp)
{
	struct usal_cmd	*cp = usalp->scmd;
	int		cmdstatus = *(Uchar *)&cp->scb;

	return (cmdstatus);
}

/*
 * Return the SCSI sense key for last command.
 */
int
usal_sense_key(SCSI *usalp)
{
	register struct usal_cmd *cp = usalp->scmd;
	int	key = -1;

	if (!usal_cmd_err(usalp))
		return (0);

	if (cp->sense.code >= 0x70)
		key = ((struct scsi_ext_sense *)&(cp->sense))->key;
	return (key);
}

/*
 * Return all the SCSI sense table last command.
 */
unsigned char *
usal_sense_table(SCSI *usalp)
{
	register struct usal_cmd *cp = usalp->scmd;

	if(!usal_cmd_err(usalp))
		return (0);

	/* if (cp->sense.code >= 0x70) */
	return (char *) &(cp->sense);
}


/*
 * Return the SCSI sense code for last command.
 */
int
usal_sense_code(SCSI *usalp)
{
	register struct usal_cmd *cp = usalp->scmd;
	int	code = -1;

	if (!usal_cmd_err(usalp))
		return (0);

	if (cp->sense.code >= 0x70)
		code = ((struct scsi_ext_sense *)&(cp->sense))->sense_code;
	else
		code = cp->sense.code;
	return (code);
}

/*
 * Return the SCSI sense qualifier for last command.
 */
int
usal_sense_qual(SCSI *usalp)
{
	register struct usal_cmd *cp = usalp->scmd;

	if (!usal_cmd_err(usalp))
		return (0);

	if (cp->sense.code >= 0x70)
		return (((struct scsi_ext_sense *)&(cp->sense))->qual_code);
	else
		return (0);
}

/*
 * Print the device type from the SCSI inquiry buffer to file.
 */
void
usal_fprintdev(FILE *f, struct scsi_inquiry *ip)
{
	if (ip->removable)
		fprintf(f, "Removable ");
	if (ip->data_format >= 2) {
		switch (ip->qualifier) {

		case INQ_DEV_PRESENT:
			break;
		case INQ_DEV_NOTPR:
			fprintf(f, "not present ");
			break;
		case INQ_DEV_RES:
			fprintf(f, "reserved ");
			break;
		case INQ_DEV_NOTSUP:
			if (ip->type == INQ_NODEV) {
				fprintf(f, "unsupported\n"); return;
			}
			fprintf(f, "unsupported ");
			break;
		default:
			fprintf(f, "vendor specific %d ",
						(int)ip->qualifier);
		}
	}
	switch (ip->type) {

	case INQ_DASD:
		fprintf(f, "Disk");		break;
	case INQ_SEQD:
		fprintf(f, "Tape");		break;
	case INQ_PRTD:
		fprintf(f, "Printer");	break;
	case INQ_PROCD:
		fprintf(f, "Processor");	break;
	case INQ_WORM:
		fprintf(f, "WORM");		break;
	case INQ_ROMD:
		fprintf(f, "CD-ROM");	break;
	case INQ_SCAN:
		fprintf(f, "Scanner");	break;
	case INQ_OMEM:
		fprintf(f, "Optical Storage"); break;
	case INQ_JUKE:
		fprintf(f, "Juke Box");	break;
	case INQ_COMM:
		fprintf(f, "Communication");	break;
	case INQ_IT8_1:
		fprintf(f, "IT8 1");		break;
	case INQ_IT8_2:
		fprintf(f, "IT8 2");		break;
	case INQ_STARR:
		fprintf(f, "Storage array");	break;
	case INQ_ENCL:
		fprintf(f, "Enclosure services"); break;
	case INQ_SDAD:
		fprintf(f, "Simple direct access"); break;
	case INQ_OCRW:
		fprintf(f, "Optical card r/w"); break;
	case INQ_BRIDGE:
		fprintf(f, "Bridging expander"); break;
	case INQ_OSD:
		fprintf(f, "Object based storage"); break;
	case INQ_ADC:
		fprintf(f, "Automation/Drive Interface"); break;
	case INQ_WELLKNOWN:
		fprintf(f, "Well known lun"); break;

	case INQ_NODEV:
		if (ip->data_format >= 2) {
			fprintf(f, "unknown/no device");
			break;
		} else if (ip->qualifier == INQ_DEV_NOTSUP) {
			fprintf(f, "unit not present");
			break;
		}
	default:
		fprintf(f, "unknown device type 0x%x",
						(int)ip->type);
	}
	fprintf(f, "\n");
}

/*
 * Print the device type from the SCSI inquiry buffer to stdout.
 */
void
usal_printdev(struct scsi_inquiry *ip)
{
	usal_fprintdev(stdout, ip);
}

/*
 * print into the SCSI error buffer, adjust the next write pointer.
 */
/* VARARGS2 */
int
usal_printf(SCSI *usalp, const char *form, ...)
{
	int	cnt;
	va_list	args;

	va_start(args, form);
	cnt = vsnprintf(usalp->errptr, usal_errrsize(usalp), form, args);
	va_end(args);

	if (cnt < 0) {
		usalp->errptr[0] = '\0';
	} else {
		usalp->errptr += cnt;
	}
	return (cnt);
}

/*
 * Flush the SCSI error buffer to SCSI errfile.
 * Clear error buffer after flushing.
 */
int
usal_errflush(SCSI *usalp)
{
	if (usalp->errfile == NULL)
		return (0);

	return (usal_errfflush(usalp, (FILE *)usalp->errfile));
}

/*
 * Flush the SCSI error buffer to a file.
 * Clear error buffer after flushing.
 */
int
usal_errfflush(SCSI *usalp, FILE *f)
{
	int	cnt;

	cnt = usalp->errptr - usalp->errbeg;
	if (cnt > 0) {
		filewrite(f, usalp->errbeg, cnt);
		fflush(f);
		usalp->errbeg = usalp->errptr;
	}
	return (cnt);
}
