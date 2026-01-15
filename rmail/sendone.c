/*
   sendone.c

   copyright (C) 1987 Stuart Lynne
   Changes copyright (C) 1989 Andrew H. Derbyshure
   Changes Copyright (C) 1991-97 by Andrey A. Chernov, Moscow, Russia.
   Changes Copyright (C) 1997 by Pavel Gulchouck, Kiev, Ukraine.

*/
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <time.h>
#include <stdlib.h>
#include <process.h>
#include <io.h>
#include <fcntl.h>
#include <malloc.h>
#include <ctype.h>
#include <errno.h>
#ifdef __OS2__
#include <signal.h>
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
#include "lib.h"
#include "hlib.h"
#include "hostable.h"
#include "address.h"
#include "usertabl.h"
#include "pcmail.h"
#include "swap.h"
#include "memlim.h"
#include "tio.h"
#include "rmailcfg.h"

currentfile();

extern boolean isFrom_(char *);

extern char raw_table[];

/*
   sendone - copies file plus headers to appropriate mailbox
*/
extern TFILE * tf;
extern char mastername[];

boolean mime_header = FALSE;
static char grade = 'L';
static int SpoolRemote = 0;
static boolean ProcessForward = FALSE;
static char buf[LINESIZE];
static char sbuf[80];

static boolean AliasLoop(char *alias, char *username, char *forfile);
char * mime(char * field, char * dest_charset);
void unmime(char * field);
void set_table(char ** xlat_table, char ** dest_MIME_charset,
               char * orig_MIME_charset,boolean fromunix,int remote);
void xlat(char * buf, char * xlat_table);

extern char *udate(void);
extern void write_stat(char *, char *, char **, char *, long);

