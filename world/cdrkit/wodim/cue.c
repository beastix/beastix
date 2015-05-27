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

/* @(#)cue.c	1.20 04/03/02 Copyright 2001-2004 J. Schilling */
/*
 *	Cue sheet parser
 *
 *	Copyright (c) 2001-2004 J. Schilling
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
#include <unixstd.h>
#include <standard.h>
#include <fctldefs.h>
#include <statdefs.h>
#include <vadefs.h>
#include <schily.h>
#include <strdefs.h>
#include <utypes.h>
#include <ctype.h>
#include <errno.h>

#include "xio.h"
#include "cdtext.h"
#include "wodim.h"
#include "auheader.h"
#include "libport.h"

typedef struct state {
	char	*filename;
	void	*xfp;
	Llong	trackoff;
	Llong	filesize;
	int	filetype;
	int	tracktype;
	int	sectype;
	int	dbtype;
	int	secsize;
	int	dataoff;
	int	state;
	int	track;
	int	index;
	long	index0;
	long	index1;		/* Current index 1 value	*/
	long	secoff;		/* Old index 1 value		*/
	long	pregapsize;
	long	postgapsize;
	int	flags;
} state_t;

#define	STATE_NONE	0
#define	STATE_POSTGAP	1
#define	STATE_TRACK	2
#define	STATE_FLAGS	3
#define	STATE_INDEX0	4
#define	STATE_INDEX1	5

typedef struct keyw {
	char	*k_name;
	int	k_type;
} keyw_t;

/*
 *	Keywords (first word on line):
 *		CATALOG		- global	CATALOG		<MCN>
 *		CDTEXTFILE	- global	CDTEXTFILE	<fname>
 *		FILE		- track static	FILE		<fame> <type>
 *		FLAGS		- track static	FLAGS		<flag> ...
 *		INDEX		- track static	INDEX		<#> <mm:ss:ff>
 *		ISRC		- track static	ISRC		<ISRC>
 *		PERFORMER	- global/static	PERFORMER	<string>
 *		POSTGAP		- track locak	POSTGAP		<mm:ss:ff>
 *		PREGAP		- track static	PREGAP		<mm:ss:ff>
 *		REM		- anywhere	REM		<comment>
 *		SONGWRITER	- global/static	SONGWRITER	<string>
 *		TITLE		- global/static	TITLE		<string>
 *		TRACK		- track static	TRACK		<#> <datatype>
 *
 *	Order of keywords:
 *		CATALOG
 *		CDTEXTFILE
 *		PERFORMER | SONGWRITER | TITLE		Doc says past FILE...
 *		FILE					Must be past CATALOG
 *		------- Repeat the following:		mehrere FILE Commands?
 *		TRACK
 *		FLAGS | ISRC | PERFORMER | PREGAP | SONGWRITER | TITLE
 *		INDEX
 *		POSTGAP
 */

#define	K_G		0x10000		/* Global			*/
#define	K_T		0x20000		/* Track static			*/
#define	K_A		(K_T | K_G)	/* Global & Track static		*/

#define	K_MCN		(0 | K_G)	/* Media catalog number 	*/
#define	K_TEXTFILE	(1 | K_G)	/* CD-Text binary file		*/
#define	K_FILE		(2 | K_T)	/* Input data file		*/
#define	K_FLAGS		(3 | K_T)	/* Flags for ctrl nibble	*/
#define	K_INDEX		(4 | K_T)	/* Index marker for track	*/
#define	K_ISRC		(5 | K_T)	/* ISRC string for track	*/
#define	K_PERFORMER	(6 | K_A)	/* CD-Text Performer		*/
#define	K_POSTGAP	(7 | K_T)	/* Post gap for track (autogen)	*/
#define	K_PREGAP	(8 | K_T)	/* Pre gap for track (autogen)	*/
#define	K_REM		(9 | K_A)	/* Remark (Comment)		*/
#define	K_SONGWRITER	(10| K_A)	/* CD-Text Songwriter		*/
#define	K_TITLE		(11| K_A)	/* CD-Text Title		*/
#define	K_TRACK		(12| K_T)	/* Track marker			*/


