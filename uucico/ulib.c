/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   ibmpc/ulib.c

   DCP system-dependent library

   Services provided by ulib.c:

   - login
   - UNIX commands simulation
   - serial I/O
   - rnews

   Updated:

      14May89  - Added hangup() procedure                               ahd
	  21Jan90  - Replaced code for rnews() from Wolfgang Tremmel
				 <tremmel@garf.ira.uka.de> to correct failure to
                 properly read compressed news.                         ahd
   6  Sep 90   - Change logging of line data to printable               ahd
	  8 Sep 90 - Split ulib.c into dcplib.c and ulib.c                  ahd
*/

#include <assert.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <process.h>
#include <errno.h>

#ifdef __TURBOC__
#include <dir.h>
#else
#include <direct.h>
#endif

#include "lib.h"
#include "hostable.h"
#include "dcp.h"
#include "dcpsys.h"
#include "dcplib.h"
#include "hlib.h"
#include "ulib.h"
#include "comm.h"
#include "fossil.h"
#include "ssleep.h"
#include "pushpop.h"
#include "arpadate.h"
#include "screen.h"

/* network definitions for Waterloo TCP library */
#include "tcp.h"
/* add some prototypes */
extern sock_init(void);
extern void dbug_init(void);

static void tcpfini(boolean);

currentfile();

boolean   port_active = FALSE;  /* TRUE = port handler handler active  */
boolean fossil = FALSE;
boolean need_timeout = FALSE;
static boolean need_out_flush = FALSE;
static boolean do_modstat = FALSE;
extern fs_isize, fs_osize;
extern boolean fs_direct;
extern long fs_baud;
extern boolean use_old_status, fs_xon, NoCheckCarrier;

/* IBM-PC I/O routines */

/* "DCP" a uucp clone. Copyright Richard H. Lamb 1985,1986,1987 */

/*************** BASIC I/O ***************************/
/* Saltzers serial package (aka Info-IBMPC COM_PKG2):
 * Some notes:  When packets are flying in both directions, there seems to
 * be some interrupt handling problems as far as receiving.  Checksum errors
 * may therefore occur often even though we recover from them.  This is
 * especially true with sliding windows.  Errors are very few in the VMS
 * version.  RH Lamb
 */


#define  STOPBIT  1
char LINELOG[] = "LineData.Log";      /* log serial line data here */
char M_device[30] = "NONE";
extern boolean Direct, Makelog;
int logmode = 0;             /* Not yet logging            */
#define WRITING 1
#define READING 2
FILE *log_stream = NULL;
extern void setflow(boolean);

/* variables needed for Waterloo TCP library routines */
static tcp_Socket far mysocket;
static tcp_Socket *netso = NULL;
long netaddr;
boolean wattcp = FALSE;
boolean tcp_break;

#define BUFFERSIZE 5120

static char *netbuf = NULL;	/* TELNET buffer */
static char *ibuf = NULL;
static i_head, i_tail;

/* TELNET data stream interpretation states */
#define T_DATA 0
#define T_IAC  1
#define T_COMMAND 2
#define T_CR      3
#define CR '\015'
#define LF '\012'
#define NUL '\0'
static int t_state = T_DATA;

#define IAC '\377'
#define T_DO 253
#define T_WONT '\374'
#define T_DONT '\376'
#define T_WILL   251

static unsigned char t_command;
/* used to indicate last write failed IAC transparency */
static boolean IAC_pending = FALSE;
static boolean NUL_pending = FALSE;

#define IPPORT_TELNET 23
#define IPPORT_UUCP  540
static int IP_port = 0;

/*
   ipport - recognize TELNET and UUCP as modem names
*/
int ipport(char *portname)
{
  if      (!strcmp(portname,"TELNET")) IP_port = IPPORT_TELNET;
  else if (!strcmp(portname, "UUCP" )) IP_port = IPPORT_UUCP;
  else {
     IP_port = 0;
     return FALSE;
  }
  return TRUE;
}

/* Replace WatTCP output fuction */
/***********************************************************/
void huge cdecl outch(char c)
{
	static boolean bol = TRUE;

	if (c == '\r')
		return;
	if (bol) {
		dbgputs("\nWaterlooTCP: ");
		bol = FALSE;
	}
	dbgputc(c);
	if (c == '\n')
		bol = TRUE;
}
/***********************************************************/

