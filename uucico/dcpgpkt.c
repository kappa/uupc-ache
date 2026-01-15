/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   dcpgpkt.c

   Revised edition of dcp

   Stuart Lynne May/87

   Copyright (c) Richard H. Lamb 1985, 1986, 1987
   Changes Copyright (c) Stuart Lynne 1987

   Maintenance notes:

   25Aug87 - Allow for up to 7 windows - Jal
   01Nov87 - those strncpy's should really be memcpy's! - Jal
   11Sep89 - Raise TimeOut to 15 - ahd
   30Apr90 - Add Jordon Brown's fix for short packet retries.
	     Reduce retry limit to 20                             ahd
   22Jul90 - Change error retry limit from per host to per
             packet.                                              ahd
   22Jul90 - Add error message for number of retries exceeded     ahd
   08Sep90 - Drop memmove to memcpy change supplied by Jordan
			 Brown, MS 6.0 and Turbo C++ agree memmove insures
             no overlap
*/

/* "DCP" a uucp clone. Copyright Richard H. Lamb 1985,1986,1987 */

/* 7-window "g" ptotocol */

/*
   Thanks goes to John Gilmore for sending me a copy of Greg Chesson's
   UUCP protocol description -- Obviously invaluable.
   Thanks also go to Andrew Tannenbaum for the section on Siding window
   protocols with a program example in his "Computer Networks" book.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <conio.h>
#include <dos.h>
#include <stdlib.h>
#ifdef __OS2__
#include <os2conio.h>
#endif

#include "lib.h"
#include "dcp.h"
#include "dcpsys.h"
#include "hostable.h"
#include "ulib.h"
#include "dcpgpkt.h"
#include "comm.h"
#include "screen.h"
#include "ssleep.h"

#define MINPKT 32
#define PKTSIZE 64
#define HDRSIZE   6
#define MAXTRY 4

/*--------------------------------------------------------------------*/
/*                     g-packet type definitions                      */
/*--------------------------------------------------------------------*/

#define DATA   0
#define CLOSE  1
#define NAK    2
#define SRJ    3
#define ACK    4
#define INITC  5
#define INITB  6
#define INITA  7

#define POK    -1

currentfile();

int local_pktsize = -1;
int remote_pktsize = -1;

/*--------------------------------------------------------------------*/
/*    We define max window as 3 to allow 1/2 the sequence numbers     */
/*    to be unused; this allows us to easily know when the remote     */
/*    is resending data we already acknowledged                       */
/*--------------------------------------------------------------------*/

int MAXWINDOW = 3;
time_t idletimer;

extern boolean need_timeout;
extern boolean remote_debug;
extern boolean force_remote_send_gsize;
extern int fs_isize;

#define NBUF   8              /* always SAME as MAXSEQ ? */
#define MAXSEQ 8

#define between(a,b,c) ((a<=b && b<c) || \
			(c<a && a<=b) || \
			(b<c && c<a))

#define nextpkt(x)    ((x + 1) % MAXSEQ)

/*--------------------------------------------------------------------*/
/*              Global variables for packet definitions               */
/*--------------------------------------------------------------------*/

static int rwl, swl, swu, rwu, irec, lazynak;
static unsigned nbuffers;
static INTEGER nerr;
static unsigned outlen[NBUF], inlen[NBUF], xmitlen[NBUF];
static boolean arrived[NBUF];
static int nwindows;
static char _Far outbuf[NBUF][MAX_PKTSIZE+1], _Far inbuf[NBUF][MAX_PKTSIZE+1];
static time_t ftimer[NBUF];
static int timeouts, outsequence, naksin, naksout, screwups;
static int shifts, badhdr, resends;
static unsigned char _Far grpkt[MAX_PKTSIZE+HDRSIZE+1];
static char protocol = 'g';   /* "G" for SVR4 procotocol            */
static boolean variablepacket = FALSE;  /* TRUE, if g-pktsize > 128 */
static boolean got_hdr = FALSE;
static int received;     /* Bytes already read into buffer */
static int saved_len = 0;
static boolean saved_active = FALSE;
static char *saved_msg = NULL;

/*--------------------------------------------------------------------*/
/*                    Internal function prototypes                    */
/*--------------------------------------------------------------------*/

static void gstats(void);
static int  gmachine(const int timeout);

static int gspack(int  type,
				   int  yyy,
				   int  xxx,
				   int  len,
				   unsigned xmit,
		   char  *data);

static int  grpack(int  *yyy,
				   int  *xxx,
				   int  *len,
				   char *data,
				   const int timeout);

static unsigned int checksum(char *data, int len);

/****************** SUB SUB SUB PACKET HANDLER ************/

#define MAXTIMEOUT 30
static int pkvtimeout [8]= { 2, 2, 1, 3, 8,15,15,20 };
static int pkvtimeack [8]= { 5, 5, 5,12,12,20,20,20 };

/* Set up times on window size for read and for write */
static int pkrsettime(int w_val,int pksize)
{
   int t = pkvtimeack[w_val]*(pksize+64L)/128;
   if (t > MAXTIMEOUT)
	t = MAXTIMEOUT;
   return t;
}

