/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   dcpsys.c

   Revised edition of dcp

   Stuart Lynne May/87

   Copyright (c) Richard H. Lamb 1985, 1986, 1987
   Changes Copyright (c) Stuart Lynne 1987
   Changes Copyright (c) Andrew H. Derbyshire 1989, 1990

   Updated:

	  13May89  - Modified checkname to only examine first token of name.
				 Modified rmsg to initialize input character before use.
				 - ahd
      16May89  - Moved checkname to router.c - ahd
	  17May89  - Wrote real checktime() - ahd
	  17May89  - Changed getsystem to return 'I' instead of 'G'
	  25Jun89  - Added Reach-Out America to keyword table for checktime
	  22Sep89  - Password file support for hosts
	  25Sep89  - Change 'ExpectStr' message to debuglevel 2
      01Jan90  - Revert 'ExpectStr' message to debuglevel 1
	  28Jan90  - Alter callup() to use table driven modem driver.
				 Add direct(), qx() procedures.
      8 Jul90  - Add John DuBois's expectstr() routine to fix problems
                 with long input buffers.
	  11Nov90  - Delete QX support, add ddelay, ssleep calls
*/

/* "DCP" a uucp clone. Copyright Richard H. Lamb 1985,1986,1987 */


/* Support routines */

/*--------------------------------------------------------------------*/
/*                        system include files                        */
/*--------------------------------------------------------------------*/

#include <ctype.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "uupcmoah.h"

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include "arpadate.h"
#include "checktim.h"
#include "dcp.h"
#include "dcpgpkt.h"
#include "dcptpkt.h"
#include "dcplib.h"
#include "dcpsys.h"
#include "hostable.h"
#include "modem.h"
#include "uundir.h"
#include "ulib.h"
#include "comm.h"
#include "ssleep.h"
#include "lock.h"
#include "screen.h"
#include "swap.h"

currentfile();

#define  PROTOS_WITH_PARMS "gG"

#define MSGTIME 20            /* Timeout for many operations */

Proto Protolst[] = {
   {'t', tgetpkt, tsendpkt, topenpk, tclosepk,
		trdmsg,  twrmsg,   teofpkt, tfilepkt},
   {'g', ggetpkt, gsendpkt, gopenpk, gclosepk,
		grdmsg,  gwrmsg,   geofpkt, gfilepkt},
   {'G', ggetpkt, gsendpkt, Gopenpk, gclosepk,
		grdmsg,  gwrmsg,   geofpkt, gfilepkt},
   {'\0', }};

procref  getpkt, sendpkt, openpk, closepk, rdmsg, wrmsg, eofpkt, filepkt;

char *flds[60];
int kflds;
static char proto[5];
static char _Far S_sysline[BUFSIZ];
extern char* phonerate[32];
extern boolean reseek;
extern fail_attempts, requests, errors_per_request;
boolean remote_debug = FALSE;
boolean WorthTrying = FALSE;
boolean scan_not_needed; /* global variable */
boolean resend;
int MaxFailAttempts, Seq_Attempts, fail_attempts, Max_Idle;
static char local_proto_parms[30],remote_proto_parms[30];
extern boolean g_setup(char *, boolean);
extern  boolean NoCheckCarrier, wattcp;
extern local_pktsize, remote_pktsize;
boolean force_remote_send_gsize = FALSE;
extern void open_connection_sound(void), close_connection_sound(void),
			lost_connection_sound(void);
extern char *hstat( hostatus current_status );

static void setproto(char wanted);
static void load_call(void);

/****************************************/
/*              Sub Systems             */
/****************************************/

/*--------------------------------------------------------------------*/
/*    g e t s y s t e m                                               */
/*                                                                    */
/*    Process a systems file (L.sys) entry.                           */
/*    Null lines or lines starting with '#' are comments.             */
/*--------------------------------------------------------------------*/