static int tcpinit(long destipaddr)
{
   int status;

   netso = &mysocket;
   /* connect it to the other end */
   printmsg(1, "tcpinit: connecting to %s via PACKET DRIVER",rmtname);

   if (!tcp_open(netso,0,destipaddr,IP_port,NULL)) {
      printmsg(0, "tcpinit: can't connect to remote host");
      netso = NULL;
      return -1;
   }
/* printmsg(1, "tcp_opened: a = %08lx, p = %d\n", destipaddr, IP_port); */
#ifdef XLAN205T
   /* Hack for Excelan 205T packet driver */ netso->mss = 512;
#endif
   printmsg(-1,"tcpinit: Waiting for connection established...");
   sock_wait_established(netso,sock_delay,NULL,&status);
   return 0;

sock_err:
   switch (status) {
	case  1: printmsg(0, "tcpinit: Connetcion closed"); break;
	case -1: printmsg(0, "tcpinit: Socket error: %s", sockerr(netso)); break;
   }
   tcpfini(FALSE);
   return -1;
}

static int readtcp(int len, char buffer[])
{
   int status, rc;
   byte command_buf[3];

   sock_tick(netso, &status);
   if (!sock_dataready(netso))
      return 0;
   rc = sock_fastread(netso,
	   (byte*)(IP_port == IPPORT_TELNET ? netbuf : buffer), len);
   printmsg(21, "readtcp: len=%d, got %d", len, rc);
   if (rc > 0) {
      if (IP_port == IPPORT_TELNET) {
	 /* network returned some data - interpret it */
	 register i, count;

	 for (count=0, i=0; i<rc; i++) {
            switch (t_state) {
	    case T_DATA:
	       if (netbuf[i] == IAC)
		  t_state = T_IAC;
               else {
		  buffer[count++] = netbuf[i];
		  if (netbuf[i] == CR) t_state = T_CR;
               }
               break;
            case T_IAC: /* interpret as command */
	       if (netbuf[i] != IAC) {
		  t_command = netbuf[i];
		  printmsg(4,"readtcp: TELNET command %d received",t_command);
		  t_state = T_COMMAND;
	       } else {
		  buffer[count++] = netbuf[i];
		  t_state = T_DATA;
	       }
	       break;
	    case T_COMMAND: /* TELNET command received */
	       /* refuse perform any options and ignore the rest */
	       if (t_command < 251) {/* is it one byte command?*/
		  buffer[count++] = netbuf[i];
	       } else if (t_command == T_DO) {
		  int ret;

		  command_buf[0] = IAC;
		  command_buf[1] = T_WONT;
		  command_buf[2] = netbuf[i];
		  ret = sock_fastwrite(netso,command_buf,3);
		  if (!tcp_tick(netso)) {
			status = 1;
			goto sock_err;
		  }
		  /* no completion codes are tested now */
		  ret = netbuf[i];
		  printmsg(4,"readtcp: refuse to do TELNET option %d",ret);
	       }
	       t_state = T_DATA;
	       break;
	    case T_CR: /* carriage return received */
	       if (netbuf[i] != NUL) {/* strip out NULs */
		  buffer[count++] = netbuf[i];
	       }
	       t_state = T_DATA;
	       break;
	    }  /* switch */
	 } /* for */
	 rc = count;
      } /* end of TELNET data processing */
   } else {
      /* there was no network data */
      rc = 0;
   }
   return rc;

sock_err: /* go there from sock_tick() macro */
    switch (status) {
	case 1 : printmsg(0, "readtcp: Connection closed");
		 break;
	case-1 : printmsg(0, "readtcp: Socket error: %s", sockerr(netso));
		 break;
    }
   tcp_break = TRUE; /* indicate connection failed */
   return 0;
}

