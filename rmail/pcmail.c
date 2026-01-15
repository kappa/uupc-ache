/*
   pcmail.c

   copyright (C) 1987 Stuart Lynne
   Changes copyright (C) 1989 Andrew H. Derbyshure
   Changes Copyright (C) 1991-97 by Andrey A. Chernov, Moscow, Russia.
   Changes Copyright (C) 1997 by Pavel Gulchouck, Kiev, Ukraine.

*/
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <sys\timeb.h>
#include <dos.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <process.h>
#include <errno.h>

#include "lib.h"
#include "arpadate.h"
#include "hlib.h"
#include "hostable.h"
#include "import.h"
#include "address.h"
#include "timestmp.h"											  /* ahd */
#include "getopt.h"
#include "getseq.h"
#include "pushpop.h"
#include "pcmail.h"
#include "tio.h"

#ifdef __OS2__
void InitKbd(void);
#endif

currentfile();

int remote_address(char *address);
int qstripcmpi(char *s1, char *s2);
extern boolean ishead(char *);

#define  MOPLEN 	 9 	 /* Length of formatted header lines */

int sendone(char *address, int, int);
static char *uniqueid(void);
static void receive_by_me(TFILE *file, boolean remote);

TFILE * tf;
static TFILE *mfile;
int hflag = 0, nodelen, domlen;
boolean ignsigs = TRUE;
extern boolean doreceipt, returnsenderbody;
extern int null(void);
boolean verbose = FALSE, metoo = FALSE;
boolean take_from_body = FALSE;
extern char compiled[];
static unsigned char buf[LINESIZE];
static unsigned char sbuf[80];

char *fromnode = NULL;	  /* For UUCP From line 	ahd   */
char *fromuser = NULL;	  /* For UUCP From line	      ahd   */
FILE *input_source = stdin;
char *input_name = "stdin";

extern int debuglevel;

boolean     fromunix = FALSE;
boolean     remove_input = FALSE;
char *uu_machine = NULL, *uu_user = NULL;
extern boolean 	filelist;
boolean bit8_body, bit8_header;
boolean GotDate, GotMID;
char datestr[80];
static int targc;
static char *targv[MAXARGS];

char mastername[] = "Postmaster";

boolean parse_args(int argc, char *argv[]) {
	int option;

	if (argc <= 1) {
Usage:
		fprintf(stderr, "Usage:\trmail [-ilvmVuZ] [-r <remote>] [-R <from>] [-f|F <file>]\n\
\t[-h <hop_count>] [-X|x <debug_level>] [-l]|[-t|<address> ...]\n");
		return FALSE;
	}
	while((option = getopt(argc, argv, "r:h:vilmVtf:F:R:x:X:uZ")) != EOF)
		switch (option) {

		case 'h':
			hflag = atoi(optarg);
			break;

		case 'v':
			verbose = TRUE;
			break;

		case 'r':
			fromnode = optarg;
			uu_machine = optarg;
			break;

		case 'R':
			fromuser = optarg;
			uu_user = optarg;
			break;

		case 'i':	/* Interactive mode */
			ignsigs = FALSE;
			break;

		case 'm':
			metoo = TRUE;
			break;

		case 'X':
			logecho = TRUE;
			visual_output = FALSE;
			/* fall */
		case 'x':
			debuglevel = atoi(optarg);
			break;

		case 'u':
			fromunix = TRUE;
			break;

		case 'l':
			filelist = TRUE;
			break;

		case 't':
			take_from_body = TRUE;
			break;

		case 'Z':
			if (!logecho)
				visual_output = TRUE;
			break;

		case 'F':
			remove_input = TRUE;
		case 'f':
			if (input_source != stdin) {
				fprintf(stderr, "Only one -f can be present\n");
				return FALSE;
			}
			input_name = strdup(optarg);
			checkref(input_name);
			if ((input_source = fopen(input_name, "r")) == NULL) {
				perror(input_name);
				fprintf(stderr, "Can't open input file\n");
				return FALSE;
			}
			break;

		case '?':
			goto Usage;
		}

	/* No addresses */
	if (optind >= argc && !filelist && !take_from_body)
		goto Usage;

	return TRUE;
}