static int pkwsettime(int w_val,int pksize)
{
   int t = pkvtimeout[w_val]*(pksize+64L)/128;
   if (t > MAXTIMEOUT)
	t = MAXTIMEOUT;
   return t;
}
/*
	g_setup
*/
boolean g_setup(char *parms, boolean local)
{
	int i, j, k;
	char *p;

	if ((p = strstr(parms, "g(")) == NULL) {
		printmsg(0, "g_setup: g-protocol %s parms not found in %s for %s; g(W,P)",
						local ? "local" : "remote", parms, rmtname);
		return FALSE;
	}
	if (sscanf(p, "g(%d,%d)", &i, &j) != 2) {
		printmsg(0, "g_setup: bad format for g-protocol %s parms in %s for %s; g(W,P)",
						local ? "local" : "remote", parms, rmtname);
		return FALSE;
	}
	if (i < 1 || i > 7) {
		printmsg(0, "g_setup: bad g-window %s size - %d for %s; (0 > X < 8)",
					local ? "local" : "remote", i, rmtname);
		return FALSE;
	}
	if (local) {
		MAXWINDOW = i;
		printmsg(4, "g_setup: local g-window size: %d", i);
	}

	if (j < MINPKT || j > MAX_PKTSIZE) {
		printmsg(0, "g_setup: bad range for %s g-packets sizes - %d for %s; (%d < X < %d)",
					local ? "local" : "remote", j, rmtname, MINPKT, MAX_PKTSIZE);
		return FALSE;
	}

	for (i = 1, k = MINPKT; i <= MAX_Kbyte; i++, k *= 2) {
		if (j == k)
			goto OK1;
	}
	printmsg(0, "g_setup: bad value for %s g-packets size - %d for %s; (2**X)",
				 local ? "local" : "remote", j, rmtname);
	return FALSE;
OK1:
	if (local) {
		local_pktsize = j;
		printmsg(4, "g_setup: local g-packets size: %d", local_pktsize);
	}
	else {
		remote_pktsize = j;
		printmsg(4, "g_setup: will try remote g-packets size: %d", remote_pktsize);
	}

	return TRUE;
}


/*
   g o p e n p k
*/

int gopenpk(void)
{
   int i, xxx, yyy, len;
   boolean varallowed = TRUE;

   pktsize = PKTSIZE;   /* change it later after the init */

/*--------------------------------------------------------------------*/
/*                    Initialize proto parameters                     */
/*--------------------------------------------------------------------*/

   nerr = nbuffers = 0;
   idletimer = 0;
   swl = swu = 1;
   rwl = 0;
   nwindows = MAXWINDOW;
   rwu = nwindows - 1;
   variablepacket = FALSE;
   got_hdr = FALSE;
   received = 0;
   saved_active = FALSE;
   timeouts = outsequence = naksout = naksin = 0;
   screwups = shifts = badhdr = resends = 0;

   if (protocol == 'G') {
	 protocol = 'g';
	 varallowed = FALSE;
   }

   if (local_pktsize < 0)
	local_pktsize = PKTSIZE;
   if (remote_pktsize < 0)
	remote_pktsize = PKTSIZE;
   if (local_pktsize + HDRSIZE + 1 > fs_isize) {
	printmsg(0, "gopenpk: local g-packet (%d + header) not fit to input buffer (%d)",
		local_pktsize, fs_isize);
	return(FAILED);
   }

   Spakerr(nerr);
   Stoterr(remote_stats.errors);

	PacketTimeout = pkrsettime(MAXWINDOW, local_pktsize);
	printmsg(4, "gopenpk: local g-packets timeout: %d", PacketTimeout);

	for (i = 0; i < NBUF; i++) {
		ftimer[i] = 0;
		arrived[i] = FALSE;
	}

/*--------------------------------------------------------------------*/
/*                          3-way handshake                           */
/*--------------------------------------------------------------------*/

   if ((i = gspack(INITA, 0, 0, 0, pktsize, NULL)) == S_LOST)
	goto lost;
rsrt:
   if (nerr >= MaxErr)
   {
	  remote_stats.errors += nerr;
	  Spakerr(nerr);
	  Stoterr(remote_stats.errors);
	  nerr = 0;
	  printmsg(0,
		 "gopenpk: consecutive error limit of %ld exceeded, %ld total errors",
		  (long) MaxErr, remote_stats.errors);
	  return(FAILED);
   }

   switch (i = grpack(&yyy, &xxx, &len, NULL, PacketTimeout )) {

   case INITA:
	  Spacket(0, 'I', 0);
	  printmsg(5, "**got INITA");
	  if ((i = gspack(INITB, 0, 0, 0, local_pktsize, NULL)) == S_LOST)  /* data segment (packet) size */
		goto lost;
	  nwindows = yyy;
	  if (nwindows > MAXWINDOW)
		 nwindows = MAXWINDOW;
	  printmsg(4, "gopenpk: result g-window size: %d", nwindows);
	  rwu = nwindows - 1;
	  goto rsrt;

   case INITB:
	  Spacket(0, 'I', 1);
	  printmsg(5, "**got INITB");
	  if (yyy < 0 || yyy > MAX_Kbyte - 1) {
		printmsg(0, "gopenpk: output data segment size not in %d..%d, wrong uucp on %s",
					 MINPKT, MAX_PKTSIZE, rmtname);
		return(FAILED);
	  }
	  pktsize = 8 * (2 << (yyy+1));
	  if (pktsize != remote_pktsize) {
		  if (force_remote_send_gsize) {
			  printmsg(4, "gopenpk: remote reports g-packets size %d, force it to configured size", pktsize);
			  pktsize = remote_pktsize;
		  } else
			  printmsg(-1, "gopenpk: remote g-packets size %d don't match configured %d", pktsize, remote_pktsize);
	  }
	  printmsg(4, "gopenpk: remote result g-packets size %d", pktsize);
	  if (pktsize > 128 && varallowed)
		variablepacket = TRUE;

	  PacketTimeout = max(PacketTimeout, pkwsettime(nwindows, pktsize));
	  printmsg(4, "gopenpk: result g-packets timeout: %d", PacketTimeout);

	  if ((i = gspack(INITC, 0, 0, 0, pktsize, NULL)) == S_LOST)
		goto lost;
	  goto rsrt;

   case INITC:
	  Spacket(0, 'I', 2);
	  printmsg(5, "**got INITC");
	  break;

   default:
lost:
	  nerr++;
	  pktsize = PKTSIZE;
	  Spacket(0, 'e', -14);
	  Spakerr(nerr);
	  if (i == S_LOST)
		return(S_LOST);
	  printmsg(5, "**got SCREW UP (%d)", i);
	  if ((i = gspack(INITA, 0, 0, 0, pktsize, NULL)) == S_LOST)
		goto lost;
	  goto rsrt;
   }

   printmsg(4, "gopenpk: smart packets %sabled",
			   variablepacket ? "en" : "dis");
   nerr = 0;
   Spakerr(nerr);
   lazynak = 0;
   local_pktsize = remote_pktsize = -1;
   time( &idletimer );              /* Flag our link has been used   */
   return(OK); /* channel open */

} /*gopenpk*/