static writetcp(int len, char buffer[])
{
   int rc;
   int i, count;
   boolean doubled = FALSE;
   boolean IAC_inserted = FALSE;
   boolean NUL_inserted = FALSE;
   boolean was_CR = FALSE;

   printmsg(11, "writetcp: len=%d", len);
   if (IP_port == IPPORT_TELNET) {
      /* move data to network buffer */
      /* perform IAC doubling and CR with NUL suffixing */
      /* may overflow network buffer on long writes */
      for (count=0, i=0; i < len; i++) {
	 if (IAC_pending) {
            printmsg(11,"writetcp: pending IAC inserted");
	    netbuf[count++] = IAC;
	    IAC_pending = FALSE;
	    IAC_inserted = TRUE;
	 }
	 if (NUL_pending) {
	    printmsg(11,"writetcp: pending NUL inserted");
	    netbuf[count++] = NUL;
	    NUL_pending = FALSE;
            NUL_inserted = TRUE;
         }
	 netbuf[count++] = buffer[i];
         if (buffer[i] == IAC) {
            printmsg(11,"writetcp: IAC doubled");
	    doubled = TRUE;
	    netbuf[count++] = buffer[i];
         }
         if (buffer[i] == CR) {
            printmsg(11,"writetcp: CR suffixed with NUL");
	    doubled = TRUE;
	    netbuf[count++] = NUL;
         }
      }
   }
   else
	count = len;

   sock_flushnext(netso);
   rc = sock_fastwrite(netso, (byte *)
	   (IP_port == IPPORT_TELNET ? netbuf : buffer), count);

   if (!tcp_tick(netso))
	goto sock_err;

   if (rc > 0) {
      /* if IAC doubling took place compute byte count */
      if (doubled && (IP_port == IPPORT_TELNET)) {
         for (count=0, i=0; i < rc; i++) {
	    if (was_CR && (netbuf[i] == NUL)) {
	       count++;
               NUL_pending = FALSE;
            }
            was_CR = FALSE;
	    if (netbuf[i] == CR) {
               count++;
	       NUL_pending = TRUE;
	       was_CR = TRUE;
	    } else if (netbuf[i] == IAC) {
	       count++;
               IAC_pending = !IAC_pending;
	    }
	 }
	 if (NUL_pending) count++;
	 if (IAC_inserted) {
	    IAC_pending = !IAC_pending;
            count--;
	 }
         if (IAC_pending)  count--;
         if (NUL_inserted) count--;
	 rc -= count/2;
      }
   }
   printmsg(11,"writetcp: %d bytes written reported",rc);
   return rc;

sock_err: /* go there from sock_tick() macro */
   printmsg(0, "writetcp: Connection closed");
   tcp_break = TRUE; /* indicate connection failed */
   return 0;
}

static void tcpfini(boolean verb)
{
   int status;

   /* see if a socket is even open */
   if (netso != NULL) {
      if (verb)
      /* let user know we're trying to close */
	      printmsg(1,"tcpfini: closing PACKET DRIVER connection");

      /* close the socket */
      sock_close(netso);
      sock_wait_closed(netso,sock_delay,NULL,&status);
sock_err:
      netso = NULL;
   }
}

/*
   openline - open the serial port for I/O
*/

int openline(char *name, char *baud)
{
	int   value;

	if (port_active)              /* Was the port already active?     ahd   */
		closeline();          /* Yes --> Shutdown it before open  ahd   */
	strcpy(M_device, "NONE");
	device=strdup(name);
	assert(!port_active);         /* Don't open the port if active!   ahd   */

	strupr(name);

	if (!strcmp(name,"PKTDRV")) {
		if (debuglevel >= 6)
			dbug_init();
		sock_init();
		if ((netaddr = resolve(baud)) == 0) {
			printerr("openline","unknown or illegal hostname");
			return -1;
		}
		if (tcpinit(netaddr) == -1)
			return -1;
		i_head = i_tail = 0; /* clear pending buffer */
		fs_isize = fs_osize = BUFFERSIZE;
		fs_direct = TRUE;
		fs_baud = 115200;
		fossil = FALSE;
		if (IP_port == IPPORT_TELNET) {
			netbuf = malloc(fs_isize * 2);
			checkref(netbuf);
		}
		ibuf = malloc(fs_isize * 2);
		checkref(ibuf);
		wattcp    = TRUE;
		tcp_break = FALSE;
	}
	else {
		sscanf(name, "COM%d", &value);
		if (value < 1 || value > 8)
			return -1;
		select_port(value);
		if (!install_com())
			return -1;
		open_com(atol(baud), (char)(Direct?'D':'M'), 'N',
			 STOPBIT, 'D');
	}

	if (log_stream == NULL && Makelog) {
		if ((log_stream = FOPEN(LINELOG, "a", BINARY)) != NULL)
			printmsg(15, "logging serial line data to %s", LINELOG);
		else
			printerr ("openline", LINELOG);
	}

	if (log_stream != NULL)
		fprintf(log_stream, "\r\n<<< Open line %s, %s baud, at %s >>\r\n", name, baud, arpadate());

	port_active = TRUE;     /* record status for error handler */

	strcpy(M_device, name);

	return 0;
} /*openline*/

