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

/* @(#)interface.c	1.40 06/02/19 Copyright 1998-2002 Heiko Eissfeldt, Copyright 2006 J. Schilling */
/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) 1994-1997 Heiko Eissfeldt heiko@colossus.escape.de
 *
 * Interface module for cdrom drive access
 *
 * Two interfaces are possible.
 *
 * 1. using 'cooked' ioctls() (Linux only)
 *    : available for atapi, sbpcd and cdu31a drives only.
 *
 * 2. using the generic scsi device (for details see SCSI Prog. HOWTO).
 *    NOTE: a bug/misfeature in the kernel requires blocking signal
 *          SIGINT during SCSI command handling. Once this flaw has
 *          been removed, the sigprocmask SIG_BLOCK and SIG_UNBLOCK calls
 *          should removed, thus saving context switches.
 *
 * For testing purposes I have added a third simulation interface.
 *
 * Version 0.8: used experiences of Jochen Karrer.
 *              SparcLinux port fixes
 *              AlphaLinux port fixes
 *
 */
#if 0
#define SIM_CD
#endif

#include "config.h"
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <unixstd.h>
#include <strdefs.h>
#include <errno.h>
#include <signal.h>
#include <fctldefs.h>
#include <assert.h>
#include <schily.h>
#include <device.h>

#include <sys/ioctl.h>
#include <statdefs.h>


#include "mycdrom.h"
#include "lowlevel.h"
/* some include file locations have changed with newer kernels */
#if defined (__linux__)
# if LINUX_VERSION_CODE > 0x10300 + 97
#  if LINUX_VERSION_CODE < 0x200ff
#   include <linux/sbpcd.h>
#   include <linux/ucdrom.h>
#  endif
#  if !defined(CDROM_SELECT_SPEED)
#   include <linux/ucdrom.h>
#  endif
# endif
#endif

#include <usal/scsitransp.h>

#include "mytype.h"
#include "byteorder.h"
#include "interface.h"
#include "icedax.h"
#include "semshm.h"
#include "setuid.h"
#include "ringbuff.h"
#include "toc.h"
#include "global.h"
#include "ioctl.h"
#include "exitcodes.h"
#include "scsi_cmds.h"

#include <utypes.h>
#include <wodim.h>
#include "scsi_scan.h"

unsigned interface;

int trackindex_disp = 0;

void	priv_init(void);
void	priv_on(void);
void	priv_off(void);

void		(*EnableCdda)(SCSI *, int Switch, unsigned uSectorsize);
unsigned (*doReadToc)(SCSI *usalp);
void	 	(*ReadTocText)(SCSI *usalp);
unsigned (*ReadLastAudio)(SCSI *usalp);
int      (*ReadCdRom)(SCSI *usalp, UINT4 *p, unsigned lSector, 
							 unsigned SectorBurstVal);
int      (*ReadCdRomData)(SCSI *usalp, unsigned char *p, unsigned lSector, 
								  unsigned SectorBurstVal);
int      (*ReadCdRomSub)(SCSI *usalp, UINT4 *p, unsigned lSector, 
								 unsigned SectorBurstVal);
subq_chnl *(*ReadSubChannels)(SCSI *usalp, unsigned lSector);
subq_chnl *(*ReadSubQ)(SCSI *usalp, unsigned char sq_format, 
							  unsigned char track);
void     (*SelectSpeed)(SCSI *usalp, unsigned speed);
int		(*Play_at)(SCSI *usalp, unsigned int from_sector, unsigned int sectors);
int		(*StopPlay)(SCSI *usalp);
void		(*trash_cache)(UINT4 *p, unsigned lSector, unsigned SectorBurstVal);

#if	defined	USE_PARANOIA
long cdda_read(void *d, void *buffer, long beginsector, long sectors);

long cdda_read(void *d, void *buffer, long beginsector, long sectors)
{
	long ret = ReadCdRom(d, buffer, beginsector, sectors);
	return ret;
}
#endif

typedef struct string_len {
  char *str;
  unsigned int sl;
} mystring;

static mystring drv_is_not_mmc[] = {
	{"DEC     RRD47   (C) DEC ",24},
/*	{"SONY    CD-ROM CDU625    1.0",28}, */
	{NULL,0}	/* must be last entry */
};

static mystring drv_has_mmc_cdda[] = {
	{"HITACHI CDR-7930",16},
/*	{"TOSHIBA CD-ROM XM-5402TA3605",28}, */
	{NULL,0}	/* must be last entry */
};

static int	Is_a_Toshiba3401;

int Toshiba3401(void);

int Toshiba3401() 
{
  return Is_a_Toshiba3401;
}

/* hook */
static void Dummy(void);
static void Dummy()
{
}

