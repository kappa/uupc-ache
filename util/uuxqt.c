/*
   d c x q t

   A command formatter for DCP.  RH Lamb

   Sets up stdin and stdout on various machines.
   There is NO command checking so watch what you send and who you let
   to have access to your machine.  "C rm /usr/?*.*" could be executed!

   Changes Copyright (C) 1991, Andrew H. Derbyshire
   Changes Copyright (C) 1991-96 by Andrey A. Chernov, Moscow, Russia.


   Change history:

	  Broken out of UUIO, June 1991 Andrew H. Derbyshire
*/

/*--------------------------------------------------------------------*/
/*                        System include files                        */
/*--------------------------------------------------------------------*/

#include <ctype.h>
#include <io.h>
#include <dos.h>
#ifdef __TURBOC__
#include <dir.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __OS2__
#include <signal.h>
#include <process.h>
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#include <os2.h>
#endif

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include "lib.h"
#include "getopt.h"
#include "hlib.h"
#include "hostable.h"
#include "import.h"
#include "pushpop.h"
#include "readnext.h"
#include "timestmp.h"
#include "arpadate.h"
#include "usertabl.h"
#include "screen.h"
#include "memlim.h"
#include "lock.h"
#include "swap.h"

unsigned _stklen = 0x3000;

#define LINESIZE (2048+32)   /* Line length in command files */
#define PIPESIZE	4096
#define RMAILSTACKSIZE	32768
#define STACK_SIZE	16384

currentfile();

extern int screen;
extern char *swapto;
extern boolean videomemory;
extern char *rnewscmd;
static char *user = NULL, *machine = NULL, *requestor = NULL;
static char badjobdir[FILENAME_MAX];
static char rtmpfile[FILENAME_MAX];
static FILE *temp0 = NULL;
int prerr = 0;

#ifdef __TURBOC__
int c_break(void);
#else
void c_break(int sig);
#endif
extern int init_charset(boolean);
static int valid(char *line);
int c_ign(void) { return 1; }
boolean BreakPressed = FALSE;
int handled;

/*--------------------------------------------------------------------*/
/*                        Internal prototypes                         */
/*--------------------------------------------------------------------*/

static int  shell(boolean dopath, char *command, char *inname, char *outname,
		  char *errname, const char *remotename);

static void usage( void );

static boolean do_uuxqt( const char *system );

static int process( char *fname, const char *remote );

static int rmail(int argc, char* argv[]);

static int rbmail(const char *inname, const char *rmtname);
static int rcbmail(const char *inname, const char *rmtname);
static int rnews (int argc, char* argv[]);
static int notimp(int argc, char* argv[]);

void doneinfo(void)
{
	if (handled == 0)
		printmsg(-1, "doneinfo: No messages received. ");
	else
		printmsg(-1, "doneinfo: %d message(s) received. ", handled);
}

#ifdef __OS2__
int createpipe(int * in, int * out);

static int orig_output=-1;
static TID out_tid;

static void out_thread(void * arg)
{
	char str[128];
	FILE * inpipe = fdopen((int)arg, "r");
	while (fgets(str, sizeof(str), inpipe))
		Swputs(WDEBUG, str);
	_endthread();
}

static void restoreout(void)
{
	fflush(stdout);
	if (orig_output == -1) return;
	printmsg(14, "Restore stdout");
	close(fileno(stdout));
	DosWaitThread(&out_tid, DCWW_WAIT);
	dup2(orig_output, fileno(stdout));
	close(orig_output);
	printmsg(20, "Restore stdout success");
}

static void makeout(void)
{
	int hr,hw;

	if (!screen) return;
	if (createpipe(&hr, &hw))
	{	printmsg(1, "makeout: can't create pipe");
		return;
	}
	fflush(stdout);
	orig_output = dup(fileno(stdout));
	if (orig_output == -1)
	{
		printmsg(1, "makeout: can't save stdout: %s", strerror(errno));
		close(hw);
		close(hr);
		return;
	}
	dup2(hw, fileno(stdout));
	close(hw);
	atexit(restoreout);
	out_tid = _beginthread(out_thread, NULL, STACK_SIZE, (void *)hr);
	printmsg(6, "makeout: out_thread started, tid=%d", out_tid);
}
#endif

/*--------------------------------------------------------------------*/
/*    m a i n                                                         */
/*                                                                    */
/*    Main program                                                    */
/*--------------------------------------------------------------------*/

int main( int argc, char **argv)
{
   int c;
   extern char *optarg;
   extern int   optind;
   char *system = "all";
   char logname[FILENAME_MAX];

/*--------------------------------------------------------------------*/
/*        Process our arguments                                       */
/*--------------------------------------------------------------------*/

   while ((c = getopt(argc, argv, "s:x:X:S")) !=  EOF)
	  switch(c) {

	  case 's':
		 system = optarg;
		 break;

	  case 'X':
		 logecho = TRUE;
	  case 'x':
		 debuglevel = atoi( optarg );
		 break;
	  case 'S':
		 screen = 0;
		 break;
	  default:
	  case '?':
		 usage();
   }
   if (optind != argc) {
	  fprintf(stderr, "Extra parameter(s) at end.\n");
	  usage();
   }

   close_junk_files();

   Sinitplace(1);
   atexit(Srestoreplace);

/*--------------------------------------------------------------------*/
/*     Report our version number and date/time compiled               */
/*--------------------------------------------------------------------*/

   banner( argv );

/*--------------------------------------------------------------------*/
/*                             Initialize                             */
/*--------------------------------------------------------------------*/

   if (!configure())
	  return 4;   /* system configuration failed */

	/* Trap control C exits */
# ifdef __TURBOC__
	ctrlbrk(c_break);
	setcbrk(1);
# else
	signal(SIGINT,  c_break);
	signal(SIGTERM, c_break);
	signal(SIGBREAK,c_break);
# endif

   mkfilename(logname, spooldir, "uuxqt.log");
   if (debuglevel > 0 && logecho) {
	if (access(logname, 0))
		logfile = FOPEN(logname,"w", TEXT);
	else
		logfile = FOPEN(logname,"a", TEXT);
	if (logfile == nil(FILE)) {
		logfile = stdout;
		printerr("main", logname);
		printmsg(0, "main: can't open log file %s", logname);
	}
   }
   if (logfile != stdout)
	fprintf(logfile, "\n>>>> Log level %d started %s\n", debuglevel, arpadate());
   if (!init_charset(FALSE))
	return 4;
   if (!configure_swap())
	return 4;

   mkfilename(badjobdir, spooldir, "bad.job");

   checkuser( homedir  );     /* Force User Table to initialize      */
   checkreal( mailserv );     /* Force Host Table to initialize      */

   (void) mktempname(rtmpfile, "RBM"); /* Only one rbmail at once */

   handled = 0;

/*--------------------------------------------------------------------*/
/*                  Switch to the spooling directory                  */
/*--------------------------------------------------------------------*/

   PushDir( spooldir );
   atexit( PopDir );

/*--------------------------------------------------------------------*/
/*    Actually invoke the processing routine for the eXecute files    */
/*--------------------------------------------------------------------*/

#ifdef __OS2__
   makeout();
#endif

   do_uuxqt( system );

   if( !BreakPressed && equal( system , "all" ) )
	   do_uuxqt( nodename );

   if (temp0 != NULL)
	(void) fclose(temp0);
   (void) unlink(rtmpfile);
#ifdef __OS2__
   restoreout();
#endif

   if (!BreakPressed && !prerr)
	   doneinfo();
   return (BreakPressed ? 100 : 0);
} /* main */