int Gopenpk(void)
{
	protocol = 'G';
	return gopenpk();
}


/*--------------------------------------------------------------------*/
/*    g f i l e p k t                                                 */
/*                                                                    */
/*    Begin a file transfer (not used by "g" protocol)                */
/*--------------------------------------------------------------------*/

int gfilepkt( void )
{
   return OK;
} /* gfilepkt */

/*--------------------------------------------------------------------*/
/*    g c l o s e p k                                                 */
/*                                                                    */
/*    Close packet machine                                            */
/*--------------------------------------------------------------------*/

int gclosepk()
{
   int i;

   for (i = 0; i < MAXTRY; i++) {
	  if (gspack(CLOSE, 0, 0, 0, 0, NULL) == S_LOST)
		return S_LOST;
	  switch (gmachine(PacketTimeout)) {
		 case CLOSE:
			goto eol;
		 case S_LOST:
			return S_LOST;
	  }
   }
eol:
   gstats();
   return(0);

} /*gclosepk*/

/*--------------------------------------------------------------------*/
/*    g s t a t s                                                     */
/*                                                                    */
/*    Report summary of errors for processing                         */
/*--------------------------------------------------------------------*/

static void gstats( void )
{
   remote_stats.errors += nerr;
   Spakerr(nerr);
   Stoterr(remote_stats.errors);
   nerr = 0;
   if ( remote_stats.errors || shifts || badhdr )
   {
	  printmsg(1,
		 "%d time outs, %d out of seq pkts, \
%d NAKs rec, %d NAKs sent",
			timeouts, outsequence, naksin, naksout);
	  printmsg(1,
		 "%d invalid pkt types, %d re-syncs, %d bad pkt hdrs, %d pkts resent",
			screwups, shifts, badhdr, resends);
   }
}

static boolean gresend_window(void)
{
	int rack;
	time_t now;

	printmsg(20, "gresend_window: from %d to %d", swl, swu);
	time(&now);
	for (rack = swl;
	     between(swl, rack, swu);
	     rack = nextpkt(rack))
	{                          /* resend rack->(swu-1)             */
		resends++;

		if (( outbuf[rack] == NULL ))
		{
			printmsg(0,"gresend_window: transmit of NULL packet (%d %d)",
					 rwl, rack);
			panic();
		}

		if (( xmitlen[rack] == 0 ))
		{
			printmsg(0,"gresend_window: transmit of 0 length packet (%d %d)",
					 rwl, rack);
			panic();
		}
		if (gspack(DATA, rwl, rack, outlen[rack], xmitlen[rack], outbuf[rack]) == S_LOST)
			return FALSE;
		printmsg(5, "*** resent %d", rack);
		idletimer = ftimer[rack] = now;
	} /* for */
	if (debuglevel == 4)
		printmsg(4, "*** resent (%d - %d) window", swl, rack);
	return TRUE;
}

/*
   g g e t p k t

   Gets no more than a packet's worth of data from
   the "packet I/O state machine".  May have to
   periodically run the packet machine to get some packets.

   on input:   don't care
   on return:  data+\0 and length in len.

   ret(0)   if all's well
   ret(-1) if problems (failed)
*/

int ggetpkt(char *data, int *len)
{
   int   retry = MaxErr;
   time_t start;
#ifdef _DEBUG
   int savedebug = debuglevel;
#endif

   irec = 1;
   checkref( data );

/*--------------------------------------------------------------------*/
/*                Loop to wait for the desired packet                 */
/*--------------------------------------------------------------------*/

   time(&start);
   while (!arrived[rwl] && retry)
   {
	  switch (gmachine(PacketTimeout)) {
		case S_LOST:
			return S_LOST;
		case POK:
			break;
		default:
			return FAILED;
	  }

	  if (!arrived[rwl] )
	  {
		time_t now;

		if (time(&now) > start + PacketTimeout) {
#ifdef _DEBUG
			if ( debuglevel < 6 )
			   debuglevel = 6;
#endif
			retry--;
			Spacket(1, 't', rwl + 1);
			printmsg(5,"ggetpkt: timeout %d waiting for inbound packet %d",
					 MaxErr - retry, remote_stats.packets + 1);
			timeouts++;
			/* Resend last mesage */
			gresend_window();
			start = now;
		} /* if (time( now ) > (start + PacketTimeout) ) */
	  } /* if (!arrived[rwl] ) */
   } /* while (!arrived[rwl] && i) */

   saved_active = FALSE;
#ifdef _DEBUG
   debuglevel = savedebug;
#endif

   if (!arrived[rwl])
   {
	  printmsg(0,"ggetpkt: remote host failed to respond after %ld seconds",
			   (long) PacketTimeout * MaxErr);
	  if (gclosepk() == S_LOST)
		return S_LOST;
	  return FAILED;
   }

/*--------------------------------------------------------------------*/
/*                           Got a packet!                            */
/*--------------------------------------------------------------------*/

   *len = inlen[rwl];
   memcpy(data, inbuf[rwl], *len);

   arrived[rwl] = FALSE;
   rwu = nextpkt(rwu)        ;  /* bump receive window */

   return(0);

} /*ggetpkt*/


