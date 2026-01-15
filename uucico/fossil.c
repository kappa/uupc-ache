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

#include <stdio.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <alloc.h>
#include <string.h>
#include <dos.h>

#include "lib.h"
#include "hlib.h"
#include "ulib.h"
#include "fossil.h"
#include "comm.h"
#include "mnp.h"
#include "ssleep.h"
#include "pushpop.h"

currentfile();

#define FOSSIL_NONE (-1)
#define FOSSIL_UNKNOWN 0
#define FOSSIL_BNU 1
#define FOSSIL_MX5 2
#define FOSSIL_X00 3

extern void Sinfo(char *s);
void setflow(boolean);

static unsigned fs_maxfn = 0;
boolean fs_direct;

static boolean buggy_avail = FALSE;

unsigned fs_port;
unsigned info_DX, info_CX;
int fs_type;
long fs_baud;
boolean fs_xon = FALSE;
unsigned char fs_tim_int;
unsigned char fs_tics_per_sec;
unsigned fs_ms_per_tic;
boolean connected = FALSE;
static time_t waittime = 0;
boolean NoCheckCarrier = FALSE;
#if 0
static boolean fs_install_timer = FALSE;
#endif
/* volatile static unsigned fs_tics; */
int fs_isize, fs_osize;
int fs_status;
boolean use_old_status = FALSE;
char far *_intibuf = NULL, far *_intobuf = NULL;
boolean com_saved = FALSE;
boolean com_opened = FALSE;

extern boolean fossil, wattcp;
extern int MnpEmulation;
extern boolean Front;
extern char *calldir;
void fossil_exit(void);

void
select_port(int port)
{
	fs_port = port - 1;
	_select_port(port);
	save_com();
	com_saved = TRUE;
}


int
status(void)
{
	if (use_old_status)
		return fs_status;
	_AH = FS_GET_STATUS;
	_DX = fs_port;
	geninterrupt(FOSSIL);
	return (fs_status = _AX);
}

boolean
far cdecl
carrier(boolean immediately)
{
	boolean CD;

	/* This hack needed only for internal driver */
	if (!immediately && NoCheckCarrier || fs_direct)
		return TRUE;

	CD = fossil ? !!(status() & FS_DCD) : _carrier();

	if (immediately)
		return CD;

	if (!CD) {
		if (waittime == 0)
			time(&waittime);
		else if (time(NULL) > waittime + 1) /* Wait 1 sec */
			return FALSE;
	}
	else
		waittime = 0;

	return TRUE;
}


int
transmit_char(char c)
{
	_AH = FS_TRANSMIT_CHAR;
	_AL = c;
	_DX = fs_port;
	geninterrupt(FOSSIL);
	return (fs_status = _AX);
}

int
receive_char(void)
{
	_AH = FS_RECEIVE_CHAR;
	_DX = fs_port;
	geninterrupt(FOSSIL);
	return _AX;
}

int
peek_ahead(void)
{
	_AH = FS_PEEK_AHEAD;
	_DX = fs_port;
	geninterrupt(FOSSIL);
	return _AX;
}


boolean
set_connected(boolean need)
{
	int l;

	if (!need) {
		connected = FALSE;
		if (!fossil && !wattcp)
			_set_connected(FALSE);
		return TRUE;
	}

	if (!fs_direct) {
		if (!Front)	/* Carrier already present */
			ssleep(1);
		connected = TRUE;
		if (!NoCheckCarrier) {
			waittime = 0;
			if (!fossil) /* Do this before carrier() */
				_set_connected(TRUE);
			if (!carrier(TRUE)) {	/* Already sleep above */
				connected = FALSE;
				return FALSE;
			}
		}
		if (!fossil)
			return TRUE;
		if (!Front && mx5_present() && (l = get_mnp_level()) > 0) {
			l = mnp_active() ? mnp_level() : 0;
			if (l)
				printmsg(-1, "set_connected: got MNP%d level", l);
			else
				printmsg(0, "set_connected: can't connect with MNP");
		}
	}
	else {
		connected = FALSE;
		printmsg(4, "set_connected: DIR at %ld baud (port)", fs_baud);
	}
	return TRUE;
}


