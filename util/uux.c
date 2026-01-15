/*
	  Program:    uux.c              20 August 1991
	  Author:     Mitch Mitchell
	  Email:      mitch@harlie.lonestar.org

	  Much of this code is shamelessly taken from extant code in
	  UUPC/Extended.

	  Usage:      uux [ options ] command-string

			   Where [ options ] are:

	 -aname    Use name as the user identification replacing the initiator
			   user-id.  (Notification will be returned to the user.)

	 -b        Return whatever standard input was provided to the uux command
			   if the exit status is non-zero.

	 -c        Do not copy local file to the spool directory for transfer to
			   the remote machine (default).

     -C        Force the copy of local files to the spool directory for
               transfer.

	 -e        Remote system should use sh to execute commands.

	 -E        Remote system should use exec to execute commands.

	 -ggrade   Grade is a single letter/number; lower ASCII sequence
			   characters will cause the job to be transmitted earlier during
			   a particular conversation.

	 -j        Output the jobid ASCII string on the standard output which is
			   the job identification.  This job identification can be used by
			   uustat to obtain the status or terminate a job.

	 -n        Do not notify the user if the command fails.

	 -p        Same as -:  The standard input to uux is made the standard
			   input to the command-string.

	 -r        Do not start the file transfer, just queue the job.
			   (Currently uux does not attempt to start the transfer
				regardless of this option).

	 -sfile    Report status of the transfer in file.

	 -xdebug_level
			   Produce debugging output on the standard output.  The
			   debug_level is a number between 0 and 9; higher numbers give
			   more detailed information.

	 -z        Send success notification to the user.


	 The command-string is made up of one or more arguments that look like a
	 normal command line, except that the command and filenames may be prefixed
	 by system-name!.  A null system-name is interpreted as the local system.

*/

/*--------------------------------------------------------------------*/
/*         System include files                                       */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dos.h>
#ifndef _MSDOS__
#include <signal.h>
#endif

/*--------------------------------------------------------------------*/
/*         Local include files                                        */
/*--------------------------------------------------------------------*/

#include  "lib.h"
#include  "hlib.h"
#include  "getopt.h"
#include  "getseq.h"
#include  "expath.h"
#include  "arpadate.h"
#include  "import.h"
#include  "pushpop.h"
#include  "hostable.h"
#include  "timestmp.h"
#include  "swap.h"

currentfile();

unsigned _stklen = 0x3000;

/*--------------------------------------------------------------------*/
/*                          Global variables                          */
/*--------------------------------------------------------------------*/


typedef enum {
		  FLG_USE_USERID,
		  FLG_OUTPUT_JOBID,
		  FLG_READ_STDIN,
		  FLG_QUEUE_ONLY,
		  FLG_NOTIFY_SUCCESS,
		  FLG_NONOTIFY_FAIL,
		  FLG_COPY_SPOOL,
		  FLG_RETURN_STDIN,
		  FLG_STATUS_FILE,
		  FLG_USE_EXEC,
		  FLG_MAXIMUM
	   } UuxFlags;

typedef enum {
	  DATA_FILE   = 0,
	  INPUT_FILE  = 1,
	  OUTPUT_FILE = 2
	  } FileType;

#define MAXARGS 500

static boolean flags[FLG_MAXIMUM] = {
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE,
			FALSE
		};
static char* st_out = nil(char);
static char* user_id = nil(char);
static char* user_path = nil(char);
static char  grade = 'N';          /* Default grade of service */

static char  job_id[15];
static char farg[128], uarg[128], darg[128];
static char buf[BUFSIZ];
static char* spool_fmt = SPOOLFMT;
static char* dataf_fmt = DATAFFMT;

static char* send_cmd  = "S %s %s %s - %s 0666\n";

boolean filelist = FALSE;
extern boolean visual_output;

/*--------------------------------------------------------------------*/
/*                        Internal prototypes                         */
/*--------------------------------------------------------------------*/

static void usage( void );
static char *SwapSlash(char *p);
static boolean split_path(char *path, char *system, char *file, boolean expand);
static boolean CopyData( const char *input, const char *output);
static boolean remove_parens(char *string);
static boolean do_uuxqt(char *job_name, char *src_syst, char *src_file, char *dest_syst, char *dest_file);
static boolean do_copy(char *src_syst, char *src_file, char *dest_syst, char *dest_file);
static boolean do_remote(int optind, int argc, char **argv);
static void preamble(FILE* stream);
static char subseq( void );

