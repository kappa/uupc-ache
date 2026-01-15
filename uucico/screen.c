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

#include <conio.h>
#ifdef __OS2__
#define INCL_SUB
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#include <os2.h>
#include <process.h>
#include <os2conio.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <dos.h>
#include "lib.h"
#include "hlib.h"
#include "hostable.h"
#include "arpadate.h"
#include "ssleep.h"
#include "screen.h"

#ifdef __MSDOS__
/* Next code copied from _video.h (internal TurboC video structure) */

typedef unsigned char uchar;

typedef struct
{
	uchar windowx1;
	uchar windowy1;
	uchar windowx2;
	uchar windowy2;
	uchar attribute;
	uchar normattr;
	uchar currmode;
	uchar screenheight;
	uchar screenwidth;
	uchar graphicsmode;
	uchar snow;
	union {
		char far * p;
		struct { unsigned off,seg; } u;
	} displayptr;
} VIDEOREC;

extern VIDEOREC _video;

/* End of _video.h **************************************************/
#endif

#define SIZE_FMT "%8ld"
#define CPS_FMT "%4s"

#define MAXINF (MAXX - 1)

static MAXY = 25;

struct HostStats remote_stats;
extern char *nodename;
char *brand, *rate, *S_sysspeed;
long bytes; 		/* from dcpxfer.c */
long restart_offs;	/* from dcpxfer.c */
struct timeb start_time;/* from dcpxfer.c */
extern char rmtname[];
int MaxTry = 2;
boolean Run_Flag = TRUE;
#ifdef __MSDOS__
char far * scr_base;
#else
extern HMTX kbdwait;
#endif
int  MAXX=80;

static void Sinitdebugwin(void);
static void Sinittranswin(void);
static void Sinitlinkwin(void);
static void ClearLastErr(void);
static int dist(int, int);

int screen = 1;
static nextcall = 0;
static time_t errtime = 0;

static int modemline = 4;
static int linkline = 5;
static int errline = 17;
static int debline = 20;
static int ftrline = 9;
static int miscline = 17;
static int packline = 19;

static int file1col = 20;
static int perrcol = 1;
static int terrcol = 32;
static int bytescol;
static int cpscol;
static int countcol;
static int arrowcol = 32;
static int nodecol = 14;
static int remotecol = 34;
static int timecol;

static short filcolor = WHITE;
static short hdrcolor = BLUE;
static short attcolor = LIGHTCYAN;
static short bitcolor = YELLOW;
static short copcolor = WHITE;
static short fgbase = BLUE;

static boolean wasnl[NWINS];
static struct rccoord begs[NWINS];
static struct rccoord ends[NWINS];
static struct rccoord curr[NWINS];
static short wcol[NWINS] = {
	LIGHTGRAY,
	LIGHTCYAN,
	LIGHTCYAN
};
static boolean inited[NWINS];
static void (*initfuncs[NWINS])(void) = {
	Sinitdebugwin,
	Sinittranswin,
	Sinitlinkwin,
};

static boolean colors = FALSE;

static void fgcolor(int);
static void bkcolor(int);

static int tail[2] = {0, 0};
static int head[2] = {0, 0};

static void fgcolor(int col)
{
	if (!screen)
		return;
	if (!colors && col != WHITE && col != BLACK)
		col = LIGHTGRAY;
	textcolor(col);
}

static void bkcolor(int col)
{
	if (!screen)
		return;
	if (!colors) {
		if (col < LIGHTGRAY)
			col = BLACK;
		else
			col = LIGHTGRAY;
	}
	textbackground(col);
}

void Sclear (void)
{
	if (!screen)
		return;
	window (1, 1, MAXX, MAXY);
	fgcolor(LIGHTGRAY);
	bkcolor(BLACK);
	clrscr();
	errtime = 0;
}