static void receive_by_me(TFILE *tempfile, boolean remote)
{
	char *p = strchr(compilev, ' ');

	if (p != NULL)
		*p = '\0';

	  /*    Also create an RFC-822 Received: header               */
	  tfprintf(tempfile,"%-*s by %s (%s %s, %2.2s%3.3s%2.2s)%s\n%-*s id AA%05u; %s\n",
		MOPLEN, "Received:", domain, compilep, compilev,
			   &compiled[4], &compiled[0], &compiled[9],
			   remote ? " with UUCP" : "",
			   MOPLEN, " ", getpid(), datestr);
	if (p != NULL)
		*p = ' ';
}

static void getaddr(char * buf, char * addrlist, int addrlen)
{
	char *p, *s, c;
	int i;

	p = strchr(buf, ':');
	if (p == NULL) return;
	for (p++;isspace(*p);p++);
	if (*p=='\0') return;
	p = strdup(p);
	checkref(p);
	for (s=p, i=0; *s;) {
		c = *s;
		if (c == '(')
			i++;
		if (i)
			strcpy(s, s+1);
		else
			s++;
		if ((c == ')') && i) i--;
	}
	
	if (strchr(p, '<') == NULL)
		for (; *p; p=s) {
			while (isspace(*p) || (*p == ',')) p++;
 			if (*p == '\0') break;
 			for (s=p; *s && !isspace(*s) && (*s != ','); s++);
			if (strlen(addrlist)+(int)(s-p)+3 >= addrlen) continue;
			if (addrlist[0] != '\0')
				strcat(addrlist, ", ");
			addrlist[strlen(addrlist)+(int)(s-p)] = '\0';
			strncpy(addrlist+strlen(addrlist), p, (int)(s-p));
		}
	else
		for (p=strchr(p,'<'); p && *p; p=strchr(s, '<')) {
			p++;
			while (isspace(*p)) p++;
			if (*p == '\0') break;
			for (s=p; *s && !isspace(*s) && (*s != '>'); s++);
			if (strlen(addrlist)+(int)(s-p)+3 >= addrlen) continue;
			if (addrlist[0] != '\0')
				strcat(addrlist, ", ");
			addrlist[strlen(addrlist)+(int)(s-p)] = '\0';
			strncpy(addrlist+strlen(addrlist), p, (int)(s-p));
		}
}

