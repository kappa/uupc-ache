/*--------------------------------------------------------------------*/
/*    m o d e m . c                                                   */
/*                                                                    */
/*    High level modem control routines for UUPC/extended             */
/*                                                                    */
/*    Copyright (c) 1991 by Andrew H. Derbyshire                      */
/*                                                                    */
/*    Change history:                                                 */
/*       21 Apr 91      Create from dcpsys.c                          */
/*--------------------------------------------------------------------*/

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <ctype.h>

#include "lib.h"
#include "arpadate.h"
#include "checktim.h"
#include "dcp.h"
#include "dcpsys.h"
#include "hlib.h"
#include "hostable.h"
#include "modem.h"
#include "script.h"
#include "ssleep.h"
#include "ulib.h"
#include "comm.h"
#include "mnp.h"
#include "screen.h"

char *device = NULL;    /*Public to show in login banner     */

#define DEF_WAIT_TICS 0
#define DEF_TIMEOUT 3
/* Modem information table */
#define MDM_INIT 0
#define MDM_RING 1
#define MDM_ANSWER 2
#define MDM_DIAL 3

#define MDM_NKIND 4

#define M_CONNECT 0
#define M_BUSY 11
#define M_LOST 12
#define M_ERROR 13

typedef boolean (*modemp)(void);

typedef struct {
	char*	mdsend;
	char*	mdexp;
	int		mdtime;
	char	mdkind;
} mdinit;

char* commrate[32];
char* phonerate[32];
int	set_speed;
boolean Direct = FALSE, Front = FALSE;
boolean echochk = FALSE;
boolean ModemMode = FALSE;
extern  boolean NoCheckCarrier;
extern  boolean WorthTrying;
extern  boolean secure_output, use_secure_output;
extern  char respbuf[256];
extern int maxwait;
extern boolean use_plus_minus;
extern char *FrontRate;
time_t startwait;

#define MAXCMDI 32
#define MAXCMSG 16
static mdinit _Far commd[MDM_NKIND][MAXCMDI+2];
static mdmsgs _Far commsg[MAXCMSG+2];


INTEGER chardelay = 55,	/* Default delay after character send */
		dialTimeout,
		modemTimeout,
		scriptTimeout = 15;
static	modemp	 driver;
extern char	 *brand;
static char 	 *ModemName = NULL;
extern char	 *S_sysspeed;
extern char	 *rate;
static char	 *hang_str;
static char	 *stat_str = NULL;
int WaitRing;
extern MaxFailAttempts, fail_attempts, Max_Idle, Seq_Attempts;
extern int MaxTry;
#ifdef __MSDOS__
extern MnpEmulation, MnpWaitTics;
#endif
extern void (*p_hangup)(void);
static boolean direct(void);
static boolean SetModem(char *);
static boolean cdial(int, boolean);

/*--------------------------------------------------------------------*/
/*                    Internal function prototypes                    */
/*--------------------------------------------------------------------*/

static boolean sendalt( char *string, int timeout);

/*--------------------------------------------------------------------*/
/*              Define current file name for references               */
/*--------------------------------------------------------------------*/

currentfile();

void increase_idle(void)
{
   hostp->idlewait *= 3;
   hostp->idlewait /= 2;
   if (Max_Idle >= 0 && hostp->idlewait > 60L * Max_Idle)
	 hostp->idlewait = Max_Idle * 60L;
   if (hostp->idlewait < MIN_IDLE)
	 hostp->idlewait = MIN_IDLE;
}

/*--------------------------------------------------------------------*/
/*    c a l l u p                                                     */
/*                                                                    */
/*    script processor - nothing fancy!                               */
/*--------------------------------------------------------------------*/

