/*--------------------------------------------------------------------*/
/*    s c r i p t . c                                                 */
/*                                                                    */
/*    Script processing routines for UUPC/extended                    */
/*                                                                    */
/*    John H. DuBois III  3/31/90                                     */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/*                        System include files                        */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include "lib.h"
#include "dcp.h"
#include "dcpsys.h"
#include "hostable.h"
#include "modem.h"
#include "script.h"
#include "ssleep.h"
#include "ulib.h"
#include "comm.h"

/*--------------------------------------------------------------------*/
/*                           Local defines                            */
/*--------------------------------------------------------------------*/

#define prefix(small,large)   (equaln((small), (large), strlen(small)))
#define notin(str,log)        (strstr((log), (str)) == nil(char))
#define MAXMATCH 64              /* max length of search string; must
									be a power of 2                  */
#define QINDMASK (MAXMATCH - 1)  /* bit mask to get queue index      */

#define EOTMSG "\004\r\004\r"

boolean secure_output = FALSE, use_secure_output = FALSE;
extern INTEGER scriptTimeout;
extern char *S_sysspeed;
extern boolean ModemMode;
extern boolean echochk;
extern boolean NoCheckCarrier;
extern int maxwait;
extern time_t startwait;
/*--------------------------------------------------------------------*/
/*                    Internal function prototypes                    */
/*--------------------------------------------------------------------*/

static int StrMatch(char *MatchStr,char C);
								 /* Internal match routine           */
static int writestr(register char *s);

/*
 *       e x p e c t s t r
 *
 *       wait for a pattern on input
 *
 *
 *       expectstr reads characters from input using sread, and
 *       compares them to a search string.  It reads characters until
 *       either the search string has been seen on the input or a
 *       specified timeout interval has passed without any characters
 *       being read.
 *
 *      Input parameters:
 *      Search is the string that is searched for on the input.
 *      Timeout is the timeout interval passed to sread.
 *
 *      Output parameters: none.
 *
 *      Return value:
 *      TRUE is returned if the search string is found on input.
 *      FALSE is returned if sread times out.
 */

int expectstr(char *Search, unsigned int Timeout)
{
		static char _Far buf[BUFSIZ];
		register char *ptr;
		int r, i;
		time_t dline;

		printmsg(5, "wanted \"%s\"", Search);

		if (!strlen(Search) || equal(Search, "\"\""))              /* expects nothing */
			return S_OK;

		StrMatch(Search,'\0');   /* set up search string in StrMatch */

		if (debuglevel >= 4)
		   dbgputs("got \"");

		if ((i = w_flush()) <= 0) {
			r = i < 0 ? S_LOST : S_TIMEOUT;
			goto Lost;
		}

		ptr = buf;
		dline = time(nil(time_t)) + Timeout;
		do {
		   if (ptr == &buf[sizeof(buf)-1])
			 ptr = buf;          /* Save last character for term \0  */

		   switch (r = sread(ptr, 1, (int) (dline - time(nil(time_t))))) {
		   case S_TIMEOUT:
		   case S_LOST:
Lost:
			  if (debuglevel >= 4)
				 dbgputs(r == S_LOST ? "\" ***LOST CARRIER***\n"
						     : "\" ***TIMEOUT***\n");
			  return r;
		   }
		   *ptr &= 0x7f;
		   if ((!use_secure_output || !secure_output) && debuglevel >= 4)
			show_char(*ptr);
		   if (*ptr == '\n')
			secure_output = FALSE;
		} while (!StrMatch(NULL, *ptr++));

		if(debuglevel >= 4)
			dbgputs("\"\n");

		if (ModemMode)
			ddelay(500);

		return S_OK;

} /*expectstr*/


/*
 *      StrMatch: Incrementally search for a string.
 *      John H. DuBois III  3/31/90
 *      StrMatch searches for a string in a sequence of characters.
 *      The string to search for is passed in an initial setup call.
 *      Further calls with the search string set to NULL pass one
 *      character per call.
 *      The characters are built up into an input string.
 *      After each character is added to the input string,
 *      the search string is compared to the last length(search string)
 *      characters of the input string to determine whether the search
 *      string has been found.
 *
 *      Global variables: none.
 *
 *      Input parameters:
 *      MatchStr is the string to search for.
 *      It is not copied; a static pointer to it is saved.
 *      C is the character to add to the input string.
 *      It is ignored on a setup call.
 *
 *      Output parameters: None.
 *
 *      Return value:
 *      On the setup call, -1 is returned if the search string is
 *      longer than the input string buffer.  Otherwise, 0 is returned.
 *
 *      On comparison calls, 1 is returned if the search string has
 *      been found.  Otherwise 0 is returned.
 */