int lmail(int argc, char *argv[])
{
   int argcount, errcnt, r, nosendcnt, res;
   char **argvec;
   static char remotes[LISTSIZ];
   static char rawlist[LISTSIZ];
   static char badlist[LISTSIZ];
   char currentpath[MAXADDR];
   static char retaddr[LISTSIZ];
   static char toaddr[LISTSIZ];
   register unsigned char *p, *q, *s;
   boolean firstline, messbody, receipt, WasLocal;
   boolean was_cr;
   char * boundary;

   currentpath[0] = '\0';
   nodelen = strlen(nodename);
   domlen = strlen(domain);

   if (debuglevel > 5) {
	  printmsg(5, "lmail: argc %d ", argc);
	  argcount = argc;
	  argvec = argv;
	  while (argcount--)
		 printmsg(5, " \"%s\"", *argvec++);
   }

   /*
	  Copy stdin to tf, adding an empty line if needed to separate
	  the body of a new message from the header, which we will generate
	  below.
	*/
   firstline = TRUE;
   messbody = FALSE;
   receipt = FALSE;
   *retaddr = '\0';
   GotDate = GotMID = FALSE;
   strcpy(datestr, arpadate());
   buf[0] = '\0';
   was_cr = TRUE;

   tf = tfopen();
   if (tf == NULL) {
	printmsg(0, "lmail: can't open temp file");
	return(1);
   }

   receive_by_me(tf, fromunix);
   bit8_body = bit8_header = FALSE;

	while (!feof(input_source)) {
		if (fgets(sbuf, sizeof(sbuf), input_source) == NULL)
			sbuf[0] = '\0';
		else if ((buf[0] == '\0') || (buf[strlen(buf)-1]!='\n') ||
		    (isspace(*sbuf) && (*sbuf != '\n') && !messbody  && (*buf != '\n' || !was_cr))) {
			if (strlen(buf)+strlen(sbuf) < sizeof(buf)) {
				strcat(buf, sbuf);
				continue;
			}
		}
		if (!messbody) {
			for (s=buf; *s && (!isspace(*s)) && (*s!=':'); s++);
			if (was_cr && (*s != ':') &&
                            ((*buf=='\n') || (!firstline) || (!ishead(buf)))) {
				messbody = TRUE;
				if (!GotMID)
					tfprintf(tf, "Message-Id: <%s@%s>\n", uniqueid(), domain);
				if (!GotDate)
					tfprintf(tf, "Date: %s\n", datestr);

				if (*buf != '\n')
					tfputs("\n", tf);
			} else if (strnicmp(buf, "Received:", 9) == 0) {
				hflag++;
			} else if (strnicmp(buf, "Return-Receipt-To:", 18) == 0) {
				getaddr(buf, retaddr, sizeof(retaddr));
				if (retaddr[0]) receipt = TRUE;
			} else if (!GotDate && strnicmp(buf, "Date:", 5) == 0)
				GotDate = TRUE;
			else if (!GotMID && strnicmp(buf, "Message-Id:", 11) == 0)
				GotMID = TRUE;
			else if (take_from_body &&
				   (strnicmp(buf, "To:", 3) == 0 ||
				    strnicmp(buf, "Cc:", 3) == 0 ||
				    strnicmp(buf, "Bcc:", 4) == 0)
				  ) {
				getaddr(buf, toaddr, sizeof(toaddr));
			} else if (firstline && ishead(buf)) {
				/*
				 * Process old ugly From ... remote from ...
				 * uucp rmail header line.
				 */
				static unsigned char remotename[LINESIZE];
				int i;

				firstline = FALSE;
				/* skip From_ */
				p = buf + 5;
				SSKIP(p);
				STRAIL(p);
				/* Following piece of code contributed by Pavel Gulchouk */
				/* with small modifications from me */
				strcpy(remotename, p);
				if ((p = strstr(remotename," remote from ")) != NULL) {
					p += 13;
					SSKIP(p);
					fromnode = p;
					p = strpbrk(fromnode," \t\n\r");
					if (p != NULL) *p = '\0';
				}
				q = NULL;
				for (p = remotename; *p; p++) {
					if (*p == '"')
						q = (q == NULL) ? p : NULL;
					else if (q != NULL && *p == '\\' && p[1] == '"')
						p++;
					else if (q == NULL && isspace(*p)) {
						*p = '\0';
						break;
					}
				}
				if (q != NULL)
					*q = '\0';
				/*** convert node1!user@node2 -> node2!node1!user */
				if (*remotename != '<') /* prevent <@node2,@node1:user> */
					while ((p = strrchr(remotename, '@')) != NULL) {
						/* shift all */
						for (q = remotename + strlen(remotename);
						     q >= remotename; q--)
							q[1] = q[0];
						q[1]='\0';
						p+=2; /* now sets to "node2" */
						while (*p != '\0') {
						    /* shift bang part */
						    for (q = p-2; *q != '\0'; q--)
							q[1] = q[0];
						    q[1] = '\0';
						    *q = *p++;
						}
						q[1] = '!';
						*--p = '\0';
					}
				if (fromuser == NULL || !equali(remotename, "uucp"))
					fromuser = remotename;
				if (fromnode == NULL)
					fromnode = nodename;
				/* Strip all initial fromnode nodes */
				i = strlen(fromnode);
				if (equal(fromnode, domain)) {
					for (p = fromuser; (q = strchr(p, '!')) != NULL; p = q + 1) {
						if (   (p + nodelen == q && equaln(p, nodename, nodelen))
						    || (p + i == q && equaln(p, fromnode, i))
						   )
							fromuser = q + 1;
						else
							break;
					}
				} else if (equal(fromnode, nodename)) {
					for (p = fromuser; (q = strchr(p, '!')) != NULL; p = q + 1) {
						if (   (p + domlen == q && equaln(p, domain, domlen))
						    || (p + i == q && equaln(p, fromnode, i))
						   )
							fromuser = q + 1;
						else
							break;
					}
				} else {
					for (p = fromuser; (q = strchr(p, '!')) != NULL; p = q + 1) {
						if (p + i == q && equaln(p, fromnode, i))
							fromuser = q + 1;
						else
							break;
					}
				}
				firstline = FALSE;
				strcpy(buf, sbuf);
				continue;
			}
		}

		if (tfputs(buf, tf) == EOF) {
			printmsg(0, "Can't write to tempfile");
			tfclose(tf);
			tunlink(tf);
			return(1);
		}
		firstline = FALSE;
		was_cr = (buf[strlen(buf)-1] == '\n');
		for (s = buf; *s; s++)
			if (*s & 0x80)
				if (!messbody)
					bit8_header = TRUE;
				else
					bit8_body = TRUE;
		strcpy(buf, sbuf);
    } /* while */

	if (input_source != stdin)
		fclose(input_source);

#ifdef __OS2__
	InitKbd();
#endif

	if (fromuser == NULL)
		fromuser = mailbox;
	if (uu_user == NULL)
		uu_user = mailbox;
	if (!receipt)
		strcpy(retaddr, fromuser);
	if (fromnode == NULL)
		fromnode = nodename;
	if (uu_machine == NULL)
		uu_machine = nodename;

 /*
	  UUPC's mail depends on each message having a validly terminated
	  header.  If this mail was not terminated by a header, (common
	  for mail sent from VMS Internet sites), add terminator now.

	  Another option would be to add a check for a completely blank
	  line, that need not be of zero (0) length (the VMS mail has
	  one or two blanks), but this takes care of the basic problem.
  */
	if (!messbody)
		if (tfputs("\n", tf) == EOF) {
			printmsg(0, "lmail: temp file write error");
			tfclose(tf);
			tunlink(tf);
			return(1);
		}

   /* loop on args, copying to appropriate postbox, doing remote only once
	  remote checking is done empirically, could be better */

	if (take_from_body) {
		targc = 0;
		q = toaddr;
		if (*q)
			do {
				q = ExtractAddress(buf, q, FALSE);
				if (!*buf)
					continue;
				p = strdup(buf);
				checkref(p);
				targv[targc++] = p;
			} while (q != NULL);
		targv[targc] = NULL;
		argcount = targc + 1;
		argvec = targv - 1;
	} else {
		argcount = argc - optind + 1;
		argvec = argv + optind - 1;
	}

	*remotes = '\0';
	*rawlist = *badlist = '\0';
	WasLocal = FALSE;
	errcnt = nosendcnt = 0;
	if (argcount <= 1) {
		nosendcnt = 1;
		strcpy(badlist, "*Nothing*\n");
	}

	while (--argcount > 0) {
		char *address = *++argvec;

		/* Remove initial local nodes */
		for (p = address; (s = strchr(p, '!')) != NULL; p = s + 1) {
			if (   (p + nodelen == s && equaln(p, nodename, nodelen))
			    || (p + domlen == s && equalni(p, domain, domlen))
			   )
				address = s + 1;
			else
				break;
		}
		printmsg(5, "lmail: address argv[%d]=%s", argcount, address);

		if (metoo && equal(address, mailbox))
			continue;

		if ((res = remote_address(address)) == D_REMOTE) {
			int rlen;
			char hisnode[MAXADDR];
			char hispath[MAXADDR];
			char hisuser[MAXADDR];

			user_at_node(address,hispath,hisnode,hisuser);
			printmsg(5, "lmail: remote address via %s",hispath);

   /* If this mail is going via the same path as any previously      */
   /* queued remote mail and the address will fit in the current     */
   /* output buffer, add it to the current remote host request.      */
   /* If multiple addresses routed via the same path are separated   */
   /* by requests queued for a different (non-local) path, then      */
   /* at least three separate files will be created.  This can only  */
   /* be corrected by sorting the list in path order, which is not   */
   /* not currently done.					      */

			rlen = strlen(remotes);
			if (
				rlen > 0 &&
				(
				 rlen + 1 + strlen(address) > LISTSIZ ||
				 !equal(hispath,currentpath)
				)
			   ) {
				/* no, too bad, dump it then */
				if ((r = sendone(remotes + 1, res, hflag)) <= 0) {	 /* remote delivery */
					if (r < 0)
						errcnt++;
					nosendcnt++;
					strcat(badlist, remotes + 1);
					strcat(badlist, "\n");
				}
				*remotes = '\0';
			}

		/* add *arvgvec to list of remotes */
			strcat(remotes, " ");
			qstrcat(remotes, address);
			strcpy(currentpath, hispath);
		} else {	/* Local */
			char * qaddress = calloc(strlen(address)*2+3, 1);
			checkref(qaddress);
			qstrcat(qaddress, address);
			if ((r = sendone(qaddress, res, hflag)) > 0) {	/* local delivery */
                        	if (res != D_GATEWAY || r != 2) {
					WasLocal = TRUE;
					strcat(rawlist, address);
					strcat(rawlist, "\n");
                                }
			}	/* if */
			else {
				if (r < 0)
					errcnt++;
				nosendcnt++;
				strcat(badlist, address);
				strcat(badlist, "\n");
			}
			free(qaddress);
		 }
	}

   /* dump any remotes if necessary */
   if (strlen(remotes) > 1) {
	  if ((r = sendone(remotes + 1, D_REMOTE, hflag)) <= 0) {   /* remote delivery */
		if (r < 0)
			errcnt++;
		nosendcnt++;
		strcat(badlist, remotes + 1);
		strcat(badlist, "\n");
	  }
   }

   mfile = tf;

   if (   *retaddr
	   && (   (receipt && doreceipt && WasLocal)
	       || nosendcnt > 0
	      )
	  ) {
	   boolean hit = FALSE;

	   printmsg(5, "lmail: send %s results to <%s>",
			   nosendcnt > 0 ? "bad" : "good",
			   retaddr);

	   tf = tfopen();
	   if (tf == NULL) {
		printmsg(0, "Can't open temp file");
		return 1;
	   }
	   fromuser = "MAILER-DAEMON";
	   fromnode = nodename;
	   receive_by_me(tf, FALSE);
	   q = uniqueid();
	   tfprintf(tf, "From: Mail Delivery Subsystem <%s@%s>\n", fromuser, domain);
	   tfprintf(tf, "To: %s\n", retaddr);
	   tfprintf(tf, "Message-Id: <%s@%s>\n", q, domain);
	   tfprintf(tf, "Subject: %s",
			receipt ? "Returned mail: " : "Delivering Errors\n");
	   if (receipt)
			if (nosendcnt > 0)
				tfprintf(tf, "Delivering errors%s\n",
					    !returnsenderbody ? " Track" : "");
			else
				tfprintf(tf, "Return receipt\n");
	   tfprintf(tf, "Date: %s\n", arpadate());
	   tfprintf(tf, "MIME-Version: 1.0\n");
	   sprintf(buf, "%s/%s", q, domain);
	   boundary = strdup(buf);
	   checkref(boundary);
	   tfprintf(tf, "Content-Type: multipart/report; boundary=\"%s\"\n\n", boundary);
	   tfprintf(tf, "This is a MIME-encapsulated message\n\n");
	   tfprintf(tf, "--%s\n\n", boundary);
	   tfprintf(tf, "   ----- Transcript of session follows -----\n");
	   if (*rawlist && receipt) {
			char *start;
			int len;

			start = rawlist;
			tfprintf(tf, "Your message successfully delivered to:\n");
			do {
				len = strcspn(start, " \n");
				if (len > 0)
					start[len] = '\0';
				tfprintf(tf, "%s\n", start);
				start += len + 1;
			}
			while (len > 0 && *start);
	   }
	   if (*badlist && nosendcnt > 0) {
			for (p = badlist; *p; p++)
				if (*p == ' ')
					*p = '\n';
		   tfprintf(tf, "I can't deliver your message to:\n%s", badlist);
	   }
	   if (nosendcnt > 0 && returnsenderbody) {
		tfprintf(tf, "I notify %s about that\n", mastername);
		tfprintf(tf, "\n   ----- Original message follows -----\n\n");
	   } else {
		if (nosendcnt > 0)
			tfprintf(tf, "I forward your message to %s\n", mastername);
		tfprintf(tf, "\n   ----- Message header follows -----\n\n");
	   }
	   tfprintf(tf, "--%s\n", boundary);
	   tfprintf(tf, "Content-Type: ");
	   if ((!bit8_header || mime_header) && nosendcnt>0 && returnsenderbody)
		tfprintf(tf, "message/rfc822");
	   else {
		tfprintf(tf, "text/plain; charset=%s\n", fromunix ? extsetname : intsetname);
		tfprintf(tf, "Content-Transfer-Encoding: 8bit");
	   }
	   tfprintf(tf, "\n\n");

	   trewind(mfile);
	   was_cr = TRUE;
	   while (tfgets(buf, sizeof(buf), mfile) != NULL) {
		if ((nosendcnt == 0 || !returnsenderbody) && (*buf == '\n') && was_cr)
			break;
		tfputs(buf, tf);
		was_cr = (strchr(buf, '\n') != NULL);
	   }

	   if (nosendcnt == 0 || !returnsenderbody)
		   tfprintf(tf, "\n   ----- Message body suppressed -----\n\n");
	   tfprintf(tf, "--%s--\n", boundary);

	   q = retaddr;
	   hit = FALSE;
	   do {
		char *pbuf;

		q = ExtractAddress(buf, q, FALSE);
		for (p = pbuf = buf; (s = strchr(p, '!')) != NULL; p = s + 1) {
			if (   (p + nodelen == s && equaln(p, nodename, nodelen))
			    || (p + domlen == s && equalni(p, domain, domlen))
			   )
				pbuf = s + 1;
		}
		if (!*pbuf) {
			if (*buf) {
				hit = TRUE;
				nosendcnt++;
				strcat(badlist, buf);
				strcat(badlist, "\n");
			}
			continue;
		}
		p = malloc(strlen(pbuf)*2+3);
		checkref(p);
		strcpy(p, pbuf);
		r = remote_address(p);
		*p = '\0';
		qstrcat(p, pbuf);
		hit = TRUE;
		if ((r = sendone(p, r, hflag)) <= 0) {
			if (r < 0)
				errcnt++;
			nosendcnt++;
			strcat(badlist, p);
			strcat(badlist, "\n");
		}
		free(p);
	   } while (q != NULL);

	   if (!hit) {
		nosendcnt++;
		strcat(badlist, retaddr);
		strcat(badlist, "\n");
	   }

	   if (nosendcnt > 0) {
		printmsg(5, "lmail: send bad results to %s", mastername);

		tfclose(tf);
		tunlink(tf);

		tf = tfopen();
		if (tf == NULL) {
			printmsg(0, "Can't open temp file");
			return 1;
		}

		receive_by_me(tf, FALSE);
		q = uniqueid();
		tfprintf(tf, "From: Mail Delivery Subsystem <%s@%s>\n", fromuser, domain);
		tfprintf(tf, "To: %s@%s\n", mastername, domain);
		tfprintf(tf, "Message-Id: <%s@%s>\n", q, domain);
		tfprintf(tf, "Subject: Delivering Errors%s\n", returnsenderbody ? " Track" : "");
		tfprintf(tf, "Date: %s\n", arpadate());
		tfprintf(tf, "MIME-Version: 1.0\n");
		tfprintf(tf, "Content-Type: multipart/report; boundary=\"%s\"\n\n", boundary);
		tfprintf(tf, "This is a MIME-encapsulated message\n\n");
		tfprintf(tf, "--%s\n\n", boundary);
		tfprintf(tf, "   ----- Transcript of session follows -----\n");
		if (*badlist)
			tfprintf(tf, "I can't deliver message to:\n%s", badlist);
		if (returnsenderbody)
			tfprintf(tf, "\n   ----- Message header follows -----\n\n");
		else
			tfprintf(tf, "\n   ----- Original message follows -----\n\n");
		tfprintf(tf, "--%s\n", boundary);
		tfprintf(tf, "Content-Type: ");
		if (!bit8_header && !returnsenderbody)
			tfprintf(tf, "message/rfc822");
		else {
			tfprintf(tf, "text/plain; charset=%s\n", fromunix ? extsetname : intsetname);
			tfprintf(tf, "Content-Transfer-Encoding: 8bit");
		}
		tfprintf(tf, "\n\n");

		trewind(mfile);
		was_cr = TRUE;

		while (tfgets(buf, sizeof(buf), mfile) != NULL) {
			if (returnsenderbody && (*buf == '\n') && was_cr)
				break;
			tfputs(buf, tf);
			was_cr = (strchr(buf, '\n') != NULL);
		}

		if (returnsenderbody)
			tfprintf(tf, "\n   ----- Message body suppressed -----\n\n");
		tfprintf(tf, "--%s--\n", boundary);
		if ((r = sendone(mastername, D_LOCAL, 0)) <= 0) {
			if (r < 0)
				errcnt++;
			nosendcnt++;
		}
	   }
	   tfclose(tf);
	   tunlink(tf);
	}

   tfclose(mfile);
   tunlink(mfile);

   if ((errcnt == 0) && (input_source != stdin) && (remove_input))
	unlink(input_name);

   return !!errcnt;

} /*lmail*/

