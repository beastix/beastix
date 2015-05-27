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

/* @(#)scsi-dos.c	1.11 03/12/10 Copyright 1998-2003 J. Schilling, 2003 Alex Kopylov reanimatolog@yandex.ru */
/*
 *	Interface for the MS-DOS ASPI managers.
 *	You need ASPI manager installed in your config.sys:
 *		aspi*.sys for SCSI devices
 *		oakaspi.sys for ATAPI devices
 *		usbaspi.sys for USB devices
 *
 *	Made from Win32 ASPI library template.
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1998-2003 J. Schilling
 *	Copyright (c) 1999 A.L. Faber for the first implementation
 *			   of this interface.
 *	Copyright (c) 2003 Alex Kopylov reanimatolog@yandex.ru
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

#include <dos.h>
#include <dpmi.h>
#include <go32.h>
#include <usal/aspi-dos.h>

/*
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-dos.c-1.11";	/* The version for this transport*/

#define	MAX_SCG	8
#define	MAX_TGT	8
#define	MAX_LUN	8

struct usal_local {
	int	dummy;
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#define	MAX_DMA_DOS	(63L*1024L)

static	BYTE	busses		= 1;
static	DWORD	SCSIMgrEntry	= 0;
static	int	AspiLoaded	= 0;

static	BOOL	_callback_flag;
static	_go32_dpmi_seginfo _callback_info;
static	_go32_dpmi_registers _callback_regs;

static	BOOL	SCSIMgrOpen(SCSI *);
static	void	SCSIMgrClose(void);
static	int 	SCSIMgrSendSRB(SRB *, time_t);
static	void	SCSIMgrCallBack(_go32_dpmi_registers *regs);

static char *
usalo_version(SCSI *usalp, int what)
{
	if (usalp != (SCSI *)0) {
		switch (what) {

		case SCG_VERSION:
			return (_usal_trans_version);
		case SCG_AUTHOR:
			return (_usal_auth_cdrkit);
		case SCG_SCCS_ID:
			return (__sccsid);
		}
	}
	return ((char *)0);
}

static int
usalo_help(SCSI *usalp, FILE *f)
{
	__usal_help(f, "ASPI", "Generic transport independent SCSI",
		"", "bus,target,lun", "1,2,0", TRUE, FALSE);
	return (0);
}

static int
usalo_open(SCSI *usalp, char *device)
{
	int	busno	= usal_scsibus(usalp);
	int	tgt	= usal_target(usalp);
	int	tlun	= usal_lun(usalp);

	if (busno >= MAX_SCG || tgt >= MAX_TGT || tlun >= MAX_LUN) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Illegal value for busno, target or lun '%d,%d,%d'",
				busno, tgt, tlun);
		return (-1);
	}

	if ((device != NULL && *device != '\0') || (busno == -2 && tgt == -2)) {
		errno = EINVAL;
		if (usalp->errstr)
			snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
				"Open by 'devname' not supported on this OS");
		return (-1);
	}

	/*
	 *  Check if variables are within the range
	 */
	if (tgt >= 0 && tgt >= 0 && tlun >= 0) {
		/*
		 * This is the non -scanbus case.
		 */
		;
	} else if (tgt != -1 || tgt != -1 || tlun != -1) {
		errno = EINVAL;
		return (-1);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			return (0);
	}
	/*
	 * Try to open ASPI-Router
	 */
	if (!SCSIMgrOpen(usalp))
		return (-1);

	/*
	 * More than we have ...
	 */
	if (busno >= busses) {
		SCSIMgrClose();
		return (-1);
	}

	/*
	 * Install Exit Function which closes the ASPI-Router
	 */
	atexit(SCSIMgrClose);

	/*
	 * Success after all
	 */
	return (1);
}

static int
usalo_close(SCSI *usalp)
{
	SCSIMgrClose();
	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	return (MAX_DMA_DOS);
}

static void *
usalo_getbuf(SCSI *usalp, long amt)
{
	if (usalp->debug > 0) {
		fprintf((FILE *)usalp->errfile,
				"usalo_getbuf: %ld bytes\n", amt);
	}
	usalp->bufbase = malloc((size_t)(amt));
	return (usalp->bufbase);
}

