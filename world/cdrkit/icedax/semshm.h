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

/* @(#)semshm.h	1.3 03/08/29 Copyright 1998,1999 Heiko Eissfeldt */
#undef DEBUG_SHM
#ifdef DEBUG_SHM
extern char *start_of_shm;
extern char *end_of_shm;
#endif

#define FREE_SEM	0
#define DEF_SEM	1

#if defined (HAVE_SEMGET) && defined(USE_SEMAPHORES)
extern int sem_id;
#else
#define sem_id 42	/* nearly any other number would do it too */
void init_pipes(void);
void init_parent(void);
void init_child(void);
#endif


#ifdef	HAVE_AREAS
/* the name of the shared memory mapping for the FIFO under BeOS. */
#define	AREA_NAME	"shmfifo"
#endif

void free_sem(void);
int semrequest(int semid, int semnum);
int semrelease(int semid, int semnum, int amount);
int flush_buffers(void);
void * request_shm_sem(unsigned amount_of_sh_mem, unsigned char **pointer);