void Sheader(const char *str, boolean cico)
{
	char *s;
	int len;
	extern boolean remote_debug;

	s = strdup(str);
	len = strlen(s);
	while (len > 0 && s[len - 1] == '\n')
		s[--len] = '\0';
	if (!screen) {
		fputs(s, stderr);
		putc('\n', stderr);
		free(s);
		return;
	}
	window (1, 1, MAXX, !nextcall ? MAXY : 2);
	bkcolor(BLACK);
	clrscr();
	fgcolor(copcolor);
	gotoxy (1, 1);
	if (len > MAXX)
		s[MAXX-1] = '\0';
	cputs(s);
	free(s);
	if (nextcall)
		return;
	if (!remote_debug) {
		if (cico)
			s = "Press Ctrl/C to abort, ESC to stop waiting, +- to change script time";
		else
			s = "Press Ctrl/C to abort program, ESC to stop waiting";
	} else {
		if (cico)
			s = "Press d:new debug level & file echo, t:enable/disable port data log";
		else
			s = "Press d:new debug level & file echo";
	}
	Smisc(s);
}

void Sinfo(const char *str)
{
	int len;
	char *s;

	s = strdup(str);
	len = strlen(s);
	while (len > 0 && s[len - 1] == '\n')
		s[--len] = '\0';
	if (!screen) {
		fputs(s, stderr);
		putc('\n', stderr);
	}
	else {
		window (1, 2, MAXX, 2);
		gotoxy (1,1);
		fgcolor(LIGHTGRAY);
		clrscr();
		if (len > MAXX)
			s[MAXX-1] = '\0';
		cputs (s);
	}
	free(s);
	if (screen)
		ClearLastErr();
}

static struct text_info vc;

void Sinitplace (int next)
{
	boolean first = TRUE;
#ifdef __MSDOS__
	union REGS ir, or;
	struct SREGS is;
	unsigned char vinfo[64];
#endif

	if (!screen) {
		directvideo = FALSE;
		return;
	}
	nextcall = next;
	directvideo = TRUE;
Again:
#ifdef __MSDOS__
	/*
	 * Standart TurboC video-detection routine can't properly
	 * determine number of rows, so use VGA state request
	 */
	ir.h.ah = 0x0F;
	int86x(0x10, &ir, &or, &is);
	if (or.h.al != 3)
		directvideo = FALSE;
	ir.x.ax = 0x1B00;
	ir.x.bx = 0;	/* state */
	is.es = FP_SEG(vinfo);
	ir.x.di = FP_OFF(vinfo);
	or.x.ax = 0;	/* Make shure */
	int86x(0x10, &ir, &or, &is);
	if (or.h.al == 0x1B) {	/* Supported */
		_video.screenheight = vinfo[0x22];
		_video.screenwidth = vinfo[0x05];
		_video.windowx2 = _video.screenwidth - 1;
		_video.windowy2 = _video.screenheight - 1;
	}
	/* Now gettextinfo works properly */
	scr_base = MK_FP(0xB800,0);
#endif
	gettextinfo( &vc );
	MAXX = vc.screenwidth;
	MAXY = vc.screenheight;
	if (MAXX < 80 || MAXY < 25) {
		if (!first) {
			screen = 0;
			printmsg(0,"Sinitplace: can't switch to CO80 mode");
			return;
		}
		first = FALSE;
		textmode(C80);
		goto Again;
	}
	bytescol = MAXX-20;
	cpscol = MAXX-13;
	countcol = MAXX-7;
	timecol = MAXX-20;
	colors =	vc.currmode != BW40
			 && vc.currmode != BW80
			 && vc.currmode != MONO
		   ;

	_setcursortype(_NOCURSOR);
	_wscroll = 0;

	if (debuglevel == 0) {
		errline = miscline = MAXY - 3;
		packline = errline + 2;
	}
	if (next) {
		errline++;
		miscline++;
	}
	else
		Sclear();
}

void Srestoreplace (void)
{
	if (!screen) return;

	_setcursortype(_NORMALCURSOR);
	textattr(vc.normattr);
	window (vc.winleft, vc.winbottom - 1, vc.winright, vc.winbottom);
	clrscr();
	window (vc.winleft, vc.wintop, vc.winright, vc.winbottom);
	gotoxy (1, vc.winbottom - 2);
	printmsg(20,"Srestoreplace success");
}