/*
   g s e n d p k t

   Put at most a packet's worth of data in the packet state
   machine for transmission.
   May have to run the packet machine a few times to get
   an available output slot.

   on input: data=*data; len=length of data in data.

   return:
	0 if all's well
   -1 if problems (failed)
*/

int gsendpkt(char *data, int len)
{
#ifdef _DEBUG
   int savedebug = debuglevel;
#endif

   checkref( data );
   irec = 0;
   /* WAIT FOR INPUT i.e. if weve sent SWINDOW pkts and none have been
	  acked, wait for acks */
   while (nbuffers >= nwindows/* || s_count_pending() >= MAXWINDOW * PKTSIZE */)
	  switch (gmachine(PacketTimeout)) {
		case S_LOST:
			return S_LOST;
		case POK:
			break;
		default:
			return(-1);
	  }

/*--------------------------------------------------------------------*/
/*               Place packet in table and mark unacked               */
/*--------------------------------------------------------------------*/

   memcpy(outbuf[swu], data, len);

/*--------------------------------------------------------------------*/
/*                       Handle short packets.                        */
/*--------------------------------------------------------------------*/

   xmitlen[swu] = pktsize;
   if (variablepacket)
	while ( ((len * 2) < (int) xmitlen[swu]) && (xmitlen[swu] > MINPKT) )
		xmitlen[swu] /= 2;

   if ( xmitlen[swu] < MINPKT )
   {
	  printmsg(0,"gsendpkt: bad packet size %d, "
			   "data length %d",
			   xmitlen[swu], len);
	  xmitlen[swu] = MINPKT;
   }

   if (len < xmitlen[swu]) {   	/* Very Long Short Packets By Ache */
	unsigned short diff;
	boolean big;

	diff = xmitlen[swu] - len;
	big = (diff >= 128);
	memmove(outbuf[swu]+1+big, outbuf[swu], len);
	if (!big)
		outbuf[swu][0] = diff;
	else {
		outbuf[swu][0] = 0x80 | (diff & 0x7f);
		outbuf[swu][1] = diff >> 7;
	}
	memset(outbuf[swu]+len+1+big, 0, diff-1-big);
				  /* Pad with nulls.  Ugh.               */
   }

/*--------------------------------------------------------------------*/
/*                            Mark packet                             */
/*--------------------------------------------------------------------*/

   outlen[swu] = len;
   time(&ftimer[swu]);
   nbuffers++;

/*--------------------------------------------------------------------*/
/*                              send it                               */
/*--------------------------------------------------------------------*/

   if (gspack(DATA, rwl, swu, outlen[swu], xmitlen[swu], outbuf[swu]) == S_LOST)
	return S_LOST;

   swu = nextpkt(swu);        /* Bump send window                    */

#ifdef _DEBUG
   debuglevel = savedebug;
#endif

   return(0);

} /*gsendpkt*/


/*--------------------------------------------------------------------*/
/*    g e o f p k t                                                   */
/*                                                                    */
/*    Transmit EOF to the other system                                */
/*--------------------------------------------------------------------*/

int geofpkt( void )
{
	switch (gsendpkt("", 0)) {  /* Empty packet == EOF              */
		case S_LOST:
			return S_LOST;
		case 0:
			return OK;
		default:
			return FAILED;
	}
} /* geofpkt */

/*--------------------------------------------------------------------*/
/*    g w r m s g                                                     */
/*                                                                    */
/*    Send a message to remote system                                 */
/*--------------------------------------------------------------------*/

int gwrmsg( char *s )
{
   int len;

   if (s != NULL) {
	   len = strlen(s);
	   if (len + 1 > saved_len) {
		   saved_len = len + 1;
		   saved_msg = realloc(saved_msg, saved_len);
		   checkref(saved_msg);
	   }
	   strcpy(saved_msg, s);
	   saved_active = TRUE;
   }
   else {
	s = saved_msg;
	len = saved_len - 1;
   }

   for(; len >= pktsize; len -= pktsize, s += pktsize) {
	  int result = gsendpkt(s, pktsize);

	  if (result)
		 return result;
   }

   return gsendpkt(s, len+1);
} /* gwrmsg */

/*--------------------------------------------------------------------*/
/*    g r d m s g                                                     */
/*                                                                    */
/*    Read a message from the remote system                           */
/*--------------------------------------------------------------------*/

int grdmsg( char *s)
{
   for ( ;; )
   {
	int len;
	int result = ggetpkt( s, &len );

	if (result || (s[len-1] == '\0'))
		return result;
	s += len;
   } /* for */

} /* grdmsg */

/**********  Packet Machine  ********** RH Lamb 3/87 */

/*--------------------------------------------------------------------*/
/*    g m a c h i n e                                                 */
/*                                                                    */
/*    Ideally we would like to fork this process off in an            */
/*    infinite loop and send and receive packets through "inbuf"      */
/*    and "outbuf".  Can't do this in MS-DOS so we setup "getpkt"     */
/*    and "sendpkt" to call this routine often and return only        */
/*    when the input buffer is empty thus "blocking" the packet-      */
/*    machine task.                                                   */
/*--------------------------------------------------------------------*/

