/*
   For best results in visual layout while viewing this file, set
   tab stops to every 4 columns.
*/

/*
   ibmpc/ulib.c

   DCP system-dependent library

   Services provided by ulib.c:

   - serial I/O

   Updated:

	  14May89  - Added hangup() procedure                               ahd
	  21Jan90  - Replaced code for rnews() from Wolfgang Tremmel
                 <tremmel@garf.ira.uka.de> to correct failure to
                 properly read compressed news.                             ahd
	  6 Sep 90 - Change logging of line data to printable               ahd
	  8 Sep 90 - Split ulib.c into dcplib.c and ulib.c                  ahd
*/

#include <assert.h>
#include <dos.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <process.h>
#include <ctype.h>
#include <conio.h>
#ifndef __EMX__
#include <direct.h>
#else
#include <sys/types.h>
#include <dirent.h>
#endif

#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSQUEUES
#define INCL_DOSERRORS
#define INCL_KBD
#include <os2.h>

#include <utils.h>
#include <netdb.h>
#include <sys/socket.h>
#include <nerrno.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "os2conio.h"
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

currentfile();

boolean port_active = FALSE;  /* TRUE = port handler active  */
boolean need_timeout = FALSE;
static boolean vmodem = FALSE;
static boolean fs_pipe = FALSE;
static boolean need_out_flush = FALSE;
static boolean do_modstat = FALSE;
extern fs_isize, fs_osize;
extern boolean fs_direct;
extern long fs_baud;
extern boolean use_old_status, fs_xon, NoCheckCarrier;

extern HEV kbdsem;
extern int kbd_tid;

int createpipe(int * in, int * out);

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
extern int  loadtcp(void);

static int gotit;
boolean wattcp = FALSE;
boolean tcp_break = FALSE;

#define BUFFERSIZE 5120

static char *netbuf = NULL;	/* TELNET buffer */

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
  vmodem = FALSE;
  if      (!strcmp(portname,"TELNET")) IP_port = IPPORT_TELNET;
  else if (!strcmp(portname, "UUCP" )) IP_port = IPPORT_UUCP;
  else if (!strcmp(portname, "VMODEM" )) {
    /* ugly hack for buggy VMODEM :-E */
    IP_port = IPPORT_TELNET;
    vmodem = TRUE;
  } else {
     IP_port = 0;
     return FALSE;
  }
  return TRUE;
}

static int tcpinit(long destipaddr)
{
   struct sockaddr_in sin;
   struct timeval timeout;

   /* connect it to the other end */
   printmsg(1, "tcpinit: connecting to %s via TCP/IP",rmtname);

   bcopy(&destipaddr,&sin.sin_addr.s_addr,sizeof(sin.sin_addr.s_addr));
   sin.sin_port=htons((unsigned)IP_port);
   sin.sin_family=AF_INET;
   if((fs_port=socket(AF_INET,SOCK_STREAM,0))==-1)
   { printmsg(-1,"Error create socket, errno %d", sock_errno());
     return -1;
   }
   printmsg(-1,"tcpinit: Waiting for connection established...");
   if(connect(fs_port,(struct sockaddr*)&sin,sizeof(sin))==-1)
   { printmsg(-1,"Error connect socket, errno %d", sock_errno());
     soclose(fs_port);
     return -1;
   }
   timeout.tv_sec=0;
   timeout.tv_usec=1000;
   setsockopt(fs_port, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
   
/* printmsg(1, "tcp_opened: a = %08lx, p = %d\n", destipaddr, IP_port); */
   return 0;

}

static int readtcp(int len, char buffer[])
{
   int  rc;
   char command_buf[3];
   ULONG actual;

repreadtcp:
   if (fs_pipe) {
	rc = DosRead(fileno(stdin), buffer, len, &actual);
	if (rc >= 0) rc = actual;
   } else { /* socket */
	rc = recv(fs_port, IP_port == IPPORT_TELNET ? netbuf : buffer, len, 0);
	if (rc == 0) {
		printmsg(0, "Connection closed by foreign host");
		tcp_break = TRUE; /* indicate connection failed */
		rc = -1;
	}
	else if (rc < 0) {
		printmsg(1, "readtcp: recv returns %d", rc);
		rc = sock_errno();
		printmsg(1, "readtcp: sock_errno %d", rc);
		if (rc == SOCEWOULDBLOCK) /* timeout */
			return 0;
		tcp_break = TRUE; /* indicate connection failed */
		return -1;
	}
   }
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
		  if ((netbuf[i] == CR) && (!vmodem)) t_state = T_CR;
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
		  ret = send(fs_port,command_buf,3,0);
		  if (ret <= 0)
			return ret;
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
         if (rc == 0) goto repreadtcp;
      } /* end of TELNET data processing */
   }
   return rc;
}