/* Determine if a given address is remote or local		  */
/* Strips address down to user id if the address is local.        */
/* Written 14 May 1989 by ahd                                     */

int remote_address(char *address)	/* address can be modified */
{
   char  hisnode[MAXADDR];
   char  hispath[MAXADDR];
   char  hisuser[MAXADDR];
   boolean result;
   char *s, *p;
   struct HostTable *hostp;

again:
   user_at_node(address,hispath,hisnode,hisuser);

   hostp = checkname( hispath );
   if ( (hostp != BADHOST) && (hostp->status.hstatus == gatewayed))
	result = D_GATEWAY;
   else if (equali(hispath,E_nodename)) {
	if (strpbrk(address, "!%@") != NULL) {
		if (strcmp(address, hisuser) == 0)
			goto remont;
		strcpy(address, hisuser);
		goto again;
	}
	strcpy(address, hisuser);
	result = D_LOCAL;
   } else {
remont:
	if (strchr(address,'!') == NULL) {
		if ( (s = strrchr(address,'@')) == NULL ) { /* Any at signs?               */
			if ((s = strrchr(address,'%')) != NULL)  /* Locate any percent signs       */
				*s = '@';               /* Yup --> Make it an at sign at  */
		} else if (strchr(address, ':') == NULL) {
			*s = '\0';
			for (p = address; (p = strchr(p,'@')) != NULL; p++)
				*p = '%';
			*s = '@';
		}
	}
	result = D_REMOTE;          /* No -->   Report user is remote            */
   }

   printmsg(4, "remote_address: %s is %s", address,
		result == D_REMOTE ? "remote" : result == D_LOCAL ? "local" : "gateway");

   return result;

}/* remote_address */

