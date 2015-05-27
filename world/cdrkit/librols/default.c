/* 
 * Copyright 2006 Eduard Bloch 
 *
 * Uses my config parser code and small wrappers to provide the old interface.
 *
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
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

enum parstate {
	KEYBEGINSEARCH,
	KEYCOMPARE,
	EQSIGNSEARCH,
	VALBEGINSEARCH,
	LASTCHARSEARCH
};

#define GETVAL_BUF_LEN 512
#define isUspace(x) isspace( (int) (unsigned char) x)

static FILE *glob_cfg_ptr = NULL;

/*
 * Warning, uses static line buffer, not reentrant. NULL returned if the key isn't found.
 */
static char *get_value(FILE *srcfile, const char *key, int dorewind) {
	static char linebuf[GETVAL_BUF_LEN];

	if(!srcfile)
		return ((char *) NULL);

	if(dorewind)
		rewind(srcfile);

	if(!key)
		return NULL;

next_line:
	while(fgets(linebuf, sizeof(linebuf)-1, srcfile)) {
		int i;
		int keybeg;
		int s=KEYBEGINSEARCH;
		char *ret=NULL;
		int lastchar=0;

		/* simple state machine, char position moved by the states (or not),
		 * state change is done by the state (or not) */
		for( i=0 ; i<sizeof(linebuf) ; ) {
			/* printf("key: %s, %s, s: %d\n", key,  linebuf, s); */
			switch(s) {
				case(KEYBEGINSEARCH):
					{
						if(isUspace(linebuf[i]))
							i++;
						else if(linebuf[i] == '#' || linebuf[i]=='\0')
							goto next_line;
						else {
							s=KEYCOMPARE;
							keybeg=i;
						}
					}
					break;
				case(KEYCOMPARE): /* compare the key */
					{
						if(key[i-keybeg]=='\0') 
							/* end of key, next state decides what to do on this position */
							s=EQSIGNSEARCH;
						else {
							if(linebuf[i-keybeg]!=key[i-keybeg])
								goto next_line;
							else
								i++;
						}
					}
					break;
				case(EQSIGNSEARCH): /* skip whitespace, stop on =, break on anything else */
					{
						if(isUspace(linebuf[i]))
							i++;
						else if(linebuf[i]=='=') {
							s=VALBEGINSEARCH;
							i++;
						}
						else
							goto next_line;
					}
					break;
				case(VALBEGINSEARCH):
					{
						if(isUspace(linebuf[i]))
							i++;
						else {
							/* possible at EOF */
							if(linebuf[i] == '\0')
								return NULL;

							lastchar=i-1; /* lastchar can be a space, see below */
							ret= & linebuf[i];
							s=LASTCHARSEARCH;
						}
					}
					break;
				case(LASTCHARSEARCH):
					{
						if(linebuf[i]) {
							if(!isUspace(linebuf[i]))
								lastchar=i;
						}
						else { /* got string end, terminate after the last seen char */
							if(linebuf+lastchar < ret) /* no non-space found */
								return NULL;
							linebuf[lastchar+1]='\0';
							return ret;
						}
						i++;
					}
					break;
			}
		}
	}
	return NULL;
}

int cfg_open(const char *name)
{
	if(glob_cfg_ptr) {
		fclose(glob_cfg_ptr);
		glob_cfg_ptr=NULL;
	}
	if(!name) {
		glob_cfg_ptr=NULL;
		return 0;
	}
	glob_cfg_ptr = fopen(name, "r");
	return (glob_cfg_ptr ? 0 : -1);
}

int cfg_close()
{
	int r;
	if(!glob_cfg_ptr)
		return 0;
	r=fclose(glob_cfg_ptr);
	glob_cfg_ptr=NULL;
	return r;
}

void cfg_restart()
{
	get_value(glob_cfg_ptr, NULL, 1);
}

char *cfg_get(const char *key)
{
	return get_value(glob_cfg_ptr, key, 1);
}

char *cfg_get_next(const char *key)
{
	return get_value(glob_cfg_ptr, key, 0);
}