#ifdef __MSDOS__
static int null(void) {return 1;}
#endif
/*--------------------------------------------------------------------*/
/*    u s a g e                                                       */
/*                                                                    */
/*    Report flags used by program                                    */
/*--------------------------------------------------------------------*/

static void usage()
{
	  fprintf(stderr, "Usage: uux [-c|-C] [-e|-E] [-b] [-gGRADE] [-p] [-j] [-n] [-r] [-sFILE]\\\n\
\t[-aNAME] [-z] [-] [-x|-X DEBUG_LEVEL] [-Z] [-l|command-string]\n");
	  fprintf(stderr, "\tdon't forget to use '(' and ')' for any parameter\n\
\twhich is not a valid filename or for unknown 'system!' prefix\n");
	  exit(4);
}


/*--------------------------------------------------------------------*/
/*    s w a p s l a s h                                               */
/*                                                                    */
/*    Change backslash in a directory path to forward slash           */
/*--------------------------------------------------------------------*/

static char *SwapSlash(char *p)
{
	 char *q = p;

     while (*q) {
		if (*q ==  '\\')
		   *q = '/';
		q++;
	 }
	 return p;
};

/*--------------------------------------------------------------------*/
/* C o p y D a t a                                                    */
/*                                                                    */
/* Copy data into its final resting spot                              */
/*--------------------------------------------------------------------*/

static boolean CopyData( const char *input, const char *output)
{
   FILE    *datain;
   FILE    *dataout;
   int c;
   boolean  status = TRUE;

   if ( (dataout = FOPEN(output, "w", BINARY)) == nil(FILE) ) {
	  printerr("CopyData", output);
	  printmsg(0,"uux: Cannot open spool file \"%s\" for output",
			   output);
	  return FALSE;
   }
   fd_lock(output, fileno(dataout));
/*--------------------------------------------------------------------*/
/*                      Verify the input opened                       */
/*--------------------------------------------------------------------*/

   if (input == nil(char))
	  datain = stdin;
   else
	  datain = FOPEN(input, "r", BINARY);

   if (datain == nil(FILE)) {
	  printerr("CopyData", input);
	  printmsg(0,"uux: unable to open input file \"%s\"",
			   (input == nil(char) ? "stdin" : input));
	  fclose(dataout);
	  return FALSE;
   } /* datain */

/*--------------------------------------------------------------------*/
/*                       Loop to copy the data                        */
/*--------------------------------------------------------------------*/

   while ((c = getc(datain)) != EOF)
   {
	  putc(c, dataout);
	  if (ferror(dataout))     /* I/O error?               */
	  {
		 printerr("CopyData", "dataout");
		 printmsg(0,"uux: I/O error on \"%s\"", output);
		 fclose(dataout);
		 return FALSE;
	  } /* if */
   } /* while */

/*--------------------------------------------------------------------*/
/*                      Close up shop and return                      */
/*--------------------------------------------------------------------*/

   if (ferror(datain))        /* Clean end of file on input?          */
   {
	  printerr("CopyData", input);
	  clearerr(datain);
	  status = FALSE;
   }

   if (input != nil(char))
	   fclose(datain);

   if (fclose(dataout) == EOF) {
		 printerr("CopyData", "dataout");
		 printmsg(0,"uux: I/O error on \"%s\"", output);
		 return FALSE;
   }
   return status;

} /* CopyData */

/*--------------------------------------------------------------------*/
/*    r e m o v e _ p a r e n s                                       */
/*                                                                    */
/*--------------------------------------------------------------------*/

static boolean remove_parens(char *string)
{
      int len = strlen(string);

	  if ((string[0] != '(') || (string[len - 1] != ')'))
		  return FALSE;

	  strcpy(string, &string[1]);
	  string[len - 2] = '\0';
	  return TRUE;                 /* and we're done */
}

/*--------------------------------------------------------------------*/
/*    s p l i t _ p a t h                                             */
/*--------------------------------------------------------------------*/

