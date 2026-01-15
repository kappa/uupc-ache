/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   ibmpc/ulib.c

   DCP system-dependent library

   Services provided by ulib.c:

   - login
   - UNIX commands simulation

   Updated:

	  14May89  - Added system name to login prompt - ahd
				 Added configuration file controlled user id, password
                 Added Kermit server option
	  17May89  - Redo login processing to time out after five minutes;
                 after all, we have to exit someday.                    ahd
	  22Sep89  - Add password file processing                           ahd
      24Sep89  - Modify login() to issue only one wait command for up
                 to 32K seconds; this cuts down LOGFILE clutter.        ahd
      01Oct89  - Re-do function headers to allow copying for function
		 prototypes in ulib.h                                   ahd
      17Jan90  - Filter unprintable characters from logged userid and
                 password to prevent premature end of file.             ahd
	  18Jan90  - Alter processing of alternate shells to directly
				 invoke program instead of using system() call.         ahd
   6  Sep 90   - Change logging of line data to printable               ahd
      8 Sep 90 - Split ulib.c into dcplib.c and ulib.c                  ahd
      8 Oct 90 - Break rmail.com and rnews.com out of uuio
                 Add FIXED_SPEED option for no-autobauding              ahd
      10Nov 90 - Move sleep call into ssleep and rename                 ahd
*/

#include <assert.h>
#include <ctype.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#ifndef __EMX__
#include <direct.h>
#else
#include <dirent.h>
#endif

#include "lib.h"
#include "arpadate.h"
#include "dcp.h"
#include "dcplib.h"
#include "dcpsys.h"
#include "hlib.h"
#include "hostable.h"
#include "import.h"
#include "modem.h"
#include "pushpop.h"
#include "ssleep.h"
#include "comm.h"
#include "ulib.h"
#include "usertabl.h"
#include "swap.h"

/*--------------------------------------------------------------------*/
/*                    Internal function prototypes                    */
/*--------------------------------------------------------------------*/

static void LoginShell( const   struct UserTable *userp );

/*--------------------------------------------------------------------*/
/*    l o g i n                                                       */
/*                                                                    */
/*    Login handler                                                   */
/*--------------------------------------------------------------------*/

