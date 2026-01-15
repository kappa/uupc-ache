/*-
 * Copyright (C) 1992-1994 by Andrey A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <io.h>
#include <dos.h>
#include <time.h>
#include <stdio.h>
#ifndef __EMX__
#include <sys/locking.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "lib.h"
#include "ssleep.h"

currentfile();
extern char *share;
extern boolean visual_output;

#define OLD_LOCK_TIME (10*60)
#define SLEEP_INTERVAL 10

#ifdef __EMX__
/* locking() and fcntl() not supported :-( */

#define INCL_DOSFILEMGR
#include <os2.h>

#define LK_UNLCK  0     /* unlock file region */
#define LK_LOCK   1     /* lock file region, try for 10 seconds */
#define LK_NBLCK  2     /* lock file region, don't block */
#define LK_RLCK   3     /* same as LK_LOCK */
#define LK_NBRLCK 4     /* same as LK_NBLCK */

int locking(int handle, int mode, long length)
{
	FILELOCK slock, sunlock;
	APIRET r;

	slock.lOffset = sunlock.lOffset = 0;
	if (mode == LK_UNLCK) {
		sunlock.lRange = length;
		slock.lRange = 0;
	} else {
		slock.lRange = length;
		sunlock.lRange = 0;
	}
	r = DosSetFileLocks(handle, &sunlock, &slock, 0,
	             ((mode == LK_NBLCK) || (mode == LK_NBRLCK)) ? 0 : 10000);
	if (r) errno = EACCES;
	return r;
}
#endif

void
fd_lock(const char *file, int fd)
{
	int trycnt;

	if (share == NULL)
		return;
	if (fd < 0 || file == NULL) {
		printmsg(0, "fd_lock: arg error, file %s, fd %d", file, fd);
		panic();
	}
	trycnt = 0;
	for (;;) {
#ifdef __OS2__
		if (locking(fd, LK_NBLCK, 0x7fffffffL) < 0) {
#else
		if (locking(fd, LK_NBLCK, 0xffffffffL) < 0) {
#endif
			if (errno == EINVAL) {
				printmsg(0, "fd_lock: share locking not installed");
				free(share);
				share = NULL;
				return;
			}
		} else {
			printmsg(6, "fd_lock: %s locked", file);
			return;
		}
		printmsg(6,"fd_lock: errno=%d", errno);
		if (trycnt++ < OLD_LOCK_TIME/SLEEP_INTERVAL) {
			if (!visual_output) {
				printmsg(0, "fd_lock: attempt %d, %s busy, wait %ds, Esc to abort",
							trycnt, file, SLEEP_INTERVAL);
				if (!ssleep(SLEEP_INTERVAL))
					continue;
			}
			else {
				extern char *viscont;
				extern int vissleep;
				extern boolean visret;
				char *vc = viscont;
				int vs = vissleep;

				viscont = "abort";
				vissleep = SLEEP_INTERVAL;
				printmsg(0, "fd_lock: attempt %d, %s busy",
							trycnt, file);
				viscont = vc;
				vissleep = vs;
				if (!visret)
					continue;
			}
		}
		printmsg(0, "fd_lock: %s busy; can't release lock", file);
		panic();
	}
}