static SCSI    *usalp;

SCSI *get_scsi_p(void);

SCSI *get_scsi_p()
{
    return usalp;
}

#if !defined(SIM_CD)

static void trash_cache_SCSI(UINT4 *p, unsigned lSector, 
									  unsigned SectorBurstVal);

static void trash_cache_SCSI(UINT4 *p, unsigned lSector, 
									  unsigned SectorBurstVal)
{
      /* trash the cache */
      ReadCdRom(get_scsi_p(), p, find_an_off_sector(lSector, SectorBurstVal), min(global.nsectors,6));
}



static void Check_interface_for_device(struct stat *statstruct, 
													char *pdev_name);
static int OpenCdRom(char *pdev_name);

static void SetupSCSI(void);

static void SetupSCSI()
{
    unsigned char *p;

    if (interface != GENERIC_SCSI) {
	/* unfortunately we have the wrong interface and are
	 * not able to change on the fly */
	fprintf(stderr, "The generic SCSI interface and devices are required\n");
	exit(SYNTAX_ERROR);
    }

    /* do a test unit ready to 'init' the device. */
    TestForMedium(usalp);

    /* check for the correct type of unit. */
    p = Inquiry(usalp);

#undef TYPE_ROM
#define TYPE_ROM 5
#undef TYPE_WORM
#define TYPE_WORM  4
    if (p == NULL) {
	fprintf(stderr, "Inquiry command failed. Aborting...\n");
	exit(DEVICE_ERROR);
    }

    if ((*p != TYPE_ROM && *p != TYPE_WORM)) {
	fprintf(stderr, "this is neither a scsi cdrom nor a worm device\n");
	exit(SYNTAX_ERROR);
    }

    if (global.quiet == 0) {
	fprintf(stderr,
		 "Type: %s, Vendor '%8.8s' Model '%16.16s' Revision '%4.4s' ",
		 *p == TYPE_ROM ? "ROM" : "WORM"
		 ,p+8
		 ,p+16
		 ,p+32);
    }
    /* generic Sony type defaults */
    density = 0x0;
    accepts_fua_bit = -1;
    EnableCdda = (void (*)(SCSI *, int, unsigned))Dummy;
    ReadCdRom = ReadCdda12;
    ReadCdRomSub = ReadCddaSubSony;
    ReadCdRomData = (int (*)(SCSI *, unsigned char *, unsigned, unsigned))ReadStandardData;
    ReadLastAudio = ReadFirstSessionTOCSony;
    SelectSpeed = SpeedSelectSCSISony;
    Play_at = Play_atSCSI;
    StopPlay = StopPlaySCSI;
    trash_cache = trash_cache_SCSI;
    ReadTocText = ReadTocTextSCSIMMC;
    doReadToc = ReadTocSCSI;
    ReadSubQ = ReadSubQSCSI;
    ReadSubChannels = NULL;

    /* check for brands and adjust special peculiaritites */

    /* If your drive is not treated correctly, you can adjust some things
       here:

       global.in_lendian: should be to 1, if the CDROM drive or CD-Writer
		  delivers the samples in the native byteorder of the audio cd
		  (LSB first).
		  HP CD-Writers need it set to 0.
       NOTE: If you get correct wav files when using sox with the '-x' option,
             the endianess is wrong. You can use the -C option to specify
	     the value of global.in_lendian.

     */

    {
      int mmc_code;

      usalp->silent ++;
      allow_atapi(usalp, 1);
      if (*p == TYPE_ROM) {
        mmc_code = heiko_mmc(usalp);
      } else {
        mmc_code = 0;
      }
      usalp->silent --;

      /* Exceptions for drives that report incorrect MMC capability */
      if (mmc_code != 0) {
	/* these drives are NOT capable of MMC commands */
        mystring *pp = drv_is_not_mmc;
	while (pp->str != NULL) {
	  if (!strncmp(pp->str, (char *)p+8,pp->sl)) {
	    mmc_code = 0;
	    break;
	  }
	  pp++;
        }
      }
      {
	/* these drives flag themselves as non-MMC, but offer CDDA reading
	   only with a MMC method. */
        mystring *pp = drv_has_mmc_cdda;
	while (pp->str != NULL) {
	  if (!strncmp(pp->str, (char *)p+8,pp->sl)) {
	    mmc_code = 1;
	    break;
	  }
	  pp++;
        }
      }

      switch (mmc_code) {
       case 2:      /* SCSI-3 cdrom drive with accurate audio stream */
	/* fall through */
       case 1:      /* SCSI-3 cdrom drive with no accurate audio stream */
	/* fall through */
lost_toshibas:
	 global.in_lendian = 1;
         if (mmc_code == 2)
	   global.overlap = 0;
	 else
           global.overlap = 1;
         ReadCdRom = ReadCddaFallbackMMC;
	 ReadCdRomSub = ReadCddaSubSony;
         ReadLastAudio = ReadFirstSessionTOCMMC;
         SelectSpeed = SpeedSelectSCSIMMC;
    	 ReadTocText = ReadTocTextSCSIMMC;
	 doReadToc = ReadTocMMC;
	 ReadSubChannels = ReadSubChannelsFallbackMMC;
	 if (!memcmp(p+8,"SONY    CD-RW  CRX100E  1.0", 27)) ReadTocText = NULL;
	 if (!global.quiet) fprintf(stderr, "MMC+CDDA\n");
       break;
       case -1: /* "MMC drive does not support cdda reading, sorry\n." */
	 doReadToc = ReadTocMMC;
	 if (!global.quiet) fprintf(stderr, "MMC-CDDA\n");
	 /* FALLTHROUGH */
       case 0:      /* non SCSI-3 cdrom drive */
	 if (!global.quiet) fprintf(stderr, "no MMC\n");
         ReadLastAudio = NULL;
    if (!memcmp(p+8,"TOSHIBA", 7) ||
        !memcmp(p+8,"IBM", 3) ||
        !memcmp(p+8,"DEC", 3)) {
	    /*
	     * older Toshiba ATAPI drives don't identify themselves as MMC.
	     * The last digit of the model number is '2' for ATAPI drives.
	     * These are treated as MMC.
	     */
	    if (!memcmp(p+15, " CD-ROM XM-", 11) && p[29] == '2') {
         	goto lost_toshibas;
	    }
	density = 0x82;
	EnableCdda = EnableCddaModeSelect;
	ReadSubChannels = ReadStandardSub;
 	ReadCdRom = ReadStandard;
        SelectSpeed = SpeedSelectSCSIToshiba;
        if (!memcmp(p+15, " CD-ROM XM-3401",15)) {
	   Is_a_Toshiba3401 = 1;
	}
	global.in_lendian = 1;
    } else if (!memcmp(p+8,"IMS",3) ||
               !memcmp(p+8,"KODAK",5) ||
               !memcmp(p+8,"RICOH",5) ||
               !memcmp(p+8,"HP",2) ||
               !memcmp(p+8,"PHILIPS",7) ||
               !memcmp(p+8,"PLASMON",7) ||
               !memcmp(p+8,"GRUNDIG CDR100IPW",17) ||
               !memcmp(p+8,"MITSUMI CD-R ",13)) {
	EnableCdda = EnableCddaModeSelect;
	ReadCdRom = ReadStandard;
        SelectSpeed = SpeedSelectSCSIPhilipsCDD2600;

	/* treat all of these as bigendian */
	global.in_lendian = 0;

	/* no overlap reading for cd-writers */
	global.overlap = 0;
    } else if (!memcmp(p+8,"NRC",3)) {
        SelectSpeed = NULL;
    } else if (!memcmp(p+8,"YAMAHA",6)) {
	EnableCdda = EnableCddaModeSelect;
        SelectSpeed = SpeedSelectSCSIYamaha;

	/* no overlap reading for cd-writers */
	global.overlap = 0;
	global.in_lendian = 1;
    } else if (!memcmp(p+8,"PLEXTOR",7)) {
	global.in_lendian = 1;
	global.overlap = 0;
        ReadLastAudio = ReadFirstSessionTOCSony;
    	ReadTocText = ReadTocTextSCSIMMC;
	doReadToc = ReadTocSony;
	ReadSubChannels = ReadSubChannelsSony;
    } else if (!memcmp(p+8,"SONY",4)) {
	global.in_lendian = 1;
        if (!memcmp(p+16, "CD-ROM CDU55E",13)) {
	   ReadCdRom = ReadCddaMMC12;
	}
        ReadLastAudio = ReadFirstSessionTOCSony;
    	ReadTocText = ReadTocTextSCSIMMC;
	doReadToc = ReadTocSony;
	ReadSubChannels = ReadSubChannelsSony;
    } else if (!memcmp(p+8,"NEC",3)) {
	ReadCdRom = ReadCdda10;
        ReadTocText = NULL;
        SelectSpeed = SpeedSelectSCSINEC;
	global.in_lendian = 1;
        if (!memcmp(p+29,"5022.0r",3)) /* I assume all versions of the 502 require this? */
               global.overlap = 0;           /* no overlap reading for NEC CD-ROM 502 */
    } else if (!memcmp(p+8,"MATSHITA",8)) {
	ReadCdRom = ReadCdda12Matsushita;
	global.in_lendian = 1;
    }
    } /* switch (get_mmc) */
    }


    /* look if caddy is loaded */
    if (interface == GENERIC_SCSI) {
	usalp->silent++;
	while (!wait_unit_ready(usalp, 60)) {
		fprintf(stderr,"load cdrom please and press enter");
		getchar();
	}
	usalp->silent--;
    }
}