static int StrMatch(char *MatchStr, char C)
{
/*
 *      The input string is stored in a circular buffer of MAXMATCH
 *      characters.  If the search string is found in the input,
 *      then the last character added to the buffer will be the last
 *      character of the search string.  Therefore, the string
 *      compare will always start SearchLen characters behind the
 *      position where characters are added to the buffer.
 */

		static char Buffer[MAXMATCH];   /* Input string buffer */
		static char *Search;    /* Search string */
		static int  PutPos;     /* Where to add chars to string buffer */
		static int SearchPos;   /* Where in buffer to start string compare */
		int SearchLen;          /* Length of search string */
		int BufInd;             /* Index to input string buffer for string
								   compare */
		char *SearchInd;        /* Index to search string for string
								   compare */

		if (MatchStr) {                 /* Set up call */
				Search = MatchStr;
				SearchLen = strlen(Search);
				if (SearchLen > MAXMATCH) {
				   printmsg(0,"StrMatch: string to match '%s' is too long",
						Search);
				   return(-1);
				}
				memset(Buffer,'\0',MAXMATCH);   /* Clear buffer */
				PutPos = 0;
				SearchPos = MAXMATCH - SearchLen;
				return 0;
		}
		Buffer[ PutPos++ & QINDMASK] = C;
        for (BufInd = ++SearchPos,SearchInd = Search; *SearchInd; SearchInd++)
                if (Buffer[BufInd++ & QINDMASK] != *SearchInd)
                        return 0;

		return 1;
}

/*--------------------------------------------------------------------*/
/*    w r i t e s t r                                                 */
/*                                                                    */
/*    Send a string to the port during login                          */
/*--------------------------------------------------------------------*/

static int writestr(register char *s)
{
   register char last = '\0';
   boolean nocr  = FALSE;
   unsigned char digit;
   int r, len;
   char *p;

   if equal(s,"BREAK")
   {
	  ssendbrk(0);
	  return TRUE;               /* Don't bother with a CR after this   */
   }
   if (!ModemMode && strchr(s, '\\') == NULL) {
		len = strlen(s);
		if ((r = swrite(s, len)) != len)
			return r;
		return FALSE;
   }
   while (*s) {
	  if (last == '\\') {
		 last = *s;
		 switch (*s) {
		 case 'b':   /* backspace */
			if ((r = slowwrite("\b", 1)) != 1)
				return r;
			break;
		 case 'c':   /* don't output CR at end of string */
			nocr = TRUE;
			break;
		 case 'd':   /* delay */
			ssleep(2);
			break;
		 case 'E':
			echochk = TRUE;
			if (ModemMode)
				r_purge();
			break;
		 case 'e':
			echochk = FALSE;
			if (ModemMode)
				r_purge();
			break;
		 case 'L':
			if (hostp == NULL) {
				printmsg(0, "writestr: Can't determine host for login escape");
				if (!NoCheckCarrier && carrier(TRUE))
					hangup();
				return S_LOST;
			}
			p = hostp->login;
			if (p == NULL) {
				printmsg(0, "writestr: login not found for this host, check CALL file");
				if (!NoCheckCarrier && carrier(TRUE))
					hangup();
				return S_LOST;
			}
			len = strlen(p);
			if ((r = swrite(p, len)) != len)
				return r;
			break;
		 case 'N':   /* null */
			if ((r = slowwrite("", 1)) != 1)
				return r;
			break;
		 case 'n':   /* new line */
			if ((r = slowwrite("\n", 1)) != 1)
				return r;
			break;
		 case 'P':
			if (hostp == NULL) {
				printmsg(0, "writestr: Can't determine host for password escape");
				if (!NoCheckCarrier && carrier(TRUE))
					hangup();
				return S_LOST;
			}
			p = hostp->password;
			if (p == NULL) {
				printmsg(0, "writestr: password not found for this host, check CALL file");
				if (!NoCheckCarrier && carrier(TRUE))
					hangup();
				return S_LOST;
			}
			len = strlen(p);
			if ((r = swrite(p, len)) != len)
				return r;
			break;
		 case 'p':   /* delay */
			ddelay(250);
			break;
		 case 'K':   /* break */
			ssendbrk(0);
			break;
		 case 'r':   /* carriage return */
			if ((r = slowwrite("\r", 1)) != 1)
				return r;
			break;
		 case 's':   /* space */
			if ((r = slowwrite(" ", 1)) != 1)
				return r;
			break;
		 case 'T':
			printmsg(4, "writestr: phone number: `%s'", S_sysspeed);
			if (!NoCheckCarrier && carrier(TRUE)) {
				printmsg(0, "writestr: why carrier present while dialing? Check your modem.");
				hangup();
				return S_LOST;
			}
			if ((r = writestr(S_sysspeed)) < 0)
				return r;
			break;
		 case 't':   /* tab */
			if ((r = slowwrite("\t", 1)) != 1)
				return r;
			break;
		 case 'W':		 /* change wait time */
		 case 'z':		 /* set serial port speed */
			{
			char	 wtime[32];
			char*	 wcp = wtime;
			boolean	 st = (*s == 'W');

			++s;	 /* bump over 'W,z' */
			while(isdigit(*s))
				*wcp++ = *s++;
			*wcp = '\0';
			--s;	/* back again */

			if (st) {
				scriptTimeout = atoi(wtime);
				if(scriptTimeout < 1)
					scriptTimeout = 15;
				printmsg(8, "writestr: wait time specified `%s'. Set to %d",
							 wtime, scriptTimeout);
			}
			else
				SIOSpeed(atol(wtime));
			}
			break;
		 case 'v':
			dtr_off();
			break;
		 case '^':
			dtr_on();
			break;
		 case '0':
		 case '1':
		 case '2':
		 case '3':
		 case '4':
		 case '5':
		 case '6':
		 case '7':
			digit = 0;
			while( (*s >= '0') && (*s < '8'))
			   digit = (unsigned char) (digit * 8 + *s++ - '0');
			s--;              /* Backup before non-numeric char      */
			if ((r = slowwrite((unsigned char *) &digit, 1)) != 1)
				return r;
			break;

		 default: /* ordinary character */
			if ((r = slowwrite(s, 1)) != 1)
				return r;
			last = '\0';      /* Zap any repeated backslash (\)      */
		 }
	  }
	  else if (*s != '\\') {/* backslash */
		if ((r = slowwrite(s, 1)) != 1)
			return r;
	  }
	  else
		 last = *s;
	  s++;
   }

   return nocr;

} /*writestr*/


