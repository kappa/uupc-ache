#define LINESIZE 2048
#define UUX	/* Call external UUX, internal in other case */
#define MAXARGS 500
#define LISTSIZ	 1024	 /* Unix stack limit */

char * getvalue(char * field);
char * getparam(char * field,const char * argname);

extern boolean parse_args(int, char *[]);
extern void Srestoreplace(void);
extern void AssignDefaultDirs(void);
extern int remote_address(char *);
extern int nodelen, domlen;
extern char *uu_machine, *uu_user;
extern boolean fromunix;
extern char *fromnode;	  /* For UUCP From line 	ahd   */
extern char *fromuser;	  /* For UUCP From line	      ahd   */
extern char *GradeList;
extern char *swapto;
extern boolean visual_output;
extern boolean ignsigs;
extern boolean mime_header;
extern char *extsetname, *intsetname;
extern char *charset[9];
extern boolean bit8_body, bit8_header;
extern boolean verbose;
extern struct charset_struct {
	char * charsetname;
	char * fname;
	unsigned char * table;
	unsigned char * exttable;
} chartab[];

#define D_LOCAL 0
#define D_REMOTE 1
#define D_GATEWAY 2

#define SSKIP(p) for ( ; *p && isspace(*p); p++)
#define STRAIL(p) { int i; \
		    for (i = strlen(p); i > 0 && isspace(p[i - 1]); i--) \
			; \
		    p[i] = '\0'; \
		  }


