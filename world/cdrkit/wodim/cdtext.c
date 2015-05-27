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

/* @(#)cdtext.c	1.10 04/03/01 Copyright 1999-2004 J. Schilling */
/*
 *	Generic CD-Text support functions
 *
 *	Copyright (c) 1999-2004 J. Schilling
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
#include <stdxlib.h>
#include <unixstd.h>	/* Include sys/types.h to make off_t available */
#include <standard.h>
#include <utypes.h>
#include <strdefs.h>
#include <schily.h>

#include <usal/scsitransp.h>	/* For write_leadin() */

#include "cdtext.h"
#include "wodim.h"
#include "crc16.h"

#define	PTI_TITLE	0x80	/* Album name and Track titles */
#define	PTI_PERFORMER	0x81	/* Singer/player/conductor/orchestra */
#define	PTI_SONGWRITER	0x82	/* Name of the songwriter */
#define	PTI_COMPOSER	0x83	/* Name of the composer */
#define	PTI_ARRANGER	0x84	/* Name of the arranger */
#define	PTI_MESSAGE	0x85	/* Message from content provider or artist */
#define	PTI_DISK_ID	0x86	/* Disk identification information */
#define	PTI_GENRE	0x87	/* Genre identification / information */
#define	PTI_TOC		0x88	/* TOC information */
#define	PTI_TOC2	0x89	/* Second TOC */
#define	PTI_RES_8A	0x8A	/* Reserved 8A */
#define	PTI_RES_8B	0x8B	/* Reserved 8B */
#define	PTI_RES_8C	0x8C	/* Reserved 8C */
#define	PTI_CLOSED_INFO	0x8D	/* For internal use by content provider */
#define	PTI_ISRC	0x8E	/* UPC/EAN code of album and ISRC for tracks */
#define	PTI_SIZE	0x8F	/* Size information of the block */

extern	int	xdebug;

typedef struct textpack {
	Uchar	pack_type;	/* Pack Type indicator	*/
	char	track_no;	/* Track Number (0..99)	*/
	char	seq_number;	/* Sequence Number	*/
	char	block_number;	/* Block # / Char pos	*/
	char	text[12];	/* CD-Text Data field	*/
	char	crc[2];		/* CRC 16		*/
} txtpack_t;

#define	EXT_DATA 0x80		/* Extended data indicator in track_no */
#define	DBCC	 0x80		/* Double byte char indicator in block */

/*
 *	CD-Text size example:
 *
 *	0  1  2  3  00 01 02 03 04 05 06 07 08 09 10 11 CRC16
 *
 *	8F 00 2B 00 01 01 0D 03 0C 0C 00 00 00 00 01 00 7B 3D
 *	8F 01 2C 00 00 00 00 00 00 00 12 03 2D 00 00 00 DA B7
 *	8F 02 2D 00 00 00 00 00 09 00 00 00 00 00 00 00 6A 24
 *
 *	charcode 1
 *	first tr 1
 *	last tr  13
 *	Copyr	 3
 *	Pack Count 80= 12, 81 = 12, 86 = 1, 8e = 18, 8f = 3
 *	last seq   0 = 2d
 *	languages  0 = 9
 */

typedef struct textsizes {
	char	charcode;
	char	first_track;
	char	last_track;
	char	copyr_flags;
	char	pack_count[16];
	char	last_seqnum[8];
	char	language_codes[8];
} txtsize_t;

typedef struct textargs {
	txtpack_t	*tp;
	char		*p;
	txtsize_t	*tsize;
	int		seqno;
} txtarg_t;


Uchar	*textsub;
int	textlen;

BOOL			checktextfile(char *fname);
static void	setuptextdata(Uchar *bp, int len);
static BOOL	cdtext_crc_ok(struct textpack *p);
void			packtext(int tracks, track_t *trackp);
static BOOL	anytext(int pack_type, int tracks, track_t *trackp);
static void	fillup_pack(txtarg_t *ap);
static void	fillpacks(txtarg_t *ap, char *from, int len, int track_no, int pack_type);
int			write_cdtext(SCSI *usalp, cdr_t *dp, long startsec);
static void	eight2six(Uchar *in, Uchar *out);
static void	six2eight(Uchar *in, Uchar *out);