/* Check to see if the device will support SCSI generic commands. A
 * better check than simply looking at the device name. Open the
 * device, issue an inquiry. If they both succeed, there's a good
 * chance that the device works... */
#if defined(__linux__)
static int check_linux_scsi_interface(char *pdev_name)
{
    SCSI *dev = NULL;
    unsigned char *p = NULL;
	char	errstr[80];
    
	dev = usal_open(pdev_name, errstr, sizeof(errstr), 0, 0);
    if (NULL == dev)
        return EINVAL;
    p = Inquiry(dev);
    if (p)
    {
        usal_close(dev);
        return 0;
    }
    usal_close(dev);
    return EINVAL;
}
#endif

/********************** General setup *******************************/

/* As the name implies, interfaces and devices are checked.  We also
   adjust nsectors, overlap, and interface for the first time here.
   Any unnecessary privileges (setuid, setgid) are also dropped here.
*/
static void Check_interface_for_device(struct stat *statstruct, char *pdev_name)
{
#if defined(__linux__)
    int is_scsi = 1;
#endif
#ifndef STAT_MACROS_BROKEN
    if (!S_ISCHR(statstruct->st_mode) &&
	!S_ISBLK(statstruct->st_mode)) {
      fprintf(stderr, "%s is not a device\n",pdev_name);
      exit(SYNTAX_ERROR);
    }
#endif

/* Check what type of device we have */
#if defined (__linux__)
    if (check_linux_scsi_interface(pdev_name))
        is_scsi = 0;
    if (interface == GENERIC_SCSI && !is_scsi)
    {
        fprintf(stderr, "device %s does not support generic_scsi; falling back to cooked_ioctl instead\n", pdev_name);
        interface = COOKED_IOCTL;
    }
    if ((interface == COOKED_IOCTL) &&
        is_scsi &&
        (SCSI_GENERIC_MAJOR == major(statstruct->st_rdev)))
    {
        fprintf(stderr, "device %s is generic_scsi NOT cooked_ioctl\n", pdev_name);
        interface = GENERIC_SCSI;
    }
#else
    
#if defined (HAVE_ST_RDEV)
    switch (major(statstruct->st_rdev)) {
#if defined (__linux__)
    case SCSI_GENERIC_MAJOR:	/* generic */
#else
    default:			/* ??? what is the proper value here */
#endif
#ifndef STAT_MACROS_BROKEN
#if defined (__linux__)
       if (!S_ISCHR(statstruct->st_mode)) {
	 fprintf(stderr, "%s is not a char device\n",pdev_name);
	 exit(SYNTAX_ERROR);
       }

       if (interface != GENERIC_SCSI) {
	 fprintf(stderr, "wrong interface (cooked_ioctl) for this device (%s)\nset to generic_scsi\n", pdev_name);
	 interface = GENERIC_SCSI;
       }
#endif
#else
    default:			/* ??? what is the proper value here */
#endif
       break;

#if defined (__linux__) || defined (__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#if defined (__linux__)
    case SCSI_CDROM_MAJOR:     /* scsi cd */
    default:			/* for example ATAPI cds */
#else
#if defined (__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#if __FreeBSD_version >= 600021
    case 0:	/* majors abandoned */
	/* FALLTHROUGH */
#endif
#if __FreeBSD_version >= 501113
    case 4:	/* GEOM */
	/* FALLTHROUGH */
#endif
    case 117:	/* pre-GEOM atapi cd */
	if (!S_ISCHR(statstruct->st_mode)) {
	    fprintf(stderr, "%s is not a char device\n",pdev_name);
	    exit(SYNTAX_ERROR);
	}
	if (interface != COOKED_IOCTL) {
	    fprintf(stderr,
"cdrom device (%s) is not of type generic SCSI. \
Setting interface to cooked_ioctl.\n", pdev_name);
	    interface = COOKED_IOCTL;
	}
	break;
    case 19:     /* first atapi cd */
#endif
#endif
	if (!S_ISBLK(statstruct->st_mode)) {
	    fprintf(stderr, "%s is not a block device\n",pdev_name);
	    exit(SYNTAX_ERROR);
	}
#if defined (__linux__)
#if LINUX_VERSION_CODE >= 0x20600
	/* In Linux kernel 2.6 it is better to use the SCSI interface
	 * with the device.
	 */
	break;
#endif
#endif
	if (interface != COOKED_IOCTL) {
	    fprintf(stderr, 
"cdrom device (%s) is not of type generic SCSI. \
Setting interface to cooked_ioctl.\n", pdev_name);
	    interface = COOKED_IOCTL;
	}

	if (interface == COOKED_IOCTL) {
		fprintf(stderr, "\nW: The cooked_ioctl interface is functionally very limited!!\n");
#if	defined (__linux__)
		fprintf(stderr, "\nW: For good sampling quality simply use the generic SCSI interface!\n"
				"For example dev=ATA:1,0,0\n");
#endif
	}

	break;
#endif
    }
#endif
#endif 
    if (global.overlap >= global.nsectors)
      global.overlap = global.nsectors-1;
}

