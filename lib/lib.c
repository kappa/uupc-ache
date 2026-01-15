/*--------------------------------------------------------------------*/
/*    l i b . c                                                       */
/*                                                                    */
/*    Support routines for UUPC/extended                              */
/*                                                                    */
/* Changes Copyright 1990, 1991 (c) Andrew H. Derbyshire              */
/*                                                                    */
/* History:                                                           */
/*    18Mar90 Add ignore, autoprint to configuration file         ahd */
/*    30 Apr 1990 -   Add autoedit support for sending mail       ahd */
/*    02 May 1990 -   Allow set of booleans options via options=  ahd */
/*  8 May  90  Add 'pager' option                                 ahd */
/* 10 May  90  Add 'purge' option                                 ahd */
/* 20 Apr 91   Reorganize to allow external calls to configuration    */
/*             routines for generic modem support.                ahd */
/*--------------------------------------------------------------------*/

/* #define __CORE__  Define it to catch malloc errors */

#ifdef __CORE__
#include <alloc.h>
#include <assert.h>
#endif
#include <ctype.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys\types.h>
#include <sys\stat.h>
#include <errno.h>
#include <conio.h>
#include <dos.h>
#include <fcntl.h>
#include <malloc.h>
#include <share.h>
#ifndef __EMX__
#include <direct.h>
#endif
#ifdef __OS2__
#include <os2conio.h>
#endif

#include "lib.h"
#include "hlib.h"
#include "dater.h"
#include "hostable.h"
#include "screen.h"
#include "ssleep.h"

currentfile();

#define LOGCHAN (logfile == stdout ? stderr : logfile)

char *mailbox = NULL, *name, *homedir;
char *E_inmodem;
char *maildir, *newsdir, *confdir, *spooldir, *pubdir, *tempdir;
char *domain, *fdomain, *nodename, *mailserv;
char *anonymous;
char *localdomain;
char *share = NULL;
char *extcharset, *extsetname, *intsetname;
char *produce_sound = NULL;
char *E_uncompress;
char *swapto;
char *E_cwd = ".";
char *batchmail;
char *batchflush;
char *rnewscmd = NULL;
char * compress;
INTEGER maxhops = 20;                                       /* ahd */
int PacketTimeout = 4;
INTEGER MaxErr= 30;           /* Allowed errors per single packet    */
INTEGER xfer_bufsize = 4096;  /* Buffering used for file transfers   */

int debuglevel = 1;
boolean logecho = FALSE;
FILE *logfile = stdout;
extern int screen;
extern int AllowTzset;
extern char *calldir;

/*--------------------------------------------------------------------*/
/*  The following table controls the configuration files processing   */
/*--------------------------------------------------------------------*/

char*    _tz = NULL;
char *sendprog, *E_indevice, *E_inspeed;

