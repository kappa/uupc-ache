/*-
 * Copyright (C) 1992-1996 by Pavel Gulchouck, Kiev, Ukraine.
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

#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <sys\types.h>
#include <sys\stat.h>
#define INCL_DOSFILEMGR
#define INCL_DOSQUEUES
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include "uupcmoah.h"

#include "swap.h"

currentfile();

int swap_flags;
unsigned swap_needed;
HMTX kbdwait=-1;

int configure_swap(void)
{
   return 1;
}

int swap_spawnv(const char *name, char * const args[])
{
  int pid;
  int rc;

  if (kbdwait!=-1) DosRequestMutexSem(kbdwait, -1);
  pid=spawnvp(P_NOWAIT,name,args);
  if (pid!=-1) {
    cwait(&rc,pid,WAIT_CHILD);
    rc &= 0xffff;
    rc= ((rc << 8) | (rc >> 8)) & 0xffff;
  }
  else
    rc=-1;
  if (kbdwait!=-1) DosReleaseMutexSem(kbdwait);
  return rc;
}

int redirect(char ** args,int *ind,char * what,int to,int * save)
{
	int h;
	char * p;
	int j;

	p=strstr(args[*ind],what);
	if (p==NULL) return 0;
	if (*save!=-1) return -1;
	*p='\0';
	if (p==args[*ind])
		for (j=*ind;args[j];j++)
			args[j]=args[j+1];
	else
		(*ind)++;
	p+=strlen(what);
	if (*p=='\0')
	{	p=args[*ind];
		if (p==NULL) return -1;
		for (j=*ind;args[j];j++)
			args[j]=args[j+1];
	}
	(*ind)--;
	if ((strcmp(what,">>")==0) || (strcmp(what,">&>")==0)) /* append */
		if (access(p,0))
			h=open(p,O_TEXT|O_WRONLY|O_CREAT,S_IWRITE);
		else {
			h=open(p,O_TEXT|O_APPEND|O_WRONLY);
			if (h!=-1) lseek(h,0,SEEK_END);
		}
	else if (strchr(what,'>'))
	{	if (!access(p,0))
			unlink(p);
		h=open(p,O_TEXT|O_WRONLY|O_CREAT,S_IWRITE);
	}
	else
		h=open(p,O_TEXT|O_RDONLY);
	if (h==-1) return -1;
	*save=dup(to);
	if (*save==-1)
	{	close(h);
		return -1;
	}
	if (dup2(h,to)==-1)
	{	close(*save);
		*save=-1;
		close(h);
		return -1;
	}
	close(h);
	return 1;
}

static void restredir(int saved,int to)
{
	if (saved==-1) return;
	dup2(saved,to);
	close(saved);
}

static char exename[FILENAME_MAX];
static char searchbuf[128];

static int searchfile(char * fname,int needsearch)
{
  if (access(fname,0)==0)
    return 1;
  if (needsearch)
  {
    _searchenv(fname,"PATH",searchbuf);
    if (searchbuf[0]==0) return 0;
    strcpy(fname,searchbuf);
    return 1;
  }
  return (access(fname,0)==0);
}

static void expand_path(const char * src,char * dest)
{
  /* make full path by src and add extention */
  const char * p;
  int  needsearch,needext;

  p=strrchr(src,'\\');
  if (p==NULL) p=strrchr(src,'/');
  if (p==NULL) p=strchr(src,':');
  if (p==NULL)
  { p=src;
    needsearch=1;
  }
  else
  { p++;
    needsearch=0;
  }
  if (strchr(p,'.'))
    needext=0;
  else
    needext=1;
  if (needext)
  {
    strcpy(dest,src);
    strcat(dest,".exe");
    if (searchfile(dest,needsearch))
      return;
    strcpy(dest,src);
    strcat(dest,".cmd");
    if (searchfile(dest,needsearch))
      return;
  }
  strcpy(dest,src);
  searchfile(dest,needsearch);
}

#define PIPESIZE    4096

int pipe_system(int * in,int * out,char * cmd)
{
  char * args[20];
  char * p;
#ifndef __EMX__
  int  r;
#endif

  p = strdup(cmd);
  checkref(p);
#ifdef __EMX__
  getargs(p, args, sizeof(args)/sizeof(args[0]));
#else
  for (r=0;*p;)
  {
    args[r++]=p;
    p=strpbrk(p," \t");
    if (p==NULL) break;
    *p++='\0';
    while ((*p==' ')||(*p=='\t')) p++;
  }
  args[r]=NULL;
#endif
  return pipe_spawnv(in, out, args[0], args);
}

int createpipe(int * in, int * out)
{
#ifdef __EMX__
  int h[2];

  if (pipe(h)) return -1;
  *in=h[0];
  *out=h[1];
#else
  if (DosCreatePipe((PHFILE)in, (PHFILE)out, PIPESIZE))
  { printmsg(0, "createpipe: DosCreatePipe error");
    return -1;
  }
  _hdopen(*in, O_TEXT|O_RDONLY);
  _hdopen(*out,O_TEXT|O_RDWR);
#endif
  DosSetFHState(*in, OPEN_FLAGS_NOINHERIT);
  DosSetFHState(*out,OPEN_FLAGS_NOINHERIT);
  printmsg(15, "createpipe: pipe created, hin=%d, hout=%d", *in, *out);
  return 0;
}

