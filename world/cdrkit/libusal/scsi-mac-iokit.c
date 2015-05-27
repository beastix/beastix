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

/* @(#)scsi-mac-iokit.c	1.10 05/05/15 Copyright 1997,2001-2004 J. Schilling */
/*
 *	Interface to the Darwin IOKit SCSI drivers
 *
 *	Notes: Uses the IOKit/scsi-commands/SCSITaskLib interface
 *
 *	As of October 2001, this interface does not support SCSI parallel bus
 *	(old-fashioned SCSI). It does support ATAPI, Firewire, and USB.
 *
 *	First version done by Constantine Sapuntzakis <csapuntz@Stanford.EDU>
 *
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 *
 *	Copyright (c) 1997,2001-2004 J. Schilling
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
 *	Warning: you may change this source, but if you do that
 *	you need to change the _usal_version and _usal_auth* string below.
 *	You may not return "schily" for an SCG_AUTHOR request anymore.
 *	Choose your name instead of "schily" and make clear that the version
 *	string is related to a modified source.
 */
static	char	_usal_trans_version[] = "scsi-mac-iokit.c-1.10";	/* The version for this transport */

#define	MAX_SCG		16	/* Max # of SCSI controllers */
#define	MAX_TGT		16
#define	MAX_LUN		8

#include <statdefs.h>
#include <mach/mach.h>
#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/scsi-commands/SCSITaskLib.h>
#include <mach/mach_error.h>

struct usal_local {
	MMCDeviceInterface	**mmcDeviceInterface;
	SCSITaskDeviceInterface	**scsiTaskDeviceInterface;
	mach_port_t		masterPort;
};
#define	usallocal(p)	((struct usal_local *)((p)->local))

#define	MAX_DMA_NEXT	(32*1024)
#if 0
#define	MAX_DMA_NEXT	(64*1024)	/* Check if this is not too big */
#endif

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
	__usal_help(f, "SCSITaskDeviceInterface", "Apple SCSI",
		"", "Mac Prom device name", "IOCompactDiscServices/0",
								FALSE, FALSE);
	return (0);
}


/*
 * Valid Device names:
 *    IOCompactDiscServices
 *    IODVDServices
 *    IOSCSIPeripheralDeviceNub
 *
 * Also a / and a number can be appended to refer to something
 * more than the first device (e.g. IOCompactDiscServices/5 for the 5th
 * compact disc attached)
 */
static int
usalo_open(SCSI *usalp, char *device)
{
	mach_port_t masterPort = NULL;
	io_iterator_t scsiObjectIterator = NULL;
	IOReturn ioReturnValue = kIOReturnSuccess;
	CFMutableDictionaryRef dict = NULL;
	io_object_t scsiDevice = NULL;
	HRESULT plugInResult;
	IOCFPlugInInterface **plugInInterface = NULL;
	MMCDeviceInterface **mmcDeviceInterface = NULL;
	SCSITaskDeviceInterface **scsiTaskDeviceInterface = NULL;
	SInt32 score = 0;
	int err = -1;
	char *realdevice = NULL, *tmp;
	int driveidx = 1, idx = 1;

	if (device == NULL) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
		"Please specify a device name (e.g. IOCompactDiscServices/0)");
		goto out;
	}

	realdevice = tmp = strdup(device);
	tmp = strchr(tmp, '/');
	if (tmp != NULL) {
		*tmp++ = '\0';
		driveidx = atoi(tmp);
	}

	if (usalp->local == NULL) {
		usalp->local = malloc(sizeof (struct usal_local));
		if (usalp->local == NULL)
			goto out;
	}

	ioReturnValue = IOMasterPort(bootstrap_port, &masterPort);

	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Couldn't get a master IOKit port. Error %d",
			    ioReturnValue);
		goto out;
	}

	dict = IOServiceMatching(realdevice);
	if (dict == NULL) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Couldn't create dictionary for searching");
		goto out;
	}

	ioReturnValue = IOServiceGetMatchingServices(masterPort, dict,
						    &scsiObjectIterator);
	dict = NULL;

	if (scsiObjectIterator == NULL ||
	    (ioReturnValue != kIOReturnSuccess)) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "No matching device %s found.", device);
		goto out;
	}

	if (driveidx <= 0)
		driveidx = 1;

	idx = 1;
	while ((scsiDevice = IOIteratorNext(scsiObjectIterator)) != NULL) {
		if (idx == driveidx)
			break;
		IOObjectRelease(scsiDevice);
		scsiDevice = NULL;
		idx++;
	}

	if (scsiDevice == NULL) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "No matching device found. Iterator failed.");
		goto out;
	}

	ioReturnValue = IOCreatePlugInInterfaceForService(scsiDevice,
			kIOMMCDeviceUserClientTypeID,
			kIOCFPlugInInterfaceID,
			&plugInInterface, &score);
	if (ioReturnValue != kIOReturnSuccess) {
		goto try_generic;
	}

	plugInResult = (*plugInInterface)->QueryInterface(plugInInterface,
				CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
				(LPVOID)&mmcDeviceInterface);

	if (plugInResult != KERN_SUCCESS) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Unable to get MMC Interface: 0x%lX",
			    (long)plugInResult);

		goto out;
	}

	scsiTaskDeviceInterface =
		(*mmcDeviceInterface)->GetSCSITaskDeviceInterface(mmcDeviceInterface);

	if (scsiTaskDeviceInterface == NULL) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Failed to get taskDeviceInterface");
		goto out;
	}

	goto init;