static keyw_t	keywords[] = {
	{ "CATALOG",	K_MCN },
	{ "CDTEXTFILE",	K_TEXTFILE },
	{ "FILE",	K_FILE },
	{ "FLAGS",	K_FLAGS },
	{ "INDEX",	K_INDEX },
	{ "ISRC",	K_ISRC },
	{ "PERFORMER",	K_PERFORMER },
	{ "POSTGAP",	K_POSTGAP },
	{ "PREGAP",	K_PREGAP },
	{ "REM",	K_REM },
	{ "SONGWRITER",	K_SONGWRITER },
	{ "TITLE",	K_TITLE },
	{ "TRACK",	K_TRACK },
	{ NULL,		0 },
};


/*
 *	Filetypes - argument to FILE Keyword (one only):
 *
 *		BINARY		- Intel binary file (least significant byte first)
 *		MOTOTOLA	- Motorola binary file (most significant byte first)
 *		AIFF		- Audio AIFF file
 *		AU		- Sun Audio file
 *		WAVE		- Audio WAVE file
 *		MP3		- Audio MP3 file
 */
#define	K_BINARY	100
#define	K_MOTOROLA	101
#define	K_AIFF		102
#define	K_AU		103
#define	K_WAVE		104
#define	K_MP3		105
#define	K_OGG		106

static keyw_t	filetypes[] = {
	{ "BINARY",	K_BINARY },
	{ "MOTOROLA",	K_MOTOROLA },
	{ "AIFF",	K_AIFF },
	{ "AU",		K_AU },
	{ "WAVE",	K_WAVE },
	{ "MP3",	K_MP3 },
	{ "OGG",	K_OGG },
	{ NULL,		0 },
};

/*
 *	Flags - argument to FLAGS Keyword (more than one allowed):
 *		DCP		- Digital copy permitted
 *		4CH		- Four channel audio
 *		PRE		- Pre-emphasis enabled (audio tracks only)
 *		SCMS		- Serial copy management system (not supported by all recorders)
 */
#define	K_DCP		1000
#define	K_4CH		1001
#define	K_PRE		1002
#define	K_SCMS		1003

static keyw_t	flags[] = {
	{ "DCP",	K_DCP },
	{ "4CH",	K_4CH },
	{ "PRE",	K_PRE },
	{ "SCMS",	K_SCMS },
	{ NULL,		0 },
};

/*
 *	Datatypes - argument to TRACK Keyword (one only):
 *		AUDIO		- Audio/Music (2352)
 *		CDG		- Karaoke CD+G (2448)
 *		MODE1/2048	- CDROM Mode1 Data (cooked)
 *		MODE1/2352	- CDROM Mode1 Data (raw)
 *		MODE2/2336	- CDROM-XA Mode2 Data
 *		MODE2/2352	- CDROM-XA Mode2 Data
 *		CDI/2336	- CDI Mode2 Data
 *		CDI/2352	- CDI Mode2 Data
 */
#define	K_AUDIO		10000
#define	K_CDG		10001
#define	K_MODE1		10002
#define	K_MODE2		10003
#define	K_CDI		10004

static keyw_t	dtypes[] = {
	{ "AUDIO",	K_AUDIO },
	{ "CDG",	K_CDG },
	{ "MODE1",	K_MODE1 },
	{ "MODE2",	K_MODE2 },
	{ "CDI",	K_CDI },
	{ NULL,		0 },
};


int	parsecue(char *cuefname, track_t trackp[]);
void	fparsecue(FILE *f, track_t trackp[]);
static	void	parse_mcn(track_t trackp[], state_t *sp);
static	void	parse_textfile(track_t trackp[], state_t *sp);
static	void	parse_file(track_t trackp[], state_t *sp);
static	void	parse_flags(track_t trackp[], state_t *sp);
static	void	parse_index(track_t trackp[], state_t *sp);
static	void	parse_isrc(track_t trackp[], state_t *sp);
static	void	parse_performer(track_t trackp[], state_t *sp);
static	void	parse_postgap(track_t trackp[], state_t *sp);
static	void	parse_pregap(track_t trackp[], state_t *sp);
static	void	parse_songwriter(track_t trackp[], state_t *sp);
static	void	parse_title(track_t trackp[], state_t *sp);
static	void	parse_track(track_t trackp[], state_t *sp);
static	void	parse_offset(long *lp);
static	void	newtrack(track_t trackp[], state_t *sp);

