/*
 * Changes Copyright (C) 1991-1996 by Andrey A. Chernov, Moscow, Russia.
 *
 * History:4,1
 * Mon May 15 19:56:44 1989 Add c_break handler                   ahd
 * 20 Sep 1989 Add check for SYSDEBUG in MS-DOS environment       ahd
 * 22 Sep 1989 Delete kermit and password environment
 *             variables (now in password file).                  ahd
 * 30 Apr 1990  Add autoedit support for sending mail              ahd
 *  2 May 1990  Allow set of booleans options via options=         ahd
 * 29 Jul 1990  Change mapping of UNIX to MS-DOS file names        ahd
 */

/*
   ibmpc/host.c

   IBM-PC host program
*/

#include <assert.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#ifdef __OS2__
#include <signal.h>
#define INCL_DOSPROCESS
#include <os2.h>
void InitKbd(void);
#endif

#include "lib.h"
#include "dcp.h"
#include "hlib.h"
#include "hostable.h"
#include "pushpop.h"
#include "getopt.h"
#include "modem.h"
#include "fossil.h"
#include "timestmp.h"
#include "ulib.h"
#include "lock.h"
#include "screen.h"
#include "swap.h"
#include "ssleep.h"

unsigned _stklen = 0x3000;

currentfile();

#ifdef __MSDOS__
int c_break( void );
#else
void c_break( int );
#endif
extern int report(void);
int c_ign(void) { return 1; }

char *logintime = nil(char);  /* Length of time to wait for login */
boolean PressBreak = FALSE;
boolean Makelog = FALSE;
char *FrontRate = NULL;
extern boolean Run_Flag;
extern boolean Front;
extern boolean remote_debug;
extern int poll_mode;
extern int screen;
extern MaxTry;
extern boolean idle_active, send_only;
boolean H_Flag = FALSE;
boolean SkipLogin = FALSE;
char  *P_Flag = NULL;

static void parse_options(int argc, char *argv[])
{
  int option;

/*--------------------------------------------------------------------*/
/*                        Process our options                         */
/*--------------------------------------------------------------------*/

   while ((option = getopt(argc, argv, "A:r:s:u:x:X:S0nLF:ORvVH:P:l")) != EOF)
	  switch (option) {
	  case 'O':
		 if (!poll_mode) {
		 err:
			fprintf(stderr, "-O can't be used with -r 0\n");
			exit(4);
		 }
		 send_only = TRUE;
		 break;
	  case 'A':
		 MaxTry = atoi(optarg);
		 break;
	  case 'n':
		 callnow = TRUE;
		 break;
	  case 'r':
		 poll_mode = atoi(optarg);
		 if (!poll_mode && send_only)
			goto err;
		 break;
	  case 's':
		 Rmtname = strdup(optarg);
		 break;
	  case 'u':
		 logintime = strdup(optarg);
		 break;
	  case 'X':
		 logecho = TRUE;
		 /* FALL THRU */
	  case 'x':
		 debuglevel = atoi(optarg);
		 break;
	  case 'S':
		screen = 0;
		break;
	  case '0':
		remote_debug = TRUE;
		break;
	  case 'L':
		Makelog = TRUE;
		break;
	  case 'F':
		Front = TRUE;
		if (strtoul(optarg, NULL, 10) != 0)
			FrontRate = optarg;
		break;
	  case 'R':
		Run_Flag = FALSE;
		break;
	  case 'H':
		if (!Front) {
			fprintf(stderr, "-H can't be used without (before) -F\n");
			exit(4);
		}
		if (strncmp(optarg,"socket=",7)==0)
			fs_port = atoi(optarg+7);
		else
			fs_port = atoi(optarg);
		if (fs_port == 0) {
			fprintf(stderr, "Incorrect port handle %s\n",optarg);
			exit(4);
		}
		if (fs_port == 1) {	/* run as shell */
			fs_port=0;	/* stdin */
			close(fileno(stderr));
			open("nul", O_BINARY|O_RDWR|O_DENYNONE);
			if (getenv("TTYSOCKET")) {
				fs_port = atoi(getenv("TTYSOCKET"));
				P_Flag = strdup("TELNET");
			}
		}
		H_Flag = TRUE;
		break;
	  case 'P':
		P_Flag = strdup(optarg);
		checkref(P_Flag);
		break;
	  case 'l':
		SkipLogin = TRUE;
		break;
	  default:
	  case '?':
   usage:
		 fprintf(stderr,
		 "Usage:\tuucico [-A <phone_attempts>] [-s <system>|all|any]\n"
		 "\t[-r 1|0] [-x|X <debug_level>] [-u <time>] [-n] [-S] [-L]\n"
		 "\t[-F <speed> [-H <handle>|-P <device>] [-l]] [-O] [-R]\n");
		 exit(4);
	  }

/*--------------------------------------------------------------------*/
/*                Abort if any options were left over                 */
/*--------------------------------------------------------------------*/

   if (optind != argc) {
	  fprintf(stderr, "Extra parameter(s) at end.\n");
	  goto usage;
   }
}


int main( int argc, char *argv[])
{
   int status;

   parse_options(argc, argv);

#ifdef __MSDOS__  /* prevent -H switch in OS/2-version */
   close_junk_files();
#endif

   if (debuglevel < 0) {
	debuglevel = -debuglevel;
	logecho = TRUE;
   }
/*--------------------------------------------------------------------*/
/*          Report our version number and date/time compiled          */
/*--------------------------------------------------------------------*/
   Sinitplace(0);
   idle_active = TRUE;
   atexit(Srestoreplace);

   banner( argv );

   if (!configure())
	  panic();

   if (!configure_swap())
      return 4;

/*--------------------------------------------------------------------*/
/*                        Trap control C exits                        */
/*--------------------------------------------------------------------*/

#ifdef __TURBOC__
   ctrlbrk(c_break);
   setcbrk(1);
#else
   signal(SIGBREAK,c_break);
   signal(SIGTERM, c_break);
   signal(SIGINT,  c_break);
#endif

   printmsg(5,"main: Control C handler set");

   PushDir(spooldir);
   atexit( PopDir );

#ifdef __OS2__
   DosSetPriority(PRTYS_PROCESS, PRTYC_TIMECRITICAL, 0, 0);
   InitKbd();
   InitSind();
#endif
   status = dcpmain();

   Sshutdown();

   if (status == 0)
	  cleantmp();

#ifdef __OS2__
   nokbd();
#endif

   return status;
} /*main*/


/*--------------------------------------------------------------------*/
/*   c_break   -- control break handler to allow graceful shutdown    */
/*                    of interrupt handlers, etc.                     */
/*--------------------------------------------------------------------*/

extern LOCKSTACK savelock;

#ifdef __MSDOS__
int c_break( void )
#else
#pragma off (unreferenced);
void c_break( int sig )
#pragma on (unreferenced);
#endif
{
   setcbrk(0);
#ifdef __MSDOS__
   ctrlbrk(c_ign);
#else
   DosEnterCritSec();
#endif
   PressBreak = TRUE;

   printmsg(0,"c_break: program aborted by user Ctrl-Break");

   if (locked)
	UnlockSystem();
   if (savelock.locket) {
	PopLock(&savelock);
	UnlockSystem();
   }
   Sshutdown();

   report();

#ifdef __OS2__
   DosExitCritSec();
   nokbd();
#endif
   exit(100);  /* Allow program to abort              */

#ifdef __MSDOS__
   return 0;
#endif
} /* c_break */