static struct Table table[] = {  /*     must    sys    dir    suff */
	{"BATCHFLUSH",	&batchflush,    FALSE,  TRUE,  FALSE, NULL},
	{"BATCHMAIL",   &batchmail,	TRUE,   TRUE,  FALSE, NULL},
	{"CONFDIR",		&confdir,	TRUE,	TRUE,  TRUE, "CONF\\"},
	{"COMPRESS",	&compress,	FALSE,	TRUE,  FALSE, NULL},
	{"DOMAIN",	&domain,	TRUE,	TRUE,  FALSE, NULL},
	{"EXTCHARSET",	&extcharset,	FALSE,  FALSE, FALSE, NULL},
	{"EXTSETNAME",	&extsetname,	FALSE,  FALSE, FALSE, NULL},
	{"HOME", 	&homedir,	FALSE,	FALSE, TRUE,  NULL},
	{"INDEVICE", 	&E_indevice, 	TRUE,	TRUE,  FALSE, NULL},
	{"INMODEM",	&E_inmodem, 	TRUE,	TRUE,  FALSE, NULL},
	{"INSPEED",  	&E_inspeed, 	TRUE,   TRUE,  FALSE, NULL},
	{"INTSETNAME",	&intsetname,	FALSE,  FALSE, FALSE, NULL},
	{"MAILBOX", 	&mailbox,	FALSE,	FALSE, FALSE, NULL},
	{"MAILDIR",	&maildir,	TRUE,	TRUE,  TRUE, "MAIL\\BOXES\\"},
	{"MAILSERV", 	&mailserv,	TRUE,	TRUE,  FALSE, NULL},
	{"NEWSDIR",	&newsdir,	TRUE,	TRUE,  TRUE, "NEWS\\INCOMING\\"},
	{"NODENAME", 	&nodename,	TRUE,	TRUE,  FALSE, NULL},
	{"PUBDIR",	&pubdir,	TRUE,	TRUE,  TRUE, "PUBLIC\\"},
	{"RNEWS",	&rnewscmd,	FALSE,	TRUE,  FALSE, NULL},
	{"SENDMAIL", 	&sendprog, 	FALSE,  TRUE,  FALSE, NULL},
	{"SHARE",	&share,		FALSE, 	TRUE,  FALSE, NULL},
	{"SOUNDS",	&produce_sound, FALSE,  FALSE, FALSE, NULL},
	{"SPOOLDIR", 	&spooldir,	TRUE,	TRUE,  TRUE, "SPOOL\\"},
	{"SWAPTO", 	&swapto,	FALSE,	TRUE,  FALSE, NULL},
	{"TEMPDIR",	&tempdir,	TRUE,	FALSE, TRUE, "TMP\\"},
	{"TZ",		&_tz,		TRUE,	FALSE, FALSE, NULL},
	{"UNCOMPRESS",	&E_uncompress,  FALSE,  TRUE,  FALSE, NULL},
	{nil(char), }
}; /* table */

void AssignDefaultDirs(void)
{
	struct Table *tptr;
	char	buf[_MAX_PATH];

	for (tptr = table; tptr->sym != nil(char); tptr++) {
		if (tptr->std && (*(tptr->loc) == nil(char))) {
			if (tptr->suff == nil(char))
				*tptr->loc = strdup(getcwd(buf, _MAX_PATH));
			else {
				*tptr->loc = malloc(strlen(calldir) + strlen(tptr->suff) + 2);
				mkfilename(*tptr->loc, calldir, tptr->suff);
			}
		}
	}
}


/*
   getconfig - process a configuration file
*/

void getconfig(FILE* fp, int sysmode, struct Table *table) {
	struct Table *tptr;
	char buf[100];
	char *cp, *s, *buff;

	for (;;) {
		if (fgets(buf, sizeof buf, fp) == nil(char))
			break;				/* end of file */
		if (*(cp = buf + strlen(buf) - 1) != '\n')
			cp++;
		while(isspace((unsigned char)*(cp - 1)))
			cp--;
		*cp = '\0';
		for (buff = buf; *buff && isspace((unsigned char)*buff); buff++)
			;
		if (*buff == '#' || !*buff)
			continue;			/* comment line */
		if ((cp = strchr(buff, '=')) == nil(char))
			continue;
		*cp++ = '\0';
		if ((s = strchr(buff, ' ')) != nil(char))
			*s = '\0';
		if ((s = strchr(buff, '\t')) != nil(char))
			*s = '\0';
		while (isspace((unsigned char)*cp))
			cp++;
		strupr(buff);
		for (tptr = table; tptr->sym != nil(char); tptr++) {
			if (equal(buff, tptr->sym)) {
				if (tptr->sys && !sysmode)
					printmsg(0, "User specified system parameter `%s' ignored.\n", tptr->sym);
				else {
					if (*(tptr->loc) != nil(char))
						free(*(tptr->loc)); /* free the previous one */
					*(tptr->loc) = strdup(cp);
				}
				break;
			}
		}
	} /*for*/
	for (tptr = table; tptr->sym != nil(char); tptr++) {
		if (   !tptr->sys
			&& (s = getenv(tptr->sym)) != NULL
		   ) {
			if (*(tptr->loc) != nil(char))
				free(*(tptr->loc));	/* free the previous one */
			*(tptr->loc) = strdup(s);
		}
	}
} /*getconfig*/

/*
   configure - define the global parameters of UUPC
*/

