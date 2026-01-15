/*--------------------------------------------------------------------*/
/*    r e a d n e x t . c                                             */
/*                                                                    */
/*    Reads a spooling directory with optional pattern matching       */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*    Changes Copyright (c) 1989-1994 by Kendra Electronic            */
/*    Wonderworks.                                                    */
/*                                                                    */
/*    All rights reserved except those explicitly granted by the      */
/*    UUPC/extended license agreement.                                */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*                          RCS Information                           */
/*--------------------------------------------------------------------*/

/*
 *    $Id: readnext.c 1.10 1994/02/19 04:45:50 ahd Exp $
 *
 *    $Log: readnext.c $
 *     Revision 1.10  1994/02/19  04:45:50  ahd
 *     Use standard first header
 *
 *     Revision 1.9  1994/02/19  03:56:48  ahd
 *     Use standard first header
 *
 *     Revision 1.9  1994/02/19  03:56:48  ahd
 *     Use standard first header
 *
 *     Revision 1.8  1994/02/18  23:13:48  ahd
 *     Use standard first header
 *
 *     Revision 1.7  1994/01/24  03:09:22  ahd
 *     Annual Copyright Update
 *
 *     Revision 1.6  1994/01/01  19:04:31  ahd
 *     Annual Copyright Update
 *
 *     Revision 1.5  1993/08/26  05:00:25  ahd
 *     Debugging code for odd failures on J. McBride's network
 *
 *     Revision 1.5  1993/08/26  05:00:25  ahd
 *     Debugging code for odd failures on J. McBride's network
 *
 *     Revision 1.4  1993/04/05  04:32:19  ahd
 *     Add timestamp, size to information returned by directory searches
 *
 *     Revision 1.3  1992/11/22  20:58:55  ahd
 *     Use strpool to allocate const strings
 *
 */

/*--------------------------------------------------------------------*/
/*                        System include files                        */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include <sys/types.h>
#include <sys/stat.h>
#include "uupcmoah.h"
#include "readnext.h"
#include "uundir.h"
#include "hostable.h"

currentfile();

/*--------------------------------------------------------------------*/
/*    r e a d n e x t                                                 */
/*                                                                    */
/*    Read a directory into a linked list                             */
/*--------------------------------------------------------------------*/

char     *readnext(char *xname,
		   const char *remote,
		   const char *subdir,
		   time_t *modified,
		   long   *size )
{
   static DIR *dirp = NULL;
   static char *SaveRemote = NULL;
   static char remotedir[FILENAME_MAX];
   char remsub[HOSTLEN+1];
   struct stat statbuf;

   struct dirent *dp;

/*--------------------------------------------------------------------*/
/*          Determine if we must restart the directory scan           */
/*--------------------------------------------------------------------*/

   if ( (remote == NULL) || ( SaveRemote == NULL ) ||
	!equal(remote, SaveRemote ) )
   {
      if ( SaveRemote != NULL )   /* Clean up old directory? */
      {                           /* Yes --> Do so           */
	 closedir(dirp);
	 dirp = NULL;
	 SaveRemote = NULL;
      } /* if */

      if ( remote == NULL )      /* Clean up only, no new search? */
	 return NULL;            /* Yes --> Return to caller      */

      strncpy(remsub, remote, HOSTLEN);
      remsub[HOSTLEN] = '\0';
      mkfilename(remotedir, E_spooldir, remsub);
      strcat(remotedir, "\\");
      strcat(remotedir, subdir);

      if ((dirp = opendir(remotedir)) == nil(DIR))
      {
	 printmsg(5, "readnext: couldn't opendir() %s", remotedir);
	 dirp = NULL;
	 return NULL;
      } /* if */

      SaveRemote = newstr( remote );
			      /* Flag we have an active search    */
   } /* if */

/*--------------------------------------------------------------------*/
/*              Look for the next file in the directory               */
/*--------------------------------------------------------------------*/

FoundDir:
   if ((dp = readdir(dirp)) != nil(struct dirent))
   {
      strlwr(dp->d_name); /* for import */
      mkfilename(xname, remotedir, dp->d_name);
      stat(xname, &statbuf);
      if (statbuf.st_mode & S_IFDIR)
         goto FoundDir;
      printmsg(5, "readnext: matched \"%s\"",xname);

      if ( modified != NULL )
         *modified = *(time_t *)(&(statbuf.st_mtime));

      if ( size != NULL )
         *size = statbuf.st_size;

      return xname;
   }

/*--------------------------------------------------------------------*/
/*     No hit; clean up after ourselves and return to the caller      */
/*--------------------------------------------------------------------*/

   printmsg(5, "readnext: \"%s\" not matched", remotedir);
   closedir(dirp);
   SaveRemote = NULL;
   dirp = NULL;
   return NULL;

} /*readnext*/