/* open the cdrom device */
static int OpenCdRom(char *pdev_name)
{
  int retval = 0;
  struct stat fstatstruct;

  /*  The device (given by pdevname) can be:
      a. an SCSI device specified with a /dev/xxx name,
      b. an SCSI device specified with bus,target,lun numbers,
      c. a non-SCSI device such as ATAPI or proprietary CDROM devices.
   */
#ifdef HAVE_IOCTL_INTERFACE
  struct stat statstruct;
  int have_named_device = 0;

	have_named_device = FALSE;
	if (pdev_name) {
		have_named_device = strchr(pdev_name, ':') == NULL
					&& memcmp(pdev_name, "/dev/", 5) == 0;
	}

  if (have_named_device) {
    if (stat(pdev_name, &statstruct)) {
      fprintf(stderr, "cannot stat device %s\n", pdev_name);
      exit(STAT_ERROR);
    } else {
      Check_interface_for_device( &statstruct, pdev_name );
    }
  }
#endif

  if (interface == GENERIC_SCSI) {
	char	errstr[80];

	priv_on();
	needroot(0);
	needgroup(0);
	/*
	 * Call usal_remote() to force loading the remote SCSI transport library
	 * code that is located in librusal instead of the dummy remote routines
	 * that are located inside libusal.
	 */
	usal_remote();
	if (pdev_name != NULL &&
	    ((strncmp(pdev_name, "HELP", 4) == 0) ||
	     (strncmp(pdev_name, "help", 4) == 0))) {
		usal_help(stderr);
		exit(NO_ERROR);
	}

	if (global.scandevs) {
		list_devices(usalp, stdout, 0);
		exit(0);
	}

	/* device name, debug, verboseopen */
	usalp = usal_open(pdev_name, errstr, sizeof(errstr), 0, 0);

	if (usalp == NULL) {
		int	err = geterrno();

		errmsgno(err, "%s%sCannot open SCSI driver.\n", errstr, errstr[0]?". ":"");
		errmsgno(EX_BAD, "For possible targets try 'wodim -scanbus'.%s\n",
					geteuid() ? " Make sure you are root.":"");
		priv_off();
        	dontneedgroup();
        	dontneedroot();
#if defined(sun) || defined(__sun)
		fprintf(stderr, "On SunOS/Solaris make sure you have Joerg Schillings usal SCSI driver installed.\n");
#endif
#if defined (__linux__)
	        fprintf(stderr, "Use the script scan_scsi.linux to find out more.\n");
#endif
	        fprintf(stderr, "Probably you did not define your SCSI device.\n");
	        fprintf(stderr, "Set the CDDA_DEVICE environment variable or use the -D option.\n");
	        fprintf(stderr, "You can also define the default device in the Makefile.\n");
		fprintf(stderr, "For possible transport specifiers try 'wodim dev=help'.\n");
	        exit(SYNTAX_ERROR);
	}
	usal_settimeout(usalp, 300);
	usal_settimeout(usalp, 60);
	usalp->silent = global.scsi_silent;
	usalp->verbose = global.scsi_verbose;

	if (global.nsectors > (unsigned) usal_bufsize(usalp, 3*1024*1024)/CD_FRAMESIZE_RAW)
		global.nsectors = usal_bufsize(usalp, 3*1024*1024)/CD_FRAMESIZE_RAW;
	if (global.overlap >= global.nsectors)
		global.overlap = global.nsectors-1;

	/*
	 * Newer versions of Linux seem to introduce an incompatible change
	 * and require root privileges or limit RLIMIT_MEMLOCK infinity
	 * in order to get a SCSI buffer in case we did call mlockall(MCL_FUTURE).
	 */
	init_scsibuf(usalp, global.nsectors*CD_FRAMESIZE_RAW);
	priv_off();
	dontneedgroup();
	dontneedroot();

	if (global.scanbus) {
		select_target(usalp, stdout);
		exit(0);
	}
  } else {
      needgroup(0);
      retval = open(pdev_name,O_RDONLY
#ifdef	linux
				| O_NONBLOCK
#endif
	);
      dontneedgroup();

      if (retval < 0) {
        fprintf(stderr, "while opening %s :", pdev_name);
        perror("");
        exit(DEVICEOPEN_ERROR);
      }

      /* Do final security checks here */
      if (fstat(retval, &fstatstruct)) {
        fprintf(stderr, "Could not fstat %s (fd %d): ", pdev_name, retval);
        perror("");
        exit(STAT_ERROR);
      }
      Check_interface_for_device( &fstatstruct, pdev_name );

#if defined HAVE_IOCTL_INTERFACE
      /* Watch for race conditions */
      if (have_named_device 
          && (fstatstruct.st_dev != statstruct.st_dev ||
              fstatstruct.st_ino != statstruct.st_ino)) {
         fprintf(stderr,"Race condition attempted in OpenCdRom.  Exiting now.\n");
         exit(RACE_ERROR);
      }
#endif
	/*
	 * The structure looks like a desaster :-(
	 * We do this more than once as it is impossible to understand where
	 * the right place would be to do this....
	 */
	if (usalp != NULL) {
		usalp->verbose = global.scsi_verbose;
	}
  }
  return retval;
}
#endif /* SIM_CD */