static	keyw_t	*lookup(char *word, keyw_t table[]);
static	void	wdebug(void);
static	FILE	*cueopen(char *name);
static	char	*cuename(void);
static	char	*nextline(FILE *f);
static	void	ungetline(void);
static	char	*skipwhite(const char *s);
static	char	*peekword(void);
static	char	*lineend(void);
static	char	*markword(char *delim);
static	char	getdelim(void);
static	char	*getnextitem(char *delim);
static	char	*neednextitem(char *delim);
static	char	*nextword(void);
static	char	*needword(void);
static	char	*curword(void);
static	char	*nextitem(void);
static	char	*needitem(void);
static	void	checkextra(void);
static	void	cueabort(const char *fmt, ...);

#ifdef	CUE_MAIN
int	debug;
int	xdebug = 1;

int write_secs(void);
int write_secs() { return (-1); }

int 
main(int argc, char *argv[])
{
	int	i;
	track_t	track[MAX_TRACK+2];	/* Max tracks + track 0 + track AA */

	save_args(argc, argv);

	fillbytes(track, sizeof (track), '\0');
	for (i = 0; i < MAX_TRACK+2; i++)
		track[i].track = track[i].trackno = i;
	track[0].tracktype = TOC_MASK;


	parsecue(argv[1], track);
	return (0);
}
#else
extern	int	xdebug;
#endif

int 
parsecue(char *cuefname, track_t trackp[])
{
	FILE	*f = cueopen(cuefname);

	fparsecue(f, trackp);
	return (0);
}

void 
fparsecue(FILE *f, track_t trackp[])
{
	char	*word;
	struct keyw *kp;
	BOOL	isglobal = TRUE;
	state_t	state;

	state.filename	= NULL;
	state.xfp	= NULL;
	state.trackoff	= 0;
	state.filesize	= 0;
	state.filetype	= 0;
	state.tracktype	= 0;
	state.sectype	= 0;
	state.dbtype	= 0;
	state.secsize	= 0;
	state.dataoff	= 0;
	state.state	= STATE_NONE;
	state.track	= 0;
	state.index	= -1;
	state.index0	= -1;
	state.index1	= -1;
	state.secoff	= 0;
	state.pregapsize = -1;
	state.postgapsize = -1;
	state.flags	= 0;

	if (xdebug > 1)
		printf("---> Entering CUE Parser...\n");
	do {
		if (nextline(f) == NULL) {
			/*
			 * EOF on CUE File
			 * Do post processing here
			 */
			if (state.state < STATE_INDEX1)
				cueabort("Incomplete CUE file");
			if (state.xfp)
				xclose(state.xfp);
			if (xdebug > 1) {
				printf("---> CUE Parser got EOF, found %d tracks.\n",
								state.track);
			}
			return;
		}
		word = nextitem();
		if (*word == '\0')	/* empty line */
			continue;

		if (xdebug > 1)
			printf("\nKEY: '%s'     %s\n", word, peekword());
		kp = lookup(word, keywords);
		if (kp == NULL)
			cueabort("Unknown CUE keyword '%s'", word);

		if ((kp->k_type & K_G) == 0) {
			if (isglobal)
				isglobal = FALSE;
		}
		if ((kp->k_type & K_T) == 0) {
			if (!isglobal)
				cueabort("Badly placed CUE keyword '%s'", word);
		}
/*		printf("%s-", isglobal ? "G" : "T");*/
/*		wdebug();*/

		switch (kp->k_type) {

		case K_MCN:	   parse_mcn(trackp, &state);		break;
		case K_TEXTFILE:   parse_textfile(trackp, &state);	break;
		case K_FILE:	   parse_file(trackp, &state);		break;
		case K_FLAGS:	   parse_flags(trackp, &state);		break;
		case K_INDEX:	   parse_index(trackp, &state);		break;
		case K_ISRC:	   parse_isrc(trackp, &state);		break;
		case K_PERFORMER:  parse_performer(trackp, &state);	break;
		case K_POSTGAP:	   parse_postgap(trackp, &state);	break;
		case K_PREGAP:	   parse_pregap(trackp, &state);	break;
		case K_REM:						break;
		case K_SONGWRITER: parse_songwriter(trackp, &state);	break;
		case K_TITLE:	   parse_title(trackp, &state);		break;
		case K_TRACK:	   parse_track(trackp, &state);		break;

		default:
			cueabort("Panic: unknown CUE command '%s'", word);
		}
	} while (1);
}

