/*
      Program:    uupoll.c              22 September 1989
      Author:     Andrew H. Derbyshire
                  108 Decatur St
                  Arlington, MA 02174
      Internet:   help@kew.com
      Function:   Performs autopoll functions for UUCICO
      Language:   Borland C++ 2.0
      Usage:      uupoll [-r 0] [-f hhmm] [-i hhmm|0400 ] [-d hhmm]
                         [-a hhmm] [-x debug] [-s systems]

                  Where:

                  -r 0     specifies that UUCICO is to run into
                           passive mode when waiting to poll out

                  -r 1     specifies that UUCICO will not run in passive
                           mode while waiting to poll out, but polling
                           out will occur.

				  -f hhmm  is the first time in the day that uuio
                           is to poll out.  If omitted, polling
						   begins after the interval specified with
                           -i.

				  -i hhmm  the interval the UUCICO is to poll out
                           at.  If omitted, a period of 4 hours
                           will be used.

                  -d hhmm  Terminate polling after hhmm.  Default is
                           not to terminate.

                  -a hhmm  Automatically poll actively using the system
                           name "any" after any successfully inbound
                           poll if hhmm have past since last poll.  hhmm
                           may be 0000.

                  In addition, the following flags will be passed
		  to UUCICO:

				  -s system      system name to poll.  By default,
								 UUCICO will be invoked with '-s all'
								 followed by '-s any'.

				  -x n           debug level.   The default level
								 is 1.

 */

#include <assert.h>
#include <ctype.h>
#include <dos.h>
#include <limits.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#ifdef __TURBOC__
#include <direct.h>
#endif
#include <io.h>
#include <malloc.h>
#include <signal.h>

#include "getopt.h"
#include "lib.h"
#include "timestmp.h"
#include "ssleep.h"
#include "hlib.h"
#include "arpadate.h"
#include "swap.h"

typedef int hhmm;
extern void noswap(void);

#define hhmm2sec(HHMM)    ((time_t)(((HHMM / 100) * 60L) + \
						   (time_t)(HHMM % 100)) * 60L)

static int     active(char *Rmtname, int debuglevel);
static boolean busywork( time_t next);
static int     execute( char *command );
static time_t  nextpoll( hhmm first, hhmm interval );
static boolean     notanumber( char *number);
static int     passive( time_t next, int debuglevel );
static void    usage(void);
static hhmm    firstpoll(hhmm interval);
static void    uuxqt( int debuglevel);

static time_t now;            /* Current time, updated at start of
				 program and by busywork() and
				 execute()                           */
char xcase = 'x';
static screen = 1;
static attempts = 2;
extern char *calldir;

/*--------------------------------------------------------------------*/
/*    m a i n                                                         */
/*                                                                    */
/*    main program                                                    */
/*--------------------------------------------------------------------*/