static boolean split_path(char *path,
                          char *sysname,
                          char *file,
						  boolean expand)
{
      char *p_left;
      char *p_right;
      char *p = path;

      *sysname = *file = '\0';    /* init to nothing */

/*--------------------------------------------------------------------*/
/*                if path is wildcarded then error                    */
/*--------------------------------------------------------------------*/

	  if (strcspn(path, "*?[") < strlen(path))
         return FALSE;

      p_left = strchr(p, '!');         /* look for the first bang */
      if (p_left == NULL || (p_left == p))
      {                                /* not a remote path */
         if (p_left == p )
         {
	    strcpy(file, p+1);         /* so just return filename */
	    if ( expand && (expand_path(file, NULL, homedir, NULL) == NULL) )
               return FALSE;
	 }
         else
	    strcpy(file, p);           /* so just return filename */
	 strcpy(sysname, nodename);
         return TRUE;
      }

      p_right = strrchr(p, '!');      /* look for the last bang */
      strcpy(file, p_right + 1);      /* and thats our filename */

      strncpy(sysname, p, p_left - p); /* and we have a system thats not us */
      sysname[p_left - p] = '\0';

      /* now see if there is an intermediate path */

      if (p_left != p_right)  {        /* yup - there is */
          return FALSE;
      }

#if 0
	  if (expand && (strcspn(file, "~") >= strlen(file)) )
		  if (expand_path(file, NULL, homedir, NULL) == NULL)
		     return FALSE;
#endif

	  return TRUE;                     /* and we're done */
}

/*--------------------------------------------------------------------*/
/*    d o _ u u x q t                                                 */
/*                                                                    */
/*    Generate a UUXQT command file for local system                  */
/*--------------------------------------------------------------------*/

static boolean do_uuxqt(char *job_name,
                        char *src_syst,
                        char *src_file,
						char *dest_syst,
			char *dest_file)
{
   long seqno = 0;
   char *seq  = nil(char);
   FILE *stream;              /* For writing out data                 */

   char msfile[FILENAME_MAX]; /* MS-DOS format name of files          */
   char msname[22];           /* MS-DOS format w/o path name          */
   char ixfile[15];           /* eXecute file for local system,
                                 UNIX format name for local system    */

/*--------------------------------------------------------------------*/
/*          Create the UNIX format of the file names we need          */
/*--------------------------------------------------------------------*/

   seqno = getseq();
   seq = JobNumber( seqno );

   sprintf(ixfile, spool_fmt, 'X', nodename, grade , seq);

/*--------------------------------------------------------------------*/
/*                     create local X (xqt) file                      */
/*--------------------------------------------------------------------*/

   importpath( msname, ixfile, nodename);
   mkfilename( msfile, spooldir, msname);

   if ( (stream = FOPEN(msfile, "w", BINARY)) == nil(FILE) ) {
	  printerr("do_uuxqt", msfile);
      printmsg(0, "uux: cannot open X file %s", msfile);
      return FALSE;
   } /* if */
   fd_lock(msfile, fileno(stream));
#if 0
   fprintf(stream, "# third party request, job id\n" );
#endif
   fprintf(stream, "J %s\n",               job_name );
   fprintf(stream, "F %s/%s/%s %s\n",      spooldir, src_syst, dest_file, src_file );
   fprintf(stream, "C uucp -C %s %s!%s\n", src_file, dest_syst, dest_file );
   fclose (stream);

   return TRUE;
}

/*--------------------------------------------------------------------*/
/*    d o _ c o p y                                                   */
/*                                                                    */
/*    At this point only one of the systems can be remote and only    */
/*    1 hop away.  All the rest have been filtered out                */
/*--------------------------------------------------------------------*/