/*
   s e n d s t r

   Send line of login sequence
*/

int sendstr(char *str)
{
   int r;

   printmsg(2, "sending \"%s\"", str);

   if (equaln(str, "BREAK", 5)) {
	  int   nulls;
	  nulls = atoi(&str[5]);
	  if (nulls <= 0 || nulls > 10)
		 nulls = 3;
	  ssendbrk(nulls);  /* send a break signal */
	  return S_OK;
   }

   if (equal(str, "EOT")) {
	  if ((r = slowwrite(EOTMSG, strlen(EOTMSG))) != strlen(EOTMSG))
		  return r;
   }

   if (equal(str, "\"\""))
	  *str = '\0';

   r = 0;
   if (!equal(str,"")) {
	  r = writestr(str);
	  if (r < 0)
		return r;
	  if (!r) {
		 if (ModemMode)
			r_purge();
		 if ((r = slowwrite("\r", 1)) != 1)
			return r;
	  }
   } else {
	  if (ModemMode)
		r_purge();
	  if ((r = slowwrite("\r", 1)) != 1)
		return r;
   }

   return S_OK;

} /*sendstr*/

/*
   s e n d e x p e c t
*/

int sendexpect(char *send, char *expect, int timeout)
{
   int r;

   if ((r = sendstr(send)) != S_OK)
		return r;
   return expectstr(expect, timeout);

} /*sendexpect*/

/*
	r e s p o n s e

	waits for list of messages from modem
*/
char respbuf[256];

int response(mdmsgs* patterns, int oldwait) {
	mdmsgs*	pp;
	char*	cp = respbuf;
	int 	state, i;
	boolean	got = FALSE;

	printmsg(5, "response: initial timeout %d secs", maxwait);

	if ((i = w_flush()) <= 0) {
		if (i < 0)
			goto Lost;
		else
			goto Tout;
	}

	state = 0;

	while(cp < respbuf + sizeof(respbuf)) {
		char	ch;

		switch (sread(&ch, 1, (int) (startwait + maxwait - time(nil(time_t))))) {
		case S_TIMEOUT:
		Tout:
			if (oldwait == maxwait && debuglevel >= 4) {
				if (!got)
					dbgputs("got \"");
				dbgputs("\" timeout\n");
			}
			return S_TIMEOUT;
		case S_LOST:
		Lost:
			if (debuglevel >= 4) {
				if (!got)
					dbgputs("got \"");
				dbgputs("\" failed\n");
			}
			return S_LOST;
		}
		if (ch == ' ' || ch == '\t') {
			state = 0;
			continue;
		}
		if (ch == '\r') {
			state = 1;
			goto savech;
		}
		if (ch == '\n' && state == 1) {
			*(--cp) = '\0';
			for(pp = patterns; pp->msg_text != NULL; pp++)
				if(strstr(respbuf, pp->msg_text) != nil(char)) {
					if (debuglevel >= 4) {
						if (!got)
							dbgputs("got \"");
						dbgputs("^J\"\n");
					}
					return pp->msg_code;
				}
			cp = respbuf;
			state = 0;
		} else {
			state = 0;
	savech:
			*cp++ = ch;
		}
		if (debuglevel >= 4) {
			if (!got) {
				dbgputs("got \"");
				got = TRUE;
			}
			show_char(ch);
		}
	}
	if (debuglevel >= 4) {
		if (!got)
			dbgputs("got \"");
		dbgputs("\" buffer owerflow\n");
	}
	return S_LOST;
}