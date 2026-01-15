/*
   ulib.h
*/

extern boolean port_active;         /* Port active flag for error handler   */
extern char *P_Flag;
extern boolean H_Flag, SkipLogin;
extern char *device;

extern int openline(char *name, char *baud);

extern int sread(char *buffer, int wanted, int timeout);
#define S_TIMEOUT (-1)
#define S_LOST (-2)
#define S_OK 1

#define DTR_WAIT 3

extern int swrite(char *data, int len);

extern void ssendbrk(int duration);

extern void closeline(void);

extern void SIOSpeed(long baud);

extern void hangup( void );
extern int  ipport(char *portname);
extern void modstat(void);

int   w_flush(void);
void  w_purge(void);
void  r_purge(void);

#define MAXSPIN 5