/******************* Simulation interface *****************/
#if	defined SIM_CD
#include "toc.h"
static unsigned long sim_pos=0;

/* read 'SectorBurst' adjacent sectors of audio sectors 
 * to Buffer '*p' beginning at sector 'lSector'
 */
static int ReadCdRom_sim(SCSI *x, UINT4 *p, unsigned lSector, 
								 unsigned SectorBurstVal);
static int ReadCdRom_sim(SCSI *x, UINT4 *p, unsigned lSector, 
								 unsigned SectorBurstVal)
{
  unsigned int loop=0;
  Int16_t *q = (Int16_t *) p;
  int joffset = 0;

  if (lSector > g_toc[cdtracks].dwStartSector || lSector + SectorBurstVal > g_toc[cdtracks].dwStartSector + 1) {
    fprintf(stderr, "Read request out of bounds: %u - %u (%d - %d allowed)\n",
	lSector, lSector + SectorBurstVal, 0, g_toc[cdtracks].dwStartSector);
  }
#if 0
  /* jitter with a probability of jprob */
  if (random() <= jprob) {
    /* jitter up to jmax samples */
    joffset = random();
  }
#endif

#ifdef DEBUG_SHM
  fprintf(stderr, ", last_b = %p\n", *last_buffer);
#endif
  for (loop = lSector*CD_FRAMESAMPLES + joffset; 
       loop < (lSector+SectorBurstVal)*CD_FRAMESAMPLES + joffset; 
       loop++) {
    *q++ = loop;
    *q++ = ~loop;
  }
#ifdef DEBUG_SHM
  fprintf(stderr, "sim wrote from %p upto %p - 4 (%d), last_b = %p\n",
          p, q, SectorBurstVal*CD_FRAMESAMPLES, *last_buffer);
#endif
  sim_pos = (lSector+SectorBurstVal)*CD_FRAMESAMPLES + joffset; 
  return SectorBurstVal;
}