CONN_STATE getsystem()
{
   int i;
   char *s, **phone;
   Proto *tproto;
   int temp_Seq_Attempts, temp_Max_Idle, temp_MaxFailAttempts;
   static boolean call_loaded = FALSE;
   boolean f_l, f_p;

Again:
   do {  /* flush to next non-comment line */
	  if (fgets(S_sysline, sizeof(S_sysline), fsys) == nil(char)) {
		 printmsg(8, "getsystem: EOF");
		 if (reseek && WorthTrying) {
			reseek = FALSE;
			if (MaxFailAttempts == 0 || fail_attempts < MaxFailAttempts) {
				rewind(fsys);
				WorthTrying = FALSE;
				goto Again;
			}
		 }
		 return CONN_EXIT;
	  }
	  i = strlen(S_sysline);
	  while (i > 0 && isspace(S_sysline[i-1]))
		  S_sysline[--i] = '\0';
   } while (i == 0 || *S_sysline == '#');

   printmsg(8, "sysline=\"%s\"", S_sysline);

   kflds = getargs(S_sysline, flds, sizeof(flds)/sizeof(*flds));
   strcpy(rmtname, flds[FLD_REMOTE]);

   if (!callnow && !checktime(flds[FLD_CCTIME], (time_t) 0)) {
	printmsg(4, "getsystem: wrong time now to call %s", rmtname);
	return CONN_INITIALIZE;
   }

   temp_Seq_Attempts = 1;
   if ((s = strrchr(rmtname, '/')) != NULL) {
	 *s++ = '\0';
	 if ((temp_Seq_Attempts = atoi(s)) <= 0) {
		printmsg(0, "getsystem: bad sequental scans field for %s", rmtname);
		return CONN_EXIT;
	 }
   }

   strcpy(proto, flds[FLD_PROTO]);
   for (s = proto; *s; s++) {
	  for (tproto = Protolst; tproto->type != '\0' ; tproto++)
		  if (*s == tproto->type)
			goto pfound;
	 printmsg(0, "getsystem: system %s, protocol '%c' not implemented", rmtname, *s);
	 return CONN_EXIT;
pfound:  ;
   }

   if ((temp_MaxFailAttempts = atoi(flds[FLD_COUNT])) < 0) {
	 printmsg(0, "getsystem: bad max failed attempts field for %s", rmtname);
	 return CONN_EXIT;
   }

   temp_Max_Idle = -1;
   if ((s = strrchr(flds[FLD_CCTIME], ';')) != NULL) {
	 *s++ = '\0';
	 if ((temp_Max_Idle = atoi(s)) < 0) {
		printmsg(0, "getsystem: bad max idle field for %s", rmtname);
		return CONN_EXIT;
	 }
   }

/*--------------------------------------------------------------------*/
/*                      Summarize the host data                       */
/*--------------------------------------------------------------------*/

   printmsg(2, "getsystem: remote=%s, call-time=%s, device=%s, phone(s)=%s, max failure count=%d, protocol=%s, pktsizes=%s",
		  rmtname, flds[FLD_CCTIME], flds[FLD_TYPE], flds[FLD_PHONE], temp_MaxFailAttempts,
		  proto, flds[FLD_PKTSIZES]);

/*--------------------------------------------------------------------*/
/*                  Determine if the remote is valid                  */
/*--------------------------------------------------------------------*/

   hostp = checkreal( rmtname );
   checkref( hostp );

   if (!call_loaded) {
	load_call();
	call_loaded = TRUE;
   }

/*--------------------------------------------------------------------*/
/*                   Display the send/expect fields                   */
/*--------------------------------------------------------------------*/

   f_l = f_p = FALSE;
   for (i = FLD_EXPECT; i < kflds; i += 2) {
	printmsg(6, "expect [%02d]: %s, send [%02d]: %s",
		     i, flds[i], i + 1, flds[i + 1]);
	if (strstr(flds[i], "\\L") != NULL || strstr(flds[i + 1], "\\L") != NULL)
		f_l = TRUE;
	if (strstr(flds[i], "\\P") != NULL || strstr(flds[i + 1], "\\P") != NULL)
		f_p = TRUE;
   }

   if (f_l && hostp->login == NULL) {
	printmsg(0, "getsystem: no login (\\L) for %s defined, check CALL file", rmtname);
	return CONN_EXIT;
   }

   if (f_p && hostp->password == NULL) {
	printmsg(0, "getsystem: no password (\\P) for %s defined, check CALL file", rmtname);
	return CONN_EXIT;
   }

/* Extract phone/speed numbers to 'phonerate' */

	phone = phonerate;
	s = flds[FLD_PHONE];
	do {
		if (!*s)
			break;
		if (*s == ':') {
			*s = '\0';
			break;
		}
		*phone++ = s;
		if ((s = strchr(s, ':')) != NULL)
			*s++ = '\0';
	}
	while (s != NULL);
	*phone = NULL;
	if (phonerate[0] == NULL) {
		printmsg(0, "getsystem: empty speed/phone list field for %s", rmtname);
		return CONN_EXIT;
	}

/* Extract packets sizes */

	s = strupr(flds[FLD_TAYLOR]);
	if (!equal(s,"Y") && !equal(s,"N")) {
		printmsg(0, "getsystem: wrong taylor field for %s", rmtname);
		return CONN_EXIT;
	}
	force_remote_send_gsize = equal(s, "Y");

	local_pktsize = remote_pktsize = -1;

	if ((s = strpbrk(proto, PROTOS_WITH_PARMS)) != NULL) {
		if (   sscanf(flds[FLD_PKTSIZES], "%[^ /\n\t]/%s",
				local_proto_parms,
				remote_proto_parms) != 2
			|| !*local_proto_parms || !*remote_proto_parms
		   ) {
			printmsg(0, "getsystem: bad '%c'-parameters syntax for %s, \"local/remote\" must present",
					s, rmtname);
			return CONN_EXIT;
		}
		if (strpbrk(s, "gG") != NULL) {
			if (   !g_setup(remote_proto_parms, FALSE)
			    || !g_setup(local_proto_parms, TRUE)
			   )
				return CONN_EXIT;
		}
	}
	else
		*remote_proto_parms = '\0';

/*--------------------------------------------------------------------*/
/*               Determine if we want to call this host               */
/*                                                                    */
/*    The following if statement breaks down to:                      */
/*                                                                    */
/*       if host not successfully called this run and                 */
/*             (  we are calling all hosts or                         */
/*                we are calling this host or                         */
/*                we are hosts with work and this host has work )     */
/*       then call this host                                          */
/*--------------------------------------------------------------------*/

   printmsg(10, "getsystem: remote: %s host: %s status: %s", rmtname,
				hostp->hostname,
				hstat( hostp->status.hstatus) );

   fwork = nil(FILE);
   if (
		 hostp->status.hstatus != called &&
		 (
			equal(Rmtname, "all") ||
			equal(Rmtname, rmtname) ||
			(
			   equal(Rmtname, "any") && (scandir(rmtname) == XFER_REQUEST)
			)
		 )
	   )
	{

	  if (fwork != nil(FILE)) /* in case matched with scandir     */
		 fclose(fwork);
	  scandir( NULL );        /* Reset directory search as well   */

	  memset( &remote_stats, 0, sizeof remote_stats);
	  fail_attempts = hostp->hattempts;
	  requests = 0;
	  Seq_Attempts = temp_Seq_Attempts;
	  MaxFailAttempts = temp_MaxFailAttempts;
	  Max_Idle = temp_Max_Idle;
	  return CONN_CALLUP1;    /* startup this system */

   } else
	  return CONN_INITIALIZE;    /* Look for next system to process   */

} /*getsystem*/