BOOL checktextfile(char *fname)
{
	FILE	*f;
	Uchar	hbuf[4];
	Uchar	*bp;
	struct textpack *tp;
	int	len;
	int	crc;
	int	n;
	int	j;
	off_t	fs;

	if ((f = fileopen(fname, "rb")) == NULL) {
		errmsg("Cannot open '%s'.\n", fname);
		return (FALSE);
	}
	fs = filesize(f);
	j = fs % sizeof (struct textpack);
	if (j == 4) {
		n = fileread(f, hbuf, 4);
		if (n != 4) {
			if (n < 0)
				errmsg("Cannot read '%s'.\n", fname);
			else
				errmsgno(EX_BAD, "File '%s' is too small for CD-Text.\n", fname);
			return (FALSE);
		}
		len = hbuf[0] * 256 + hbuf[1];
		len -= 2;
		n = fs - 4;
		if (n != len) {
			errmsgno(EX_BAD, "Inconsistent CD-Text file '%s' length should be %d but is %lld\n",
				fname, len+4, (Llong)fs);
			return (FALSE);
		}
	} else if (j != 0) {
		errmsgno(EX_BAD, "Inconsistent CD-Text file '%s' not a multiple of pack length\n",
			fname);
		return (FALSE);
	} else {
		len = fs;
	}
	printf("Text len: %d\n", len);
	bp = malloc(len);
	if (bp == NULL) {
		errmsg("Cannot malloc CD-Text read buffer.\n");
		return (FALSE);
	}
	n = fileread(f, bp, len);

	tp = (struct textpack *)bp;
	for (n = 0; n < len; n += sizeof (struct textpack), tp++) {
		if (tp->pack_type < 0x80 || tp->pack_type > 0x8F) {
			errmsgno(EX_BAD, "Illegal pack type 0x%02X pack #%ld in CD-Text file '%s'.\n",
				tp->pack_type, (long)(n/sizeof (struct textpack)), fname);
			return (FALSE);
		}
		crc = (tp->crc[0] & 0xFF) << 8 | (tp->crc[1] & 0xFF);
		crc ^= 0xFFFF;
		if (crc != calcCRC((Uchar *)tp, sizeof (*tp)-2)) {
			if (cdtext_crc_ok(tp)) {
				errmsgno(EX_BAD,
				"Corrected CRC ERROR in pack #%ld (offset %d-%ld) in CD-Text file '%s'.\n",
				(long)(n/sizeof (struct textpack)),
				n+j, (long)(n+j+sizeof (struct textpack)),
				fname);
			} else {
			errmsgno(EX_BAD, "CRC ERROR in pack #%ld (offset %d-%ld) in CD-Text file '%s'.\n",
				(long)(n/sizeof (struct textpack)),
				n+j, (long)(n+j+sizeof (struct textpack)),
				fname);
			return (FALSE);
			}
		}
	}
	setuptextdata(bp, len);
	free(bp);

	return (TRUE);
}

static void setuptextdata(Uchar *bp, int len)
{
	int	n;
	int	i;
	int	j;
	Uchar	*p;

	if (xdebug) {
		printf("%ld packs %% 4 = %ld\n",
			(long)(len/sizeof (struct textpack)),
			(long)(len/sizeof (struct textpack)) % 4);
	}
	i = (len/sizeof (struct textpack)) % 4;
	if (i == 0) {
		n = len;
	} else if (i == 2) {
		n = 2 * len;
	} else {
		n = 4 * len;
	}
	n = (n * 4) / 3;
	p = malloc(n);
	if (p == NULL) {
		errmsg("Cannot malloc CD-Text write buffer.\n");
	}
	for (i = 0, j = 0; j < n; ) {
		eight2six(&bp[i%len], &p[j]);
		i += 3;
		j += 4;
	}
	textsub = p;
	textlen = n;

#ifdef	DEBUG
	{
	Uchar	sbuf[10000];
	struct textpack *tp;
	FILE		*f;
	int		crc;

	tp = (struct textpack *)bp;
	p = sbuf;
	for (n = 0; n < len; n += sizeof (struct textpack), tp++) {
		crc = (tp->crc[0] & 0xFF) << 8 | (tp->crc[1] & 0xFF);
		crc ^= 0xFFFF;

		printf("Pack:%3d ", n/ sizeof (struct textpack));
		printf("Pack type: %02X ", tp->pack_type & 0xFF);
		printf("Track #: %2d ", tp->track_no & 0xFF);
		printf("Sequence #:%3d ", tp->seq_number & 0xFF);
		printf("Block #:%3d ", tp->block_number & 0xFF);
		printf("CRC: %04X (%04X) ", crc, calcCRC((Uchar *)tp, sizeof (*tp)-2));
		printf("Text: '%.12s'\n", tp->text);
		movebytes(tp->text, p, 12);
		p += 12;
	}
	printf("len total: %d\n", n);
	f = fileopen("cdtext.out", "wctb");
	if (f) {
		filewrite(f, sbuf, p - sbuf);
		fflush(f);
		fclose(f);
	}
	}
#endif
}