int main( int argc , char *argv[] )
{

   int option;
   hhmm first = - 1;
   hhmm interval = -1;
   time_t duration = LONG_MAX;
   int nopassive = 2;
   boolean autoforward = TRUE;
   boolean done = FALSE;
   time_t autowait = 0;
   char *Rmtname = NULL;
   int debuglevel = 0;
   int returncode = 0;

   close_junk_files();

   banner(argv);

   if (!configure())
	exit(4);

   if (!configure_swap())
	exit(4);

   while((option = getopt(argc, argv, "A:a:d:f:i:s:r:X:x:S")) != EOF)
   switch(option)
   {

/*--------------------------------------------------------------------*/
/*       Automatically poll "any" after an incoming phone call        */
/*--------------------------------------------------------------------*/

	  case 'a':
		 autoforward = TRUE;
		 if (notanumber(optarg))
			usage();
		 autowait = hhmm2sec( atoi(optarg) );
		 break;

/*--------------------------------------------------------------------*/
/*                         First time to poll                         */
/*--------------------------------------------------------------------*/

	case 'f':
		 if (notanumber(optarg))
			usage();
		 first = atoi(optarg);
		 if ( interval == -1 )
			interval = 400;
		 break;

/*--------------------------------------------------------------------*/
/*                          Interval to poll                          */
/*--------------------------------------------------------------------*/

	  case 'i':
		 if (notanumber(optarg))
			usage();
		 interval = atoi(optarg);
		 nopassive = min(1,nopassive);
		 break;

/*--------------------------------------------------------------------*/
/*                      Time (duration) to poll                       */
/*--------------------------------------------------------------------*/

	  case 'd':
		 if (notanumber(optarg))
			usage();
		 duration  = atoi(optarg);
		 break;

/*--------------------------------------------------------------------*/
/*                        System name to poll                         */
/*--------------------------------------------------------------------*/

	  case 's':
		 Rmtname = strdup(optarg);
		 break;

/*--------------------------------------------------------------------*/
/*                            Debug level                             */
/*--------------------------------------------------------------------*/

	  case 'x':
	  case 'X':
		 if (notanumber(optarg))
			usage();
		 xcase = option;
		 debuglevel = atoi(optarg);
		 break;

/*--------------------------------------------------------------------*/
/*                       Passive polling option                       */
/*--------------------------------------------------------------------*/

	  case 'r':
		 if (notanumber(optarg))
			usage();
		 nopassive = atoi(optarg);
		 break;

	  case 'A':
		 if (notanumber(optarg))
			usage();
		 attempts = atoi(optarg);
		 break;

	  case 'S':
		 screen = 0;
		 break;

/*--------------------------------------------------------------------*/
/*                                Help                                */
/*--------------------------------------------------------------------*/
	  default:
	  case '?':
		 usage();
   } /* switch */

/*--------------------------------------------------------------------*/
/*             Terminate with error if too many arguments             */
/*--------------------------------------------------------------------*/

   if (optind != argc)
   {
	  puts("Extra parameters on command line.");
	  usage();
   }

/*--------------------------------------------------------------------*/
/* Terminate if neither active polling nor passive polling requested  */
/*--------------------------------------------------------------------*/

   if ( nopassive == 2 && (first < 0))
   {
	  puts("Must specify -r 0, -f hhmm, or -i hhmm");
	  usage();
   }

   time( &now );

/*--------------------------------------------------------------------*/
/*            If running under MS-DOS, enable Cntrl-Break.            */
/*--------------------------------------------------------------------*/

#ifdef __MSDOS__
   setcbrk(1);                   /* Turn it on to allow abort        */
#endif

   puts("Press Ctrl+C to abort program, ESC to force poll");

   if ( interval >= 0 && first < 0)
	  first = firstpoll(interval);

/*--------------------------------------------------------------------*/
/*    If a quitting internal was specified, convert it to an          */
/*    absolute time                                                   */
/*--------------------------------------------------------------------*/

   if (duration != LONG_MAX)
   {
	  duration = hhmm2sec( duration ) + (now / 60L) * 60L;
									/* Round to next lower minute    */
	  printf("Will terminate upon completion of first event \
after %s", ctime(&duration));
   } /* if */

/*--------------------------------------------------------------------*/
/*                       Beginning of main loop                       */
/*--------------------------------------------------------------------*/

   while ( !done && (duration > now))
   {
	  time_t next = LONG_MAX;
	  time_t autonext = now + autowait;
	  time_t wait = 10;      /* Time to wait after first panic()    */

      if (first >= 0)
	 next = nextpoll(first,interval);
	  else
	 next = duration;

	  while ((now < next) && ! done )
	  {
		 if (nopassive) {
			if (busywork(next))
				next = now;
		 } else {
			time_t spin;

			returncode = passive(next,debuglevel);
			if (returncode == 3)    /* Error in UUCICO?              */
			{                       /* Yes --> Allow time to fix it  */
			   spin = now + wait;   /* Figure next wait              */
			   wait *= 2 ;          /* Double wait for next time     */
			   if (busywork( spin > next ? next : spin ))
				next = now;
						/* But only wait till next poll  */
			} /* if (returncode == 3) */
			else {
			   wait = 10;
			   if ((returncode == 0) && autoforward && (now > autonext))
			   {
				  returncode = active("any",debuglevel);
				  autonext = now + autowait;
			   } /* if */
			} /* else */

			if ( (now > duration) && (now < next))
			   done = TRUE;
			else if ( returncode == 100 )
			   done = TRUE;

		 } /* else */

	  } /* while */

	  if ( ! done && (first >= 0) )
	  {
		 returncode = active(Rmtname,debuglevel);
		 if ( returncode == 100 )
			done = TRUE;
	  } /* if ( ! done && (first >= 0) ) */

   } /* while */

/*--------------------------------------------------------------------*/
/*                          End of main loop                          */
/*--------------------------------------------------------------------*/

   uuxqt( debuglevel );          /* One last call to UUXQT                 */

   return(returncode);
} /* main */


