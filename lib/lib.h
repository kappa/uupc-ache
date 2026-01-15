/*--------------------------------------------------------------------*/
/*      l i b . h                                                     */
/*                                                                    */
/*      Update log:                                                   */
/*                                                                    */
/*      13 May 89    Added PCMAILVER                        ahd       */
/*      Summer 89    Added equali, equalni, compiled,                 */
/*                         compilet                         ahd       */
/*      22 Sep 89    Add boolean typedef                    ahd       */
/*      01 Oct 89    Make logecho boolean                   ahd       */
/*      19 Mar 90    Move FOPEN prototype to here           ahd       */
/*      02 May 1990  Allow set of booleans options via options=       */
/*  8 May  90  Add 'pager' option                                     */
/* 10 May  90  Add 'purge' option                                     */
/*--------------------------------------------------------------------*/

#ifndef __LIB
#define __LIB

#ifdef __MSDOS__
#define _Far far /* for far arrays declaration */
#else
#define _Far
#endif

/*--------------------------------------------------------------------*/
/*                 Macro for recording when UUPC dies                 */
/*--------------------------------------------------------------------*/

#define panic()  bugout( __LINE__, cfnptr);

/*--------------------------------------------------------------------*/
/*                     Configuration file defines                     */
/*--------------------------------------------------------------------*/

#define B_REQUIRED 0x00000001L /* Line must appear in configuration   */
#define B_FOUND    0x00000002L /* We found the token                  */

#define B_GLOBAL   0x00000004L /* Must not appear in PERSONAL.RC      */
#define B_LOCAL    0x00000008L /* The opposite of B_GLOBAL, sort of   */

#define B_MTA      0x00000010L /* Used by Mail Delivery (RMAIL)       */
#define B_MUA      0x00000020L /* Used by Mail User Agent (MAIL)      */
#define B_MUSH     0x00000040L /* Used by MUSH - Not used by UUPC     */
#define B_NEWS     0x00000080L /* Used by NEWS software               */
#define B_UUCICO   0x00000100L /* Used by transport program UUCICO    */
#define B_UUCP     0x00000200L /* Used by UUCP command                */
#define B_UUPOLL   0x00000400L /* UUPOLL program                      */
#define B_UUSTAT   0x00000800L /* UUSTAT, UUSUB, UUNAME programs      */
#define B_UUXQT    0x00001000L /* Used by queue processor UUXQT       */
#define B_INSTALL  0x00002000L /* Used by install program only        */
#define B_BATCH    0x00004000L /* Used by news batching program - GMM */
#define B_GENERIC  0x00008000L /* Generic utilties with no spec vars  */
#define B_MAIL     (B_MUA | B_MTA | B_MUSH)
#define B_SPOOL    (B_MTA | B_NEWS | B_UUCICO | B_UUXQT | B_UUCP | B_UUSTAT)
#define B_ALL      (B_MAIL|B_SPOOL|B_NEWS|B_UUPOLL|B_UUSTAT|B_BATCH|B_GENERIC)

#define B_SHORT    0x00010000L /* Pointer is to short int, not string */
#define B_TOKEN    0x00020000L /* Pointer is one word, ignore blanks  */
#define B_BOOLEAN  0x00040000L /* Pointer is to boolean keywords      */
#define B_LIST     0x00080000L /* Pointer to array of char pointers   */
#define B_CLIST    0x00100000L /* Pointer to array of char pointers,
                                  input is separated by colons, not
                                  spaces                              */
#define B_STRING   0x00200000L /* String value (same as original UUPC
                                  configuration processor             */
#define B_NORMAL   0x00400000L /* Normalize backslashes to slashes in
                                  in this variable                    */
#define B_OBSOLETE 0x00800000L /* Option is obsolete, should be
                                  deleted                             */
#define B_MALLOC   0x01000000L  /* Use malloc(), not newstr()         */
#define B_LONG     0x02000000L  /* Pointer is to long, not string     */
#define B_PATH     (B_TOKEN | B_NORMAL)
                               /* DOS Path name                       */

#define DCSTATUS    "hostatus"
#define LOGFILE     "uucico.log"
#define PASSWD      "passwd"
#define PATHS       "hostpath"
#define PERMISSIONS "permissn"
#define RMAILLOG    "rmail.log"
#define SYSLOG      "syslog"
#define SYSTEMS     "systems"
#define DIALERS		"dialers"

#define WHITESPACE " \t\n\r"

/*--------------------------------------------------------------------*/
/*    Equality macros                                                 */
/*--------------------------------------------------------------------*/

#define equal(a,b)               (!strcmp(a,b))
#define equali(a,b)              (!stricmp(a,b))                     /*ahd */
#define equalni(a,b,n)           (!strnicmp(a,b,n))                  /*ahd */
#define equaln(a,b,n)            (!strncmp(a,b,n))

