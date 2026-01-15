/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
   "DCP" a uucp clone. Copyright Richard H. Lamb 1985,1986,1987
*/

/*
   dcpxfer.c

   Revised edition of dcp

   Stuart Lynne May/87

   Copyright (c) Richard H. Lamb 1985, 1986, 1987
   Changes Copyright (c) Stuart Lynne 1987
   Changes Copyright (c) Jordan Brown 1990, 1991
   Changes Copyright (c) Andrew H. Derbyshire 1989, 1991


   Maintenance Notes:

   01Nov87 - that strncpy should be a memcpy! - Jal
   22Jul90 - Add check for existence of the file before writing
             it.                                                  ahd
   09Apr91 - Add numerous changes from H.A.E.Broomhall and Cliff
             Stanford for bidirectional support                   ahd
   05Jul91 - Merged various changes from Jordan Brown's (HJB)
             version of UUPC/extended to clean up transmission
	     of commands, etc.                                    ahd
   09Jul91 - Rewrite to use unique routines for all four kinds of
             transfers to allow for testing and security          ahd

*/


/*--------------------------------------------------------------------*/
/*                        System include files                        */
/*--------------------------------------------------------------------*/

#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <share.h>
#include <process.h>
#ifndef __EMX__
#include <direct.h>
#else
#include <dirent.h>
#endif

/*--------------------------------------------------------------------*/
/*                    UUPC/extended include files                     */
/*--------------------------------------------------------------------*/

#include "lib.h"
#include "dcp.h"
#include "dcpsys.h"
#include "dcpxfer.h"
#include "dcpgpkt.h"
#include "expath.h"
#include "hlib.h"
#include "hostable.h"
#include "import.h"
#include "ulib.h"
#include "screen.h"
#include "ssleep.h"

/*--------------------------------------------------------------------*/
/*                          Global variables                          */
/*--------------------------------------------------------------------*/

static unsigned char _Far rpacket[MAX_PKTSIZE+1], _Far spacket[MAX_PKTSIZE+1];

static int S_size;   /* number of bytes in the spacket buffer */

static char fname[FILENAME_MAX], tname[FILENAME_MAX], dname[FILENAME_MAX];
#define USERLEN 20
static char type, who[256], cmdopts[32];

extern long bytes;	/* moved to screen.c */
extern long restart_offs;  /* moved to screen.c */
extern struct timeb start_time; /* moved to screen.c */
extern int requests;
extern boolean resend;
extern char M_device[];
extern boolean scan_not_needed; /* from dcpsys.c */
int errors_per_request;
boolean send_only = FALSE;
static int seq = 0;

static char _Far command[BUFSIZ];


static boolean spool = FALSE; /* Received file is into spool dir     */
static char spolname[FILENAME_MAX];
							  /* Final host name of file to be
								 received into spool directory       */
static char tempname[FILENAME_MAX];
							  /* Temp name used to create received
								 file                                */

currentfile();

/*--------------------------------------------------------------------*/
/*                    Internal function prototypes                    */
/*--------------------------------------------------------------------*/

static boolean pktgetstr( char *s);
static boolean pktsendstr( char *s );
static void start_transfer(void);

static int  bufill(char  *buffer);
static int  bufwrite(char  *buffer,int  len);

/*************** SEND PROTOCOL ***************************/

/*--------------------------------------------------------------------*/
/*    s d a t a                                                       */
/*                                                                    */
/*    Send File Data                                                  */
/*--------------------------------------------------------------------*/

XFER_STATE sdata( void )
{

   if ((*sendpkt)((char *) spacket, S_size) != OK)  /* send data */
	  return XFER_LOST;    /* Trouble!                               */

   if ((S_size = bufill((char *) spacket)) == 0)  /* get data from file */
	  return XFER_SENDEOF; /* if EOF set state to that               */
   else if (S_size == -1)
	  return XFER_ABORT;

   return XFER_SENDDATA;   /* Remain in send state                   */
} /*sdata*/


/*--------------------------------------------------------------------*/
/*    b u f i l l                                                     */
/*                                                                    */
/*    Get a bufferful of data from the file that's being sent.        */
/*--------------------------------------------------------------------*/

static int bufill(char *buffer)
{
   size_t count = fread(buffer, sizeof *buffer, pktsize, xfer_stream);

   bytes += count;
   if (count < pktsize && ferror(xfer_stream))
   {
	  printerr("bufill", "read packet");
	  printmsg(0, "bufill: error reading data from file");
	  clearerr(xfer_stream);
	  return -1;
   }

   Saddbytes(bytes, SF_SEND);

   return count;
} /*bufill*/


/*--------------------------------------------------------------------*/
/*    b u f w r i t e                                                 */
/*                                                                    */
/*    Write a bufferful of data to the file that's being received.    */
/*--------------------------------------------------------------------*/

static int bufwrite(char *buffer, int len)
{
   size_t count = fwrite(buffer, sizeof *buffer, len, xfer_stream);

   bytes += count;
   if (count < len)
   {
	  printerr("bufwrite", "write file");
	  printmsg(0, "bufwrite: Tried to write %d bytes, actually wrote %u",
			len, count);
	  clearerr(xfer_stream);
   }

   Saddbytes(bytes, SF_RECV);

   return count;

} /*bufwrite*/

