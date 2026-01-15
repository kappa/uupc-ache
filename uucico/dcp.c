/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   dcp.c

   Revised edition of dcp

   Stuart Lynne May/87

   Copyright (c) Richard H. Lamb 1985, 1986, 1987
   Changes Copyright (c) Stuart Lynne 1987
   Changes Copyright (c) Andrew H. (Drew) Derbyshire 1989, 1990
   Changes Copyright (C) by Andrey A. Chernov, Moscow, Russia.

   Maintenance Notes:

   25Aug87 - Added a version number - Jal
   25Aug87 - Return 0 if contact made with host, or 5 otherwise.
   04Sep87 - Bug causing premature sysend() fixed. - Randall Jessup.
   13May89 - Add date to version message  - Drew Derbyshire
   17May89 - Add '-u' (until) option for login processing
   01 Oct 89      Add missing function prototypes                    ahd
   28 Nov 89      Add parse of incoming user id for From record      ahd
   18 Mar 90      Change checktime() calls to Microsoft C 5.1        ahd
*/

/* "DCP" a uucp clone. Copyright Richard H. Lamb 1985,1986,1987 */

/*
   This program implements a uucico type file transfer and remote
   execution protocol.

   Usage:   uuio [-s sys] [-r 0|1] [-x debug]

   e.g.

   uuio [-x n] -r 0 [-u time]    client mode, wait for an incoming call
			 until 'time'.
   uuio [-x n] -s HOST     call the host "HOST".
   uuio [-x n] -s all      call all known hosts in the systems file.
   uuio [-x n] -s any      call any host we have work queued for.
   uuio [-x n]          same as the above.
*/

#include <assert.h>                                               /* ahd   */
#include <stdio.h>                                                /* ahd   */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <io.h>
#include <time.h>

/*--------------------------------------------------------------------*/
/*                      UUPC/extended prototypes                      */
/*--------------------------------------------------------------------*/

#include "lib.h"
#include "arpadate.h"
#include "checktim.h"
#include "dcp.h"
#include "dcplib.h"
#include "dcpstats.h"
#include "dcpsys.h"
#include "dcpxfer.h"
#include "hlib.h"
#include "hostable.h"
#include "hostatus.h"
#include "modem.h"
#include "ssleep.h"
#include "screen.h"
#include "ulib.h"
#include "lock.h"

/*--------------------------------------------------------------------*/
/*    Define passive and active polling modes; passive is             */
/*    sometimes refered to as "slave", "active" as master.  Since     */
/*    the roles can actually switch during processing, we avoid       */
/*    the terms here                                                  */
/*--------------------------------------------------------------------*/

typedef enum {
	  POLL_PASSIVE = 0,       /* We answer the telephone          */
	  POLL_ACTIVE  = 1        /* We call out to another host      */
	  } POLL_MODE ;

/*--------------------------------------------------------------------*/
/*                          Global variables                          */
/*--------------------------------------------------------------------*/

size_t pktsize;         /* packet size for this protocol*/
FILE *xfer_stream = NULL;        /* stream for file being handled    */
boolean callnow = FALSE;           /* TRUE = ignore time in L.SYS        */
FILE *fwork = NULL, *fsys= NULL ;
char workfile[FILENAME_MAX];  /* name of current workfile         */
char *Rmtname = nil(char);    /* system we want to call           */
char rmtname[20];             /* system we end up talking to      */
char s_systems[FILENAME_MAX]; /* full-name of systems file        */
struct HostTable *hostp;
int poll_mode = POLL_ACTIVE;   /* Default = dial out to system     */
extern char *logintime;
boolean reseek = FALSE;
static boolean dialed = FALSE;/* True = We attempted a phone call */
extern MaxFailAttempts, fail_attempts;
int requests = 0, totreqs = 0;
static boolean Contacted = FALSE;
static boolean AnyContacted = FALSE;
static boolean Aborted = FALSE;
extern void close_log_stream(void);
extern boolean PressBreak;
extern boolean Front;

currentfile();

