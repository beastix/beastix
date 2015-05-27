/* 
 * Copyright 2006 Eduard Bloch 
 *
 * This code emulates the interface of the original defaults.c file. However,
 * it improves its behaviour and deals with corner cases: prepended and
 * trailing spaces on variable and value, no requirement for using TABs
 * anymore. No requirements to insert dummy values like -1 or "".
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
#include <deflts.h>
#include <ctype.h>
#include <string.h>

#define CFGPATH "/etc/wodim.conf"
/* The better way would be exporting the meta functions to getnum.h or so */
extern int	getnum(char *arg, long *valp);

void 
cdr_defaults(char **p_dev_name, int *p_speed, long *p_fifosize, 
					char **p_drv_opts)
{
	char *t; /* tmp */
	int wc=0;
	char loc[256], sSpeed[11], sFs[11], sOpts[81];
	char *devcand=NULL;

  cfg_open(CFGPATH);

	if(p_dev_name && *p_dev_name)
		devcand=*p_dev_name;
	else if(NULL!=(t=getenv("CDR_DEVICE")))
		devcand=t;
	else if(NULL!=(t=cfg_get("CDR_DEVICE")))
		devcand=strdup(t); /* needs to use it as a key later, same stat. memory */

	if(devcand && NULL != (t=cfg_get(devcand))) {
		/* extract them now, may be used later */
		wc=sscanf(t, "%255s %10s %10s %80s", loc, sSpeed, sFs, sOpts);
	}

	if(p_dev_name) {
		if(wc>0)
			*p_dev_name = strdup(loc);
		else if(devcand) /* small mem. leak possible, does not matter, checks for that would require more code size than we loose */
			*p_dev_name=strdup(devcand);
	}
	if(p_speed) { /* sth. to write back */
		char *bad;
		int cfg_speed=-1;

		/* that value may be used twice */
		if(NULL!=(t=cfg_get("CDR_SPEED"))) {
			cfg_speed=strtol(t,&bad,10);
			if(*bad || cfg_speed<-1) {
				fprintf(stderr, "Bad default CDR_SPEED setting (%s).\n", t);
				exit(EXIT_FAILURE);
			}
		}

		if(*p_speed>0) { 
			/* ok, already set by the program arguments */
		}
		else if(NULL!=(t=getenv("CDR_SPEED"))) {
			*p_speed=strtol(t,&bad,10);
			if(*bad || *p_speed<-1) {
				fprintf(stderr, "Bad CDR_SPEED environment (%s).\n", t);
				exit(EXIT_FAILURE);
			}
		}
		else if(wc>1 && *sSpeed) {
			*p_speed=strtol(sSpeed, &bad, 10);
			if(*bad || *p_speed<-1) {
				fprintf(stderr, "Bad speed (%s) in the config, drive description.\n", sSpeed);
				exit(EXIT_FAILURE);
			}
			if(*p_speed==-1) 
				/* that's autodetect, use the config default as last ressort */
				*p_speed=cfg_speed;
		}
		else 
			*p_speed=cfg_speed;
	}
	if(p_fifosize) { /* sth. to write back */
		if(*p_fifosize>0) { 
			/* ok, already set by the user */
		}
		else if(NULL!=(t=getenv("CDR_FIFOSIZE"))) {
			if(getnum(t, p_fifosize)!=1 || *p_fifosize<-1) {
				fprintf(stderr, "Bad CDR_FIFOSIZE environment (%s).\n", t);
				exit(EXIT_FAILURE);
			}
		}
		else if(wc>2 && *sFs && strcmp("-1", sFs)) {
			if(getnum(sFs, p_fifosize)!=1 || *p_fifosize<-1) {
				fprintf(stderr, "Bad fifo size (%s) in the config, device description.\n", sFs);
				exit(EXIT_FAILURE);
			}
		}
		else if(NULL!=(t=cfg_get("CDR_FIFOSIZE"))) {
			if(getnum(t, p_fifosize)!=1 || *p_fifosize<-1) {
				fprintf(stderr, "Bad speed default setting (%s).\n", t);
				exit(EXIT_FAILURE);
			}
		}
		if(NULL!=(t=cfg_get("CDR_MAXFIFOSIZE"))) {
			long max;
			if(getnum(t, &max)!=1 || *p_fifosize<-1) {
				fprintf(stderr, "Bad CDR_MAXFIFOSIZE setting (%s).\n", t);
				exit(EXIT_FAILURE);
			}
			if(*p_fifosize>max)
				*p_fifosize=max;
		}
	}

	if(p_drv_opts && !*p_drv_opts && wc>3 && strcmp(sOpts, "\"\""))
		*p_drv_opts=strdup(sOpts);

  cfg_close();

}