static boolean do_copy(char *src_syst,
					   char *src_file,
					   char *dest_syst,
					   char *dest_file)
{
	  char    tmfile[25];               /* Unix style name for c file */
	  char    idfile[25];       /* Unix style name for data file copy */
	  char    work[66];             /* temp area for filename hacking */
	  char    icfilename[66];               /* our hacked c file path */
	  char    idfilename[66];               /* our hacked d file path */

	  struct  stat    statbuf;

	  long    int     sequence;
	  char    *remote_syst;    /* Non-local system in copy            */
	  char    *sequence_s;
	  FILE        *cfile;

	  sequence = getseq();
	  sequence_s = JobNumber( sequence );

	  remote_syst =  equal(src_syst, nodename) ? dest_syst : src_syst;

	  sprintf(tmfile, spool_fmt, 'C', remote_syst, grade, sequence_s);
	  importpath(work, tmfile, remote_syst);
	  printmsg(6, "Create local call file %s (%s)", tmfile, work);
	  mkfilename(icfilename, spooldir, work);

	  if (!equal(src_syst, nodename))  {
		 if (expand_path(dest_file, NULL, homedir, NULL) == NULL)
			return FALSE;

		 SwapSlash(src_file);

		 printmsg(1, "uux: from \"%s\" - control = %s", src_syst,
				  tmfile);
		 if ((cfile = FOPEN(icfilename, "a", TEXT)) == nil(FILE))  {
			printerr( "do_copy", icfilename );
			printmsg(0, "dp_copy: cannot append to %s\n", icfilename);
			return FALSE;
		 }
		 fd_lock(icfilename, fileno(cfile));
		 fprintf(cfile, "R %s %s %s -c D.0 0666", src_file, dest_file,
				  mailbox);

		 if (flags[FLG_USE_USERID])
			 fprintf(cfile, " %s\n", user_id);
		 else
			 fprintf(cfile, " %s\n", mailbox);


		 fclose(cfile);
		 return TRUE;
	  } else if (!equal(dest_syst, nodename))  {

		 printmsg(1,"uux: spool %s - execute %s",
				  flags[FLG_COPY_SPOOL] ? "on" : "off",
				  flags[FLG_QUEUE_ONLY] ? "do" : "don't");
		 printmsg(1,"     - dest m/c = %s  sequence = %ld  control = %s",
				  dest_syst, sequence, tmfile);

		 if (expand_path(src_file, NULL, homedir, NULL) == NULL)
			return FALSE;

		 SwapSlash(dest_file);

		 if (stat(src_file, &statbuf) != 0)  {
			printerr( "do_copy", src_file );
			return FALSE;
		 }

		 if (statbuf.st_mode & S_IFDIR)  {
			printf("uux: directory name \"%s\" illegal\n",
					src_file );
			return FALSE;
		 }

		 if (flags[FLG_COPY_SPOOL]) {
			sprintf(idfile , dataf_fmt, 'D', nodename, subseq(), sequence_s);
			importpath(work, idfile, remote_syst);
			printmsg(6, "Spool copy %s to %s (%s)", src_file, idfile, work);
			mkfilename(idfilename, spooldir, work);

			/* Do we need a MKDIR here for the system? */

			if (!cp(src_file, FALSE, idfilename, FALSE))  {
			   printmsg(0, "copy \"%s\" to \"%s\" failed",
				  src_file, idfilename);           /* copy data */
			   return FALSE;
			}
		 }
		 else
			strcpy(idfile, "D.0");

		 if ((cfile = FOPEN(icfilename, "a", TEXT)) == nil(FILE))  {
			printerr( "do_copy", icfilename );
			printf("do_copy: cannot append to %s\n", icfilename);
			return FALSE;
		 }
		 fd_lock(icfilename, fileno(cfile));

		 fprintf(cfile, "S %s %s %s -%s %s 0666", src_file, dest_file,
				  mailbox, flags[FLG_COPY_SPOOL] ? "c" : " ", idfile);

		 if (flags[FLG_USE_USERID])
			 fprintf(cfile, " %s\n", user_id);
		 else
			 fprintf(cfile, " %s\n", mailbox);

		 fclose(cfile);

		 return TRUE;
	  }
	  else  {
		 if (expand_path(src_file, NULL, homedir, NULL) == NULL)
			return FALSE;
		 if (expand_path(dest_file, NULL, homedir, NULL) == NULL)
			return FALSE;
		 if (strcmp(src_file, dest_file) == 0)  {
			printmsg(0, "%s %s - same file; can't copy\n",
				  src_file, dest_file);
			return FALSE;
		 }
		 return(cp(src_file, FALSE, dest_file, FALSE));
	  }
}

/*--------------------------------------------------------------------*/
/*    p r e a m b l e                                                 */
/*                                                                    */
/*    write the execute file preamble based on the global flags       */
/*--------------------------------------------------------------------*/

static void preamble(FILE* stream)
{

	 fprintf(stream, "U %s %s\n", mailbox, nodename);

	 if (flags[FLG_RETURN_STDIN])
		 fprintf(stream, "B\n");

	 if (flags[FLG_NOTIFY_SUCCESS])
		 fprintf(stream, "n\n");

	 if (flags[FLG_NONOTIFY_FAIL])
		 fprintf(stream, "N\n");
#if 0
	 else
		 fprintf(stream, "Z\n");
#endif

	 if (flags[FLG_USE_EXEC])
		 fprintf(stream, "E\n");
#if 0
	 else
		 fprintf(stream, "e\n");
#endif

	 if (flags[FLG_STATUS_FILE])
		fprintf(stream, "M %s\n", st_out );

	 if (flags[FLG_USE_USERID])
		 fprintf(stream, "R %s\n", user_path );

	 if (*farg) {
		fprintf(stream, "F %s", farg);
		if (*uarg)
			fprintf(stream, " %s", uarg);
		fprintf(stream, "\n");
	 }

	 fprintf(stream, "J %s\n", job_id );

}

