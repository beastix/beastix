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

#include <mconfig.h>
#include <stdio.h>
#include <standard.h>
#include <stdxlib.h>
#include <timedefs.h>
#include <strdefs.h>

#define	Uchar	unsigned char

static int encspeed(void);

static int encspeed()
{
	register Uchar	*sect;
	register int	i;
	register int	end;
	register int	secs;
	struct	timeval tv;
	struct	timeval tv2;

	sect = malloc(2352);

	secs = 10;
	end = 75*1000000 * secs;

	memset(sect, 0, sizeof(sect));
	for (i=0; i < 2352; ) {
		sect[i++] = 'J';
		sect[i++] = 'S';
	}

	gettimeofday(&tv, (struct timezone *)0);
	for (i=0; i < end; i++) {
#ifdef	OLD_LIBEDC
		do_encode_L2(sect, 1, 1);
		scramble_L2(sect);
#else

/*		lec_encode_mode1_sector(12345, sect);*/
/*		lec_scramble(sect);*/
#endif
/*		if ((i & 31) == 0) {*/
		if (1) {
			gettimeofday(&tv2, (struct timezone *)0);
			if (tv2.tv_sec >= (tv.tv_sec+secs) &&
			    tv2.tv_usec >= tv.tv_usec)
				break;
		}
	}
	printf("%d sectors/%ds\n", i, secs);
	printf("%d sectors/s\n", i/secs);
	printf("speed: %5.2fx\n", (1.0*i)/750.0);
	return ((i+74)/75) / secs ;
}

int main(int argc, char *argv[])
{
/*	lec_init();*/

/*	printf("speed: %d\n",  encspeed());*/
	encspeed();
	return (0);
}
