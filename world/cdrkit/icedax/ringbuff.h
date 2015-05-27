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

/* @(#)ringbuff.h	1.5 01/10/20 Copyright 1998,1999,2000 Heiko Eissfeldt */
/* This file contains data structures that reside in the shared memory
 * segment.
 */


/* the linux semctl prototype is broken as is the definition
   of union semun in sys/sem.h. */

#ifdef HAVE_UNION_SEMUN
#	define	my_semun	semun
#else
union my_semun {
  int val;
  struct semid_ds *pid;
  unsigned short *array;
};
#endif

/* Ringbuffer structures.
   Space for the ringbuffer is allocated page aligned
	 and contains the following

	-------------------- start of page
	header (once for the ring buffer) \\
	space for page alignment	  ||+- HEADER_SIZE
RB_BASE -+v				  ||
	myringbuffer.offset		  |/
	-------------------- start of page/-- pagesize
	myringbuffer.data (SEGMENT_SIZE)\
	space for page alignment	|+- ENTRY_SIZE_PAGE_AL
	myringbuffer.offset		/
	-------------------- start of page
	myringbuffer.data
	space for page alignment
	...
*/	
typedef struct {
  int offset;
  UINT4 data[CD_FRAMESAMPLES];
} myringbuff;

struct ringbuffheader {
  myringbuff *p1;
  myringbuff *p2;
  volatile unsigned long total_read;
  volatile unsigned long total_written;
  volatile int child_waitstate;
  volatile int parent_waitstate;
  volatile int input_littleendian;
  volatile int end_is_reached;
  volatile unsigned long nSamplesToDo;
  int offset;
  UINT4 data[CD_FRAMESAMPLES];
};

extern myringbuff **he_fill_buffer;
extern myringbuff **last_buffer;
extern volatile unsigned long *total_segments_read;
extern volatile unsigned long *total_segments_written;
extern volatile int *child_waits;
extern volatile int *parent_waits;
extern volatile int *in_lendian;
extern volatile int *eorecording;

#define palign(x, a)    (((char *)(x)) + ((a) - 1 - (((unsigned)((x)-1))%(a))))
#define multpage(x, a)    ((((x) + (a) - 1) / (a)) * (a))

#define HEADER_SIZE multpage(offset_of(struct ringbuffheader, data), global.pagesize)
#define SEGMENT_SIZE (global.nsectors*CD_FRAMESIZE_RAW)
#define ENTRY_SIZE_PAGE_AL multpage(SEGMENT_SIZE + offset_of(myringbuff, data), global.pagesize)

#define RB_BASE ((myringbuff *)(((unsigned char *)he_fill_buffer) + HEADER_SIZE - offset_of(myringbuff, data)))

#define INC(a) (myringbuff *)(((char *)RB_BASE) + (((((char *) (a))-((char *)RB_BASE))/ENTRY_SIZE_PAGE_AL + 1) % total_buffers)*ENTRY_SIZE_PAGE_AL)


void set_total_buffers(unsigned int num_buffers, int mysem_id);
const myringbuff *get_previous_read_buffer(void);
const myringbuff *get_he_fill_buffer(void);
myringbuff *get_next_buffer(void);
myringbuff *get_oldest_buffer(void);
void define_buffer(void);
void drop_buffer(void);
void drop_all_buffers(void);
