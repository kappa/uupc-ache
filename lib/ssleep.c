/*--------------------------------------------------------------------*/
/*    s s l e e p . c                                                 */
/*                                                                    */
/*    Smart sleep routines for UUPC/extended                          */
/*                                                                    */
/*    Written by Dave Watts, modified by Drew Derbyshire              */
/*                                                                    */
/*    Generates DOS specific code with Windows support by default,    */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <dos.h>
#include <conio.h>
#include <signal.h>
#include <setjmp.h>
#include <bios.h>
#include <io.h>

#include "lib.h"
#include "ssleep.h"
#include "screen.h"
#include "arpadate.h"
#include "hlib.h"
#if 0
#include "mnp.h"
#endif

#define MULTIPLEX 0x2F

currentfile();

extern boolean port_active;
extern char *produce_sound;
int maxwait;
boolean use_plus_minus = FALSE;
boolean force_redirect = FALSE;
boolean idle_active = FALSE;

/*--------------------------------------------------------------------*/
/*              Use this first to see if the rest are OK              */
/*                                                                    */
/*                  MOV AX,1600h   ; Check for win386/win3.0          */
/*                                   present                          */
/*                  INT 2Fh                                           */
/* Return AL = 0 -> No Windows, AL = 80 -> No WIn386 mode             */
/*        AL = 1 or AL = FFh -> Win386 2.xx running                   */
/*   else AL = Major version (3), AH = Minor version                  */
/*--------------------------------------------------------------------*/
/* --------------- Release time slice                                 */
/*                  MOV AX,1680h   ; **** Release time slice          */
/*                  INT 2Fh        ; Let someone else run             */
/* Return code is AL = 80H -> service not installed, AL = 0 -> all    */
/*                                                              OK    */
/*--------------------------------------------------------------------*/
/* --------------- Enter critical section (disable task switch)       */
/*                  MOV AX,1681H   ; Don't tread on me!               */
/*                  INT 2Fh                                           */
/*--------------------------------------------------------------------*/
/* --------------- End critical section (Permit task switching)       */
/*                  MOV AX,1682h                                      */
/*                  INT 2Fh                                           */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*    R u n n i n g U n d e r W i n d o w s W i t h 3 8 6             */
/*                                                                    */
/*    Easily the longest function name in UUPC/extended.              */
/*                                                                    */
/*    Determines if we are running under MS-Windows 386 or            */
/*    MS-Windows 3.  We save the result, to avoid excessively         */
/*    disabling interrupts when in a spin loop.                       */
/*--------------------------------------------------------------------*/

static boolean	RunningUnderWindowsWith386(void)
{
   static int result = 2;
   union REGS regs;

   if (result != 2)           /* First call?                         */
      return result;          /* No --> Return saved result          */

   regs.x.ax = 0x1600;
   int86(MULTIPLEX, &regs, &regs);
   result = ((regs.h.al & 0x7f) != 0);
   if (result)
	printmsg(1, "RunningUnderWindowsWith386: Windows detected, use time slice");
   return result;
} /* RunningUnderWindowsWith386 */

/*--------------------------------------------------------------------*/
/*    G i v e U p T i m e S l i c e                                   */
/*                                                                    */
/*    Surrender our time slice when executing under Windows/386       */
/*    or Windows release 3.                                           */
/*--------------------------------------------------------------------*/

static void GiveUpTimeSlice(boolean use_windows)
{
   union REGS regs;

   printmsg(11, "GiveUpTimeSlice");
   if (use_windows && RunningUnderWindowsWith386()) {
		regs.x.ax = 0x1680;
		int86(MULTIPLEX, &regs, &regs);
		if (regs.h.al != 0) {
			printmsg(0, "GiveUpTimeSlice: problem giving up timeslice:  %u", regs.h.al);
			panic();
		}
   } else
		geninterrupt(0x28);

} /* GiveUpTimeSlice */


/*--------------------------------------------------------------------*/
/*    ssleep() - wait n seconds					      */
/*                                                                    */
/*    Simply delay until n seconds have passed.                       */
/*--------------------------------------------------------------------*/

#define NOT_OVER_SECS 32

boolean ssleep(time_t interval)
{
   while (interval > NOT_OVER_SECS) {
	  if (ddelay(NOT_OVER_SECS * 1000))
		  return TRUE;
	  interval -= NOT_OVER_SECS;
   }
   return ddelay((int)interval * 1000);
} /*ssleep*/