try_generic:
	ioReturnValue = IOCreatePlugInInterfaceForService(scsiDevice,
					kIOSCSITaskDeviceUserClientTypeID,
					kIOCFPlugInInterfaceID,
					&plugInInterface, &score);
	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Unable to get plugin Interface: %x",
			    ioReturnValue);
		goto out;
	}

	plugInResult = (*plugInInterface)->QueryInterface(plugInInterface,
			    CFUUIDGetUUIDBytes(kIOSCSITaskDeviceInterfaceID),
					(LPVOID)&scsiTaskDeviceInterface);

	if (plugInResult != KERN_SUCCESS) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Unable to get generic Interface: 0x%lX",
			    (long)plugInResult);

		goto out;
	}

init:
	ioReturnValue =
		(*scsiTaskDeviceInterface)->ObtainExclusiveAccess(scsiTaskDeviceInterface);

	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Unable to get exclusive access to device");
		goto out;
	}

	if (mmcDeviceInterface) {
		(*mmcDeviceInterface)->AddRef(mmcDeviceInterface);
	}
	(*scsiTaskDeviceInterface)->AddRef(scsiTaskDeviceInterface);
	usallocal(usalp)->mmcDeviceInterface = mmcDeviceInterface;
	usallocal(usalp)->scsiTaskDeviceInterface = scsiTaskDeviceInterface;
	usallocal(usalp)->masterPort = masterPort;
	usal_settarget(usalp, 0, 0, 0);
	err = 1;

out:
	if (scsiTaskDeviceInterface != NULL) {
		(*scsiTaskDeviceInterface)->Release(scsiTaskDeviceInterface);
	}

	if (plugInInterface != NULL) {
		(*plugInInterface)->Release(plugInInterface);
	}

	if (scsiDevice != NULL) {
		IOObjectRelease(scsiDevice);
	}

	if (scsiObjectIterator != NULL) {
		IOObjectRelease(scsiObjectIterator);
	}

	if (err < 0) {
		if (usalp->local) {
			free(usalp->local);
			usalp->local = NULL;
		}

		if (masterPort) {
			mach_port_deallocate(mach_task_self(), masterPort);
		}
	}

	if (dict != NULL) {
		CFRelease(dict);
	}

	if (realdevice != NULL) {
		free(realdevice);
	}
	return (err);
}

static int
usalo_close(SCSI *usalp)
{
	SCSITaskDeviceInterface	**sc;
	MMCDeviceInterface	**mmc;

	if (usalp->local == NULL)
		return (-1);

	sc = usallocal(usalp)->scsiTaskDeviceInterface;
	(*sc)->ReleaseExclusiveAccess(sc);
	(*sc)->Release(sc);
	usallocal(usalp)->scsiTaskDeviceInterface = NULL;

	mmc = usallocal(usalp)->mmcDeviceInterface;
	if (mmc != NULL)
		(*mmc)->Release(mmc);

	mach_port_deallocate(mach_task_self(), usallocal(usalp)->masterPort);

	free(usalp->local);
	usalp->local = NULL;

	return (0);
}

