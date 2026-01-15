/*
          Changes (from uux.c) Copyright (c) 1996 by Pavel Gulchouck

	  Date:       20 August 1991
	  Author:     Mitch Mitchell
	  Email:      mitch@harlie.lonestar.org

	  Much of this code is shamelessly taken from extant code in
	  UUPC/Extended.
*/

#include <stdio.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dos.h>
#include <process.h>
#include <ctype.h>
#include <stdlib.h>
#ifndef __TURBOC__
#include <signal.h>
#endif
#ifdef __OS2__
#define INCL_DOSPROCESS
#include <os2.h>
#endif

#define DO_CHECK   0
#define BUFSIZE    BUFSIZ

/*--------------------------------------------------------------------*/
/*         Local include files                                        */
/*--------------------------------------------------------------------*/

#include  "lib.h"
#include  "hlib.h"
#include  "getopt.h"
#include  "expath.h"
#include  "arpadate.h"
#include  "import.h"
#include  "pushpop.h"
#include  "hostable.h"
#include  "timestmp.h"
#include  "lock.h"
#include  "swap.h"
#include  "ssleep.h"

currentfile();

#define updatecrc(c,crc) crc=((crc>>1)|(crc<<15))+c

#define MAXRBSIZE  (40*1024l)
#define PKTFMT     "%s%s\\batch\\rbmail.pk"
#define ZPKTFMT    "%s%s\\batch\\rbmail.pkz"
#define NEWSFMT    "%s%s\\batch\\rnews.pk"
#define ZNEWSFMT   "%s%s\\batch\\rnews.pkz"

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

typedef enum { DO_RBMAIL, DO_NEWS } task_type;

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
static char grade = 'N';		/* Default grade of service */
static long maxrbsize = 0;
static char rbname[FILENAME_MAX];	/* MS-DOS format name of files */
static char cmdline[BUFSIZ];
static char buf[BUFSIZ];
static long crc_pos = -1;
static FILE *stream;           /* For writing out data              */

boolean filelist = FALSE, do_compress = TRUE, gen_rzbmail = FALSE;
extern boolean visual_output;
extern char * compress;
extern char * calldir;
extern int  screen;

/*--------------------------------------------------------------------*/
/*                        Internal prototypes                         */
/*--------------------------------------------------------------------*/

static void usage( void );
static boolean split_path(char *path, char *system, char *file);
static boolean do_remote(int optind, int argc, char **argv);

/*--------------------------------------------------------------------*/
/*    u s a g e                                                       */
/*                                                                    */
/*    Report flags used by program                                    */
/*--------------------------------------------------------------------*/

static void usage()
{
	fprintf(stderr, "Usage: batch [-c|-C] [-e|-E] [-b] [-gGRADE] [-G] [-p] [-j] [-n] [-r] [-sFILE]\\\n\
\t[-S Size] [-aNAME] [-z] [-] [-x|-X DEBUG_LEVEL] [-Z] [-H] [-l|command-string]\n");
	fprintf(stderr, "Flush: batch -B [-S Size] [-G] [-H] system\n");
	fprintf(stderr, "\tdon't forget to use '(' and ')' for any parameter\n\
\twhich is not a valid filename or for unknown 'system!' prefix\n");
	exit(4);
}

int isfile(int h)
{
#ifdef __MSDOS__
	if (ioctl(h,0) & 0xA0)
#elif defined (__OS2__)
	unsigned long l,pipetype;
	DosQueryHType(h,&pipetype,&l);
	if (pipetype!=0)
#endif
		return 0;
	return 1;
}

#ifdef __MSDOS__
static int null(void) {return 1;} /* at ctrlbrk */

int atkill(void)
#else
#pragma off (unreferenced);
void atkill(int i)
#pragma on (unreferenced);
#endif
{
	if ((crc_pos != -1) && (stream != NULL)) {
		fseek(stream, crc_pos, SEEK_SET);
		chsize(fileno(stream), crc_pos);
		fclose(stream);
	}
	exit(100);
#ifdef __MSDOS__
	return 0;
#endif
}