/*--------------------------------------------------------------------*/
/*    s y s e n d                                                     */
/*                                                                    */
/*    End UUCP session negotiation                                    */
/*--------------------------------------------------------------------*/

CONN_STATE sysend(int master)
{
   char msg[80];
   static char *O[2] = {"OOOOOOO", "OOOOOO"};
   boolean none;

   if (hostp->status.hstatus == inprogress)
	  hostp->status.hstatus = call_failed;

   if (master) {
	   if (wmsg(O[master], TRUE) == S_LOST)
			return CONN_DROPLINE;
	   if (!NoCheckCarrier && carrier(TRUE))
			(void) w_flush();    /* Wait for it to be transmitted       */
	   close_connection_sound();
   }
   msg[1] = '\0';
   none = (rmsg(msg, TRUE, 5) < S_OK || strstr(msg, O[!master]) == NULL);
   if (!master || none) {
	   if (!master)
		   close_connection_sound();
	   if (wmsg(O[master], TRUE) == S_LOST)
			return CONN_DROPLINE;
	   if (!NoCheckCarrier && carrier(TRUE))
			(void) w_flush();    /* Wait for it to be transmitted       */
	   ssleep(2);
   }

   return CONN_DROPLINE;
} /*sysend*/


/*
   w m s g

   write a ^P type msg to the remote uucp
*/

