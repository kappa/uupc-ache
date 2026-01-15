/*--------------------------------------------------------------------*/
/*    c h e c k t i m . c                                             */
/*                                                                    */
/*    Time of day validation routine for UUPC/extended                */
/*                                                                    */
/*    Copyright (c) 1989, 1990, 1991 by Andrew H. Derbyshire          */
/*                                                                    */
/*    Change history:                                                 */
/*       20 Apr 1991 Broken out of dcpsys.c                    ahd    */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "lib.h"
#include "checktim.h"

/*--------------------------------------------------------------------*/
/*   The following day values are based on the fact the               */
/*   localtime() call returns the day of the week as a value zero     */
/*   (0) through six (6), which is converted into the bit number      */
/*   and then AND'ed against the date mask.                           */
/*--------------------------------------------------------------------*/

#define SUN 0x80
#define MON 0x40
#define TUE 0x20
#define WED 0x10
#define THU 0x08
#define FRI 0x04
#define SAT 0x02
#define NEVER 0x00
#define WEEKEND (SAT | SUN)
#define WEEKDAY (MON | TUE | WED | THU | FRI)
#define ANY (WEEKEND | WEEKDAY)

/*--------------------------------------------------------------------*/
/*   Table of values for schedules.  Based on values given for        */
/*   legal schedule keywords in "Managing uucp and Usenet" by         */
/*   O'Reilly & Associates.  Multiple entries for a single keyword    */
/*   are processed in logical OR fashion, just as multiple entries    */
/*   for the same host in L.sys are treated in a logical OR           */
/*   fashion.                                                         */
/*                                                                    */
/*   Timing windows are adjusted five minutes away from higher        */
/*   telephone rates in an attempt to avoid massive charges           */
/*   because of inaccurate PC clocks and the fact that a telephone    */
/*   call generally requires more than one minute if the system is    */
/*   trying to do useful work.                                        */
/*                                                                    */
/*   Does not support multiple keywords in one token                  */
/*   (TuFr0800-0805), but allows multiple tokens                      */
/*   (Tu0800-805,Fr0800-0805) on one line.                            */
/*                                                                    */
/*   The NonPeak keyword has been corrected to the current (May       */
/*   1989) NonPeak hours for Telenet's PC-Pursuit.  However, keep     */
/*   in mind the PC-Pursuit customer agreement specifies that you     */
/*   can't use PC-Pursuit to network multiple PC's, so thou shalt     */
/*   not use PC-Pursuit from a central mail server.  *sigh*           */
/*                                                                    */
/*   I also have Reach-Out America from ATT, for which night rates    */
/*   begin at 10:00 pm.  It is duly added to the table.               */
/*--------------------------------------------------------------------*/

static struct tTable {
   char *keyword;
   int wdays;
   unsigned int start;
   unsigned int end;

} table[] = {
   {"Any",     ANY,         0, 2400},
   {"Wk",      WEEKDAY,     0, 2400},
   {"Su",      SUN,         0, 2400},
   {"Mo",      MON,         0, 2400},
   {"Tu",      TUE,         0, 2400},
   {"We",      WED,         0, 2400},
   {"Th",      THU,         0, 2400},
   {"Fr",      FRI,         0, 2400},
   {"Sa",      SAT,         0, 2400},
   {"Evening", WEEKDAY,  1705,  755},
   {"Evening", WEEKEND,     0, 2400},
   {"Night",   WEEKDAY,  2305,  755},
   {"Night",   SAT,         0, 2400},
   {"Night",   SUN,      2305, 1655},
   {"NonPeak", WEEKDAY,  1805,  655}, /* Subject to change at TELENET's whim */
   {"NonPeak", WEEKEND,     0, 2400},
   {"Never",   NEVER,       0, 2400},
   {nil(char),}
}; /* table */

/*--------------------------------------------------------------------*/
/*    c h e c k t i m e                                               */
/*                                                                    */
/*    Validate a time of day field                                    */
/*--------------------------------------------------------------------*/

