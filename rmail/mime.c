/*-
 * Copyright (C) 1997 by Andrey A. Chernov, Moscow, Russia.
 * Changes copyright (C) 1997 Pavel Gulchouck, Kiev, Ukraine
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <io.h>
#include <fcntl.h>
#include <stdlib.h>
#include "lib.h"
#include "pcmail.h"
#include "rmailcfg.h"

#define MAXPARAMLEN 2048
#define MAXVALUELEN 2048

currentfile();

extern char raw_table[];
extern unsigned char ext2pc[128];
extern unsigned char pc2ext[128];

static char * struct_fields[]=
            { "Cc:",
              "Bcc:",
              "Content-Type:",
              "Content-Transfer-Encoding:",
              "Content-Disposition:",
              "Content-Length:",
              "Mime-Version:",
              "Received:",
              "Precedence:",
              "Lines:",
              "Resent-From:",
              "Resent-To:",
	      "Resent-Cc:",
              "Resent-Date:",
              "Resent-Message-Id:",
              "Apparently-To:",
	      "Followup-To:",
	      "Mail-Followup-To:",
              "To:",
              "From:",
              "Message-Id:",
              "Date:",
              "References:",
              "NNTP-Posting-Host:",
              "Approved:",
              "Return-Receipt-To:",
              "Sender:",
              "Errors-To:",
              "Newsgroups:",
              "In-Reply-To:",
              "Return-Path:"
            };

static unsigned char cunbase64[256]={
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255, 64,255,255,
255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};

char * getvalue(char * field)
{ char * p;
  static char value[MAXVALUELEN];
  int i;
  int inbracket;
  boolean inquote;

  value[0]='\0';
  for (p=field;*p && !isspace(*p);p++);
  if (*p=='\0') return NULL;
  for (p++;isspace(*p);p++);
  inbracket=0;
  inquote=FALSE;
  i=0;
  for (;*p;p++)
  { if (*p=='"') {
      inquote=!inquote;
      continue;
    }
    if (*p=='(') {
      inbracket++;
      continue;
    }
    if (*p==')') {
      if (inbracket) inbracket--;
      continue;
    }
    if (inbracket>0) continue;
    if (i>=sizeof(value)-1) break;
    if (*p=='\\') {
      if ((p[1]=='\"') || (p[1]=='\\'))
        value[i++]=*++p;
      continue;
    }
    if ((isspace(*p) || (*p==';')) && !inquote)
      break;
    value[i++]=*p;
  }
  value[i]='\0';
  return value;
}

static char * getparam1(char * field, const char * argname, char * param)
{
  char * p;
  int i;
  int inbracket, inname;
  boolean inquote, inparam;

  param[0]='\0';
  for (p=field;*p && !isspace(*p);p++);
  if (*p=='\0') return NULL;
  for (p++;isspace(*p);p++);
  inbracket=0;
  inquote=inparam=FALSE;
  inname=-1;
  i=0;
  for (;*p;p++)
  { if (*p=='"') {
      inquote=!inquote;
      continue;
    }
    if (*p=='(') {
      inbracket++;
      continue;
    }
    if (*p==')') {
      if (inbracket) inbracket--;
      continue;
    }
    if (inbracket>0) continue;
    if (i>=MAXPARAMLEN-1) break;
    if (*p=='\\') {
      if ((p[1]=='\"') || (p[1]=='\\')) 
      { p++;
        if (inparam)
          param[i++]=*p;
      }
      continue;
    }
    if ((isspace(*p) || (*p==';')) && !inquote) {
      if (inparam) break;
      inname=0;
      continue;
    }
    if (inname>=0 && !inparam)
      { if (argname[inname]=='\0' && isspace(*p)) continue;
        if (argname[inname]=='\0' && *p=='=')
        { inparam=TRUE;
          continue;
        }
        if (toupper(*p)==toupper(argname[inname]))
          inname++;
        else
          inname=-1;
        continue;
      }
    if (inparam) {
      if (isspace(*p) && (i==0) && !inquote) continue;
      param[i++]=*p;
    }
  }
  if (!inparam) return NULL;
  param[i]='\0';
  return param;
}

static void decode2184(char * str)
{
  char *src, *dest;

  src=strchr(str, '\'');
  if (src)
    src=strchr(src+1, '\'');
  if (src==NULL) src=str;
  else src++;
  for (dest=str;*src;)
  { if (*src!='%' || !isxdigit(src[1]) || !isxdigit(src[2]))
    { *dest++=*src++;
      continue;
    }
    src++;
    dest[0]=src[0];
    dest[1]=src[1];
    dest[2]='\0';
    *dest++=(unsigned char)strtol(dest, NULL, 16);
  }
}

char * getparam(char * field, const char * argname)
{ int i;
  char *p, *value;
  static char param[MAXPARAMLEN];

  if (getparam1(field, argname, param))
    return param;
  p = malloc(strlen(argname)+10);
  if (p == NULL) return NULL;
  strcpy(p, argname);
  strcat(p, "*");
  if (getparam1(field, p, param))
  { decode2184(param);
    free(p);
    return param;
  }
  value = param;
  for (i=0; i<9999; i++)
  { sprintf(p, "%s*%d", argname, i);
    if (getparam1(field, p, value))
    { value+=strlen(value);
      continue;
    }
    strcat(p, "*");
    if (getparam1(field, p, value)==NULL)
      if (i==0)
        continue;
      else if ((i==1) && (param[0]=='\0'))
      { free(p);
        return NULL;
      }
      else
      { free(p);
        return param;
      }
    decode2184(value);
    value+=strlen(value);
  }
  free(p);
  return param;
}

void xlat(char * buf, char * xlat_table)
{
	for (; *buf; buf++)
		if (*buf & 0x80)
			*buf = xlat_table[(*buf) & 0x7f];
}

void set_table(char ** xlat_table, char ** dest_MIME_charset,
                      char * orig_MIME_charset,boolean fromunix,int remote)
{
	int i, k;
	int h;

	if (orig_MIME_charset == NULL)
		orig_MIME_charset = "us-ascii";
#ifndef CHANGETRANSIT
	if (fromunix && remote == D_REMOTE) {
		*dest_MIME_charset = orig_MIME_charset;
		*xlat_table = raw_table;
	}
#endif
	if (stricmp(orig_MIME_charset, "us-ascii") == 0)
		if (bit8_body ||
		   (bit8_header && (!mime_header || remote == D_LOCAL || fromunix)))
			orig_MIME_charset = fromunix ? extsetname : intsetname;
	for (i=0; chartab[i].charsetname; i++) {
		if (stricmp(chartab[i].charsetname, orig_MIME_charset) == 0) {
			if (chartab[i].exttable == NULL) {
				h = open(chartab[i].fname, O_BINARY|O_RDONLY);
				if (h == -1) {
					printerr("sendone", chartab[i].fname);
					printmsg(0, "Can't open charset file %s!", chartab[i].fname);
					break;
				}
				chartab[i].exttable = malloc(128);
				checkref(chartab[i].exttable);
				lseek(h, 128, SEEK_SET);
				read(h, chartab[i].exttable, 128);
				close(h);
			}
			if (remote == D_REMOTE) {
				*xlat_table = chartab[i].exttable;
				*dest_MIME_charset = extsetname;
				return;
			}
			if (chartab[i].table == NULL) {
				chartab[i].table = malloc(128);
				checkref(chartab[i].table);
				for (k=0; k<128; k++)
					chartab[i].table[k] = ext2pc[chartab[i].exttable[k] & 0x7f];
			}
			*xlat_table = chartab[i].table;
			*dest_MIME_charset = intsetname;
			return;
		}
	}
	*xlat_table = raw_table;
	*dest_MIME_charset = orig_MIME_charset;
}

static int qpchar(char c)
{
  if (c & 0x80) return 1;
  if (c<=32) return 1;
  if (isspace(c)) return 1;
  if (strchr("()<>@,;:\"/[]?.=\\_", c)) return 1;
  return 0;
}

char * mime(char * str, char * charsetname)
{ unsigned char * s, * p, * pl, * pldest, * sdest;
  int  llen, lword, lenword=0, i, structured;
  unsigned char c;

  lword=7; /* bit */
  for (p=str;*p;p++)
    if ((*p=='\n') & p[1])
      *p=' ';
    else if (*p & 0x80)
      lword=8;
  if (lword==7)
    return str;
  s=strdup(str);
  if (s==NULL) return str;
  lword=7;
  sdest=malloc(strlen(str)*3+128);
  if (sdest==NULL)
  { free(s);
    return str;
  }
  structured=0;
  for (i=0;i<sizeof(struct_fields)/sizeof(struct_fields[0]);i++)
    if (strnicmp(str,struct_fields[i],strlen(struct_fields[i]))==0)
      structured=1;
  pl=s,pldest=sdest;
  llen=0;
  for (;;)
  { int wasrbr=0;

    if (structured)
    { if (*pl=='(')
      { if (lword==8)
        { strcpy(pldest,"?= ");
          pldest+=2;
          lword=7;
        }
        *pldest++=*pl++;
      }
      else if (*pl==')')
      { if (lword==8)
        { strcpy(pldest,"?=");
          lword=7;
        }
        *pldest++=*pl++;
        wasrbr=1;
      }
    }
    for (p=pl;isspace(*p);p++);
    for (;*p && !isspace(*p) && ((!structured) || ((*p!='(') && (*p!=')')));p++)
      if (*p & 0x80) break;
    if ((*p & 0x80) == 0)
    { /* 7 bit */
      if (lword==8)
      { strcpy(pldest,"?=");
        pldest+=2;
        llen+=2;
      }
      c=*p;
      *p='\0';
      strcpy(pldest,pl);
      pldest+=strlen(pl);
      pl+=strlen(pl);
      llen+=strlen(pl);
      *p=c;
      if (*p=='\0')
      { free(s);
        return sdest;
      }
      lword=7;
      if (llen>MAXUNFOLD)
      { strcpy(pldest,"\n\t");
        pldest+=2;
        pl++; /* skip space */
        llen=1;
      }
      continue;
    }
    /* 8bit */
    if (lword==7)
    { if (structured && wasrbr && (!isspace(*pl)))
        *pldest++=' ';
      else
        while(isspace(*pl))
          *pldest++=*pl++;
      sprintf(pldest,"=?%s?Q?",charsetname);
      lenword=strlen(pldest);
      pldest+=lenword;
    }
    else
    { while (isspace(*pl))
      { if (lenword+3+2>=75)
        { strcpy(pldest,"?=");
          llen+=2;
          pldest+=2;
          if (llen>=MAXUNFOLD)
          { strcpy(pldest,"\n\t");
            pldest+=2;
            llen=1;
          }
          else
          { *pldest++=' ';
            llen++;
          }
          sprintf(pldest,"=?%s?Q?",charsetname);
          lenword=strlen(pldest);
          pldest+=lenword;
        }
        sprintf(pldest,"=%02X",*pl++);
        pldest+=3;
        llen+=3;
        lenword+=3;
      }
    }
    for (;*pl && !isspace(*pl);pl++)
    {
      if (lenword+3+2>=75)
      { strcpy(pldest,"?=");
        llen+=2;
        pldest+=2;
        if (llen>=MAXUNFOLD)
        { strcpy(pldest,"\n\t");
          pldest+=2;
          llen=1;
        }
        else
        { *pldest++=' ';
          llen++;
        }
        sprintf(pldest,"=?%s?Q?",charsetname);
        lenword=strlen(pldest);
        pldest+=lenword;
      }
      if (!qpchar(*pl))
      { *pldest++=*pl;
        llen++;
        lenword++;
        continue;
      }
      sprintf(pldest,"=%02X",*pl);
      pldest+=3;
      llen+=3;
      lenword+=3;
    }
    lword=8;
    if ((*pl=='\0') || (*pl=='\n'))
    { free(s);
      strcpy(pldest,"?=");
      if (*pl=='\n')
        strcat(pldest,"\n");
      return sdest;
    }
    if (llen>MAXUNFOLD)
    { if (*pl=='\0')
      { free(s);
        strcpy(pldest,"?=");
        return sdest;
      }
      strcpy(pldest,"?=\n\t");
      pldest+=4;
      llen=1;
      lword=7;
    }
  }
}