int wmsg(char *msg, const boolean synch)
{

   if (synch)
	  if (swrite("\020", 1) < S_OK)
		goto lost;

   if (swrite(msg, strlen(msg)) < S_OK)
		goto lost;

   if (synch)
	  if (swrite("\0", 1) < S_OK)
		goto lost;

   return 0;

lost:
	if (hostp != BADHOST)
		hostp->status.hstatus = call_failed;
	printmsg(0, "wmsg: LOST %s aborted",
			wattcp ? "TCP CONNECTION," : "CARRIER, connection");
	lost_connection_sound();
	return S_LOST;
} /*wmsg*/


/*
   r m s g

   read a ^P msg from UUCP
*/

int rmsg(char *msg, const boolean synch, unsigned int msgtime)
{
   int i;
   static int max_len = 60;
   char ch = '?';       /* Initialize to non-zero value  */    /* ahd   */

/*--------------------------------------------------------------------*/
/*                        flush until next ^P                         */
/*--------------------------------------------------------------------*/

   if (synch == 1)
   {
	  do {
		 switch (sread(&ch, 1, msgtime)) {
		 case S_TIMEOUT:
			if (hostp != BADHOST) /* login() */
				hostp->status.hstatus = call_failed;
			return TIMEOUT;
		 case S_LOST:
			goto lost;
		 }

	  } while ((ch & 0x7f) != '\020');
   }

/*--------------------------------------------------------------------*/
/*   Read until timeout, next newline, or we fill the input buffer    */
/*--------------------------------------------------------------------*/

   for (i = 0; (i < max_len) && (ch != '\0'); ) {
	  switch (sread(&ch, 1, msgtime)) {
	  case S_TIMEOUT:
		 if (hostp != BADHOST)
			 hostp->status.hstatus = call_failed;
		 return TIMEOUT;
	  case S_LOST:
		 goto lost;
	  }
	  if ( synch == 2 )
		 if (swrite( &ch, 1) < S_OK)
			goto lost;
	  ch &= 0x7f;
	  if (ch == '\r' || ch == '\n')
		 ch = '\0';
	  msg[i++] = ch;
   }
   msg[i] = '\0';

   return i;

lost:
   if (hostp != BADHOST)
	hostp->status.hstatus = call_failed;
   printmsg(0, "rmsg: LOST CARRIER, connection aborted");
   lost_connection_sound();
   return S_LOST;
} /*rmsg*/


/*--------------------------------------------------------------------*/
/*    s t a r t u p _ s e r v e r                                     */
/*                                                                    */
/*    Exchange host and protocol information for a system calling us  */
/*--------------------------------------------------------------------*/