static void reopen_com(boolean flow)
{
	if (wattcp) return;

	open_com(fs_baud, (Direct?'D':'M'), 'N', STOPBIT, (flow ?'E':'D'));
}

/*
   sread - read from the serial port
*/

/* Non-blocking read essential to "g" protocol.
   See "dcpgpkt.c" for description.
   This all changes in a multi-tasking system.  Requests for
   I/O should get queued and an event flag given.  Then the
   requesting process (e.g. gmachine()) waits for the event
   flag to fire processing either a read or a write.
   Could be implemented on VAX/VMS or DG but not MS-DOS. */

int sread(register char *buffer, int wanted, int timeout)
{
	struct timeb start;
	long tickout;
	int	i, r_pending, c;
	int oldpending;

	if ( wanted > fs_isize ) {
		printmsg(0,"sread: Receive buffer too small; buffer size %d, "
			    "needed %d",
		       fs_isize,wanted);
		panic();
	}

	if (log_stream != NULL && logmode != READING) {
		fputs("\r\nRead:", log_stream);
		logmode = READING;
	}

	printmsg(20, "sread: wanted=%d, timeout=%d", wanted, timeout);

	oldpending = -1;
	tickout = 0;

	for ( ; ; ) {
		if (wattcp) {
			if (i_tail - i_head < wanted) {
				sdelay(0);	/* ugly hack */
				i_tail += readtcp(fs_isize, ibuf + i_tail);
				printmsg(20, "sread: pending=%d, wanted=%d",
						i_tail - i_head, wanted);
			}
			if ((r_pending = i_tail - i_head) >= wanted) {
				/* got enough in the buffer? */
				memcpy(buffer, ibuf + i_head, wanted);
				i_head += wanted;
				if (i_head >= i_tail)
					/* clear the pending buffer */
					i_head = i_tail = 0;
				else if (i_tail >= fs_isize) {
					/* shift the pending buffer */
					memmove(ibuf, ibuf + i_head, i_tail - i_head);
					i_tail -= i_head;
					i_head = 0;
				}
				if (log_stream != NULL) {
					for (i = 0; i < wanted; i++)
						putc(buffer[i], log_stream);
				}
				return r_pending;
			}
		} else {
			r_pending = (wanted > 1 ? r_count_pending() : r_1_pending());
			if (r_pending < 0) {
Lost:
				if (log_stream != NULL)
					fputs("<<LOST CARRIER R>>\r\n", log_stream);
				return S_LOST;
			}
			if (debuglevel >= 20)
				if (oldpending != r_pending) {
					printmsg(20, "sread: pending=%d, wanted=%d, timeout=%d",
						  r_pending, wanted, timeout);
					oldpending = r_pending;
				}
			if (r_pending >= wanted) {
				if (fossil) {
					if (read_block(wanted, buffer) < wanted)
						goto Lost;
				} else {
					for (i = 0; i < wanted; i++) {
						if ((c = receive_com()) < 0)
							goto Lost;
						buffer[i] = c;
					}
				}
				if (log_stream != NULL) {
					for (i = 0; i < wanted; i++)
						putc(buffer[i], log_stream);
				}
				return r_pending;
			}
		}
		if (wattcp && tcp_break) {
			if (log_stream != NULL)
			      fputs("<<SOCKET LOST CONNECTION>>\r\n", log_stream);
			return S_LOST;
		}
		if (timeout <= 0)
			goto tout;
		if (tickout == 0) {
			tickout = timeout * (long)1000;
			ftime(&start);
		}
		if (msecs_from(&start) > tickout) {
	tout:
			if (!wattcp && connected && !carrier(FALSE))
				goto Lost;
			if (log_stream != NULL)
				fputs("<<TIMEOUT R>>\r\n", log_stream);
			return S_TIMEOUT;
		}
		if (ddelay(0)) {
			need_timeout = TRUE;
			goto tout;
		}
	}
} /*sread*/


/*
   swrite - write to the serial port
*/
#define MAXSPIN 5