static void Sinittranswin(void)
{
	if (!screen)
		return;
	begs[WTRANS].row = ftrline + 1; begs[WTRANS].col = 1;
	ends[WTRANS].row = errline - 2 - nextcall; ends[WTRANS].col = MAXX;
	fgcolor(colors ? LIGHTCYAN : BLACK);
	bkcolor(colors ? hdrcolor : LIGHTGRAY);
	window (begs[WTRANS].col, begs[WTRANS].row - 1,
			ends[WTRANS].col, begs[WTRANS].row - 1);
	clrscr();
	gotoxy (1,1);
	cputs ("TRANSFER");
	gotoxy (arrowcol, 1);
	cputs ("FILE");
	gotoxy (cpscol, 1);
	cputs (" CPS");
	gotoxy (countcol, 1);
	cputs ("   BYTES");
	bkcolor(BLACK);
}

static void Sinitdebugwin (void)
{
	if (!screen)
		return;
	begs[WDEBUG].row = debline + 1; begs[WDEBUG].col = 1;
	ends[WDEBUG].row = MAXY; ends[WDEBUG].col = MAXX;
	fgcolor(colors ? LIGHTCYAN : BLACK);
	bkcolor(colors ? hdrcolor : LIGHTGRAY);
	window (begs[WDEBUG].col, begs[WDEBUG].row - 1,
			ends[WDEBUG].col, begs[WDEBUG].row - 1);
	clrscr();
	gotoxy (1,1);
	cputs ("DEBUG WINDOW");
	bkcolor(BLACK);
}

static void Sinitlinkwin(void)
{
	if (!screen)
		return;
	begs[WLINK].row = linkline + 1; begs[WLINK].col = 1;
	ends[WLINK].row = linkline + 3; ends[WLINK].col = MAXX;
	fgcolor(colors ? LIGHTCYAN : BLACK);
	bkcolor(colors ? hdrcolor : LIGHTGRAY);
	window (begs[WLINK].col, begs[WLINK].row - 1,
			ends[WLINK].col, begs[WLINK].row - 1);
	clrscr();
	gotoxy (1,1);
	cputs ("LINK");
	gotoxy (nodecol, 1);
	cputs ("NODE");
	gotoxy (remotecol, 1);
	cputs ("REMOTE");
	gotoxy (timecol, 1);
	cputs ("TIME");
	bkcolor(BLACK);
}

void Sundo(void) {

	if (!screen) return;

	window (1, modemline, MAXX, modemline);
	clrscr();
	window (1, errline - 1 - nextcall, bytescol - 1, errline - 1 - nextcall);
	clrscr();
	window (1, packline - 1, MAXX, packline);
	clrscr();
	tail[0] = tail[1] = head[0] = head[1] = 0;

	ClearLastErr();
}

void Slink(Slmesg master, const char *rmtname)
{
	char *s = NULL, *stime;
	char buf[50];
	static int link_count = 0;

	stime = arpadate() + 5;
	switch (master) {
	case SL_CONNECTED:
		if (!screen)
			(void) fprintf(stderr, "%d: (%s) %s connected to host %s at %s\n",
						   link_count, S_sysspeed, nodename, rmtname, stime);
		else {
			s = nodename;
			sprintf(buf, "\r\f%d %s", link_count, S_sysspeed);
			Swputs(WLINK, buf);
		}
		break;
	case SL_CALLEDBY:
		link_count++;
		if (!screen)
			(void) fprintf(stderr, "%d: %s called by %s at %s\n",
						   link_count, nodename, rmtname, stime);
		else {
			s = nodename;
			sprintf(buf, "\n\f%d called", link_count);
			Swputs(WLINK, buf);
		}
		break;
	case SL_CALLING:
		link_count++;
		if (!screen)
			(void) fprintf(stderr, "%d: calling host %s via %s at %s\n",
						   link_count, rmtname, brand, stime);
		else {
			s = brand;
			sprintf(buf, "\n\f%d call via", link_count);
			Swputs(WLINK, buf);
		}
		break;
	case SL_FAILED:
		if (!screen)
			(void) fprintf(stderr, "%d: call host %s via %s at %s failed\n",
						   link_count, rmtname, brand, stime);
		else {
			s = brand;
			Swputs(WLINK, "\r\f");
			fgcolor(RED);
			sprintf(buf, "%d fail via", link_count);
			cputs(buf);
		}
		break;
	}
	if (screen) {
		fgcolor(WHITE);
		gotoxy (nodecol, curr[WLINK].row);
		cputs (s);
		gotoxy (remotecol, curr[WLINK].row);
		cputs (rmtname);
		fgcolor(wcol[WLINK]);
		gotoxy (timecol, curr[WLINK].row);
		cprintf("%.20s", stime);
		curr[WLINK].row = wherey();
		curr[WLINK].col = wherex();
		ClearLastErr();
	}
}