CONN_STATE callup()
{
   char *exp;
   int i;

   /* Dial a modem  */
   driver = NULL;
   p_hangup = NULL;
#ifdef __MSDOS__
   MnpEmulation = 0;
   MnpWaitTics = DEF_WAIT_TICS;
#endif
   ModemName = flds[FLD_TYPE];
/*--------------------------------------------------------------------*/
/*      Determine if the window for calling this system is open       */
/*--------------------------------------------------------------------*/

   if ( !callnow && equal(flds[FLD_CCTIME],"Never" ))
											 /* Don't update if we   */
	  return CONN_INITIALIZE;                /* never try calling    */

   time(&hostp->status.ltime); /* Save time of last attempt to call   */

/*--------------------------------------------------------------------*/
/*    Check the time of day and whether or not we should call now.    */
/*                                                                    */
/*    If calling a system to set the clock and we determine the       */
/*    system clock is bad (we fail the sanity check of the last       */
/*    connected a host to being in the future), then we ignore the    */
/*    time check field.                                               */
/*--------------------------------------------------------------------*/

   if (!(callnow || checktime(flds[FLD_CCTIME],(time_t) 0)))
   {
	hostp->status.hstatus = wrong_time;
	return CONN_INITIALIZE;
   } /* if */

/*--------------------------------------------------------------------*/
/*             Announce we are trying to call the system              */
/*--------------------------------------------------------------------*/

   printmsg(1, "callup: calling \"%s\" via %s at %s on %s",
		  rmtname, ModemName, flds[FLD_PHONE], arpadate());

/*--------------------------------------------------------------------*/
/*                     Get the modem information                      */
/*--------------------------------------------------------------------*/

	if (   equal(ModemName, "DIR")
	    || ipport(ModemName)
	   ) {
		driver = direct;
		Direct = TRUE;
	}
	else {   /* Modem from Dialers */
		if (!SetModem(ModemName)) {
			hostp->status.hstatus = invalid_device;
			return CONN_INITIALIZE;
		}
	}
	brand = ModemName;
	if (Front)
		driver = direct;

   if (driver == NULL)
   {
	  printmsg(0,"callup: Unsupported modem type \"\%s\"",
			   ModemName);
	  hostp->status.hstatus = invalid_device;
	  return CONN_INITIALIZE;
   }

/*--------------------------------------------------------------------*/
/*                         Dial the telephone                         */
/*--------------------------------------------------------------------*/

   Slink(SL_CALLING, rmtname);

#ifdef __MSDOS__
   if (!Direct && !Front)
	   set_answer_mode(FALSE);
#endif

   hostp->status.hstatus =  dial_script_failed; /* Assume failure          */

   if (!driver())	/* Check lost carrier now */
	   return CONN_DROPLINE;

/*--------------------------------------------------------------------*/
/*             The modem is connected; now login the host             */
/*--------------------------------------------------------------------*/

   hostp->status.hstatus =  script_failed; /* Assume failure          */
   use_secure_output = TRUE;
   secure_output = FALSE;
   for (i = FLD_EXPECT; i < kflds; i += 2) {

	  exp = flds[i];
	  printmsg(2, "expecting %d of %d \"%s\"", i, kflds, exp);
	  if (!sendalt( exp, scriptTimeout ))
	  {
		 printmsg(0, "callup: login script FAILED, bad line quality (maybe)");
		 use_secure_output = FALSE;
		 return CONN_DROPLINE;
	  } /* if */

	  printmsg(2, "callup: sending %d of %d \"%s\"",
				   i + 1, kflds, flds[i + 1]);
	  secure_output = strstr(flds[i + 1], "\\P") || strstr(flds[i + 1], "\\L");
	  if (sendstr(flds[i + 1]) != S_OK) {
		use_secure_output = FALSE;
		return CONN_DROPLINE;
	  }
   } /*for*/
   use_secure_output = FALSE;

   return CONN_PROTOCOL;

} /*callup*/

/*--------------------------------------------------------------------*/
/*    c a l l i n                                                     */
/*                                                                    */
/*    Answer the modem in passive mode                                */
/*--------------------------------------------------------------------*/