#if 0
void
timer_params(void)
{
	union REGS inregs, outregs;

	inregs.h.ah = FS_TIMER_PARAMS;
	int86(FOSSIL, &inregs, &outregs);
	fs_tim_int = outregs.h.al;
	fs_tics_per_sec = outregs.h.ah;
	fs_ms_per_tic = outregs.x.dx;
	printmsg(19, "timer_params: int %d, tics/sec %u, ms/tic %u",
		fs_tim_int, fs_tics_per_sec, fs_ms_per_tic);
}

int
timer_function(void (interrupt *ff)(void), int ins)
{
	union REGS r;
	struct SREGS segregs;

	r.h.ah = FS_TIMER_FUNC;
	r.h.al = !!ins;
	segregs.es = FP_SEG(ff);
	r.x.dx = FP_OFF(ff);
	return int86x(FOSSIL, &r, &r, &segregs);
}

static
void
interrupt
tick_count(void)
{
	fs_tics--;
}

void
fossil_delay(unsigned tics)
{
	if (tics == 0)
		return;

	if (mx5_present()) {
		wait_tics(tics);
		return;
	}

	if (!fs_install_timer) {
		(void) ddelay(tics * fs_ms_per_tic);
		return;
	}

	fs_tics = tics;
	while (fs_tics > 0)
		;
}
#endif

static
struct fs_info far *
get_info(void)
{
	static struct fs_info far info;

	_DX = fs_port;
	_ES = FP_SEG(&info);
	_AH = FS_GET_INFO;
	_DI = FP_OFF(&info);
	_CX = sizeof(info) - 3;  /* Hack HERE!!! last 3 not needed */
	geninterrupt(FOSSIL);
	info_CX = _CX;
	info_DX = _DX;
	return &info;
}

static
boolean
fossil_present(void)
{
	union REGS inregs, outregs;
	struct fs_info far * info;
	char buf[256];
	static int done = -1;
	char *s;

	if (done != -1)
		return done;

	done = FALSE;
	fs_type = FOSSIL_NONE;

	inregs.h.ah = FS_INIT_DRIVER;
	inregs.x.dx = fs_port;
	inregs.x.bx = 0;
	outregs.x.ax = 0;	/* Make shure */
	int86(FOSSIL, &inregs, &outregs);

	if (outregs.x.ax != FS_SIGNATURE) {
		sprintf(buf, "Can't connect to FOSSIL, use internal driver (COM%d)", fs_port + 1);
		Sinfo(buf);
		printmsg(1, "fossil_present: %s", buf);
		return done;
	}

	fs_maxfn = outregs.h.bl;
	if (fs_maxfn < FS_MAXFN) {
		printmsg(0, "fossil_present: Maximum Function=%X too small", fs_maxfn);
		return done;
	}

	fs_type = FOSSIL_UNKNOWN;

	inregs.h.ah = BNU_CHECK;
	inregs.x.dx = FS_SIGNATURE;
	outregs.x.ax = 0;	/* Make shure */
	int86(BNU_INSTALL, &inregs, &outregs);
	if (outregs.x.ax == FS_SIGNATURE)
		fs_type = FOSSIL_BNU;

	if (mx5_present()) {
		fs_type = FOSSIL_MX5;
		set_mnp_level(0);
	}
/*******************
	else {
	   if (timer_function(tick_count, TRUE) != 0)
		   printmsg(0, "fossil_present: can't install tick_count");
	   else
		   fs_install_timer = TRUE;
	}
*******************/

	info = get_info();
	fs_isize = info->ibufr;
	fs_osize = info->obufr;
	if (info_CX == 'X0' && info_DX == '0 ')
		fs_type = FOSSIL_X00;

	if (info->oavail == 0) {
		buggy_avail = TRUE;
		printmsg(-1, "fossil_present: your FOSSIL has status bug: avail size == 0 [CORRECTED]");
	}
	else
		fs_osize = info->oavail;
	atexit(fossil_exit);

	switch (fs_type) {
	case FOSSIL_UNKNOWN:
		s = "Unknown";
		break;
	case FOSSIL_X00:
		s = "X00";
		break;
	case FOSSIL_BNU:
		s = "BNU";
		break;
	case FOSSIL_MX5:
		s = "MX5";
		break;
	}

	sprintf(buf, "FOSSILv%u.%u, Type: %s, %.80Fs",
				 info->majver, info->minver, s, info->ident);
	buf[80] = '\0';
	if ((s = strchr(buf, '\n')) != NULL)
		*s = '\0';
	printmsg(2, "fossil_present: %s", buf);
	Sinfo(buf);

	printmsg(2, "fossil_present: input buffer: %d chars, output buffer: %d chars",
				fs_isize, fs_osize);
#if 0
	timer_params();
#endif
	done = TRUE;

	return done;
}

