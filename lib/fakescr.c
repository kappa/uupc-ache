/*-
 * Copyright (C) 1992-1994 by Andrey A. Chernov, Moscow, Russia.
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

#include "uupcmoah.h"
#include "screen.h"

#pragma off (unreferenced);
int screen = 0;
void Srestoreplace(void){}
#pragma argsused
void Ssaveplace(int a){}
#pragma argsused
void Serror(const char *fmt, ...){}
#pragma argsused
void Slink(Slmesg a, const char *b){}
#pragma argsused
void Smodem(Smdmesg a, int b, int c, int d){}
#pragma argsused
void Smisc(const char *a,...){}
#pragma argsused
void Swputs(int a, const char *b){}
#pragma argsused
void Saddbytes(long a, Sfmesg b){}
#pragma argsused
void Sftrans(Sfmesg a, const char *b, const char *c, int d){}
void Sclear(void){}
#pragma argsused
void Sheader(const char *a, boolean b){}
#pragma argsused
void Spacket(int a, char b, int c){}
#pragma argsused
void Spakerr(int a){}
#pragma argsused
void Stoterr(int a){}
void SIdleIndicator(void) {}
#pragma on (unreferenced);