int swrite(char *data, int len)
{
	int j, s_free, s, spin, old_s_free;
	long limit;
	struct timeb start;
	extern boolean false(void);

	if (len < 0)
		return len;

	if (debuglevel >= 20)
		printmsg(20, "swrite: len=%d", len);

	if ( len > fs_osize ) {
		printmsg(0,"swrite: Transmit buffer overflow; buffer size %d, "
			    "needed %d",
		       fs_osize,len);
		panic();
	}

	if (log_stream != NULL && logmode != WRITING) {
		fputs("\r\nWrite:", log_stream);
		logmode = WRITING;
	}

	limit = 0;

	if (wattcp) {
		register i, k;

		for (i=len; i>0;) {
			need_out_flush = TRUE;
			if ((k = writetcp(i, data)) < i && (k < 0 || tcp_break)) {
				if (log_stream != NULL)
					fputs("<<SOCKET LOST CONNECTION>>\r\n", log_stream);
				return S_LOST;
			}
			if (log_stream != NULL) {
				for (j=0; j<k; j++)
					putc(data[j], log_stream);
			}
			if ((i -= k) <= 0)
				return len;
			data += k;
			if (limit == 0) {
				limit = MSGTIME * (long)1000;
				ftime(&start);
			}
			if (   msecs_from(&start) > limit
			    || sdelay(0)
			   ) {
				if (log_stream != NULL)
					fputs("<<TCP OWERFLOW W>>\r\n", log_stream);
				return S_TIMEOUT;
			}
		}
		return len;
	}

	spin = 0;
	for ( ; ;) {
		s_free = s_count_free();
		printmsg(20, "swrite: s_free %d", s_free);
		if (s_free < 0) {
Lost:   		if (log_stream != NULL)
				fputs("<<LOST CARRIER W>>\r\n", log_stream);
			return S_LOST;
		}
		if (   s_free >= len
		    && (   limit == 0 && spin == 0 /* Enter first time */
			|| s_free > fs_osize/2     /* More than half */
		       )
		   ) {
			need_out_flush = TRUE;
			if (!fossil) {
				for (j = 0; j < len; j++) {
					if (send_com(data[j]) < 0)
						goto Lost;
				}
			}
			else {
				if (len == 1) {
					(void) transmit_char(*data);
					if (connected && !NoCheckCarrier) {
						use_old_status = TRUE;
						s = carrier(FALSE);
						use_old_status = FALSE;
						if (!s)
							goto Lost;
					}
				}
				else {
					if (write_block(len, data) < len)
						goto Lost;
				}
			}
			if (log_stream != NULL) {
				for (j = 0; j < len; j++)
					putc( data[j] , log_stream);
			}
			return len;
		} else {	/* No space, wait some time */
			if (limit == 0) {
				int needed;

				/* Minimize thrashing by requiring
				   big chunks */
				needed = max(fs_osize / 2, len - s_free);
				/* Compute time in milliseconds
				   assuming 10 bits per byte           */
				limit = (needed * (long)10000) / 1200 + 1;
				old_s_free = s_free;
				ftime(&start);
			}
			if (   msecs_from(&start) > limit
			    || sdelay(0)
			   ) {
				if (connected) {
					if (!NoCheckCarrier && !carrier(FALSE))
						goto Lost;
					if (spin < MAXSPIN) {
						if (   old_s_free == s_free  /* No progress */
						    || msecs_from(&start) <= limit /* Forced timeout */
						   ) {
							spin++;
							printmsg(4, "swrite: try to release flow control");
							flowcontrol(fs_xon);
							if (log_stream != NULL)
								fputs("<<FLOW W>>\r\n", log_stream);
						}
						limit = 0;
						continue;
					}
				}
				if (log_stream != NULL)
					fputs("<<OVERFLOW W>>\r\n", log_stream);
				printmsg(0, "swrite: port write buffer overflow, purge buffer contents");
				return S_TIMEOUT;
			}
		}
	}
} /*swrite*/


/*
   ssendbrk - send a break signal out the serial port
*/

void ssendbrk(int duration)
{
   if (wattcp) return;

   printmsg(12, "ssendbrk: %d", duration);

   break_com(duration);

} /*ssendbrk*/


/*
   closeline - close the serial port down
*/