void
fossil_exit(void)
{
	if (!fossil) {
		if (_intibuf != NULL) {
			farfree(_intibuf);
			farfree(_intobuf);
		}
		return;
	}
#if 0
	if (fs_install_timer) {
		fs_install_timer = FALSE;
		if (timer_function(tick_count, FALSE) != 0)
			printmsg(0, "fossil_exit: can't delete tick_count");
	}
#endif
}

boolean
install_com(void)
{
   static boolean first = TRUE;
   
   fossil = fossil_present();
   if (fossil)
	return fossil;
   if (first) {
	first = FALSE;
	fs_isize = r_count_size();
	fs_osize = s_count_size();
	printmsg(2, "install_com: internal input buffer: %d chars, output buffer: %d chars",
			fs_isize, fs_osize);
	if (   (_intibuf = farmalloc(fs_isize)) == NULL
	    || (_intobuf = farmalloc(fs_osize)) == NULL
	   ) {
		if (_intibuf != NULL) {
			farfree(_intibuf);
			_intibuf = NULL;
		}
		printmsg(0, "install_com: no memory for internal buffers");
		return FALSE;
	}
	_install_buffers(_intibuf, _intobuf);
   }
   if (!_install_com())
	return FALSE;
   return TRUE;
}

void
open_com(long baud, char modem, char parity, int stopbits, char xon)
{
	union REGS r;
	unsigned mask;
	boolean use_old_speed;

	printmsg(15, "open_com(%ld, '%c', '%c', %d, '%c') called",
		 baud, modem, parity, stopbits, xon);

	fs_direct = (modem != 'M');
	fs_baud = baud;

	if (baud > 115200 || baud < 300) {
		printmsg(0, "open_com: baud value out of range 300..115200");
		panic();
	}

	mask = 0;
	use_old_speed = TRUE;

	switch (baud) {
	case 115200:
		if (!fossil)
			baud = 0;	/* 0 == 115200 */
		/* fallthrough */
	case 57600:
	case 28800:
		if (!fossil)
			break;
		if (fs_maxfn < FS_X00_SET_BAUD) {
			printmsg(0, "open_com: %ld baud is impossible for this FOSSIL driver", baud);
			panic();
		}
		use_old_speed = FALSE;
		break;
	case 38400:
		mask |= FS_38400;
		break;
	case 19200:
		mask |= FS_19200;
		break;
	case 9600:
		mask |= FS_9600;
		break;
	case 4800:
		mask |= FS_4800;
		break;
	case 2400:
		mask |= FS_2400;
		break;
	case 1200:
		mask |= FS_1200;
		break;
	case 600:
		mask |= FS_600;
		break;
	case 300:
		mask |= FS_300;
		break;
	default:
		printmsg(0, "open_com: baud value out of possible port baud set");
		panic();
	}

	if (!fossil) {
		_open_com((unsigned)baud, modem, parity, stopbits, xon);
		com_opened = TRUE;
		if (!Front)
			dtr_on();
		return;
	}

	r.h.ah = FS_INIT_DRIVER;
	r.x.dx = fs_port;
	r.x.bx = 0;
	int86(FOSSIL, &r, &r);

	if (use_old_speed) {
		switch(parity) {
		default:
		case 'N':
			mask |= FS_NOPAR;
			break;
		case 'O':
			mask |= FS_ODD;
			break;
		case 'E':
			mask |= FS_EVEN;
			break;
		}
		switch (stopbits) {
		default:
		case 1:
			mask |= FS_ONESTOP;
			break;
		case 2:
			mask |= FS_TWOSTOP;
			break;
		}
		mask |= FS_CHAR8;
		r.h.ah = FS_SET_BAUD;
		r.h.al = mask;
	} else {	/* new speed */
		r.h.ah = FS_X00_SET_BAUD;
		r.h.al = FS_STOP_BREAK;
		switch(parity) {
		default:
		case 'N':
			r.h.bh = FS_X00_NOPAR;
			break;
		case 'O':
			r.h.bh = FS_X00_ODD;
			break;
		case 'E':
			r.h.bh = FS_X00_EVEN;
			break;
		}
		switch (stopbits) {
		default:
		case 1:
			r.h.bl = FS_X00_ONESTOP;
			break;
		case 2:
			r.h.bl = FS_X00_TWOSTOP;
			break;
		}
		r.h.ch = FS_CHAR8;
		switch (baud) {
		case 115200:
			r.h.cl = FS_X00_115200;
			break;
		case 57600:
			r.h.cl = FS_X00_57600;
			break;
		case 28800:
			r.h.cl = FS_X00_28800;
			break;
		}
	}
	r.x.dx = fs_port;
	int86(FOSSIL, &r, &r);

	switch (xon) {
	default:
	case 'D':
		fs_xon = FALSE;
		break;
	case 'E':
		fs_xon = TRUE;
		break;
	}

	setflow(fs_xon);
}

