/*
 * Changes Copyright (C) 1991-96 by Andrey A. Chernov, Moscow, Russia.
 *
 * History:4,1
 * Mon May 15 19:56:44 1989 Add c_break handler                   ahd
 * 20 Sep 1989 Add check for SYSDEBUG in MS-DOS environment       ahd
 * 22 Sep 1989 Delete kermit and password environment
			   variables (now in password file).                  ahd

   IBM-PC host program
*/

# include	<stdio.h>
# include	<setjmp.h>
# include	<errno.h>
# include	<malloc.h>
# include	<fcntl.h>
# include	<conio.h>
# include	<signal.h>
# include	<dos.h>
# include	<string.h>
# include	<stdlib.h>

# include	"lib.h"
# include	"hlib.h"
# include	"timestmp.h"
# include 	"arpadate.h"
# include	"pcmail.h"
# include	"pushpop.h"
# include	"getopt.h"
# include	"swap.h"
# include	"ssleep.h"

unsigned _stklen = 0x4000;

currentfile();

static char *receipt = NULL;
char *GradeList = NULL;
static char *Bodyreturn = NULL;
boolean returnsenderbody;
boolean doreceipt = FALSE;
static char *do_MIME = NULL;

static struct Table table[] = {/*    must    sys   dir   suff */
	{"RETURNBODY",  &Bodyreturn, 	TRUE,	TRUE,  FALSE, NULL},
	{"RECEIPT",	&receipt,    	TRUE,	TRUE,  FALSE, NULL},
	{"GRADES",	&GradeList,  	FALSE,	TRUE,  FALSE, NULL},
	{"CHARSET1",    &charset[0],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET2",    &charset[1],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET3",    &charset[2],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET4",    &charset[3],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET5",    &charset[4],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET6",    &charset[5],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET7",    &charset[6],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET8",    &charset[7],	FALSE,  FALSE, FALSE, NULL},
	{"CHARSET9",    &charset[8],	FALSE,  FALSE, FALSE, NULL},
	{"8BIT-HEADER", &do_MIME,	FALSE,	FALSE, FALSE, NULL},
	{nil(char), }
}; /* table */

extern int lmail(int argc, char *argv[]);
extern int init_charset(boolean);

char*	replyto;
extern boolean force_redirect;
#ifdef __MSDOS__
int	c_break(void);
int null(void) { return 1; }
#else
void	c_break(int);
#endif
boolean 	filelist = FALSE;
int nargc = 0;
char *nargv[MAXARGS];
extern FILE *input_source;
extern char *input_name;

int main(int argc, char** argv) {
	FILE *fp;
	char buf[FILENAME_MAX];
	char **sargv = argv;

	force_redirect = TRUE;

	close_junk_files();

	if (!parse_args(argc,argv))
		return 4;

	if (ignsigs) {
		setcbrk(0);
#ifdef	__MSDOS__
		ctrlbrk(null);
#endif
	}
	else {
#ifdef	__MSDOS__
		setcbrk(1);
		ctrlbrk(c_break);
#else
		signal(SIGINT,  c_break);
		signal(SIGTERM, c_break);
		signal(SIGBREAK,c_break);
#endif
	}

	if (setvbuf(input_source, NULL, _IOFBF, xfer_bufsize)) {
		perror(input_name);
		fprintf(stderr, "Can't buffer input file\n");
		return 1;
	}

	if (filelist) {
		static char buf[1024+2], *s;

		while (fgets(buf, sizeof(buf) - 1, input_source) != NULL) {
			buf[sizeof(buf) - 1] = '\0';
			if ((s = strchr(buf, '\n')) != NULL)
				*s = '\0';
			if (strcmp(buf, "<<NULL>>") == 0) {
				nargv[nargc] = NULL;
				argc = nargc;
				argv = nargv;
				init_getopt();
				if (!parse_args(argc, argv))
					return 4;
				goto Done;
			}
			else {
				if (nargc >= MAXARGS) {
					fprintf(stderr, "arg count > %d\n", MAXARGS);
					return 4;
				}
				nargv[nargc] = strdup(buf);
				checkref(nargv[nargc]);
				nargc++;
			}
		}
		fprintf(stderr, "End Of Arg list not found\n");
		return 4;
	}

Done:
	banner(sargv);

	if (!configure())
		return 4;		/* system configuration failed */

	mkfilename(buf, spooldir, "rmail.log");
	if (debuglevel > 0 && logecho &&
			(logfile = FOPEN(buf, "a", TEXT)) == nil(FILE)) {
		logfile = stdout;
		printerr("main", buf);
	}
	if (logfile != stdout)
		fprintf(logfile, "\n>>>> Log level %d started %s\n", debuglevel, arpadate());
	if (filelist)
		printmsg(7, "main: arguments passes via input source (-l)");
	else
		printmsg(7, "main: arguments passed via command line"
#ifdef __MSDOS__
		            " (128 limit)"
#endif
		        );

	mkfilename (buf, confdir, "sendmail.cf");
	if ((fp = FOPEN(buf, "r", TEXT)) == nil(FILE)) {
		printerr("main", buf);
		printmsg(0, "main: Cannot open sendmail configuration file %s", buf);
		return 1;
	}
	getconfig(fp, TRUE, table);
	fclose(fp);
	if (   Bodyreturn == NULL
	    || (stricmp("sender", Bodyreturn) != 0
	    && stricmp("postmaster", Bodyreturn) != 0)
	    ) {
		printmsg(0, "main: incorrect 'RETURNBODY' settings in %s file", buf);
		return 1;
	}
	returnsenderbody = (stricmp("sender", Bodyreturn) == 0);
	printmsg(7, "main: returnsenderbody %d\n", returnsenderbody);

	if (receipt == NULL || strchr("YyTtNnFf", *receipt) == NULL) {
		printmsg(0, "main: incorrect 'RECEIPT' settings in %s file", buf);
		return 1;
	}
	doreceipt = (strchr("YyTt", *receipt) != NULL);
	printmsg(7, "main: doreceipt %d\n", doreceipt);

	if (GradeList != NULL && strlen(GradeList) != 4) {
		printmsg(0, "main: incorrect 'GRADES' settings in %s file", buf);
		return 1;
	}
	if (GradeList == NULL)
		GradeList = "LMNU";
	printmsg(7, "main: GradeList %s\n", GradeList);

	if (do_MIME) {
		if (strchr("YyTtNnFf", *do_MIME) == NULL) {
			printmsg(0, "main: incorrect '8BIT-HEADER' settings in %s file", buf);
			return 1;
		}
		mime_header = (strchr("NnFf", *do_MIME) != NULL);
	}
	printmsg(7, "main: 8bit-header %d\n", !mime_header);

	if (!init_charset(TRUE))
		return 4;
	if (!configure_swap())
		return 4;

#ifdef __MSDOS__
	return lmail(argc, argv);
#else
	{	int r = lmail(argc, argv);
		nokbd();
		return r;
	}
#endif
} /*main*/

/* c_break   -- control break handler to allow graceful shutdown   */
/* of interrupt handlers, etc.               */

#ifdef __MSDOS__
int c_break(void) {
	setcbrk(0);
	ctrlbrk(null);
#else
void c_break(int sig) {
	setcbrk(0);
	nokbd();
#endif
	printmsg(0, "c_break: program aborted by user Ctrl-Break");

	exit(100);              /* Allow program to abort */

#ifdef __MSDOS__
	return 0;
#endif
}