static void
usalo_freebuf(SCSI *usalp)
{
	if (usalp->bufbase)
		free(usalp->bufbase);
	usalp->bufbase = NULL;
}

static __SBOOL
usalo_havebus(SCSI *usalp, int busno)
{
	if (busno < 0 || busno >= busses)
		return (FALSE);

	return (TRUE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	if (busno < 0 || busno >= busses ||
	    tgt < 0 || tgt >= MAX_TGT ||
	    tlun < 0 || tlun >= MAX_LUN)
		return (-1);

	/*
	 * Return fake
	 */
	return (1);
}

static int
usalo_initiator_id(SCSI *usalp)
{
	return (-1);
}

static int
usalo_isatapi(SCSI *usalp)
{
	return (-1);
}

static int
usalo_reset(SCSI *usalp, int what)
{
	errno = EINVAL;
	return (-1);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd	*sp = usalp->scmd;
	SRB		Srb;
	int		dos_memory_data_seg;
	int		dos_memory_data_sel;
	DWORD		dos_memory_data_ptr;

	/*
	 * Check if ASPI library is loaded
	 */
	if (!SCSIMgrEntry) {
		errmsgno(EX_BAD, "error in usalo_send: ASPI driver not loaded.\n");
		sp->error = SCG_FATAL;
		return (-1);
	}

	if (usalp->fd < 0) {
		sp->error = SCG_FATAL;
		return (0);
	}

	/*
	 * Initialize variables
	 */
	sp->error		= SCG_NO_ERROR;
	sp->sense_count		= 0;
	sp->u_scb.cmd_scb[0]	= 0;
	sp->resid		= 0;

	memset(&Srb, 0, sizeof (Srb));

	switch (sp->cdb_len) {

	case 6:
		movebytes(&sp->cdb, &(Srb.Type.ExecSCSICmd.CmdLen._6.CDBByte), sp->cdb_len);
		break;
	case 10:
		movebytes(&sp->cdb, &(Srb.Type.ExecSCSICmd.CmdLen._10.CDBByte), sp->cdb_len);
		break;
	case 12:
		movebytes(&sp->cdb, &(Srb.Type.ExecSCSICmd.CmdLen._12.CDBByte), sp->cdb_len);
		break;
	default:
		sp->error = SCG_FATAL;
		sp->ux_errno = EINVAL;
		fprintf((FILE *)usalp->errfile,
			"Unsupported sp->cdb_len: %u. Fatal error in usalo_send, exiting...\n", sp->cdb_len);
		return (-1);
	}

	if ((dos_memory_data_seg = __dpmi_allocate_dos_memory((sp->size>>4)+1, &dos_memory_data_sel)) == -1) {
		sp->error = SCG_FATAL;
		return (-1);
	}
	dos_memory_data_ptr = dos_memory_data_seg<<16;

	_dosmemputb(sp->addr, sp->size, dos_memory_data_seg<<4);

	Srb.Cmd = SC_EXEC_SCSI_CMD;
	Srb.HaId = usal_scsibus(usalp);
	Srb.Type.ExecSCSICmd.Target = usal_target(usalp);
	Srb.Type.ExecSCSICmd.Lun = usal_lun(usalp);
	Srb.Type.ExecSCSICmd.BufLen = sp->size;
	Srb.Type.ExecSCSICmd.BufPointer = (void *)dos_memory_data_ptr;
	Srb.Type.ExecSCSICmd.CDBLen = sp->cdb_len;
	if (sp->sense_len <= (SENSE_LEN+2))
		Srb.Type.ExecSCSICmd.SenseLen = sp->sense_len;
	else
		Srb.Type.ExecSCSICmd.SenseLen = (SENSE_LEN+2);

	/*
	 * Enable SCSIMgrCallBack()
	 */
	Srb.Flags |= SRB_POSTING;
	Srb.Type.ExecSCSICmd.PostProc = (void *)(_callback_info.rm_offset|(_callback_info.rm_segment<<16));

	/*
	 * Do we receive data from this ASPI command?
	 */
	if (sp->flags & SCG_RECV_DATA) {

		Srb.Flags |= SRB_DIR_IN;
	} else {
		/*
		 * Set direction to output
		 */
		if (sp->size > 0) {
			Srb.Flags |= SRB_DIR_OUT;
		}
	}

	SCSIMgrSendSRB(&Srb, usalp->scmd->timeout);
	Srb.Type.ExecSCSICmd.BufPointer = sp->addr;
	_dosmemgetb(dos_memory_data_seg<<4, sp->size, sp->addr);
	__dpmi_free_dos_memory(dos_memory_data_sel);

	if (Srb.Status == SS_PENDING) {	/* Check if we got a timeout*/
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
					"Timeout....\n");
		}
			sp->error = SCG_TIMEOUT;
		return (1);		/* Return error		    */
	}

	/*
	 * Check ASPI command status
	 */
	if (Srb.Status != SS_COMP) {

		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"Error in usalo_send: Srb.Status is 0x%x\n", Srb.Status);
		}

		switch (Srb.Status) {

			case SS_COMP:			/* 0x01 SRB completed without error */
				sp->error = SCG_NO_ERROR;
				sp->ux_errno = 0;
				break;

			case SS_PENDING:		/* 0x00 SRB being processed	    */
			case SS_ABORTED:		/* 0x02 SRB aborted		    */
			case SS_ABORT_FAIL:		/* 0x03 Unable to abort SRB	    */
			default:
				sp->error = SCG_RETRYABLE;
				sp->ux_errno = EIO;
				break;

			case SS_ERR:			/* 0x04 SRB completed with error    */
				/*
				 * If the SCSI Status byte is != 0, we definitely could send
				 * the command to the target. We signal NO transport error.
				 */
				sp->error = SCG_NO_ERROR;
				sp->ux_errno = EIO;
				if (Srb.Type.ExecSCSICmd.TargStat)
					break;

			case SS_INVALID_CMD:		/* 0x80 Invalid ASPI command	    */
			case SS_INVALID_HA:		/* 0x81 Invalid host adapter number */
			case SS_NO_DEVICE:		/* 0x82 SCSI device not installed   */
				sp->error = SCG_FATAL;
				sp->ux_errno = EINVAL;
				break;
		}

		sp->sense_count	= Srb.Type.ExecSCSICmd.SenseLen;
		if (sp->sense_count > sp->sense_len)
			sp->sense_count = sp->sense_len;

		memset(&sp->u_sense.Sense, 0x00, sizeof (sp->u_sense.Sense));

		switch (sp->cdb_len) {

		case 6:
			memcpy(&sp->u_sense.Sense, Srb.Type.ExecSCSICmd.CmdLen._6.SenseArea, sp->sense_count);
			break;
		case 10:
			memcpy(&sp->u_sense.Sense, Srb.Type.ExecSCSICmd.CmdLen._10.SenseArea, sp->sense_count);
			break;
		case 12:
			memcpy(&sp->u_sense.Sense, Srb.Type.ExecSCSICmd.CmdLen._12.SenseArea, sp->sense_count);
			break;
		}
		sp->u_scb.cmd_scb[0] = Srb.Type.ExecSCSICmd.TargStat;

		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"Mapped to: error %d errno: %d\n", sp->error, sp->ux_errno);
		}
		return (1);
	}

	return (0);
}