/*--------------------------------------------------------------------*/
/*    s b r e a k                                                     */
/*                                                                    */
/*    Switch from master to slave mode                                */
/*                                                                    */
/*    Sequence:                                                       */
/*                                                                    */
/*       We send "H" to other host to ask if we should hang up        */
/*       If it responds "HN", it has work for us, we become           */
/*          the slave.                                                */
/*       If it responds "HY", it has no work for us, we               */
/*          response "HY" (we have no work either), and               */
/*          terminate protocol and hangup                             */
/*                                                                    */
/*    Note that if more work is queued on the local system while      */
/*    we are in slave mode, schkdir() causes us to become the         */
/*    master again; we just decline here to avoid trying the queue    */
/*    again without intervening work from the other side.             */
/*--------------------------------------------------------------------*/

XFER_STATE sbreak( void )
{
   if (!pktsendstr("H"))      /* Tell slave it can become the master */
	  return XFER_LOST;       /* Xmit fail?  If so, quit transmitting*/

   if (!pktgetstr((char *)spacket)) /* Get their response            */
	  return XFER_LOST;       /* Xmit fail?  If so, quit transmitting*/

   if ((*spacket != 'H') || ((spacket[1] != 'N') && (spacket[1] != 'Y')))
   {
	  printmsg(0,"sbreak: invalid response from remote: %s",spacket);
	  return XFER_ABORT;
   }

   if (spacket[1] == 'N')     /* "HN" (have work) message from host? */
   {                          /* Yes --> Enter Receive mode          */
	  if (send_only) {
		printmsg(2, "sbreak: don't enter slave mode, send only");
		return XFER_ENDP;
	  }
	  printmsg(2, "sbreak: switch into slave mode");
	  return XFER_SLAVE;
   }
   else {                     /* No --> Remote host is done as well  */
	  hostp->status.hstatus = called;/* Update host status flags     */
	  if (!pktsendstr("HY"))  /* Tell the host we are done as well   */
		return XFER_LOST;
	  return XFER_ENDP;       /* Terminate the protocol              */
   } /* else */

} /*sbreak*/

/*--------------------------------------------------------------------*/
/*    s e o f                                                         */
/*                                                                    */
/*    Send End-Of-File                                                */
/*--------------------------------------------------------------------*/

XFER_STATE seof( const boolean purge_file )
{

   struct tm  *tmx;
   time_t msecs, now;
   char hostname[FILENAME_MAX];
   FILE *syslog;

/*--------------------------------------------------------------------*/
/*    Send end-of-file indication, and perhaps receive a              */
/*    lower-layer ACK/NAK                                             */
/*--------------------------------------------------------------------*/

   switch ((*eofpkt)())
   {
	  case RETRY:                /* retry */
		 printmsg(0, "seof: remote system asks that the file be resent");
		 rewind(xfer_stream);
		 S_size = bufill((char *)spacket);
		 if ( S_size == -1 )
			return XFER_ABORT;   /* cannot send file */
		 start_transfer();
		 return (S_size == 0 ? XFER_SENDEOF : XFER_SENDDATA);   /* stay in data phase */

	  case FAILED:
		 return XFER_ABORT;      /* cannot send file */

	  case OK:
		 if (xfer_stream != NULL) {
			 (void) fclose(xfer_stream);
			 xfer_stream = NULL;
		 }
		 break;                  /* sent, proceed */

	  default:
		 return XFER_LOST;
   }

   if (!pktgetstr((char *)spacket)) /* Receive CY or CN              */
	  return XFER_LOST;       /* Bomb the connection if no packet    */

   if ((*spacket != 'C') || ((spacket[1] != 'N') && (spacket[1] != 'Y')))
   {
	  printmsg(0,"seof: invalid response from remote: %s",spacket);
	  return XFER_ABORT;
   }

   if (!equaln((char *) spacket, "CY", 2)) {
	  char *s;
	  switch(spacket[2]) {
		case '5':
			s = "can't rename temp file";
			break;
		case '\0':
			s = "unknown";
			break;
		default:
			s = &spacket[2];
			break;
	  }
	  printmsg(0, "seof: remote unable to save %s, reason: %s", tname, s);
	  return XFER_ABORT;
   }

/*--------------------------------------------------------------------*/
/*                   If local spool file, delete it                   */
/*--------------------------------------------------------------------*/

   importpath(hostname, dname, rmtname);
							  /* Local name also used by logging     */

   if (purge_file && !equal(dname,"D.0"))
   {
	 unlink( hostname );
	 printmsg(4,"seof: file %s (%s) deleted", dname, hostname );
   }

/*--------------------------------------------------------------------*/
/*                            Update stats                            */
/*--------------------------------------------------------------------*/

   remote_stats.fsent++;
   remote_stats.bsent += bytes;
   hostp->idlewait = MIN_IDLE;	/* Reset wait time */

   msecs = msecs_from(&start_time);
   if (msecs <= 0)
		msecs = 1;

   Sftrans(SF_SDONE, hostname, NULL, 0); /* Sending done */

   printmsg(2, "seof: transfer completed, %ld chars/sec",
			bytes * 1000 / msecs);

   if (   access(SYSLOG,2) == 0
       && (syslog = FOPEN(SYSLOG, "a", TEXT)) != nil(FILE)
      ) {
	   fd_lock(SYSLOG, fileno(syslog));
	   time(&now);
	   tmx = localtime(&now);
	   seq++;
	   fprintf( syslog, "%s!%s %c %s (%d/%d-%02d:%02d:%02d) (C,%d,%d) [%s] -> %ld / %ld.%02d secs\n",
				hostp->via, who, type, tname,
				(tmx->tm_mon+1), tmx->tm_mday,
				tmx->tm_hour, tmx->tm_min, tmx->tm_sec,
				getpid(),
				seq,
				M_device,
				bytes,
				msecs / 1000, (int) ((msecs % 1000) / 10) );
	   fclose(syslog);
   }

/*--------------------------------------------------------------------*/
/*                          Return to caller                          */
/*--------------------------------------------------------------------*/

   return (spacket[2] == 'M') ?     /* Slave want to become master? */
		XFER_SND2RCV :
		XFER_FILEDONE ;     /* go get the next file to send */

} /*seof*/