CONN_STATE callin( const char *logintime )
{
   int    offset;             /* Time to wait for telephone          */
   char buf[80];

/*--------------------------------------------------------------------*/
/*    Determine how long we can wait for the telephone, up to         */
/*    MAX_INT seconds.  Aside from Turbo C limits, this insures we    */
/*    kick the modem once in a while.                                 */
/*--------------------------------------------------------------------*/

   if  (logintime == NULL)    /* Any time specified?                 */
	  offset = INT_MAX;       /* No --> Run almost forever           */
   else {                     /* Yes --> Determine elapsed time      */
	  int delta = 4096;       /* Rate at which we change offset      */
	  boolean split = FALSE;

	  if (!checktime(logintime,(time_t) 0)) /* Still want system up? */
		 return CONN_EXIT;             /* No --> shutdown            */

	  offset = 0;             /* Wait until end of this minute       */
	  while ( ((INT_MAX - delta) > offset ) && (delta > 0))
	  {
		 if (checktime(logintime,(time_t) offset + delta))
			offset += delta;
		 else
			split = TRUE;     /* Once we starting splitting, we
								 never stop                          */
		 if ( split )
			delta /= 2;
	  } /* while */
   } /* else */

/*--------------------------------------------------------------------*/
/*                        Open the serial port                        */
/*--------------------------------------------------------------------*/

   if (E_inmodem == NULL)
   {
	printmsg(0,"callin: No modem name supplied for incoming calls!");
	return CONN_EXIT;
   } /* if */

   p_hangup = NULL;
   ModemName = E_inmodem;
#ifdef __MSDOS__
   MnpEmulation = 0;
   MnpWaitTics = DEF_WAIT_TICS;
#endif
   if (equal(E_inmodem, "DIR") || ipport(E_inmodem))
	 Direct = TRUE;
   else if (!SetModem(E_inmodem))  /* Initialize modem configuration      */
	return CONN_EXIT;
   brand = E_inmodem;

   if ((!Direct && !Front && commd[MDM_RING][0].mdexp == NULL) || (E_inspeed == NULL))
   {
	printmsg(0,"callin: Missing inspeed and/or ring values in modem \
configuration file.");
	return CONN_EXIT;
   } /* if */

/*--------------------------------------------------------------------*/
/*                    Open the communications port                    */
/*--------------------------------------------------------------------*/

   if (openline(P_Flag ? P_Flag : E_indevice, E_inspeed) == -1)
	  return CONN_EXIT;

   if (!Direct && !Front) {
/*--------------------------------------------------------------------*/
/*                        Initialize the modem                        */
/*--------------------------------------------------------------------*/

	   if (!cdial(MDM_INIT, FALSE))
	   {
		  printmsg(0,"callin: Modem failed to initialize");
		  return CONN_EXIT;
	   }
	   if (!NoCheckCarrier && carrier(TRUE))
	   {
		  printmsg(0, "callin: why carrier present after init? Check your modem.");
		  return CONN_EXIT;
	   }

/*--------------------------------------------------------------------*/
/*                   Wait for the telephone to ring                   */
/*--------------------------------------------------------------------*/

	   WaitRing = offset;
	   if (offset == INT_MAX)
		   strcpy(buf, "ever");
	   else
		   sprintf(buf, " %d minutes until %s", offset / 60, logintime);
	   printmsg(-1, "callin: monitoring port %s device %s for%s",
					 E_indevice, E_inmodem , buf);

	   if (!cdial(MDM_RING, FALSE))
							  /* Did it ring?                        */
		  return CONN_DROPLINE;     /* No --> Return to caller       */

#ifdef __MSDOS__
	   set_answer_mode(TRUE);
#endif

	   if (!cdial(MDM_ANSWER, FALSE))
							  /* Pick up the telephone               */
	   {
		  printmsg(0,"callin: modem failed to connect to incoming call");
		  return CONN_DROPLINE;
	   }

	   printmsg(14, "callin: got CONNECT");
   }	/* !Direct */

   if (!set_connected(TRUE))  {	/* Check lost carrier now */
	printmsg(0,"callin: no carrier present on incoming call");
	return CONN_DROPLINE;
   }

   memset( &remote_stats, 0, sizeof remote_stats);
							  /* Clear remote stats for login        */
   time(&remote_stats.ltime); /* Remember time of last attempt conn  */
   remote_stats.calls ++ ;
   return CONN_LOGIN;

} /* callin */


/*--------------------------------------------------------------------*/
/*    s e n d a l t                                                   */
/*                                                                    */
/*    Expect a string, with alternates                                */
/*--------------------------------------------------------------------*/

static boolean sendalt( char *exp, int timeout)
{
   boolean ok = FALSE;
   int r;

   while (ok != TRUE) {
	  char *alternate = strchr(exp, '-');

	  if (alternate != nil(char))
		 *alternate++ = '\0';

	  r = expectstr(exp, timeout);

	  switch (r) {
	  case S_LOST:
		 return FALSE;
	  case S_TIMEOUT:
		 break;
	  default:
		 printmsg(2, "got that");
		 return TRUE;
	  }

	  if (alternate == nil(char))
		 return FALSE;

	  exp = strchr(alternate, '-');
	  if (exp != nil(char))
		 *exp++ = '\0';

	  printmsg(4, "sending alternate \"%s\"", alternate);
	  secure_output = strstr(alternate, "\\P") || strstr(alternate, "\\L");
	  if (sendstr(alternate) != S_OK)
		return FALSE;
   } /*while*/
   return TRUE;

} /* sendalt */

/*--------------------------------------------------------------------*/
/*    s l o w w r i t e                                               */
/*                                                                    */
/*    Write characters to the serial port at a configurable           */
/*    snail's pace.                                                   */
/*--------------------------------------------------------------------*/

#define MAXLEN 47

int slowwrite( unsigned char *s, int len)
{
   int r = swrite( s , len );

   if (r > 0 && ModemMode) {
	  if (echochk && len == 1) {
		  char buf[1];
		  int i;

		  if ((i = w_flush()) <= 0)
			  return i < 0 ? S_LOST : S_TIMEOUT;
		  for (i = 0; i < MAXLEN; i++) {
			  r = sread(buf,1,1);
			  if (r > 0) {
				  if (*buf != *s) {
					  if (*buf == '\0' || strchr("\n\r\b ", *buf) != NULL)
						continue;
					  return S_LOST;
				  }
				  else
					break;
			  }
			  else
				break;
		  }
		  if (i >= MAXLEN)
			  return S_LOST;
	  }
	  else if (chardelay > 0)
		ddelay(chardelay);
   }
   return r;
} /* slowwrite */

/*
   d i r e c t

   Opens a line for a direct connection
 */