void closeline(void)
{
	static boolean recurse = FALSE;
	boolean level0 = !recurse;

	if (!port_active)
		return;
	strcpy(M_device, "NONE");
	recurse = TRUE;

	if (level0) {
		if (connected && !wattcp)
			do_modstat = TRUE;
		set_connected(FALSE);
		if (!wattcp)
			hangup();
	}
	recurse = FALSE;

	port_active = FALSE; /* flag port closed for error handler  */

	if (wattcp) {
		wattcp = FALSE;
		tcpfini(TRUE);
		if (IP_port == IPPORT_TELNET)
			free(netbuf);
		free(ibuf);
	}
	else {
		int far *stats;
		extern boolean com_saved;

		close_com();
		if (!fossil) {
			if (com_saved) {
				restore_com();
				com_saved = FALSE;
			}
		}
		if (!fossil) {
			stats = com_errors();
			printmsg(10, "Receive buffer overflows:  %-4d",stats[COM_EROVFLOW]);
			printmsg(10, "Transmit buffer overflows: %-4d",stats[COM_ETOVFLOW]);
			printmsg(10, "Receive overruns: %-4d", stats[COM_EOVRUN]);
			printmsg(10, "Break characters: %-4d", stats[COM_EBREAK]);
			printmsg(10, "Framing errors:   %-4d", stats[COM_EFRAME]);
			printmsg(10, "Parity errors:    %-4d", stats[COM_EPARITY]);
			printmsg(10, "Transmit errors:  %-4d", stats[COM_EXMIT]);
			printmsg(10, "DSR errors:       %-4d", stats[COM_EDSR]);
			printmsg(10, "CTS errors:       %-4d", stats[COM_ECTS]);
		}
	}
	printmsg(3,"closeline: done.");
	if (log_stream != NULL) {  /* close serial line log file */
		fflush(log_stream);
		real_flush(fileno(log_stream));
	}
} /*closeline*/

/*    Hangup the telephone by dropping DTR.  Works with HAYES and many  */
/*    compatibles.  - 14 May 89 Drew Derbyshire                         */
void (*p_hangup)(void);

void hangup( void )
{
	if (wattcp) return;

	if ( p_hangup )
		(*p_hangup)();
	else {
		dtr_off();              /* Hang the phone up                         */
		ssleep(DTR_WAIT);
		dtr_on();               /* Bring the modem back on-line              */
		ssleep(1);
	}
	r_purge();
	if (!do_modstat) return;
	modstat();
	do_modstat = FALSE;
}

/*
   S I O S p e e d

   Re-specify the speed of an opened serial port

   Dropped the DTR off/on calls because this makes a Hayes drop the
   line if configured properly, and we don't want the modem to drop
   the phone on the floor if we are performing autobaud.

   (Configured properly = standard method of making a Hayes hang up
   the telephone, especially when you can't get it into command state
   because it is at the wrong speed or whatever.)
															ahd
 */


void SIOSpeed(long baud)
{
	if (wattcp) return;

	fs_baud = baud;
	printmsg(4, "SIOSpeed: set to %ld baud", fs_baud);
	reopen_com(fs_xon);
} /*SIOSpeed*/

/*--------------------------------------------------------------------*/
/*    f l o w c o n t r o l                                           */
/*                                                                    */
/*    Enable/Disable in band (XON/XOFF) flow control                  */
/*--------------------------------------------------------------------*/
void flowcontrol( boolean flow )
{
	if (wattcp) return;

	printmsg(10,"flowcontrol: set flow control %s", flow ? "XON/XOFF" : "CTS/RTS");
	if (fossil)
		setflow(flow);
	else
		reopen_com(flow ? 'E' : 'D');
} /*flowcontrol*/

void
r_purge(void)
{
	if (log_stream != NULL)
		fputs("<<PURGE INPUT>>\r\n", log_stream);

	if (wattcp) {
#if 0
		/* purge input */
		while (sock_dataready(netso))
			(void)sock_getc(netso);
#endif
		i_head = i_tail = 0; /* clear the pending buffer */
	}
	else
		s_r_purge();
}

int
w_flush(void)
{
	if (!need_out_flush) {
		if (!wattcp && connected && !NoCheckCarrier && !carrier(FALSE))
			return -1;
		return fs_osize;
	}
/*
	if (log_stream != NULL)
		fputs("<<FLUSH OUTPUT>>\r\n", log_stream);
*/
	need_out_flush = FALSE;
	if (wattcp) {
		sock_flush(netso);
		return fs_osize;
	}
	return s_w_flush();
}

void
w_purge(void)
{
	if (wattcp) return;
	if (log_stream != NULL)
		fputs("<<PURGE OUTPUT>>\r\n", log_stream);
	s_w_purge();
}

void
close_log_stream(void)
{
	if (log_stream != NULL) {
		fclose(log_stream);
		log_stream = NULL;
	}
}