int configure(void) {
	FILE*		fp;
	char*		sysrc;
	char*		usrrc;
	struct Table *tptr;

	getrcnames(&sysrc, &usrrc);
	if ((fp = FOPEN(sysrc, "r", TEXT)) == nil(FILE)) {
		printmsg(0, "Cannot open system configuration file `%s'", sysrc);
		return FALSE;
	}
	getconfig(fp, TRUE, table);
	fclose(fp);
	if ((fp = FOPEN(usrrc, "r", TEXT)) != nil(FILE)) {
		getconfig(fp, FALSE, table);
		fclose(fp);
	}
	printmsg(30, "CONFDIR: '%s', HOME: '%s'", confdir, homedir);
	for (tptr = table; tptr->sym != nil(char); tptr++) {
		if (tptr->must && (*(tptr->loc) == nil(char))) {
			printmsg(0, "Configuration parameter `%s' must be set.", tptr->sym);
			return FALSE;
		}
	}

	tzset();
	AllowTzset = 0;		/* No More! */

	if (!mailbox || !*mailbox)
		mailbox = "uucp";
	printmsg(30, "MAILBOX: '%s', TEMPDIR: '%s'", mailbox, tempdir);
	if (share != NULL && (!stricmp(share, "NO") || !stricmp(share,"FALSE"))) {
		free(share);
		share = NULL;
	}
	if (produce_sound != NULL && (!stricmp(produce_sound, "NO") || !stricmp(produce_sound, "FALSE"))) {
		free(produce_sound);
		produce_sound = NULL;
	}

	/* Verify that the user gave us a good name server    */
	if (equali(mailserv, nodename) || equali(mailserv, domain)) {
		printmsg(0, "`%s' is name of this host and cannot be mail server", mailserv);
		return FALSE;
	} else if (checkname(mailserv) == BADHOST) {
		printmsg(0, "Mail server `%s' must be listed in SYSTEMS file", mailserv);
		return FALSE;
	}
	if (extsetname != NULL)
		strlwr(extsetname);
	if (intsetname != NULL)
		strlwr(intsetname);
	return TRUE;
} /*configure*/

/*--------------------------------------------------------------------*/
/*    M K D I R                                                       */
/*                                                                    */
/*    Like mkdir() but create intermediate directories as well        */
/*--------------------------------------------------------------------*/

int MKDIR(const char *path)
{
   char *cp, *s;
   int len;
   struct stat statbuf;

   if (*path == '\0') {
	  errno = EEXIST;
	  return -1;
   }

   s = cp = strdup(path);
   checkref(cp);
   while ((s = strchr(s, '/')) != nil(char))
	  *s = '\\';

   len = strlen(cp);
   while (len > 1 && cp[len-1] == '\\' && (len != 3 || cp[1] != ':'))
	cp[--len] = '\0';

   if (   stat(cp, &statbuf) == 0
	   && (statbuf.st_mode & S_IFDIR) != 0
	  ) {
	  errno = EEXIST;
	  return -1;
   }

   /* see if we need to make any intermediate directories */
   for (s = cp; (s = strchr(s, '\\')) != nil(char); s++) {
	  if (cp == s || s[-1] == ':')
		continue;
	  *s = '\0';
	  (void) mkdir(cp);
	  *s = '\\';
   }

   /* make last dir */
   len = mkdir(cp);
   free(cp);

   return len;
} /*MKDIR*/

/*--------------------------------------------------------------------*/
/*    F O P E N                                                       */
/*                                                                    */
/*    Like fopen() but create imtermediate directories                */
/*                                                                    */
/*    This routine has dependency on the path separator characters    */
/*    being '/', we should relove that somehow someday.               */
/*--------------------------------------------------------------------*/

FILE *FOPEN(const char *fname, const char *mode, const char ftype)
{

   char *last, *cp;
   FILE *results;
   int r;
   char *name;

   /* are we opening for write or append */
   FILEMODE(ftype);

   name = strdup(fname);
   checkref(name);

   cp = name;
   while ((cp = strchr(cp, '/')) != nil(char))
	  *cp = '\\';

   results = _fsopen(name, mode, SH_DENYNO);

   if ((results != nil(FILE)) || (*mode == 'r')) {
	  free(name);
	  return results;
   }

   /* verify all intermediate directories in the path exist */
   if ((last = strrchr(name, '\\')) != nil(char)) {
	  *last = '\0';
	  r = MKDIR(name);
	  *last = '\\';
	  if (r == 0)
		  /* now try open again */
		  results = _fsopen(name, mode, SH_DENYNO);
   }
   free(name);
   return results;
} /*FOPEN*/