int sendone(char *address, int remote, int hop)
{

   char hidfile[FILENAME_MAX]; /* host's idfile  */
   char hispath[FILENAME_MAX];
   char hisnode[MAXADDR];
   char hisuser[MAXADDR];
   static char command[BUFSIZ];
   struct UserTable *user = NULL;
   boolean messbody;
   boolean RemoteGateway = FALSE;
   long msgsz;		/*DK*/
   char *sh = NULL;		/*DK*/
   boolean filter = FALSE;	/*DK*/
   boolean sense_header;
   struct HostTable *hostp;
   char *addrlist[200];
   int  i, k;
   boolean was_cr;
   FILE * mailfile = NULL;
   TFILE * tempf;
   long lines, length;
   boolean GotCT, GotMV, GotCTE;
   long begheader, offs_header=0;
   char * orig_MIME_charset;
   char *s, *p;
   boolean known_encoding, text;
   char * xlat_table;
   char * dest_MIME_charset;
   char * boundary[3], * cont_type, * cont_encoding;
   int  level;
#ifdef __OS2__
   int  pid=0;
#endif

   printmsg(2, "calling sendone(%s, %d, %d)", address, remote, hop);
   if (++hop > MAXHOPS) {
	printmsg(0, "sendone: max hop count (%d) exceeded", hop);
	return 0;
   }
   i = getargs(address, addrlist, sizeof(addrlist)/sizeof(addrlist[0]));
   if (i < 0) {
	printmsg(0, "sendone: too many addresses");
	return -1;
   }
   if (i == 0) {
	printmsg(0, "sendone: incorrect params");
	return -1;
   }

   if (remote == D_GATEWAY)
		if (i > 1) {
			printmsg(0, "sendone: more then one gateway address");
			return -1;
		}

   if (remote == D_LOCAL) {
		boolean UseForward = TRUE;
		boolean got_user = TRUE;

		if (i > 1) {
			printmsg(0, "sendone: more then one local address");
			return -1;
		}

		user_at_node(*addrlist, hispath, hisnode, hisuser);

		if (*hisuser == '+' && ProcessForward) {
			strcpy(hidfile, hisuser + 1);
			UseForward = FALSE;
			got_user = FALSE;
		} else if (*hisuser == '\\') {
			strcpy(hisuser, hisuser + 1);
			UseForward = FALSE;
		}

		if (got_user) {
			if ((user = checkuser(hisuser)) == BADUSER) {
				printmsg(0, "sendone: no such user: %s, check %s", hisuser, PASSWD);
				return 0;
			} else {
				printmsg(5,"sendone: hisuser=%s", hisuser);
				filter = (sh = user->sh) != nil(char);  	/*DK*/
				if (!filter) {
					/* postbox file name */
					if (strchr(hisuser, SEPCHAR) == nil(char))
						mkfilename(hidfile, maildir, hisuser);
					else
						strcpy(hidfile, hisuser);
				} else
					(void) mktempname(hidfile, "FLT");
			}
		}
		printmsg(5, "sendone: hidfile=%s", hidfile);

		/* Handle local delivery                  */

		if (UseForward) {
			char forfile[FILENAME_MAX];

			mkfilename(forfile, user->homedir, "forward");
			printmsg(5, "sendone: checking for forwarding: %s", forfile);
			if ((mailfile = FOPEN(forfile, "r", TEXT)) != nil(FILE))
			{
				int r;
				char *cp = NULL;

				if (!isatty(fileno(mailfile)))	 /* Is target normal file?  */
					cp = fgets(buf, sizeof(buf), mailfile);     /* Yes--> Read line */

				fclose(mailfile);
				if (cp != nil(char))
				{
					int OldRemote, NewRemote, oldProcessForward;
					int status;
					unsigned char *h, *s, *pcoll, *pp, *ss, *psa, sa[MAXADDR];
					char hisnode[MAXADDR];
					char hispath[MAXADDR];
					char remoteuser[MAXADDR];
					char currentpath[MAXADDR]="";

					SSKIP(cp);
					STRAIL(cp);
					if (!*cp) {
						printmsg(0, "sendone: empty forward in \"%s\"", forfile);
						return 0;
					}
					status = 0;

					cp = strdup(cp);
					checkref(cp);
					pcoll = malloc(strlen(cp) + 1);
					checkref(pcoll);
					*pcoll = '\0';

					h = cp;
					s = NULL;
					if (*h && (s = strpbrk(h, " \t")) != NULL) {
						*s++ = '\0';
						SSKIP(s);
					}
					for (pp = h; (ss = strchr(pp, '!')) != NULL; pp = ss + 1) {
						if (   (pp + nodelen == ss && equaln(pp, nodename, nodelen))
						    || (pp + domlen == ss && equalni(pp, domain, domlen))
						   )
							h = ss + 1;
					}
					if (*h) {
						if (AliasLoop(h, hisuser, forfile))
							return 0;
						strcpy(sa, h);
						OldRemote = remote_address(sa);
						if (OldRemote == D_REMOTE) {
							user_at_node(sa,currentpath,hisnode,remoteuser);
							printmsg(5, "sendone: remote address via %s",currentpath);
						}
						strcpy(pcoll, sa);
					} else {
					badfor:
						printmsg(0, "sendone: bad address in \"%s\"", forfile);
						return 0;
					}
					oldProcessForward = ProcessForward;
					ProcessForward = TRUE;
					if (s != NULL) {
						h = s;
						while (*h && (s = strpbrk(h, " \t")) != NULL) {
							*s++ = '\0';
							SSKIP(s);
							strcpy(sa, h);
							h = s;
						Once:
							for (pp = psa = sa; (ss = strchr(pp, '!')) != NULL; pp = ss + 1) {
								if (   (pp + nodelen == ss && equaln(pp, nodename, nodelen))
								    || (pp + domlen == ss && equalni(pp, domain, domlen))
								   )
									psa = ss + 1;
							}
							if (!*psa)
								goto badfor;
							if (AliasLoop(psa, hisuser, forfile)) {
								ProcessForward = oldProcessForward;
								return 0;
							}
							NewRemote = remote_address(psa);
							if (NewRemote == D_REMOTE) {
								user_at_node(psa,hispath,hisnode,remoteuser);
								printmsg(5, "sendone: remote address via %s",hispath);
							}
							if (   NewRemote != OldRemote
							    || OldRemote != D_REMOTE
							    || strlen(pcoll)+strlen(psa)+1 > LISTSIZ
							   ) { /* Dump it! */
								if ((r = sendone(pcoll, OldRemote, hop)) != 0)
									status = r;
								*pcoll = '\0';
								OldRemote = NewRemote;
								if (NewRemote == D_REMOTE)
									strcpy(currentpath, hispath);
							}
							else {
				 				if (!equal(hispath,currentpath)) {
									if ((r = sendone(pcoll, OldRemote, hop)) != 0)
										status = r;
									*pcoll = '\0';
									strcpy(currentpath, hispath);
								} else
									strcat(pcoll, " ");
							}
							qstrcat(pcoll, psa);
						}
						if (*h) {
							strcpy(sa, h);
							*h = '\0';
							goto Once;
						}
					}
					if (*pcoll) {
						if ((r = sendone(pcoll, OldRemote, hop)) != 0)
							status = r;
					}
					free(pcoll);
					free(cp);

					ProcessForward = oldProcessForward;
					return status;
				 }
			}
		}
   }

   /* decode pass */
   /* mime/unmime headers if need */

   trewind(tf);
   tempf = tfopen();
   if (tempf == NULL) {
		printerr("sendone", "temp file error");
		printmsg(0, "sendone: can't create temp file");
		return -1;
   }

	was_cr = TRUE;
	messbody = FALSE;
	sense_header = FALSE;
	buf[0] = '\0';
	lines = 0;
	begheader = 0;
	level = 0;
	orig_MIME_charset = NULL;
	cont_type = cont_encoding = NULL;
	known_encoding = TRUE;
	text = TRUE;

	for (;;) {
		if (tfgets(sbuf, sizeof(sbuf), tf) == NULL) {
			sbuf[0] = '\0';
			if (buf[0] == '\0') break;
		}
		else if ((*buf == '\0') || (buf[strlen(buf)-1] != '\n') ||
		        ((sense_header || !messbody) && isspace(*sbuf) && (*sbuf != '\n') && (*buf != '\n' || !was_cr)))
			if (strlen(buf)+strlen(sbuf) < sizeof(buf)) {
				strcat(buf, sbuf);
				continue;
			}
	again:
		if (sense_header || !messbody) {
			for (s=buf; *s && (!isspace(*s)) && (*s!=':'); s++);
			if (was_cr && (*s != ':')) {
				/* copy, decode and (un)mime header */
				/* put content-type and content-transfer-encoding */
				if (cont_type) {
					free(cont_type);
					cont_type = NULL;
				}
				if (cont_encoding) {
					free(cont_encoding);
					cont_encoding = NULL;
				}
				tfseek(tf, begheader);
				if (known_encoding || !text) {
					set_table(&xlat_table,&dest_MIME_charset,orig_MIME_charset,fromunix,remote);
					printmsg(10, "sendone: set %s charset to %s",
							 messbody ? "part" : "message", dest_MIME_charset);
				} else {
					xlat_table = raw_table;
					dest_MIME_charset = fromunix ? extsetname : intsetname;
				}
				buf[0] = '\0';
				was_cr = TRUE;
				GotCT = GotMV = GotCTE = FALSE;
				for(;;) {
					if (tfgets(sbuf, sizeof(sbuf), tf) == NULL) {
						sbuf[0] = '\0';
						if (buf[0] == '\0') break;
					}
					else if ((*buf == '\0') || (buf[strlen(buf)-1] != '\n') ||
		        			(isspace(*sbuf) && (*sbuf != '\n') && (*buf != '\n' || !was_cr)))
						if (strlen(buf)+strlen(sbuf) < sizeof(buf)) {
							strcat(buf, sbuf);
							continue;
						}
					for (s=buf; *s && (!isspace(*s)) && (*s!=':'); s++);
					if (was_cr && (*s != ':')) {
						/* end of header */
						if (!GotMV  && !messbody) {
							tfputs("Mime-Version: 1.0\n", tempf);
							if (messbody) lines++;
						}
						if (!GotCT) {
							if ((cont_type == NULL)
#ifndef CHANGEPARTHEADER
							    && !messbody
#endif
							   )
								cont_type = strdup("Content-Type: text/plain\n");
							if (cont_type) {
								/* change charset param to dest_MIME_charset */
								boolean tofile, inquote;
								cont_type[strlen(cont_type) - 1] = '\0'; /* '\n' */
								for (p=cont_type; *p && !isspace(*p); p++)
									tfputc(*p, tempf);
								while (isspace(*p)) tfputc(*p++, tempf);
								tofile = TRUE;
								inquote = FALSE;
								while (*p) {
									for (; *p && (*p != ';' || inquote); p++) {
										if (*p == '"') inquote = !inquote;
										if (tofile) tfputc(*p, tempf);
									}
									if (!*p)
										break;
									if (tofile) tfputc(*p, tempf);
									for (p++; isspace(*p); p++)
										if (tofile) tfputc(*p, tempf);
									tofile = TRUE;
									if (strnicmp(p, "charset=", 8) == 0 ||
									    strnicmp(p, "charset*", 8) == 0)
										tofile = FALSE;
									for (; *p && *p != '='; p++)
										if (tofile) tfputc(*p, tempf);
								}
								if (tofile) tfputs("; ", tempf);
								tfprintf(tempf, " charset=%s\n", dest_MIME_charset);
								strcat(cont_type, "\n");

								if (messbody)
									for (p = cont_type; *p; p++)
										if (*p == '\n') lines++;
							}
						}
						if (!GotCTE) {
							if ((cont_encoding == NULL)
#ifndef CHANGEPARTHEADER
							    && !messbody
#endif
								) {
								cont_encoding = strdup("7bit");
								checkref(cont_encoding);
							}
							if (cont_encoding) {
								if (stricmp(cont_encoding, "binary")) {
									if (bit8_body || (bit8_header && !mime_header)) {
										free(cont_encoding);
										cont_encoding = strdup("8bit");
										checkref(cont_encoding);
									}
								}
								tfprintf(tempf, "Content-Transfer-Encoding: %s\n", cont_encoding);
								if (messbody) lines++;
							}
						}
						break;
					}
					if (strnicmp(buf, "Mime-Version:", 13) == 0) {
						GotMV = TRUE;
						s=getvalue(buf);
						if (strcmp(s, "1.0"))
							known_encoding = FALSE;
					} else if (strnicmp(buf, "Lines:", 6) == 0) {
			skipfield:
						strcpy(buf, sbuf);
						continue;
					} else if (strnicmp(buf, "Content-Type:", 13) == 0) {
						if (cont_type) free(cont_type);
						cont_type = strdup(buf);
						checkref(cont_type);
						s = getvalue(buf);
						if (stricmp(s, "text") == 0) {
							free(cont_type);
							cont_type = NULL;
							goto skipfield;
						}
						if (strnicmp(s, "multipart/", 10) == 0) {
							if (level < sizeof(boundary)/sizeof(boundary[0])) {
								s = getparam(buf, "boundary");
								if (s)
									boundary[level++] = strdup(s);
							}
						} else if ((xlat_table != raw_table) && text)
							goto skipfield;
						GotCT = TRUE;
					} else if (strnicmp(buf, "Content-Transfer-Encoding:", 26) == 0) {
						if (known_encoding) {
							cont_encoding = strdup(getvalue(buf));
							goto skipfield;
						}
						GotCTE = TRUE;
					} else if (strnicmp(buf, "Content-Length:", 15) == 0)
						goto skipfield;
					xlat(buf, xlat_table);
					if (remote == D_LOCAL)
						unmime(buf);
					if (mime_header && remote != D_LOCAL
#ifndef MIMEPARTHEADER
					    && !messbody
#endif
#ifndef CHANGETRANSIT
						&& !fromunix
#endif
						)
						s = mime(buf, dest_MIME_charset);
					else s = buf;
					tfputs(s, tempf);
					if (messbody)
						for (p=s; *p; p++)
							if (*p == '\n')
								lines++;
					if (s != buf) free(s);
					strcpy(buf, sbuf);
					continue;
				}

				tfputs("\n", tempf);
				if (!messbody)
					offs_header = tftell(tempf);
				else
					lines++;
				messbody = TRUE;
				sense_header = FALSE;
				if (!known_encoding)
					xlat_table = raw_table;
				if (cont_type && stricmp(getvalue(cont_type), "message/rfc822")==0) {
					begheader = tftell(tf) - strlen(sbuf) - strlen(buf) + 1;
					sense_header = TRUE;
				}
				if (*buf != '\n') {
					printmsg(4, "sendone: LF inserted after message header");
					goto again;
				}
				strcpy(buf, sbuf);
				continue;
			} else if (strnicmp(buf, "Content-Type:", 13) == 0) {
				s = getvalue(buf);
				printmsg(10, "sendone: get %s content-type %s",
						 messbody ? "part" : "message", s);
				if ((stricmp(s, "text/plain") == 0) ||
				    (stricmp(s, "text/html") == 0) ||
				    (stricmp(s, "text") == 0)) {
					orig_MIME_charset = getparam(buf, "charset");
					text = TRUE;
					if (orig_MIME_charset)
						printmsg(10, "sendone: get %s charset %s",
							     messbody ? "part" : "message", orig_MIME_charset);
				} else
					text = FALSE;
			} else if (strnicmp(buf, "Content-Transfer-Encoding:", 26) == 0) {
				s = getvalue(buf);
				printmsg(10, "sendone: get %s encodong %s",
				         messbody ? "part" : "message", s);
				if ((stricmp(s, "7bit") != 0) &&
				    (stricmp(s, "8bit") != 0) &&
				    (stricmp(s, "binary") != 0))
					known_encoding = FALSE;
				else
					known_encoding = TRUE;
			}
			strcpy(buf, sbuf);
			continue;
		}
		for (i=0; i<level; i++)
			if ((strnicmp(buf, "--", 2) == 0) &&
				(strnicmp(buf+2, boundary[i], strlen(boundary[i])) == 0)) {
				if (buf[strlen(boundary[i])+2] == '\n')
					sense_header = TRUE;
				else if (strncmp(buf+strlen(boundary[i])+2, "--\n", 3) == 0 )
					sense_header = FALSE;
				else /* not boundary */
					continue;
				printmsg(10, "sendone: boundary found in multipart message");
				if (i != level-1)
					printmsg(2, "sendone: end boundary not found in multipart message");
				if (!sense_header) { /* end-boundary */
					i--;
					set_table(&xlat_table, &dest_MIME_charset, "us-ascii",
					          fromunix, remote);
				}
				else
					begheader = tftell(tf) - strlen(sbuf);
				for (k=i+1; k<level; k++)
					free(boundary[k]);
				level = i+1;
			}
		if (isFrom_(buf) && (remote == D_LOCAL))
			tfputs(">", tempf);
		xlat(buf, xlat_table);
		tfputs(buf, tempf);
		for (p=buf; *p; p++)
			if (*p == '\n')
				lines++;
		strcpy(buf, sbuf);
	}

	if (level != 0) {
		printmsg(2, "sendone: end boundary not found in multipart message");
		for (i=0; i<level; i++)
			free(boundary[i]);
	}

	msgsz = tftell(tempf);
	length = msgsz - offs_header;
	trewind(tempf);

	switch (remote) {
	case D_GATEWAY: {
		user_at_node(*addrlist, hispath, hisnode, hisuser);
		hostp = checkname( hispath );
		checkref(hostp);
#ifdef __MSDOS__
		(void) mktempname(hidfile, "GTW");
#endif
		sprintf(command , "%s %s \"%s\" \"%s\" \"%s\" \"%s\""
#ifdef __MSDOS__
		        " < %s"
#endif
                     ,

		     hostp->via,          /* Program to perform forward */
		     hostp->hostname,     /* Nominal host routing via  */
		     hisnode,             /* Final destination system  */
		     hisuser,             /* user on "node" for delivery*/
#ifdef __MSDOS__
		     uu_machine,          /* Remote node (uuxqt) */
		     uu_user,             /* Remote user (uuxqt) */
		     hidfile
#else
		     fromnode,
		     fromuser
#endif
		     );
		/* prevent security hole */
		if (strpbrk(hisnode, "<>") ||
		    strpbrk(hisuser, "<>")) {
			printmsg(0, "sendone: incorrect address \"%s!%s\"",
			         hisnode,hisuser);
			return 0;
		}
#ifndef __MSDOS__
		if (strpbrk(fromnode, "<>") ||
		    strpbrk(fromuser, "<>")) {
			sprintf(command , "%s %s \"%s\" \"%s\" \"%s\" \"%s\"",
			     hostp->via,          /* Program to perform forward */
			     hostp->hostname,     /* Nominal host routing via  */
			     hisnode,             /* Final destination system  */
			     hisuser,             /* user on "node" for delivery*/
			     uu_machine,          /* Remote node (uuxqt) */
			     uu_user              /* Remote user (uuxqt) */
			     );
#else
		{
#endif
			if (strpbrk(uu_machine, "<>") ||
			    strpbrk(uu_user, "<>")) {
				printmsg(0, "sendone: incorrect address \"%s!%s\"",
				         uu_machine,uu_user);
				return 0;
			}
#ifndef __MSDOS__
		}
#else
		}
#endif

#ifdef __MSDOS__
		if (strlen(command)>127)
			sprintf(command , "%s %s \"%s\" \"%s\" < %s",
			     hostp->via,          /* Program to perform forward */
			     hostp->hostname,     /* Nominal host routing via  */
			     hisnode,             /* Final destination system  */
			     hisuser,             /* user on "node" for delivery*/
			     hidfile);


		/* open mailfile */
		if ((mailfile = FOPEN(hidfile, "w", TEXT)) == nil(FILE)) {
			printerr("sendone", hidfile);
			printmsg(0, "sendone: cannot create: %s", hidfile);
			return -1;
		}
#else

		hidfile[0] = '\0';
		printmsg(4, "sendone: execute '%s'", command);
		pid = pipe_system(&i, NULL, command);
		if (pid == -1) {
			printmsg(0, "sendone: cannot execute \"%s\"", command);
			return 0;
		}
		setmode(i, O_TEXT);
		mailfile = fdopen(i, "w+t");
		if (mailfile == NULL) {
			printerr("sendone", "pipe");
			printmsg(0, "sendone: can't fdopen pipe");
			DosKillProcess(DKP_PROCESSTREE, pid);
			cwait(&k, pid, WAIT_CHILD);
			close(i);
			return -1;
		}
#endif
	}
	break;

	case D_LOCAL: {

		/* open mailfile */
		if ((mailfile = FOPEN(hidfile, "a", TEXT)) == nil(FILE)) {
			printerr("sendone", hidfile);
			printmsg(0, "sendone: cannot open/create for append: %s", hidfile);
			return -1;
		}
		fd_lock(hidfile, fileno(mailfile));
		(void) fseek(mailfile, 0L, SEEK_END);	/* to satisfy ftell */
	}
	break;

	case D_REMOTE: {
#ifndef __MSDOS__
		char *argv[20];
		int argc = 0;
		char dbuf[20];
		static char batchname[FILENAME_MAX];
		static char gbuf[] = "-gX";
		int i;
		extern char *batchmail;

		/* All have the same path */
		user_at_node(*addrlist, hispath, hisnode, hisuser);

		if (msgsz < 5000)
			grade = GradeList[0];
		else if (msgsz < 20000)
			grade = GradeList[1];
		else if (msgsz < 50000L)
			grade = GradeList[2];
		else
			grade = GradeList[3];
		argv[argc++] = "uux";
		strcpy(batchname, batchmail);
		argv[argc] = strpbrk(batchname, " \t");
		if (argv[argc] != NULL)
			*argv[argc++]++ = '\0';
		if (debuglevel > 0) {
			sprintf(dbuf, "-%c%d", logecho ? 'X' : 'x', debuglevel);
			argv[argc++] = dbuf;
		}
		if (visual_output && !logecho)
			argv[argc++] = "-Z";
		gbuf[2] = grade;
		argv[argc++] = gbuf;
		argv[argc++] = "-r";
		argv[argc++] = "-l";
		argv[argc++] = "-";
		argv[argc] = NULL;
			printmsg(5, "sendone: (%s) %s executing...", argv[0], batchname);
		if (debuglevel >= 10) {
			printmsg(10, "sendone: args list:");
			for (i = 0; i < argc; i++)
				printmsg(10, "  argv[%d] = '%s'", i, argv[i]);
		}
		fflush(logfile);
		if (logfile != stdout)
			real_flush(fileno(logfile));
		pid = pipe_spawnv(&i, NULL, batchname, argv);
		if (pid < 0) {
			printmsg(1, "sendone: (%s) %s not started",
					argv[0], batchname);
			return -1;
		}
		setmode(i,O_BINARY);
		mailfile = fdopen(i, "wb");
		if (mailfile == NULL) {
			printerr("sendone", "can't fdopen pipe");
			DosKillProcess(pid, DKP_PROCESSTREE);
			close(i);
			cwait(&i, pid, WAIT_CHILD);
			return -1;
		}
		hidfile[0] = '\0';
#else
		/* All have the same path */
		user_at_node(*addrlist, hispath, hisnode, hisuser);

		(void) mktempname(hidfile, "TMP");
		/* Don't use shared open here, because uux open stdin non-shared */
		if ((mailfile = fopen(hidfile, "wb+")) == nil(FILE)) {
			printerr("sendone", hidfile);
			printmsg(0, "sendone: cannot create %s", hidfile);
			return -1;
		}
#endif
	}
	break;
   }

   if (setvbuf(mailfile, NULL, _IOFBF, xfer_bufsize)) {
	printerr("sendone", hidfile);
	printmsg(0, "sendone: can't buffer=%d file %s",
		  xfer_bufsize, hidfile);
#ifndef __MSDOS__
	if (pid != -1) {
	        DosKillProcess(DKP_PROCESSTREE, pid);
		cwait(&i, pid, WAIT_CHILD);
	}
#endif
	fclose(mailfile);
	return -1;
   }
   if (remote == D_REMOTE) {
	fputs(domain, mailfile);
	putc('!', mailfile);
	if (!equal(fromnode,nodename) && !equal(fromnode, domain)) {
	    fputs(fromnode,mailfile);
	    putc('!',mailfile);
	}
	fputs(fromuser, mailfile);
	putc('\n', mailfile);

	fputs(hispath, mailfile);
	fputs("!rmail\n", mailfile);

	if (ferror(mailfile))
		goto Ewr;

	for (i=0; addrlist[i]; i++)
		fprintf(mailfile, "(%s)\n", addrlist[i]);
	fputs("<<NULL>>\n", mailfile);
	if (ferror(mailfile))
		goto Ewr;
   }
   if (hidfile[0]) { /* not pipe */
	msgsz = ftell(mailfile);			/*DK*/
	printmsg(5, "sendone: writing to mailfile \"%s\" at %ld", hidfile, msgsz);
   } else
	printmsg(5, "sendone: writing to pipe");

   /*    Create standard UUCP header for the remote system     */
   if (remote != D_GATEWAY)
	if (remote == D_REMOTE || (!equal(fromnode, nodename) && !equal(fromnode, domain))) {
		checkref(fromnode);
		checkref(fromuser);
		if (remote == D_REMOTE)	{ /* RFC 1123 */
		    if (!equal(fromnode, nodename) && !equal(fromnode, domain))
			fprintf(mailfile, "From %s!%s!%s %s remote from %s\n",
			    domain, fromnode, fromuser, udate(), nodename);
		    else
			fprintf(mailfile, "From %s!%s %s remote from %s\n",
			    domain, fromuser, udate(), nodename);
		} else {
			int i = strlen(fromnode);

			if (   strncmp(fromnode, fromuser, i) == 0
			    && fromuser[i] == '!'
			   )
				goto simple;
			fprintf(mailfile, "From %s!%s %s\n",
				fromnode, fromuser, udate());
		}
	}
	else
	simple:
		fprintf(mailfile,"From %s %s\n", fromuser, udate());

   /* copy tempfile to mailbox file */
   printmsg(4, "sendone: copying tempfile to mailfile (%s)",
	  hidfile);

   messbody = FALSE;
   buf[0] = '\0';
   was_cr = TRUE;

   for (;;) {
	char *p;
	int len;
	char nbuf[LINESIZE];

	if (tfgets(sbuf, sizeof(sbuf), tempf) == NULL) {
		sbuf[0] = '\0';
		if (buf[0] == '\0') break;
	}
        else if ((buf[0] == '\0') || (buf[strlen(buf)-1] != '\n') ||
	         ((sense_header || !messbody) && isspace(*sbuf) && (*sbuf != '\n') && (*buf != '\n' || !was_cr)))
		if (strlen(buf) + strlen(sbuf) < sizeof(buf)) {
			strcat(buf, sbuf);
			continue;
		}

	p = buf;
	if (!messbody && (*p == '\n') && was_cr) {
		messbody = TRUE;

		if (length >= 0) {
			sprintf(nbuf, "Content-Length: %ld\n", length);
			if (fputs(nbuf, mailfile) == EOF)
				goto Ewr;
			if (!hidfile[0]) msgsz += strlen(nbuf);
		}
		if (lines >= 0) {
			sprintf(nbuf, "Lines: %ld\n", lines);
			if (fputs(nbuf, mailfile) == EOF)
				goto Ewr;
			if (!hidfile[0]) msgsz += strlen(nbuf);
		}
	}
#ifdef __OS2__
	if (hidfile[0] == '\0') {
		char *s;
		while ((s = strchr(p, '\x1b')) != NULL)
			*s = ' ';
	}
#endif
	fputs(p, mailfile);
	if (ferror(mailfile)) {
Ewr:
		printerr("sendone", hidfile);
		printmsg(0, "sendone: disk write error on %s", hidfile);
#ifndef __MSDOS__
		if (hidfile[0] == '\0') {
			DosKillProcess(pid, DKP_PROCESSTREE);
			cwait(&i, pid, WAIT_CHILD);
		}
#endif
		fclose(mailfile);
		return -1;
	}
	len = strlen(p);
	if (p[len-1] != '\n') {
		putc('\n', mailfile);
		if (!hidfile[0]) msgsz++;
	}
	was_cr = (buf[strlen(buf)-1] == '\n');
	strcpy(buf, sbuf);
   }

	tfclose(tempf);

	if (hidfile[0]) /* not pipe */
		msgsz = ftell(mailfile) - msgsz;

	if (remote == D_LOCAL)	/* Mailbox terminator */
		putc('\n', mailfile);

	/* close files */

	printmsg(10, "sendone: message size in mailfile (%s) is %ld",
		hidfile, msgsz);

	if (remote != D_REMOTE) {
		if (fclose(mailfile))
			goto Ewr;
	}
	else {
#ifdef __MSDOS__
		char *argv[20];
		int argc = 0;
		char dbuf[20];
		static char batchname[FILENAME_MAX];
		static char gbuf[] = "-gX";
		int savein, i, en;
		extern char *batchmail;

		if (msgsz < 5000)
			grade = GradeList[0];
		else if (msgsz < 20000)
			grade = GradeList[1];
		else if (msgsz < 50000L)
			grade = GradeList[2];
		else
			grade = GradeList[3];
		argv[argc++] = "uux";
		strcpy(batchname, batchmail);
		argv[argc] = strpbrk(batchname, " \t");
		if (argv[argc] != NULL)
			*argv[argc++]++ = '\0';
		if (debuglevel > 0) {
			sprintf(dbuf, "-%c%d", logecho ? 'X' : 'x', debuglevel);
			argv[argc++] = dbuf;
		}
		if (visual_output && !logecho)
			argv[argc++] = "-Z";
		gbuf[2] = grade;
		argv[argc++] = gbuf;
		argv[argc++] = "-r";
		argv[argc++] = "-l";
		argv[argc++] = "-";
		argv[argc] = NULL;

		rewind(mailfile);
		if (ferror(mailfile))
			goto Ewr;
		if ((savein = dup(fileno(stdin))) < 0) {
			printerr("sendone", "dup(0)");
			printmsg(0, "sendone: can't save stdin");
			return -1;
		}
		if (dup2(fileno(mailfile), fileno(stdin)) < 0) {
			printerr("sendone", "dup2(mailfile, 0)");
			printmsg(0, "sendone: can't replace stdin");
			return -1;
		}
		if (fclose(mailfile)) {
			(void) dup2(savein, fileno(stdin));
			close(savein);
			goto Ewr;
		}

		printmsg(5, "sendone: (%s) %s executing...", argv[0], batchname);
		if (debuglevel >= 10) {
			printmsg(10, "sendone: args list:");
			for (i = 0; i < argc; i++)
				printmsg(10, "  argv[%d] = '%s'", i, argv[i]);
		}
		fflush(logfile);
		if (logfile != stdout)
			real_flush(fileno(logfile));

		i = -1;
		if ((en = needkbmem(XKBSIZE - RKBSIZE, batchname)) != 0)
			goto Err;
		if (swapto == NULL && (en = needkbmem(XKBSIZE, batchname)) != 0)
			goto Err;

		i = swap_spawnv(batchname, argv);
		setcbrk(!ignsigs);

		if (i < 0) {
			en = ENOMEM;
		Err:
			printmsg(en == ENOMEM, "sendone: (%s) %s not started",
					argv[0], batchname);
		}

		if (dup2(savein, fileno(stdin)) < 0) {
			printerr("sendone", "dup2(savein, 0)");
			close(savein);
			printmsg(0, "sendone: can't restore stdin");
			return -1;
		}
		close(savein);
		remove(hidfile);
#else
		fflush(logfile);
		if (logfile != stdout)
			real_flush(fileno(logfile));

		if (fclose(mailfile))
			goto Ewr;
		cwait(&i, pid, WAIT_CHILD);
		i &= 0xffff;
		i = ((i>>8) & (i<<8)) & 0xffff;
		if (kbdwait!=-1) DosReleaseMutexSem(kbdwait);
#endif

		printmsg( (i > 0) ? 0 : 12, "sendone: uux error code %d", i);
		if (i != 0)
			return -1;
		SpoolRemote++;
		if (verbose)
			printmsg(1 , "sendone: message for remote system %s queued", hispath);
	}

	if (filter || remote == D_GATEWAY) {
		int i;

		if (filter)
			sprintf(command, "%s %s", sh, hidfile);

#ifndef __MSDOS__
		if (hidfile[0] == '\0') {
			cwait(&i, pid, WAIT_CHILD);
			i &= 0xffff;
			i = ((i<<8) | (i>>8)) & 0xffff;
			if (kbdwait!=-1) DosReleaseMutexSem(kbdwait);
		} else
#endif
		{
			printmsg(4, "sendone: execute '%s'", command);
			i = swap_system(command);
			setcbrk(!ignsigs);
			remove(hidfile);
		}

		printmsg((i!=0 && i!=48) ? 0 : 7, "sendone: gateway return error code %d", i);
		if (i != 0 && i != 48)
			return (i > 0) ? 0 : -1;
		if (i == 48)
			RemoteGateway = TRUE;
	}	/*DK*/

	tunlink(tempf);

	write_stat(fromuser,fromnode,addrlist,hispath,msgsz);		/*DK*/

        return RemoteGateway ? 2 : 1;
} /*sendone*/


static boolean AliasLoop(char *alias, char *username, char *forfile)
{
	char *talias, *tusername, *s;
	boolean res = FALSE;

	if (remote_address(alias))
		return res;

	talias = strdup(alias);
	checkref(talias);
	if ((s = strchr(talias, '@')) != NULL)
		*s = '\0';
	tusername = strdup(username);
	checkref(tusername);
	if ((s = strchr(tusername, '@')) != NULL)
		*s = '\0';

	/* Check Postmaster loop */
	if (   equali(tusername, mastername)
	    && equali(talias, mastername)
	   ) {
		printmsg(0, "AliasLoop: %s cause infinite loop in (must be real user) \"%s\"",
					alias, forfile);
		res = TRUE;
	}
	/* Check User loops (case is significant) */
	else if (strcmp(talias, tusername) == 0) {
		printmsg(0, "AliasLoop: %s user name cause infinite alias loop in \"%s\"",
					alias, forfile);
		res = TRUE;
	}
	free(talias);
	free(tusername);

	return res;
}