CONN_STATE startup_server()
{
   char msg[80], parm[30];
   char *s;
   int r;

/*--------------------------------------------------------------------*/
/*                      Begin normal processing                       */
/*--------------------------------------------------------------------*/

   hostp->status.hstatus = startup_failed;
   hostp->via     = hostp->hostname;   /* Save true hostname           */

   if ((r = rmsg(msg, TRUE, PROTOCOL_TIME)) < 0)
   {
	  if (r == TIMEOUT) {
		  printmsg(1,"startup_server: timeout for first message");
		  printmsg(0,"startup_server: can't login, check your login name and password");
	  }
	  return CONN_DROPLINE;
   }
   printmsg(2, "1st msg = %s", msg);

/*--------------------------------------------------------------------*/
/*              The first message must begin with Shere               */
/*--------------------------------------------------------------------*/

   if (!equaln(msg,"Shere",5))
   {
	  printmsg(0,"startup_server: first message not 'Shere'");
	  return CONN_DROPLINE;
   }

/*--------------------------------------------------------------------*/
/*    The host can send either a simple Shere, or Shere=hostname;     */
/*    we allow either.                                                */
/*--------------------------------------------------------------------*/

   if ((msg[5] == '=') && !equaln(&msg[6], rmtname, HOSTLEN))
   {
	  printmsg(0,"startup_server: wrong host %s, expected %s",
			   &msg[6], rmtname);
	  hostp->status.hstatus = wrong_host;
	  return CONN_DROPLINE; /* wrong host */              /* ahd */
   }

   if (*remote_proto_parms)
	   sprintf(parm, " -P%s", remote_proto_parms);
   if (remote_debug)	  /* -Q0 -x16 remote debuglevel set */
	 (void) sprintf(msg, "S%s -Q0 -x%d -R%s", nodename, debuglevel,
		(!*remote_proto_parms || force_remote_send_gsize) ? "" : parm);
   else
	 (void) sprintf(msg, "S%s -Q0 -R%s", nodename,
		(!*remote_proto_parms || force_remote_send_gsize) ? "" : parm);
   printmsg(4, "startup_server: parameters '%s'", msg+1);
   if (wmsg(msg, TRUE) == S_LOST)
		return CONN_DROPLINE;

   if ((r = rmsg(msg, TRUE, PROTOCOL_TIME)) < 0)
   {
	  if (r == TIMEOUT)
		  printmsg(0,"startup_server: timeout for second message");
	  return CONN_DROPLINE;
   }

/*--------------------------------------------------------------------*/
/*                Second message is protocol exchange                 */
/*--------------------------------------------------------------------*/

	printmsg(2, "2nd msg = %s", msg);

	if (!equaln(msg, "ROK", 3)) {
		if (equal(msg, "RLCK"))
			printmsg(0, "startup_server: %s thinks it's already talking to %s (locked)",
				rmtname, nodename);
		else if (equal(msg, "RCB"))
			printmsg(0, "startup_server: %s wants to call %s back",
				rmtname, nodename);
		else if (equal(msg, "RBADSEQ"))
			printmsg(0, "startup_server: call sequence number is wrong for %s",
				rmtname);
		else if (equal(msg, "RLOGIN"))
			printmsg(0, "startup_server: %s login name isn't known to %s uucp",
				nodename, rmtname);
		else if (equaln(msg, "RYou", 4) || equaln(msg, "RUnknown", 8))
			printmsg(0, "startup_server: %s", &msg[1]);
		else
			printmsg(0,"startup_server: unexpected second message: %s", msg);
		return CONN_DROPLINE;
	}

	if( strstr( msg, "-R" ) != NULL ) {
		resend = TRUE;
		printmsg(4, "startup_server: remote can restart failed file transmissions");
	}
	else {
		resend = FALSE;
		printmsg(4, "startup_server: remote CAN'T restart failed file transmissions");
	}

	if ((r = rmsg(msg, TRUE, PROTOCOL_TIME)) < 0) {
		if (r == TIMEOUT)
			printmsg(0, "startup_server: timeout for third message");
		 return CONN_DROPLINE;
	}
	printmsg(2, "3rd msg = %s", msg);

	if (*msg != 'P') {
		printmsg(0,"startup_server: unexpected third message: %s",&msg[1]);
		return CONN_DROPLINE;
	}

/*--------------------------------------------------------------------*/
/*                      Locate a common procotol                      */
/*--------------------------------------------------------------------*/

   s = strpbrk(proto, &msg[1]);
   if ( s == NULL )
   {
	  printmsg(0,"startup_server: no common protocol (%s)", &msg[1]);
	  (void) wmsg("UN", TRUE);
	  return CONN_DROPLINE; /* no common protocol */
   }
   else {
	  sprintf(msg, "U%c", *s);
	  if (wmsg(msg, TRUE) == S_LOST)
		return CONN_DROPLINE;
   }

   setproto(*s);

   printmsg(1,"startup_server: %s connected to host %s using %c protocol",
		 nodename, rmtname, *s);

   hostp->status.hstatus = inprogress;

   Slink(SL_CONNECTED, rmtname);	/* Connected */
   open_connection_sound();

   return CONN_SERVER;
} /*startup_server*/

