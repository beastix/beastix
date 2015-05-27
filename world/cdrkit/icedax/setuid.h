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

/* @(#)setuid.h	1.2 99/12/19 Copyright 1998,1999 Heiko Eissfeldt */
/* Security functions */
void initsecurity(void);

void needroot(int necessary);
void dontneedroot(void);
void neverneedroot(void);

void needgroup(int necessary);
void dontneedgroup(void);
void neverneedgroup(void);

#if defined (HPUX)
#define HAVE_SETREUID
#define HAVE_SETREGID
int seteuid(uid_t uid);
int setreuid(uid_t uid1, uid_t uid2);
int setregid(gid_t gid1, gid_t gid2);
#endif