/*--------------------------------------------------------------------*/
/*    C R E A T                                                       */
/*                                                                    */
/*    Create a file with the specified mode                           */
/*--------------------------------------------------------------------*/

int CREAT(char *name, const int mode, const char ftyp)
{

   char *last, *cp;
   int results, r;

   /* are we opening for write or append */
   FILEMODE(ftyp);

   cp = name;
   while ((cp = strchr(cp, '/')) != nil(char))
	  *cp = '\\';

   results = creat(name, mode);

   if (results != -1)
	  return results;

   /* see if we need to make any intermediate directories */
   if ((last = strrchr(name, '\\')) != nil(char)) {
	  *last = '\0';
	  r = MKDIR(name);
	  *last = '\\';
	  if (r == 0)
		  /* now try open again */
		  return creat(name, mode);
   }

   return results;
} /*CREAT*/

/*--------------------------------------------------------------------*/
/*    R E N A M E                                                     */
/*                                                                    */
/*    Rename a file, creating the target directory if needed          */
/*--------------------------------------------------------------------*/

int RENAME(char *oldname, char *newname )
{

	char *last, *cp;
	FILE *of, *nf;
	int c;

	cp = oldname;
	while ((cp = strchr(cp, '/')) != nil(char))
	  *cp = '\\';

	cp = newname;
	while ((cp = strchr(cp, '/')) != nil(char))
	  *cp = '\\';

/*--------------------------------------------------------------------*/
/*                     Attempt to rename the file                     */
/*--------------------------------------------------------------------*/

	if (!rename( oldname, newname )) /* Success?                      */
	  return 0;                     /* Yes --> Return to caller      */

/*--------------------------------------------------------------------*/
/*      Try rebuilding the directory and THEN renaming the file       */
/*--------------------------------------------------------------------*/

   if ((last = strrchr(newname, '\\')) != nil(char))
   {
	  *last = '\0';
	  (void) MKDIR(newname);
	  *last = '\\';
   }

   if (!rename( oldname, newname ))
	   return 0;

   if ((of = FOPEN(oldname, "r", BINARY)) == NULL)
	   return -1;
   if ((nf = FOPEN(newname, "w", BINARY)) == NULL) {
	   fclose(of);
	   return -1;
   }
   while ((c = getc(of)) != EOF)
		if (putc(c, nf) == EOF) {
			fclose(of);
			fclose(nf);
			unlink(newname);
			return -1;
	   }
   fclose(of);
   if (fclose(nf) == EOF) {
	   unlink(newname);
	   return -1;
   }

   unlink(oldname);

   return 0;
} /* RENAME */


/* quote string and strcat */
char *qstrcat(char *dest, char *s)
{	char *p;
	int  quote=0;

	if (strpbrk(s, " \t\'\"\\") == NULL)
		return strcat(dest, s);
	p = dest + strlen(dest);
	if (strpbrk(s, " \t\'")) {
		*p++ = '"';
		quote = 1;
	}
	while (*s)
		switch (*s) {
			case '"':
			case '\\':	*p++ = '\\';
			default:	*p++ = *s++;
		}
	if (quote)
		*p++ = '"';
	*p = '\0';
	return dest;
}

/*--------------------------------------------------------------------*/
/*    g e t a r g s                                                   */
/*                                                                    */
/*    Return a list of pointers to tokens in the given line           */
/*--------------------------------------------------------------------*/

int getargs(char *line, char *flds[], int maxfields)
{
   int i = 0;
   char quoted = '\0';

   while ((*line != '\0') && (*line != '\n')) {
	  if (isspace(*line))
		 line++;
	  else {
		 char *out = line;
		 if (maxfields-- <= 0)
			return -1;
		 *flds++ = line;
		 i++;
		 while((quoted || !isspace(*line)) && (*line != '\0'))
		 {
			switch(*line)
			{
			   case '"':
			   case '\'':
				  if (quoted)
				  {
					 if (quoted == *line)
					 {
						quoted = 0;
						line++;
					 }
					 else
						*out++ = *line++;
				  } /* if */
				  else
					 quoted = *line++;
				  break;

			   case '\\':
				  switch(*++line)         /* Unless the following    */
				  {                       /* character is very       */
					 default:             /* special we pass the \   */
								if (!isspace(*line))
						   *out++ = '\\'; /* and following char on   */
					 case '"':
					 case '\'':
					 case '\\':
						*out++ = *line++;
				  }
				  break;

			   default:
				  *out++ = *line++;

			} /*switch*/
		 } /* while */
		 if (isspace(*line))
			line++;
		 *out = '\0';
	  } /* else */
   }

   if (maxfields-- <= 0)
	return -1;
   *flds++ = NULL;
   return i;

} /*getargs*/