static BOOL
SCSIMgrOpen(SCSI *usalp)
{
	int		hSCSIMgr;
	int		dos_memory_seg;
	int		dos_memory_sel;
	__dpmi_regs	_regs;
	SRB		Srb;

	if (SCSIMgrEntry) {
		AspiLoaded++;
		return (TRUE);
	}

	/*
	 * Open "SCSIMRG$"
	 */
	if (!_dos_open("SCSIMGR$", 0, &hSCSIMgr)) {

		/* Alloc 16 bytes DOS memory */
		if ((dos_memory_seg = __dpmi_allocate_dos_memory(1, &dos_memory_sel)) != -1) {

			/* Look for SCSIMgr entry point */
			memset(&_regs, 0, sizeof (_regs));
			_regs.x.ax = 0x4402;
			_regs.x.bx = hSCSIMgr;
			_regs.x.cx = 0x0004;
			_regs.x.ds = dos_memory_seg;
			_regs.x.dx = 0x0000;
			if (!__dpmi_simulate_real_mode_interrupt(0x21,  &_regs)) {
				_dosmemgetb(dos_memory_seg<<4, 4, &SCSIMgrEntry);
			}

			/* Free DOS memory */
			__dpmi_free_dos_memory(dos_memory_sel);
		}

		/* Close "SCSIMRG$" */
		_dos_close(hSCSIMgr);

		/* Allocate real mode callback */
		_callback_info.pm_offset = (unsigned long)&SCSIMgrCallBack;
		if (_go32_dpmi_allocate_real_mode_callback_retf(&_callback_info, &_callback_regs) == -1) {
			fprintf((FILE *)usalp->errfile, "Cannot allocate callback address!\n");
			SCSIMgrEntry = 0;
		}
	}

	/* SCSIMgr entry point founded? */
	if (!SCSIMgrEntry) {
		fprintf((FILE *)usalp->errfile, "Cannot open ASPI manager! Try to get one from http://bootcd.narod.ru/\n");
		return (FALSE);
	}

	memset(&Srb, 0, sizeof (Srb));
	Srb.Cmd = SC_HA_INQUIRY;

	SCSIMgrSendSRB(&Srb, 10);

	if (usalp->debug) {
		fprintf((FILE *)usalp->errfile, "Status : %ld\n", Srb.Status);
		fprintf((FILE *)usalp->errfile, "hacount: %d\n", Srb.Type.HAInquiry.Count);
		fprintf((FILE *)usalp->errfile, "SCSI id: %d\n", Srb.Type.HAInquiry.SCSI_ID);
		fprintf((FILE *)usalp->errfile, "Manager: '%.16s'\n", Srb.Type.HAInquiry.ManagerId);
		fprintf((FILE *)usalp->errfile, "Identif: '%.16s'\n", Srb.Type.HAInquiry.Identifier);
		usal_prbytes("Unique:", Srb.Type.HAInquiry.Unique, 16);
	}

	AspiLoaded++;
	return (TRUE);
}