static int Play_at_sim(SCSI *x, unsigned int from_sector, unsigned int sectors);
static int Play_at_sim(SCSI *x, unsigned int from_sector, unsigned int sectors)
{
  sim_pos = from_sector*CD_FRAMESAMPLES; 
  return 0;
}

static unsigned sim_indices;


/* read the table of contents (toc) via the ioctl interface */
static unsigned ReadToc_sim(SCSI *x, TOC *toc);
static unsigned ReadToc_sim(SCSI *x, TOC *toc)
{
    unsigned int scenario;
    int scen[12][3] = { 
      {1,1,500}, 
      {1,2,500}, 
      {1,99,150*99}, 
      {2,1,500}, 
      {2,2,500}, 
      {2,99,150*99},
      {2,1,500}, 
      {5,2,500}, 
      {5,99,150*99}, 
      {99,1,1000}, 
      {99,2,1000}, 
      {99,99,150*99}, 
    };
    unsigned int i;
    unsigned trcks;
#if 0
    fprintf(stderr, "select one of the following TOCs\n"
	    "0 :  1 track  with  1 index\n"
	    "1 :  1 track  with  2 indices\n"
	    "2 :  1 track  with 99 indices\n"
	    "3 :  2 tracks with  1 index each\n"
	    "4 :  2 tracks with  2 indices each\n"
	    "5 :  2 tracks with 99 indices each\n"
	    "6 :  2 tracks (data and audio) with  1 index each\n"
	    "7 :  5 tracks with  2 indices each\n"
	    "8 :  5 tracks with 99 indices each\n"
	    "9 : 99 tracks with  1 index each\n"
	    "10: 99 tracks with  2 indices each\n"
	    "11: 99 tracks with 99 indices each\n"
	    );

    do {
      scanf("%u", &scenario);
    } while (scenario > sizeof(scen)/2/sizeof(int));
#else
    scenario = 6;
#endif
    /* build table of contents */

#if 0
    trcks = scen[scenario][0] + 1;
    sim_indices = scen[scenario][1];

    for (i = 0; i < trcks; i++) {
        toc[i].bFlags = (scenario == 6 && i == 0) ? 0x40 : 0xb1;
        toc[i].bTrack = i + 1;
        toc[i].dwStartSector = i * scen[scenario][2];
        toc[i].mins = (toc[i].dwStartSector+150) / (60*75);
        toc[i].secs = (toc[i].dwStartSector+150 / 75) % (60);
        toc[i].frms = (toc[i].dwStartSector+150) % (75);
    }
    toc[i].bTrack = 0xaa;
    toc[i].dwStartSector = i * scen[scenario][2];
    toc[i].mins = (toc[i].dwStartSector+150) / (60*75);
    toc[i].secs = (toc[i].dwStartSector+150 / 75) % (60);
    toc[i].frms = (toc[i].dwStartSector+150) % (75);
#else
    {
      int starts[15] = { 23625, 30115, 39050, 51777, 67507, 
		88612, 112962, 116840, 143387, 162662,
		173990, 186427, 188077, 209757, 257120};
      trcks = 14 + 1;
      sim_indices = 1;

      for (i = 0; i < trcks; i++) {
        toc[i].bFlags = 0x0;
        toc[i].bTrack = i + 1;
        toc[i].dwStartSector = starts[i];
        toc[i].mins = (starts[i]+150) / (60*75);
        toc[i].secs = (starts[i]+150 / 75) % (60);
        toc[i].frms = (starts[i]+150) % (75);
      }
      toc[i].bTrack = 0xaa;
      toc[i].dwStartSector = starts[i];
      toc[i].mins = (starts[i]) / (60*75);
      toc[i].secs = (starts[i] / 75) % (60);
      toc[i].frms = (starts[i]) % (75);
    }
#endif
    return --trcks;           /* without lead-out */
}