void
close_com(void)
{
	connected = FALSE;

	if (!fossil) {
		_close_com();
		com_opened = FALSE;
		return;
	}
	if (mx5_present())
		set_mnp_level(0);

	_AH = FS_DEINIT_DRIVER;
	_DX = fs_port;
	geninterrupt(FOSSIL);
}


void
dtr_on()
{
	if (fs_direct) return;

	if (!fossil) {
		_dtr_on();
		if (fs_direct != (_get_conn_type() == 'D')) {
			printmsg(0, "dtr_on: can't set dtr on COM%d", fs_port+1);
			panic();
		}
		return;
	}

	_AH = FS_DTR_CHANGE;
	_AL = FS_RAISE_DTR;
	_DX = fs_port;
	geninterrupt(FOSSIL);
}

void
dtr_off()
{
	connected = FALSE;

	w_purge();

	if (fs_direct) return;

	if (!fossil)
		_dtr_off();
	else {
		if (mx5_present())
			set_mnp_level(0);

		_AH = FS_DTR_CHANGE;
		_AL = FS_LOWER_DTR;
		_DX = fs_port;
		geninterrupt(FOSSIL);
	}
}

void
break_com(int length)
{
	w_flush();

	if (fs_direct)
		return;

	if (!fossil) {
		_break_com();
		return;
	}

	_AH = FS_SEND_BREAK;
	_AL = FS_START_BREAK;
	_DX = fs_port;
	geninterrupt(FOSSIL);

	ddelay(length);

	_AH = FS_SEND_BREAK;
	_AL = FS_STOP_BREAK;
	_DX = fs_port;
	geninterrupt(FOSSIL);
}

void
s_r_purge(void)
{
	if (fossil) {
		_AH = FS_PURGE_INPUT;
		_DX = fs_port;
		geninterrupt(FOSSIL);
	}
	else
		_r_purge();
}

void
s_w_purge(void)
{
	if (fossil) {
		_AH = FS_PURGE_OUTPUT;
		_DX = fs_port;
		geninterrupt(FOSSIL);
	}
	else
		_w_purge();
}


int
not_empty_out(void)
{
	int s = status();
	int c;

	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	if (s & FS_TSRE)
		return 0;
	return 1;
}


int
s_count_pending(void)
{
	int r;

	if (!fossil)
		r = !connected || carrier(FALSE) ?
		    fs_osize - s_count_free() : -1;
	else {
		r = not_empty_out();
		if (r > 0)
			r = buggy_avail ? get_info()->oavail : fs_osize - get_info()->oavail;
	}
	return r;
}