static boolean direct(void)
{
   WorthTrying = TRUE;

   if (Front) {
	if (openline(P_Flag ? P_Flag : E_indevice, E_inspeed) == -1) {
		printmsg(0, "direct: can't open serial port %s speed %s",
			P_Flag ? P_Flag : E_indevice, E_inspeed);
		return FALSE;
	}
   } else {
	if (openline(P_Flag ? P_Flag : flds[FLD_DEV], flds[FLD_PHONE]) == -1) {
		printmsg(0, "direct: can't open serial port %s speed %s",
			flds[FLD_TYPE], flds[FLD_PHONE]);
		goto DirFailedAttempt;
	}
   }

   if (!set_connected(TRUE)) {	/* Check lost carrier now */
	printmsg(0,"direct: no carrier present on outgoing call");
	if (Front)
		return FALSE;
DirFailedAttempt:
	if (hostp != NULL)
		hostp->hattempts = fail_attempts;
	fail_attempts++;
	if (hostp != NULL)
		hostp->status.hstatus =  dial_failed; /* Assume failure          */
	if (MaxFailAttempts > 0) {
		if (fail_attempts >= MaxFailAttempts) {
			printmsg(-1, "direct: failed attempts count (%d) exceeded", MaxFailAttempts);
			if (hostp != NULL)
				hostp->status.hstatus =  max_retry; /* Assume failure          */
		} else
			printmsg(-1, "direct: attempt #%d/%d failed", fail_attempts, MaxFailAttempts);
	}
	else
			printmsg(-1, "direct: attempt #%d failed", fail_attempts);
	closeline();
	return FALSE;
   }

   rate = (FrontRate != NULL) ? FrontRate : flds[FLD_PHONE];
   printmsg(5, "direct: connected at %s", rate);
   Smodem(SM_CONNECT, 0, 0, 0);

   time( &remote_stats.lconnect );
   remote_stats.calls ++ ;
   increase_idle();

   return TRUE;         /* Return success */
}

static boolean commdm(void)
{
	boolean new;

	WorthTrying = TRUE;
	new = (commd[MDM_DIAL][0].mdsend != NULL);	/* New dialer format */
	if (!cdial(new ? MDM_DIAL : MDM_INIT, TRUE))
		return FALSE;

	if (!set_connected(TRUE)) {	/* Check lost carrier now */
		printmsg(0,"commdm: no carrier present on outgoing call");
		return FALSE;
	}

	time( &remote_stats.lconnect );
	remote_stats.calls ++;
	increase_idle();

	return TRUE;
}