boolean
keyb_control(boolean kb_only)
{
	int val;
	char *ad;
	boolean stdin_redirect;
	boolean tick_changed, ind_changed;
	struct timeb tb;
	int new_tick;

	extern boolean logecho, Makelog, remote_debug;
	extern FILE *log_stream, *logfile;
	extern char LINELOG[];
	extern int logmode;

	static int not_a_tty = -1;
	static int old_tick = -1;
	static long old_time = -1;

	if (not_a_tty < 0)
		not_a_tty = !isatty(fileno(stdin));
	stdin_redirect = force_redirect || not_a_tty;

	ftime(&tb);
	new_tick = tb.millitm / (1000 / 9/* times per sec*/);
	ind_changed = (old_tick != new_tick || old_time != tb.time);
	old_tick = new_tick;
	old_time = tb.time;
	if (kb_only)
		tick_changed = TRUE;
	else
		tick_changed = ind_changed;

	if (   tick_changed
	    && (stdin_redirect ? (bioskey(1) > 0) : kbhit())
	    ) {
		if (stdin_redirect) {
			(void) bioskey(0);
			return TRUE;
		}
		else {
			val = getch();
		again:
			switch (val) {
			case 'd':
				printmsg(-1, "keyb_control: enter new debug level (echo to file, if < 0): ");
				cscanf("%d", &val);
				if (val < 0) {
					logecho = TRUE;
					val = -val;
				}
				debuglevel = val;
				if (debuglevel > 0 && logecho && logfile == stdout) {
					if ((logfile = FOPEN(LOGFILE, "a", TEXT)) == nil(FILE)) {
						logfile = stdout;
						printerr("keyb_control", LOGFILE);
					}
					else
						printmsg(1, "keyb_control: open %s at %s", LOGFILE, arpadate());
				}
				printmsg(1, "keyb_control: new debug level set to %d", debuglevel);
				if (logfile != stdout && (!debuglevel || !logecho)) {
					fclose(logfile);
					logfile = stdout;
				}
				return FALSE;
			case 't':
				if (!port_active)
					return FALSE;
				printmsg(-1, "keyb_control: enter 'y' to log port data or 'n': ");
				Makelog = (getche() == 'y');
				ad = arpadate();
				if (Makelog && log_stream == NULL) {
					if ((log_stream = FOPEN(LINELOG, "a", BINARY)) == NULL)
						printerr("keyb_control", LINELOG);
					else {
						logmode = 0;
						printmsg(15, "logging serial line data to %s at %s", LINELOG, ad);
						fprintf(log_stream, "\r\n<<< Open %s at %s >>\r\n", LINELOG, ad);
					}
				}
				if (!Makelog && log_stream != NULL) {
					fclose(log_stream);
					log_stream = NULL;
					printmsg(15, "finish logging serial line data to %s at %s", LINELOG, ad);
				}
				return FALSE;
			case '-':
				if (use_plus_minus && maxwait > 0) {
					maxwait--;
					return TRUE;
				} else {
					while (kbhit()) {
						if ((val = getch()) != '-')
							goto again;
					}
				}
				break;
			case '+':
				if (use_plus_minus && maxwait < 32767) {
					maxwait++;
					return TRUE;
				} else {
					while (kbhit()) {
						if ((val = getch()) != '+')
							goto again;
					}
				}
				break;
			case 'e':
			case 'l':
			case 'p':
			case 'h':
			case 's':
				if (remote_debug)
					ungetch(val);
				break;
			case '\0':
				(void) getch();
				break;
#ifdef	__TURBOC__
			case '\3':
				raise(SIGINT);
#endif
			case '\33':
				return TRUE;
		   }
		}
	}
	if (kb_only)
		return FALSE;

	if (idle_active && ind_changed)
		SIdleIndicator();

	return FALSE;
}

static
boolean
WaitEvent(int milliseconds, boolean (*code)(void))
{
   struct timeb start;

/*--------------------------------------------------------------------*/
/*       Handle the special case of 0 delay, which is simply a        */
/*                  request to give up our timeslice                  */
/*--------------------------------------------------------------------*/
   if (milliseconds < 0)
	return TRUE;

   if (milliseconds == 0)     /* Make it compatible with DosSleep    */
   {
	if ((*code)())
		return FALSE;

	if (keyb_control(FALSE))
		return TRUE;

	GiveUpTimeSlice(FALSE);

	return FALSE;
   } /* if */

   ftime(&start);

   for(;;)   /* Begin the spin loop                 */
   {
	  if ((*code)())
		return FALSE;

	  if (keyb_control(FALSE))
		return TRUE;

	  GiveUpTimeSlice(milliseconds >= 1000);

	  if (msecs_from(&start) >= milliseconds)
		break;
   } /* while */

   return FALSE;

} /* ddelay */

static boolean false(void) {return FALSE;}

boolean
sdelay(int milliseconds)
{
	return WaitEvent(milliseconds, false);
}

/*--------------------------------------------------------------------*/
/*    d e l a y 						      */
/*                                                                    */
/*    Delay for an interval of milliseconds                           */
/*--------------------------------------------------------------------*/
boolean
ddelay(int milliseconds)
{
	extern w_flush(void);

	if (port_active && w_flush() < 0)
		return TRUE;
	return sdelay(milliseconds);
}

long msecs_from(struct timeb *start)
{
	struct timeb currt;

	ftime(&currt);
	return ((currt.time - start->time) * 1000 +
		currt.millitm - start->millitm);
}

void open_connection_sound(void)
{
	if (produce_sound == NULL)
		return;
	sound(880);
	(void)sdelay(200);
	nosound();
}

void close_connection_sound(void)
{
	if (produce_sound == NULL)
		return;
	sound(440);
	(void)sdelay(200);
	nosound();
}

void lost_connection_sound(void)
{
	if (produce_sound == NULL)
		return;
	sound(110);
	(void)sdelay(300);
	nosound();
}

