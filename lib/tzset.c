/*-
 * Copyright (C) 1992-1996 by Andrey A. Chernov, Moscow, Russia.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma option -zC_TEXT

#include <io.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TLEN 3

#define MSKDaylight  1
#define MSKZone -3L
#define MSKTZName   "MSK"
#define MSKDSTName  "MSD"   

static char Zone[TLEN+1] = MSKTZName, Light[TLEN+1] = MSKDSTName;
#ifdef __TURBOC__
char _FAR *const _Cdecl tzname[2] = {Zone, Light};
#else
char * tzname[2] = {Zone, Light};
#endif

long  timezone = MSKZone * 60L * 60L; /* Set for MSK */

int   daylight = MSKDaylight;      

extern char *_tz;

int AllowTzset = 1;

#ifdef __TURBOC__
void  cdecl _FARFUNC tzset(void)
#else
void tzset(void)
#endif
{
	char *e;

	if (!AllowTzset)
		return;

	if ((e = getenv("TZ")) == NULL && (e = _tz) == NULL) {
		daylight = MSKDaylight;
		timezone = MSKZone * 60L * 60L;
		strcpy(tzname[0], MSKTZName);
		strcpy(tzname[1], MSKDSTName);
	}
	else {
		*tzname[1] = '\0';
		strncpy(tzname[0], e, TLEN);
		tzname[0][TLEN] = '\0';   
		timezone = atol(&e[TLEN]) * 3600L; 
		daylight = 0;
		for (e += TLEN; *e; e++) {
			if (isalpha(*e)) {
				strncpy(tzname[1], e, TLEN);
				tzname[1][TLEN] = '\0';
				daylight = 1;
				break;
			}
		}
	}
}

#ifdef __TURBOC__

#pragma startup tzset 20

#define M_START_DST 3 /* March */
#define M_END_DST   10 /* October */

/* Derived from astrolog sources */

static unsigned long mdytojulian(unsigned mon, unsigned day, unsigned yea)
{
  unsigned long im, j;

  im = 12*((unsigned long)yea+4800)+mon-3;
  j = (2*(im%12)+7+365*im)/12;
  j = j+day+im/48-32083;
  if (j > 2299171L)              /* Take care of dates in */
	j += im/4800-im/1200+38;    /* Gregorian calendar.   */
  return j;
}

static short mdays[12] = {
   31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int pascal near __isDST(unsigned hour, unsigned mday, unsigned month, unsigned year)
{
	register unsigned i;

	year += 1970;
	mdays[1] = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
		  ? 29 : 28;
	if (month == 0)		/* if only day of year given	*/
	{
		mday++;
		for (i = 0; i < 12 && mday > mdays[i]; i++)
			mday -= mdays[i];
		month = i + 1;
	}

	/* Test for March/September */
	if (month < M_START_DST || month > M_END_DST)
		return 0;
	if (month != M_START_DST && month != M_END_DST)
		return 1;

	i = mdays[month-1] - (int)((mdytojulian(month, mdays[month-1], year) + 1) % 7);

	/* Test for last Sunday */
	if (mday < i)
		return (month == M_END_DST);
	if (mday > i)
		return (month == M_START_DST);

	/* Test for 2am (March) and 3am (October) */
	return (   (month == M_START_DST && hour >= 2)
			|| (month == M_END_DST && hour < 3)
			);
}
#endif