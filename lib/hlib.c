/*
   hlib.c

   Host dependent library routines for UUPC/extended

   Change history:

   08 Sep 90 - Split from local\host.c                            ahd
 */


#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <process.h>
#include <ctype.h>
#include "uupcmoah.h"

#include "uundir.h"
#include "hostable.h"

extern char program[];

currentfile();

/*
   mkfilename - build a path name out of a directory name and a file name
*/

void mkfilename(char *pathname, const char *path, const char *name)
{
	const char *lastc = path + strlen(path) - 1;
	char *s;

	if (*lastc != '\\' && *lastc != '/')
		sprintf(pathname, "%s\\%s", path, name);
	else
		sprintf(pathname, "%s%s", path, name);
	strlwr(pathname);
	for (s = pathname; *s; s++)
		if (*s == '/')
			*s = '\\';
} /*mkfilename*/


static starttime = -1;
static pid;

static void init_starttime(void)
{
	struct timeb timeb;

	ftime(&timeb);
	starttime = (((timeb.millitm / 10) & 0xff) +
		     (((int)timeb.time & 0xf) << 8)) & 0xfff;
	pid = (getpid() >> 4) & 0xff;
	printmsg(20, "init_starttime: time %Xh, pid %Xh", starttime, pid);
}

/*
   m k t e m p n a m e

   Generate a temporary name with a pre-defined extension
 */

char *mktempname( char *buf, char extension[])
{
   static int file = 0;
   int i;
   char buf2[FILENAME_MAX];
   char *name;

   if (buf == NULL)           /* Do we need to allocate buffer?         */
   {
	  buf = malloc(strlen(tempdir) + 15);
	  checkref(buf);
   } /* if */
   if (starttime == -1)
	init_starttime();

   name = program;
   if (strnicmp(name, "uu", 2) == 0)
	  name += 2;

   for (i = 0; i < 16; i++)
   {
	sprintf(buf2, "%.2s%x%x%x.%s",
		name, pid, starttime,
		file++ & 0xf, extension);
	mkfilename(buf, tempdir, buf2);
	if (access(buf, 0) == -1)
		break;
   } /* for */

   if ( i >= 16 ) {
	printmsg(0, "mktempname: temp name space lost!");
	panic();
   }
   printmsg(5,"mktempname: generated temporary name: %s", buf);
   return buf;

} /* mktempname */

static int mstrcmp(char * wildcard,char * str)
{
	char * p,* p1;
	for (p=wildcard,p1=str;;p++,p1++) {
		if (*p=='*') return 0;
		if ((*p=='?') && (*p1)) continue;
		if ((*p==0) && (*p1==0)) return 0;
		if (toupper(*p)!=toupper(*p1)) return 1;
	}
}

int wildmatch(char * str,char * wildcard)
{	/* TODO: make clean like regext */
	char *p=wildcard, *p1=str;
	if (*p!='*') {
		if (mstrcmp(p,p1)) return 1;
		while ((*p!='*') && (*p!=0)) p++,p1++;
		if (*p==0) return 0;
	}
	p++;
	for (;;) {
		while (mstrcmp(p,p1)) {
			if (*p1==0) return 1;
			p1++;
		}
		while ((*p!='*') && (*p!=0)) p++,p1++;
		if (*p==0) return 0;
		p++;
	}
}

void cleantmp(void)
{
	DIR *dirp;
	char pat[20], *name;
	char buf[FILENAME_MAX];
	struct dirent *dp;

	if (starttime == -1)
	   init_starttime();
	name = program;
	if (strnicmp(name, "uu", 2) == 0)
	   name += 2;
	sprintf(pat, "%.2s%x%x?.*",
		name, pid, starttime);
	if ((dirp = opendir(tempdir)) == NULL)
		return;

	while ((dp = readdir(dirp)) != NULL) {
		if (wildmatch(dp->d_name, pat) != 0) continue;
		mkfilename(buf, tempdir, dp->d_name);
		remove(buf);
	}

	closedir(dirp);
}

/*--------------------------------------------------------------------*/
/*    g e t r c n a m e s                                             */
/*                                                                    */
/*    Return the name of the configuration files                      */
/*--------------------------------------------------------------------*/

# define	SYSRCSYM	"UUPCSYSRC"
# define	USRRCSYM	"UUPCUSRRC"
# define	SYSDEBUG	"UUPCDEBUG"

extern char *homedir, *calldir;
extern boolean visual_output;
extern void AssignDefaultDirs(void);

void getrcnames(char** sysp, char** usrp) {
	char*	debugp;		/* Pointer to debug environment variable  */
	int	lvl;
	char	drv[_MAX_DRIVE];
	char	dir[_MAX_DIR];
	char	ext[_MAX_EXT];
	char	fname[_MAX_FNAME];
	char	buf[_MAX_PATH];
	char	*try_confdir;
	static char pers[] = ".\\PERSONAL.RC";

	lvl = 0;
	debugp = getenv(SYSDEBUG);
	if (debugp != nil(char)) /* Debug specified in environment? */
		lvl = atoi(debugp); /* Yes --> preset debuglevel for user */
	if (lvl < 0) {
		logecho = TRUE;
		debuglevel = (-lvl);
		visual_output = FALSE;
	} else if (lvl > 0) {
		logecho = FALSE;
		debuglevel = lvl;
	}

	if ((*sysp = getenv(SYSRCSYM)) != nil(char)) {
		*sysp = strdup(*sysp);
		_splitpath(*sysp, drv, dir, fname, ext);
		confdir = malloc(strlen(drv) + strlen(dir) + 1);
		strcpy(confdir, drv);
		strcat(confdir, dir);
	} else {
		try_confdir = malloc(strlen(calldir) + 5 + 1);
		strcpy(try_confdir, calldir);
		strcat(try_confdir, "CONF\\");
		mkfilename(buf, try_confdir, "UUPC.RC");
		*sysp = strdup(buf);
	}

	if ((*usrp = getenv(USRRCSYM)) != nil(char)) {
		*usrp = strdup(*usrp);
		_splitpath(*usrp, drv, dir, fname, ext);
		homedir = malloc(strlen(drv) + strlen(dir) + 1);
		strcpy(homedir, drv);
		strcat(homedir, dir);
	} else
		*usrp = strdup(pers);
	AssignDefaultDirs();
	printmsg(30, "(CONFDIR?): '%s', (HOME?): '%s'", confdir, homedir);
} /*getrcnames*/


/*
   filemode - default the text/binary mode for subsequently opened files
*/

void filemode(char mode)
{
#ifdef __EMX__
    extern int _fmode_bin;
#endif

   if (mode != BINARY && mode != TEXT)
		panic();
#ifndef __EMX__
   _fmode = (mode == TEXT) ? O_TEXT : O_BINARY;
#else
   _fmode_bin = (mode == TEXT) ? 0 : 1;
#endif

} /*filemode*/