/*--------------------------------------------------------------------*/
/*                     Local function prototypes                      */
/*--------------------------------------------------------------------*/

static CONN_STATE process( const POLL_MODE poll_mode );
static char *conn_state(CONN_STATE c);
static char *xfer_state(XFER_STATE c);
char *hstat( hostatus current_status );

static void cant(char *file);

static void closelog(void)
{
   printmsg(18, "closelog");
   fclose(logfile);
   logfile = stdout;
}

int report(void)
{
/*--------------------------------------------------------------------*/
/*                         Report our results                         */
/*--------------------------------------------------------------------*/

   if (!PressBreak && !Aborted) {
	   if (!AnyContacted) {
		  if (dialed)
			 printmsg(-1, "report: could not connect to remote system(s) or connection aborted");
		  else
			 printmsg(-1, "report: all attempts failed or no work for any system(s) or wrong time to call");
	   }
	   else {
		  if (totreqs == 0)
			  printmsg(-1, "report: all done, no messages sent");
		  else
			  printmsg(-1, "report: all done, total %d message(s) sent", totreqs);
	   }
	}

   dcupdate();

   return Aborted ? 13 : AnyContacted ? 0 : 5;
}

/*--------------------------------------------------------------------*/
/*    d c p m a i n                                                   */
/*                                                                    */
/*    main program for DCP, called by uuhost                          */
/*--------------------------------------------------------------------*/