static void 
parse_mcn(track_t trackp[], state_t *sp)
{
	char	*word;
	textptr_t *txp;

	if (sp->track != 0)
		cueabort("CATALOG keyword must be before first TRACK");

	word = needitem();
	setmcn(word, &trackp[0]);
	txp = gettextptr(0, trackp); /* MCN is isrc for trk 0 */
	txp->tc_isrc = strdup(word);

	checkextra();
}

static void 
parse_textfile(track_t trackp[], state_t *sp)
{
	char	*word;

	if (sp->track != 0)
		cueabort("CDTEXTFILE keyword must be before first TRACK");

	word = needitem();

	if (trackp[MAX_TRACK+1].flags & TI_TEXT) {
		if (!checktextfile(word)) {
			comerrno(EX_BAD,
				"Cannot use '%s' as CD-Text file.\n",
				word);
		}
		trackp[0].flags |= TI_TEXT;
	} else {
		errmsgno(EX_BAD, "Ignoring CDTEXTFILE '%s'.\n", word);
		errmsgno(EX_BAD, "If you like to write CD-Text, call wodim -text.\n");
	}

	checkextra();
}

static void 
parse_file(track_t trackp[], state_t *sp)
{
	char	cname[1024];
	char	newname[1024];
	struct keyw *kp;
	char	*word;
	char	*filetype;
	struct stat	st;
#ifdef	hint
	Llong		lsize;
#endif

	if (sp->filename != NULL)
		cueabort("Only one FILE allowed");

	word = needitem();
	if (sp->xfp)
		xclose(sp->xfp);
	sp->xfp = xopen(word, O_RDONLY|O_BINARY, 0);
	if (sp->xfp == NULL && geterrno() == ENOENT) {
		char	*p;

		if (strchr(word, '/') == 0 &&
		    strchr(cuename(), '/') != 0) {
			snprintf(cname, sizeof (cname),
				"%s", cuename());
			p = strrchr(cname, '/');
			if (p)
				*p = '\0';
			snprintf(newname, sizeof (newname),
				"%s/%s", cname, word);
			word = newname;
			sp->xfp = xopen(word, O_RDONLY|O_BINARY, 0);
		}
	}
	if (sp->xfp == NULL)
		comerr("Cannot open FILE '%s'.\n", word);

	sp->filename	 = strdup(word);
	sp->trackoff	 = 0;
	sp->filesize	 = 0;
	sp->flags	&= ~TI_SWAB;	/* Reset what we might set for FILE */

	filetype = needitem();
	kp = lookup(filetype, filetypes);
	if (kp == NULL)
		cueabort("Unknown filetype '%s'", filetype);

	switch (kp->k_type) {

	case K_BINARY:
	case K_MOTOROLA:
			if (fstat(xfileno(sp->xfp), &st) >= 0 &&
			    S_ISREG(st.st_mode)) {
				sp->filesize = st.st_size;
			} else {
				cueabort("Unknown file size for FILE '%s'",
								sp->filename);
			}
			break;
	case K_AIFF:
			cueabort("Unsupported filetype '%s'", kp->k_name);
			break;
	case K_AU:
			sp->filesize = ausize(xfileno(sp->xfp));
			break;
	case K_WAVE:
			sp->filesize = wavsize(xfileno(sp->xfp));
			sp->flags |= TI_SWAB;
			break;
	case K_MP3:
	case K_OGG:
			cueabort("Unsupported filetype '%s'", kp->k_name);
			break;

	default:	cueabort("Panic: unknown filetype '%s'", filetype);
	}

	if (sp->filesize == AU_BAD_CODING) {
		cueabort("Inappropriate audio coding in '%s'",
							sp->filename);
	}
	if (xdebug > 0)
		printf("Track %d File '%s' Filesize %lld\n",
			sp->track, sp->filename, sp->filesize);

	sp->filetype = kp->k_type;

	checkextra();


#ifdef	hint
		trackp->itracksize = lsize;
		if (trackp->itracksize != lsize)
			comerrno(EX_BAD, "This OS cannot handle large audio images.\n");
#endif
}