static BOOL cdtext_crc_ok(struct textpack *p)
{
	int		crc;
	struct textpack	new;

	movebytes(p, &new, sizeof (struct textpack));
	new.crc[0] ^= 0xFF;
	new.crc[1] ^= 0xFF;
	crc = calcCRC((Uchar *)&new, sizeof (struct textpack));
	crc = flip_crc_error_corr((Uchar *)&new, sizeof (struct textpack), crc);
	new.crc[0] ^= 0xFF;
	new.crc[1] ^= 0xFF;
	if (crc == 0)
		movebytes(&new, p, 18);

	return (crc == 0);
}


void packtext(int tracks, track_t *trackp)
{
	int	type;
	int	i;
	struct textpack *tp;
	struct textsizes tsize;
	txtarg_t targ;
	char	sbuf[256*18];

	fillbytes(sbuf, sizeof (sbuf), 0);
	fillbytes(&tsize, sizeof (tsize), 0);

	tsize.charcode		= CC_8859_1;		/* ISO-8859-1	    */
	tsize.first_track	= trackp[1].trackno;
	tsize.last_track	= trackp[1].trackno + tracks - 1;
#ifdef	__FOUND_ON_COMMERCIAL_CD__
	tsize.copyr_flags	= 3;			/* for titles/names */
#else
	tsize.copyr_flags	= 0;			/* no Copyr. limitat. */
#endif
	tsize.pack_count[0x0F]	= 3;			/* 3 size packs	    */
	tsize.last_seqnum[0]	= 0;			/* Start value only */
	tsize.language_codes[0]	= LANG_ENGLISH;		/* English	    */

	tp = (struct textpack *)sbuf;

	targ.tp = tp;
	targ.p = NULL;
	targ.tsize = &tsize;
	targ.seqno = 0;

	for (type = 0; type <= 0x0E; type++) {
		register int	maxtrk;
		register char	*s;

		if (!anytext(type, tracks, trackp))
			continue;
		maxtrk = tsize.last_track;
		if (type == 6) {
			maxtrk = 0;
		}
		for (i = 0; i <= maxtrk; i++) {
			s = trackp[i].text;
			if (s)
				s = ((textptr_t *)s)->textcodes[type];
			if (s)
				fillpacks(&targ, s, strlen(s)+1, i, 0x80| type);
			else
				fillpacks(&targ, "", 1, i, 0x80| type);

		}
		fillup_pack(&targ);
	}

	/*
	 * targ.seqno overshoots by one and we add 3 size packs...
	 */
	tsize.last_seqnum[0] = targ.seqno + 2;

	for (i = 0; i < 3; i++) {
		fillpacks(&targ, &((char *)(&tsize))[i*12], 12, i, 0x8f);
	}

	setuptextdata((Uchar *)sbuf, targ.seqno*18);

#ifdef	DEBUG
	{	FILE	*f;

	f = fileopen("cdtext.new", "wctb");
	if (f) {
		filewrite(f, sbuf, targ.seqno*18);
		fflush(f);
		fclose(f);
	}
	}
#endif
}

static BOOL anytext(int pack_type, int tracks, track_t *trackp)
{
	register int	i;
	register char	*p;

	for (i = 0; i <= tracks; i++) {
		if (trackp[i].text == NULL)
			continue;
		p = ((textptr_t *)(trackp[i].text))->textcodes[pack_type];
		if (p != NULL && *p != '\0')
			return (TRUE);
	}
	return (FALSE);
}

static void fillup_pack(register txtarg_t *ap)
{
	if (ap->p) {
		fillbytes(ap->p, &ap->tp->text[12] - ap->p, '\0');
		fillcrc((Uchar *)ap->tp, sizeof (*ap->tp));
		ap->p  = 0;
		ap->tp++;
	}
}