int dcpmain(void)
{

   fwork = nil(FILE);

   if (Rmtname == nil(char))
	  Rmtname = "any";

/*--------------------------------------------------------------------*/
/*        Initialize logging and the name of the systems file         */
/*--------------------------------------------------------------------*/

	if (debuglevel > 0 && logecho &&
		(logfile = FOPEN(LOGFILE, "a", TEXT)) == nil(FILE)) {
		logfile = stdout;
		cant(LOGFILE);
	}
	if (logfile != stdout)
		atexit(closelog);
	if (logfile != stdout)
		fprintf(logfile, "\n>>>> Log level %d started %s\n", debuglevel, arpadate());
	atexit(close_log_stream);

/*--------------------------------------------------------------------*/
/* logecho = ((poll_mode == POLL_ACTIVE) ? TRUE : FALSE);             */
/*--------------------------------------------------------------------*/

   /*logecho = FALSE;            /* ahd - One too many missed messages  */


   mkfilename(s_systems, confdir, SYSTEMS);
   printmsg(5, "Using system file '%s'",s_systems);
   if (P_Flag)
	if (ipport(P_Flag)) {
		free(E_inmodem);
		E_inmodem=P_Flag;
		P_Flag=strdup("TCP");
		checkref(P_Flag);
	}

/*--------------------------------------------------------------------*/
/*              Load connection stats for previous runs               */
/*--------------------------------------------------------------------*/

   HostStatus();

   atexit( closeline );        /* Insure port is closed by panic()    */
   remote_stats.hstatus = nocall;
			      /* Known state for automatic status
				 update                              */
/*--------------------------------------------------------------------*/
/*                     Begin main processing loop                     */
/*--------------------------------------------------------------------*/

   if (poll_mode == POLL_ACTIVE) {

	  CONN_STATE m_state = CONN_INITIALIZE;

	  printmsg(2, "calling \"%s\", debug=%d", Rmtname, debuglevel);

	  if ((fsys = FOPEN(s_systems, "r", TEXT)) == nil(FILE)) {
		 printerr("dcpmain", s_systems);
		 printmsg(0, "dcpmain: can't open %s", s_systems);
		 panic();
	  }

	  reseek = FALSE;
	  while (m_state != CONN_EXIT )
	  {
		 printmsg(4, "Master state = %s", conn_state(m_state));

		 if ((hostp != NULL ) &&
		     (remote_stats.hstatus != hostp->status.hstatus ))
		 {
		    printmsg(4, "Updating status for host %s, status %s",
				hostp->hostname ,
				hstat( hostp->status.hstatus) );
		    dcupdate();
		    remote_stats.hstatus = hostp->status.hstatus;
		 }

		 switch (m_state)
		 {
			case CONN_INITIALIZE:
			   hostp = NULL;
			   Contacted = FALSE;

			   if ( locked )
				UnlockSystem();

			   if ((m_state = getsystem()) == CONN_CALLUP1)
				cleantmp();
			   if ( hostp != NULL )
				remote_stats.hstatus = hostp->status.hstatus;
			   break;

			case CONN_CALLUP1:
			   if ( LockSystem( hostp->hostname , B_UUCICO))
			   {
				dialed = TRUE;
				time(&hostp->status.ltime);
				/* Save time of last attempt to call   */
				hostp->status.hstatus = autodial;
				m_state = CONN_CALLUP2;
			   }
			   else
				m_state = CONN_INITIALIZE;
			   break;

			case CONN_CALLUP2:
			   Sundo();
			   m_state = callup();
			   break;

			case CONN_PROTOCOL:
			   dialed = TRUE;
			   m_state = startup_server();
			   break;

			case CONN_SERVER:
			   m_state = process( poll_mode );
			   break;

			case CONN_TERMINATE:
			   m_state = sysend(1);
			   break;

			case CONN_DROPLINE:
			   Sshutdown();
			   hostp->hattempts = fail_attempts;
			   if ( locked )
				UnlockSystem();
			   if (!AnyContacted)
				reseek = TRUE;
			   if (   hostp->status.hstatus == call_failed
			       || hostp->status.hstatus == dial_failed
			       || hostp->status.hstatus == dial_script_failed
			       || hostp->status.hstatus == startup_failed
			       || hostp->status.hstatus == max_retry
			       || hostp->status.hstatus == script_failed
			      )
				Slink(SL_FAILED, rmtname);
			   dcstats();
			   m_state = (Front || Aborted) ? CONN_EXIT : CONN_INITIALIZE;
			   break;

			case CONN_EXIT:
			   break;

			default:
			   printmsg(0,"dcpmain: Unknown master state = %c",m_state );
			   panic();
			   break;
		 } /* switch */
	  } /* while */
	  fclose(fsys);

   }
   else { /* client mode */

	  CONN_STATE s_state = CONN_INITIALIZE;

	  if (logintime != NULL)
	  {
		 if (!checktime(logintime,(time_t) 0))
			printmsg(-1,"dcpmain: awaiting login window %s",logintime);

		 while(!checktime(logintime,(time_t) 0) )  /* Wait for window   */
				 ssleep(60);                   /* Checking one per minute    */

		 printmsg(-1,"dcpmain: enabling %s for remote login until '%s'",
					E_inmodem, logintime);
	  }

	if (  (hostp != NULL ) &&
	      (remote_stats.hstatus != hostp->status.hstatus ))
	{
	    printmsg(4, "Updating status for host %s, status %s",
			hostp->hostname ,
			hstat( hostp->status.hstatus) );
	    dcupdate();
	    remote_stats.hstatus = hostp->status.hstatus;
	}

	  while (s_state != CONN_EXIT )
	  {
		 printmsg(4, "Slave state = %s", conn_state(s_state));
		 switch (s_state) {
			case CONN_INITIALIZE:
			   Contacted = FALSE;
			   if ( locked )
			   	UnlockSystem();
			   if (MaxFailAttempts > 0 && fail_attempts >= MaxFailAttempts)
				   s_state = CONN_EXIT;
			   else {
				   cleantmp();
				   s_state = CONN_ANSWER;
			   }
			   hostp = NULL;
			   break;

			case CONN_ANSWER:
			   Sundo();
			   s_state = callin( logintime  );
			   break;

			case CONN_LOGIN:
			   if (SkipLogin)
				s_state = CONN_PROTOCOL;
			   else
				s_state = login();
			   break;

			case CONN_PROTOCOL:
			   s_state = startup_client();
			   break;

			case CONN_CLIENT:
			   s_state = process( poll_mode );
			   break;

			case CONN_TERMINATE:
			   s_state = sysend(0);
			   break;

			case CONN_DROPLINE:
			   Sshutdown();
			   if ( locked )
				UnlockSystem();
			   if (hostp != BADHOST) {
				if (   hostp->status.hstatus == call_failed
				    || hostp->status.hstatus == dial_failed
				    || hostp->status.hstatus == dial_script_failed
				    || hostp->status.hstatus == script_failed
				   )
					Slink(SL_FAILED, rmtname);
				dcstats();
			   }
			   s_state = (Front || AnyContacted || Aborted) ? CONN_EXIT : CONN_INITIALIZE;
			   break;

			case CONN_EXIT:
			   break;

			default:
			   printmsg(0,"dcpmain: Unknown slave state = %d",s_state );
			   panic();
			   break;
		 } /* switch */
	  } /* while */
   } /* else */

/*--------------------------------------------------------------------*/
/*               Cleanup communicatons port, if active                */
/*--------------------------------------------------------------------*/

   if(port_active)
   {
	  Sshutdown();
	  printmsg(0,"Error: port was still active after dcp shutdown!");
	  panic();
   }

   return report();

} /*dcpmain*/