static int gmachine(const int timeout )
{

   boolean done   = FALSE;    /* True = drop out of machine loop  */
   boolean close  = FALSE;    /* True = terminate connection upon
					exit                      */
   boolean inseq  = TRUE;     /* True = Count next out of sequence
					packet as an error        */
   int pkttype;

   while ( !done )
   {
	  boolean resend = FALSE;    /* True = resend data packets       */
	  boolean donak  = FALSE;    /* True = NAK the other system      */
	  unsigned long packet_no = remote_stats.packets;
	  int rack, rseq, rlen, i1;
	  time_t now;

	  if (debuglevel > 9)     /* Optimize processing a little bit */
	  {
		 printmsg(10, "* send %d < W < %d, "
			      "receive %d < W < %d, "
			      "error %d, packet %ld",
			swl, swu, rwl, rwu, nerr,
			(long) remote_stats.packets);

/*--------------------------------------------------------------------*/
/*    Waiting for ACKs for swl to swu-1.  Next pkt to send=swu        */
/*    rwl=expected pkt                                                */
/*--------------------------------------------------------------------*/

	  }

/*--------------------------------------------------------------------*/
/*             Attempt to retrieve a packet and handle it             */
/*--------------------------------------------------------------------*/

	  need_timeout = FALSE;
	  pkttype = grpack(&rack, &rseq, &rlen, inbuf[nextpkt(rwl)], timeout);
	  time(&now);
	  switch (pkttype) {

		 case CLOSE:
			Spacket(0, 'C', 0);
			remote_stats.packets++;
			printmsg(5, "**got CLOSE");
			close = done = TRUE;
			break;

		 case EMPTY:
			printmsg(6, "**got EMPTY");

			/* Don't send ACK here, because duplicate ACK's recognized like NAK */

			if (ftimer[swl])
			{
			   printmsg(6, "---> seq, elapst %d %ld sec", swl,
					now - ftimer[swl]);
			   if ( need_timeout || ftimer[swl] <= now - PacketTimeout)
			   {
				   Spacket(0, 't', swl + 1);
				   printmsg(4, "*** timeout %d (%ld)",
					swl, (long) remote_stats.packets);
					   /* Since "g" is "go-back-N", when we time out we
						  must send the last N pkts in order.  The generalized
						  sliding window scheme relaxes this reqirment. */
				   nerr++;
				   Spakerr(nerr);
				   timeouts++;
				   resend = TRUE;
			   } /* if */
			} /* if */
			else if (need_timeout || now >= idletimer + PacketTimeout) {
			   printmsg(2,"*** BORED");
			   if (need_timeout)
				lazynak = 0;
			   donak = TRUE;
			}

			done = TRUE;
			break;

		case DATA:
			printmsg(5, "**got DATA %d %d", rack, rseq);
			i1 = nextpkt(rwl);   /* (R+1)%8 <-- -->(R+W)%8 */
			if (i1 == rseq) {
				Spacket(0, 'D', i1 + 1);
				lazynak--;
				remote_stats.packets++;
				idletimer = now;
				rwl = i1;
				inseq = arrived[rwl] = TRUE;
				inlen[rwl] = rlen;
				printmsg(5, "*** ACK d %d", rwl);
				if (gspack(ACK, rwl, 0, 0, 0, NULL) == S_LOST) {
					pkttype = S_LOST;
					goto lost;
				}
				done = TRUE;   /* return to caller when finished      */
							  /* in a mtask system, unneccesary      */
			} else {
				Spacket(0, 'd', -14);
				if (inseq || now > idletimer + PacketTimeout) {
					if (MAXWINDOW <= 3 || now > idletimer + PacketTimeout)
						donak = TRUE;  /* Only flag first out of sequence
										  packet as error, since we know
										  following ones also bad             */
				}
				if (inseq) {
					printmsg(4, "*** unexpect %d ne %d (%d - %d)",
						    rseq, i1, rwl, rwu);
					outsequence++;
					inseq = FALSE;
				}
				else
					printmsg(4, "*** wrong seq %d (%d - %d)",
						    rseq, rwl, rwu);
			} /* else */

			if ( swl == swu )     /* We waiting for an ACK?     */
				break;        /* No --> Skip ACK processing */

			/* else Fall through to ACK case */

		 case NAK:
		 case ACK:
			if (pkttype == NAK)
				printmsg(5, "**got NAK %d", rack);
			else if (pkttype == ACK)
				printmsg(5, "**got ACK %d", rack);

			i1 = swl;
			while(between(swl, rack, swu))
			{                             /* S<-- -->(S+W-1)%8 */
				remote_stats.packets++;
				printmsg(5, "*** ACK %d (%d - %d)", swl, i1, swu);
				if (pkttype == ACK)
					Spacket(0, 'A', rack + 1);
				else if (pkttype == NAK) {
					Spacket(0, 'n', rack + 1);
					printmsg(5, "*** NOT %d (%d - %d)", nextpkt(swl), i1, swu);
				}
				ftimer[swl] = 0;
				idletimer = now;
				nbuffers--;
				done = TRUE;            /* Get more data for input */
				swl = nextpkt(swl);
			} /* while */

			/* NAK after ACK with same number */
			if (   !done && pkttype == NAK
			    && between(swl, nextpkt(rack), swu)
			   ) {
				Spacket(0, 'n', rack + 1);
				remote_stats.packets++;
				printmsg(5, "*** NOT %d (%d - %d)", nextpkt(rack), i1, swu);
				done = TRUE;
			}

			if (!done) {
				outsequence++;
				if (pkttype == ACK) {
					Spacket(0, 'a', -14);
					printmsg(4,"*** ACK for wrong packet %d (%d - %d), ignored",
						   rack, swl, swu);
				} else {
					Spacket(0, 'n', -14);
					printmsg(4,"*** NOT for wrong packet %d (%d - %d), ignored",
						   nextpkt(rack), swl, swu);
				}
			}
			if (pkttype == NAK && done) {
				nerr++;
				Spakerr(nerr);
				naksin++;
				resend = TRUE;
			}
			break;

		 case ERROR:
			Spacket(0, 'e', -14);
			printmsg(5, "*** got BAD CHK");
			donak = TRUE;
			lazynak = 0;               /* Always NAK bad checksum */
			break;

		 default:
	  lost:
			nerr++;
			Spakerr(nerr);
			Spacket(0, 'e', -14);
			if (pkttype == S_LOST)
				return(S_LOST);
			screwups++;
			printmsg(5, "*** got SCREW UP");
			break;

	  } /* switch */

/*--------------------------------------------------------------------*/
/*      If we received an NAK or timed out, resend data packets       */
/*--------------------------------------------------------------------*/

	  if (resend)  {
		resend = FALSE;
		if (!gresend_window()) {
			pkttype = S_LOST;
			goto lost;
		}
	  }

/*--------------------------------------------------------------------*/
/*  If we have an error and have not recently sent a NAK, do so now.  */
/*  We then reset our counter so we receive at least a window full of */
/*                 packets before sending another NAK                 */
/*--------------------------------------------------------------------*/

	  if ( donak )
	  {
		 nerr++;
		 Spakerr(nerr);
		 if ( (lazynak < 1) || (now > idletimer + PacketTimeout))
		 {
			printmsg(5, "*** NAK d %d", rwl);
			if (gspack(NAK, rwl, 0, 0, 0, NULL) == S_LOST) {
				pkttype = S_LOST;
				goto lost;
			}
			naksout++;
			idletimer = now;
			lazynak = nwindows + 1;
		 } /* if ( lazynak < 1 ) */
	  } /* if ( donak ) */

/*--------------------------------------------------------------------*/
/*                   Update error counter if needed                   */
/*--------------------------------------------------------------------*/

	  if ((close || (packet_no != remote_stats.packets)) && (nerr > 0))
	  {
		 printmsg(2,"gmachine: packet %ld had %ld errors during transfer",
					 remote_stats.packets, (long) nerr);
		 remote_stats.errors += nerr;
		 Spakerr(nerr);
		 Stoterr(remote_stats.errors);
		 nerr = 0;
	  }

/*--------------------------------------------------------------------*/
/*    If we have an excessive number of errors, drop out of the       */
/*    loop                                                            */
/*--------------------------------------------------------------------*/

	  if (nerr >= MaxErr)
	  {
		 printmsg(0,
			"gmachine: consecutive error limit of %d exceeded, %d total errors",
			MaxErr, nerr + remote_stats.errors);
		 done = close = TRUE;
		 gstats();
	  }
   } /* while */

/*--------------------------------------------------------------------*/
/*    Return to caller, gracefully terminating packet machine if      */
/*    requested                                                       */
/*--------------------------------------------------------------------*/

   if ( close )
   {
	  if (gspack(CLOSE, 0, 0, 0, 0, NULL) == S_LOST) {
		pkttype = S_LOST;
		goto lost;
	  }
	  return CLOSE;
   }
   else
	  return POK;

} /*gmachine*/