#if DO_CHECK
static boolean check_batch(char * fname)
{	FILE * f;
	long l;
	char str[128];
	unsigned short crc, calccrc;
	int  c, i;

	f = fopen(fname, "rb");
	if (f == nil(FILE))
		goto badbatch;
	for (;;) {
		/* loop by messages */
		if (fgets(str,sizeof(str),f) == NULL) {
			fclose(f);
			return 0;
		}
		l=0;
		for (i=0;i<7;i++) {
			if (!isdigit(str[i]))
				goto fbadbatch;
			l=l*10+str[i]-'0';
		}
		if (str[7]!=' ')
			goto fbadbatch;
		crc=0;
		for (i=8;i<12;i++) {
			if (!isxdigit(str[i]))
				goto fbadbatch;
			crc=crc*16+tolower(str[i])-'0';
			if (!isdigit(str[i]))
				crc=crc+'0'+10-'a';
		}
		if (str[12]!=' ')
			goto fbadbatch;
		while (!strchr(str,'\n'))
			if (fgets(str,sizeof(str),f)==NULL)
				goto fbadbatch;
		calccrc=0;
		for (;l>0;l--) {
			c=fgetc(f);
			if (c==EOF)
				goto fbadbatch;
			updatecrc(c,calccrc);
		}
		if (crc!=calccrc) {
fbadbatch:
			fclose(f);
badbatch:
			for (i=0;i<1000;i++) {
				sprintf(str,"%sbad.job\\rbmail.%03d",spooldir,i);
				if (access(str,0))
					break;
			}
			rename(fname,str);
			return 8;
		}
	}
}
#endif

static int do_uux(char ** argv, char ** nargv, char * line)
{	int i,j;

	mkfilename(cmdline,calldir,"uux.exe");
	argv[0]=cmdline;
	/* remove -S and -G switches */
	for (i=1; argv[i]!=NULL; i++) {
		if ((strcmp(argv[i], "-G")==0) || (strcmp(argv[i], "-H")==0))
			for (j=i--; argv[j]!=NULL; j++)
				argv[j]=argv[j+1];
		else if (strcmp(argv[i], "-S")==0)
			for (j=i--; argv[j]!=NULL; j++)
				argv[j]=argv[j+2];
		else if (strncmp(argv[i], "-S", 2)==0)
			for (j=i--; argv[j]!=NULL; j++)
				argv[j]=argv[j+1];
	}
	if ((filelist && nargv) || line)
#ifndef __MSDOS__
		if (!isfile(fileno(stdin))) {
			int  uux_pid, hin;
			FILE *fout;

			printmsg(6,"batch: running uux");
			for (i=1; argv[i]!=NULL; i++) {
				strcat(cmdline, " ");
				strcat(cmdline, argv[i]);
			}
			uux_pid = pipe_system(&hin, NULL, cmdline);
			if (uux_pid == -1) {
				printmsg(0, "batch: Can't run uux");
				return FALSE;
			}
			setmode(hin, O_BINARY);
			fout = fdopen(hin, "wb");
			if (filelist) {
				fprintf(fout, "%s\n", buf);
				for (i = 0; nargv[i]; i++)
					fprintf(fout, "%s\n", nargv[i]);
				fprintf(fout, "<<NULL>>\n");
			}
			if (line)
				fprintf(fout, line);
			while ((i = fgetc(stdin)) != EOF)
				fputc(i, fout);
			fclose(fout);
			cwait(&i, uux_pid, WAIT_CHILD);
			printmsg( i ? 4 : 7, "uux retcode %d", i);
			return i ? FALSE : TRUE;
		} else
#endif
		{
			fseek(stdin, 0, SEEK_SET);
			fflush(stdin);
		}
	printmsg(6, "batch: running uux");
	flags[FLG_QUEUE_ONLY]=TRUE;
	i = spawnv(P_WAIT,cmdline,argv);
	printmsg( i ? 4 : 7, "uux retcode %d", i);
	return i ? FALSE : TRUE;
}