boolean visual_output = FALSE;
boolean visret;
int vissleep = 5;	/* Default 5 seconds waiting */
char *viscont = "continue";

void visput(char *msg, int level)
{
#define MAXLINES 3
	static char scrbuf[132*2*MAXLINES];
	struct text_info ti;

	gettextinfo(&ti);
	gettext(1, 1, ti.screenwidth, MAXLINES, scrbuf);
	window(1, 1, ti.screenwidth, MAXLINES);
	clrscr();
	gotoxy(1, 1);

	textattr(level == 0 ? WHITE : LIGHTGRAY);
	cputs(msg);
	putch('\r');
	textattr(LIGHTGRAY);
	cprintf("Press ESC to %s or wait %d seconds: ", viscont, vissleep);

	visret = ssleep(vissleep);

	puttext(1, 1, ti.screenwidth, MAXLINES, scrbuf);
	window(ti.winleft, ti.wintop, ti.winright, ti.winbottom);
	gotoxy(ti.curx, ti.cury);
	textattr(ti.attribute);
}

/*--------------------------------------------------------------------*/
/*   p r i n t m s g                                                  */
/*                                                                    */
/*   Print an error message if its severity level is high enough.     */
/*   Print message on standard output if not in remote mode           */
/*   (call-in).  Always log the error message into the log file.      */
/*                                                                    */
/*   Modified by ahd 10/01/89 to check for Turbo C NULL pointers      */
/*   being de-referenced anywhere in program.  Fixed 12/14/89         */
/*                                                                    */
/*   Modified by ahd 04/18/91 to use true variable parameter list,    */
/*   supplied by Harald Boegeholz                                     */
/*--------------------------------------------------------------------*/

void printmsg(int level, char *fmt, ...)
{
   va_list arg_ptr;
   static char msg[4096/*max packet size*/+256];
   char *s;

#ifdef __CORELEFT__
   static unsigned freecore = 63 * 1024;
   unsigned nowfree;
#endif

#ifdef __CORE__
   int heapstatus;

   heapstatus = heapcheck();
   if (heapstatus == _HEAPCORRUPT)
	   debuglevel = level;  /* Force this last message to print ahd   */

#endif

#ifdef __CORELEFT__
   nowfree = coreleft();
   if (nowfree < freecore)
   {
	  freecore = ( nowfree / 1024) * 1024;
	  printmsg(0,"free core left = %u bytes", nowfree);
   }
#endif

   if (level <= debuglevel)
   {
	  va_start(arg_ptr,fmt);
	  vsprintf(msg, fmt, arg_ptr);
	  va_end(arg_ptr);
	  if (msg[strlen(msg)-1] != '\n')
		  strcat(msg, "\n");

	  if (logfile != stdout)
	  {
		 if ( debuglevel > 1 )
			fprintf(logfile, "(%d) ", level);
		 else
			fprintf(logfile, "%s ", dater( time( NULL ), NULL));
	  }

	  /* First, write it to logfile */
	  if (!screen || logfile != stdout) {
		 if (   logfile == stdout
			 && visual_output
			)
			visput(msg, level);
		 else {
			 fputs(msg, LOGCHAN);
			 if (fflush(LOGCHAN)) {
				perror("Logfile error");
				abort();
			 } /* if */
		 }
	  }
	  /* Put it to debug window */
	  if (screen && (level > 0 || (level == 0 && debuglevel > 0)))
			Swputs(WDEBUG, msg);
	  /* Or duplicate it from logfile to screen */
	  else if (!screen && logfile != stdout) {
			if (visual_output)
				visput(msg, level);
			else
				fputs(msg, stderr);
	  }

	  if (level <= 0 && screen) {
		if ((s = strchr(msg, ':')) == NULL || s - msg > 20)
			s = msg;
		else
			s += 2;
		if ('a' <= *s && *s <= 'z')
			*s ^= 040;
		if (level < 0)
			Smisc(s);
		else
			Serror(s);
	  }

#ifdef __CORE__                                         /* ahd */
	  assert(heapstatus != _HEAPCORRUPT);
#endif
	  if (logfile != stdout) {
		fflush(logfile);
		real_flush(fileno(logfile));
	  }
   } /* if (level <= debuglevel) */

} /*printmsg*/