static void fillpacks(register txtarg_t *ap, register char *from, int len, 
          				 int track_no, int pack_type)
{
	register int		charpos;
	register char		*p;
	register txtpack_t	*tp;

	tp = ap->tp;
	p  = ap->p;
	charpos = 0;
	do {
		if (p == 0) {
			p = tp->text;
			tp->pack_type = pack_type;
			if (pack_type != 0x8f)
				ap->tsize->pack_count[pack_type & 0x0F]++;
			tp->track_no = track_no;
			tp->seq_number = ap->seqno++;
			if (charpos < 15)
				tp->block_number = charpos;
			else
				tp->block_number = 15;
		}
		for (; --len >= 0 && p < &tp->text[12]; charpos++) {
			*p++ = *from++;
		}
		len++;	/* Overshoot compensation */

		if (p >= &tp->text[12]) {
			fillcrc((Uchar *)tp, sizeof (*tp));
			p = 0;
			tp++;
		}
	} while (len > 0);

	ap->tp = tp;
	ap->p = p;
}

int write_cdtext(SCSI *usalp, cdr_t *dp, long startsec)
{
	char	*bp = (char *)textsub;
	int	buflen = textlen;
	long	amount;
	long	bytes = 0;
	long	end = -150;
	int	secspt = textlen / 96;
	int	bytespt = textlen;
	long	maxdma = usalp->maxbuf;
	int	idx;
	int	secs;
	int	nbytes;

/*maxdma = 4320;*/
	if (maxdma >= (2*textlen)) {
		/*
		 * Try to make each CD-Text transfer use as much data
		 * as possible.
		 */
		bp = usalp->bufptr;
		for (idx = 0; (idx + textlen) <= maxdma; idx += textlen)
			movebytes(textsub, &bp[idx], textlen);
		buflen = idx;
		secspt = buflen / 96;
		bytespt = buflen;
/*printf("textlen: %d buflen: %d secspt: %d\n", textlen, buflen, secspt);*/
	} else if (maxdma < buflen) {
		/*
		 * We have more CD-Text data than we may transfer at once.
		 */
		secspt = maxdma / 96;
		bytespt = secspt * 96;
	}
	while (startsec < end) {
		if ((end - startsec) < secspt) {
			secspt = end - startsec;
			bytespt = secspt * 96;
		}
		idx = 0;
		secs = secspt;
		nbytes = bytespt;
		do {			/* loop over CD-Text data buffer */

			if ((idx + nbytes) > buflen) {
				nbytes = buflen - idx;
				secs = nbytes / 96;
			}
/*printf("idx: %d nbytes: %d secs: %d startsec: %ld\n",*/
/*idx, nbytes, secs, startsec);*/
			amount = write_secs(usalp, dp,
				(char *)&bp[idx], startsec, nbytes, secs, FALSE);
			if (amount < 0) {
				printf("write CD-Text data: error after %ld bytes\n",
						bytes);
				return (-1);
			}
			bytes += amount;
			idx += amount;
			startsec += secs;
		} while (idx < buflen && startsec < end);
	}
	return (0);
}


/*
 * 3 input bytes (8 bit based) are converted into 4 output bytes (6 bit based).
 */
static void eight2six(register Uchar *in, register Uchar *out)
{
	register int	c;

	c = in[0];
	out[0]  = (c >> 2) & 0x3F;
	out[1]  = (c & 0x03) << 4;

	c = in[1];
	out[1] |= (c & 0xF0) >> 4;
	out[2]  = (c & 0x0F) << 2;

	c = in[2];
	out[2] |= (c & 0xC0) >> 6;
	out[3]  = c & 0x3F;
}

/*
 * 4 input bytes (6 bit based) are converted into 3 output bytes (8 bit based).
 */
static void six2eight(register Uchar *in, register Uchar *out)
{
	register int	c;

	c = in[0] & 0x3F;
	out[0]  = c << 2;

	c = in[1] & 0x3F;
	out[0] |= c >> 4;
	out[1]  = c << 4;

	c = in[2] & 0x3F;
	out[1] |= c >> 2;
	out[2]  = c << 6;

	c = in[3] & 0x3F;
	out[2] |= c;
}
