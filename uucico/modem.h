/*--------------------------------------------------------------------*/
/*    m o d e m . h                                                   */
/*                                                                    */
/*    Prototypes for high level modem support routines                */
/*--------------------------------------------------------------------*/

CONN_STATE callup( void );

CONN_STATE callin( const char *logintime );

int slowwrite( unsigned char *s, int len);

typedef struct {
	int	msg_code;
	char*	msg_text;
} mdmsgs;

int     response(mdmsgs* patterns, int ow);
void	Sshutdown(void);

