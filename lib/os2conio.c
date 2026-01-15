#define INCL_SUB
#define INCL_DOSPROCESS
#define INCL_KBD
#include <os2.h>
#include <conio.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <os2conio.h>
#include <lib.h>

#define MAXTIDS 10

USHORT directvideo;
USHORT wscroll[MAXTIDS];
static USHORT _winx1[MAXTIDS],_winy1[MAXTIDS],_winx2[MAXTIDS],_winy2[MAXTIDS];
static USHORT _curx[MAXTIDS], _cury[MAXTIDS];
static BYTE   _curcol[MAXTIDS];

int gettid(void)
{ 
  static int ntids=0;
  static int tidnum[MAXTIDS];
  PIB *pinfo;
  TIB *tinfo;
  ULONG tid;
  int i;

  DosGetInfoBlocks(&tinfo, &pinfo);
  tid=tinfo->tib_ordinal;
  for (i=0;i<ntids;i++)
    if (tidnum[i]==tid)
      return i;
  if (i==MAXTIDS) return -1;
  tidnum[ntids++]=tid;
  return i;
}

void window(USHORT x1, USHORT y1, USHORT x2, USHORT y2)
{ int tid = gettid();

  _winx1[tid]=x1;
  _winy1[tid]=y1;
  _winx2[tid]=x2;
  _winy2[tid]=y2;
  _curx[tid]=y1-1;
  _cury[tid]=x1-1;
  _wscroll = 1;
}

void clrscr(void)
{ BYTE attribute[2]; 
  USHORT row;
  int tid=gettid();

  attribute[0] = ' ';
  attribute[1] = _curcol[tid];
  for (row=_winy1[tid]-1; row<_winy2[tid]; row++)
    VioWrtNCell(attribute, _winx2[tid]-_winx1[tid]+1, row, _winx1[tid]-1, 0);
}

void gotoxy(USHORT x, USHORT y)
{ int tid=gettid();
  _curx[tid]=y+_winy1[tid]-2;
  _cury[tid]=x+_winx1[tid]-2;
}

void textcolor(BYTE col)
{ int tid=gettid();
  _curcol[tid]=(_curcol[tid] & 0x70) | col;
}

void textbackground(BYTE col)
{ int tid=gettid();
  _curcol[tid]=(_curcol[tid] & 0x0F) | ((col & 7) << 4);
}

int putch(int c)
{ BYTE cc[2];
  int tid=gettid();

  cc[1] = _curcol[tid];
  if (c == '\r')
  {
    _cury[tid]=_winx1[tid]-1;
    return 0;
  }
  if (c == '\n')
    _curx[tid]++;
  else if (c == '\t')
    _cury[tid] += (_cury[tid]+7)%8 + 1;
  else
  { cc[0] = c;
    VioWrtNCell(cc, 1, _curx[tid], _cury[tid], 0);
    _cury[tid]++;
  }
  if (_cury[tid] >= _winx2[tid])
  { _cury[tid] = _winx1[tid]-1;
    _curx[tid]++;
  }
  if (_curx[tid] == _winy2[tid])
  { if (_wscroll && (_winy1[tid]!=_winy2[tid]))
    { cc[0] = ' ';
      VioScrollUp(_winy1[tid]-1, _winx1[tid]-1, _winy2[tid]-1, _winx2[tid]-1, 1, cc, 0);
    }
    _curx[tid]--;
  }
  return 0;
}

int cputs(const char * s)
{ char * p;

  for (p = (s == NULL) ? "(Null)" : (char *)s; *p; p++) 
    putch(*p); 
  return 0;
}

int cprintf(const char * fmt, ...)
{
  char str[256];
  va_list args;

  va_start(args, fmt);
  vsprintf(str,fmt,args);
  va_end(args);
  return cputs(str);
}

#ifdef __EMX__
int kbhit(void)
{
  KBDKEYINFO key;
  
  if (KbdPeek(&key, 0))
    return 0;
  if ((key.fbStatus & 0xC0) == 0) return 0;
  if ((key.chChar == 0) || ((key.chChar == 0xe0) && (key.chScan != 0)))
    return (key.chScan<<8);
  return key.chChar;
}

int cscanf(char * fmt, ...)
{
  va_list args;
  char str[256];
  char c;
  int  i;

  va_start(args, fmt);
  for (i=0; i<sizeof(str)-1; i++)
  { c=getche();
    if ((c!='\n') && (c!='\r'))
      str[i]=c;
  }
  str[i]=0;
  i=vsscanf(str, fmt, args);
  va_end(args);
  return i;
}
#endif

void _setcursortype(BYTE cursor)
{ VIOCURSORINFO curtype;

  VioGetCurType(&curtype, 0);
  curtype.attr = (cursor == _NOCURSOR) ? 0xffff : 0;
  VioSetCurType(&curtype, 0);
}

void textmode(BYTE mode)
{
  VIOMODEINFO vinfo = { sizeof(vinfo), VGMT_OTHER, 4, 80, 25, 640, 350, 0, 0 };
  if ((mode) == C80)
    VioSetMode(&vinfo, 0);
}

void textattr(BYTE att)
{ int tid=gettid();
  _curcol[tid] = att;
}

USHORT wherex(void)
{ int tid=gettid();
  return _cury[tid]+2-_winx1[tid];
}

USHORT wherey(void)
{ int tid=gettid();
  return _curx[tid]+2-_winy1[tid];
}

void clreol(void)
{ char cc[2];
  int tid=gettid();

  cc[0]=' ';
  cc[1]=_curcol[tid];
  VioWrtNCell(cc, _winx2[tid]-1-_cury[tid], _curx[tid], _cury[tid], 0);
}

void gettextinfo(struct text_info * vc)
{
  VIOMODEINFO vinfo;

  vinfo.cb = sizeof(vinfo);
  VioGetMode(&vinfo,0);
  vc->screenwidth = vinfo.col;
  vc->screenheight = vinfo.row;
  vc->winleft = vc->wintop = 1;
  vc->winright = vinfo.col;
  vc->winbottom = vinfo.row;
  vc->normattr = LIGHTGRAY;
  if ((vinfo.fbType == 0) || (vinfo.fbType == VGMT_DISABLEBURST))
    vc->currmode = MONO;
  else if (vinfo.color == 1)
    vc->currmode = BW80;
  else
    vc->currmode = C80;
}

void gettext(USHORT x1, USHORT y1, USHORT x2, USHORT y2, char * buf)
{
#if 0
  USHORT fWait=VP_WAIT;

  VioPopUp (&fWait, 0);
#else
  int i;
  USHORT length=(x2-x1+1)*2;

  for (i=0; i<(y2-y1+1); i++)
    VioReadCellStr(buf+(x2-x1+1)*2, &length, y1+i-1, x1-1, 0);
#endif
}

void puttext(USHORT x1, USHORT y1, USHORT x2, USHORT y2, char * buf)
{
#if 0
  VioEndPopUp (0);
#else
  int i;

  for (i=0; i<(y2-y1+1); i++)
    VioWrtCellStr(buf+(x2-x1+1)*2, (x2-x1+1)*2, y1+i-1, x1-1, 0);
#endif
}