/*--------------------------------------------------------------------*/
/*    n e w r e q u e s t                                             */
/*                                                                    */
/*    Determine the next request to be sent to other host             */
/*--------------------------------------------------------------------*/

XFER_STATE newrequest( XFER_STATE fromstate )
{
   int i;

/*--------------------------------------------------------------------*/
/*                 Verify we have no work in progress                 */
/*--------------------------------------------------------------------*/

   if (xfer_stream != NULL) {
	  printmsg(0, "newrequest: previous request not completed");
	  return XFER_ABORT;      /* Something's already open.  We're in
								 trouble!                            */
   }

/*--------------------------------------------------------------------*/
/*    Look for work in the current call file; if we do not find       */
/*    any, the job is complete and we can delete all the files we     */
/*    worked on in the file                                           */
/*--------------------------------------------------------------------*/

   if (fgets(command, sizeof(command), fwork) == nil(char))    /* More data?     */
   {                          /* No --> clean up list of files       */
	  printmsg(3, "newrequest: EOF for workfile %s",workfile);
	  fclose(fwork);
	  fwork = nil(FILE);
	  if (!errors_per_request) {
		if (unlink(workfile) == 0) /* Delete completed call file          */
			scan_not_needed = FALSE;
		requests++;
	  }
	  return (fromstate == XFER_REQUEST) ? XFER_NEXTJOB : XFER_NOLOCAL;
   }

/*--------------------------------------------------------------------*/
/*                  We have a new request to process                  */
/*--------------------------------------------------------------------*/

   i = strlen(command) - 1;
   printmsg(3, "newrequest: got command from %s",workfile);
   if (command[i] == '\n')            /* remove new_line from card */
	  command[i] = '\0';

   *cmdopts = *dname = *who = '\0';

   sscanf(command, "%c %s %s %s %s %s",
	 &type, fname, tname, who, cmdopts, dname);

   if (!*dname)
      strcpy(dname, "D.0");

   who[USERLEN] = '\0';

/*--------------------------------------------------------------------*/
/*                           Reset counters                           */
/*--------------------------------------------------------------------*/

   start_transfer();

/*--------------------------------------------------------------------*/
/*             Process the command according to its type              */
/*--------------------------------------------------------------------*/

   switch( type )
   {
	  case 'R':
		 return XFER_GETFILE;

	  case 'S':
		 return XFER_PUTFILE;

	  default:
		 return XFER_FILEDONE;   /* Ignore the line                  */
	} /* switch */

} /* newrequest */

/*--------------------------------------------------------------------*/
/*    s s f i l e                                                     */
/*                                                                    */
/*    Send File Header for file to be sent                            */
/*--------------------------------------------------------------------*/