/*--------------------------------------------------------------------*/
/*    s t a r t u p _ c l i e n t                                     */
/*                                                                    */
/*    Setup a host connection with a system which has called us       */
/*--------------------------------------------------------------------*/

CONN_STATE startup_client()
{
   char plist[20];
   char msg[80];
   int xdebug = debuglevel;
   char *sysname = rmtname;
   Proto *tproto;
   char *s;
   int  kflds,i;
   char grade;

   local_pktsize = remote_pktsize = -1;
   *local_proto_parms = '\0';
   force_remote_send_gsize = FALSE;
/*--------------------------------------------------------------------*/
/*    Challange the host calling in with the name defined for this    */
/*    login (if available) otherwise our regular node name.  (It's    */
/*    a valid session if the securep pointer is NULL, but this is     */
/*    trapped below in the call to ValidateHost()                     */
/*--------------------------------------------------------------------*/

   sprintf(msg, "Shere=%s", nodename);
   if (wmsg(msg, TRUE) == S_LOST)
	  return CONN_DROPLINE;

   if (rmsg(msg, TRUE, PROTOCOL_TIME) < 0)
	  return CONN_DROPLINE;

   printmsg(2, "1st msg from remote = %s", msg);

/*--------------------------------------------------------------------*/
/*             Parse additional flags from remote system              */
/*--------------------------------------------------------------------*/

   kflds = getargs(msg,flds,sizeof(flds)/sizeof(*flds));
   strcpy(sysname,&flds[0][1]);

   resend = FALSE;
   for (i=1; i < kflds; i++)
   {
      if (flds[i][0] != '-')
	 printmsg(0,"startup_client: invalid argument \"%s\" from system %s, ignored",
		    flds[i], sysname);
      else
	 switch(flds[i][1])
	 {
	    case 'Q' :             /* Ignore the remote sequence number   */
	       break;

	    case 'R' :
	       resend = TRUE;
	       break;

	    case 'x' :
	       sscanf(flds[i], "-x%d", &xdebug);
	       break;

	    case 'p' :
	       sscanf(flds[i], "-p%c", &grade);
	       break;

	    case 'P' :
	       sscanf(flds[i], "-P%s", local_proto_parms);
	       break;

	    case 'v' :
	       sscanf(flds[i], "-vgrade=%c", &grade);
	       break;

	    default  :
	       printmsg(0,"startup_client: invalid argument \"%s\" from system %s, ignored",
			  flds[i], sysname);
	       break;
	 } /* switch */
   } /* for */

/*--------------------------------------------------------------------*/
/*                Verify the remote host name is good                 */
/*--------------------------------------------------------------------*/

   hostp = checkreal( sysname );
   if ( hostp == BADHOST )
   {
	 (void)wmsg("RYou are unknown to me",TRUE);
	 printmsg(0,"startup_client: Unknown host \"%s\"", sysname);
	 return CONN_DROPLINE;
   } /* if ( hostp == BADHOST ) */
   else if ( LockSystem( hostp->hostname , B_UUCICO ))
      hostp->via = hostp->hostname;
   else {
      wmsg("RLCK",TRUE);
      return CONN_DROPLINE;
   } /* else */

   strcpy(rmtname,hostp->hostname);       /* Make sure we use the
											 full host name       */

/*--------------------------------------------------------------------*/
/*                     Set the local debug level                      */
/*--------------------------------------------------------------------*/

   if ( xdebug > debuglevel )
   {
	  debuglevel = xdebug;
	  printmsg(1, "startup_client: Debuglevel set to %d by remote", debuglevel);
   }

/*--------------------------------------------------------------------*/
/*                     Build local protocol list                      */
/*--------------------------------------------------------------------*/

   s = plist;
   for (tproto = Protolst; tproto->type != '\0' ; tproto++)
	  *s++ = tproto->type;

   *s = '\0';                 /* Terminate our string                */

/*--------------------------------------------------------------------*/
/*              The host name is good; get the protocol               */
/*--------------------------------------------------------------------*/

   if (wmsg(resend ? "ROK -R" : "ROK", TRUE) == S_LOST)
	  return CONN_DROPLINE;

   sprintf(msg, "P%s", plist);
   if (wmsg(msg, TRUE) == S_LOST)
	  return CONN_DROPLINE;

   if (rmsg(msg, TRUE, PROTOCOL_TIME) < 0)
	  return CONN_DROPLINE;

   if (msg[0] != 'U')
   {
	  printmsg(0,"startup_client: Unexpected second message: %s", msg);
	  return CONN_DROPLINE;
   }

   if (strchr(plist, msg[1]) == nil(char))
   {
	  printmsg(0,"startup_client: Host does not support our protocols");
	  return CONN_DROPLINE;
   }

   setproto(msg[1]);

   if ((msg[1] == 'g' || msg[1] == 'G') && *local_proto_parms) {
	if (!g_setup(local_proto_parms, TRUE))
		return CONN_DROPLINE;
   }

/*--------------------------------------------------------------------*/
/*            Report that we connected to the remote host             */
/*--------------------------------------------------------------------*/

   printmsg(1,"startup_client: %s called by %s using %c protocol",
		 nodename,
		 hostp->via,
		 msg[1]);

   if ( hostp == BADHOST )
	  panic();

   hostp->status.hstatus = inprogress;
   time( &remote_stats.lconnect );

   Slink(SL_CALLEDBY, hostp->hostname); /* Called by */
   open_connection_sound();

   return CONN_CLIENT;

} /*startup_client*/

