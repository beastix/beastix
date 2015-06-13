#ifndef LIBBSD_SYS_CDEFS_H
#define LIBBSD_SYS_CDEFS_H

#define DFLTPHYS        (64 * 1024)     /* default max raw I/O transfer size */
#define MAXPHYS         (128 * 1024)    /* max raw I/O transfer size */
#define MAXDUMPPGS      (DFLTPHYS/PAGE_SIZE)
#define S_IFWHT  0160000

#endif
