#include <time.h>
#include <stdio.h>

#include "arpadate.h"

static char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static char *wdays[]  = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/*
 * Unix Date
 */
char *udate(void)
{
	static char out[50];
	time_t now;
	static time_t save_t = 0;
	struct tm *t;

	time(&now);
	if (now == save_t)
		return out;
	t = localtime(&now);
	sprintf(out, "%3.3s %3.3s %02d %02d:%02d:%02d %04d",
			 wdays[t->tm_wday],
			 months[t->tm_mon],
			 t->tm_mday,
			 t->tm_hour, t->tm_min, t->tm_sec,
			 1900 + t->tm_year );
	save_t = now;
	return out;
}

/*
 * Arpa Date
 * RFC1123 format  "Mon, 21 Nov 83 11:31:54 -0700\0"
 */
char *arpadate(void)
{
	static char out[50];
	char zname[10];
	time_t now;
	static time_t save_t = 0;
	struct tm *t;
	int tzshift;
	char tzsign;

	time(&now);
	if (now == save_t)
		return out;
	t = localtime(&now);
	tzshift = (int)(timezone - (t->tm_isdst != 0) * 3600)/60;
	tzsign = tzshift <= 0 ? '+' : '-';
	if ( tzshift < 0 ) tzshift = -tzshift;
	tzshift = 100*(tzshift/60) + (tzshift%60);
	sprintf(zname, tzshift ? "%c%04d" : "GMT", tzsign, tzshift);
	sprintf(out, "%3.3s, %2d %3.3s %4d %02d:%02d:%02d %s (%s)",
			wdays[t->tm_wday],
			t->tm_mday,
			months[t->tm_mon],
			t->tm_year + 1900,
			t->tm_hour, t->tm_min, t->tm_sec,
			zname, tzname[t->tm_isdst]
		);
	save_t = now;
	return out;
}