/*--------------------------------------------------------------------*/
/*    p r o c e s s                                                   */
/*                                                                    */
/*    The procotol state machine                                      */
/*--------------------------------------------------------------------*/

static CONN_STATE process( const POLL_MODE poll_mode )
{
   boolean master = TRUE;
   XFER_STATE state = ( poll_mode == POLL_ACTIVE ) ? XFER_SENDINIT :
													 XFER_RECVINIT;
   XFER_STATE old_state = XFER_EXIT;
							  /* Initialized to any state but the
								 original value of "state"           */
   XFER_STATE save_state = XFER_EXIT;

/*--------------------------------------------------------------------*/
/*  Yea old state machine for the high level file transfer procotol   */
/*--------------------------------------------------------------------*/

   while( state != XFER_EXIT )
   {
	printmsg(state == old_state ? 14 : 4 ,
			   "Machine state = %s", xfer_state(state));
	old_state = state;

	  switch( state )
	  {

		 case XFER_SENDINIT:  /* Initialize outgoing protocol        */
			state = sinit();
			break;

		 case XFER_RECVINIT:  /* Initialize Receive protocol         */
			state = rinit();
			break;

		 case XFER_MASTER:    /* Begin master mode                   */
			master = TRUE;
			state = XFER_NEXTJOB;
			break;

		 case XFER_SLAVE:     /* Begin slave mode                    */
			master = FALSE;
			state = XFER_RECVHDR;
			break;

		 case XFER_NEXTJOB:   /* Look for work in local queue        */
			state = scandir( rmtname );
			break;

		 case XFER_REQUEST:   /* Process next file in current job
					 in queue                            */
		 case XFER_SND2RCV:
			state = newrequest(state);
			break;

		 case XFER_PUTFILE:   /* Got local tranmit request           */
			state = ssfile();
			break;

		 case XFER_GETFILE:   /* Got local tranmit request           */
			state = srfile();
			break;

		 case XFER_SENDDATA:  /* Remote accepted our work, send data */
			state = sdata();
			break;

		 case XFER_SENDEOF:   /* File xfer complete, send EOF        */
			state = seof( master );
			break;

		 case XFER_FILEDONE:  /* Receive or transmit is complete     */
			state = master ? XFER_REQUEST : XFER_RECVHDR;
			break;

		 case XFER_NOLOCAL:   /* No local work, remote have any?     */
			state = sbreak();
			break;

		 case XFER_NOREMOTE:  /* No remote work, local have any?     */
			state = schkdir();
			break;

		 case XFER_RECVHDR:   /* Receive header from other host      */
			state = rheader();
			break;

		 case XFER_TAKEFILE:  /* Set up to receive remote requested
								 file transfer                       */
			state = rrfile();
			break;

		 case XFER_GIVEFILE:  /* Set up to transmit remote
								 requuest file transfer              */
			state = rsfile();
			break;

		 case XFER_RECVDATA:  /* Receive file data from other host   */
			state = rdata();
			break;

		 case XFER_RECVEOF:
			state = reof();
			break;

		 case XFER_LOST:      /* Lost the other host, flame out      */
		 case XFER_ABORT:
			if (state == XFER_LOST)
				printmsg(0,"process: connection lost to %s, \
previous state = %s", rmtname, xfer_state(save_state) );
			else
				printmsg(1,"process: aborting connection to %s, \
previous state = %s", rmtname, xfer_state(save_state) );
			hostp->status.hstatus = call_failed;
			if (xfer_stream != NULL) {
				fclose(xfer_stream);
				xfer_stream = NULL;
			}
			if (fwork != NULL) {
				fclose(fwork);
				fwork = NULL;
			}
			if (state == XFER_ABORT) {
				Aborted = TRUE;
				state = XFER_ENDP;
				break;
			} else
				return CONN_DROPLINE;

		 case XFER_ENDP:      /* Terminate the protocol              */
			if (!Aborted) {
				AnyContacted = Contacted = TRUE;
				if (hostp->status.hstatus == inprogress)
					hostp->status.hstatus = called;
			}
			state = endp();
			break;

		 default:
			printmsg(0,"process: Unknown state = %c, \
previous system state = %c", state, save_state );
			state = XFER_ABORT;
			break;
	  } /* switch */
	  save_state = old_state; /* Used only if we abort               */
   } /* while( state != XFER_EXIT ) */

/*--------------------------------------------------------------------*/
/*           Protocol is complete, terminate the connection           */
/*--------------------------------------------------------------------*/

   return CONN_TERMINATE;

} /* process */