int pipe_spawnv(int * in,int * out,const char * name, char * args[])
{
  int savein=-1,saveout=-1,saveerr=-1;
  int r,i,rc;
  int hrin,hwin;
  char * p;

  /* do we need exec via comspec? */
  expand_path(name,exename);
  p=strrchr(exename,'.');
  if (p)
    if (stricmp(p,".cmd")==0)
    { for (r=0; args[r]; r++);
      for (;r>=0;r--)
        args[r+2]=args[r];
      args[0]=getenv("COMSPEC");
      if (args[0]==NULL) args[0]="cmd.exe";
      strcpy(exename, args[0]);
      args[1]="/c";
    }
  /* do redirections */
  for (i=0;args[i];i++)
  {
    rc=redirect(args,&i,">&>",fileno(stderr),&saveerr);
    if (rc==-1) return -1;
    if (rc) continue;
    rc=redirect(args,&i,">&",fileno(stderr),&saveerr);
    if (rc==-1)
    { restredir(saveerr,fileno(stderr));
      return -1;
    }
    if (rc) continue;
    rc=redirect(args,&i,">>",out ? -1 : fileno(stdout),&saveout);
    if (rc==-1)
    { restredir(saveerr,fileno(stderr));
      return -1;
    }
    if (rc) continue;
    rc=redirect(args,&i,">",out ? -1 : fileno(stdout),&saveout);
    if (rc==-1)
    { restredir(saveout,fileno(stdout));
      restredir(saveerr,fileno(stderr));
      return -1;
    }
    if (rc) continue;
    rc=redirect(args,&i,"<",in ? -1 : fileno(stdin),&savein);
    if (rc==-1)
    { restredir(saveout,fileno(stdout));
      restredir(saveerr,fileno(stderr));
      return -1;
    }
  }
  if (in)
  { if (createpipe(&hrin, in))
      return -1;
    savein=dup(fileno(stdin));
    dup2(hrin,fileno(stdin));
    close(hrin);
  }
  if (out)
  { if (createpipe(out, &hwin))
    { if (in)
      { dup2(savein,fileno(stdin));
        close(savein);
      }
      return -1;
    }
    fflush(stdout);
    saveout=dup(fileno(stdout));
    dup2(hwin,fileno(stdout));
    close(hwin);
  }
  if (kbdwait!=-1) DosRequestMutexSem(kbdwait, -1);
  r=spawnvp(P_NOWAIT,exename,args);
  restredir(savein,fileno(stdin));
  restredir(saveout,fileno(stdout));
  restredir(saveerr,fileno(stderr));
  return r;
}

int swap_system(const char *str)
{
	char * args[20];
        char * buffer;
        int saveerr=-1, saveout=-1, savein=-1;
        char * p;
        int pid;
        int i;
        int rc;

        buffer=strdup(str);
        checkref(buffer);
#ifdef __EMX__
        getargs(buffer, args, sizeof(args)/sizeof(args[0]));
#else
        for (p=buffer,i=0;*p;)
        { 
          args[i++]=p;
          p=strpbrk(p," \t");
          if (p==NULL) break;
          *p++='\0';
          while ((*p==' ')||(*p=='\t')) p++;
        }
        args[i]=NULL;
#endif
        /* do we need exec via comspec? */
        expand_path(args[0],exename);
        p=strrchr(exename,'.');
        if (p)
          if (stricmp(p,".cmd")==0)
          { for (;i>=0;i--)
              args[i+2]=args[i];
            args[0]=getenv("COMSPEC");
            if (args[0]==NULL) args[0]="cmd.exe";
            args[1]="/c";
          }
        /* do redirections */
        for (i=0;args[i];i++)
        {
          rc=redirect(args,&i,">&",fileno(stderr),&saveerr);
          if (rc==-1)
          {
swpreterr:  free(buffer);
            return -1;
          }
          if (rc) continue;
          rc=redirect(args,&i,">&>",fileno(stderr),&saveerr);
          if (rc==-1)
          { restredir(saveerr,fileno(stderr));
            goto swpreterr;
          }
          if (rc) continue;
          rc=redirect(args,&i,">>",fileno(stdout),&saveout);
          if (rc==-1)
          { restredir(saveerr,fileno(stderr));
            goto swpreterr;
          }
          if (rc) continue;
          rc=redirect(args,&i,">",fileno(stdout),&saveout);
          if (rc==-1)
          { restredir(saveout,fileno(stdout));
            restredir(saveerr,fileno(stderr));
            goto swpreterr;
          }
          if (rc) continue;
          rc=redirect(args,&i,"<",fileno(stdin),&savein);
          if (rc==-1)
          { restredir(saveout,fileno(stdout));
            restredir(saveerr,fileno(stderr));
            goto swpreterr;
          }
        }
        if (kbdwait!=-1) DosRequestMutexSem(kbdwait, -1);
        pid=spawnvp(P_NOWAIT,exename,args);
        if (pid!=-1) {
          cwait(&rc,pid,WAIT_CHILD);
          rc &= 0xffff;
          rc= ((rc << 8) | (rc >> 8)) & 0xffff;
        } else
          rc=-1;
        if (kbdwait!=-1) DosReleaseMutexSem(kbdwait);
        restredir(savein,fileno(stdin));
        restredir(saveout,fileno(stdout));
        restredir(saveerr,fileno(stderr));
        free(buffer);
        return rc;
}