/*************** FRAMING *****************************/

/*
   g s p a c k

   Send a packet

   type=type yyy=pkrec xxx=timesent len=length<=PKTSIZE data=*data
   ret(0) always
*/

static int gspack(int type,
				   int yyy,
				   int xxx,
				   int len,
				   unsigned xmit,
				   char *data)
{
   unsigned short check;
   unsigned char header[HDRSIZE+1];
   unsigned char dpkerr;
   int i;

   /***** Link Testing Mods - create artificial errors *****/
	dpkerr = 'n';
	if (remote_debug) {
		static boolean wasinfo = FALSE;

		if (!wasinfo) {
			Smisc("Press d:debug,t:port log,e:error,l:lost,p:partial,h:bad hdr,s:new seq--> ");
			wasinfo = TRUE;
		}
		dpkerr = kbhit();
		if (dpkerr && (dpkerr!='d') && (dpkerr!='t') &&
		    (dpkerr!='\33') && (dpkerr!='\3'))
			switch (dpkerr = getche()) {
				case 's':
					printmsg(-1, "gspack: enter new value: ");
					cscanf("%d", &xxx);
					break;
				case '\0':
					(void) getche();
					dpkerr = 'n';
					break;
			}
	}
   /***** End Link Testing Mods *****/

   if ( debuglevel > 5 )
	  printmsg(6, "send packet type %d yyy=%d xxx=%d len=%d buf=%d",
			   type, yyy, xxx, len, xmit);

   header[0] = '\020';
   header[4] = type << 3;
   switch (type) {

   case CLOSE:
	  Spacket(1, 'C', 0);
	  break;   /* stop protocol */

   case NAK:
	  Spacket(1, 'n', yyy + 1);
	  header[4] += yyy;
	  break;   /* reject */

   case ACK:
	  Spacket(1, 'A', yyy + 1);
	  header[4] += yyy;
	  break;   /* ack */

   case INITC:
	  Spacket(1, 'I', 2);
	  header[4] += nwindows;
	  break;

   case INITB:
	  Spacket(1, 'I', 1);
	  i = MINPKT;
	  while( i < xmit )
	  {
		 header[4] ++;
		 i *= 2;
	  }
	  break;

   case INITA:
	  Spacket(1, 'I', 0);
	  header[4] += MAXWINDOW;
	  break;

   case DATA:
	  Spacket(1, 'D', xxx + 1);
	  header[4] = 0x80 + (xxx << 3) + yyy;
	  if (len < xmit) { /* short packet? */
		 header[4] |= 0x40;
		 printmsg(13, "    data(%d)=|%.*s|", len+1, len+1, data);
	  }
	  else
		 printmsg(13, "    data(%d)=|%.*s|", len, len, data);
	  break;

   default:
	  printmsg(0,"gspack: Invalid packet type %d",type);
	  panic();
   }

   if (type != DATA) {
	  header[1] = 9; /* control packet size = 0 (9) */
	  check = (0xaaaa - header[4]) & 0xffff;
   } else {
	  header[1] = 1;
	  i = MINPKT;
	  while( i < xmit )
	  {
		 header[1] ++;
		 i *= 2;
	  }

	  if ( i != xmit )        /* Did it come out exact power of 2?   */
	  {
		 printmsg(0,"gspack: packet length error ... %d != %d for K = %d",
			   i, xmit, header[1]);
		 panic();             /* No --> Well, we blew THAT math      */
	  } /* if ( i != xmit ) */

/*--------------------------------------------------------------------*/
/*                        Compute the checksum                        */
/*--------------------------------------------------------------------*/

	  check = checksum(data, xmit);
	  check = (check ^ header[4]) & 0xffff;
	  check = (0xaaaa - check) & 0xffff;
   }
   header[2] = check;
   header[3] = check >> 8;
   header[5] = header[1] ^ header[2] ^ header[3] ^ header[4];

   /***** More Link Testing Mods *****/
   switch(dpkerr) {
   case 'e':
	  data[10] = - data[10];
	  break;
   case 'h':
	  header[5] = ~header[5];
	  break;
   case 'l':
	  return S_LOST;
   case 'p':
	  if (swrite((char *) header, HDRSIZE) == S_LOST)
		return S_LOST;
	  if (header[1] != 9)
		 if (swrite(data, xmit - 3) == S_LOST)
			return S_LOST;
	  return 0;
   }
   /***** End Link Testing Mods *****/

   if (swrite((char *) header, HDRSIZE) == S_LOST)      /* header is 6-bytes long */
		return S_LOST;
   if (header[1] != 9)
	  if (swrite(data, xmit) == S_LOST)     /* data is always PKTSIZE bytes long */
		return S_LOST;

   return 0;
} /*gspack*/