static void 
parse_flags(track_t trackp[], state_t *sp)
{
	struct keyw *kp;
	char	*word;

	if ((sp->state < STATE_TRACK) ||
	    (sp->state >= STATE_INDEX0))
		cueabort("Badly placed FLAGS keyword");
	sp->state = STATE_FLAGS;

	do {
		word = needitem();
		kp = lookup(word, flags);
		if (kp == NULL)
			cueabort("Unknown flag '%s'", word);

		switch (kp->k_type) {

		case K_DCP:	sp->flags |= TI_COPY;	break;
		case K_4CH:	sp->flags |= TI_QUADRO;	break;
		case K_PRE:	sp->flags |= TI_PREEMP;	break;
		case K_SCMS:	sp->flags |= TI_SCMS;	break;
		default:	cueabort("Panic: unknown FLAG '%s'", word);
		}

	} while (peekword() < lineend());

	if (xdebug > 0)
		printf("Track %d flags 0x%08X\n", sp->track, sp->flags);
}

static void 
parse_index(track_t trackp[], state_t *sp)
{
	char	*word;
	long	l;
	int	track = sp->track;

	if (sp->state < STATE_TRACK)
		cueabort("Badly placed INDEX keyword");


	word = needitem();
	if (*astolb(word, &l, 10) != '\0')
		cueabort("Not a number '%s'", word);
	if (l < 0 || l > 99)
		cueabort("Illegal index '%s'", word);

	if ((sp->index < l) &&
	    (((sp->index + 1) == l) || l == 1))
		sp->index = l;
	else
		cueabort("Badly placed INDEX %ld number", l);

	if (l > 0)
		sp->state = STATE_INDEX1;
	else
		sp->state = STATE_INDEX0;

	parse_offset(&l);

	if (xdebug > 1)
		printf("Track %d Index %d %ld\n", sp->track, sp->index, l);

	if (sp->index == 0)
		sp->index0 = l;
	if (sp->index == 1) {
		sp->index1 = l;
		trackp[track].nindex = 1;
		newtrack(trackp, sp);

		if (xdebug > 1) {
			printf("Track %d pregapsize %ld\n",
				sp->track, trackp[track].pregapsize);
		}
	}
	if (sp->index == 2) {
		trackp[track].tindex = malloc(100*sizeof (long));
		trackp[track].tindex[1] = 0;
		trackp[track].tindex[2] = l - sp->index1;
		trackp[track].nindex = 2;
	}
	if (sp->index > 2) {
		trackp[track].tindex[sp->index] = l - sp->index1;
		trackp[track].nindex = sp->index;
	}

	checkextra();
}

static void 
parse_isrc(track_t trackp[], state_t *sp)
{
	char	*word;
	textptr_t *txp;
	int	track = sp->track;

	if (track == 0)
		cueabort("ISRC keyword must be past first TRACK");

	if ((sp->state < STATE_TRACK) ||
	    (sp->state >= STATE_INDEX0))
		cueabort("Badly placed ISRC keyword");
	sp->state = STATE_FLAGS;

	word = needitem();
	setisrc(word, &trackp[track]);
	txp = gettextptr(track, trackp);
	txp->tc_isrc = strdup(word);

	checkextra();
}

static void 
parse_performer(track_t trackp[], state_t *sp)
{
	char	*word;
	textptr_t *txp;

	word = needitem();
	txp = gettextptr(sp->track, trackp);
	txp->tc_performer = strdup(word);

	checkextra();
}

static void 
parse_postgap(track_t trackp[], state_t *sp)
{
	long	l;

	if (sp->state < STATE_INDEX1)
		cueabort("Badly placed POSTGAP keyword");
	sp->state = STATE_POSTGAP;

	parse_offset(&l);
	sp->postgapsize = l;

	checkextra();
}

static void 
parse_pregap(track_t trackp[], state_t *sp)
{
	long	l;

	if ((sp->state < STATE_TRACK) ||
	    (sp->state >= STATE_INDEX0))
		cueabort("Badly placed PREGAP keyword");
	sp->state = STATE_FLAGS;

	parse_offset(&l);
	sp->pregapsize = l;

	checkextra();
}

static void 
parse_songwriter(track_t trackp[], state_t *sp)
{
	char	*word;
	textptr_t *txp;

	word = needitem();
	txp = gettextptr(sp->track, trackp);
	txp->tc_songwriter = strdup(word);

	checkextra();
}

static void 
parse_title(track_t trackp[], state_t *sp)
{
	char	*word;
	textptr_t *txp;

	word = needitem();
	txp = gettextptr(sp->track, trackp);
	txp->tc_title = strdup(word);

	checkextra();
}