XFER_STATE ssfile( void )
{
   char hostfile[FILENAME_MAX];
   char *filename, *showname;

   if (equal(dname, "D.0"))   /* Is there a spool file?              */
	  filename = fname;       /* No --> Use the real name            */
   else
	  filename = dname;       /* Yes --> Use it                      */

/*--------------------------------------------------------------------*/
/*              Convert the file name to our local name               */
/*--------------------------------------------------------------------*/

   showname = tname;
   if (strncmp(filename, "D.", 2) == 0)
	   importpath(hostfile, filename, rmtname);
   else {
	   strcpy(hostfile, filename);
	   (void) expand_path(hostfile, NULL, pubdir, NULL);
	   showname = hostfile;
   }

   printmsg(3, "ssfile: opening %s (%s) for sending", filename, hostfile);

   if (xfer_stream != NULL)
	(void) fclose(xfer_stream);

/*--------------------------------------------------------------------*/
/*    Try to open the file; if we fail, we just continue, because we  */
/*    may have sent the file on a previous call which failed part     */
/*    way through this job                                            */
/*--------------------------------------------------------------------*/

   xfer_stream = FOPEN( hostfile, "r", BINARY );

   if (xfer_stream == NULL)
   {
	  if (errno != ENOENT)
		errors_per_request ++;
	  printerr("ssfile", hostfile);
	  printmsg(-1, "ssfile: can't open/read %s (%s), already transmitted?", filename, hostfile);
	  return XFER_FILEDONE;      /* Try next file in this job  */
   } /* if */

/*--------------------------------------------------------------------*/
/*              The file is open, now set its buffering               */
/*--------------------------------------------------------------------*/

   if (setvbuf( xfer_stream, NULL, _IOFBF, xfer_bufsize))
   {
	  printerr("ssfile", hostfile);
	  printmsg(0, "ssfile: can't buffer=%d file %s (%s)",
				  xfer_bufsize, filename, hostfile);
	  return XFER_ABORT;         /* Clearly not our day; quit  */
   } /* if */


/*--------------------------------------------------------------------*/
/*    Okay, we have a file to process; offer it to the other host     */
/*--------------------------------------------------------------------*/

   printmsg(1, "ssfile: sending \"%s\" (%s) as \"%s\"", fname, hostfile, tname);

   Sftrans(SF_SEND, hostfile, showname, 0); /* Send */

   if (!pktsendstr( command ))   /* Tell them what is coming at them */
	  return XFER_LOST;

   if (!pktgetstr((char *)spacket))
	  return XFER_LOST;

   if ((*spacket != 'S') || ((spacket[1] != 'N') && (spacket[1] != 'Y')))
   {
	  printmsg(0,"ssfile: invalid response from remote: %s",spacket);
	  return XFER_ABORT;
   }

   if( spacket[1] == 'Y' ) {
       long offset = 0, size;

       if( resend && spacket[2] && sscanf( &spacket[2], "%li", &offset ) == 1 && offset > 0) {
	   if (   fseek( xfer_stream, 0L, SEEK_END ) == 0
	       && (size = ftell( xfer_stream )) != -1L
	       && offset <= size
	       && fseek( xfer_stream, offset, SEEK_SET ) == 0
	      ) {
		printmsg(4, "ssfile: remote already have this file, restart at %ld", offset );
	   } else {
		printmsg(0, "ssfile: wrong restart position requested: %ld, transfer aborted", offset);
		return XFER_ABORT;
	   }
       }
       restart_offs = offset;
   } else {                    /* Otherwise reject file transfer?     */
			       /* Yes --> Look for next file          */
	  char *s;

	  switch(spacket[2]) {
		case '2':
			s = "not permitted";
			break;
		case '4':
			s = "can't create temp file";
			break;
		case '6':
			s = "file is too large to transfer for now";
			break;
		case '7':
			s = "file is too large to transfer";
			break;
		case '8':
			s = "file is already received";
			break;
		case '\0':
			s = "reason unknown";
			break;
		default:
			s = &spacket[2];
			break;
	  }
	  printmsg(0, "ssfile: %s rejected by remote: %s", tname, s);
	  if (xfer_stream != NULL) {
		  (void) fclose( xfer_stream );
		  xfer_stream = NULL;
	  }
	  return XFER_FILEDONE;
   }

   S_size = bufill((char *) spacket);
   if ( S_size == -1 )
      return XFER_ABORT;   /* cannot send file */

   return (S_size == 0 ? XFER_SENDEOF : XFER_SENDDATA);      /* Enter data transmission mode        */

} /*ssfile*/

/*--------------------------------------------------------------------*/
/*    s r f i l e                                                     */
/*                                                                    */
/*    Send File Header for file to be received                        */
/*--------------------------------------------------------------------*/

XFER_STATE srfile( void )
{
   char hostfile[FILENAME_MAX];
   struct  stat    statbuf;
   char *showname;
   char *p;

/*--------------------------------------------------------------------*/
/*               Convert the filename to our local name               */
/*--------------------------------------------------------------------*/

   showname = tname;
   if (   strncmp(tname, "D.", 2) == 0
	   || strncmp(tname, "X.", 2) == 0
	  ) {
	   spool = TRUE;
	   importpath(hostfile, tname, rmtname);
	   mkfilename(tempname, tempdir, hostfile);
	   mkfilename(spolname, spooldir, hostfile);
   } else {
	   strcpy(hostfile, tname);
	   (void) expand_path(hostfile, NULL, pubdir, NULL);
	   showname = hostfile;
	   spool = FALSE;
   }

/*--------------------------------------------------------------------*/
/*    If the destination is a directory, put the originating          */
/*    original file name at the end of the path                       */
/*--------------------------------------------------------------------*/

   if ((hostfile[strlen(hostfile) - 1] == '/') ||
	   ((stat(hostfile , &statbuf) == 0) && (statbuf.st_mode & S_IFDIR)))
   {
	  char *slash = strrchr( fname, '/');

	  if ( slash == NULL )
		 slash = fname;
	  else
		 slash ++ ;

	  printmsg(5, "srfile: destination \"%s\" is directory, \
appending filename \"%s\"", hostfile, slash);

	  if (hostfile[strlen(hostfile) - 1] != '/')
		 strcat(hostfile, "/");

	  strcat( hostfile, slash );
   } /* if */

   for (p = hostfile; *p; p++)
	if (*p == '/') *p = '\\';

   if (!spool) {
	strcpy(spolname, hostfile);
	p = strrchr(hostfile, '\\');
	if (p == NULL) p = strrchr(hostfile, ':');
	if (p == NULL) p = hostfile;
	else p++;
	mkfilename(tempname, tempdir, p);
   }

   printmsg(1, "srfile: receiving \"%s\" as \"%s\" (%s)", fname, tname, hostfile);
   printmsg(5, "srfile: using temp name %s", tempname);

   Sftrans(SF_RECV, hostfile, showname, 0); /* Receive */

   if (!pktsendstr( command ))
	  return XFER_LOST;

   if (!pktgetstr((char *)spacket))
	  return XFER_LOST;

   if ((*spacket != 'R') || ((spacket[1] != 'N') && (spacket[1] != 'Y')))
   {
	  printmsg(0,"srfile: invalid response from remote: %s",spacket);
	  return XFER_ABORT;
   }

   if (spacket[1] != 'Y')     /* Otherwise reject file transfer?     */
   {                          /* Yes --> Look for next file          */
	  char *s;

	  switch (spacket[2]) {
		case '2':
			s = "not permitted or file doesn't exist";
			break;
		case '6':
			s = "file is too large to send now";
			break;
		case '\0':
			s = "reason unknown";
			break;
		default:
			s = &spacket[2];
			break;
	  }
	  printmsg(0, "srfile: access denied by remote: %s", s);
	  if (xfer_stream != NULL) {
		  (void) fclose( xfer_stream );
		  xfer_stream = NULL;
	  }
	  return XFER_FILEDONE;
   }

   if (xfer_stream != NULL)
	(void) fclose(xfer_stream);

/*--------------------------------------------------------------------*/
/*    We should verify the directory exists if the user doesn't       */
/*    specify the -d option, but I've got enough problems this        */
/*    week; we'll just auto-create using FOPEN()                      */
/*--------------------------------------------------------------------*/

   xfer_stream = FOPEN(tempname, "w", BINARY);

   if (xfer_stream == NULL)
   {
	  printerr("srfile", tempname);
	  printmsg(0, "srfile: cannot create %s", tempname);
	  return XFER_ABORT;
   }

/*--------------------------------------------------------------------*/
/*                     Set buffering for the file                     */
/*--------------------------------------------------------------------*/

   if (setvbuf( xfer_stream, NULL, _IOFBF, xfer_bufsize))
   {
	  printerr("srfile", tempname);
	  printmsg(0, "srfile: can't buffer=%d file %s (%s)",
		  xfer_bufsize, tname, tempname);
	  return XFER_ABORT;
   } /* if */

   return XFER_RECVDATA;      /* Now start receiving the data     */

} /*stfile*/