static int unqp(int (*getbyte)(void),int (*putbyte)(char))
{
  int  c;
  char s[3];
  char * p;

  for (;;)
  {
    c=getbyte();
nextqpbyte:
    if (c==-1) return 0;
    if (c!='=')
    { if (putbyte(c)==0)
        continue;
      else
        return -1;
    }
    c=getbyte();
    if (c==-1)
    { putbyte('=');
      break;
    }
    s[0]=(char)c;
    if (s[0]=='\n') continue;
    if (s[0]=='\r')
    { c=getbyte();
      if (c==-1)
        break;
      if (c=='\n') continue;
      goto nextqpbyte;
    }
    if (!isxdigit(s[0]))
    { putbyte('=');
      break;
    }
    c=getbyte();
    if (c==-1)
    { putbyte('=');
      putbyte(s[0]);
      break;
    }
    s[1]=(char)c;
    if (!isxdigit(s[1]))
    { putbyte('=');
      putbyte(s[0]);
      break;
    }
    s[2]=0;
    if (putbyte((char)strtol(s,&p,16)))
      return -1;
  }
  printmsg(1,"Bad quoted-printable code\n");
  for (;c!=-1;c=getbyte())
    if (putbyte(c))
      return -1;
  return 1;
}

static int unbase64(int (*getbyte)(void),int (*putbyte)(char))
{
  int  ret=0;
  int  i;
  unsigned char s[4];
  int  c;

  for (;;)
  {
    for (i=0;i<4;)
    { c=getbyte();
      if (c==-1)
      { if (i)
        { ret=1;
          break;
        }
        return 0;
      }
      s[i]=(char)c;
      if (isspace(s[i])) continue;
      s[i]=cunbase64[s[i]];
      if (s[i]==(unsigned char)'\xff')
      { ret=1;
        break;
      }
      i++;
    }
    if (ret) break;
    if ((s[0]==64)||(s[1]==64))
    { ret=1;
      break;
    }
    if (putbyte((s[0]<<2)|(s[1]>>4))) return -1;
    if (s[2]==64)
    { if (s[3]!=64) ret=1;
      break;
    }
    if (putbyte(((s[1]<<4)|(s[2]>>2)) & 0xff)) return -1;
    if (s[3]==64)
      break;
    if (putbyte(((s[2]<<6)|s[3]) & 0xff)) return -1;
  }
  /* wait for EOF */
  if (ret==0) c=getbyte();
  for(;c!=-1;c=getbyte())
  { if (!isspace(c))
      ret=1;
    if (ret)
      if (putbyte(c))
        return -1;
  }
  if (ret==1)
    printmsg(1,"Bad base64 code\n");
  return ret;
}