void dbgputc(const char c)
{
	if (logfile != stdout || !screen) {
		fputc(c, LOGCHAN);
		if (ferror(LOGCHAN)) {
			perror(LOGFILE);
			panic();
		}
	}
	if (screen) {
		static char msg[2];

		msg[0] = c;
		Swputs(WDEBUG, msg);
	}
}


void dbgputs(char *str)
{
	if (logfile != stdout || !screen) {
		fputs(str, LOGCHAN);
		if (ferror(LOGCHAN)) {
			perror(LOGFILE);
			panic();
		}
	}
	if (screen)
		Swputs(WDEBUG, str);
}

void show_char(const unsigned char byte)
{
	if(byte < ' ') {
		dbgputc('^');
		dbgputc((unsigned char)(byte + 0x40));
	}
	else if(byte == 0x7f)
		dbgputs("^?");
	else
		dbgputc(byte);
}


/*--------------------------------------------------------------------*/
/*    p r i n t e r r                                                 */
/*                                                                    */
/*    Perform a perror() with logging                                 */
/*--------------------------------------------------------------------*/

void printerr(const char *func, const char *prefix)
{
   printmsg(1, "%s: %s - %s", func, prefix, strerror(errno));
}

/*--------------------------------------------------------------------*/
/*    c h e c k p t r                                                 */
/*                                                                    */
/*    Verfiy that a pointer is not null                               */
/*--------------------------------------------------------------------*/

void checkptr(const char *file, int line)
{
  printmsg(0,"Storage allocation failure in file %s at line %d; \
possible cause: memory shortage.",
	file,line);
  panic();
} /* checkptr */

/*--------------------------------------------------------------------*/
/*    b u g o u t                                                     */
/*                                                                    */
/*    Perform a panic() exit from UUPC/extended                       */
/*--------------------------------------------------------------------*/

void bugout( const size_t lineno, const char *fname )
{
   static char bell[] = "\a";

   if (isatty(2))
	  write(2, bell, 1);
   printmsg(1,"panic: program aborted at line %d in file %s",
			  lineno, fname );
   exit(3);	/* don't use abort here, because of atexit stuff */
}

/* This STUPID dos can't update disk information on fflush! */

int real_flush(int fd)
{
   int nd;

#ifdef __MSDOS__
   if (_osmajor > 3 || _osmajor == 3 && _osminor >= 30)
      return _dos_commit(fd);
#endif

   if ((nd = dup(fd)) < 0)
      return -1;
   if (close(nd) < 0)
      return -1;
   return 0;
}

/* Check needed memory in KB */

#pragma off (unreferenced);
int needkbmem(int kbsize, char *pgm)
#pragma on (unreferenced);
{
#ifdef __MSDOS__
	unsigned segbuf;
	unsigned have, size;

	if (kbsize <= 0)
		return 0;
	size = (unsigned) ((unsigned long) kbsize * 1024) / 16;
	have = allocmem(size, &segbuf);
	if (have == (unsigned) -1) {	/* Ok */
		if (freemem(segbuf))
			return errno;
		return 0;
	}
	printmsg(0, "needkbmem: additional %uKb needed for %s, get rid of TSR's!",
		(unsigned) (((unsigned long)(size - have) * 16 + (1024-16)) / 1024),
		pgm);
	return ENOMEM;
#else
	return 0;
#endif
}

void close_junk_files(void)
{
	int i, j;

#ifdef __MSDOS__
	(void) fclose(stdaux);
	(void) fclose(stdprn);
	i = 5;
#else /* OS/2 */
	i = 3;
#endif
	for (j=0; j <= FOPEN_MAX; j++)
		(void) close(i+j);
}

