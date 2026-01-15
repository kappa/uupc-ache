#ifdef __MSDOS__
#define TBUFSIZE 16384
#else
#define TBUFSIZE 100000
#endif

typedef struct
	{
		boolean incore;
		int  curpos;
		char * name;
		char * mem;
		FILE * file;
	} TFILE;

char * tfgets(char * buf, int bufsize, TFILE * from);
int tfprintf(TFILE * to, char * format, ...);
int tfputs(char * line, TFILE * to);
int tfputc(int c, TFILE * to);
TFILE * tfopen(void);
int tfclose(TFILE * what);
int tunlink(TFILE * what);
void trewind(TFILE * what);
void tfseek(TFILE * what, long offset);
long tftell(TFILE * what);