void Smodem(Smdmesg answer, int seq, int kind, int pass)
{
	char buf[10], *str;

	if (screen) {
		window (1, modemline, MAXX, modemline);
		clrscr();
		fgcolor(LIGHTCYAN);
		bkcolor(hdrcolor);
		gotoxy (1,1);
		cputs ("MODEM:");
		bkcolor(BLACK);
		gotoxy (nodecol, 1);
	}
	else
		fprintf(stderr, "Modem:\t");
	switch (kind) {
		case 0:
			str = "COMMON";
			break;
		case 1:
			str = "RING";
			break;
		case 2:
			str = "ANSWER";
			break;
		case 3:
			str = "DIAL";
			break;
		default:
			str = "UNKNOWN";
			break;
	}
	switch (answer) {
		case SM_CONNECT:
			if (!screen)
				(void) fprintf(stderr, "%s: connection established at %s baud\n", str, rate);
			else {
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("CONNECT ");
				fgcolor(attcolor);
				cputs (" at ");
				fgcolor(bitcolor);
				cputs(rate);
				fgcolor(WHITE);
				cputs (" baud.");
			}
			break;
		case SM_BUSY:
			if (!screen)
				(void) fprintf(stderr, "Pass #%d/%d. %s: line busy, redialing\n",
									   pass, MaxTry, str);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": line ");
				fgcolor(filcolor);
				cputs ("BUSY");
				fgcolor(attcolor);
				cputs (", redialing.");
			}
			break;
		case SM_NOREPLY:
			if (!screen)
				(void) fprintf(stderr, "Pass #%d/%d. %s: no reply, aborting\n",
										  pass, MaxTry, str);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("NO REPLY");
				fgcolor(attcolor);
				cputs (", aborting.");
			}
			break;
		case SM_NOCARRY:
			if (!screen)
				(void) fprintf(stderr, "Pass #%d/%d. %s: no carrier, redialing\n",
									  pass, MaxTry, str);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("NO CARRIER");
				fgcolor(attcolor);
				cputs (", redialing.");
			}
			break;
		case SM_NOTONE:
			if (!screen)
				(void) fprintf(stderr,
						   "Pass #%d/%d. %s: no dialtone. Check phone cable.\n",
						pass, MaxTry, str);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("NO DIALTONE");
				fgcolor(attcolor);
				cputs (". Check phone cable.");
			}
			break;
		case SM_NOPOWER:
			if (!screen)
				fprintf(stderr, "Pass #%d/%d. %s: no reply from modem. Check power and/or cable.\n",
								pass, MaxTry, str);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("NO REPLY");
				fgcolor(attcolor);
				cputs (". Check power and/or cable.");
			}
			break;
		case SM_INIT:
			if (!screen)
				fprintf(stderr, "Pass #%d/%d. %s seq. #%d at %s baud\n",
								pass, MaxTry, str, seq, rate);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (" seq. #");
				fgcolor(bitcolor);
				cputs(itoa(seq, buf, 10));
				fgcolor(attcolor);
				cputs (" at ");
				fgcolor(bitcolor);
				cputs(rate);
				fgcolor(attcolor);
				cputs (" baud");
			}
			break;
		case SM_NOBAUD:
			if (!screen)
				(void) fprintf(stderr, "Pass #%d/%d. %s: no reply at %s baud\n",
										pass, MaxTry, str, rate);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (": ");
				fgcolor(filcolor);
				cputs ("NO REPLY");
				fgcolor(attcolor);
				cputs (" at ");
				fgcolor(bitcolor);
				cputs (rate);
				fgcolor(attcolor);
				cputs (" baud");
			}
			break;
		case SM_DIALING:
			if (!screen)
				(void)fprintf(stderr,"Pass #%d/%d. %s number %s, wait for %d sec.\n",
								pass, MaxTry, str, S_sysspeed,seq);
			else {
				fgcolor(attcolor);
				cputs ("Pass #");
				fgcolor(bitcolor);
				cputs(itoa(pass, buf, 10));
				fgcolor(attcolor);
				cputs ("/");
				cputs(itoa(MaxTry, buf, 10));
				cputs (". ");
				fgcolor(filcolor);
				cputs (str);
				fgcolor(attcolor);
				cputs (" number ");
				fgcolor(bitcolor);
				cputs (S_sysspeed);
				fgcolor(attcolor);
				cputs (", wait for ");
				cputs(itoa(seq, buf, 10));
				fgcolor(attcolor);
				cputs (" sec.");
			}
			break;
		default:
			answer = -1;
	}
	if (screen)
		ClearLastErr();
}