static void 
parse_track(track_t trackp[], state_t *sp)
{
	struct keyw *kp;
	char	*word;
	long	l;
	long	secsize = -1;

	if ((sp->state >= STATE_TRACK) &&
	    (sp->state < STATE_INDEX1))
		cueabort("Badly placed TRACK keyword");
	sp->state = STATE_TRACK;
	sp->index = -1;

	word = needitem();
	if (*astolb(word, &l, 10) != '\0')
		cueabort("Not a number '%s'", word);
	if (l <= 0 || l > 99)
		cueabort("Illegal TRACK number '%s'", word);

	if ((sp->track < l) &&
	    (((sp->track + 1) == l) || sp->track == 0))
		sp->track = l;
	else
		cueabort("Badly placed TRACK %ld number", l);

	word = needword();
	kp = lookup(word, dtypes);
	if (kp == NULL)
		cueabort("Unknown filetype '%s'", word);

	if (getdelim() == '/') {
		word = needitem();
		if (*astol(++word, &secsize) != '\0')
			cueabort("Not a number '%s'", word);
	}

	/*
	 * Reset all flags that may be set in TRACK & FLAGS lines
	 */
	sp->flags &= ~(TI_AUDIO|TI_COPY|TI_QUADRO|TI_PREEMP|TI_SCMS);

	if (kp->k_type == K_AUDIO)
		sp->flags |= TI_AUDIO;

	switch (kp->k_type) {

	case K_CDG:
		if (secsize < 0)
			secsize = 2448;
	case K_AUDIO:
		if (secsize < 0)
			secsize = 2352;

		sp->tracktype = TOC_DA;
		sp->sectype = SECT_AUDIO;
		sp->dbtype = DB_RAW;
		sp->secsize = secsize;
		sp->dataoff = 0;
		if (secsize != 2352)
			cueabort("Unsupported sector size %ld for audio", secsize);
		break;

	case K_MODE1:
		if (secsize < 0)
			secsize = 2048;

		sp->tracktype = TOC_ROM;
		sp->sectype = SECT_ROM;
		sp->dbtype = DB_ROM_MODE1;
		sp->secsize = secsize;
		sp->dataoff = 16;
		/*
		 * XXX Sector Size == 2352 ???
		 * XXX It seems that there exist bin/cue pairs with this value
		 */
		if (secsize != 2048)
			cueabort("Unsupported sector size %ld for data", secsize);
		break;

	case K_MODE2:
	case K_CDI:
		sp->tracktype = TOC_ROM;
		sp->sectype = SECT_MODE_2;
		sp->dbtype = DB_ROM_MODE2;
		sp->secsize = secsize;
		sp->dataoff = 16;
		if (secsize == 2352) {
			sp->tracktype = TOC_XA2;
			sp->sectype = SECT_MODE_2_MIX;
			sp->sectype |= ST_MODE_RAW;
			sp->dbtype = DB_RAW;
			sp->dataoff = 0;
		} else if (secsize != 2336)
			cueabort("Unsupported sector size %ld for mode2", secsize);
		if (kp->k_type == K_CDI)
			sp->tracktype = TOC_CDI;
		break;

	default:	cueabort("Panic: unknown datatype '%s'", word);
	}

	if (sp->flags & TI_PREEMP)
		sp->sectype |= ST_PREEMPMASK;
	sp->secsize = secsize;

	if (xdebug > 1) {
		printf("Track %d Tracktype %s/%d\n",
			sp->track, kp->k_name, sp->secsize);
	}

	checkextra();
}

static void 
parse_offset(long *lp)
{
	char	*word;
	char	*p;
	long	m = -1;
	long	s = -1;
	long	f = -1;

	word = needitem();

	if (strchr(word, ':') == NULL) {
		if (*astol(word, lp) != '\0')
			cueabort("Not a number '%s'", word);
		return;
	}
	if (*(p = astolb(word, &m, 10)) != ':')
		cueabort("Not a number '%s'", word);
	if (m < 0 || m >= 160)
		cueabort("Illegal minute value in '%s'", word);
	p++;
	if (*(p = astolb(p, &s, 10)) != ':')
		cueabort("Not a number '%s'", p);
	if (s < 0 || s >= 60)
		cueabort("Illegal second value in '%s'", word);
	p++;
	if (*(p = astolb(p, &f, 10)) != '\0')
		cueabort("Not a number '%s'", p);
	if (f < 0 || f >= 75)
		cueabort("Illegal frame value in '%s'", word);

	m = m * 60 + s;
	m = m * 75 + f;
	*lp = m;
}