CONN_STATE login(void)
{
   static char _Far line[BUFSIZ];                  /* Allow for long domain names!  */
   char user_b[50], *user;
   char pswd_b[50], *pswd;
   char attempts = 0;                  /* Allows login tries         */
   char *token;                        /* Pointer to returned token  */
   struct UserTable *userp;
   char *iptr, *optr;


/*--------------------------------------------------------------------*/
/*    Our modem is now connected.  Begin actual login processing      */
/*    by displaying a banner.                                         */
/*--------------------------------------------------------------------*/

   ssleep(1);
   sprintf(line,"\r\n\n%s %d.%02d (%s) (%s)\r\n",
#ifdef __MSDOS__
			"MS-DOS",
#else
			"OS/2",
#endif
			_osmajor, _osminor,
       domain, device);   /* Print a hello message            */
   if (wmsg(line,0) == S_LOST)
	   return CONN_DROPLINE;
   ddelay(250);

/*--------------------------------------------------------------------*/
/*    Display a login prompt until we get a printable character or    */
/*    the login times out                                             */
/*--------------------------------------------------------------------*/

   for ( attempts = 0; attempts < 5 ; attempts++ )
   {
      boolean invalid = TRUE;
      while (invalid)         /* Spin for a user id or timeout       */
	  {
		 r_purge();
		 if (wmsg("\r\nlogin: ", 0) == S_LOST)
			return CONN_DROPLINE;
		 *user_b = '\0';
		 if (rmsg(user_b, 2/*with echo */, 30) < 0) /* Did the user enter data?  */
			return CONN_DROPLINE;   /* No --> Give up                */

		 if ((user = strrchr(user_b, '\025')) == NULL)
			user = user_b;
		 else
			user++;
		  for (iptr = optr = user; *iptr; iptr++)
			if ((*iptr == '\b' || *iptr == '\177') && optr > user)
				optr--;
			else
				*optr++ = *iptr;
		  *optr = '\0';
		 if (equal(user,"NO CARRIER"))
			return CONN_DROPLINE;

		 token = user;
	 while ((*token != '\0') && invalid) /* Ingore empty lines   */
			invalid = ! isgraph(*token++);
      } /* while */

/*--------------------------------------------------------------------*/
/*          Zap unprintable characters, then log the user id          */
/*--------------------------------------------------------------------*/

      while (*token != '\0')
      {
	 if (*token < ' ')
	    *token = '?';
	 token++;
      }

	  printmsg(14, "login: login=%s", user);

/*--------------------------------------------------------------------*/
/*               We have a user id, now get a password                */
/*--------------------------------------------------------------------*/

	  r_purge();		/* prevent \r\n */
	  if (wmsg("\r\nPassword:", 0) == S_LOST)
		 return CONN_DROPLINE;
	  *pswd_b = '\0';
	  if (rmsg(pswd_b, 0/*without echo*/, 30) < 0)
		 return CONN_DROPLINE;
	  if ((pswd = strrchr(pswd_b, '\025')) == NULL)
		pswd = pswd_b;
	  else
		pswd++;
	  for (iptr = optr = pswd; *iptr; iptr++)
		if ((*iptr == '\b' || *iptr == '\177') && optr > pswd)
			optr--;
		else
			*optr++ = *iptr;
	  *optr = '\0';

/*--------------------------------------------------------------------*/
/*       Zap unprintable characters before we log the password        */
/*--------------------------------------------------------------------*/

     token = pswd;
     while (*token != '\0')
     {
	 if (*token < ' ')
		*token = '?';
	 token++;
      }

      printmsg(14, "login: password=%s", pswd);

/*--------------------------------------------------------------------*/
/*                 Validate the user id and passowrd                  */
/*--------------------------------------------------------------------*/

      token = user;
      while (!isalnum( *token ) && (*token !=  '\0'))
	  token ++;                  /* Scan for first alpha-numeric  */

      userp = checkuser(token);         /* Locate user id in host table  */

      if (userp == BADUSER)            /* Does user id exist?           */
	  {                                /* No --> Notify the user        */
		 printmsg(0,"login: login user %s failed, no such user",token);
		 if (wmsg("\r\nlogin failed",0) == S_LOST)
			return CONN_DROPLINE;

	  }
	  else if ( equal(pswd,userp->password))   /* Correct password?     */
	  {                                /* Yes --> Log the user "in"     */
				   /*   . . ..+....1....  +....2....+....3....  + .   */
		 sprintf(line,"\r\n\nWelcome to %s; login complete at %s\r\n",
				  domain, arpadate());
		 if (wmsg(line, 0) == S_LOST)
			return CONN_DROPLINE;
		 printmsg(0,"login: login user %s (%s) at %s",
					 userp->uid, userp->realname, arpadate());

		 if (userp->sh && equal(userp->sh,UUCPSHELL)) /* Standard uucp shell?       */
			return CONN_PROTOCOL;   /* Yes --> Startup the machine   */
		 else {                     /* No --> run special shell      */
			LoginShell( userp );
			return CONN_DROPLINE;   /* Hang up phone and exit        */
		 }
	  }
	  else {                        /* Password was wrong.  Report   */
		 printmsg(0,"login: login user %s (%s) failed, wrong password",
				  userp->uid, userp->realname);
		 if (wmsg("\r\nlogin failed",0) == S_LOST)
			return CONN_DROPLINE;
	  }
   }  /* for */

/*-----------------------------------------------------------------*/
/*    If we fall through the loop, we have an excessive number of  */
/*    login attempts; hangup the telephone and try again.          */
/*-----------------------------------------------------------------*/

   return CONN_DROPLINE;      /* Exit the program                    */

} /*login*/

/*--------------------------------------------------------------------*/
/*    L o g i n S h e l l                                             */
/*                                                                    */
/*    Execute a non-default remote user shell                         */
/*--------------------------------------------------------------------*/

static void LoginShell( const   struct UserTable *userp )
{
   char *s;
   int   rc;

   s = userp->sh;
   if (!s || !*s) {
	printmsg(0, "LoginShell: empty field not allowed");
	return;
   }
/*--------------------------------------------------------------------*/
/*              Get the program to run and its arguments              */
/*--------------------------------------------------------------------*/

   printmsg(1,"LoginShell: invoking '%s' in directory %s",
		 s, userp->homedir);

/*--------------------------------------------------------------------*/
/*       Run the requested program in the user's home directory       */
/*--------------------------------------------------------------------*/

   PushDir(userp->homedir);/* Switch to user's home dir     */
   rc = swap_system(s);
   PopDir();               /* Return to original directory  */

/*--------------------------------------------------------------------*/
/*                     Report any errors we found                     */
/*--------------------------------------------------------------------*/

   if ( rc != 0 )
	  printmsg(4,"LoginShell: execution return code is %d", rc);
} /* LoginShell */
