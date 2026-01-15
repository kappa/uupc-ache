/*
   timestmp.c

   Compiler timestamps for display at program start-up

   History:

      12/13/89 Add Copyright statements - ahd
 */

#include <dos.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib.h"
#include "screen.h"

#if defined(__EMX__) && defined(__OS2__)
#define INCL_DOSPROCESS
#include <os2.h>
#endif

#ifdef __OS2__
#define UUPCV   "v7.03 Changes (C) 1991-2001 Ache, ported by Gul"
#else
#define UUPCV   "v7.03 Changes Copyright (C) 1991-2001 Andrey A. Chernov"
#endif

char compiled[] = { __DATE__ } ;
char compilet[] = { __TIME__ } ;
char compilev[] = { UUPCV } ;

char compilep[] = { "UUPC/@" } ;

char program[_MAX_FNAME];
char *calldir;
static char	drv[_MAX_DRIVE];
static char	dir[_MAX_DIR];
static char	ext[_MAX_EXT];

extern int screen;
void banner (char **argv)
{
	  char *cp, dummy[128];

#if defined(__EMX__) && defined(__OS2__)
	  /* dummy argv[0] - get real progname from environment */
	  PIB *pib;
	  TIB *tib;

 	  DosGetInfoBlocks(&tib, &pib);
	  for (cp = pib->pib_pchenv; *cp; cp+=strlen(cp)+1);
	  cp++;
	  argv[0] = cp;
#endif
          if (argv[0])
		_splitpath(argv[0], drv, dir, program, ext);
	  else
		program[0] = '\0';
	  if (program[0])
	  {
		 calldir = malloc(strlen(drv) + strlen(dir) + 1);
		 strcpy(calldir, drv);
		 strcat(calldir, dir);
		 sprintf(dummy, "%s: ", program);
		 cp = dummy+strlen(dummy)-1;
	  } /* if */
	  else {
		cp = dummy;
		strcpy(program, "UUNONE");
		calldir = ".";
	  }

	  if (!isatty(fileno(stdin))) /* Is the console I/O redirected?  */
		 return;              /* Yes --> Run quietly              */


	  sprintf(cp, "%s %s %2.2s%3.3s%2.2s",
				  compilep,
				  compilev,
				  &compiled[4],
				  &compiled[0],
				  &compiled[9]);
	 if (screen)
		 Sheader(dummy, !strnicmp(program, "UUCICO", 6));
	 else
		 fprintf(stderr, "%s\n", cp);
#ifdef __MSDOS__
	 if (_osmajor < 3) {
		fputs("Can work only with DOS version 3.0 or higher\n", stderr);
                abort();
	 }
#endif
} /* banner */