/*--------------------------------------------------------------------*/
/*    s i n i t                                                       */
/*                                                                    */
/*    Send Initiate:  send this host's parameters and get other       */
/*    side's back.                                                    */
/*--------------------------------------------------------------------*/

XFER_STATE sinit( void )
{
   int r = (*openpk)();

   return (r == S_LOST) ? XFER_LOST : (r ? XFER_ABORT : XFER_MASTER);

} /*sinit*/


/*********************** MISC SUB SUB PROTOCOL *************************/

/*
   s c h k d i r

   scan spooling directory for C.* files for the other system
*/

XFER_STATE schkdir( void )
{
   XFER_STATE c;

   scandir( NULL );           /* Reset directory search pointers     */
   c = scandir(rmtname);      /* Determine if data for the host      */
   scandir( NULL );           /* Reset directory search pointers     */

   switch ( c )
   {
	  case XFER_ABORT:        /* Internal error opening file         */
		 return XFER_ABORT;

	  case XFER_NOLOCAL:      /* No work for host                    */
		 hostp->status.hstatus = called;/* Update host status     */
		 if (! pktsendstr("HY") )
			return XFER_LOST;

		 if (!pktgetstr((char *)rpacket))
			return XFER_LOST; /* Didn't get response, die quietly    */
		 else {
			if (*rpacket != 'H' || rpacket[1] != 'Y')
			{
				printmsg(0,"schkdir: invalid response from remote: %s",rpacket);
				return XFER_ABORT;
			}
			return XFER_ENDP; /* Got response, we're out of here     */
		 }

	  case XFER_REQUEST:
		 if (! pktsendstr("HN") )
			return XFER_LOST;
		 else {
			printmsg( 2, "schkdir: switch into master mode" );
			return XFER_MASTER;
		 }

	 default:
		panic();
		return XFER_ABORT;

   } /* switch */
} /*schkdir*/

/*--------------------------------------------------------------------*/
/*    e n d p                                                         */
/*                                                                    */
/*    end the protocol                                                */
/*--------------------------------------------------------------------*/

XFER_STATE endp( void )
{
   if ((*closepk)() == S_LOST)
	  return XFER_LOST;

   if (spool)
   {
	  unlink(tempname);
	  spool = FALSE;
   }
   return XFER_EXIT;

} /*endp*/


/*********************** RECEIVE PROTOCOL **********************/

/*--------------------------------------------------------------------*/
/*    r d a t a                                                       */
/*                                                                    */
/*    Receive Data                                                    */
/*--------------------------------------------------------------------*/

XFER_STATE rdata( void )
{
   int   len;

   if ((*getpkt)((char *) rpacket, &len) != OK)
	  return XFER_LOST;

/*--------------------------------------------------------------------*/
/*                         Handle end of file                         */
/*--------------------------------------------------------------------*/

   if (len == 0)
	  return XFER_RECVEOF;

/*--------------------------------------------------------------------*/
/*                  Write incoming data to the file                   */
/*--------------------------------------------------------------------*/

   if (bufwrite((char *) rpacket, len) < len)
	  return XFER_ABORT;

   return XFER_RECVDATA;      /* Remain in data state                */

} /*rdata*/

/*--------------------------------------------------------------------*/
/*    r e o f                                                         */
/*                                                                    */
/*    Process EOF for a received file                                 */
/*--------------------------------------------------------------------*/

