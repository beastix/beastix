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

/* @(#)cdtext.h	1.5 04/03/02 Copyright 1999-2004 J. Schilling */
/*
 *	Generic CD-Text support definitions
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

#ifndef	CDTEXT_H
#define	CDTEXT_H

/*
 * Strings for the CD-Text Pack Type indicators 0x80...0x8F
 * We cannot use a plain structure here because we like to loop
 * over all members.
 */
typedef struct textcodes {
	char	*textcodes[16];
} textptr_t;

#define	tc_title	textcodes[0x00]
#define	tc_performer	textcodes[0x01]
#define	tc_songwriter	textcodes[0x02]
#define	tc_composer	textcodes[0x03]
#define	tc_arranger	textcodes[0x04]
#define	tc_message	textcodes[0x05]
#define	tc_diskid	textcodes[0x06]
#define	tc_genre	textcodes[0x07]
#define	tc_toc		textcodes[0x08]
#define	tc_toc2		textcodes[0x09]

#define	tc_closed_info	textcodes[0x0d]
#define	tc_isrc		textcodes[0x0e]

/*
 *	binaere Felder sind
 *	Disc ID			(Wirklich ???)
 *	Genre ID
 *	TOC
 *	Second TOC
 *	Size information
 */

/*
 * Genre codes from Enhanced CD Specification page 21
 */
#define	GENRE_UNUSED		0	/* not used			    */
#define	GENRE_UNDEFINED		1	/* not defined			    */
#define	GENRE_ADULT_CONTEMP	2	/* Adult Contemporary		    */
#define	GENRE_ALT_ROCK		3	/* Alternative Rock		    */
#define	GENRE_CHILDRENS		4	/* Childrens Music		    */
#define	GENRE_CLASSIC		5	/* Classical			    */
#define	GENRE_CHRIST_CONTEMP	6	/* Contemporary Christian	    */
#define	GENRE_COUNTRY		7	/* Country			    */
#define	GENRE_DANCE		8	/* Dance			    */
#define	GENRE_EASY_LISTENING	9	/* Easy Listening		    */
#define	GENRE_EROTIC		10	/* Erotic			    */
#define	GENRE_FOLK		11	/* Folk				    */
#define	GENRE_GOSPEL		12	/* Gospel			    */
#define	GENRE_HIPHOP		13	/* Hip Hop			    */
#define	GENRE_JAZZ		14	/* Jazz				    */
#define	GENRE_LATIN		15	/* Latin			    */
#define	GENRE_MUSICAL		16	/* Musical			    */
#define	GENRE_NEWAGE		17	/* New Age			    */
#define	GENRE_OPERA		18	/* Opera			    */
#define	GENRE_OPERETTA		19	/* Operetta			    */
#define	GENRE_POP		20	/* Pop Music			    */
#define	GENRE_RAP		21	/* RAP				    */
#define	GENRE_REGGAE		22	/* Reggae			    */
#define	GENRE_ROCK		23	/* Rock Music			    */
#define	GENRE_RYTHMANDBLUES	24	/* Rhythm & Blues		    */
#define	GENRE_SOUNDEFFECTS	25	/* Sound Effects		    */
#define	GENRE_SPOKEN_WORD	26	/* Spoken Word			    */
#define	GENRE_WORLD_MUSIC	28	/* World Music			    */
#define	GENRE_RESERVED		29	/* Reserved is 29..32767	    */
#define	GENRE_RIAA		32768	/* Registration by RIAA 32768..65535 */

/*
 * Character codings used in CD-Text data.
 * Korean and Mandarin Chinese to be defined in sept 1996
 */
#define	CC_8859_1	0x00		/* ISO 8859-1			*/
#define	CC_ASCII	0x01		/* ISO 646, ASCII (7 bit)	*/
#define	CC_RESERVED_02	0x02		/* Reserved codes 0x02..0x7f	*/
#define	CC_KANJI	0x80		/* Music Shift-JIS Kanji	*/
#define	CC_KOREAN	0x81		/* Korean			*/
#define	CC_CHINESE	0x82		/* Mandarin Chinese		*/
#define	CC_RESERVED_83	0x83		/* Reserved codes 0x83..0xFF	*/


/*
 * The language code is encoded as specified in ANNEX 1 to part 5 of EBU
 * Tech 32 58 -E (1991).
 *
 * The current language codes are guessed
 */
#define	LANG_CZECH	 6		/* 0x06				*/
#define	LANG_DANISH	 7		/* 0x07				*/
#define	LANG_GERMAN	 8		/* 0x08				*/
#define	LANG_ENGLISH	 9		/* 0x09				*/
#define	LANG_SPANISH	10		/* 0x0A				*/
#define	LANG_FRENCH	15		/* 0x0F				*/
#define	LANG_ITALIAN	21		/* 0x15				*/
#define	LANG_HUNGARIAN	27		/* 0x1B				*/
#define	LANG_DUTCH	29		/* 0x1D				*/
#define	LANG_NORWEGIAN	30		/* 0x1E				*/
#define	LANG_POLISH	32		/* 0x20				*/
#define	LANG_PORTUGUESE	33		/* 0x21				*/
#define	LANG_SLOVENE	38		/* 0x26				*/
#define	LANG_FINNISH	39		/* 0x27				*/
#define	LANG_SWEDISH	40		/* 0x28				*/
#define	LANG_RUSSIAN	86		/* 0x56				*/
#define	LANG_KOREAN	101		/* 0x65				*/
#define	LANG_JAPANESE	105		/* 0x69				*/
#define	LANG_GREEK	112		/* 0x70				*/
#define	LANG_CHINESE	117		/* 0x75				*/

#endif