/*VARARGS1*/
void Serror (const char *fmt, ...)
{
		va_list args;
		static char _Far msg[BUFSIZ];
		char *s;
		int len;

		va_start(args, fmt);
		vsprintf(msg, fmt, args);
		va_end(args);
		s = msg;
		len = strlen(s);
		if (len > 0 && s[len - 1] == '\n')
			s[--len] = '\0';
		if (!screen) {
			fputs(s, stderr);
			putc('\n', stderr);
			return;
		}
		if (len > MAXX-7)
			s[MAXX-1-7/*ERROR: */] = '\0';
		window (1, errline, MAXX, errline);
		bkcolor(BLACK);
		clrscr();
		fgcolor(YELLOW);
		bkcolor(hdrcolor);
		gotoxy (1,1);
		cputs ("ERROR:");
		bkcolor(BLACK);
		putch(' ');
		fgcolor(YELLOW);
		bkcolor(RED);
		cputs (s);
		bkcolor(BLACK);
		(void)time(&errtime);
}

/*VARARGS1*/
void Smisc(const char *fmt, ...)
{
		va_list args;
		static char _Far msg[BUFSIZ];
		char *s;
		int len;

		va_start(args, fmt);
		vsprintf(msg, fmt, args);
		va_end(args);
		s = msg;
		len = strlen(s);
		if (len > 0 && s[len - 1] == '\n')
			s[--len] = '\0';
		if (!screen) {
			fputs(s, stderr);
			putc('\n', stderr);
			return;
		}
		if (len > MAXX-9)
			s[MAXX-1-9/*MESSAGE: */] = '\0';
		window (1, miscline, MAXX, miscline);
		bkcolor(BLACK);
		clrscr();
		gotoxy (1,1);
		fgcolor(YELLOW);
		bkcolor(hdrcolor);
		cputs ("MESSAGE:");
		fgcolor(LIGHTCYAN);
		bkcolor(BLACK);
		putch(' ');
		cputs (s);
		(void)time(&errtime);
}

