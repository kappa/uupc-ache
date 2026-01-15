/*
** declarations from FOSSIL & internal drivers
*/

#ifdef __MSDOS__
#define _Fcdecl	far cdecl
#else
#define _Fcdecl
#endif

void _Fcdecl _select_port(int); /* select active port (1 or 2) */
void _Fcdecl save_com(void);   /* save the interupt vectors */
void _Fcdecl restore_com(void);   /* restore those vectors */
#ifdef __MSDOS__
void _Fcdecl _install_buffers(char far *in, char far *out);   /* allocate memory */
#endif
int  _Fcdecl _install_com(void); /* install our vectors */
char _Fcdecl _get_conn_type(void); /* get modem or direct */

void _Fcdecl _open_com(         /* open com port */
   unsigned,  /* baud */
   int,  /* 'M'odem or 'D'irect */
   int,  /* Parity 'N'one, 'O'dd, 'E'ven, 'S'pace, 'M'ark */
   int,  /* stop bits (1 or 2) */
   int); /* Xon/Xoff 'E'nable, 'D'isable */

void _Fcdecl _close_com(void);  /* close com port */
void _Fcdecl _dtr_off(void);    /* clear DTR */
void _Fcdecl _dtr_on(void);     /* set DTR */
void _Fcdecl _rts_on(void);     /* set RTS */
void _Fcdecl _set_connected(int);

unsigned long _Fcdecl r_count(void);    /* receive counts */
   /* high word = total size of receive buffer */
   /* low word = number of pending chars */
#define r_count_size() ((int)(r_count() >> 16))
#define _r_count_pending() ((int)r_count())

int  _Fcdecl receive_com(void); /* get one character */
   /* return -1 if none available */

unsigned long _Fcdecl s_count(void);    /* send counts */
   /* high word = total size of transmit buffer */
   /* low word = number of bytes free in transmit buffer */
#define s_count_size() ((int)(s_count() >> 16))
#define _s_count_free() ((int)s_count())

void _Fcdecl _r_purge(void);
void _Fcdecl _w_purge(void);
int  _Fcdecl send_com(int);    /* send a character */
void _Fcdecl send_local(int);  /* simulate receive of char */
void _Fcdecl sendi_com(int);   /* send immediately */
void _Fcdecl _break_com(void);  /* send a BREAK */
int  _Fcdecl _carrier(void);

void  select_port(int  port);
boolean set_connected(boolean need);
boolean	install_com(void);
void  open_com(long baud,char  modem,char  parity,int  stopbits,char  xon);
void  close_com(void);
void  dtr_on(void);
void  dtr_off(void);
void  break_com(int  length);
void  s_r_purge(void);
int   s_w_flush(void);
void  s_w_purge(void);
int   s_count_free(void);
int   s_1_free(void);
int   r_count_pending(void);
int   r_1_pending(void);
int   s_count_pending(void);
unsigned read_block(unsigned wanted, char *buf);
int receive_char(void);
int transmit_char(char c);
int status(void);
boolean _Fcdecl carrier(boolean);
void timer_parms(void);
#ifdef __MSDOS__
int timer_function(void (interrupt far *cdecl ff)(void), int ins);
#endif
unsigned write_block(unsigned wanted, char *buf);
int peek_ahead(void);
void fossil_delay(unsigned tics);
void fossil_exit(void);
void flowcontrol(boolean);

extern unsigned char fs_tim_int, fs_tics_per_sec;
extern unsigned fs_ms_per_tic;
extern boolean connected;

/* Status bits */

#define FS_TSRE 0x4000
#define FS_THRE 0x2000
#define FS_OVRN 0x0200
#define FS_RDA	0x0100
#define FS_DCD	0x0080

int _Far * _Fcdecl com_errors(void);  /* return far pointer to error counts
					    (in static area) */

#define COM_EROVFLOW 0   /* rec buffer overflows */
#define COM_EOVRUN  1   /* receive overruns */
#define COM_EBREAK  2   /* break chars */
#define COM_EFRAME  3   /* framing errors */
#define COM_EPARITY 4   /* parity errors */
#define COM_EXMIT   5   /* transmit erros */
#define COM_EDSR    6   /* data set ready errors */
#define COM_ECTS    7   /* clear to send errors */
#define COM_ETOVFLOW 8   /* trn buffer overflows */