static int flush(char * remote, task_type task)
{
	int ret;
	char remotedir[8];
	char *pktfmt, *zpktfmt;

	printmsg(9, "Flush %s %s", remote,
	         (task == DO_RBMAIL) ? "rbmail" : "rnews");
	strncpy(remotedir, remote, 7);
	remotedir[7] = '\0';
	if (task == DO_RBMAIL) {
		pktfmt = PKTFMT;
		zpktfmt = ZPKTFMT;
	} else {
		pktfmt = NEWSFMT;
		zpktfmt = ZNEWSFMT;
	}
	sprintf(rbname, pktfmt, spooldir, remotedir);
	if (access(rbname, 0)) {
		printmsg(9, "Flush: no batchmail-packet found, do nothing");
		return 0;
	}
#if DO_CHECK
	if (task == DO_RBMAIL)
		if (check_batch(rbname)) {
			printmsg(0, "Batch packet corrupted!");
			return 9;
		}
#endif
	/* compress */
	if (!compress)
		do_compress = FALSE;
#ifdef __MSDOS__
	if (do_compress) {
		int  h, saveout;
		static char rcbname[FILENAME_MAX];

		sprintf(rcbname, zpktfmt, spooldir, remotedir);
		h = open(rcbname, O_BINARY|O_RDWR|O_CREAT, S_IWRITE);
		if (h == -1) {
			printmsg(0, "Can't create %s: %s", rcbname, sys_errlist[errno]);
			do_compress = FALSE;
		} else {
			if (task == DO_NEWS)
				write(h, "#! cunbatch\n", 12);
			saveout = dup(fileno(stdout));
			dup2(h, fileno(stdout));
			close(h);
			sprintf(cmdline, compress, "");
			sprintf(cmdline + strlen(cmdline), " < %s", rbname);
			printmsg(7, "Executing %s", cmdline);
			ret = swap_system(cmdline);
			dup2(saveout, fileno(stdout));
			close(saveout);
			printmsg(ret ? 0 : 8, "Compress retcode %d", ret);
			if (ret) {
				do_compress = FALSE;
				sprintf(cmdline, zpktfmt, spooldir, remotedir);
				unlink(cmdline);
			} else {
				unlink(rbname);
				strcpy(rbname, rcbname);
			}
		}
	}
#endif

	mkfilename(cmdline, calldir, "uux.exe");
	sprintf(cmdline+strlen(cmdline), " -r -p -g%c -%c%d %s!%s",
		grade,
		logecho ? 'X' : 'x',
		debuglevel,
		remote,
		(task == DO_NEWS) ? "rnews" :
		(do_compress ? (gen_rzbmail ? "rzbmail" : "rcbmail") : "rbmail"));
#ifndef __MSDOS__
	if (!do_compress)
#endif
	{
		strcat(cmdline," <");
		strcat(cmdline, rbname);
		printmsg(5, "xcmd: %s", cmdline);
		ret = swap_system(cmdline);
		printmsg(ret ? 0 : 7, "uux retcode %d", ret);
	}
#ifndef __MSDOS__
	else {
		int r, uux_pid, hin, saveout;

		printmsg(5, "xcmd: %s", cmdline);
		uux_pid=pipe_system(&hin,NULL,cmdline);
		if (uux_pid==-1) {
			printmsg(0,"Can't execute uux (%s)",cmdline);
			ret=-1;
		} else {
			fflush(stdout);
			saveout=dup(fileno(stdout));
			setmode(hin,O_BINARY);
			dup2(hin,fileno(stdout));
			if (task==DO_NEWS)
				write(hin,"#! cunbatch\n",12);
			close(hin);
			sprintf(cmdline,compress,"");
			strcat(cmdline," <");
			strcat(cmdline,rbname);
			printmsg(5,"xcmd: %s",cmdline);
			ret=swap_system(cmdline);
			dup2(saveout,fileno(stdout));
			close(saveout);
			printmsg(ret ? 0 : 7, "Compress retcode %d", ret);
			if (ret==0) {
				cwait(&ret,uux_pid,WAIT_CHILD);
				ret&=0xffff;
				ret=((ret>>8) | (ret<<8)) & 0xffff;
				printmsg(ret ? 0 : 7, "uux retcode %d", ret);
			} else {
				DosKillProcess(DKP_PROCESSTREE,uux_pid);
				cwait(&r,uux_pid,WAIT_CHILD);
			}
		}
 	}
#endif
	if (ret==0)
		unlink(rbname);
	return ret;
}

static boolean LockBatch(char * system, int LockTask)
{
#define SLEEP_INTERVAL    10
#define OLD_LOCK_TIME     (60*10)

	int trycnt=0;

	printmsg(7, "Locking system %s", system);
	while (!LockSystem(system, LockTask)) {
		if (trycnt++ >= OLD_LOCK_TIME/SLEEP_INTERVAL) {
			printmsg(0, "Can't lock system %s", system);
			return FALSE;
		}
		ssleep(SLEEP_INTERVAL);
	}
	return TRUE;
}