/*--------------------------------------------------------------------*/
/*    d o _ r e m o t e                                               */
/*                                                                    */
/*   gather data files to ship to execution system and build X file   */
/*--------------------------------------------------------------------*/

static boolean do_remote(int optind, int argc, char **argv)
{
	  FILE    *stream;           /* For writing out data              */
	  char    *sequence_s;

	  boolean s_remote;
	  boolean d_remote;
	  boolean i_remote = FALSE;
	  boolean o_remote = FALSE;

	  long    sequence;

	  char    src_system[100];
	  char    dest_system[100];
	  char    src_file[FILENAME_MAX];
	  char    dest_file[FILENAME_MAX];

	  char    command[1536];

	  char    msfile[FILENAME_MAX];    /* MS-DOS format name of files */
	  char    msname[22];              /* MS-DOS format w/o path name */

	  char    tmfile[15];        /* Call file, UNIX format name       */
	  char    lxfile[15];        /* eXecute file for remote system,
									UNIX format name for local system */
	  char    rxfile[15];        /* Remote system UNIX name of eXecute
									file                              */
	  char    lifile[15];        /* Data file, UNIX format name       */
	  char    rifile[15];        /* Data file name on remote system,
									UNIX format                       */
	  char 	  *jobid_fmt = spool_fmt + 3;
	  int nargc;
	  char    *nargv[MAXARGS];

	if (filelist) {
		char *s;

		nargc = 0;
		while (fgets(command, sizeof(command), stdin) != NULL) {
			if ((s = strpbrk(command, "\r\n")) != NULL)
				*s = '\0';
			if (strcmp(command, "<<NULL>>") == 0) {
				nargv[nargc] = NULL;
				argc = nargc;
				argv = nargv;
				filelist = FALSE;
				optind = 0;
				goto Done;
			}
			else {
				if (nargc >= MAXARGS) {
					printmsg(0, "uux: arg count > %d\n", MAXARGS);
					return FALSE;
				}
				nargv[nargc] = strdup(command);
				checkref(nargv[nargc]);
				nargc++;
			}
		}
		printmsg(0, "uux: End of arg list not found");
		return FALSE;
	}

Done:
	if (debuglevel >= 10) {
		int i;

		printmsg(10, "uux: commands list:");
		for (i = optind; i < argc; i++)
			printmsg(10, "  argv[%d] = '%s'", i, argv[i]);
	}

	  if (!split_path(argv[optind++], dest_system, command, FALSE)) {
		 printmsg(0, "uux: illegal syntax %s", argv[optind]);
		 return FALSE;
	  }

	  d_remote = equal(dest_system, nodename) ? FALSE : TRUE ;

/*--------------------------------------------------------------------*/
/*        OK - we have a destination system - do we know him?         */
/*--------------------------------------------------------------------*/

	  if ((d_remote) && (checkreal(dest_system) == BADHOST))  {
		 printmsg(0, "uux: bad dest system: %s", dest_system);
		 return FALSE;
	  }

	  printmsg(9,"xsys -> %s", dest_system);
	  printmsg(9, "system \"%s\", rest \"%s\"", dest_system, command);

	  sequence = getseq();
	  sequence_s = JobNumber( sequence );

	  sprintf(job_id, jobid_fmt, dest_system, grade, sequence_s);

/*--------------------------------------------------------------------*/
/*                     create remote X (xqt) file                     */
/*--------------------------------------------------------------------*/

	sprintf(rxfile, spool_fmt, 'X', nodename, grade, sequence_s);
	sprintf(lxfile, spool_fmt, d_remote ? 'D' : 'X', nodename, grade,
			  sequence_s);

      importpath( msname, lxfile, dest_system);
      mkfilename( msfile, spooldir, msname);

      if ( (stream = FOPEN(msfile, "w", BINARY)) == nil(FILE) ) {
		 printerr("do_remote", msfile);
		 printmsg(0, "do_remote: cannot open X file %s", msfile);
		 return FALSE;
      } /* if */
      fd_lock(msfile, fileno(stream));
      preamble(stream);

/*--------------------------------------------------------------------*/
/*                                                                    */
/*--------------------------------------------------------------------*/

	for (; optind < argc; optind++)  {

	 FileType f_remote = DATA_FILE;

         if (*argv[optind] == '-') {
	     strcat(command," ");
	     strcat(command,argv[optind]);
			 printmsg(9, "prm -> %s", argv[optind]);
	     continue;
	 } else if (equal(argv[optind], "<")) {
			 if (i_remote) {
				 printmsg(0, "uux: multiple intput files specified");
				 return FALSE;
			 } else
				 i_remote = TRUE;
			 f_remote = INPUT_FILE;
			 printmsg(9, "prm -> %s", argv[optind]);
			 optind++;
	 } else if (equal(argv[optind], ">")) {
			 if (o_remote) {
				 printmsg(0, "uux: multiple output files specified");
				 return FALSE;
			 } else
				 o_remote = TRUE;
			 f_remote = OUTPUT_FILE;
			 printmsg(9, "prm -> %s", argv[optind]);
			 optind++;
	 } else if (equal(argv[optind], "|")) {
			 strcat(command," ");
			 strcat(command,argv[optind]);
			 printmsg(9, "prm -> %s", argv[optind]);
			 continue;
	 } else if (remove_parens(argv[optind])) {
			 strcat(command," ");
			 qstrcat(command,argv[optind]);
			 printmsg(9, "prm -> %s", argv[optind]);
			 continue;
		 }

		 printmsg(9, "prm -> %s", argv[optind]);

		 if (!split_path(argv[optind], src_system, src_file, TRUE)) {
		    printmsg(0, "uux: illegal syntax %s", argv[optind]);
		    return FALSE;
		 }

		 s_remote = equal(src_system, nodename) ? FALSE : TRUE ;

/*--------------------------------------------------------------------*/
/*                   Do we know the source system?                    */
/*--------------------------------------------------------------------*/

	 if ((s_remote) && (checkreal(src_system) == BADHOST))  {
			printmsg(0, "uux: bad src system %s\n", src_system);
			return FALSE;
	 }

		if (f_remote == DATA_FILE) {
		   strcat(command, " ");
		   strcat(command, src_file);
		}

	if (f_remote == OUTPUT_FILE) {
		   fprintf(stream, "O %s %s\n", src_file,
			   (equal(src_system, dest_system) ? " " : src_system) );
		   continue;
	} else if (f_remote == INPUT_FILE)
		   fprintf(stream, "I %s\n", src_file);

/*--------------------------------------------------------------------*/
/*    if both source & dest are not the same we must copy src_file    */
/*--------------------------------------------------------------------*/

		 if (s_remote) {

			sprintf(dest_file, dataf_fmt, 'D', src_system, subseq(), sequence_s);
			printmsg(6, "Remote copy %s to %s", src_file, dest_file);
			fprintf(stream, "F %s %s\n", dest_file, src_file );

/*--------------------------------------------------------------------*/
/*      if source is remote and dest is local copy source to local    */
/*      if source is local and dest is remote copy source to remote   */
/*--------------------------------------------------------------------*/

			if ((s_remote && !d_remote) || (!s_remote && d_remote)) {
			   if (!do_copy(src_system, src_file, dest_system, dest_file))
				   return FALSE;

/*--------------------------------------------------------------------*/
/*      if both source & dest are on remote nodes we need uuxqt       */
/*--------------------------------------------------------------------*/

            } else if (s_remote && d_remote) {
	       if (!do_uuxqt(job_id, src_system, src_file, dest_system, dest_file))
		   return FALSE;
	       if (!do_copy(src_system, src_file, nodename, dest_file))
		   return FALSE;
	    }

	    continue;
	}
	printmsg(4, "system \"%s\", rest \"%s\"", src_system, src_file);
      }

/*--------------------------------------------------------------------*/
/*  Create the data file if any to send to the remote system          */
/*--------------------------------------------------------------------*/

      if (flags[FLG_READ_STDIN]) {
		if (i_remote) {
			  printmsg(0, "uux: multiple input files specified");
		return FALSE;
       }

		  sprintf(rifile, dataf_fmt, 'D', nodename, subseq(), sequence_s);
		  if (!d_remote)
			strcpy(lifile,rifile);
		  else
			sprintf(lifile, dataf_fmt, 'D', nodename, subseq(), sequence_s);
		  importpath(msname, lifile, dest_system);
		  printmsg(6, "Copy stdin to %s (%s)", lifile, msname);
		  mkfilename(msfile, spooldir, msname);

	  if (!CopyData( nil(char), msfile )) {
	      remove( msfile );
	      return FALSE;
	  }

	  fprintf(stream, "F %s\n", rifile);
		  fprintf(stream, "I %s\n", rifile);
      }

/*--------------------------------------------------------------------*/
/*           here finish writing parameters to the X file             */
/*--------------------------------------------------------------------*/

	  printmsg(4, "command \"%s\"", command);


	  fprintf(stream, "C %s\n", command);
	  fclose(stream);

/*--------------------------------------------------------------------*/
/*                     create local C (call) file                     */
/*--------------------------------------------------------------------*/

	 if (d_remote) {
		  sprintf(tmfile, spool_fmt, 'C', dest_system,  grade, sequence_s);
		  importpath( msname, tmfile, dest_system);
		  printmsg(6, "Create local call file %s (%s)", tmfile, msname);
		  mkfilename( msfile, spooldir, msname);

		  if ( (stream = FOPEN(msfile, "w", TEXT)) == nil(FILE)) {
			 printerr( "do_remote", msname );
			 printmsg(0, "do_remote: cannot open C file %s", msfile);
			 return FALSE;
		  }
		  fd_lock(msfile, fileno(stream));

          /* copy darg to start of C-file */
          if (*darg) {
              FILE * fdarg = FOPEN(darg, "r", TEXT);
              if (fdarg == nil(FILE)) {
                  printerr( "do_remote", darg );
                  printmsg(0, "do_remote: cannot open %s", darg);
                  return FALSE;
              }
              while (fgets(buf, sizeof(buf), fdarg))
                  fputs(buf, stream);
              fclose(fdarg);
              unlink(darg);
          }

		  /* send D. file first */
		  if (flags[FLG_READ_STDIN])
			  fprintf(stream, send_cmd, lifile, rifile, mailbox, lifile);

		  /* then send X. file */
		  fprintf(stream, send_cmd, lxfile, rxfile, mailbox, lxfile);

		  fclose(stream);
	 }
	 return TRUE;
}

