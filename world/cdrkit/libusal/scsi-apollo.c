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

/* @(#)scsi-apollo.c	1.5 04/01/14 Copyright 1997,2000 J. Schilling */
/*
 *	Code to support Apollo Domain/OS 10.4.1
 *
 *	Copyright (c) 1997,2000 J. Schilling
 *	Apollo support code written by Paul Walker.
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

#include <apollo/base.h>
#include <apollo/scsi.h>
#include <assert.h>
#define	DomainScsiTimeout	100000

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-apollo.c-1.5";	/* The version for this transport */


#define	MAX_SCG		1	/* Max # of SCSI controllers */
#define	MAX_TGT		1	/* Max # of SCSI targets */
#define	MAX_LUN		1	/* Max # of SCSI logical units */

struct usal_local {
	scsi_$handle_t	handle;
	unsigned char	*DomainSensePointer;
	short		usalfiles[MAX_SCG][MAX_TGT][MAX_LUN];
};

#define	usallocal(p)	((struct usal_local *)((p)->local))

#ifndef	SG_MAX_SENSE
#define	SG_MAX_SENSE	16	/* Too small for CCS / SCSI-2	 */
#endif				/* But cannot be changed	 */

/*
 * Return version information for the low level SCSI transport code.
 * This has been introduced to make it easier to trace down problems
 * in applications.
 */
static char *
usalo_version(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_version);
		/*
		 * If you changed this source, you are not allowed to
		 * return "schily" for the SCG_AUTHOR request.
		 */
		case SCG_AUTHOR:
			return ("Paul Walker");
		case SCG_SCCS_ID:
			return (__sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "scsi_$do_command_2", "SCSI transport from Apollo DomainOS drivers",
		"", "DomainOS driver name", "A DomainOS device node name", FALSE, TRUE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
	register int	nopen = 0;
	status_$t	status;

	if (usalp->debug > 1)
		printf("Entered scsi_open, usalp=%p, device='%s'\n", usalp, device);
	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);
	}
	if (device == NULL || *device == '\0') {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE, "Must give device name");
		return (0);
	}

	scsi_$acquire(device, strlen(device), &usallocal(usalp)->handle, &status);
	if (status.all) {
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE, "Cannot open '%s', status %08x", device, status.all);
		return (0);
	}
	/*
	 * Allocate the sense buffer
	 */
	usallocal(usalp)->DomainSensePointer = (Uchar *)valloc((size_t) (SCG_MAX_SENSE + getpagesize()));
	assert(status.all == 0);
	/*
	 * Wire the sense buffer
	 */
	scsi_$wire(usallocal(usalp)->handle, (caddr_t)(usallocal(usalp)->DomainSensePointer), SCG_MAX_SENSE, &status);
	assert(status.all == 0);

	if (usallocal(usalp)->usalfiles[0][0][0] == -1)
		usallocal(usalp)->usalfiles[0][0][0] = 1;
	usal_settarget(usalp, 0, 0, 0);
	return (++nopen);
}

static int
usalo_close(SCSI *usalp)
{
	status_$t	status;

	if (usalp->debug > 1)
		printf("Entering scsi_close\n");
	scsi_$release(usallocal(usalp)->handle, &status);
	/*
	 * should also unwire the sense buffer
	 */
	return (status.all);
}


static long
usalo_maxdma(SCSI *usalp, long amt)
{
	status_$t	status;
	scsi_$info_t	info;

	scsi_$get_info(usallocal(usalp)->handle, sizeof (info), &info, &status);
	if (status.all) {
		/*
		 * Should have some better error handling here
		 */
		printf("scsi_$get_info returned %08x\n", status.all);
		return (0);
	}
	return (info.max_xfer);
}


