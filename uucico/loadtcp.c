#include <stdlib.h>
#include <sys/types.h>

#include <sys\socket.h>
#include <netdb.h>
#include <utils.h>
#define INCL_DOSMODULEMGR
#include <os2.h>
#include "lib.h"

static PFN SOCKET, SOCLOSE, SOCK_ERRNO, SOCK_INIT;
static PFN CONNECT, SHUTDOWN, SEND, RECV, SELECT, SETSOCKOPT;
static PFN IOCTL;
static PFN GETHOSTBYNAME, INET_ADDR;
static PFN LSWAP=NULL, BSWAP=NULL;
static HMODULE tcp32dll, so32dll;
static int tcploaded=0;

int _System socket( int domain, int type, int protocol)
{ return SOCKET(domain, type, protocol); }

int _System soclose( int sock )
{ return SOCLOSE(sock); }

int _System sock_init( void )
{ return SOCK_INIT(); }

int _System sock_errno( void )
{ return SOCK_ERRNO(); }

int _System connect( int sock, struct sockaddr * addr, int laddr)
{ return CONNECT(sock, addr, laddr); }

int _System shutdown(int sock, int howto)
{ return SHUTDOWN(sock, howto); }

int _System send( int sock, char * buf, int len, int protocol)
{ return SEND(sock, buf, len, protocol); }

int _System recv( int sock, char * buf, int len, int protocol)
{ return RECV(sock, buf, len, protocol); }

int _System select( int * socks, int noreads, int nowrites, int noexcepts, long timeout)
{ return SELECT(socks, noreads, nowrites, noexcepts, timeout); }

int _System setsockopt( int sock, int level, int optname, char * optval, int optlen)
{ return SETSOCKOPT(sock, level, optname, optval, optlen); }

int _System ioctl(int sock, int cmd, char * data, int ldata)
{ return IOCTL(sock, cmd, data, ldata); }

struct hostent * _System gethostbyname( char * name )
{ return (struct hostent *)GETHOSTBYNAME(name); }

unsigned long _System inet_addr( char * cp )
{ return INET_ADDR(cp); }

unsigned long _System lswap(unsigned long l)
{ return LSWAP ? LSWAP(l) : l; }

unsigned short _System bswap(unsigned short u)
{ return BSWAP ? BSWAP(u) : u; }

static void freemodules(void)
{
  printmsg(9, "Free TCP modules");
  DosFreeModule(tcp32dll);
  DosFreeModule(so32dll);
}

int loadtcp(void)
{ APIRET r;
  char buf[256];

  if (tcploaded) return 0;
  printmsg(9, "Loading tcp modules");
  r=DosLoadModule(buf, sizeof(buf), "SO32DLL.DLL", &so32dll);
  if (r)
  { printmsg(4,"loadtcp: DosLoadModule returns %d\n", r);
    printmsg(-1, "Can't find SO32DLL.DLL");
    return 1;
  }
  r=DosLoadModule(buf, sizeof(buf), "TCP32DLL.DLL", &tcp32dll);
  if (r)
  { printmsg(4,"loadtcp: DosLoadModule returns %d\n", r);
    printmsg(-1, "Can't find TCP32DLL.DLL");
    return 1;
  }
  atexit(freemodules);
  printmsg(9, "loadtcp: modules loaded");
  tcploaded=1;
  r=DosQueryProcAddr(so32dll, 0, "SOCKET", &SOCKET);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SOCKET()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SOCK_ERRNO", &SOCK_ERRNO);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SOCK_ERRNO()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SOCLOSE", &SOCLOSE);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SOCLOSE()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SOCK_INIT", &SOCK_INIT);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SOCK_INIT()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "CONNECT", &CONNECT);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of CONNECT()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SHUTDOWN", &SHUTDOWN);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SHUTDOWN()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SEND", &SEND);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SEND()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "RECV", &RECV);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of RECV()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SELECT", &SELECT);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SELECT()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "SETSOCKOPT", &SETSOCKOPT);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of SETSOCKOPT()");
    return 1;
  }
  r=DosQueryProcAddr(so32dll, 0, "IOCTL", &IOCTL);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of IOCTL()");
    return 1;
  }
  r=DosQueryProcAddr(tcp32dll, 0, "GETHOSTBYNAME", &GETHOSTBYNAME);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of GETHOSTBYNAME()");
    return 1;
  }
  r=DosQueryProcAddr(tcp32dll, 0, "INET_ADDR", &INET_ADDR);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of INET_ADDR()");
    return 1;
  }
  r=DosQueryProcAddr(tcp32dll, 0, "LSWAP", &LSWAP);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of LSWAP()");
    return 1;
  }
  r=DosQueryProcAddr(tcp32dll, 0, "BSWAP", &BSWAP);
  if (r)
  { printmsg(-1, "loadtcp: can't get address of BSWAP()");
    return 1;
  }
  return 0;
}