/*--------------------------------------------------------------------*/
/*    a c t i v e                                                     */
/*                                                                    */
/*    Perform an active (outgoing) poll of other hosts                */
/*--------------------------------------------------------------------*/

static int active(char *Rmtname, int debuglevel)
{
   int result;

   if (Rmtname == NULL)             /* Default?                         */
   {                                /* Yes --> do -s all and -s any     */
	  if (active("all",debuglevel) < 100)
		 return active("any",debuglevel);
      else
         return 100;
   }
   else {
	  char buf[128], buf2[128];

	  mkfilename(buf2, calldir, "uucico.exe");
	  sprintf(buf,"%s -r 1 -s %s -%c %d -A %d%s",
					buf2,Rmtname,xcase,debuglevel,
					attempts,screen?"":" -S");
      result = execute(buf);
      if ( result == 0 )
		 uuxqt( debuglevel );

      return result;
   }
 } /* active */

/*--------------------------------------------------------------------*/
/*    b u s y w o r k                                                 */
/*                                                                    */
/*    Waits for next time to poll without answering the telephone.    */
/*    Maybe we should at least beep on the hour?  :-)                 */
/*--------------------------------------------------------------------*/

static boolean busywork( time_t next)
{
   time_t naptime;
   time_t hours, minutes, seconds;
   boolean force;

   naptime = next - now;

   hours   = (naptime / 3600) % 24;    /* Get pretty time to display... */
   minutes = (naptime / 60) % 60;
   seconds = naptime % 60;

   printf("Going to sleep for %02ld:%02ld:%02ld, next poll is %s",
			 hours, minutes, seconds, ctime(&next) );
   force = ssleep( naptime );

   time( & now );

   if (force)
	printf("Force poll at %s", ctime(&next));
   return force;
}


/*--------------------------------------------------------------------*/
/*    e x e c u t e                                                   */
/*                                                                    */
/*    Executes a command via a spawn() system call.  This avoids      */
/*    the storage overhead of COMMAND.COM and returns the actual      */
/*    return code from the command executed.                          */
/*                                                                    */
/*    Note that this does not allow quoted command arguments, which   */
/*    not a problem for the intended argv[0]s of UUCICO.              */
/*--------------------------------------------------------------------*/

static int execute( char *command )
{
   char *argv[20];
   char buf[128];
   int argc = 0;
   int result;
   FILE *stream = NULL;

   printf("Executing command: %s\n",command);
   mkfilename(buf, calldir, "spool\\uupoll.log");
   if (access(buf, 0) == 0) {
	   stream = FOPEN(buf,"a",TEXT);
	   if (stream == NULL)
	   {
		  perror(buf);
		  abort();
	   } /* if */
	   fd_lock(buf, fileno(stream));
	   fprintf(stream, "%s: %s\n",arpadate(), command);
	   fclose(stream);
   }

   argv[argc] = strtok(command," \t");

   while ( argv[argc++] != NULL )
	  argv[argc] = strtok( NULL," \t");
   result = swap_spawnv(argv[0], argv);
   if ( result < 0 )
   {
	  printf("\a\nSpawning \"%s\" failed!\n", argv[0]);
	  abort();
   }
   else
	  setcbrk(1);

   time( & now );
   return result;
}

/*--------------------------------------------------------------------*/
/*    n e x t p o l l                                                 */
/*                                                                    */
/*    Returns next time to poll in seconds                            */
/*                                                                    */
/*    modified 14 October 1990 By Ed Keith.                           */
/*    modified 4 November 1990 by Drew Derbyshire.                    */
/*--------------------------------------------------------------------*/

static time_t nextpoll( hhmm first, hhmm interval )
{
   time_t sfirst;
   time_t sinterval = hhmm2sec( interval );
   time_t today;
   time_t tomorrow;
   struct tm  *time_record;   /* Ed K. 10/14/1990 */

   if (sinterval > 0) {
	   time_record = localtime(&now); /* Ed K. 10/14/1990 */
	   time_record->tm_sec = 0;   /* Ed K. 10/14/1990 */
	   time_record->tm_min = 0;   /* Ed K. 10/14/1990 */
	   time_record->tm_hour= 0;   /* Ed K. 10/14/1990 */
	   today = mktime(time_record);
	   tomorrow = today + 86400;
	   sfirst = today + hhmm2sec(first);

	   while (sfirst < now)
		  sfirst += sinterval;

	   if (sfirst > tomorrow)
		  sfirst = tomorrow + hhmm2sec(first);
   }
   else
	  sfirst = now;

   return sfirst;
} /* nextpoll */


