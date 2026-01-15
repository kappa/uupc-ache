/*
 * Copyright (C) 1996 by Pavel Gulchouck, Kiev, Ukraine.
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
#include <malloc.h>
#include <dos.h>
#include <process.h>
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#ifndef ASYNC_EXTSETBAUDRATE
#define ASYNC_EXTSETBAUDRATE 0x0043
#endif

#include "lib.h"
#include "hlib.h"
#include "ulib.h"
#include "fossil.h"
#include "comm.h"
#include "ssleep.h"
#include "pushpop.h"
#include "dcp.h"
#include "modem.h"

currentfile();

extern void Sinfo(char *s);
void setflow(boolean);

boolean fs_direct;

extern HEV kbdsem;
unsigned fs_port;
long fs_baud;
boolean fs_xon = FALSE;
unsigned char fs_tim_int;
unsigned char fs_tics_per_sec;
unsigned fs_ms_per_tic;
boolean connected = FALSE;
static time_t waittime = 0;
boolean NoCheckCarrier = FALSE;
int fs_isize, fs_osize;
int fs_status;
boolean use_old_status = FALSE;
boolean com_saved = FALSE;
boolean com_opened = FALSE;

unsigned long ParamLen, DataLen;

extern boolean Front;
extern char *calldir;
void fossil_exit(void);

static char * devname[]={"COM1","COM2","COM3","COM4",
			 "COM5","COM6","COM7","COM8"};

void
select_port(int port)
{
#define COMMODE O_BINARY|O_RDWR

	if (H_Flag)
		setmode(fs_port, COMMODE);
	else
		fs_port = open(devname[port-1], COMMODE);
	if (fs_port != (unsigned)-1)
	{	printmsg(12, "Device %s opened, handle %d", devname[port-1], fs_port);
		com_saved = TRUE;
	}
	else
		printmsg(0, "Can't open %s", devname[port-1]);
}


int
status(void)
{

	if (use_old_status)
		return fs_status;
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETMODEMINPUT,
		NULL, 0, &ParamLen,
		&fs_status, sizeof(fs_status), &DataLen);
	return fs_status;
}
		
boolean
carrier(boolean immediately)
{
	boolean CD;

	CD = !!(status() & FS_DCD);

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

int send_com(int c)
{ return transmit_char((char)c);
}

int
transmit_char(char c)
{
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_TRANSMITIMM,
		&c, 1, &ParamLen,
		NULL, 0, &DataLen);
	return status();
}

int receive_com(void)
{ return receive_char();
}

int
receive_char(void)
{	ULONG l;
	int c = 0, r;

	for (;;) {
		r = DosRead(fs_port, &c, 1, &l);
		if (r) {
			printmsg(1, "receive_char: DosRead returns %d", r);
			return -1;
		}
		if (l)
			return c;
		DosSleep(1);
	}
}

int
peek_ahead(void)
{
	RXQUEUE rq;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETINQUECOUNT,
		NULL, 0, &ParamLen,
		&rq, sizeof(rq), &DataLen);
	if (rq.cch == 0)
		return -1;
	/* No way to non-destructive read character :-( */
	return 0;
}


boolean
set_connected(boolean need)
{
	if (!need) {
		connected = FALSE;
		return TRUE;
	}

	if (!fs_direct) {
		if (!Front)	/* Carrier already present */
			ssleep(1);
		connected = TRUE;
		if (!NoCheckCarrier) {
			waittime = 0;
			if (!carrier(TRUE)) {	/* Already sleep above */
				connected = FALSE;
				return FALSE;
			}
		}
	}
	else {
		connected = FALSE;
		printmsg(4, "set_connected: DIR at %ld baud (port)", fs_baud);
	}
	return TRUE;
}

void
fossil_exit(void)
{
	return;
}

boolean
install_com(void)
{
	RXQUEUE tx;
	int r;

	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	if (r) {
		printmsg(1, "install_com: DosDevIOCtl returns %d", r);
		return FALSE;
	}
	fs_osize = tx.cb;
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETINQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	if (r) {
		printmsg(1, "install_com: DosDevIOCtl returns %d", r);
		return FALSE;
	}
	fs_isize = tx.cb;
	return TRUE;
}