static boolean cdial(int kind, boolean dial) {
	static int irate;
	static int frate;
	int answer, trycount;
	mdinit *script;
	int iii=0, wtime, i, oldkind;
	char *s;
	boolean firsttime = TRUE, changed;
	time_t waittime;

  trycount = 0;
  changed = FALSE;

  if (dial) {
	irate = frate = 0;
	S_sysspeed = phonerate[frate];    /* Phone loop */
  }

  for (;;) {	/* Loop in Speeds */
	if (dial && trycount >= MaxTry) {
		irate++;
		trycount = 0;
		printmsg(4, "cdial: choose next speed from list");
	}
	rate = dial ? commrate[irate] : E_inspeed;

redial:
	if (dial && hostp != NULL && (changed || (firsttime && fail_attempts > 0))) {
		if (hostp->status.hstatus == call_failed) {
			waittime = hostp->idlewait - (time(NULL) - hostp->status.lconnect);
			if (waittime <= 0)
				waittime = MIN_IDLE;
		}
		else
			waittime = hostp->idlewait;

		if (changed || (MaxFailAttempts > 0 && fail_attempts >= MaxFailAttempts)) {
FailedAttempt:
			if (hostp != NULL)
				hostp->hattempts = fail_attempts;
			fail_attempts++;
			if (hostp != NULL)
				hostp->status.hstatus =  dial_failed; /* Assume failure          */
			if (MaxFailAttempts > 0) {
				if (fail_attempts >= MaxFailAttempts) {
					printmsg(-1, "cdial: failed attempts count (%d) exceeded", MaxFailAttempts);
					if (hostp != NULL)
						hostp->status.hstatus =  max_retry; /* Assume failure          */
				} else
					printmsg(-1, "cdial: attempt #%d/%d failed", fail_attempts, MaxFailAttempts);
			}
			else
					printmsg(-1, "cdial: attempt #%d failed", fail_attempts);
			closeline();
			return FALSE;
		}

		if ((fail_attempts + 1) % Seq_Attempts == 0) { /* End of Serial */
			if (MaxFailAttempts > 0)
				printmsg(-1, "cdial: attempt #%d/%d, idle wait %ld secs",
							 fail_attempts + 1, MaxFailAttempts, waittime);
			else
				printmsg(-1, "cdial: attempt #%d, idle wait %ld secs",
							 fail_attempts + 1, waittime);
			if (ssleep(waittime)) {
				hostp->idlewait = MIN_IDLE;
				printmsg(-1, "cdial: reset idle wait time to %ld", hostp->idlewait);
			}
			else
				increase_idle();
		}
		else {
			if (MaxFailAttempts > 0)
				printmsg(-1, "cdial: attempt #%d/%d",
							 fail_attempts + 1, MaxFailAttempts);
			else
				printmsg(-1, "cdial: attempt #%d",
							 fail_attempts + 1);
		}
	}
	firsttime = FALSE;

	Direct = FALSE;
	if (dial) {
		if (openline(P_Flag ? P_Flag : flds[FLD_DEV], rate)) {
			printmsg(0, "cdial: can't open serial port %s at %s baud.", flds[FLD_TYPE], rate);
			goto FailedAttempt;
		}
	}
	script = &commd[kind][0];
	answer = 1;
	echochk = FALSE;
	oldkind = -1;
	for( ; script->mdexp != NULL && answer > 0; script++, iii++) {
		if (oldkind != script->mdkind)
			iii = 0;
		wtime = (iii == 0 && kind == MDM_RING) ? WaitRing : script->mdtime;
		printmsg(5, "cdial: %d-type initialize %d at %s baud, pass %d/%d",
					script->mdkind, iii, rate, trycount + 1, MaxTry);
		Smodem(SM_INIT, iii, script->mdkind, trycount + 1);
		if (   script->mdkind == MDM_DIAL
			&& !NoCheckCarrier && carrier(TRUE)
			) {
BadCarrier:
			  printmsg(0, "cdial: why carrier present? Check your modem.");
			  hangup();
			  goto FailedAttempt;
		}
		ModemMode = TRUE;
		answer = sendexpect(script->mdsend, script->mdexp, wtime);
		ModemMode = FALSE;
		oldkind = script->mdkind;
	}
	if ( answer < S_OK ) {
		printmsg(0, "cdial: pass #%d/%d: no answer at %s baud, init seq. #%d",
						trycount + 1, MaxTry, rate, iii);
		Smodem(SM_NOBAUD, iii, script->mdkind, trycount + 1);
		if (dial) {
			closeline();
			if (++trycount >= MaxTry && commrate[irate + 1] == NULL) {
			/* End of speed list reached */
				irate = 0;
				trycount = 0;
				changed = TRUE;
			}
			continue;
		}
		else
			goto FailedAttempt;
	}
	if (!dial && kind != MDM_ANSWER)
		return TRUE;

	if ( script->mdsend == NULL )
		 answer = 0;
	else
	{
		int oldwait;

		if (oldkind != script->mdkind)
			iii = 0;
		printmsg(5, "cdial: %d-type initialize %d at %s baud",
					script->mdkind, iii, rate);
		maxwait = script -> mdtime;
		printmsg(8, "cdial: max wait time for modem answer is %d", maxwait);
		Smodem(SM_INIT, iii, script->mdkind, trycount + 1);
		if (   script->mdkind == MDM_DIAL
		    && !NoCheckCarrier
		    && carrier(TRUE)
		   )
			goto BadCarrier;
		ModemMode = TRUE;
		if (sendstr(script->mdsend) != S_OK)  /* Begin dialing phone num in S_sysspeed  */
			goto Failed;
#ifdef __MSDOS__
		MnpSupport();
#endif
		use_plus_minus = TRUE;
		(void) time(&startwait);
		do {
			if (dial) {
				printmsg(5, "cdial: %d: dialing %s", kind, S_sysspeed);
				Smodem(SM_DIALING, maxwait, kind, trycount + 1);
			}
			oldwait = maxwait;
			answer = response(commsg, oldwait);
		} while (answer == S_TIMEOUT && oldwait != maxwait);
		use_plus_minus = FALSE;
		ModemMode = FALSE;
		oldkind = script->mdkind;
	}

	switch(answer) {
	case S_LOST:
	case S_TIMEOUT:
	default:
	Failed:
		printmsg(0, "cdial: pass #%d/%d: no reply in %d seconds",
					trycount + 1, MaxTry, maxwait);
		Smodem(SM_NOREPLY, iii, script->mdkind, trycount + 1);
		break;

	case M_CONNECT:
	case (M_CONNECT+1):
	case (M_CONNECT+2):
	case (M_CONNECT+3):
	case (M_CONNECT+4):
	case (M_CONNECT+5):
	case (M_CONNECT+6):
	case (M_CONNECT+7):
	case (M_CONNECT+8):
	case (M_CONNECT+9):
		if ( irate != answer )
		{
			rate = commrate[answer];
			if ( set_speed ) {
				SIOSpeed(atol(rate));
				printmsg(-1, "cdial: baud rate changed to %s", rate);
			}
		}
		else {
			if ((s = strchr(respbuf, '/')) != NULL)
				*s = '\0';
			i = strcspn(respbuf, "0123456789");
			if (i < strlen(respbuf)) {
				s = rate = &respbuf[i];
				while (isdigit(*s)) s++;
				*s = '\0';
			} else if ((s = strstr(respbuf, "FAST")) != NULL) {
				rate = s;
				*(rate + 4) = '\0';
			}
		}
		printmsg(5, "cdial: %d, connected at %s", script->mdkind, rate);
		Smodem(SM_CONNECT, 0, script->mdkind, 0);
		return TRUE;

	case M_BUSY:
		printmsg(-1, "cdial: pass #%d/%d: phone is BUSY", trycount + 1, MaxTry);
		Smodem(SM_BUSY, iii, script->mdkind, trycount + 1);
		trycount = MaxTry - 1;
		break;

	case M_LOST:
		printmsg(0, "cdial: pass #%d/%d: no modem CARRIER", trycount + 1, MaxTry);
		Smodem(SM_NOCARRY, iii, script->mdkind, trycount + 1);
		break;

	case M_ERROR:
		printmsg(0, "cdial: pass #%d/%d: NO DIALTONE. Check phone or cable",
					trycount + 1, MaxTry);
		Smodem(SM_NOTONE, iii, script->mdkind, trycount + 1);
		goto FailedAttempt;
	}

	if (!dial)
		goto FailedAttempt;

	closeline();
	if (++trycount >= MaxTry) {
		trycount = 0;
		if (phonerate[frate + 1] == NULL) {
			/* End of phone list reached */
			frate = 0;
			if (phonerate[frate] == NULL) {
				printmsg(0, "cdial: bad phones list!");
				panic();
			}
			S_sysspeed = phonerate[frate];
			if (commrate[irate + 1] == NULL) {		/* One more speed ? */
				irate = 0;
				changed = TRUE;
			}
			continue;
		}
		frate++;
		printmsg(4, "cdial: choose next phone from list");
		S_sysspeed = phonerate[frate];    /* Phone loop */
	}
	goto redial;
  }

} /* cdial */

