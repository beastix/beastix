/* 
 * Copyright 2006 Eduard Bloch 
 *
 * This code emulates the interface of the original defaults.c file. However,
 * it improves its behaviour and deals with corner cases: prepended and
 * trailing spaces on variable and value, no requirement for using TABs
 * anymore. No requirements to insert dummy values like -1 or "".
 *
 */
#ifndef _DEFAULTS_H_
#define _DEFAULTS_H_
extern int	getnum(char *arg, long *valp);
void cdr_defaults(char **p_dev_name, int *p_speed, long *p_fifosize, char **p_drv_opts);
#endif