static char *conn_state(CONN_STATE c)
{
	char *s;

	switch(c) {
	case CONN_INITIALIZE:       /* Select system to call, if any       */
		s = "INITIALIZE";
		break;
	case CONN_CALLUP1:           /* Dial out to another system          */
		s = "CALLUP1";
		break;
	case CONN_CALLUP2:           /* Dial out to another system          */
		s = "CALLUP2";
		break;
	case CONN_ANSWER:           /* Wait for phone to ring and user to  */
		s = "ANSWER";
		break;
	case CONN_LOGIN:            /* Modem is connected, do a login      */
		s = "LOGIN";
		break;
	case CONN_PROTOCOL:         /* Exchange protocol information       */
		s = "PROTOCOL";
		break;
	case CONN_SERVER:           /* Process files after dialing out     */
		s = "SERVER";
		break;
	case CONN_CLIENT:           /* Process files after being called    */
		s = "CLIENT";
		break;
	case CONN_TERMINATE:        /* Terminate procotol                  */
		s = "TERMINATE";
		break;
	case CONN_DROPLINE:         /* Hangup the telephone                */
		s = "DROPLINE";
		break;
	case CONN_EXIT:             /* Exit state machine loop             */
		s = "EXIT";
		break;
	default:
		s = "UNKNOWN";
		break;
	}
	return s;
}