/*--------------------------------------------------------------------*/
/*    f i r s t p o l l                                               */
/*                                                                    */
/*    Determine first time to poll if not specified                   */
/*--------------------------------------------------------------------*/

static hhmm firstpoll(hhmm interval)
{
   struct tm  *time_record;
   time_t sfirst;
   hhmm first;

   if (interval > 0) {
	   time_record = localtime(&now);
	   sfirst = time_record->tm_hour * 3600L +
				time_record->tm_min * 60L +
				time_record->tm_sec;
	   sfirst  = sfirst % hhmm2sec(interval);
	   first = (hhmm) ((sfirst / 3600L) * 100L + (sfirst % 3600L) / 60L);
   }
   else
	   first = 0;

   printf("First polling time computed to be %-2.2d:%-2.2d\n",
		 first / 100, first % 100);
   return first;
} /* firstpoll */

/*--------------------------------------------------------------------*/
/*    n o t a n u m b e r                                             */
/*                                                                    */
/*    Examines string, returns true if non-numeric                    */
/*--------------------------------------------------------------------*/

 static boolean notanumber( char *start)
 {
   char *number = start;
   while (*number != '\0')
   {
      if (!isdigit(*number))
      {
         printf("Parameter must be numeric, was %s\n",start);
         return TRUE;
      }
      number++;
   }
   return FALSE;
 } /* notanumber */


/*--------------------------------------------------------------------*/
/*    p a s s i v e                                                   */
/*                                                                    */
/*    Invoke UUCICO in passive mode until next active poll (if any).  */
/*--------------------------------------------------------------------*/

 static int passive( time_t next, int debuglevel )
 {
   char buf[128], buf2[128];    /* Buffer for execute() commands          */
   int result;

   mkfilename(buf2, calldir, "uucico.exe");
   sprintf(buf,"%s -r 0 -%c %d -A %d%s",
		buf2,xcase,debuglevel,attempts,screen?"":" -S");

   if (next > 0)              /* Should we poll out?                    */
   {                          /* Yes --> Compute time to poll           */
      char xnow[5];            /* Current time as a string              */
	  char then[5];           /* Next time to poll as a string          */
	  struct tm *tm_now;      /* Format time in integer values          */

      tm_now = localtime(&now);   /* Get time broken down             */
	  sprintf(xnow,"%02d%02d", tm_now->tm_hour, tm_now->tm_min);

/*--------------------------------------------------------------------*/
/*    Format the time to stop polling.  Note that to have the         */
/*    system actively poll at time x, we compute the window to        */
/*    passively poll to end at x-1, since the passive window will     */
/*    remain open until x-1 + 59 seconds.                             */
/*--------------------------------------------------------------------*/

      if ( (next - now) > 120)   /* Poll at least two minutes?       */
         next--;                 /* Yes --> End at start of the min  */

      tm_now = localtime(&next); /* Get time broken down            */
	  sprintf(then,"%02d%02d", tm_now->tm_hour, tm_now->tm_min);
      strcat(buf," -u Any");
      strcat(buf,xnow);
	  strcat(buf,"-");
	  strcat(buf,then);
   }

   result = execute(buf);
   if ( result == 0 )
	  uuxqt( debuglevel );
   return result;
 } /* passive */

/*--------------------------------------------------------------------*/
/*    u u x q t                                                       */
/*                                                                    */
/*    Execute the UUXQT program to run files received by UUCICO       */
/*--------------------------------------------------------------------*/

 static void uuxqt( int debuglevel)
 {
   int result;
   char buf[128], buf2[128];        /* Buffer for execute() commands          */

   mkfilename(buf2, calldir, "uuxqt.exe");
   sprintf(buf,"%s -%c %d%s", buf2, xcase, debuglevel, screen?"":" -S");
   result = execute(buf);
   assert( result == 0 );
 } /* uuxqt */


/*--------------------------------------------------------------------*/
/*    u s a g e                                                       */
/*                                                                    */
/*    Report correct usage of the program and then exit.              */
/*--------------------------------------------------------------------*/

 static void usage(void)
 {
   printf("Usage:\tuupoll [-a hhmm] [-d hhmm] [-f hhmm] [-i hhmm] [-A <attempts>]\n\
\t[-r 0|1] [-s <system>|all|any] [-x|X <debug_level>] [-S]\n");
   exit(4);
 }