int
s_w_flush(void)
{
	int i, old_i, spin;
	long limit;
	struct timeb start;

	spin = 0;
	limit = 0;
	for (;;) {
		i = s_count_pending();
		if (i < 0)
			return i;
		if (i == 0)
			return fs_osize;
		if (limit == 0) {
			old_i = i;
			/* Compute time in milliseconds
			   assuming 10 bits per byte           */
			limit = (i * (long)10000) / 1200 + 1;
			ftime(&start);
		}
		if (   msecs_from(&start) > limit
		    || sdelay(0)
		   ) {
			if (connected) {
				if (!NoCheckCarrier && !carrier(FALSE))
					return -1;
				if (spin < MAXSPIN) {
					if (   old_i == i  /* No progress */
					    || msecs_from(&start) <= limit /* Forced timeout */
					   ) {
						spin++;
						printmsg(4, "s_w_flush: try to release flow control");
						flowcontrol(fs_xon);
					}
					limit = 0;
					continue;
				}
			}
			printmsg(0,"s_w_flush: port write buffer overflow");
			return 0;
		}
	}
}


int
r_count_pending(void)
{
	int s, r, c;

	if (!fossil)
		r = !connected || carrier(FALSE) ? _r_count_pending() : -1;
	else {
		s = status();
		if (connected && !NoCheckCarrier) {
			use_old_status = TRUE;
			c = carrier(FALSE);
			use_old_status = FALSE;
			if (!c)
				r = -1;
			else
				goto Next;
		}
		else
	Next:
		if (s & FS_OVRN)
			printmsg(0, "r_count_pending: internal FOSSIL input buffer (%d bytes) overrun, increase it!", fs_isize);
		if (!(s & FS_RDA))
			r = 0;
		else if (buggy_avail)
			r = get_info()->iavail;
		else
			r = fs_isize - get_info()->iavail;
	}

	return r;
}

int
r_1_pending(void)
{
	int s, c;

	if (!fossil) {
		if (connected && !carrier(FALSE))
			return -1;
		return _r_count_pending() != 0;
	}
	s = status();
	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	if (s & FS_OVRN)
		printmsg(0, "r_1_pending: internal FOSSIL input buffer (%d bytes) overrun, increase it!", fs_isize);
	return !!(s & FS_RDA);
}


int
s_count_free(void)
{
	int s, r, c;

	if (!fossil)
		r = !connected || carrier(FALSE) ? _s_count_free() : -1;
	else {
		s = status();
		if (connected && !NoCheckCarrier) {
			use_old_status = TRUE;
			c = carrier(FALSE);
			use_old_status = FALSE;
			if (!c)
				r = -1;
			else
				goto Next;
		}
		else
	Next:
		if (s & FS_TSRE)
			r = fs_osize;
		else if (buggy_avail)
			r = fs_osize - get_info()->oavail;
		else
			r = get_info()->oavail;
	}

	return r;
}

int
s_1_free(void)
{
	int s, c;

	if (!fossil) {
		if (connected && !carrier(FALSE))
			return -1;
		return _s_count_free() != 0;
	}

	s = status();
	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	return !!(s & FS_THRE);
}


unsigned
read_block(unsigned wanted, char *buf)
{
	_DX = fs_port;
	_CX = wanted;
	_ES = FP_SEG(buf);
	_DI = FP_OFF(buf);
	_AH = FS_READ_BLOCK;
	geninterrupt(FOSSIL);
	return _AX;
}

unsigned
write_block(unsigned wanted, char *buf)
{
	_DX = fs_port;
	_CX = wanted;
	_ES = FP_SEG(buf);
	_DI = FP_OFF(buf);
	_AH = FS_WRITE_BLOCK;
	geninterrupt(FOSSIL);
	return _AX;
}

void
setflow(boolean xon)
{
	int flow;

	if (!fossil)
		return;

	flow = (xon ? (FS_RECEIVE_XON|FS_TRANSMIT_XON) : FS_CTS_RTS) | 0xF4;

	_AH = FS_FLOW_CONTROL;
	_AL = flow;
	_DX = fs_port;
	geninterrupt(FOSSIL);

	_AH = FS_CHK_TRANSMIT;
	_AL = 0;	/* Enable transmitter */
	_DX = fs_port;
	geninterrupt(FOSSIL);
}