static char * pstrgetbyte,* pstrputbyte;

static int strgetbyte(void)
{ char c;

  c=*pstrgetbyte;
  if (c)
    return *pstrgetbyte++;
  return -1;
}

static int strputbyte(char c)
{ *pstrputbyte++=c;
  return 0;
}

static void strunqp(char * src,char * dest)
{ char * p;
  pstrgetbyte=src;
  pstrputbyte=dest;
  /* only header! */
  for (p=src;*p;p++)
    if (*p=='_') *p=' ';
  unqp(strgetbyte,strputbyte);
  strputbyte(0);
}

static void strunb64(char * src,char * dest)
{
  pstrgetbyte=src;
  pstrputbyte=dest;
  unbase64(strgetbyte,strputbyte);
  strputbyte(0);
}

void unmime(char * header)
{ char * pch,* pp;
  char * p,* cword;
  int  nq,i;
  static char charset[256];
  char * dest_charset, * xlat_table;
  char * saved_charset = NULL;
  char * lastword;

  p=header;
  if ((*p!=' ') && (*p!='\t'))
  { for (;*p && (!isspace(*p)) && (*p!=':');p++);
    if (*p!=':')
      return; /* bad header */
    p++;
  }
  lastword=NULL;
  for (;;)
  {
    for (;isspace(*p);p++)
#ifdef UNFOLDINGSUBJ
      if ((*p=='\n') && (strlen(p)>1))
        if (strnicmp(header,"Subject:",8)==0)
        { *p=' ';
          for (pp=p+1;isspace(*pp);pp++);
          strcpy(p+1,pp);
        }
#else
		;
#endif
    if (*p=='\0')
      break;
    if (strncmp(p,"=?",2))
    { lastword=NULL;
      p++;
      continue;
    }
    cword=p;
    for (nq=0;*p && (strncmp(p,"?=",2) || nq<3);p++)
      if (*p=='?') nq++;
    if (*p=='\0')
    { lastword=NULL;
      p=cword+1;
      continue;
    }
    *p='\0';
    /* mime word */
    for (pp=cword+2,pch=charset;*pp!='?' && *pp!='*';*pch++=*pp++);
    *pch++='\0';
    while (*pp!='?') pp++;
    set_table(&xlat_table, &dest_charset, charset, TRUE, D_LOCAL);
    if (strcmp(dest_charset, intsetname))
      if (saved_charset == NULL)
        saved_charset = strdup(charset);
    pp++;
    if (pp[1]!='?')
    { *p='?';
      p=cword+1;
      lastword=NULL;
      continue;
    }
    switch(toupper(*pp))
    { case 'Q': strunqp(pp+2,cword);
                xlat(cword, xlat_table);
                break;
      case 'B': strunb64(pp+2,cword);
                xlat(cword, xlat_table);
                break;
      default:  *p='?';
                p=cword+1;
                lastword=NULL;
                continue;
    }
    pp=cword+strlen(cword);
    if (lastword)
    { /* del spaces */
      i=(int)(cword-lastword);
      strcpy(lastword,cword);
      cword=pp-i;
    }
    else
      cword=pp;
    strcpy(cword,p+2);
    p=cword;
    lastword=cword;
  }
  if (!saved_charset) return;
#ifdef SAVECHARSET
  pp=strchr(header, ':');
  if (pp==NULL) pp=header; /* ? */
  for (p=header+strlen(header); p!=pp; p--)
    p[strlen(saved_charset)+3]=*p;
  sprintf(p+1, " [%s", saved_charset);
  header[strlen(header)] = ']';
#endif
  free(saved_charset);
}