XFER_STATE reof( void )
{
   struct tm *tmx;
   time_t msecs, now;
   char *cy = "CY";
   char *cn = "CN";
   char *response = cy;
   char *fname = tempname;
   FILE *syslog;

/*--------------------------------------------------------------------*/
/*            Close out the file, checking for I/O errors             */
/*--------------------------------------------------------------------*/
   if (xfer_stream != NULL) {
	   if (fclose(xfer_stream) == EOF)  {
		  response = cn;          /* Report we had a problem             */
		  printerr( "reof", fname );
		  printmsg(0, "reof: error closing writed data file");
	   }
	   xfer_stream = NULL;        /* To make sure!                       */
   }

/*--------------------------------------------------------------------*/
/*    If it was a spool file, rename it to its permanent location     */
/*--------------------------------------------------------------------*/

   if (equal(response,cy))
   {
	  if (spool)
		  unlink( spolname );

	  if ( RENAME(tempname, spolname ))
	  {
		 printmsg(0,"reof: unable to rename %s to %s",
				  tempname, spolname);
		 response = cn;
	  } /* if ( RENAME(tempname, spolname )) */
	  spool = FALSE;
   } /* if (equal(response,cy)) */
   else {  /* If we had an error, delete file  */
	  printmsg(0,"reof: deleting corrupted file %s", fname );
	  unlink( fname );
   }

   if (!pktsendstr(response)) /* Announce we accepted the file       */
	  return XFER_LOST;       /* No ACK?  Return, if so              */

   if ( !equal(response, cy) )   /* If we had an error, delete file  */
	  return XFER_ABORT;

/*--------------------------------------------------------------------*/
/*            The file is delivered; compute stats for it             */
/*--------------------------------------------------------------------*/

   remote_stats.freceived++;
   remote_stats.breceived += bytes;
   hostp->idlewait = MIN_IDLE;	/* Reset wait time */

   msecs = msecs_from(&start_time);
   if (msecs <= 0)
		msecs = 1;

   Sftrans(SF_RDONE, fname, NULL, 0); /* Receiving done */

   printmsg(2, "reof: transfer completed, %ld chars/sec",
				  bytes * 1000 / msecs);

   if (   access(SYSLOG,2) == 0
       && (syslog = FOPEN(SYSLOG, "a", TEXT)) != nil(FILE)
      ) {
	   fd_lock(SYSLOG, fileno(syslog));
	   time(&now);
	   tmx = localtime(&now);
	   seq++;
	   fprintf( syslog,
			"%s!%s %c %s (%d/%d-%02d:%02d:%02d) (C,%d,%d) [%s] <- %ld / %ld.%02d secs\n",
				   hostp->via, who, type, tname,
				   (tmx->tm_mon+1), tmx->tm_mday,
				   tmx->tm_hour, tmx->tm_min, tmx->tm_sec,
				   getpid(),
				   seq,
				   M_device,
				   bytes,
				   msecs / 1000 , (int) ((msecs % 1000) / 10) );
	   fclose(syslog);
   }

/*--------------------------------------------------------------------*/
/*                      Return success to caller                      */
/*--------------------------------------------------------------------*/

   return XFER_FILEDONE;
} /* reof */

/*
   r h e a d e r

   Receive File Header
*/

XFER_STATE rheader( void )
{
   if (!pktgetstr(command))
	  return XFER_LOST;

/*--------------------------------------------------------------------*/
/*        Return if the remote system has no more data for us         */
/*--------------------------------------------------------------------*/

   if ((command[0] & 0x7f) == 'H')
	  return XFER_NOREMOTE;   /* Report master has no more data to   */

/*--------------------------------------------------------------------*/
/*                  Begin transforming the file name                  */
/*--------------------------------------------------------------------*/

   printmsg(5, "rheader: command \"%s\"", command);

   *cmdopts = *dname = *who = '\0';

   sscanf(command, "%c %s %s %s %s %s",
		 &type, fname, tname, who, cmdopts, dname);

   if (!*dname)
      strcpy(dname, "D.0");

   who[USERLEN] = '\0';

/*--------------------------------------------------------------------*/
/*                           Reset counters                           */
/*--------------------------------------------------------------------*/

   start_transfer();

/*--------------------------------------------------------------------*/
/*                 Return with next state to process                  */
/*--------------------------------------------------------------------*/

   switch (type)
   {
	  case 'R':
		 return XFER_GIVEFILE;

	  case 'S':
		 return XFER_TAKEFILE;

	  default:
		 printmsg(0,"rheader: unsupported verb \"%c\" rejected",type);
		 if (!pktsendstr("XN"))  /* Reject the request               */
			return XFER_LOST;    /* Die if reponse fails             */
		 return XFER_FILEDONE;   /* Process next request          */
   } /* switch */

} /* rheader */

/*--------------------------------------------------------------------*/
/*    r r f i l e                                                     */
/*                                                                    */
/*    Setup for receiving a file as requested by the remote host      */
/*--------------------------------------------------------------------*/