static char *xfer_state(XFER_STATE c)
{
	char *s;

	switch(c) {
	case XFER_SENDINIT:          /* Initialize outgoing protocol        */
		s = "SENDINIT";
		break;
	case XFER_MASTER:            /* Begin master mode                   */
		s = "MASTER";
		break;
	case XFER_FILEDONE:          /* Receive or transmit is complete     */
		s = "FILEDONE";
		break;
	case XFER_NEXTJOB:           /* Look for work in local queue        */
		s = "NEXTJOB";
		break;
	case XFER_REQUEST:           /* Process work in local queue         */
		s = "REQUEST";
		break;
	case XFER_PUTFILE:           /* Send a file to remote host at our req */
		s = "PUTFILE";
		break;
	case XFER_GETFILE:           /* Retrieve a file from a remote host req */
		s = "GETFILE";
		break;
	case XFER_SENDDATA:          /* Remote accepted our work, send data */
		s = "SENDDATA";
		break;
	case XFER_SENDEOF:           /* File xfer complete, send EOF        */
		s = "SENDEOF";
		break;
	case XFER_NOLOCAL:           /* No local work, remote have any?     */
		s = "NOLOCAL";
		break;
	case XFER_SND2RCV:           /* Remote requests to send data        */
		s = "SND2RCV";
		break;
	case XFER_SLAVE:             /* Begin slave mode                    */
		s = "SLAVE";
		break;
	case XFER_RECVINIT:          /* Initialize Receive protocol         */
		s = "RECVINIT";
		break;
	case XFER_RECVHDR:           /* Receive header from other host      */
		s = "RECVHDR";
		break;
	case XFER_GIVEFILE:          /* Send a file to remote host at their req */
		s = "GIVEFILE";
		break;
	case XFER_TAKEFILE:          /* Retrieve a file from a remote host req	 */
		s = "TAKEFILE";
		break;
	case XFER_RECVDATA:          /* Receive file data from other host   */
		s = "RECVDATA";
		break;
	case XFER_RECVEOF:           /* Close file received from other host */
		s = "RECVEOF";
		break;
	case XFER_NOREMOTE:          /* No remote work, local have any?     */
		s = "NOREMOTE";
		break;
	case XFER_LOST:              /* Lost the other host, flame out      */
		s = "LOST";
		break;
	case XFER_ABORT:             /* Internal error, flame out           */
		s = "ABORT";
		break;
	case XFER_ENDP:              /* End the protocol                    */
		s = "ENDP";
		break;
	case XFER_EXIT:              /* Return to caller                    */
		s = "EXIT";
		break;
	default:
		s = "UNKNOWN";
		break;
	}
	return s;
}

/*--------------------------------------------------------------------*/
/*    c a n t                                                         */
/*                                                                    */
/*    report that we cannot open a critical file                      */
/*--------------------------------------------------------------------*/

static void cant(char *file)
{

   fprintf(stderr, "Can't open: \"%s\"\n", file);
   perror( file );
   abort();
} /*cant*/

char *hstat( hostatus current_status )
{
   switch ( current_status )
   {
      default:
       return "??????";

      case  phantom:          /* Entry not fully initialized      */
	    return "noinit";

      case  localhost:        /* This entry is for ourselves      */
	    return "local";

      case  gatewayed:        /* This entry is delivered to via   */
			      /* an external program on local sys */
	    return "gatway";

      case  nocall:           /* real host: never called          */
	 return "NEVER";

      case autodial:          /* Calling now                      */
	 return "DIALNG";

      case nodevice:          /* Device open failed               */
	 return "NODEV";

      case startup_failed:
	 return "NSTART";

      case  inprogress:       /* Call now active                  */
	 return "INPROG";

      case invalid_device:    /* Bad systems file entry           */
	 return "INVDEV";

      case  callback_req:     /* System must call us back         */
	  return "CALLBK";

      case  dial_script_failed:
			      /* Hardcoded auto-dial failed       */
	 return "NDIALS";

      case  dial_failed:      /* Hardcoded auto-dial failed       */
	 return "NODIAL";

      case  script_failed:    /* script in L.SYS failed           */
	 return "NSCRPT";

      case  max_retry:        /* Have given up calling this sys   */
	 return "MAXTRY";

      case  too_soon:         /* In retry mode: too soon to call  */
	 return "TOSOON";

      case  succeeded:        /* self-explanatory                 */
	 return "SUCESS";

      case  called:           /* self-explanatory                 */
	 return "CALLED";

      case  wrong_host:       /* Call out failed: wrong system    */
	 return "WRGHST";

      case  unknown_host:     /* Call in cailed: unknown system   */
	 return "UNKNWN";

      case  wrong_time:       /* Unable to call because of time   */
	 return "WRGTIM";

      case  call_failed:      /* Connection dropped in mid-call   */
	 return "FAILED";
   } /* switch */

} /* status */