static
void
do_hangup(void)
{
	ModemMode = TRUE;
	(void)sendstr(hang_str);
	ModemMode = FALSE;
}

void modstat(void)
{
	char buf[BUFSIZ];
	char *ptr;

	if (stat_str == NULL)
		return;
	if (sendstr(stat_str) != S_OK) {
		printmsg(1, "modstat: can't request modem statistics");
		return;
	}

	ptr = buf;
	for (;;) {
		if (ptr == &buf[sizeof(buf)-1]) {
			*ptr = '\0';
			printmsg(1, "modstat: %s\\", buf);
			ptr = buf;          /* Save last character for term \0  */
		}

		if (sread(ptr, 1, 3) != 1) {
			*ptr = '\0';
			if (buf[0])
				printmsg(1, "modstat: %s", buf);
			printmsg(1, "modstat: timeout");
			return;
		}
		if (*ptr == '\n') {
			if ((ptr != buf) && (*(ptr-1) == '\r'))
				ptr--;
			*ptr = '\0';
			printmsg(1, "modstat: %s", buf);
			ptr = buf;
			if (equal(buf, "OK"))
				break;
		}
		else
			ptr++;
	}
}

static	boolean SetModem(char *nm)
{
	char s_dialer[FILENAME_MAX];
	register char *cp, **pp;
	char *cp1;
	static char _Far S_sysline[BUFSIZ];
	char *flds[128];
	int t, kind;
	int i, Kfld=0;
	FILE *dlf;
	register int i_mdi, i_mds= 0, i_baud = 0;
	boolean old_format = TRUE;

	mkfilename(s_dialer, confdir, DIALERS);
	if ( (dlf = FOPEN(s_dialer,"r",TEXT)) == nil(FILE)) {
		printerr("SetModem", s_dialer);
		printmsg(0, "SetModem: can't open %s", s_dialer);
		return FALSE;
	}
	while ( (cp = fgets(S_sysline, sizeof(S_sysline), dlf)) != (char *)0 )
	{
		i = strlen(S_sysline);
		if (i > 0 && S_sysline[i-1] == '\n')
			S_sysline[i-1] = '\0';
		if ( S_sysline[0] == '#' || S_sysline[0] == '\0' )
			continue;
		/* \\ -> \\\\ */
		for (cp = S_sysline; *cp; cp++)
			if ((strncmp(cp, "\\\\", 2) == 0) &&
			    (strlen(S_sysline) + 2 < sizeof(S_sysline))) {
				for (cp1 = cp + strlen(cp); cp1 >= cp; cp1--)
					cp1[2] = cp1[0];
				cp += 3;
			}
		/* \\\\ -> \\ */
		Kfld = getargs(S_sysline, flds, sizeof(flds)/sizeof(*flds));
		if (Kfld <= 1)
			continue;
		if ( equal(S_sysline,nm) )
			break;
	}
	fclose(dlf);

	if ( !cp ) {
		printmsg(0, "SetModem: Modem %s not found in %s", nm, DIALERS);
		return FALSE;
	}
	flds[Kfld] = NULL;
	cp = flds[1];
	if ( *cp == '!' )
	{
		set_speed = 1;
		cp++;
	}
	cp1 = cp;
	do {
		if ( *cp == ',' || !*cp )
		{
			if ( cp == cp1 ) break;
			if ( *cp == ',' ) *cp++ = '\0';
			if (commrate[i_baud] != NULL)
				free(commrate[i_baud]);
			commrate[i_baud++] = strdup(cp1);
			cp1 = cp;
			if ( !*cp ) break;
		}
		else
		{
			if ( *cp < '0' || *cp > '9' )
			{
				printmsg(0, "SetModem: Bad baud rate for modem %s: %s",nm, cp1);
				return FALSE;
			}
			cp++;
		}
	}
	while ( 1 );

	i_mdi = 0;
	/* Cleanup first entry only */
	if (commsg[0].msg_text != NULL)
		free(commsg[0].msg_text);
	commsg[0].msg_text = NULL;
	if (hang_str != NULL)
		free(hang_str);
	hang_str = NULL;
	if (stat_str != NULL)
		free(stat_str);
	stat_str = NULL;
	for (pp = flds+2;*pp && *(cp = *pp) == '-'; pp++)
	{
		switch (*++cp)
		{
		case '\0':
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			goto TSE;
		case 'h':
			if (hang_str != NULL)
				free(hang_str);
			hang_str = strdup(cp+1);
			p_hangup = do_hangup;
			break;
		case 's':
			if (stat_str != NULL)
				free(stat_str);
			stat_str = strdup(cp+1);
			break;
		case 'c':
			i = M_CONNECT;
			goto setc;
		case 'C':
			cp++;
			if ( *cp < '0' || *cp-'0' >= i_baud)
			{
				printmsg(0, "SetModem: Bad baud rate index in -C%c", pp[-1]);
				return FALSE;
			}
			i = M_CONNECT + (*cp - '0');
			goto setc;
		case 'b':
			i = M_BUSY;
			goto setc;
		case 'l':
			i = M_LOST;
			goto setc;
		case 'e':
			i = M_ERROR;
setc:
			commsg[i_mds].msg_code = i;
			if (commsg[i_mds].msg_text != NULL)
				free(commsg[i_mds].msg_text);
			commsg[i_mds].msg_text = strdup(cp+1);
			if ( ++i_mds > MAXCMSG-1 )
				i_mds--;
			/* Cleanup last entry */
			if (commsg[i_mds].msg_text != NULL)
				free(commsg[i_mds].msg_text);
			commsg[i_mds].msg_text = NULL;
			break;
		case 'S':
			if (E_inspeed != NULL)
				free(E_inspeed);
			E_inspeed = strdup(cp + 1);
			printmsg(5, "SetModem: inspeed set to %s", E_inspeed);
			break;
		case 'P':
			i = atoi(cp + 1);
			if (i >= 0) {
				chardelay = i;
				printmsg(5, "SetModem: chardelay set to %d", chardelay);
			}
			else
				printmsg(0, "SetModem: %s -- bad -P[chardelay] value", cp + 1);
			break;
		case 'M':
#ifdef __MSDOS__
			i = atoi(cp + 1);
			if (i == 0 || i == 2 || i == 4 || i == 5) {
				MnpEmulation = i;
				printmsg(5, "SetModem: MnpEmulation set to %d", MnpEmulation);
			}
			else
				printmsg(0, "SetModem: %s -- bad -M[npEmulation] value {0,2,4,5}", cp + 1);
#endif
			break;
		case 'E':
			i = atoi(cp + 1);
			if (i >= 1 || i <= 200) {
#ifdef __MSDOS__
				MnpWaitTics = i;
				printmsg(5, "SetModem: MnpWaitTics set to %d", MnpWaitTics);
#endif
			}
			else
				printmsg(0, "SetModem: %s -- bad -E[MnpWaitTics] value {1-200}", cp + 1);
			break;
		case 'N':
			NoCheckCarrier = TRUE;
			break;
		case 'A':
		case 'R':
		case 'D':
			goto init_err;
		case 'I':
			i_mdi = -1;
			old_format = FALSE;
			goto TSE;
		case 'T':
		case 'W':
			printmsg(0, "SetModem: switch -%s obsoleted and ignored, remove it from '%s'", cp, DIALERS);
			break;
		default:
			printmsg(0, "SetModem: unknown -%s in '%s', chat script aborted", cp, DIALERS);
			return FALSE;
		}
	}
TSE:
	/* Clean up first entries only */
	for (i = 0; i < MDM_NKIND; i++) {
		if (commd[i][0].mdsend != NULL) {
			free(commd[i][0].mdsend);
			commd[i][0].mdsend = NULL;
		}
		if (commd[i][0].mdexp != NULL) {
			free(commd[i][0].mdexp);
			commd[i][0].mdexp = NULL;
		}
	}
	t = DEF_TIMEOUT;
	kind = MDM_INIT;
	while ((cp = *pp++) != NULL) {
		if ( *cp == '-' ) {
			switch (cp[1]) {
			default:
				if ( cp[1] >= '0' && cp[1] <= '9' ) {
					t = atoi(cp+1);
					if ( t < 1 ) t = 15;
				}
				else if (cp[1] || !old_format) {
					printmsg(0, "SetModem: unknown switch %s in '%s'", cp, DIALERS);
					return FALSE;
				}
				break;
			case 'A':
				if (old_format || i_mdi == 0 || commd[MDM_INIT][0].mdsend == NULL) {
init_err:
					printmsg(0, "SetModem: no initialization section -I in '%s'", DIALERS);
					return FALSE;
				}
				i_mdi = 0;
				kind = MDM_ANSWER;
				break;
			case 'R':
				if (old_format || i_mdi == 0 || commd[MDM_INIT][0].mdsend == NULL)
					goto init_err;
				i_mdi = 0;
				kind = MDM_RING;
				break;
			case 'D':
				if (old_format || i_mdi == 0 || commd[MDM_INIT][0].mdsend == NULL)
					goto init_err;
				kind = MDM_DIAL;
				/* Copy initialization seq */
				for (i = 0; i < MAXCMDI && commd[MDM_INIT][i].mdsend != NULL; i++) {
					if (commd[MDM_DIAL][i].mdsend != NULL)
						free(commd[MDM_DIAL][i].mdsend);
					commd[MDM_DIAL][i].mdsend = strdup(commd[MDM_INIT][i].mdsend);
					if (commd[MDM_DIAL][i].mdexp != NULL)
						free(commd[MDM_DIAL][i].mdexp);
					commd[MDM_DIAL][i].mdexp =
						(commd[MDM_INIT][i].mdexp?strdup(commd[MDM_INIT][i].mdexp):NULL);
					commd[kind][i].mdtime = commd[MDM_INIT][i].mdtime;
					commd[kind][i].mdkind = commd[MDM_INIT][i].mdkind;
				}
				i_mdi = i;
				break;
			case 'I':
				if (old_format || i_mdi == 0)
					goto init_err;
				i_mdi = 0;
				kind = MDM_INIT;
				break;
			}
			/* Skip to next field */
			continue;
		}
		/* Get exp field */
		if ((cp1 = *pp) != NULL && *cp1 == '-') /* If switch, get it back */
			cp1 = NULL;
		if (i_mdi >= 0) {
			if ( t == DEF_TIMEOUT && !cp1 ) t = 60;
			if (commd[kind][i_mdi].mdsend != NULL)
				free(commd[kind][i_mdi].mdsend);
			commd[kind][i_mdi].mdsend = strdup(cp);
			if (commd[kind][i_mdi].mdexp != NULL)
				free(commd[kind][i_mdi].mdexp);
			commd[kind][i_mdi].mdexp  = (cp1 ? strdup(cp1) : cp1);
			commd[kind][i_mdi].mdtime = t;
			commd[kind][i_mdi].mdkind = kind;

			if ( ++i_mdi > MAXCMDI )
			{
				printmsg(0,"SetModem: too many init strings for modem %s in '%s'", nm, DIALERS);
				return FALSE;
			}
			/* Clear last entry */
			if (commd[kind][i_mdi].mdsend != NULL)
				free(commd[kind][i_mdi].mdsend);
			commd[kind][i_mdi].mdsend = NULL;
			if (commd[kind][i_mdi].mdexp != NULL)
				free(commd[kind][i_mdi].mdexp);
			commd[kind][i_mdi].mdexp = NULL;
		}
		if (*pp == NULL)
			break;
		if (cp1 != NULL)
			pp++;
	}
	if (i_mdi <= 0)
		goto init_err;
	driver = commdm;
	return TRUE;
}

/*--------------------------------------------------------------------*/
/*    s h u t d o w n                                                 */
/*                                                                    */
/*    Terminate modem processing via hangup                           */
/*--------------------------------------------------------------------*/

void Sshutdown( void )
{
   extern Sfmesg Stransstate;

   if (!port_active)
		return;
   if (Stransstate == SF_SEND)
	Sftrans(SF_SABORTED, NULL, NULL, 0);
   else if (Stransstate == SF_RECV)
	Sftrans(SF_RABORTED, NULL, NULL, 0);

   ssleep(1);	/* wait while information transfered */

   closeline();
}
