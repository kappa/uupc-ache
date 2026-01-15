/*-
 * Copyright (C) 1992-1994 by Pavel Gulchouck, Kiev, Ukraine.
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

/* temporary files or buffers working */

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <io.h>
#include <errno.h>
#include "lib.h"
#include "hlib.h"
#include "tio.h"

static char buf[2048]; /* for bprintf */

static int mem2file(TFILE * what)
{
	FILE * f;

	what->name=mktempname(NULL, "TMP");
	f=fopen(what->name, "wb+");
	if (f == NULL) {
		printerr("mem2file", what->name);
		free(what->name);
		return 1;
	}
	if (fwrite(what->mem, 1, strlen(what->mem), f) != strlen(what->mem)) {
		fclose(f);
		unlink(what->name);
		free(what->name);
		return 1;
	}
	free(what->mem);
	what->file = f;
	what->incore = FALSE;
	return 0;
}

char * tfgets(char * buf, int bufsize, TFILE * from)
{
	int i;

	if (from->incore) {
		for (i=0; i<bufsize-1;) {
			if (from->mem[from->curpos] == '\0')
				break;
			buf[i++] = from->mem[from->curpos++];
			if (buf[i-1]=='\n')
				break;
		}
		if (i == 0) return NULL;
		buf[i] = '\0';
		return buf;
	}
	return fgets(buf, bufsize, from->file);
}

int tfprintf(TFILE * to, char * format, ...)
{
	va_list arg;
	int r;

	va_start(arg,format);
	if (to->incore) {
		vsprintf(buf, format, arg);
		if (to->curpos+strlen(buf) >= TBUFSIZE) {
			if (mem2file(to)) return EOF;
		} else {
			strcpy(to->mem+to->curpos, buf);
			to->curpos+=strlen(buf);
			va_end(arg);
			return strlen(buf);
		}
	}
	r = vfprintf(to->file, format, arg);
	va_end(arg);
	return r;
}

int tfputs(char * line, TFILE * to)
{
	if (to->incore) {
		if (to->curpos+strlen(line) >= TBUFSIZE) {
			if (mem2file(to)) return EOF;
		} else {
			strcpy(to->mem+to->curpos, line);
			to->curpos+=strlen(line);
			return strlen(line);
		}
	}
	return fputs(line, to->file);
}

int tfputc(int c, TFILE * to)
{
	if (to->incore) {
		if (to->curpos+1 >= TBUFSIZE) {
			if (mem2file(to)) return EOF;
		} else {
			to->mem[to->curpos++] = (char)c;
			to->mem[to->curpos] = '\0';
			return c;
		}
	}
	return fputc(c, to->file);
}

TFILE * tfopen(void)
{
	TFILE * b;

	b = malloc(sizeof(TFILE));
	if (b == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	b->curpos = 0;
	b->incore = TRUE;
	b->mem = malloc(TBUFSIZE);
	if (b->mem == NULL) {
		b->mem = strdup("");	/* to satisfy free */
		if (b->mem == NULL) {
			free(b);
			errno = ENOMEM;
			return NULL;
		}
		if (mem2file(b)) {
			free(b);
			return NULL;
		}
	}
	return b;
}

int tfclose(TFILE * what)
{
	if (!what->incore)
		return fclose(what->file);
	free(what->mem);
	return 0;
}

int tunlink(TFILE * what)
{
	if (!what->incore) {
		unlink(what->name);
		free(what->name);
	}
	free(what);
	return 0;
}

void trewind(TFILE * what)
{
	if (what->incore)
		what->curpos = 0;
	else
		rewind(what->file);
}

void tfseek(TFILE * what, long offset)
{
	if (what->incore)
		what->curpos = (unsigned)offset;
	else
		fseek(what->file, offset, SEEK_SET);
}

long tftell(TFILE * what)
{
	if (what->incore)
		return what->curpos;
	else
		return ftell(what->file);
}
