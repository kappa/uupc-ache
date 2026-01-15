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
#include <io.h>
#include <fcntl.h>
#include <process.h>
#include <ctype.h>
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_KBD
#include <os2.h>

#include "os2conio.h"

#include "lib.h"
#include "ssleep.h"
#include "screen.h"
#include "arpadate.h"
#include "hlib.h"

extern boolean port_active;
extern char *produce_sound;
HEV kbdsem;
extern HMTX kbdwait;
int kbd_tid;
int maxwait;
boolean use_plus_minus = FALSE;
boolean force_redirect = FALSE;
boolean idle_active = FALSE;

static void kbd_control(void * args);

boolean ssleep(time_t interval)
{ return ddelay(interval*1000); }

void InitKbd(void)
{

#if 0
   if (KbdGetFocus(1, 0)) {
     printmsg(0, "InitKbd: can't KbdGetFocus");
     return;
   }
#endif
   DosCreateEventSem(NULL,&kbdsem,0,0);
   DosCreateMutexSem(NULL,&kbdwait,0,0);
   kbd_tid=_beginthread(kbd_control,NULL,STACK_SIZE,"");
}

void nokbd(void)
{
	if (kbdwait != -1)
		DosRequestMutexSem(kbdwait, -1);
}

static int getkbd(void)
{
    int c;

    DosRequestMutexSem(kbdwait, -1);
    while (kbhit()==0) {
      DosReleaseMutexSem(kbdwait);
      DosSleep(100);
      DosRequestMutexSem(kbdwait, -1);
    }
    c = getch();
    if (c==0)
      c = getch()<<8;
    DosReleaseMutexSem(kbdwait);
    return c;
}

#pragma off (unreferenced);
boolean keyb_control(boolean keyb_only)
#pragma on (unreferenced);
{
    ULONG semcount;

    DosResetEventSem(kbdsem, &semcount);
    return (semcount > 0);
}

#pragma off (unreferenced);
static void kbd_control(void * args)
#pragma on (unreferenced);
{
    static int not_a_tty = -1;
    ULONG semcount;
    boolean stdin_redirect;
    int val;
    char * ad;
    char c;

    extern boolean logecho, Makelog;
    extern FILE *log_stream, *logfile;
    extern char LINELOG[];
    extern int logmode;

    not_a_tty = !isatty(fileno(stdin));
    stdin_redirect = force_redirect || not_a_tty;

    DosSetPriority(PRTYS_THREAD,PRTYC_REGULAR,-31,0);

    for (;;)
    {
	c = getkbd();
	if (stdin_redirect)
	{
		DosResetEventSem(kbdsem, &semcount); /* prevent overflow */
		DosPostEventSem(kbdsem);
		continue;
	}
again:
	switch (c) {
	case 'd':
		printmsg(-1, "keyb_control: enter new debug level (echo to file, if < 0): ");
		c=getkbd();
		if (c=='-') {
			logecho = TRUE;
			putch(c);
			c = getkbd();
		}
		val=0;
		do {
			val=val*10+(c-'0');
			putch(c);
		} while (isdigit(c=getkbd()));

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
		goto again;
	case 't':
		if (!port_active)
			break;
		printmsg(-1, "keyb_control: enter 'y' to log port data or 'n': ");
		Makelog = ((c=tolower(getkbd())) == 'y');
		putch(c);
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
		break;
	case '-':
		if (use_plus_minus && maxwait > 0)
			maxwait--;
		break;
	case '+':
		if (use_plus_minus && maxwait < 32767)
			maxwait++;
		break;
	case '\33':
		DosResetEventSem(kbdsem, &semcount); /* prevent overflow */
		DosPostEventSem(kbdsem);
		break;
    }
  }
}

static
boolean
WaitEvent(int milliseconds)
{
  ULONG semcount;
/*--------------------------------------------------------------------*/
/*       Handle the special case of 0 delay, which is simply a        */
/*                  request to give up our timeslice                  */
/*--------------------------------------------------------------------*/
   if (milliseconds < 0)
	return TRUE;
   if (milliseconds == 0)
    milliseconds = 1;

   DosResetEventSem(kbdsem, &semcount);
   if (DosWaitEventSem(kbdsem,milliseconds))
     return FALSE;
   DosResetEventSem(kbdsem, &semcount);
   return TRUE;
} /* ddelay */

boolean
sdelay(int milliseconds)
{
	return WaitEvent(milliseconds);
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
	DosBeep(880,200);
}

void close_connection_sound(void)
{
	if (produce_sound == NULL)
		return;
	DosBeep(440,200);	
}

void lost_connection_sound(void)
{
	if (produce_sound == NULL)
		return;
	DosBeep(110,200);	
}