void Swputs(int num, const char *msg)
{
	const char *s;
	int i;

	if (!screen) {
		fputs(msg, stderr);
		return;
	}
	if (!inited[num])
		(*initfuncs[num])();
	window (begs[num].col, begs[num].row,
				 ends[num].col, ends[num].row);
	fgcolor(wcol[num]);
	if (!inited[num]) {
		curr[num].row = 1;
		curr[num].col = 1;
		clrscr();
		inited[num] = TRUE;
		wasnl[num] = FALSE;
	}
	gotoxy (curr[num].col, curr[num].row);
	if (num == WDEBUG)
		_wscroll = 1;
	for (s = msg; *s; s++) {
		if (wasnl[num]) {
			wasnl[num] = FALSE;
			curr[num].col = wherex();
			curr[num].row = wherey();
			if (curr[num].row != 1 ||
				curr[num].col != 1
			   )
				cputs("\r\n");
		}
		switch(*s) {
		case '\n':
			wasnl[num] = TRUE;
			break;
		case '\t':
			curr[num].col = wherex();
			for (i = ((curr[num].col - 1) & ~07) + 9 - curr[num].col; i > 0; i--)
			putch(' ');
			break;
		case '\f':
			clreol();
			break;
		default:
			putch(*s);
			break;
		}
	}
	_wscroll = 0;
	curr[num].col = wherex();
	curr[num].row = wherey();
	if (num == WDEBUG)   /* Prevent ^C scrolling */
		gotoxy(1, 1);
}

Sfmesg Stransstate = SF_NONE;

void Saddbytes(long count, Sfmesg state)
{
	char buf[20];
	time_t msecs;
	long cps;

	if (!screen || state != Stransstate)
		return;

	msecs = msecs_from(&start_time);
	if (msecs < 50)
		msecs = 50;

	Swputs(WTRANS, "");
	fgcolor(bitcolor);
	cps = bytes * 10 / ((msecs + 50) / 100);
	cprintf(CPS_FMT, cps < 9999 ? ltoa(cps, buf, 10) : "****");

	gotoxy(countcol, curr[WTRANS].row);
	cprintf(SIZE_FMT, count + restart_offs);

	ClearLastErr();
}