void
open_com(long baud, char modem, char parity, int stopbits, char xon)
{
#define SIO_NOPAR	0
#define SIO_ODD		1
#define SIO_EVEN	2
#define SIO_ONESTOP	0
#define SIO_TWOSTOP	2
#define SIO_CHAR8	8

	struct	{ char data, parity, stop; } ctl;
	DCBINFO dcb;
	int  r;

	printmsg(15, "open_com(%ld, '%c', '%c', %d, '%c') called",
		 baud, modem, parity, stopbits, xon);

	fs_direct = (modem != 'M');
	fs_baud = baud;

	if (baud > 115200 || baud < 300) {
		printmsg(0, "open_com: baud value out of range 300..115200");
		panic();
	}

	switch (baud) {
	case 115200:
	case 57600:
	case 28800:
	case 38400:
	case 19200:
	case 9600:
	case 4800:
	case 2400:
	case 1200:
	case 600:
	case 300:
		break;
	default:
		printmsg(0, "open_com: baud value out of possible port baud set");
		panic();
	}

	if (baud == 115200) {
		struct {
			ULONG baud;
			UCHAR fraction;
		} extbaud;

		extbaud.baud = baud;
		extbaud.fraction = 0;
		r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE,
			&extbaud, sizeof(extbaud), &ParamLen,
			NULL, 0, &DataLen);
	} else
		r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETBAUDRATE,
			&baud, 2, &ParamLen,
			NULL, 0, &DataLen);
	if (r) {
		printmsg(1, "open_com: DosDevIOCtl returns %d", r);
		return;
	}

	switch(parity) {
		default:
		case 'N':
			ctl.parity = SIO_NOPAR;
			break;
		case 'O':
			ctl.parity = SIO_ODD;
			break;
		case 'E':
			ctl.parity = SIO_EVEN;
			break;
	}
	switch (stopbits) {
		default:
		case 1:
			ctl.stop = SIO_ONESTOP;
			break;
		case 2:
			ctl.stop = SIO_TWOSTOP;
			break;
	}
	ctl.data = SIO_CHAR8;
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETLINECTRL,
		&ctl, sizeof(ctl), &ParamLen,
		NULL, 0, &DataLen);
	if (r) {
		printmsg(1, "open_com: DosDevIOCtl returns %d", r);
		return;
	}


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
/* set read timeout */
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETDCBINFO,
		NULL, 0, &ParamLen,
		&dcb, sizeof(dcb), &DataLen);
	if (r) {
		printmsg(1, "open_com: DosDevIOCtl returns %d", r);
		return;
	}
	dcb.usReadTimeout = dcb.usWriteTimeout = 10; /* timeout 1 sec */
	dcb.fbTimeout = (dcb.fbTimeout & ~6) | 2; /* Normal read timeout */
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETDCBINFO,
		&dcb, sizeof(dcb), &ParamLen,
		NULL, 0, &DataLen);
	if (r) {
		printmsg(1, "open_com: DosDevIOCtl returns %d", r);
		return;
	}
	dtr_on();
	_rts_on();
	ssleep(1);
}

void
close_com(void)
{
	connected = FALSE;
	close(fs_port);
}

#define SIO_DTR		1
#define SIO_RTS		2

void
dtr_on()
{
	MODEMSTATUS st;
	USHORT comerr;
	int  r;

	if (fs_direct) return;

	st.fbModemOn  = SIO_DTR;
	st.fbModemOff = (UCHAR)-1;
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETMODEMCTRL,
		&st, sizeof(st), &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	if (r) {
		printmsg(1, "dtr_on: DosDevIOCtl returns %d", r);
		return;
	}

}

void
_rts_on()
{
	MODEMSTATUS st;
	USHORT comerr;
	int  r;

	if (fs_direct) return;

	st.fbModemOn  = SIO_RTS;
	st.fbModemOff = (UCHAR)-1;
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETMODEMCTRL,
		&st, sizeof(st), &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	if (r) {
		printmsg(1, "rts_on: DosDevIOCtl returns %d", r);
		return;
	}

}

void
dtr_off()
{
	MODEMSTATUS st;
	USHORT comerr;
	int  r;

	connected = FALSE;

	w_purge();

	if (fs_direct) return;

	st.fbModemOff = (UCHAR)(~SIO_DTR);
	st.fbModemOn  = 0;
	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETMODEMCTRL,
		&st, sizeof(st), &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	if (r) {
		printmsg(1, "dtr_off: DosDevIOCtl returns %d", r);
		return;
	}

}

void
break_com(int length)
{
	USHORT comerr;
	int r;

	w_flush();

	if (fs_direct)
		return;

	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETBREAKON,
		NULL, 0, &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	if (r) {
		printmsg(1, "break_com: DosDevIOCtl returns %d", r);
		return;
	}

	ddelay(length);

	r = DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_SETBREAKOFF,
		NULL, 0, &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	if (r)
		printmsg(1, "break_com: DosDevIOCtl returns %d", r);
}

void
s_r_purge(void)
{
  while (peek_ahead()>=0)
    receive_com();
}

void
s_w_purge(void)
{
}


int
not_empty_out(void)
{
	int c;
	RXQUEUE tx;

	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	if (tx.cch != 0)
		return 1;
	else
		return 0;
}


int
s_count_pending(void)
{
	RXQUEUE tx;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	return tx.cch;
}

