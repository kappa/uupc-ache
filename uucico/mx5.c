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

#include <dos.h>
#include <stdio.h>
#include "lib.h"
#include "fossil.h"
#include "mnp.h"
#include "mx5.h"

int Mx5done = -1;
int	MnpEmulation;
int	MnpWaitTics;

boolean
mnp_present(void)
{
	union REGS inregs, outregs;

	if (Mx5done != -1)
		return Mx5done;

	Mx5done = FALSE;
	inregs.h.ah = FS_MX5;
	inregs.h.al = MX_VERSION;
	inregs.x.bx = 0;
	int86(FOSSIL, &inregs, &outregs);
	Mx5done = (outregs.x.bx == MX_SIGNATURE);

	printmsg(2, "mx5_present: %d", Mx5done);

	return Mx5done;
}

void
set_mnp_level(int level)
{
    union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
	inregs.h.al = MX_MNP_LEVEL;
    inregs.h.bl = level;
    inregs.h.bh = MX_SET;
    inregs.x.dx = fs_port;
    int86(FOSSIL, &inregs, &outregs);
}

int
get_mnp_level(void)
{
	union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
    inregs.h.al = MX_MNP_LEVEL;
    inregs.h.bh = MX_GET;
    inregs.x.dx = fs_port;
    int86(FOSSIL, &inregs, &outregs);

    return outregs.h.bl;
}

void
set_mnp_wait_tics(int tics)
{
    union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
    inregs.h.al = MX_WAIT_TICS;
    inregs.h.bl = tics;
    inregs.h.bh = MX_SET;
    inregs.x.dx = fs_port;
    int86(FOSSIL, &inregs, &outregs);
}

int
get_mnp_wait_tics(void)
{
    union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
	inregs.h.al = MX_WAIT_TICS;
    inregs.h.bh = MX_GET;
    inregs.x.dx = fs_port;
    int86(FOSSIL, &inregs, &outregs);

    return outregs.h.bl;
}

static
struct mxinfo *
mx_status(void)
{
	union REGS inregs, outregs;
	struct SREGS segregs;
	struct mxinfo *info;

	inregs.h.ah = FS_MX5;
	inregs.h.al = MX_STATUS;
	inregs.x.dx = fs_port;
	int86x(FOSSIL, &inregs, &outregs, &segregs);
	FP_SEG(info) = segregs.es;
	FP_OFF(info) = outregs.x.bx;

	return info;
}


boolean
mnp_active(void)
{
	return mx_status()->active;
}

int
mnp_level(void)
{
    return mx_status()->level;
}

void
wait_tics(unsigned tics)
{
    union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
	inregs.h.al = MX_WAIT;
    inregs.x.cx = tics;
    int86(FOSSIL, &inregs, &outregs);
}

void
set_answer_mode(boolean mode)
{
	union REGS inregs, outregs;

	if (!mx5_present())
		return;
	inregs.h.ah = FS_MX5;
	inregs.h.al = MX_MODE;
	inregs.h.bh = MX_SET;
	inregs.h.bl = mode;
	inregs.x.dx = fs_port;
	int86(FOSSIL, &inregs, &outregs);
}

void
set_sound(boolean mode)
{
	union REGS inregs, outregs;

    inregs.h.ah = FS_MX5;
	inregs.h.al = MX_SOUND;
	inregs.h.bh = MX_SET;
	inregs.h.bl = mode;
	inregs.x.dx = fs_port;
	int86(FOSSIL, &inregs, &outregs);
}

/*********************************************************
void
remove_mx5(void)
{
	union REGS inregs, outregs;

	inregs.h.ah = FS_MX5;
	inregs.h.al = MX_REMOVE;
	outregs.x.bx = 0;
	int86(FOSSIL, &inregs, &outregs);
	if (outregs.x.bx == 0) {
Cant:
		printmsg(0, "remove_mx5: can't uninstall MNP from memory");
		return;
	}
	if (freemem(outregs.x.bx) != 0)
		goto Cant;
}
*************************************************************/

void MnpSupport(void)
{
   if (MnpEmulation && mx5_present()) {
		printmsg(2, "MnpSupport: try mnp emulation level %d", MnpEmulation);
		set_mnp_level(MnpEmulation);
		if (MnpWaitTics != 0) {
			printmsg(4, "MnpSupport: mnp wait tics %d", MnpWaitTics);
			set_mnp_wait_tics(MnpWaitTics);
		}
		set_sound(TRUE);
	}
}