void Sftrans(Sfmesg mode, const char *fromfile, const char *hostfile, int param)
{
	Stransstate = mode;
	switch (mode) {
		case SF_SEND:
			if (!screen) {
				(void) fprintf(stderr, "Sending %s to %s\n", fromfile, hostfile);
				return;
			}
			Swputs (WTRANS, "\nSEND:");
			break;

		case SF_RECV:
			if (!screen) {
				(void) fprintf(stderr, "Receiving %s from %s\n", fromfile, hostfile);
				return;
			}
			Swputs(WTRANS, "\nRECEIVE:");
			break;

		case SF_SDONE:
			if (!screen) {
				(void) fprintf(stderr, "Sending %s completed.\n", fromfile);
				return;
			}
			Swputs(WTRANS, "\rSENDING DONE:");
			break;

		case SF_RDONE:
			if (!screen) {
				(void) fprintf(stderr, "Receiving %s completed.\n", fromfile);
				return;
			}
			Swputs(WTRANS, "\rRECEIVING DONE:");
			break;

		case SF_SABORTED:
			if (!screen) {
				(void) fprintf(stderr, "Sending aborted.\n");
				return;
			}
			Swputs(WTRANS, "\r");
			fgcolor(RED);
			cputs("SENDING ABORTED:");
			break;

		case SF_RABORTED:
			if (!screen) {
				(void) fprintf(stderr, "Receiving aborted.\n");
				return;
			}
			Swputs(WTRANS, "\r");
			fgcolor(RED);
			cputs("RECEIVING ABORTED:");
			break;

		case SF_DELIVER:
			if (!screen) {
				(void) fprintf(stderr, "Deliver message from %s to %s\n", fromfile, hostfile);
				return;
			}
			Swputs(WTRANS, "\n");
			cprintf("%2d Mail from ", param);
			fgcolor(filcolor);
			cputs (fromfile);
			fgcolor(wcol[WTRANS]);
			cputs (" to ");
			fgcolor(filcolor);
			cputs (hostfile);
			curr[WTRANS].col = wherex();
			curr[WTRANS].row = wherey();
			ClearLastErr();
			return;
		case SF_NONE:;
	}
	if (mode == SF_RDONE || mode == SF_SDONE) {
		char buf[20];
		time_t connected;
		unsigned long cps, bytes;

		connected = time(NULL) - remote_stats.lconnect;
		bytes = remote_stats.bsent + remote_stats.breceived;
		if ( connected <= 0 )
		   connected = 1;
		cps = bytes / connected;
		window (bytescol, errline - 1, MAXX, errline - 1);
		fgcolor(wcol[WTRANS]);
		clrscr();
		gotoxy (1, 1);
		cputs ("Total:");
		fgcolor(bitcolor);
		gotoxy (cpscol - bytescol + 1, 1);
		cprintf(CPS_FMT, cps < 9999 ? ltoa(cps, buf, 10) : "****");
		gotoxy (countcol - bytescol + 1, 1);
		cprintf(SIZE_FMT, bytes);

		ClearLastErr();
		return;
	}
	gotoxy (file1col, curr[WTRANS].row);
	fgcolor(filcolor);
	if (mode == SF_SEND && (*hostfile == 'X' || *hostfile == 'D')) {
		FILE *f;

		if ((f = FOPEN(fromfile, "r", BINARY)) != nil(FILE)) {
			char *s = nil(char);
			boolean first;
			static char buf[BUFSIZ], sbuf[BUFSIZ];

			first = TRUE;
			while(fgets(buf, sizeof(buf), f) != nil(char)) {
				if (first) {
					first = FALSE;
					if (  (*hostfile == 'D' && strncmp(buf, "From ", 5) != 0)
						|| (*hostfile == 'X' && strncmp(buf, "U ", 2) != 0))
						break;
					if (*hostfile == 'D') {
						strcpy(sbuf, buf);
						s = sbuf;
					}
				}
				else if (   (*hostfile == 'X' && strncmp(buf, "C ", 2) == 0)
					 || (*hostfile == 'D' && strncmp(buf, "From: ", 6) == 0))
				{
					int olen, slen;
					char *p;

					s = buf;
					if (*hostfile == 'X')
						s += 2;
			Found:
					fclose(f);
					slen = strlen(s);
					if (slen > 0 && s[slen - 1] == '\n')
						s[--slen] = '\0';
					olen = cpscol - file1col - 1;
					s[olen] = '\0';
					if (slen > olen)
						s[olen - 1] = s[olen - 2] = s[olen - 3] = '.';
					for (p = s; *p; p++)
						if (*p < ' ' || *p >= 0177)
							*p = '.';
					cputs (s);
					goto Skip;
				}
				else if (*buf == '\n')
					break;
			}
			if (*hostfile == 'D' && s != nil(char))
				goto Found;
			fclose(f);
		}
	}
	gotoxy (arrowcol, curr[WTRANS].row);
	fgcolor(filcolor);
	if (hostfile != NULL)
		cputs (hostfile);

Skip:
	gotoxy(cpscol, curr[WTRANS].row);
	curr[WTRANS].col = wherex();
	curr[WTRANS].row = wherey();

	gotoxy(countcol, curr[WTRANS].row);
	fgcolor(bitcolor);
	if ((mode != SF_SABORTED) && (mode != SF_RABORTED))
		cprintf(SIZE_FMT, 0L);

	ClearLastErr();
}


void Spakerr(int nerr)
{
	char buf[20];

	if (!screen)
		return;
	window (perrcol, errline - 1, terrcol - 1, errline - 1);
	fgcolor(BLACK);
	clrscr();
	if (nerr == 0)
		return;
	gotoxy (1, 1);
	fgcolor(RED);
	cputs ("Errors per packet: ");
	fgcolor (bitcolor);
	cputs(itoa(nerr, buf, 10));

	ClearLastErr();
}

void Stoterr(int nerrt)
{
	char buf[20];

	if (!screen)
		return;
	window (terrcol, errline - 1, bytescol - 1, errline - 1);
	fgcolor(BLACK);
	clrscr();
	if (nerrt == 0)
		return;
	gotoxy (1, 1);
	fgcolor(RED);
	cputs ("Total errors: ");
	fgcolor (bitcolor);
	cputs(itoa(nerrt, buf, 10));

	ClearLastErr();
}