int
s_w_flush(void)
{
	int  i, spin;
	long limit, now;
        ULONG semcount;

	spin = 0;
	limit = 0;
	i = s_count_pending();
	printmsg(25, "s_w_flush: %d bytes in buffer", i);
	if (i>0) {
		now = time(NULL);
again:
		limit = now + (i * 10) / 1200 + 1;
		DosResetEventSem(kbdsem,&semcount);
		while (limit > time(NULL)) {
			i=DosWaitEventSem(kbdsem, (i * 4000) / fs_baud + 1);
			if (connected)
				if (!NoCheckCarrier && !carrier(FALSE))
					break;
			if (i == 0) break; /* escape pressed */
			i = s_count_pending();
			if (i == 0)
				break;
		}
		DosResetEventSem(kbdsem,&semcount);
		i = s_count_pending();
		if (i) {
			if (connected) {
				if (!NoCheckCarrier && !carrier(FALSE))
					return -1;
				if (spin < MAXSPIN) {
						spin++;
						printmsg(4, "s_w_flush: try to release flow control");
						flowcontrol(fs_xon);
						i = s_count_pending();
						goto again;
				}
			}
			printmsg(0,"s_w_flush: port write buffer overflow");
			i = 0;
		}
	}
	if (i == 0)
		i = fs_osize;
	return i;
}


int
r_count_pending(void)
{
#define SIO_OVRN	1
	int r, c;
	USHORT comerr;
	RXQUEUE tx;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETCOMMERROR,
		NULL, 0, &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETINQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			r = -1;
		else
			goto Next;
	} else {
Next:
		if (comerr & SIO_OVRN)
			printmsg(0, "r_count_pending: internal input buffer (%d bytes) overrun, increase it!", tx.cb);
		r = tx.cch;
	}

	return r;
}

int
r_1_pending(void)
{
	RXQUEUE tx;
	USHORT comerr;
	int c;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETCOMMERROR,
		NULL, 0, &ParamLen,
		&comerr, sizeof(comerr), &DataLen);
	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETINQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	if (comerr & SIO_OVRN)
		printmsg(0, "r_1_pending: internal input buffer (%d bytes) overrun, increase it!", tx.cb);
	return !!(tx.cch);
}


int
s_count_free(void)
{
	RXQUEUE tx;
	int c, r;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);
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
		r = tx.cb - tx.cch;

	return r;
}

int
s_1_free(void)
{
	RXQUEUE tx;
	int c;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT,
		NULL, 0, &ParamLen,
		&tx, sizeof(tx), &DataLen);

	if (connected && !NoCheckCarrier) {
		use_old_status = TRUE;
		c = carrier(FALSE);
		use_old_status = FALSE;
		if (!c)
			return -1;
	}
	return !!(tx.cb - tx.cch);
}


unsigned
read_block(unsigned wanted, char *buf)
{   ULONG nb;
    int  r;

    printmsg(30, "read_block: wanted=%d, buf=%x", wanted, (unsigned)buf);
    if ((r = DosRead(fs_port, buf, wanted, &nb)) != 0) {
	printmsg(1, "read_block: DosRead returns %d", r);
	return -1;
    }
    if (nb < wanted)
	printmsg(11, "read_block: DosRead read %d bytes, wanted %d", nb, wanted);
    printmsg(30, "read_block: return %d", (unsigned)nb);
    return (unsigned)nb;
}

unsigned
write_block(unsigned wanted, char *buf)
{   ULONG nb, wrote=0;
    int  r;
    
    for (;;)
    { if ((r = DosWrite(fs_port, buf, wanted, &nb)) != 0) {
	printmsg(1, "write_block: DosWrite return %d", r);
        return 0;
      }
      wrote+=nb;
      if (nb == wanted) break;
      printmsg(11, "write_block: DosWrite writes %d bytes, wanted %d", nb, wanted);
      wanted-=nb;
      buf+=nb;
#if 0
      if (!NoCheckCarrier) {
        if (!carrier(TRUE)) {   /* Already sleep above */
          connected = FALSE;
          printmsg(5, "write_block: carrier lost");
          return -1;
        }
      }
#endif
      DosSleep(nb==0 ? 200 : 10);
    }
    return wrote;
}

void
setflow(boolean xon)
{
#define SIO_CTS_RTS	0x40
#define SIO_XON_XOFF	3
	DCBINFO dcb;

	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_GETDCBINFO,
		NULL, 0, &ParamLen,
		&dcb, sizeof(dcb), &DataLen);
	/* Not clean :-( */
	if (xon)
		dcb.fbFlowReplace = (dcb.fbFlowReplace & ~SIO_CTS_RTS) | SIO_XON_XOFF;
	else
		dcb.fbFlowReplace = (dcb.fbFlowReplace & ~SIO_XON_XOFF) | SIO_CTS_RTS;


	DosDevIOCtl(fs_port, IOCTL_ASYNC, ASYNC_STARTTRANSMIT,
		NULL, 0, &ParamLen,
		NULL, 0, &DataLen);
}

void restore_com(void)
{}