XFER_STATE rrfile( void )
{
   char filename[FILENAME_MAX];
   size_t subscript;
   struct  stat    statbuf;
   char *showname;
   char *name;
   char comm[32];
   long offset;

/*--------------------------------------------------------------------*/
/*       Determine if the file can go into the spool directory        */
/*--------------------------------------------------------------------*/

   spool = ((*tname == 'D' || *tname == 'X') && tname[1] == '.');

   expand_path( strcpy( filename, tname),
	  spool ? "." : pubdir, pubdir , NULL);

   printmsg(5, "rrfile: destination \"%s\"", filename );

/*--------------------------------------------------------------------*/
/*       Check if the name is a directory name (end with a '/')       */
/*--------------------------------------------------------------------*/

   subscript = strlen( filename ) - 1;

   if ((filename[subscript] == '/') ||
	   ((stat(filename , &statbuf) == 0) && (statbuf.st_mode & S_IFDIR)))
   {
	  char *slash = strrchr(fname, '/');
	  if (slash  == NULL)
		 slash = fname;
	  else
		 slash++;

	  printmsg(5, "rrfile: destination is directory \"%s\", adding \"%s\"",
			   filename, slash);

	  if ( filename[ subscript ] != '/')
		 strcat(filename, "/");
	  strcat(filename, slash);
   } /* if */

/*--------------------------------------------------------------------*/
/*          Let host munge filename as appropriate                    */
/*--------------------------------------------------------------------*/

   showname = tname;
   if (spool) {
	   static char relname[FILENAME_MAX];

	   importpath(relname, filename, rmtname);
	   mkfilename(tempname, tempdir, relname);
	   mkfilename(spolname, spooldir, relname);
   }
   else {
	   char *path = strchr(pubdir, ':');
	   char *rpath = strchr(filename, ':');
	   char *s;

	   if (!path || !rpath) {
			if (path)
				path++;
			else
				path = pubdir;
			if (rpath)
				strcpy(filename, rpath + 1);
	   }
	   else
			path = pubdir;
	   for (s = filename; *s; s++)
			if (*s == '/')
				*s = '\\';

	   printmsg(6, "rrfile: valid path for remotes is %s", path);

	   if (   strnicmp(filename, path, strlen(path)) != 0
		   || strstr(filename, "..") != NULL
		  ) {
		  printmsg(0, "rrfile: path not permitted, \"%s\" rejected",
					  filename);
		  if (!pktsendstr("SN2"))    /* Request not permitted  */
			 return XFER_LOST;       /* School is out, die            */
		  return XFER_FILEDONE;   /* Tell them to send next file   */
	   }
	   s = strrchr(filename, '\\');
	   if (s == NULL) s = strrchr(filename, ':');
	   if (s == NULL) s = filename;
	   else s++;
	   mkfilename(tempname, tempdir, s);
	   strcpy(spolname, filename);
	   showname = filename;
   }

   if (xfer_stream != NULL)
	(void) fclose(xfer_stream);
/*--------------------------------------------------------------------*/
/*            The filename is transformed, try to open it             */
/*--------------------------------------------------------------------*/

   name = tempname;
   xfer_stream = FOPEN( name, resend ? "a" : "w", BINARY );

   if (xfer_stream == NULL)
   {
	  printerr("rrfile", name);
	  printmsg(0, "rrfile: cannot open/write %s (%s)", filename, name);
	  if (!pktsendstr("SN4"))    /* Report cannot create file     */
		 return XFER_LOST;       /* School is out, die            */
	  return XFER_FILEDONE;   /* Tell them to send next file   */
   } /* if */

/*--------------------------------------------------------------------*/
/*               The file is open, now try to buffer it               */
/*--------------------------------------------------------------------*/

   if (setvbuf( xfer_stream, NULL, _IOFBF, xfer_bufsize))
   {
	  printerr("rrfile", name);
	  printmsg(0, "rrfile: can't buffer=%d file %s (%s)",
		  xfer_bufsize, filename, name);
	  if (!pktsendstr("SN4"))             /* Report cannot create file     */
		return XFER_LOST;
	  return XFER_ABORT;
   } /* if */

/*--------------------------------------------------------------------*/
/*    Announce we are receiving the file to console and to remote     */
/*--------------------------------------------------------------------*/

   printmsg(1, "rrfile: receiving \"%s\" as \"%s\" (%s)",
			   fname,filename,spolname);
   printmsg(5, "rrfile: using temp name %s",tempname);

   Sftrans(SF_RECV, name, showname, 0); /* Receive */

   offset = 0;
   if (resend) {
	fseek( xfer_stream, 0L, SEEK_END );
	offset = ftell( xfer_stream );
	if (offset > 0)
		printmsg( 4, "rrfile: restart %s at %ld", name, offset );
   }
   restart_offs = offset;

   if ( resend )
       sprintf( comm, "SY 0x%lx", offset );
   else
       strcpy( comm, "SY" );

   if (!pktsendstr(comm))
	  return XFER_LOST;

   return XFER_RECVDATA;   /* Switch to data state                */

} /*rrfile*/

/*--------------------------------------------------------------------*/
/*    r s f i l e                                                     */
/*                                                                    */
/*    Setup to transmit remote requested us to                        */
/*    send                                                            */
/*--------------------------------------------------------------------*/