/*--------------------------------------------------------------------------*/
static void 
newtrack(track_t trackp[], state_t *sp)
{
	register int	i;
	register int	track = sp->track;
		Llong	tracksize;

	if (xdebug > 1)
		printf("-->Newtrack %d\n", track);
	if (track > 1) {
		tracksize = (sp->index1 - sp->secoff) * trackp[track-1].secsize;

		if (xdebug > 1)
			printf("    trackoff %lld filesize %lld index1 %ld size %ld/%lld\n",
				sp->trackoff, sp->filesize, sp->index1,
				sp->index1 - sp->secoff,
				tracksize);

		trackp[track-1].itracksize = tracksize;
		trackp[track-1].tracksize = tracksize;
		trackp[track-1].tracksecs = sp->index1 - sp->secoff;

		sp->trackoff += tracksize;
		sp->secoff = sp->index1;
	}
	/*
	 * Make 'tracks' immediately usable in track structure.
	 */
	for (i = 0; i < MAX_TRACK+2; i++)
		trackp[i].tracks = track;

	trackp[track].filename = sp->filename;
	trackp[track].xfp = xopen(sp->filename, O_RDONLY|O_BINARY, 0);
	trackp[track].trackstart = 0L;
/*
SEtzen wenn tracksecs bekannt sind
d.h. mit Index0 oder Index 1 vom nächsten track

	trackp[track].itracksize = tracksize;
	trackp[track].tracksize = tracksize;
	trackp[track].tracksecs = -1L;
*/
	tracksize = sp->filesize - sp->trackoff;

	trackp[track].itracksize = tracksize;
	trackp[track].tracksize = tracksize;
	trackp[track].tracksecs = (tracksize + sp->secsize - 1) / sp->secsize;

	if (xdebug > 1)
		printf("    Remaining Filesize %lld (%lld secs)\n",
			(sp->filesize-sp->trackoff),
			(sp->filesize-sp->trackoff +sp->secsize - 1) / sp->secsize);

	if (sp->pregapsize >= 0) {
/*		trackp[track].flags &= ~TI_PREGAP;*/
		sp->flags &= ~TI_PREGAP;
		trackp[track].pregapsize = sp->pregapsize;
	} else {
/*		trackp[track].flags |= TI_PREGAP;*/
		if (track > 1)
			sp->flags |= TI_PREGAP;
		if (track == 1)
			trackp[track].pregapsize = sp->index1 + 150;
		else if (sp->index0 < 0)
			trackp[track].pregapsize = -1;
		else
			trackp[track].pregapsize = sp->index1 - sp->index0;
	}
/*	trackp[track].padsecs = xxx*/

	trackp[track].isecsize = sp->secsize;
	trackp[track].secsize = sp->secsize;
	trackp[track].flags = sp->flags | trackp[0].flags;

	trackp[track].secspt = 0;	/* transfer size is set up in set_trsizes() */
/*	trackp[track].pktsize = pktsize; */
	trackp[track].pktsize = 0;
	trackp[track].trackno = sp->track;
	trackp[track].sectype = sp->sectype;

	trackp[track].dataoff = sp->dataoff;
	trackp[track].tracktype = sp->tracktype;
	trackp[track].dbtype = sp->dbtype;

	if (track == 1) {
		trackp[0].tracktype &= ~TOC_MASK;
		trackp[0].tracktype |= sp->tracktype;

		if (xdebug > 1) {
			printf("Track %d Tracktype %X\n",
					0, trackp[0].tracktype);
		}
	}
	if (xdebug > 1) {
		printf("Track %d Tracktype %X\n",
				track, trackp[track].tracktype);
	}
	trackp[track].nindex = 1;
	trackp[track].tindex = 0;

	if (xdebug > 1) {
		printf("Track %d flags 0x%08X\n", 0, trackp[0].flags);
		printf("Track %d flags 0x%08X\n", track, trackp[track].flags);
	}
}