#define currentfile()            static char *cfnptr = __FILE__
#define checkref(a)              { if (!a) checkptr(cfnptr,__LINE__); }
#define newstr(a)                (strpool(a, cfnptr ,__LINE__))

#define nil(type)               ((type *)NULL)

/*--------------------------------------------------------------------*/
/*                  Your basic Boolean logic values                   */
/*--------------------------------------------------------------------*/

#undef FALSE
#undef TRUE
typedef enum { FALSE = 0, TRUE = 1 } boolean;

typedef unsigned short INTEGER;  /* Integers in the config file      */

/*--------------------------------------------------------------------*/
/*                          Global variables                          */
/*--------------------------------------------------------------------*/

extern int debuglevel;
extern boolean logecho;
extern FILE *logfile;

/*--------------------------------------------------------------------*/
/*      Configuration file strings                                    */
/*--------------------------------------------------------------------*/

extern char *name, *mailbox, *homedir;
#define E_mailbox mailbox
extern char *maildir, *newsdir, *spooldir, *confdir, *pubdir, *tempdir;
#define E_spooldir spooldir
#define E_confdir confdir
extern char *localdomain;
#define E_localdomain localdomain                                    /* ahd   */
extern char *E_indevice, *E_inspeed, *E_inmodem;
extern char *domain, *fdomain, *nodename, *mailserv;
#define E_domain domain
#define E_fdomain fdomain
#define E_nodename nodename
#define E_mailserv mailserv
extern char *postmaster, *anonymous;                            /* ahd   */
extern char *E_cwd;
extern INTEGER maxhops;
extern int PacketTimeout;
extern INTEGER MaxErr;
extern INTEGER xfer_bufsize;

/*--------------------------------------------------------------------*/
/*                        Function prototypes                         */
/*--------------------------------------------------------------------*/

void printerr(const char *func, const char *prefix);

extern void checkptr(const char *file, int line);

extern int MKDIR(const char *path);
							  /* Make a directory              ahd */

extern int CHDIR(const char *path);
							  /* Change to a directory          ahd */

extern int CREAT(char *name,
		 const int mode,
		 const char ftype);                          /* ahd */

extern FILE *FOPEN(const char *name,
		   const char *mode,
		   const char ftype);                       /* ahd   */

extern int RENAME(char *oldname, char *newname);

char *qstrcat(char *dest, char *s);

int getargs(char *line,
	    char *flds[],
            int maxfields);                                   /* ahd */

void printmsg(int level, char *fmt, ...);

int configure(void);

char *normalize( const char *path );

#define denormalize( path ) { char *xxp = path; \
   while ((xxp = strchr(xxp,'/')) != NULL)  \
      *xxp++ = '\\';  }

void bugout( const size_t lineno, const char *fname);

char *strpool( const char *input , const char *file, size_t line);

int real_flush(int);

/*--------------------------------------------------------------------*/
/*                   Compiler specific information                    */
/*--------------------------------------------------------------------*/

/* Ache added */
void dbgputc(const char c);
void dbgputs(char *str);
void show_char(const unsigned char byte);

struct	Table {
	char*	sym;
	char**	loc;
	char	must;
	char	sys;
	char	std;
	char	*suff;
};

void getconfig(FILE* fp, int sysmode, struct Table *table);

void close_junk_files(void);

boolean cp(const char *from, boolean ftmp, const char *to, boolean ttmp);

#ifndef __TURBOC__
#define setcbrk(r)		{ signal(SIGBREAK, (r) ? SIG_DFL : SIG_IGN); \
                                  signal(SIGTERM , (r) ? SIG_DFL : SIG_IGN); \
                                  signal(SIGINT  , (r) ? SIG_DFL : SIG_IGN); }
#endif

#ifdef __WATCOMC__
#define _chdrive(disk)		(_dos_setdrive(disk,(unsigned *)"dummy"), 0)
#endif

#ifndef O_DENYNONE
#define O_DENYNONE		0u
#endif

#ifndef min
#define max(a,b)		((a)>(b) ? (a) : (b))
#define min(a,b)		((a)<(b) ? (a) : (b))
#endif

#ifdef __EMX__
extern int _fmode_bin;
#define mkdir(dirname)			mkdir(dirname, 0750)
#define _fullpath(buf, path, size)	(_fullpath(buf, path, size) ? NULL : buf)
#define cwait(status, pid, option)	waitpid(pid, status, 0)
#define itoa(val, str, radix)		_itoa(val, str, radix)
#define ltoa(val, str, radix)		_ltoa(val, str, radix)
#define _fmode				_fmode_bin;
#define _chdrive(drive)			_chdrive((drive)+'A'-1)
#define _getdrive()			(_getdrive()-'A'+1)
#define getcwd(buf, size)		_getcwd2(buf, size)
#endif

#endif