static subq_chnl *ReadSubQ_sim(SCSI *usalp, unsigned char sq_format, 
										 unsigned char track);
/* request sub-q-channel information. This function may cause confusion
 * for a drive, when called in the sampling process.
 */
static subq_chnl *ReadSubQ_sim(SCSI *usalp, unsigned char sq_format, 
										 unsigned char track)
{
    subq_chnl *SQp = (subq_chnl *) (SubQbuffer);
    subq_position *SQPp = (subq_position *) &SQp->data;
    unsigned long sim_pos1;
    unsigned long sim_pos2;

    if ( sq_format != GET_POSITIONDATA ) return NULL;  /* not supported by sim */

    /* simulate CDROMSUBCHNL ioctl */

    /* copy to SubQbuffer */
    SQp->audio_status 	= 0;
    SQp->format 	= 0xff;
    SQp->control_adr	= 0xff;
    sim_pos1 = sim_pos/CD_FRAMESAMPLES;
    sim_pos2 = sim_pos1 % 150;
    SQp->track 		= (sim_pos1 / 5000) + 1;
    SQp->index 		= ((sim_pos1 / 150) % sim_indices) + 1;
    sim_pos1 += 150;
    SQPp->abs_min 	= sim_pos1 / (75*60);
    SQPp->abs_sec 	= (sim_pos1 / 75) % 60;
    SQPp->abs_frame 	= sim_pos1 % 75;
    SQPp->trel_min 	= sim_pos2 / (75*60);
    SQPp->trel_sec 	= (sim_pos2 / 75) % 60;
    SQPp->trel_frame 	= sim_pos2 % 75;

    return (subq_chnl *)(SubQbuffer);
}

static void SelectSpeed_sim(SCSI *x, unsigned sp);
/* ARGSUSED */
static void SelectSpeed_sim(SCSI *x, unsigned sp)
{
}

static void trash_cache_sim(UINT4 *p, unsigned lSector, 
									 unsigned SectorBurstVal);

/* ARGSUSED */
static void trash_cache_sim(UINT4 *p, unsigned lSector, 
									 unsigned SectorBurstVal)
{
}

static void SetupSimCd(void);