/*
   g r p a c k

   Read packet

   on return: yyy=pkrec xxx=pksent len=length<=PKTSIZE  data=*data

   ret(type)   ok
   ret(EMPTY)  input buf empty
   ret(ERROR)  bad header

   ret(EMPTY)  lost packet timeout
   ret(ERROR)  checksum error

   NOTE (specifications for sread()):

   sread(buf, n, timeout)
	  while(TRUE) {
         if (# of chars available >= n) (without dec internal counter)
            read n chars into buf (decrement internal char counter)
            break
    else
	   if (time > timeout)
		  break;
	  }
      return(# of chars available)

*/

static int grpack(int *yyy, int *xxx, int *len, char *data, const int timeout)
{
   int needed;
   unsigned short type, check, checkchk, i, total=0;
   unsigned char c, c2;
   time_t start;

   if (keyb_control(TRUE)) {
	need_timeout = TRUE;
	return EMPTY;
   }

   if (got_hdr)
	  goto get_data;

/*--------------------------------------------------------------------*/
/*   Spin up to timeout waiting for a Control-P, our sync character   */
/*--------------------------------------------------------------------*/

   start = 0;
   while (!got_hdr)
   {
	  unsigned char *psync;

	  needed = HDRSIZE - received;
	  if ( needed > 0 )       /* Have enough bytes for header?       */
	  {                       /* No --> Read as much as we need      */
		 int wait;

		 if ( start == 0 )    /* First pass through data?            */
		 {                    /* Yes --> Set timers up               */
			time(&start);
			wait = timeout;
		 }
		 else {
			wait = timeout - time(NULL) - start;
			if (wait < 0)     /* Negative timeout?                   */
			   wait = 0;      /* Make it no time out                 */
		 } /* else */

		 switch (sread(&grpkt[received], needed, wait)) {
		 case S_TIMEOUT:
			 return EMPTY;
		 case S_LOST:
			 return S_LOST;
		 }

		 received += needed;
	  } /* if ( needed < received ) */

/*--------------------------------------------------------------------*/
/*            Search for sync character in newly read data            */
/*--------------------------------------------------------------------*/

	  printmsg(10,"grpack: have %d chars after reading %d",
			   received, needed);
	  psync = memchr( grpkt, '\020', received );
	  if ( psync == NULL )    /* Did we find the sync character?     */
		 received = 0;        /* No --> Reset to empty buffer        */
	  else if ( psync != grpkt ) /* First character in buffer?       */
	  {                       /* No --> Make it first character      */
		 received -= psync - grpkt;
		 shifts++;
		 memmove( grpkt, psync, received );
							  /* Shift buffer over                   */
	  }

/*--------------------------------------------------------------------*/
/*    If we have read an entire packet header, then perform a         */
/*    simple XOR checksum to determine if it is valid.  If we have    */
/*    a valid checksum, drop out of this search, else drop the        */
/*    sync character and restart the scan.                            */
/*--------------------------------------------------------------------*/

	  if ( received >= HDRSIZE )
	  {
		 i = grpkt[1] ^ grpkt[2] ^ grpkt[3] ^ grpkt[4] ^ grpkt[5];
		 printmsg(i ? 5 : 10, "prpkt %02x %02x %02x %02x %02x .. %02x ",
			grpkt[1], grpkt[2], grpkt[3], grpkt[4], grpkt[5], i);

		 if (i == 0 && grpkt[1] >= 1 && grpkt[1] <= 9) {
			if (grpkt[1] == 9) {
				c = grpkt[4] >> 3;
				if (c < CLOSE || c > INITA || c == SRJ)
					goto bad_header;
				check = grpkt[2] | (grpkt[3] << 8);
				checkchk = 0xaaaa - grpkt[4];
				if (checkchk != check)
					goto bad_header;
			} else {
				c = grpkt[4] >> 6;
				if (c != 2 && c != 3)
					goto bad_header;
			}
			got_hdr = TRUE;   /* Yes --> Drop out of loop            */
		 } else {               /* No  --> Flag it, continue loop      */
bad_header:
			badhdr++;
			printmsg(4, "*** bad pkt header ***");
			memmove( grpkt, &grpkt[ 1 ], --received );
							  /* Begin scanning for sync character
								 with next byte                      */
		 }
	  } /* if ( received > HDRSIZE ) */
	  else
		printmsg(10, "grpack: received %d protocol chars", received);
   } /* while */

/*--------------------------------------------------------------------*/
/*                       Handle control packets                       */
/*--------------------------------------------------------------------*/

   if (grpkt[1] == 9)
   {
	  got_hdr = FALSE;
	  if ( data != NULL )
		 *data = '\0';
	  *len = 0;
	  c = grpkt[4];
	  type = c >> 3;
	  *yyy = c & 0x07;
	  *xxx = 0;
   }
/*--------------------------------------------------------------------*/
/*                        Handle data packets                         */
/*--------------------------------------------------------------------*/
   else {
get_data:
	  if ( data == NULL )
	  {
		 printmsg(0,"grpack: unexpected data packet!");
		 received = 0;
		 return(ERROR);
	  }

/*--------------------------------------------------------------------*/
/*             Compute the size of the data block desired             */
/*--------------------------------------------------------------------*/

	  total = 8 * (2 << grpkt[1]);
	  needed = total + HDRSIZE - received;

/*--------------------------------------------------------------------*/
/*     If we don't have enough data in the buffer, read some more     */
/*--------------------------------------------------------------------*/

	  if (needed > 0)
		 switch (sread(&grpkt[HDRSIZE+total-needed], needed , timeout)) {
		 case S_TIMEOUT:
			 return EMPTY;
		 case S_LOST:
			 return S_LOST;
		 }

	  got_hdr = FALSE;

/*--------------------------------------------------------------------*/
/*              Break packet header into various values               */
/*--------------------------------------------------------------------*/

	  type = DATA;
	  c2 = grpkt[4];
	  c = c2 & 0x3f;
	  *xxx = c >> 3;
	  *yyy = c & 0x07;
	  check = grpkt[2] | (grpkt[3] << 8);
	  checkchk = checksum(grpkt + HDRSIZE, total);
	  checkchk = 0xaaaa - (checkchk ^ c2);
	  checkchk &= 0xffff;
	  if (checkchk != check) {
		 printmsg(4, "*** checksum error ***");
		 memmove( grpkt, grpkt + HDRSIZE, total );
							  /* Save data so we can scan for sync   */
		 received = total;    /* Note the amount of the data in buf  */
		 return(ERROR);       /* Return to caller with error         */
	  }

/*--------------------------------------------------------------------*/
/*    The checksum is correct, now determine the length of the        */
/*    data to return.                                                 */
/*--------------------------------------------------------------------*/

	  *len = total;
	  if (c2 & 0x40) {	/* Short Packets */
						/* Very Long Short Packets By Ache */
		boolean big;
		unsigned short diff;

		big = !!(grpkt[HDRSIZE] & 0x80);
		diff = grpkt[HDRSIZE] & 0x7f;
		if (big)
			diff |= grpkt[HDRSIZE+1] << 7;
		*len -= diff;
		memmove(data, grpkt+HDRSIZE+1+big, *len);
	  }
	  else
		memcpy( data, grpkt + HDRSIZE, *len);
   } /* else */

/*--------------------------------------------------------------------*/
/*           Announce what we got and return to the caller            */
/*--------------------------------------------------------------------*/

   received = 0;              /* Returning entire buffer, reset count */
   if (debuglevel > 5) {
	   printmsg(6, "receive packet type %d yyy=%d xxx=%d len=%d chk=%04x",
		  type, *yyy, *xxx, *len, check);
	   if (type == DATA)
		   printmsg(13, "    data(%d)=|%.*s|",
		                total, total, grpkt + HDRSIZE);
   }

   return(type);
} /*grpack*/


/*
   c h e c k s u m
*/

static unsigned int checksum(char *data, int len)
{
   int i, j;
   unsigned int tmp, chk1, chk2;
   chk1 = 0xffff;
   chk2 = 0;
   j = len;
   for (i = 0; i < len; i++) {
	  if (chk1 & 0x8000) {
         chk1 <<= 1;
         chk1++;
      } else {
         chk1 <<= 1;
      }
	  tmp = chk1;
      chk1 += (data[i] & 0xff);
      chk2 += chk1 ^ j;
	  if ((chk1 & 0xffff) <= (tmp & 0xffff))
		 chk1 ^= chk2;
      j--;
   }
   return(chk1 & 0xffff);

} /*checksum*/