static int dist(int tail, int head)
{
	if (tail < head)
		return head - tail;
	else
		return MAXINF - tail + head;
}

void Spacket(int tp, char c, int fg)
{
	static struct {
		unsigned char chr;
		unsigned char fgc;
	} infs[2][132], *inf;
	register int hd, tl;

	if (!screen)
		return;
	if (!Run_Flag)
		return;
	inf = infs[tp];
	hd = head[tp];
	tl = tail[tp];
	if (fg >= 0)
		inf[hd].fgc = (fg + fgbase) % NCOLORS;
	else
		inf[hd].fgc = -fg;
	if (islower(c))
		inf[hd].fgc |= BLINK;
	inf[hd].chr = toupper(c);
	hd = (hd + 1) % MAXINF;
	bkcolor(BLACK);
	window (MAXX, packline - !tp, MAXX, packline - !tp);
	clrscr();
	window (1, packline - tp, MAXX, packline - tp);
	fgcolor(bitcolor);
	gotoxy (MAXX, 1);
	putch (tp ? '>' : '<');
#ifdef __MSDOS__
	if (directvideo)
		if (hd > tl) {
			memcpy(scr_base+((packline-tp-1)*MAXX+MAXINF-hd+tl)*2,
				inf+tl, (hd-tl)*2);
		} else {
			memcpy(scr_base+(packline-tp-1)*MAXX*2, inf+tl, (MAXINF-tl)*2);
			memcpy(scr_base+((packline-tp-1)*MAXX+MAXINF-tl)*2, inf, hd*2);
		}
	else {
		register int i;

		if (hd > tl) {
			gotoxy (MAXINF - hd + tl + 1, 1);
			for (i = tl; i < hd; i++) {
				fgcolor(inf[i].fgc);
				putch(inf[i].chr);
			}
		} else {
			gotoxy (1, 1);
			for (i = tl; i < MAXINF; i++) {
				fgcolor(inf[i].fgc);
				putch(inf[i].chr);
			}
			for (i = 0; i < hd; i++) {
				fgcolor(inf[i].fgc);
				putch(inf[i].chr);
			}
		}
	}
#else
	if (hd > tl) {
		VioWrtCellStr((PCH)(inf+tl), (hd-tl)*2, packline-tp-1, MAXINF - hd + tl, 0);
	} else {
		VioWrtCellStr((PCH)(inf+tl), (MAXINF-tl)*2, packline-tp-1, 0, 0);
		VioWrtCellStr((PCH)inf, hd*2, packline-tp-1, MAXINF-tl, 0);
	}
#endif
	if (dist(tl, hd) >= MAXINF)
	tl = (tl + 1) % MAXINF;
	head[tp] = hd;
	tail[tp] = tl;
}

static
void ClearLastErr(void)
{
	if (!screen)
		return;
	if (errtime && time((time_t *)NULL) - errtime > 30) {
		errtime = 0;
		window (1, errline, MAXX, errline);
		clrscr();
	}
}

void SIdleIndicator(void)
{
#define MAXSHOW 4
#define SHOWCOL 12 /* "UUCICO:UUCP/@" */
	static char show[MAXSHOW] = {'/', '-', '\\', '|'};
	static curind = 0;

	if (!screen)
		return;
	curind = (curind + 1) % MAXSHOW;
	window(1, 1, MAXX, 1);
	gotoxy(SHOWCOL, 1);
	fgcolor(copcolor);
	putch(show[curind]);
}

#ifdef __OS2__
#pragma off (unreferenced);
static void show_sind(void * args)
#pragma on (unreferenced);
{
	DosSetPriority(PRTYS_THREAD,PRTYC_IDLETIME,-31,0);
	for (;;) {
		DosRequestMutexSem(kbdwait, -1);
		SIdleIndicator();
		DosReleaseMutexSem(kbdwait);
		DosSleep(125);
	}
} 

void InitSind(void)
{
	_beginthread(show_sind,NULL,STACK_SIZE,NULL);
}
#endif