/*--------------------------------------------------------------------------*/
static keyw_t *
lookup(char *word, keyw_t table[])
{
	register keyw_t	*kp = table;

	while (kp->k_name) {
		if (streql(kp->k_name, word))
			return (kp);
		kp++;
	}
	return (NULL);
}

/*--------------------------------------------------------------------------*/
/*
 * Parser low level functions start here...
 */

static	char	linebuf[4096];
static	char	*fname;
static	char	*linep;
static	char	*wordendp;
static	char	wordendc;
static	int	olinelen;
static	int	linelen;
static	int	lineno;

static	char	worddelim[] = "=:,/";
static	char	nulldelim[] = "";

static void 
wdebug()
{
/*		printf("WORD: '%s' rest '%s'\n", word, peekword());*/
		printf("WORD: '%s' rest '%s'\n", linep, peekword());
		printf("linep %lX peekword %lX end %lX\n",
			(long)linep, (long)peekword(), (long)&linebuf[linelen]);
}

static FILE *
cueopen(char *name)
{
	FILE	*f;

	f = fileopen(name, "r");
	if (f == NULL)
		comerr("Cannot open '%s'.\n", name);

	fname = name;
	return (f);
}

static char *
cuename()
{
	return (fname);
}

static char *
nextline(FILE *f)
{
	register int	len;

	do {
		fillbytes(linebuf, sizeof (linebuf), '\0');
		len = fgetline(f, linebuf, sizeof (linebuf));
		if (len < 0)
			return (NULL);
		if (len > 0 && linebuf[len-1] == '\r') {
			linebuf[len-1] = '\0';
			len--;
		}
		linelen = len;
		lineno++;
	} while (linebuf[0] == '#');

	olinelen = linelen;
	linep = linebuf;
	wordendp = linep;
	wordendc = *linep;

	return (linep);
}

static void 
ungetline()
{
	linelen = olinelen;
	linep = linebuf;
	*wordendp = wordendc;
	wordendp = linep;
	wordendc = *linep;
}

static char *
skipwhite(const char *s)
{
	register const Uchar	*p = (const Uchar *)s;

	while (*p) {
		if (!isspace(*p))
			break;
		p++;
	}
	return ((char *)p);
}

static char *
peekword()
{
	return (&wordendp[1]);
}

static char *
lineend()
{
	return (&linebuf[linelen]);
}

static char *
markword(char *delim)
{
	register	BOOL	quoted = FALSE;
	register	Uchar	c;
	register	Uchar	*s;
	register	Uchar	*from;
	register	Uchar	*to;

	for (s = (Uchar *)linep; (c = *s) != '\0'; s++) {
		if (c == '"') {
			quoted = !quoted;
/*			strcpy((char *)s, (char *)&s[1]);*/
			for (to = s, from = &s[1]; *from; ) {
				c = *from++;
				if (c == '\\' && quoted && (*from == '\\' || *from == '"'))
					c = *from++;
				*to++ = c;
			}
			*to = '\0';
			c = *s;
linelen--;
		}
		if (!quoted && isspace(c))
			break;
		if (!quoted && strchr(delim, c) && s > (Uchar *)linep)
			break;
	}
	wordendp = (char *)s;
	wordendc = (char)*s;
	*s = '\0';

	return (linep);
}

static char 
getdelim()
{
	return (wordendc);
}

static char *
getnextitem(char *delim)
{
	*wordendp = wordendc;

	linep = skipwhite(wordendp);
	return (markword(delim));
}

static char *
neednextitem(char *delim)
{
	char	*olinep = linep;
	char	*nlinep;

	nlinep = getnextitem(delim);

	if ((olinep == nlinep) || (*nlinep == '\0'))
		cueabort("Missing text");

	return (nlinep);
}

static char *
nextword()
{
	return (getnextitem(worddelim));
}

static char *
needword()
{
	return (neednextitem(worddelim));
}

static char *
curword()
{
	return (linep);
}

static char *
nextitem()
{
	return (getnextitem(nulldelim));
}

static char *
needitem()
{
	return (neednextitem(nulldelim));
}

static void 
checkextra()
{
	if (peekword() < lineend())
		cueabort("Extra text '%s'", peekword());
}

/* VARARGS1 */
static void cueabort(const char *fmt, ...)
{
	va_list	args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
	va_end(args);
  fprintf(stderr, " on line %d in '%s'.\n", lineno, fname);
  exit(EXIT_FAILURE);
}