static long
usalo_maxdma(SCSI *usalp, long amt)
{
	long maxdma = MAX_DMA_NEXT;
#ifdef	SGIOCMAXDMA
	int  m;

	if (ioctl(usallocal(usalp)->usalfile, SGIOCMAXDMA, &m) >= 0) {
		maxdma = m;
		if (usalp->debug > 0) {
			fprintf((FILE *)usalp->errfile,
				"maxdma: %d\n", maxdma);
		}
	}
#endif
	return (maxdma);
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

static BOOL
usalo_havebus(SCSI *usalp, int busno)
{
	if (busno == 0)
		return (TRUE);
	return (FALSE);
}

static int
usalo_fileno(SCSI *usalp, int busno, int tgt, int tlun)
{
	return (-1);
}

static int
usalo_initiator_id(SCSI *usalp)
{
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
	if (what == SCG_RESET_NOP)
		return (0);
	if (what != SCG_RESET_BUS) {
		errno = EINVAL;
		return (-1);
	}

	errno = 0;
	return (-1);
}

static int
usalo_send(SCSI *usalp)
{
	struct usal_cmd		*sp = usalp->scmd;
	SCSITaskDeviceInterface	**sc = NULL;
	SCSITaskInterface	**cmd = NULL;
	IOVirtualRange		iov;
	SCSI_Sense_Data		senseData;
	SCSITaskStatus		status;
	UInt64			bytesTransferred;
	IOReturn		ioReturnValue;
	int			ret = 0;

	if (usalp->local == NULL) {
		return (-1);
	}

	sc = usallocal(usalp)->scsiTaskDeviceInterface;

	cmd = (*sc)->CreateSCSITask(sc);
	if (cmd == NULL) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Failed to create SCSI task");
		ret = -1;

		sp->error = SCG_FATAL;
		sp->ux_errno = EIO;
		goto out;
	}


	iov.address = (IOVirtualAddress) sp->addr;
	iov.length = sp->size;

	ioReturnValue = (*cmd)->SetCommandDescriptorBlock(cmd,
						sp->cdb.cmd_cdb, sp->cdb_len);

	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "SetCommandDescriptorBlock failed with status %x",
			    ioReturnValue);
		ret = -1;
		goto out;
	}

	ioReturnValue = (*cmd)->SetScatterGatherEntries(cmd, &iov, 1, sp->size,
				(sp->flags & SCG_RECV_DATA) ?
				kSCSIDataTransfer_FromTargetToInitiator :
				kSCSIDataTransfer_FromInitiatorToTarget);
	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "SetScatterGatherEntries failed with status %x",
			    ioReturnValue);
		ret = -1;
		goto out;
	}

	ioReturnValue = (*cmd)->SetTimeoutDuration(cmd, sp->timeout * 1000);
	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "SetTimeoutDuration failed with status %x",
			    ioReturnValue);
		ret = -1;
		goto out;
	}

	memset(&senseData, 0, sizeof (senseData));

	seterrno(0);
	ioReturnValue = (*cmd)->ExecuteTaskSync(cmd,
				&senseData, &status, &bytesTransferred);

	sp->resid = sp->size - bytesTransferred;
	sp->error = SCG_NO_ERROR;
	sp->ux_errno = geterrno();

	if (ioReturnValue != kIOReturnSuccess) {
		snprintf(usalp->errstr, SCSI_ERRSTR_SIZE,
			    "Command execution failed with status %x",
			    ioReturnValue);
		sp->error = SCG_RETRYABLE;
		ret = -1;
		goto out;
	}

	memset(&sp->scb, 0, sizeof (sp->scb));
	memset(&sp->u_sense.cmd_sense, 0, sizeof (sp->u_sense.cmd_sense));
	if (senseData.VALID_RESPONSE_CODE != 0 || status == 0x02) {
		/*
		 * There is no sense length - we need to asume that
		 * we always get 18 bytes.
		 */
		sp->sense_count = kSenseDefaultSize;
		memmove(&sp->u_sense.cmd_sense, &senseData, kSenseDefaultSize);
		if (sp->ux_errno == 0)
			sp->ux_errno = EIO;
	}

	sp->u_scb.cmd_scb[0] = status;

	/* ??? */
	if (status == kSCSITaskStatus_No_Status) {
		sp->error = SCG_RETRYABLE;
		ret = -1;
		goto out;
	}
	/*
	 * XXX Is it possible to have other senseful SCSI transport error codes?
	 */

out:
	if (cmd != NULL) {
		(*cmd)->Release(cmd);
	}

	return (ret);
}