static void SetupSimCd()
{
    EnableCdda = (void (*)(SCSI *, int, unsigned))Dummy;
    ReadCdRom = ReadCdRom_sim;
    ReadCdRomData = (int (*)(SCSI *, unsigned char *, unsigned, unsigned))ReadCdRom_sim;
    doReadToc = ReadToc_sim;
    ReadTocText = NULL;
    ReadSubQ = ReadSubQ_sim;
    ReadSubChannels = NULL;
    ReadLastAudio = NULL;
    SelectSpeed = SelectSpeed_sim;
    Play_at = Play_at_sim;
    StopPlay = (int (*)(SCSI *))Dummy;
    trash_cache = trash_cache_sim;
 
}

#endif /* def SIM_CD */

/* perform initialization depending on the interface used. */
void SetupInterface()
{
#if	defined SIM_CD
    fprintf( stderr, "SIMULATION MODE !!!!!!!!!!!\n");
#else
    /* ensure interface is setup correctly */
    global.cooked_fd = OpenCdRom ( global.dev_name );
#endif

    global.pagesize = getpagesize();

    /* request one sector for table of contents */
    bufferTOC = malloc( CD_FRAMESIZE_RAW + 96 );      /* assumes sufficient aligned addresses */
    /* SubQchannel buffer */
    SubQbuffer = malloc( 48 );               /* assumes sufficient aligned addresses */
    cmd = malloc( 18 );                      /* assumes sufficient aligned addresses */
    if ( !bufferTOC || !SubQbuffer || !cmd ) {
       fprintf( stderr, "Too low on memory. Giving up.\n");
       exit(NOMEM_ERROR);
    }

#if	defined SIM_CD
    usalp = malloc(sizeof(* usalp));
    if (usalp == NULL) {
	FatalError("No memory for SCSI structure.\n");
    }
    usalp->silent = 0;
    SetupSimCd();
#else
    /* if drive is of type scsi, get vendor name */
    if (interface == GENERIC_SCSI) {
        unsigned sector_size;

	SetupSCSI();
        sector_size = get_orig_sectorsize(usalp, &orgmode4, &orgmode10, &orgmode11);
	if (!SCSI_emulated_ATAPI_on(usalp)) {
          if ( sector_size != 2048 && set_sectorsize(usalp, 2048) ) {
	    fprintf( stderr, "Could not change sector size from %d to 2048\n", sector_size );
          }
        } else {
          sector_size = 2048;
        }

	/* get cache setting */

	/* set cache to zero */

    } else {
#if defined (HAVE_IOCTL_INTERFACE)
	usalp = malloc(sizeof(* usalp));
	if (usalp == NULL) {
		FatalError("No memory for SCSI structure.\n");
	}
	usalp->silent = 0;
	SetupCookedIoctl( global.dev_name );
#else
	FatalError("Sorry, there is no known method to access the device.\n");
#endif
    }
#endif	/* if def SIM_CD */
	/*
	 * The structure looks like a desaster :-(
	 * We do this more than once as it is impossible to understand where
	 * the right place would be to do this....
	 */
	if (usalp != NULL) {
		usalp->verbose = global.scsi_verbose;
	}
}

#ifdef	HAVE_PRIV_H
#include <priv.h>
#endif

void
priv_init()
{
#ifdef	HAVE_PRIV_SET
	/*
	 * Give up privs we do not need anymore.
	 * We no longer need:
	 *	file_dac_read,sys_devices,proc_priocntl,net_privaddr
	 */
	priv_set(PRIV_OFF, PRIV_EFFECTIVE,
		PRIV_FILE_DAC_READ, PRIV_PROC_PRIOCNTL,
		PRIV_NET_PRIVADDR, NULL);
	priv_set(PRIV_OFF, PRIV_INHERITABLE,
		PRIV_FILE_DAC_READ, PRIV_PROC_PRIOCNTL,
		PRIV_NET_PRIVADDR, PRIV_SYS_DEVICES, NULL);
#endif
}

void
priv_on()
{
#ifdef	HAVE_PRIV_SET
	/*
	 * Get back privs we may need now.
	 * We need:
	 *	file_dac_read,sys_devices,proc_priocntl,net_privaddr
	 */
	priv_set(PRIV_ON, PRIV_EFFECTIVE,
		PRIV_FILE_DAC_READ, PRIV_PROC_PRIOCNTL,
		PRIV_NET_PRIVADDR, NULL);
#endif
}

void
priv_off()
{
#ifdef	HAVE_PRIV_SET
	/*
	 * Give up privs we do not need anymore.
	 * We no longer need:
	 *	file_dac_read,sys_devices,proc_priocntl,net_privaddr
	 */
	priv_set(PRIV_OFF, PRIV_EFFECTIVE,
		PRIV_FILE_DAC_READ, PRIV_PROC_PRIOCNTL,
		PRIV_NET_PRIVADDR, NULL);
#endif
}