/*--------------------------------------------------------------------*/
/*    s p l i t _ p a t h                                             */
/*--------------------------------------------------------------------*/

static boolean split_path(char *path,
                          char *sysname,
                          char *file)
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
                                       /* not a remote path */
		return FALSE;

	p_right = strrchr(p, '!');      /* look for the last bang */
	strcpy(file, p_right + 1);      /* and thats our filename */

	strncpy(sysname, p, p_left - p); /* and we have a system thats not us */
	sysname[p_left - p] = '\0';

	/* now see if there is an intermediate path */

	if (p_left != p_right)  {        /* yup - there is */
		return FALSE;
	}
	printmsg(9, "split_path: %s parsed as %s at %s", path, file, sysname);

	return TRUE;                     /* and we're done */
}

static char * remove_parens(char *string)
{
	int len = strlen(string);

	if ((string[0] != '(') || (string[len - 1] != ')'))
		return string;

	strcpy(string, &string[1]);
	string[len - 2] = '\0';
	return string;                 /* and we're done */
}


/*--------------------------------------------------------------------*/
/*    d o _ r e m o t e                                               */
/*                                                                    */
/*   gather data files to ship to execution system and build X file   */
/*--------------------------------------------------------------------*/

static boolean do_remote(int optind, int argc, char **argv)
{
	char    dest_system[100];
        char    remotedir[8];
	char    command[1536];
	int     nargc, rargc, ret=0;
	char    *nargv[MAXARGS];
	char	**rargv;
	unsigned short crc;
	long    len;
	int     c;
	task_type task;

	rargv = argv;
	rargc = argc;
	if (filelist) {
		char *s;

		nargc = 0;
		while (fgets(command, sizeof(command), stdin) != NULL) {
			if ((s = strpbrk(command, "\r\n")) != NULL)
				*s = '\0';
			if (strcmp(command, "<<NULL>>") == 0) {
				nargv[nargc] = NULL;
				optind = 0;
				rargv = nargv;
				rargc = nargc;
				goto Done;
			}
			else {
				if (nargc >= MAXARGS) {
					printmsg(0, "batch: arg count > %d\n", MAXARGS);
					return FALSE;
				}
				nargv[nargc] = strdup(command);
				checkref(nargv[nargc]);
				nargc++;
			}
		}
		printmsg(0, "batch: End of arg list not found");
		return FALSE;
	}

Done:
	if (debuglevel >= 10) {
		int i;

		printmsg(10, "batch: commands list:");
		for (i = optind; i < rargc; i++)
			printmsg(10, "  argv[%d] = '%s'", i, rargv[i]);
	}

	if (!split_path(rargv[optind++], dest_system, command))
		command[0] = '\0';

	if ((strcmp(command, "rmail")==0) || (strncmp(command,"rmail ",6)==0))
		task = DO_RBMAIL;
	else if (strcmp(command, "rnews")==0)
		task = DO_NEWS;
	else
		/* Not for us, running uux */
		return do_uux(argv, nargv, NULL);

	if (equal(dest_system, nodename))
		return do_uux(argv, nargv, NULL);

/*--------------------------------------------------------------------*/
/*        OK - we have a destination system - do we know him?         */
/*--------------------------------------------------------------------*/

	if (checkreal(dest_system) == BADHOST)  {
		printmsg(0, "batch: bad dest system: %s", dest_system);
		return FALSE;
	}

	printmsg(9,"xsys -> %s", dest_system);
	printmsg(9, "system \"%s\", rest \"%s\"", dest_system, command);

	if (!LockBatch(dest_system, (task == DO_RBMAIL) ? B_BATCH : B_NEWS))
		return FALSE;

	strncpy(remotedir, dest_system, 7);
	remotedir[7] = '\0';
	if (task == DO_RBMAIL)
		sprintf(rbname, PKTFMT, spooldir, remotedir);
	else /* DO_NEWS */
		sprintf(rbname, NEWSFMT, spooldir, remotedir);

	if (access(rbname, 0))
		stream = FOPEN(rbname, "w", BINARY);
	else
		stream = FOPEN(rbname, "r+", BINARY);
	if ( stream == nil(FILE) ) {
		printerr("do_remote", rbname);
		printmsg(0, "do_remote: cannot open rbmail file %s", rbname);
		UnlockSystem();
		return FALSE;
	} /* if */
#ifdef __TURBOC__
	ctrlbrk(atkill);
#else
	signal(SIGTERM,  atkill);
	signal(SIGINT,   atkill);
	signal(SIGBREAK, atkill);
#endif
	fseek(stream, 0, SEEK_END);

/*--------------------------------------------------------------------*/
/*                                                                    */
/*--------------------------------------------------------------------*/

	crc_pos = ftell(stream);
	if (task==DO_RBMAIL) {
		fprintf(stream, "0000000 0000");

		for (; optind < rargc; optind++)
			fprintf(stream, " %s", remove_parens(rargv[optind]));
		fprintf(stream, "\n");

		len = crc = 0;

		while ((c=fgetc(stdin)) != EOF) {
			crc = updatecrc(c, crc);
			len++;
			if (fputc(c, stream) == EOF)
				goto errwrite;
		}

		fseek(stream, crc_pos, SEEK_SET);
		fprintf(stream, "%07ld %04x", len, crc);
	} else { /* rnews */
		static char str[1024];
		fgets(str,sizeof(str),stdin);
		if (strncmp(str,"#!",2)) {
#ifndef __MSDOS__
			if (!isfile(fileno(stdin))) {
				int  curbufsize;
				long ibuf;
				char * msgbuf;

				curbufsize=BUFSIZE;
				msgbuf=malloc(curbufsize);
				ibuf=0;
				while (msgbuf) {
					if ((c=fgetc(stdin))==EOF)	
						break;
					msgbuf[ibuf++]=(char)c;
					if (ibuf==curbufsize)
						msgbuf=realloc(msgbuf,curbufsize+=BUFSIZE);
				}
				if (msgbuf==NULL) {
					fclose(stream);
					printmsg(0, "Error: Can't write to file: %s!", sys_errlist[ret]);
					UnlockSystem();
					return FALSE;
				}
				fprintf(stream, "#! rnews %lu\n", ibuf+strlen(str));
				fputs(str, stream);
				if (fwrite(msgbuf, ibuf, 1, stream)==EOF)
					goto errwrite;
				free(msgbuf);
			} else
#endif
			{
				fprintf(stream,"#! rnews %lu\n",
            				filelength(fileno(stdin))-ftell(stdin)+strlen(str));
				fputs(str, stream);
				while ((c=fgetc(stdin)) != EOF)
					if (fputc(c, stream) == EOF)
						goto errwrite;
			}
		} else {
			char *p;

			for(p=str+2;isspace(*p);p++);
			if (strncmp(p,"rnews",5)) {
				if (filelength(fileno(stream))==0) {
					fclose(stream);
					unlink(rbname);
				} else
					fclose(stream);
				UnlockSystem();
				return do_uux(argv, nargv, str);
			}
			fputs(str, stream);
			while ((c=fgetc(stdin)) != EOF) {
				if (fputc(c, stream) == EOF) {
errwrite:
					ret=errno;
					fseek(stream, crc_pos, SEEK_SET);
					chsize(fileno(stream), crc_pos);
					fclose(stream);
					printmsg(0, "Error: Can't write to file: %s!", sys_errlist[ret]);
					UnlockSystem();
					return FALSE;
				}
			}
		}
	}	

	len=filelength(fileno(stream));
	fclose(stream);
	stream = NULL;
	if (len >= maxrbsize) {
		printmsg(5, "Batchmail packet size %ld, exec flush\n", len);
		ret = flush(dest_system, task);
	}
	else
		ret = 0;

	UnlockSystem();

	return (ret == 0);
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
	extern int  optind;
	boolean     do_flush=FALSE, via_uux = FALSE;
	int         ret;

/*--------------------------------------------------------------------*/
/*     Report our version number and date/time compiled               */
/*--------------------------------------------------------------------*/

	debuglevel = 0;

	close_junk_files();

	banner(argv);

	if (!configure())
		return 4;   /* system configuration failed */
	if (!configure_swap())
		return 4;
	if (spooldir[strlen(spooldir)-1] != '\\')
		strcat(spooldir, "\\");

	setcbrk(0);
#ifdef __TURBOC__
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

	while ((c = getopt(argc, argv, "-a:bcCD:EeF:jg:lnprs:S:U:x:X:zZBGH")) !=  EOF)
		switch(c) {
			case 'l':
				filelist = TRUE;
				break;
			case '-':
			case 'p':
				flags[FLG_READ_STDIN] = TRUE;
				break;
			case 'c':               /* don't spool */
			case 'C':               /* force spool */
			case 'E':               /* use exec to execute */
			case 'e':               /* use sh to execute */
			case 'j':               /* output job id to stdout */
				break;
			case 'r':               /* queue job only */
				flags[FLG_QUEUE_ONLY] = TRUE;
				break;
			case 'g':               /* set grade of transfer */
				grade = *optarg;
				break;
			case 'X':
				logecho = TRUE;
			case 'x':
				if (debuglevel == 0)
					debuglevel = atoi( optarg );
				break;
			case 'b':
			case 'n':               /* do not notify user if command fails */
			case 's':               /* report status of transfer to file */
			case 'z':
			case 'U':
			case 'F':
			case 'D':
				via_uux=TRUE;
				break;
			case 'Z':
				if (!logecho)
				visual_output = TRUE;
				break;
			case 'B':
				do_flush = TRUE;
				break;
			case 'S':
				maxrbsize = atol(optarg)*1024l;
				break;
			case 'G':
				do_compress = FALSE;
				break;
			case 'H':
				gen_rzbmail = TRUE;
				break;
			default:
				fprintf(stderr, "uux: bad argument from getopt \"%c\"\n", c);
			case '?':
				usage();
		}

	if ((!filelist && argc - optind < 1)
	   || (filelist && argc - optind != 0))
		usage();

	if (!flags[FLG_READ_STDIN] && !do_flush)
		via_uux=TRUE;
#ifdef __MSDOS__
	if (filelist)
		if (!isfile(fileno(stdin)))
			/* device, not disk file - can't seek to start */
			via_uux=TRUE;
#endif

	if (maxrbsize == 0)
		maxrbsize = MAXRBSIZE;

	mkfilename(buf, spooldir, "batch.log");
	if (debuglevel > 0 && logecho &&
			(logfile = FOPEN(buf, "a", TEXT)) == nil(FILE)) {
		logfile = stdout;
		printerr("batch", buf);
	}
	if (logfile != stdout)
		fprintf(logfile, "\n>>>> Log level %d started %s\n", debuglevel, arpadate());
	if (filelist)
		printmsg(7, "main: arguments passes via stdin (-l)");
	else
		printmsg(7, "main: arguments passed via command line"
#ifdef __MSDOS__ 
                            " (128 limit)"
#endif
                                                                     );

	if (via_uux) {
		printmsg(5, "Starting uux");
		return do_uux(argv,NULL,NULL);
	}

	if (do_flush) {
		if (!LockBatch(argv[optind], B_BATCH))
			return 10;
		printmsg(4, "Do flush");
		ret = flush(argv[optind], DO_RBMAIL);
		UnlockSystem();
		if (ret==0) {
			if (!LockBatch(argv[optind], B_NEWS))
				return 10;
			ret = flush(argv[optind], DO_NEWS);
		}
		UnlockSystem();
		return ret;
	}

	setmode(fileno(stdin), O_BINARY);   /* Don't die on control-Z, etc */

	if (filelist) {
		char *s;

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			printerr("batch", "stdin");
			printmsg(0, "batch: can't read stdin arg list (-l)");
			return 1;
		}
		if ((s = strpbrk(buf, "\r\n")) != NULL)
			*s = '\0';
	}

/*--------------------------------------------------------------------*/
/*                   Switch to the spool directory                    */
/*--------------------------------------------------------------------*/

	PushDir( spooldir );
	atexit( PopDir );

	if (!do_remote(optind, argc, argv)) {
		printmsg(0, "batch command failed");
		return 1;
	};
	if (!flags[FLG_QUEUE_ONLY]) {
		printmsg(5, "Executing uupc");
#ifdef __MSDOS__
		(void) swap_system("uupc.bat");
#else
		system("uupc.cmd");
#endif
	}
	return 0;
} /* main */