/*--------------------------------------------------------------------*/
/*    s e t p r o t o                                                 */
/*                                                                    */
/*    set the protocol to be used                                     */
/*--------------------------------------------------------------------*/

static void setproto(char wanted)
{
   Proto *tproto;

   for (tproto = Protolst;
	  tproto->type != '\0' && tproto->type != wanted;
	  tproto++) {
	  printmsg(3, "setproto: wanted '%c', have '%c'", wanted, tproto->type);
   }
   if (tproto->type == '\0') {
	  printmsg(0, "setproto: you said I have protocol '%c' but I cant find it!",
			wanted);
	  panic();
   }

   getpkt  = tproto->getpkt;
   sendpkt = tproto->sendpkt;
   openpk  = tproto->openpk;
   closepk = tproto->closepk;
   rdmsg   = tproto->rdmsg;
   wrmsg   = tproto->wrmsg;
   eofpkt  = tproto->eofpkt;
   filepkt = tproto->filepkt;
} /*setproto*/

/*
	  s c a n d i r

	  Scan spooling directory for C.* files for the remote host
	  (rmtname)

	  Assumes the parameter remote is from static storage!
*/

XFER_STATE scandir(char *remote)
{
   static DIR *dirp;
   static char *saveremote = NULL;
   static char remotedir[FILENAME_MAX];
   static boolean was_flush = FALSE;
   char buf[20];
   struct dirent *dp;
   extern char *batchflush;

/*--------------------------------------------------------------------*/
/*          Determine if we must restart the directory scan           */
/*--------------------------------------------------------------------*/

   if (fwork != NULL )
   {
	  fclose( fwork );
	  fwork = NULL;
   }

   if ( (remote == NULL) || (saveremote == NULL ) ||
		!equal(remote, saveremote) )
   {
	  scan_not_needed = TRUE;

	  if ( saveremote != NULL )  /* Clean up old directory? */
	  {                          /* Yes --> Do so           */
		 closedir(dirp);
		 saveremote = NULL;
	  } /* if */

	  if ( remote == NULL )      /* Clean up only, no new search? */
		 return XFER_NOLOCAL;    /* Yes --> Return to caller      */

	  sprintf(buf,"%.7s/C",remote);
	  mkfilename(remotedir, spooldir, buf);
	  if ((dirp = opendir(remotedir)) == nil(DIR))
	  {
		 printmsg(2, "scandir: couldn't opendir() %s", remotedir);
		 goto go_flush;
	  } /* if */

	  saveremote = (char *) remote;
							  /* Flag we have an active search    */
   } /* if */

/*--------------------------------------------------------------------*/
/*              Look for the next file in the directory               */
/*--------------------------------------------------------------------*/

SubdirFound:
   if ((dp = readdir(dirp)) != nil(struct dirent))
   {	  struct stat statbuf;

	  mkfilename(workfile, remotedir, dp->d_name);
	  stat(workfile, &statbuf);
	  if (statbuf.st_mode & S_IFDIR)
		goto SubdirFound;
	  printmsg(5, "scandir: matched \"%s\"",workfile);
	  if ((fwork = FOPEN(workfile, "r", TEXT)) == nil(FILE))
	  {
		 printerr("scandir", workfile);
		 printmsg(0, "scandir: can't open %s", workfile);
		 saveremote = NULL;
		 scan_not_needed = TRUE;
		 return XFER_ABORT;   /* Very bad, since we just read its
					 directory entry!                 */
	  }

	  errors_per_request = 0;
	  return XFER_REQUEST; /* Return success                   */
   }

/*--------------------------------------------------------------------*/
/*     No hit; clean up after ourselves and return to the caller      */
/*--------------------------------------------------------------------*/

   printmsg(5, "scandir: \"%s\" not matched", remotedir);
   closedir(dirp);
   saveremote = NULL;

go_flush:
   if (scan_not_needed)
	if (was_flush || (batchflush == NULL) || (*batchflush == '\0')) {
		was_flush = FALSE;
		return XFER_NOLOCAL;
	} else {
		int i;

		sprintf(remotedir, "%s %s > nul >& nul", batchflush, remote);
		printmsg(4, "scandir: executing '%s'", remotedir);
		i = swap_system(remotedir);
		printmsg((i == 0) ? 7 : 4, "scandir: execution return code is %d", i);
		was_flush = TRUE;
		return scandir(remote);
	}
   else
	return scandir(remote);
} /*scandir*/