static int writetcp(int len, char buffer[])
{
   int rc;
   int i, count;
   boolean doubled = FALSE;
   boolean IAC_inserted = FALSE;
   boolean NUL_inserted = FALSE;
   boolean was_CR = FALSE;

   printmsg(11, "writetcp: len=%d", len);
repwritetcp:
   if (IP_port == IPPORT_TELNET) {
      /* move data to network buffer */
      /* perform IAC doubling */
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
         if ((buffer[i] == CR) && (!vmodem)) {
            printmsg(11,"writetcp: CR suffixed with NUL");
	    doubled = TRUE;
	    netbuf[count++] = NUL;
         }
      }
   }
   else
	count = len;

   if (fs_pipe) {
	rc=DosWrite(fileno(stdout), buffer, count, (PULONG)&i);
	if (rc>=0) rc = i;
   } else
	rc = send(fs_port, IP_port == IPPORT_TELNET ? netbuf : buffer, count, 0);

   if (rc > 0) {
      /* if IAC doubling took place compute byte count */
      if (doubled && (IP_port == IPPORT_TELNET)) {
         for (count=0, i=0; i < rc; i++) {
	    if (was_CR && (netbuf[i] == NUL)) {
	       count++;
               NUL_pending = FALSE;
            }
            was_CR = FALSE;
	    if ((netbuf[i] == CR) && (!vmodem)) {
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
         if (rc == 0)
           goto repwritetcp;
      }
   }
   else if (rc < 0) {
	printmsg(0, "writetcp: Connection closed, send returns %d, errno %d (socket %d)", rc, sock_errno(),fs_port);
	tcp_break = TRUE; /* indicate connection failed */
	return rc;
   }
   printmsg(11,"writetcp: %d bytes written reported",rc);
   return rc;
}

/*
   openline - open the serial port for I/O
*/

int openline(char *name, char *baud)
{
	int   value;

        printmsg(7,"openline: started");
        printmsg(7,"name=%s, baud=%s", name, baud);
        printmsg(7,"H_Flag=%s, fs_port=%d", H_Flag ? "TRUE" : "FALSE", fs_port);
        printmsg(7,"port_active=%s", port_active ? "TRUE" : "FALSE");
	if (port_active)              /* Was the port already active?     ahd   */
		closeline();          /* Yes --> Shutdown it before open  ahd   */

	strcpy(M_device, "NONE");
	device=strdup(name);
	assert(!port_active);         /* Don't open the port if active!   ahd   */

	strupr(name);

	if (H_Flag && (fs_port == 0)) {
		ULONG htype, attr;
		device = "stdin";
                printmsg(7,"DosQueryHType starting");
		DosQueryHType(value, &htype, &attr);
		printmsg(4,"running as shell, device type is %d, attrib %d",
		         htype, attr);
		if ((htype==1) && ((attr==0x8083) || (attr==0x8084))) {
			printmsg(-1, "working via stdin/stdout not supported");
			return -1;
		}

		if ((char)htype == 2) {	/* pipe */
			fs_pipe = TRUE;
			IP_port = IPPORT_UUCP;
		}
	}

	if ((!strcmp(name,"PKTDRV")) || (!strcmp(name,"TCP")) || fs_pipe) {
		struct hostent* remote;
		long defaddr;

		printmsg(7, "openline: running via TCP");
		if (loadtcp())
			exit(9);
		if (!fs_pipe) {
			sock_init();
			if (!H_Flag) {
				if ((!isdigit(baud[0])) ||
				    ((defaddr=inet_addr(baud))==INADDR_NONE)) {
					if((remote=gethostbyname(baud))==NULL) {
						printmsg(0, "openline: unknown or illegal hostname %s", baud);
						return -1;
					}
					if (remote->h_addr_list[0]==NULL) {
						printmsg(0, "openline: can gethostbyname for %s", baud);
						return -1;
					}
					defaddr=*(long *)(remote->h_addr_list[0]);
				}
				if (tcpinit(defaddr) == -1) {
					sleep(3);
					return -1;
				}
			}
		}
		fs_isize = fs_osize = BUFFERSIZE;
		fs_direct = TRUE;
		fs_baud = 115200;
		wattcp    = TRUE;
		tcp_break = FALSE;
		if (IP_port == IPPORT_TELNET) {
			netbuf = malloc(fs_isize * 2);
			checkref(netbuf);
		}
	} else {
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
    int i, r;
    int oldpending;
    ULONG semcount;
    int now;

	if (timeout < 0) return S_TIMEOUT;
	if (timeout == 0)
		if (wattcp) {
			for(i=0; i<wanted && select((int *)&fs_port, 1, 0, 0, 0)==1; i++)
				if (readtcp(1, buffer + i) < 0)
					return -1;
			return (i == wanted) ? i : S_TIMEOUT;
		} else {
			i = s_count_pending();
			if (i < wanted) {
				read_block(i, buffer);
				return S_TIMEOUT;
			}
		}
	if (wanted == 0)
		return wanted;
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

    gotit=0;
    DosResetEventSem(kbdsem,&semcount);
    now = time(NULL);
    i = 0;
    while (now + timeout >= time(NULL)) {
      if (wattcp)
        r=readtcp(wanted-i, buffer+i);
      else
        r=read_block(wanted-i, buffer+i);
      printmsg(20, "sread: %s returns %d, wanted %d",
               wattcp ? "readtcp" : "readblock", r, wanted-i);
      if (r<0) {
        gotit=-1;
        break;
      }
      if ((i+=r) == wanted) {
        gotit=1;
        break;
      }
      if (DosWaitEventSem(kbdsem, SEM_IMMEDIATE_RETURN) == 0)
        break;
      if (!wattcp)
        if (connected && !carrier(FALSE)) {
          printmsg(15, "sread: carrier lost");
          break;
        }
    }
    if (gotit == 0) {
      need_timeout = TRUE;
      printmsg(12, "sread: Escape pressed");
    }
    i=DosResetEventSem(kbdsem,&semcount);
    if (gotit<=0)
    {   /* TimeOut || Esc pressed || carrier lost */
	if (wattcp) {
		if (gotit<0) {
			printmsg(20, "sread: connection lost");
			if (log_stream != NULL)
				fputs("<<LOST CONNECTION R>>\r\n", log_stream);
			return S_LOST;
		}
        }
	else
		if (connected && !carrier(FALSE)) {
			printmsg(15, "sread: carrier lost");
			if (log_stream != NULL)
				fputs("<<LOST CARRIER R>>\r\n", log_stream);
			return S_LOST;
        }
	printmsg(20, "sread: timeout");
	if (log_stream != NULL)
		fputs("<<TIMEOUT R>>\r\n", log_stream);
	return S_TIMEOUT;
    }
    if (log_stream != NULL) {
	for (i = 0; i < wanted; i++)
		putc(buffer[i], log_stream);
    }
    printmsg(20, "sread: return %d", wanted);
    return wanted;
} /*sread*/


/*
   swrite - write to the serial port
*/
int swrite(char *data, int len)
{
	int j;

	if (log_stream != NULL && logmode != WRITING) {
		fputs("\r\nWrite:", log_stream);
		logmode = WRITING;
	}

	if (wattcp) {
		int r=0;
		for (;;) {
			j=writetcp(len-r, data+r);
			if (j<0) {
				printmsg(1, "swrite: writetcp returns %d", j);
				if (log_stream != NULL)
					fputs("<<LOST CONNECTION W>>\r\n", log_stream);
				return S_LOST;
			}
			printmsg(20, "send returns %d", j);
			if ((r+=j)==len)
				break;
		}
	}
	else if (write_block(len, data)<len)
	{
		if (log_stream != NULL)
			fputs("<<LOST CARRIER W>>\r\n", log_stream);
		return S_LOST;
	}
	if (log_stream != NULL) {
		for (j = 0; j < len; j++)
			putc( data[j] , log_stream);
	}
	return len;
}

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

	printmsg(7,"closeline start; wattcp=%s", wattcp ? "TRUE" : "FALSE");

	if (level0) {
		if (connected && !wattcp)
			do_modstat = TRUE;
		set_connected(FALSE);
		if (!wattcp)
			hangup();
	}
	recurse = FALSE;

	port_active = FALSE; /* flag port closed for error handler  */

	{
		extern boolean com_saved;

		DosPostEventSem(kbdsem); /* terminate r/w */
		if (wattcp)
		{
			if (!fs_pipe) {
				shutdown(fs_port,2);
				soclose(fs_port);
				if (IP_port == IPPORT_TELNET)
					free(netbuf);
			}
		} else {
			close_com();
			if (com_saved) {
				restore_com();
				com_saved = FALSE;
			}
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

	printmsg(7, "hangup");
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
	setflow(flow);
} /*flowcontrol*/

void
r_purge(void)
{
	if (wattcp) {
		int i, data;
                char c;
		if (fs_pipe) {
			/* ??? */
		} else {
			ioctl(fs_port, FIONREAD, (char *)&data, sizeof(data));
			for (i=0; i<data; i++)
				recv(fs_port, &c, 1, 0);
		}
		return;
	}
	if (log_stream != NULL)
		fputs("<<PURGE INPUT>>\r\n", log_stream);

	s_r_purge();
}

int
w_flush(void)
{
	if (wattcp) return 1;
	if (!need_out_flush) {
		if (connected && !NoCheckCarrier && !carrier(FALSE))
			return -1;
		return fs_osize;
	}
	need_out_flush = FALSE;
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