static void
SCSIMgrClose()
{
	if (--AspiLoaded > 0)
		return;
	if (SCSIMgrEntry) {
		_go32_dpmi_free_real_mode_callback(&_callback_info);
		SCSIMgrEntry = 0;
	}
}

static int
SCSIMgrSendSRB(SRB *Srb, time_t timeout)
{
	int		dos_memory_srb_seg;
	int		dos_memory_srb_sel;
	DWORD		dos_memory_srb_ptr;
	struct timeval	st;
	struct timeval	cr;
	__dpmi_regs	_regs;

	/* Alloc DOS memory */
	if ((dos_memory_srb_seg = __dpmi_allocate_dos_memory((sizeof (SRB) >> 4) + 1, &dos_memory_srb_sel)) == -1) {
		Srb->Status = SS_NO_DEVICE; /* ??? fatal error */
		return (Srb->Status);
	}
	dos_memory_srb_ptr = dos_memory_srb_seg<<16;

	/* Copy SRB to DOS memory */
	_dosmemputb((void *)Srb, sizeof (SRB), dos_memory_srb_seg<<4);

	/* Reset callback flag */
	_callback_flag = 0;

	/* Call SCSIMgr */
	memset(&_regs, 0, sizeof (_regs));
	_regs.x.ip = SCSIMgrEntry & 0xffff;
	_regs.x.cs = SCSIMgrEntry >> 16;
	__dpmi_simulate_real_mode_procedure_retf_stack(&_regs, 2, &dos_memory_srb_ptr);

	/* Wait 'timeout' seconds for Srb->Status != SS_PENDING */
	gettimeofday(&st, NULL);
	do {
		_dosmemgetb(dos_memory_srb_seg << 4, sizeof (SRB), (void *)Srb);
		gettimeofday(&cr, NULL);
	} while ((Srb->Status == SS_PENDING) && (cr.tv_sec - st.tv_sec < timeout));

	/* Free DOS memory */
	__dpmi_free_dos_memory(dos_memory_srb_sel);

	return (Srb->Status);
}

static void
SCSIMgrCallBack(_go32_dpmi_registers *regs)
{
	_callback_flag = 1;
}