static void load_call(void)
{
   char buf[128], buf1[256], sys[20], login[80], pass[80];
   char *s, *p;
   FILE *f;
   int i, l;
   struct HostTable *host;
   boolean found = FALSE;

   mkfilename(buf, confdir, "call");
   printmsg(5, "Read call file '%s'", buf);
   if ((f = FOPEN(buf, "r", TEXT)) == nil(FILE)) {
	 printerr("load_call", buf);
	 printmsg(0, "load_call: can't open %s", buf);
	 return;
   }
   l = 0;
   while (fgets(buf1, sizeof(buf1), f) != NULL) {
	  l++;
	  i = strlen(buf1);
	  while (i > 0 && isspace(buf1[i-1]))
		  buf1[--i] = '\0';
	  if (i == 0 || *buf1 == '#')
		continue;
	  for (s = buf1; *s && !isspace(*s); s++)
		;
	  if (!*s) {
	  wrong:
		printmsg(0, "load_call: wrong format of CALL file, line %d, needs: <node> <login> <password>", l);
		fclose(f);
		return;
	  }
	  *s++ = '\0';
	  strcpy(sys, buf1);
	  while(*s && isspace(*s))
		s++;
	  if (!*s)
		goto wrong;
	  p = s;
	  while (*s && !isspace(*s))
		s++;
	  if (!*s)
		goto wrong;
	  *s++ = '\0';
	  strcpy(login, p);
	  while(*s && isspace(*s))
		s++;
	  if (!*s)
		goto wrong;
	  strcpy(pass, s);
	  host = checkreal(sys);
	  if (host == BADHOST) {
		printmsg(0, "load_call: unknown node name '%s' from CALL file, line %d", sys, l);
		continue;
	  }
	  if (host->login != NULL || host->password != NULL) {
		printmsg(0, "load_call: duplicate login/password for node '%s' in CALL file, line %d", sys, l);
		fclose(f);
		return;
	  }
	  host->login = strdup(login);
	  checkref(host->login);
	  host->password = strdup(pass);
	  checkref(host->password);
	  found = TRUE;
   }
   fclose(f);
   if (!found)
	goto wrong;
}