/* c_break   -- control break handler to allow graceful shutdown   */
/* of interrupt handlers, etc.               */

#ifdef __MSDOS__
int c_break(void) {
#else
#pragma off (unreferenced);
void c_break(int sig) {
#pragma on (unreferenced);
#endif

	setcbrk(0);
#ifdef	__TURBOC__
	ctrlbrk(c_ign);
#endif

	printmsg(0, "c_break: program aborted by user Ctrl-Break (wait...)");

	BreakPressed = TRUE;

#ifdef __MSDOS__
	return 1;
#endif
}


/*--------------------------------------------------------------------*/
/*    d o _ u u x q t                                                 */
/*                                                                    */
/*    Processing incoming eXecute (X.*) files for a remote system     */
/*--------------------------------------------------------------------*/

static boolean do_uuxqt( const char *sysname )
{
   struct HostTable *hostp;
#if 0
   static char uu_machine[] = "UU_MACHINE=";
   char hostenv[sizeof uu_machine + 25 + 2];
#endif

/*--------------------------------------------------------------------*/
/*                 Determine if we have a valid host                  */
/*--------------------------------------------------------------------*/

   if( !equal( sysname , "all" ) ) {
	  if (equal( sysname , nodename ))
		  hostp = checkname( sysname );
	  else
		  hostp = checkreal( sysname );

	  if (hostp  ==  BADHOST) {
		 printmsg(0, "do_uuxqt: unknown host \"%s\".", sysname );
		 exit(1);
	  }
   } /* if */
   else
	  hostp = nexthost( TRUE );

/*--------------------------------------------------------------------*/
/*             Outer loop for processing different hosts              */
/*--------------------------------------------------------------------*/

   while  (hostp != BADHOST)
   {
      char fname[FILENAME_MAX];
      boolean locked = FALSE;

      if (BreakPressed)
	exit(100);

#if 0
/*--------------------------------------------------------------------*/
/*              Set up environment for the machine name               */
/*--------------------------------------------------------------------*/

      sprintf(hostenv,"%s%.25s", uu_machine, hostp->hostname);

      if (putenv( hostenv ))
      {
	 printmsg(0,"do_uuxqt: unable to set environment \"%s\"",hostenv);
	 panic();
      }
#endif
/*--------------------------------------------------------------------*/
/*           Inner loop for processing files from one host            */
/*--------------------------------------------------------------------*/

	 while (readnext(fname, hostp->hostname, "X", NULL, NULL) )
	    if ( locked || LockSystem( hostp->hostname , B_UUXQT ))
	    {
	       if (BreakPressed)
		   exit(100);
	       prerr |= process( fname , hostp->hostname );
	       locked = TRUE;
	    }
	    else
	       break;               /* We didn't get the lock        */

	 if ( locked )
	    UnlockSystem();

#if 0
/*--------------------------------------------------------------------*/
/*                        Restore environment                         */
/*--------------------------------------------------------------------*/

      putenv( uu_machine );   /* Reset to empty string               */
#endif
/*--------------------------------------------------------------------*/
/*    If processing all hosts, step to the next host in the queue     */
/*--------------------------------------------------------------------*/

      if( equal(sysname,"all") )
	 hostp = nexthost( FALSE );
      else
	 hostp = BADHOST;

   } /*while nexthost*/

   return FALSE;

} /* do_uuxqt */

/*--------------------------------------------------------------------*/
/*    p r o c e s s                                                   */
/*                                                                    */
/*    Process a single execute file                                   */
/*--------------------------------------------------------------------*/

static int process(char *fname, const char *remote )
{
   char *command = NULL,
		*input = NULL,
		*output = NULL,
		*token = NULL;
   static char line[LINESIZE];
   char hostfile[FILENAME_MAX];
   boolean skip = FALSE;
   boolean reject = FALSE;
   int result = 0, en = 0;
   FILE *fxqt;

   if (requestor != NULL) {
	 free(requestor);
	 requestor = NULL;
   }
   if (user != NULL) {
	  free(user);        /* Release any old name       */
	  user = NULL;
   }
   user = strdup("uucp");   /* user if none given */
							/* must allocate for later free call */
   checkref(user);
   if (machine != NULL) {
	   free(machine);        /* Release any old name       */
	   machine = NULL;
   }
   machine = strdup(remote);
   checkref(machine);

/*--------------------------------------------------------------------*/
/*                         Open the X.* file                          */
/*--------------------------------------------------------------------*/

   fxqt = FOPEN(fname, "r", BINARY);   /* inbound X.* file */
   if (fxqt == NULL)
   {
	  printerr("process", fname);
	  printmsg(0,"process: cannot open file \"%s\"",fname);
	  return 1;
   }
   else
	  printmsg(2, "process: processing %s", fname);

/*--------------------------------------------------------------------*/
/*                  Begin loop to read the X.* file                   */
/*--------------------------------------------------------------------*/

   while (!skip && fgets(line, sizeof(line) - 1, fxqt) != nil(char))
   {
	  char *cp;

	  line[sizeof(line) - 1] = '\0';
	  if ((cp = strchr(line, '\n')) != nil(char))
		 *cp = '\0';

	  printmsg(8, "process: %s", line);

/*--------------------------------------------------------------------*/
/*            Process the input line according to its type            */
/*--------------------------------------------------------------------*/

	  switch (line[0])
	  {

/*--------------------------------------------------------------------*/
/*                  User which submitted the command                  */
/*--------------------------------------------------------------------*/

	  case 'U':
		 strtok(line," \t\n");      /* Trim off leading "U"       */
		 cp = strtok(NULL," \t\n"); /* Get the user name          */
		 if ( cp == NULL )
		 {
			printmsg(0,"process: no user on U line \
in execute file \"%s\"", fname );
			cp = "uucp";            /* Use a nice default ...     */
		 }
		 free(user);
		 user = strdup(cp);     /* Reallocate new user name   */
		 checkref(user);

									/* Get the system name        */
		 if ( (cp = strtok(NULL," \t\n")) == NULL) { /* Did we get a string?       */
			printmsg(0,"process: no node on U line in file \"%s\"", fname );
			cp = (char *) remote;
		 } else if (!equal(cp,remote)) {
			/* Just a message */
			printmsg(-1, "process: node '%s' on U line in file \"%s\" doesn't match remote",
					 cp, fname );
			cp = (char * ) remote;
		 }
		 free(machine);
		 machine = strdup(cp);     /* Reallocate new node name   */
		 checkref(machine);
		 checkreal(machine);
		 break;

/*--------------------------------------------------------------------*/
/*                       Input file for command                       */
/*--------------------------------------------------------------------*/

	  case 'I':
		 input = strdup( &line[2] );                     /* ahd   */
		 checkref(input);
		 if (!equaln(input,"D.",2)) {
			printmsg(0, "process: invalid input file name: %s", input);
			reject = TRUE;
		 }
		 break;

/*--------------------------------------------------------------------*/
/*                      Output file for command                       */
/*--------------------------------------------------------------------*/

	  case 'O':
		 output = strdup( &line[2] );                    /* ahd   */
		 checkref(output);
		 if (!equaln(output,"D.",2)) {
			printmsg(0, "process: invalid output file name: %s", output);
			reject = TRUE;
		 }
		 break;

/*--------------------------------------------------------------------*/
/*                         Command to execute                         */
/*--------------------------------------------------------------------*/

	  case 'C':
		 command = strdup( &line[2] );                   /* ahd   */
		 checkref(command);                              /* ahd   */
		 break;

/*--------------------------------------------------------------------*/
/*                 Check that a required file exists                  */
/*--------------------------------------------------------------------*/

	  case 'F':
		 token = strtok(&line[1]," \t");
		 importpath(hostfile, token, remote);
		 printmsg(4, "process: check existance of \"%s\"", hostfile);
		 if ( access( hostfile, 0 ))   /* Does the host file exist?  */
		 {
			printmsg(0,"process: missing required file %s (%s) for %s, \
command skipped", token, hostfile, fname);
			skip = TRUE;
		 }
		 break;

/*--------------------------------------------------------------------*/
/*             Requestor name (overrides user name, above)            */
/*--------------------------------------------------------------------*/

	  case 'R':
		 strtok(line," \t\n");      /* Trim off leading "R"       */
									/* Get the user name          */
		 if ( (cp = strtok(NULL," \t\n")) == NULL )
			printmsg(0,"process: no requestor on R line in file \"%s\"", fname );
		 else {
			 requestor = strdup( cp );
			 checkref(requestor);
		 }
		 break;

	  default :
		 break;

	  } /* switch */
   } /* while */

   if ( fxqt != NULL )
	  fclose(fxqt);

   if ((command == NULL) && !skip)
   {
	  printmsg(0,"process: no command supplied for X.* file %s", fname);
	  reject = TRUE;
   } else if (requestor == NULL) {
	  requestor = strdup(user);
	  checkref(requestor);
   }

/*--------------------------------------------------------------------*/
/*           We have the data for this command; process it            */
/*--------------------------------------------------------------------*/

   if ( ! (skip || reject ))
   {
	  printmsg(2, "process: %s", command);

	  result = shell(TRUE, command, input, output, nil(char), remote);

	  if (result == 0) {
		  if (   strncmp(command, "rmail", 5) == 0
		      && (!command[5] || isspace(command[5]))
		     )
			 handled++;
		  unlink(fname);       /* Already a local file name            */

		  if (equaln(input,"D.",2)) {
			  importpath(hostfile, input, remote);
			  unlink(hostfile);
		  }
	  }

	  if (input != NULL)
		  free(input);
	  if ( output != NULL )
	  {
		 if (result == 0) {
			 importpath(hostfile, output, remote);
			 unlink(hostfile);
		 }
		 free(output);
	  }
	  if (result != 0)
		goto skp;
   } /* if (!skip ) */
   else {
	char	name[_MAX_FNAME];
	char	drv[_MAX_DRIVE];
	char	dir[_MAX_DIR];
	char	ext[_MAX_EXT];
skp:
	  printmsg(1,"process: job in file \"%s\" %s, \
\"%s\" return %d", fname, reject ? "rejected" : "skipped",
reject ? "process" : (command == NULL ? "????" : command), result );

	  if (en != ENOMEM) {
		  *name = *ext = '\0';
		  _splitpath(fname, drv, dir, name, ext);
		  strcpy(line, name);
		  strcat(line, ext);
		  mkfilename(hostfile, badjobdir, line);
		  if (RENAME(fname, hostfile) != 0) {
			printmsg(1,"process: can't move \"%s\" to %s, job deleted",
						fname, badjobdir);
			unlink(fname);
		  }
		  else
			printmsg(1,"process: job \"%s\" moved to %s for later perusal",
					fname, badjobdir);
	  }
   }

   if (command != NULL)
		free(command);
   return (result || reject || skip);
}

/*--------------------------------------------------------------------*/
/*    s h e l l                                                       */
/*                                                                    */
/*    Simulate a Unix command                                         */
/*                                                                    */
/*    Only the 'rmail', 'rbmail', 'rcbmail' , 'rzbmail' and 'rnews'   */
/*    are currently  supported                                        */
/*--------------------------------------------------------------------*/
#ifdef	__TURBOC__
#pragma argsused
#elif defined (__WATCOMC__)
#pragma off (unreferenced);
#endif
static int shell(boolean dopath, char *command,
				  char *inname,
				  char *outname,
				  char *errname,
				  const char *remotename)
#ifdef __WATCOMC__
#pragma on (unreferenced);
#endif
{

	char *argv[200];
	int argc;
	char *savcom;
	int result = 0;
	int (*proto)(int, char *[]);
   char inlocal[FILENAME_MAX]; char inf[FILENAME_MAX];
   char outlocal[FILENAME_MAX]; char outf[FILENAME_MAX];
   char tmpf[FILENAME_MAX];
   int newout, savein, saveout, i;
   FILE *newin;
   boolean savdopath;

   savcom = strdup(command);
   checkref(savcom);
   argc = getargs(command, argv, sizeof(argv)/sizeof(argv[0]));
   if (argc < 0) {
	printmsg(0, "shell: too many arguments");
	return 1;
   }

   if (debuglevel >= 6) {
	  char **argvp = argv;

	  i = 0;
	  while (i < argc)
		 printmsg(6, "shell: argv[%d]=\"%s\"", i++, *argvp++);
	}

   if (   equal(argv[0], "rzbmail")
       || equal(argv[0], "rcbmail")
      ) {
	importpath(inf, inname, remotename);
	mkfilename(inlocal, spooldir, inf);

	return rcbmail(inlocal, remotename);
   }

   if (equal(argv[0], "rbmail")) {
	importpath(inf, inname, remotename);
	mkfilename(inlocal, spooldir, inf);

	return rbmail(inlocal, remotename);
   }

   if (equal(argv[0], "rmail"))
	  proto = rmail;
   else if (equal(argv[0], "rnews"))
	  proto = rnews;
   else
	  proto = notimp;

/*--------------------------------------------------------------------*/
/*               We support the command; execute it                   */
/*--------------------------------------------------------------------*/

   *tmpf = '\0';
   fflush(logfile);
   if (logfile != stdout)
	  real_flush(fileno(logfile));
   PushDir(homedir);

/*--------------------------------------------------------------------*/
/*                     Open files for processing                      */
/*--------------------------------------------------------------------*/
	newin = NULL;
	savdopath = dopath;
	if (inname == NULL || !*inname) {
		inname = "nul";
		dopath = FALSE;
	}

	if(dopath) {
		importpath(inf, inname, remotename);
		mkfilename(inlocal, spooldir, inf);
	} else
		strcpy(inlocal, inname);

	 /* don't use shared open because of stdin non-shared */
	if (   newin == NULL
	    && strcmp(inname, "nul") != 0
	    && (newin = fopen(inlocal, "rb")) == NULL) {
		ErrIn:
		 printerr("shell", inlocal);
		 printmsg(0, "shell: can't open %s (%s): %s",
			inname, inlocal, strerror(errno));
		 result = -1;
		 goto Pop;
	}
	if (newin == NULL) /* inname == "nul" */
		if ((i = creat(inlocal, S_IWRITE)) < 0)
			goto ErrIn;
		else if ((newin = fdopen(i, "rb")) == NULL) {
				close(i);
				goto ErrIn;
		}

	if ((savein = dup(fileno(stdin))) < 0) {
		 printerr("shell", "dup(0)");
		 printmsg(0, "shell: can't save stdin");
		 if (newin != NULL)
			 fclose(newin);
		 result = -1;
		 goto Pop;
	}
#ifdef __MSDOS__ /* short command line, pass arguments via stdin */
	if (proto == rmail) {
		const char *ad;
		FILE  * argin;

		(void) mktempname(tmpf, "ARG");
		/* don't use shared open because of stdin non-shared */
		if ((argin = fopen(tmpf, "w+")) == NULL) {
			printerr("shell", tmpf);
			printmsg(0, "shell: can't open temp %s", tmpf);
			if (newin != NULL)
				fclose(newin);
			result = -1;
			goto Pop;
		}
		fprintf(argin, "%s\n", argv[0]);
		if (user != NULL || requestor != NULL) {
			fputs("-R\n", argin);
			ad = requestor != NULL ? requestor : user;
			/* we can don't quote '-' and '"' in file */
			fputs(ad, argin);
			fputc('\n', argin);
			if (ferror(argin))
				goto wrterr;
		}
		for (i = 1; i < argc; i++) {
			ad = argv[i];
			if ((*ad == '-') && (i == 1))
 				fputs("--\n", argin);
			fputs(ad, argin);
			fputc('\n', argin);
			if (ferror(argin)) {
		wrterr:
				printerr("shell", tmpf);
				printmsg(0, "shell: error writing to %s", tmpf);
				if (newin != NULL)
					fclose(newin);
				fclose(argin);
				result = -1;
				goto Pop;
			}
		 }
		 fputs("<<NULL>>\n", argin);
		 while ((i = getc(newin)) != EOF)
			if (putc(i, argin) == EOF)
				goto wrterr;
		 fclose(newin);
		 rewind(argin);
		 if (ferror(argin))
			goto wrterr;
		 newin = argin;
		 argc = 1;
		 argv[argc++] = savcom;	/* Fake argument, needed for recepients list */
		 argv[argc] = NULL;
	}
#endif
	setmode(fileno(newin), O_TEXT);
	if (dup2(fileno(newin), fileno(stdin)) < 0) {
		 printerr("shell", "dup2(fileno(newin), 0)");
		 printmsg(0, "shell: can't replace stdin");
		 if (newin != NULL)
			 fclose(newin);
		 result = -1;
		 goto Pop;
	}
	else
		setmode(fileno(stdin), O_BINARY);
	fclose(newin);
	dopath = savdopath;

	/* Redirect output */

	newout = -1;
	if (outname == NULL || !*outname) {
		outname = "nul";
		dopath = FALSE;
	}

	if (dopath) {
		importpath(outf, outname, remotename);
		mkfilename(outlocal, spooldir, outf);
	} else
		strcpy(outlocal, outname);

	/* don't use shared open here because stdout is non-shared */
	if (   newout < 0
	    && strcmp(outname, "nul") != 0
	    && (newout = open(outlocal, O_WRONLY|O_BINARY|O_APPEND)) < 0) {
		 printerr("shell", outlocal);
		 printmsg(0, "shell: can't open %s (%s): %s",
			outname, outlocal, strerror(errno));
errfound:
		/* Restore input */

		 if (inname != NULL && *inname) {
			if (dup2(savein, fileno(stdin)) < 0) {
				 printerr("shell", "dup2(savein, 0)");
				 printmsg(0, "shell: can't restore stdin");
				 result = -1;
				 goto Pop;
			}
			close(savein);
		 }
		 result = -1;
		 goto Pop;
	}
#ifdef __MSDOS__
	if (newout < 0) /* outname == "nul" */
		if ((newout = creat(outlocal, S_IWRITE)) < 0)
			goto errfound;
#endif
	fflush(stdout);
	if ((saveout = dup(fileno(stdout))) < 0) {
		printerr("shell", "dup(1)");
		printmsg(0, "shell: can't save stdout");
		goto errfound;
	}
#ifdef __OS2__
	if (newout > 0)
#endif
	{	if (dup2(newout, fileno(stdout)) < 0) {
			printerr("shell", "dup2(newout, 1)");
			printmsg(0, "shell: can't replace stdout");
			goto errfound;
		}
		close(newout);
	}

	result = (*proto)(argc, argv);

	/* Restore output */
	if (dup2(saveout, fileno(stdout)) < 0) {
		printerr("shell", "dup2(saveout, 1)");
		printmsg(0, "shell: can't restore stdout");
		result = -1;
	}
	close(saveout);

	/* Restore input */
	if (dup2(savein, fileno(stdin)) < 0) {
		printerr("shell", "dup2(savein, 0)");
		printmsg(0, "shell: can't restore stdin");
		result = -1;
		goto Pop;
	}
	close(savein);

Pop:
   PopDir();

   free(savcom);
   if (*tmpf)
	unlink(tmpf);
/*--------------------------------------------------------------------*/
/*                     Report results of command                      */
/*--------------------------------------------------------------------*/

   printmsg(8,"Result of spawn is ... %d",result);
   fflush(logfile);

   return result;

} /*shell*/

/*--------------------------------------------------------------------*/
/*    u s a g e                                                       */
/*                                                                    */
/*    Report how to run this program                                  */
/*--------------------------------------------------------------------*/

static void usage( void )
{
	fprintf(stderr, "Usage: uuxqt [-x|X <debug_level>] [-S] [-s <system>|all]\n");
	fprintf(stderr, "Valid remote commands are: rmail, rbmail, rcbmail, rzbmail, rnews\n");
	exit(4);
}

int rmail(int argc, char* argv[]) {
	extern char *sendprog, *calldir;
	static char sendmail[FILENAME_MAX] = "";
	int i, en, saverr;
	char* args[100];
	char *cp, *buf;
	char ubuf[LINESIZE];
	static char dbuf[LINESIZE];

	if (debuglevel >= 14) {
		printmsg(14, "rmail: started");
		for (i=0; i<argc; i++)
			printmsg(14, "    argv[%d] = %s", i, argv[i]);
	}
#ifdef __MSDOS__
	if ((buf = strpbrk(argv[argc - 1], " \t")) == NULL)
		buf = "*nobody*";
	else
		buf++;
#else
	if (argc > 1)
		buf = (char *)argv[argc-1];
	else
		buf = "*nobody*";
#endif

	cp = requestor != NULL ? requestor : user;
	i = strlen(machine);
	if (strncmp(machine, cp, i) == 0 && cp[i] == '!')
		strcpy(ubuf, cp);
	else
		sprintf(ubuf, "%s!%s", machine, cp);
	cp = ubuf;
	while ((cp = strchr(cp, '@')) != NULL)
		*cp++ = '%';

	Sftrans(SF_DELIVER, ubuf, buf, handled + 1);

	if (! *sendmail)
		if (sendprog == NULL)
			mkfilename(sendmail, calldir, "rmail.exe");
		else
			strcpy(sendmail, sendprog);
	i = 0;
	args[i++] = argv[0];
	/* args[i++] = "-i"; */
	args[i++] = "-u";	/* From UNIX, needs decoding */
	if (machine != NULL) {
		args[i++] = "-r";
		args[i++] = machine;
	}
	if (logecho && debuglevel > 0) {
		sprintf(dbuf, "-X%d", debuglevel);
		args[i++] = dbuf;
	}
	if (screen && !logecho)
		args[i++] = "-Z";	/* Use screen debug */
#ifdef __MSDOS__
	args[i++] = "-l";	/* Use arglist */
#else
	buf = dbuf + strlen(dbuf) + 1;
	if (argv[1][0] == '-')
		args[i++] = "--";
	for (en = 1; en < argc; en++) {
#ifndef __EMX__
		if (strpbrk(argv[en], " \"\\")) {
			args[i++] = buf;
			qstrcat(buf, (char *)argv[en]);
			buf += strlen(buf) + 1;
		} else
#endif
			args[i++] = argv[en];
		if (i == sizeof(args)/sizeof(args[0])) {
			printmsg(0, "rmail: too many arguments");
			return 1;
		}
	}
#endif
	args[i] = NULL;

	printmsg(5, "rmail: %s executing...", sendmail);
	i = -1;

#ifdef __MSDOS__
	if ((en = needkbmem(RKBSIZE - QKBSIZE, sendmail)) != 0)
		goto Err;
	if (swapto == NULL && (en = needkbmem(RKBSIZE, sendmail)) != 0)
		goto Err;
#endif

	if ((saverr = dup(fileno(stderr))) < 0) {
		printerr("rmail", "dup(stderr)");
		goto Err;
	}
	if ((i = creat("nul", S_IWRITE)) < 0) {
		printerr("rmail", "open(nul)");
		goto Err;
	}
	if (dup2(i,fileno(stderr)) < 0) {
		printerr("rmail", "dup2(nul,stderr)");
		goto Err;
	}
	(void) close(i);

	setcbrk(0);
#ifdef __MSDOS__
	i = swap_spawnv(sendmail, args);
#else
	i = spawnv(P_NOWAIT, sendmail, args);
	close(fileno(stdin)); /* prevent waiting for pipe forever after exiting rmail before EOF (by ^Z) */
	if (i != -1) {
		cwait(&i, i, WAIT_CHILD);
		i = ((i << 8) | (i >> 8)) & 0xffff;
	}
#endif
	if (i < 0) {
		en = ENOMEM;
		(void) dup2(saverr, fileno(stderr));
		close(saverr);
	Err:
		printmsg(en == ENOMEM, "rmail: %s not started: %s", sendmail, strerror(en));
	} else {
		(void) dup2(saverr, fileno(stderr));
		close(saverr);
	}
#ifdef __TURBOC__
	setcbrk(1);
#else
	signal(SIGINT,  c_break);
	signal(SIGTERM, c_break);
	signal(SIGBREAK,c_break);
#endif

	return i;
}

/*
	  r n e w s

	  Receive incoming news into the news directory.

	  This procedure needs to be rewritten to perform real news
	  processing.  Next release(?)
*/
#ifdef	__TURBOC__
#pragma argsused
#endif
int rnews(int argc, char *argv[])
{

   static int count = 1;
   int c;
   static char filename[FILENAME_MAX], format[FILENAME_MAX];
   FILE *f;
   long now = time(nil(time_t));

   if (rnewscmd)
	if (rnewscmd[0]) {
		printmsg(14, "rnews: executing %s", rnewscmd);
#ifdef __OS2__
		/* do redirections, exec via cmd if need,
		   and set argv[0] as "rnews" - call pipe_spawnv */
		{
			char * args[20];
			char * p, * cmd;
			const char * cmdname;
			int  r;

			cmd = strdup(rnewscmd);
			checkref(cmd);
			for (p=cmd,r=0;*p;) {
				args[r++]=p;
				p=strpbrk(p," \t");
				if (p==NULL) break;
				*p++='\0';
				while ((*p==' ')||(*p=='\t')) p++;
			}
			for (c=1; c<argc && r<sizeof(args)/sizeof(args[0])-1; c++)
				args[r++] = argv[c];
			args[r]=NULL;
			cmdname = args[0];
			args[0] = "rnews";
			r = pipe_spawnv(NULL, NULL, cmdname, args);
			if (r>0) {
				cwait(&r, r, WAIT_CHILD);
				r = ((r << 8) | (r >> 8)) & 0xffff;
			}
			free(cmd);
			return r;
		}
#else
		return swap_system(rnewscmd);
#endif
	}

/*--------------------------------------------------------------------*/
/*                        Get output file name                        */
/*--------------------------------------------------------------------*/

   mkfilename(format, newsdir, "%08.8lX.%03.3d");  /* make pattern first */

   do {
	  sprintf(filename, format, now, count++);  /* build real file name */
	  f = FOPEN(filename, "r", BINARY);
	  if (f != NULL)             /* Does the file exist?             */
		 fclose(f);              /* Yes --> Close the stream         */
   } while (f != NULL);

   printmsg(6, "rnews: delivering incoming news into %s", filename);

   if ((f = FOPEN(filename, "w", BINARY)) == nil(FILE))
   {
	  printmsg(0, "rnews: cannot open %s", filename);
	  return 2;
   }

/*--------------------------------------------------------------------*/
/*                  Main loop to write the file out                   */
/*--------------------------------------------------------------------*/

   while ((c = getchar()) != EOF)
		putc(c, f);

/*--------------------------------------------------------------------*/
/*                     Close open files and exit                      */
/*--------------------------------------------------------------------*/

   fclose(f);

   return 0;

} /*rnews*/

/*
   notimp - "perform" Unix commands which aren't implemented
*/

static
int
notimp(int argc, char *argv[])
{
   int i = 1;

   printmsg(0, "notimp: command '%s' not implemented", *argv);
   while (i < argc)
	  printmsg(6, "notimp: argv[%d]=\"%s\"", i++, *argv++);

   return 1;

} /*notimp*/

/*
 * Write a log record
 */
static void rbm_log(char *f, char *a1, char *a2)
{
	int fd;
	char buf[200];
	char logfile[FILENAME_MAX] = "";
	time_t now;
	struct tm *t;
	int l;

	sprintf(buf, "rbm_log: ");
	sprintf(buf + strlen(buf), f,a1,a2);
	printmsg(0, buf);
	if (!*logfile)
		mkfilename(logfile, spooldir, "rbmail.log");
	if( (fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT|O_TEXT|O_DENYNONE,S_IREAD|S_IWRITE)) < 0 )
		return;
	time(&now);
	t = localtime(&now);
	sprintf(buf, "%s\t%02d:%02d:%02d %02d.%02d.%04d\t", machine,
			 t->tm_hour, t->tm_min, t->tm_sec,
			 t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
	sprintf(buf + strlen(buf), f,a1,a2);
	l = strlen(buf);
	if ( buf[l - 1 ] != '\n' ) buf[l++] = '\n';
	fd_lock(logfile, fd);
	write(fd, buf, l);
	close(fd);
}

static void put_warning(FILE *file, char *str)
{
rbm_log(str, NULL, NULL);
fprintf(file,"\n******************************************************************\n");
fprintf(file,  "** ERROR DETECTED DURING BATCH-MAIL PROCESSING                  **\n");
fprintf(file,  "** WHILE RECEIVING BATCH FROM: %10s                       **\n",machine);
fprintf(file,  "** ERROR MESSAGE: %42.42s    **\n",str);
fprintf(file,  "******************************************************************\n");
}

#ifdef __OS2__
static void rmail_thread(void * arg)
{
	int  argc;
	char **args = (char **)arg;

	for (argc = 0; args[argc]; argc++);
	/* run rmail */
	*(int *)arg = rmail(argc, args);
	_endthread();
}
#endif

/*
 * Read Batched Mail
 */
static int
rbmail(const char *inname, const char *rmtname)
{
	extern void xf_ext(char *);
	unsigned short rcsum, csum;
	char     *p, *err1;
	static char headerline[LINESIZE];
	static char diagheader[LINESIZE];
	char	 errfile[FILENAME_MAX];
	char     curline[LINESIZE];
	char	 err[256];
	char     *rargs[100];
	FILE     *in, *temp = NULL, *diag = NULL;
	int      c, i;
	long	 len;
	int      syncflg = 0;        /* Find new header */
	long     saveseek = -1;
	int      firstopen = 0;
	boolean  wasCtrlZ;
#ifdef __OS2__
	int savein=0;
	int hin, hout;
	TID rmail_tid;
#else
	char     ** ap;
#endif

	if (inname == NULL)
        {	i = dup(fileno(stdin));
		setmode(i, O_BINARY);
		in = fdopen(i, "rb");
	} else if((in = FOPEN(inname, "r", BINARY)) == NULL) {
		printmsg(0, "rbmail: can't open batch %s: %s", inname, strerror(errno));
		return 1;
	}

	strcpy(err, "Rmail failure");

	/*
	 * Loop!!!
	 */
	while(fgets(headerline, (sizeof headerline)-1, in) != NULL) {
rescan:
		/*
		 * Parse header line
		 */
		rargs[0] = "rmail";
		headerline[(sizeof headerline)-1] = '\0';
		(void) strcpy(diagheader, headerline);
		temp = NULL;
		p = strchr(headerline, '\n');
		if (p == NULL) {
			strcpy(err, "Bad header (missing NL)");
filesync:
			syncflg = 1;
			if (saveseek != -1)
				fseek(in, saveseek, 0);
			goto do_error;
copy_on_error:
			syncflg = 0;
do_error:
			if (diag == NULL) {
				char buf[20];
				time_t tt;

				(void) time(&tt);
				sprintf(buf, "%08lx.ERR", tt);
				mkfilename(errfile, badjobdir, buf);
				diag = FOPEN(errfile,"a",TEXT);
				firstopen = 1;
			}

			if( diag == NULL ) {
				err1 = strerror(errno);
				printmsg(0, "rbmail: can't create %s: %s", errfile, err1);
				rbm_log("error: can't create %s", errfile, err1);
			}
			else {
				if (firstopen) {
					fd_lock(errfile, fileno(diag));
					firstopen = 0;
				}
				rbm_log("error: %s rest in: %.64s",err,errfile);
				fprintf(diag,"*** from %s, %s\n", machine, err);
				fputs(diagheader, diag);
				if( temp != NULL ) {
					char s[2];

					s[1] = '\0';
					while( (c = getc(temp)) != EOF ) {
						s[0] = c;
						xf_ext(s);
						putc(s[0], diag);
					}
				}
				headerline[0] = 0;
				while( fgets(headerline, (sizeof headerline)-1, in) != NULL ) {
					saveseek = ftell(in);
					if (valid(headerline) && syncflg) {
						rbm_log("Rescan from line: %.64s",headerline, NULL);
						goto rescan;
					}
					xf_ext(headerline);
					fputs(headerline,diag);
				}
				fclose(diag);
				firstopen = 0;
			}
			fclose(in);
			if (temp0 != NULL) {
				rewind(temp0);
				if (ferror(temp0) || chsize(fileno(temp0), 0l) < 0) {
					printmsg(0, "rbmail: can't truncate %s (non-fatal)", rtmpfile);
					fclose(temp0);
					goto CreateErr;
				}
				fseek(temp0, 0l, SEEK_SET);
			} else {
				/* don't need shared open for temp file */
	CreateErr:
				printmsg(6, "rbmail: Creating temp file: %s", rtmpfile);
				temp0 = fopen(rtmpfile, "w+b");
			}
#ifdef __OS2__
			if (savein != -1) {
				dup2(savein, fileno(stdin));
				close(savein);
			}
			if (temp != NULL) {
				fclose(temp);
				DosWaitThread(&rmail_tid, DCWW_WAIT);
			}
#endif
			if (temp0 != NULL) {
				/*
				 * try to send corrupted batch to postmaster
				 */
				fprintf(temp0, "From: \"UUPC/@ Daemon\" <MAILER-DAEMON@%s>\n", domain);
				fprintf(temp0, "To: Postmaster\n");
				fprintf(temp0, "Subject: Batch Reassembling Error\n");
				fprintf(temp0, "Date: %s\n\n", arpadate());
				fprintf(temp0, "error: %s rest in: %.64s\n",err,errfile);
				(void) fflush(temp0);
				(void) real_flush(fileno(temp0));
				p = strdup("rmail postmaster");
				checkref(p);
				(void) shell(FALSE, p,
					rtmpfile, NULL, NULL, rmtname);
			} else
				printmsg(0, "rmail: failure opening %s", rtmpfile);

			return 1;
		}
		*p =  '\0';

		/* Length */
		p = headerline;
		len = 0;
		for(i = 0; i < 7; i++) {
			c = *p++;
			if(c < '0' || c > '9') {
				strcpy(err, "Header corrupted (bad char in length)");
				goto filesync;
			}
			len = (len*10) + (c - '0');
		}
		if(*p++ != ' ') {
			strcpy(err, "Header corrupted (no space after length)");
			goto filesync;
		}

		/* Checksum */
		rcsum = 0;
		for(i = 0; i < 4; i++) {
			c = *p++;
			rcsum <<= 4;
			if('0' <= c && c <= '9')
				rcsum |= c - '0';
			else if('A' <= c && c <= 'F')
				rcsum |= c - 'A' + 0xa;
			else if('a' <= c && c <= 'f')
				rcsum |= c - 'a' + 0xa;
			else {
				strcpy(err, "Header corrupted (bad char in checksum)");
				goto filesync;
			}
		}
		if(*p++ != ' ') {
			strcpy(err, "Bad header (no space after checksum)");
			goto filesync;
		}

		/* Destination addressee */
		if( ! *p ) {
			strcpy(err, "Header corrupted (empty dest address)");
			goto filesync;
		}
		i = getargs(p, &rargs[1], sizeof(rargs)/sizeof(rargs[0])-1);
		if (i < 0) {
			strcpy(err, "Header corrupted (too many addressee)");
			goto filesync;
		}
		if( rargs[1] == NULL) {
			strcpy(err, "Header corrupted (no dest address)");
			goto filesync;
		}

#ifdef __MSDOS__
		/*
		 * Create temporary file
		 */
		if (temp0 != NULL) {
			rewind(temp0);
			if (ferror(temp0) || chsize(fileno(temp0), 0l) < 0) {
				printmsg(0, "rbmail: can't truncate %s (non-fatal)", rtmpfile);
				fclose(temp0);
				goto CreateIt;
			} else
				temp = temp0;
			fseek(temp0, 0l, SEEK_SET);
		} else {
CreateIt:	/* don't need shared open on temp. file */
			printmsg(6, "rbmail: Creating temp file: %s", rtmpfile);
			temp = temp0 = fopen(rtmpfile, "w+b");
		}
#else /* __OS2__ */
		if (createpipe(&hin, &hout))
		{
			printmsg(0, "rbmail: can't create pipe");
			goto filesync;
		}
		setmode(hout, O_BINARY);
		temp = fdopen(hout, "w+b");
		savein = dup(fileno(stdin));
		dup2(hin, fileno(stdin));
		close(hin);
		rmail_tid = _beginthread(rmail_thread, NULL, RMAILSTACKSIZE, rargs);
#endif

		if (temp == NULL ) {
			sprintf(err, "Can't create temp %s: %s",
				  rtmpfile, strerror(errno));
			printmsg(0, "rbmail: %s", err);
			goto copy_on_error;
		}

		saveseek = ftell(in);
		if (fgets(curline, (sizeof curline)-1, in) == NULL && rcsum) {
			strcpy(err, "EOF before end of package");
			goto copy_on_error;
		}

		p = curline;
		csum = 0;
		if (strncmp(curline, "From ",5) != 0) {
			time_t t;

			if (*curline == '\n') {
				p = NULL;
				csum += '\n';
				len--;
			}
			time(&t);
			fprintf(temp, "From %s!uucp %s", machine, ctime(&t));
			if (ferror(temp))
				goto err_wr;
		}
		/*
		 * Copy into temp. file calculating checksum on the fly
		 */
		wasCtrlZ = FALSE;
		while(len-- > 0) {
			if (p != NULL) {
				c = (unsigned char) *p++;
				if (!*p)
					p = NULL;
			}
			else
				c = getc(in);
			if (c == EOF) {
				put_warning(temp,"EOF before end of package");
				csum = ~rcsum;
				break;
			}
			if (c == '\x1a') wasCtrlZ = TRUE;
			if (!wasCtrlZ)
				putc(c, temp);
			if(ferror(temp)) {
	err_wr:
				rewind(temp);
				sprintf(err, "Error writing temp %s: %s",
					  rtmpfile, strerror(errno));
				printmsg(0, "rbmail: %s", err);
				goto copy_on_error;
			}
			if(csum & 01)
				csum = (csum>>1) + 0x8000;
			else
				csum >>= 1;
			csum += c;
			csum &= 0xffff;
		}
		if( csum != rcsum ) {
			char buf[64];

			sprintf(buf,"Checksum error: %04x != %04x",csum,rcsum);
			put_warning(temp,buf);
			strcpy(err, buf);
		}
		else
			saveseek = ftell(in);
#ifdef __MSDOS__
		rewind(temp);
		if (ferror(temp) || real_flush(fileno(temp)) < 0)
			goto err_wr;
		/*
		 * Call rmail
		 */
		strcpy(curline, "rmail");
		for (ap = &rargs[1]; *ap; ap++) {
			strcat(curline, " ");
			strcat(curline, *ap);
		}
		i = shell(FALSE, curline, rtmpfile, NULL, NULL, rmtname);
#else
		if (fclose(temp)) {
			temp = NULL;
			DosWaitThread(&rmail_tid, DCWW_WAIT);
			goto err_wr;
		}
		temp = NULL;
		DosWaitThread(&rmail_tid, DCWW_WAIT);
		i = *(int *)rargs; /* rmail retcode returned by thread */
		dup2(savein, fileno(stdin));
		close(savein);
		savein = -1;
#endif

		if (i < 0) {
			strcpy(err, "Can't spawn rmail");
			printmsg(1, "rbmail: %s", err);
			goto copy_on_error;
		}
		else if (i > 0) {
			sprintf(err, "rmail return error code %d", i);
			printmsg(0, "rbmail: %s", err);
			goto copy_on_error;
		}
		if (csum != rcsum)
			goto filesync;
		handled++;
	}
	fclose(in);

	return 0;
}

#define MAXUARGS 20

/*
 * Read Compressed Batched Mail
 */
static int
rcbmail(const char *inname, const char *rmtname)
{
	extern char *E_uncompress;
	char compress[FILENAME_MAX * 2];
	int i;
#ifdef __MSDOS__
	char ctmpfile[FILENAME_MAX];
	int en = 0;
	char *s;
	char *args[MAXUARGS];
#endif

	if (E_uncompress == NULL) {
		printmsg(0, "rcbmail: Uncompress variable not defined!");
		return -1;
	}
#ifdef __MSDOS__
	(void) mktempname(ctmpfile, "Z");
	sprintf(compress, E_uncompress, ctmpfile);
	printmsg(20, "rcbmail: command line: %s", compress);
	i = getargs(compress, args, sizeof(args)/sizeof(args[0]));	/* Insert nuls into compress str */
	if (i < 0) {
		printmsg(0, "rcbmail: to many args in Uncompress variable!");
		return -1;
	}

	if (!cp(inname, FALSE, ctmpfile, TRUE)) {
		printerr("rcbmail: cp", ctmpfile);
		return -1;
	}

	printmsg(5, "rcbmail: %s executing...", compress);
	i = -1;
	if ((en = needkbmem(CKBSIZE - QKBSIZE, compress)) != 0)
		goto Err;
	if (swapto == NULL && (en = needkbmem(CKBSIZE, compress)) != 0)
		goto Err;

	setcbrk(0);
	i = swap_spawnv(compress, args);
	if (i < 0) {
		en = ENOMEM;
	Err:
		printmsg(en == ENOMEM, "rcbmail: %s not started: %s", compress, strerror(en));
	}
#ifdef __TURBOC__
	setcbrk(1);
#else
	signal(SIGINT,  c_break);
	signal(SIGTERM, c_break);
	signal(SIGBREAK,c_break);
#endif

	if (i != 0) {
		(void) unlink(ctmpfile);	/* old .Z name */
		if (i > 0) {
			if (   (s = strrchr(ctmpfile, '.')) != NULL
			    && stricmp(s, ".Z") == 0
			   )
				*s = '\0';
			else {
		badnm:
				printmsg(0, "rcbmail: bad temporary name %s", ctmpfile);
				panic();
			}
			(void) unlink(ctmpfile);	/* uncompressed name */
			printmsg(0, "rcbmail: Uncompress error code %d", i);
			return i;
		}
		return i;
	}

	(void) unlink(ctmpfile);	/* old .Z name */
	if (   (s = strrchr(ctmpfile, '.')) != NULL
	    && stricmp(s, ".Z") == 0
	   )
		*s = '\0';
	else
		goto badnm;
	i = rbmail(ctmpfile, rmtname);
	(void) unlink(ctmpfile); /* uncompressed name */
#else /* OS/2 */
	{
		int gzip_pid, h, savein;

		sprintf(compress, E_uncompress, ""); /* not clean :( */
		strcat(compress, "<");
		strcat(compress, inname);
		gzip_pid = pipe_system(NULL, &h, compress);
		if (gzip_pid == -1) {
			printmsg(0, "rcbmail: %s not started", compress);
			return 1;
		}
		savein = dup(fileno(stdin));
		setmode(h, O_BINARY);
		dup2(h, fileno(stdin));
		close(h);
		i = rbmail(NULL, rmtname);
		dup2(savein, fileno(stdin));
		close(savein);
		if (i) {
			DosKillProcess(DKP_PROCESSTREE, gzip_pid); /* prevent "broken pipe" */
			cwait(&i, gzip_pid, WAIT_CHILD);
			i = 1;
		} else {
			cwait(&i, gzip_pid, WAIT_CHILD);
			if (i)
				printmsg(0, "rcbmail: uncompress return code %u",
            				((i<<8) | (i>>8)) & 0xffff);
		}
	}
#endif
	return i;
}

static int valid(char *line)
{
	int i;

	for (i = 0; i < 7; i++)
		if (line[i] < '0' || line[i] > '9')
			return 0;
	if (line[7] != ' ')
		return 0;
	for (i = 8; i < 8 + 4; i++)
		if (   (line[i] < '0' || line[i] > '9')
		    && (line[i] < 'a' || line[i] > 'f')
		    && (line[i] < 'A' || line[i] > 'F')
		   )
			return 0;
	if (line[8 + 4] != ' ')
		return 0;
	return 1;
}