static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	void	*ret;

	if (usalp->debug > 1)
		printf("scsi_getbuf: %ld bytes\n", amt);
	ret = valloc((size_t)amt);
	if (ret == NULL)
		return (ret);
	usalp->bufbase = ret;
	return (ret);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->debug > 1)
		printf("Entering scsi_freebuf\n");

	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	register int	t;
	register int	l;

	if (usalp->debug > 1)
		printf("Entered scsi_havebus:  usalp=%p, busno=%d\n", usalp, busno);

	if (busno < 0 || busno >= MAX_SCG)
		return (FALSE);

	if (usalp->local == NULL)
		return (FALSE);

	for (t = 0; t < MAX_TGT; t++) {
		for (l = 0; l < MAX_LUN; l++)
			if (usallocal(usalp)->usalfiles[busno][t][l] >= 0)
				return (TRUE);
	}
	return (FALSE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (usalp->debug > 1)
		printf("Entered scsi_fileno:  usalp=%p, busno=%d, tgt=%d, tlun=%d\n", usalp, busno, tgt, tlun);
	if (busno < 0 || busno >= MAX_SCG ||
		tgt < 0 || tgt >= MAX_TGT ||
		tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	if (usalp->local == NULL)
		return (-1);
	if (usalp->debug > 1)
		printf("exiting scsi_fileno, returning %d\n", usallocal(usalp)->usalfiles[busno][tgt][tlun]);
	return ((int) usallocal(usalp)->usalfiles[busno][tgt][tlun]);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	if (usalp->debug > 1)
		printf("Entering scsi_initiator\n");

	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (FALSE);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	status_$t	status;

	if (usalp->debug > 0)
		printf("Entering scsi_reset\n");

	if (what == SCG_RESET_NOP)
		return (0);

	if (what == SCG_RESET_TGT) {
		scsi_$reset_device(usallocal(usalp)->handle, &status);
		if (status.all)
			printf("Error - scsi_$reset_device failed, status is 0x%08x\n", status.all);
		return (status.all);
	} else {
		errno = EINVAL;
		return (-1);
	}
}

static void
scsi_do_sense(SCSI *usalp, struct usal_cmd *sp)
{
	scsi_$op_status_t	op_status;
	static scsi_$cdb_t	sense_cdb;
	static linteger		sense_cdb_size;
	static linteger		sense_buffer_size;
	static scsi_$operation_id_t sense_op_id;
	static status_$t	sense_status;
	static pinteger		sense_return_count;
		int		i;

	/*
	 * Issue the sense command (wire, issue, wait, unwire
	 */
	sense_buffer_size = sp->sense_len;
	sense_cdb_size = SC_G0_CDBLEN;
	memset(sense_cdb.all, 0, sense_cdb_size);	/* Assuming Apollo sense */
							/* structure is correct */
							/* size */
	sense_cdb.g0.cmd = SC_REQUEST_SENSE;
	sense_cdb.g0.lun = sp->cdb.g0_cdb.lun;
	sense_cdb.g0.len = sp->sense_len;
	scsi_$do_command_2(usallocal(usalp)->handle, sense_cdb, sense_cdb_size, (caddr_t) (usallocal(usalp)->DomainSensePointer), sense_buffer_size, scsi_read, &sense_op_id, &sense_status);
	if (sense_status.all) {
		printf("Error executing sense command, status is 0x%08x\n", sense_status.all);
	}
	scsi_$wait(usallocal(usalp)->handle, DomainScsiTimeout, true, sense_op_id, 1, &op_status, &sense_return_count, &sense_status);
	/*
	 * Print the sense information if debug is on, or if the information is
	 * "unusual"
	 */
	if (usalp->debug > 0 ||
		/*
		 * I don't prinqqt info for sense codes 0, 2, 5, 6 because
		 * they aren't interesting
		 */
		(((u_char *) usallocal(usalp)->DomainSensePointer)[2] == 1) ||
		(((u_char *) usallocal(usalp)->DomainSensePointer)[2] == 3) ||
		(((u_char *) usallocal(usalp)->DomainSensePointer)[2] == 4) ||
		(((u_char *) usallocal(usalp)->DomainSensePointer)[2] >= 7)) {
		printf(" Sense dump:\n");
		for (i = 0; i < sp->sense_len; i++)
			printf(" %02x", ((u_char *) usallocal(usalp)->DomainSensePointer)[i]);
		printf("\n");
	}
	if (((u_char *) usallocal(usalp)->DomainSensePointer)[2] == 5) {
		/*
		 * Illegal command
		 */
		printf("Illegal command detected, ASC=0x%02x, ASQ=0x%02x\n", ((u_char *) usallocal(usalp)->DomainSensePointer)[12], ((u_char *) usallocal(usalp)->DomainSensePointer)[13]);
	}
	/*
	 * Copy the sense information to the driver
	 */
	memcpy(sp->u_sense.cmd_sense, usallocal(usalp)->DomainSensePointer, sp->sense_len);
	sp->sense_count = sp->sense_len;
}


static int
usalo_send(SCSI *usalp)
{
	linteger	buffer_length;
	linteger	cdb_len;
	scsi_$operation_id_t operation;
	scsi_$wait_index_t wait_index;
	scsi_$op_status_t op_status;
	pinteger	return_count;
	status_$t	status;
	char	*ascii_wait_status;
	int		i;
	struct usal_cmd *sp = usalp->scmd;

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	if (usalp->debug > 0) {
		printf("Entered usalo_send, usalp=%p, sp=%p\n", usalp, sp);
		printf("usalcmd dump:\n");
		printf("  addr=%p\n", sp->addr);
		printf("  size=0x%x\n", sp->size);
		printf("  flags=0x%x\n", sp->flags);
		printf("  cdb_len=%d\n", sp->cdb_len);
		printf("  sense_len=%d\n", sp->sense_len);
		printf("  timeout=%d\n", sp->timeout);
		printf("  kdebug=%d\n", sp->kdebug);
		printf("  CDB:");
		for (i = 0; i < sp->cdb_len; i++)
				printf(" %02x", sp->cdb.cmd_cdb[i]);
		printf("\n");
	}

	/*
	 * Assume complete transfer, so residual count = 0
	 */
	sp->resid = 0;
	buffer_length = sp->size;
	if (sp->addr) {
		if (usalp->debug > 0)
			printf(" wiring 0x%x bytes at 0x%x\n", buffer_length, sp->addr);
		scsi_$wire(usallocal(usalp)->handle, sp->addr, buffer_length, &status);
		if (status.all) {
			/*
			 * Need better error handling
			 */
			printf("scsi_$wire failed, 0x%08x\n", status.all);
		}
	}
	cdb_len = sp->cdb_len;
	scsi_$do_command_2(usallocal(usalp)->handle,		/* device handle*/
			*(scsi_$cdb_t *) &(sp->cdb.cmd_cdb[0]),	/* SCSI CDB	*/
			cdb_len,				/* CDB len	*/
			sp->addr,				/* DMA buf	*/
			buffer_length,				/* DMA len	*/
			(sp->flags & SCG_RECV_DATA) ? scsi_read : scsi_write,
			&operation,				/* OP ID	*/
			&status);				/* Status ret	*/

	if (status.all) {
		/*
		 * Need better error handling
		 */
		printf("scsi_$do_command failed, 0x%08x\n", status.all);
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		return (0);
	} else if (usalp->debug > 0) {
		printf("Command submitted, operation=0x%x\n", operation);
	}
	wait_index = scsi_$wait(usallocal(usalp)->handle,		/* device handle*/
				sp->timeout * 1000,		/* timeout	*/
				0,				/* async enable	*/
				operation,			/* OP ID	*/
				1,				/* max count	*/
				&op_status,			/* status list	*/
				&return_count,			/* count ret	*/
				&status);			/* Status ret	*/
	if (status.all) {
		/*
		 * Need better error handling
		 */
		printf("scsi_$wait failed, 0x%08x\n", status.all);
		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		return (0);
	} else {
		if (usalp->debug > 0) {
			printf("wait_index=%d, return_count=%d, op_status: op=0x%x, cmd_status=0x%x, op_status=0x%x\n",
				wait_index, return_count, op_status.op, op_status.cmd_status, op_status.op_status);
		}
		switch (wait_index) {

		case scsi_device_advance:
			ascii_wait_status = "scsi_device_advance";
			break;
		case scsi_timeout:
			ascii_wait_status = "scsi_timeout";
			break;
		case scsi_async_fault:
			ascii_wait_status = "scsi_async_fault";
			break;
		default:
			ascii_wait_status = "unknown";
			break;
		}
		/*
		 * See if the scsi_$wait terminated "abnormally"
		 */
		if (wait_index != scsi_device_advance) {
			printf("scsi_$wait terminated abnormally, status='%s'\n", ascii_wait_status);
			sp->error = SCG_FATAL;
			sp->ux_errno = EIO;
			return (0);
		}
		/*
		 * Normal termination, what's the scoop?
		 */
		assert(return_count == 1);
		switch (op_status.cmd_status.all) {

		case status_$ok:
			switch (op_status.op_status) {

			case scsi_good_status:
				sp->error = SCG_NO_ERROR;
				sp->ux_errno = 0;
				break;
			case scsi_busy:
				sp->error = SCG_NO_ERROR;
				sp->ux_errno = 0;
				break;
			case scsi_check_condition:
				if (usalp->debug > 0)
					printf("SCSI ERROR - CheckCondition\n");
				scsi_do_sense(usalp, sp);
				/*
				 * If this was a media error, then call it retryable,
				 * instead of no error
				 */
				if ((((u_char *) usallocal(usalp)->DomainSensePointer)[0] == 0xf0) &&
					(((u_char *) usallocal(usalp)->DomainSensePointer)[2] == 0x03)) {
					if (usalp->debug > 0)
						printf("  (retryable)\n");
					sp->error = SCG_RETRYABLE;
					sp->ux_errno = EIO;
				} else {
				/* printf("  (no error)\n"); */
					sp->error = SCG_NO_ERROR;
					sp->ux_errno = 0;
				}
				break;
			default:
				/*
				 * I fault out in this case because I want to know
				 * about this error, and this guarantees that it will
				 * get attention.
				 */
				printf("Unhandled Domain/OS op_status error:  status=%08x\n",
									op_status.op_status);
				exit(EXIT_FAILURE);
			}
			break;
		/*
		 * Handle recognized error conditions by copying the error
		 * code over
		 */
		case scsi_$operation_timeout:
			printf("SCSI ERROR - Timeout\n");
			scsi_do_sense(usalp, sp);
			sp->error = SCG_TIMEOUT;
			sp->ux_errno = EIO;
			break;
		case scsi_$dma_underrun:
			/*
			 * This condition seems to occur occasionaly.  I no longer
			 *  complain because it doesn't seem to matter.
			 */
			if (usalp->debug > 0)
				printf("SCSI ERROR - Underrun\n");
			scsi_do_sense(usalp, sp);
			sp->resid = sp->size;	/* We don't have the right number */
			sp->error = SCG_RETRYABLE;
			sp->ux_errno = EIO;
			break;
		case scsi_$hdwr_failure:	/* received when both scanners were active */
			printf("SCSI ERROR - Hardware Failure\n");
			scsi_do_sense(usalp, sp);
			sp->error = SCG_RETRYABLE;
			sp->ux_errno = EIO;
			break;
		default:
			printf("\nUnhandled Domain/OS cmd_status error:  status=%08x\n", op_status.cmd_status.all);
			error_$print(op_status.cmd_status);
			exit(EXIT_FAILURE);
		}
	}
	if (sp->addr) {
		if (usalp->debug > 0)
			printf(" unwiring buffer\n");
		scsi_$unwire(usallocal(usalp)->handle, sp->addr, buffer_length, sp->flags & SCG_RECV_DATA, &status);
		if (status.all) {
			/*
			 * Need better error handling
			 */
			printf("scsi_$unwire failed, 0x%08x\n", status.all);
			sp->error = SCG_FATAL;
			sp->ux_errno = EIO;
			return (0);
		}
	}
	return (0);
}