void set_user_id(void)
{
	char *p;

	flags[FLG_USE_USERID] = TRUE;
	if ((p = strchr(user_path, '@')) != NULL) {
		*p = '\0';
		user_id = strdup(user_path);
		*p = '%';
		checkref(user_id);
	}
	else if ((p = strrchr(user_path, '!')) != NULL)
		user_id = p + 1;
	else
		user_id = user_path;
}

/*--------------------------------------------------------------------*/
/*    m a i n                                                         */
/*                                                                    */
/*    main program                                                    */
/*--------------------------------------------------------------------*/

int main(int  argc, char  **argv)
{
   int         c;
   extern char *optarg;
   extern int   optind;
   char    	buf[BUFSIZ];

/*--------------------------------------------------------------------*/
/*     Report our version number and date/time compiled               */
/*--------------------------------------------------------------------*/

   debuglevel = 0;

   close_junk_files();

   banner( argv );

#if defined(__CORE__)
   copywrong = strdup(copyright);
   checkref(copywrong);
#endif

   if (!configure())
	return 4;   /* system configuration failed */
   if (!configure_swap())
	return 4;

   setcbrk(0);
#ifdef __MSDOS__
   ctrlbrk(null);
#endif

/*--------------------------------------------------------------------*/
/*        Process our arguments                                       */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*
 *
 *   -aname    Use name as the user identification replacing the initiator
 *   -b        Return whatever standard input was provided to the uux command
 *   -c        Do not copy local file to the spool directory for transfer to
 *   -C        Force the copy of local files to the spool directory for
 *   -E        run job using exec
 *   -e        run job using sh
 *   -ggrade   Grade is a single letter number; lower ASCII sequence
 *   -j        Output the jobid ASCII string on the standard output which is
 *   -n        Do not notify the user if the command fails.
 *   -p        Same as -:  The standard input to uux is made the standard
 *   -r        Do not start the file transfer, just queue the job.
 *   -sfile    Report status of the transfer in file.
 *   -xdebug_level
 *   -z        Send success notification to the user.
 *
 *--------------------------------------------------------------------*/

   while ((c = getopt(argc, argv, "-a:bcCD:EeF:jg:lnprs:U:x:X:zZ")) !=  EOF)
	  switch(c) {
	  case 'l':
		 filelist = TRUE;
		 break;
	  case '-':
	  case 'p':
		 flags[FLG_READ_STDIN] = TRUE;
		 break;
	  case 'a':
		 user_path = optarg;
		 set_user_id();
		 break;
	  case 'b':
		 flags[FLG_RETURN_STDIN] = TRUE;
		 break;
	  case 'c':               /* don't spool */
		 flags[FLG_COPY_SPOOL] = FALSE;
		 break;
	  case 'C':               /* force spool */
		 flags[FLG_COPY_SPOOL] = TRUE;
		 break;
	  case 'E':               /* use exec to execute */
		 flags[FLG_USE_EXEC] = TRUE;
		 break;
	  case 'e':               /* use sh to execute */
		 flags[FLG_USE_EXEC] = FALSE;
		 break;
	  case 'j':               /* output job id to stdout */
		 flags[FLG_OUTPUT_JOBID] = TRUE;
		 break;
	  case 'n':               /* do not notify user if command fails */
		 flags[FLG_NONOTIFY_FAIL] = TRUE;
		 break;
	  case 'r':               /* queue job only */
		 flags[FLG_QUEUE_ONLY] = TRUE;
		 break;
	  case 'z':
		 flags[FLG_NOTIFY_SUCCESS] = TRUE;
		 break;
	  case 'g':               /* set grade of transfer */
		 grade = *optarg;
		 break;
	  case 's':               /* report status of transfer to file */
		 flags[FLG_STATUS_FILE] = TRUE;
		 st_out = optarg;
		 break;
	  case 'X':
		 logecho = TRUE;
	  case 'x':
		 debuglevel = atoi( optarg );
		 break;
	  case 'U':
		 if (!*farg) {
			fprintf(stderr, "uux: -U needs -F first\n");
			usage();
		 }
		 strcpy(uarg, optarg);
		 break;
	  case 'F':
		 strcpy(farg, optarg);
		 break;
	  case 'Z':
		 if (!logecho)
			 visual_output = TRUE;
		 break;
	  case 'D':
		 mkfilename(darg, tempdir, optarg);
		 break;
	  default:
		 fprintf(stderr, "uux: bad argument from getopt \"%c\"\n", c);
	  case '?':
		 usage();
   }

   if (   (!filelist && argc - optind < 1)
	   || (filelist && argc - optind != 0)
	  )
	  usage();

	mkfilename(buf, spooldir, "uux.log");
	if (debuglevel > 0 && logecho &&
			(logfile = FOPEN(buf, "a", TEXT)) == nil(FILE)) {
		logfile = stdout;
		printerr("uux", buf);
	}
	if (logfile != stdout)
		fprintf(logfile, "\n>>>> Log level %d started %s\n", debuglevel, arpadate());
	if (filelist)
		printmsg(7, "main: arguments passes via stdin (-l)");
	else
		printmsg(7, "main: arguments passed via command line (128 limit)");

	setmode(fileno(stdin), O_BINARY);   /* Don't die on control-Z, etc */

	if (filelist) {
		char *s;

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			printerr("uux", "stdin");
			printmsg(0, "uux: can't read stdin arg list (-l)");
			return 1;
		}
		if ((s = strpbrk(buf, "\r\n")) != NULL)
			*s = '\0';
		user_path = strdup(buf);
		checkref(user_path);
		set_user_id();
	}

	if (!user_id || !*user_id)
		user_id = mailbox;
	if (!user_path || !*user_path)
		user_path = user_id;

	printmsg(10, "main: user_path %s", user_path);
	printmsg(10, "main: user_id %s", user_id);

/*--------------------------------------------------------------------*/
/*                   Switch to the spool directory                    */
/*--------------------------------------------------------------------*/

   PushDir( spooldir );
   atexit( PopDir );

   if (!do_remote(optind, argc, argv)) {
	  printmsg(0, "uux command failed");
	  return 1;
   };
   if (flags[FLG_OUTPUT_JOBID])
	   printf("%s\n", job_id);
   if (!flags[FLG_QUEUE_ONLY])
#ifdef __MSDOS__
	 (void) swap_system("uupc.bat");
#else
	 system("uucp.cmd");
#endif

   return 0;
} /* main */

/*--------------------------------------------------------------------*/
/*    s u b s e  q                                                    */
/*                                                                    */
/*    Generate a valid sub-sequence number                            */
/*--------------------------------------------------------------------*/

static char subseq( void )
{
   static char next = '0' - 1;

   do {
	   switch( next )
	   {
		  case '9':
			 next = 'A';
			 break;

		  case 'Z':
			 next = 'a';
			 break;

		  default:
			 next++;
	   } /* switch */
   } while (next == grade);

   return next;

} /* subseq */