boolean checktime(const char *xtime, time_t delta)
{

   struct tTable *tptr;
   struct tm *tm_now;
   time_t secs_now;
   size_t hhmm;
   int   weekday;
   static char  _Far buf[BUFSIZ];
   char  *token;
   char  *nexttoken;
   char  found = 0;           /* Did not yet find current keyword    */
   boolean  dial  = FALSE;    /* May dial host at this time          */
   char  tdays[10];           /* String version of user tokens       */
   char  tstart[10];
   char  tend[10];
   size_t istart;
   size_t iend;
   static char  *wday[] = { "Sun","Mon","Tue","Wed","Thu","Fri","Sat" };

   strcpy(buf,xtime);         /* Copy time to local buffer we can alter */
   time(&secs_now);
   secs_now = secs_now + delta;
   tm_now = localtime(&secs_now);
                                        /* Create structure with time    */
   weekday = SUN >> tm_now->tm_wday;   /* Get day of week as single bit */
   hhmm = tm_now->tm_hour*100 + tm_now->tm_min;
   nexttoken = buf;           /* First pass, look at start of buffer    */

   while (!(dial) && ((token = strtok(nexttoken,",")) != NULL) )
   {

/*--------------------------------------------------------------------*/
/*     Parse a day/time combination from the L.SYS file         *     */
/*--------------------------------------------------------------------*/

      strcpy(tstart,"0000");  /* Set default times to all day           */
      strcpy(tend,"2400");

      sscanf(token,"%[ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz]"
	"%[0123456789]-%[0123456789]",tdays,tstart,tend);
      assert((strlen(tstart)<10)
         && (strlen(tend)<10)
         && (strlen(tdays)<10));
      printmsg(8,"checktime: %s broken into '%s' from '%s' to '%s'",
	       token,tdays,tstart,tend);

      istart = atoi(tstart);  /* Convert start/end times to binary          */
      iend  = atoi(tend);

/*--------------------------------------------------------------------*/
/*    Handle case of midnight specified in such a way that the        */
/*    time wraps through the daylight hours; we'll take the           */
/*    conservative approach that the user really meant to start at    */
/*    midnight.                                                       */
/*--------------------------------------------------------------------*/

      if ((istart > iend) && (istart == 2400))
	 istart = 0;

      /* Search for the requested keyword */

      for (tptr = table, found = FALSE;
	    (tptr->keyword != nil(char)) && !dial; tptr++)
      {

/*--------------------------------------------------------------------*/
/*      We found the keyword, see if today qualifies for dialing      */
/*--------------------------------------------------------------------*/

         if (equal(tptr->keyword,tdays))
         {
            found = TRUE;     /* Win or Lose, keyword is valid          */
            if (weekday & tptr->wdays)    /* Can we dial out today?     */
            {                             /* Yes --> Check the time     */

/*--------------------------------------------------------------------*/
/*    This entry allows us to dial out today; now determine if the    */
/*    time slot in the table allows dialing.                          */
/*--------------------------------------------------------------------*/

               if (tptr->start > tptr->end)  /* span midnight?          */
                  dial = (tptr->start <= hhmm) || (tptr->end >= hhmm);
               else
                  dial = (tptr->start <= hhmm) && (tptr->end >= hhmm);

/*--------------------------------------------------------------------*/
/*    Now do a logical AND of that time with the time the user        */
/*    specified in L.sys for this particular system.                  */
/*--------------------------------------------------------------------*/

               if (istart > iend)            /* span midnight?          */
                  dial = ((istart <= hhmm) || (iend >= hhmm)) && dial;
               else if (istart == iend)
		  dial = (istart == hhmm) && dial;
               else
                  dial = (istart <= hhmm) && (iend >= hhmm) && dial;
            } /* if */
         } /* if */
      } /* for */
      if (!found)
         printmsg(0,"checktime: keyword '%s' not found",token);
      nexttoken = NULL;       /* Continue parsing same string           */
   }

   printmsg(3,"checktime: call window '%s' %s at %d:%02d (%s)",
         dial ? token : xtime,
         dial ? "open" :"closed",
         tm_now->tm_hour,tm_now->tm_min,
         wday[tm_now->tm_wday]);
   return (dial);
} /*checktime*/
