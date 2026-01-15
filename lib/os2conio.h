#include <os2.h>

#if !defined(__COLORS)
#define __COLORS

enum COLORS {
    BLACK,          /* dark colors */
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    BROWN,
    LIGHTGRAY,
    DARKGRAY,       /* light colors */
    LIGHTBLUE,
    LIGHTGREEN,
    LIGHTCYAN,
    LIGHTRED,
    LIGHTMAGENTA,
    YELLOW,
    WHITE
};
#endif

#define BLINK       128 /* blink bit */

struct text_info {
    unsigned char winleft;
    unsigned char wintop;
    unsigned char winright;
    unsigned char winbottom;
    unsigned char attribute;
    unsigned char normattr;
    unsigned char currmode;
    unsigned char screenheight;
    unsigned char screenwidth;
    unsigned char curx;
    unsigned char cury;
};

enum text_modes { LASTMODE=-1, BW40=0, C40, BW80, C80, MONO=7, C4350=64 };

#define _NOCURSOR	0
#define _NORMALCURSOR	2

extern USHORT directvideo, wscroll[];
#define _wscroll wscroll[gettid()]

void window(USHORT x1, USHORT y1, USHORT x2, USHORT y2);

void clrscr(void);
void gotoxy(USHORT x, USHORT y);
void textcolor(BYTE col);
void textbackground(BYTE col);
int  cputs(const char * s);
int  cprintf(const char * fmt, ...);
int  putch(int c);
void _setcursortype(BYTE c);
void textmode(BYTE mode);
void textattr(BYTE att);
USHORT wherex(void);
USHORT wherey(void);
void clreol(void);
void gettextinfo(struct text_info * vc);
void gettext(USHORT x1, USHORT y1, USHORT x2, USHORT y2, char * buf);
void puttext(USHORT x1, USHORT y1, USHORT x2, USHORT y2, char * buf);
int  gettid(void);
int  kbhit(void);
int  cscanf(char * fmt, ...);