/*
 * Return the unique name
 */
char *uniqueid(void)
{
	static char uq[12];
	static char ss[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
	static char zs[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	static unsigned ucntr = 0;
	struct timeb tt;
	int i;
	unsigned short pid = getpid() >> 4;

#define nss     (sizeof ss - 1)         /* 64 */
#define nzs     (sizeof zs - 1)         /* 52 */

	i = 0;
	uq[i++] = zs[ucntr % nzs];
	uq[i++] = zs[(ucntr>>4) % nzs];
	ucntr++;
	ftime(&tt);
	uq[i++] = ss[(int)(tt.time % nss)];
	uq[i++] = ss[(int)((tt.time>> 6) % nss)];
	uq[i++] = ss[(int)((tt.time>>12) % nss)];
	uq[i++] = ss[(int)((tt.time>>18) % nss)];
	uq[i++] = ss[(int)((tt.time>>24) % nss)];
	uq[i++] = ss[(int)(((tt.time>>30)|(pid<<2)) % nss)];
	uq[i++] = ss[(pid>>4) % nss];
	uq[i++] = ss[(pid>>8) % nss];
	uq[i++] = ss[(tt.millitm/10) % nss];

	return uq;
}

int qstripcmpi(char *s1, char *s2)
{
	char *rs1, *rs2;
	int res;

	if (*s1 == '"')
		s1++;
	if (*s2 == '"')
		s2++;
	if ((rs1 = strrchr(s1, '"')) != NULL)
		*rs1 = '\0';
	if ((rs2 = strrchr(s2, '"')) != NULL)
		*rs2 = '\0';
	res = stricmp(s1, s2);
	if (rs1 != NULL)
		*rs1 = '"';
	if (rs2 != NULL)
		*rs2 = '"';
	return res;
}