XFER_STATE rsfile( void )
{
   char filename[FILENAME_MAX];
   char hostname[FILENAME_MAX];
   struct  stat    statbuf;
   size_t subscript;
   boolean localspool;
   char *showname;

/*--------------------------------------------------------------------*/
/*       Determine if the file can go into the spool directory        */
/*--------------------------------------------------------------------*/

   localspool = ((*tname == 'D' || *tname == 'X') && tname[1] == '.');

   expand_path( strcpy(filename, fname ) ,
				localspool ? "." : pubdir, pubdir, NULL);

/*--------------------------------------------------------------------*/
/*               Let host munge filename as appropriate               */
/*--------------------------------------------------------------------*/

   if (localspool)
	   importpath(hostname, filename, rmtname);
   else
	   strcpy(hostname, filename);
   printmsg(3, "rsfile: input \"%s\", source \"%s\", host \"%s\"",
									fname, filename , hostname);

/*--------------------------------------------------------------------*/
/*       Check if the name is a directory name (end with a '/')       */
/*--------------------------------------------------------------------*/

   subscript = strlen( filename ) - 1;

   if ((filename[subscript] == '/') ||
	   ((stat(hostname , &statbuf) == 0) && (statbuf.st_mode & S_IFDIR)))
   {
	  printmsg(0, "rsfile: source is directory \"%s\", rejecting",
			   hostname);
reject:
	  if (!pktsendstr("RN2"))    /* Report cannot send file       */
		 return XFER_LOST;       /* School is out, die            */
	  return XFER_FILEDONE;   /* Tell them to send next file   */
   } /* if */

   showname = tname;
   if (!localspool) {
	  char *path = strchr(pubdir, ':');
	  char *spath = strchr(hostname, ':');
	  char *s;

	  if (!path || !spath) {
			if (path)
				path++;
			else
				path = pubdir;
			if (spath)
				strcpy(hostname, spath + 1);
	  }
	   else
			path = pubdir;
	  for (s = hostname; *s; s++)
			if (*s == '/')
				*s = '\\';

	  printmsg(6, "rsfile: valid path for remotes is %s", path);

	  if (   strnicmp(hostname, path, strlen(path)) != 0
		  || strstr(hostname, "..") != NULL
		 ) {
		  printmsg(0, "rsfile: path not permitted, \"%s\" rejected",
			   hostname);
		  goto reject;
	  }
	  showname = hostname;
   }

   if (xfer_stream != NULL)
	(void) fclose(xfer_stream);

/*--------------------------------------------------------------------*/
/*            The filename is transformed, try to open it             */
/*--------------------------------------------------------------------*/

   printmsg(3, "rsfile: opening %s (%s) for sending", fname, hostname);
   xfer_stream = FOPEN( hostname, "r" , BINARY);
							  /* Open stream to transmit       */
   if (xfer_stream == NULL)
   {
	  printerr("rsfile", hostname);
	  printmsg(0, "rsfile: cannot open/read %s (%s)", fname, hostname);
	  if (!pktsendstr("RN2"))    /* Report cannot send file       */
		 return XFER_LOST;       /* School is out, die            */
	  return XFER_FILEDONE;   /* Tell them to send next file   */
   } /* if */

   if (setvbuf( xfer_stream, NULL, _IOFBF, xfer_bufsize))
   {
	  printerr("rsfile", hostname);
	  printmsg(0, "rsfile: can't buffer=%d file %s (%s)",
			   xfer_bufsize, fname, hostname);
	  if (!pktsendstr("RN2"))         /* Tell them we cannot handle it */
		return XFER_LOST;
	  return XFER_ABORT;
   } /* if */

/*--------------------------------------------------------------------*/
/*  We have the file open, announce it to the log and to the remote   */
/*--------------------------------------------------------------------*/

   if (!pktsendstr("RY"))
	  return XFER_LOST;

   printmsg(1, "rsfile: sending \"%s\" (%s) as \"%s\"", fname, hostname, tname);

   Sftrans(SF_SEND, hostname, showname, 0); /* Send */

   S_size = bufill((char *) spacket);
   if ( S_size == -1 )
	  return XFER_ABORT;   /* cannot send file */

   return (S_size == 0 ? XFER_SENDEOF : XFER_SENDDATA);   /* Switch to send data state        */

} /*rsfile*/


/*--------------------------------------------------------------------*/
/*    r i n i t                                                       */
/*                                                                    */
/*    Receive Initialization                                          */
/*--------------------------------------------------------------------*/

XFER_STATE rinit( void )
{

   return (*openpk)() == OK  ? XFER_SLAVE : XFER_LOST;

} /*rinit*/

/*--------------------------------------------------------------------*/
/*    p k t s e n d s t r                                             */
/*                                                                    */
/*    Transmit a control packet                                       */
/*--------------------------------------------------------------------*/

static boolean pktsendstr( char *s )
{
   printmsg(2, ">>> %s", s);

   if((*wrmsg)(s) != OK )
	  return FALSE;

   /* remote_stats.bsent += strlen(s) + 1; */

   return TRUE;
} /* pktsendstr */

/*--------------------------------------------------------------------*/
/*    p k t g e t s t r                                               */
/*                                                                    */
/*    Receive a control packet                                        */
/*--------------------------------------------------------------------*/

static boolean pktgetstr( char *s)
{
   if ((*rdmsg)(s) != OK )
	 return FALSE;

   /* remote_stats.breceived += strlen( s ) + 1; */
   printmsg(2, "<<< %s", s);

   return TRUE;
} /* pktgetstr */

static void start_transfer(void)
{
	bytes = 0;
	restart_offs = 0;
	ftime(&start_time);
	(*filepkt)();
}