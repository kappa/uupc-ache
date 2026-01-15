/*-
 * Copyright (C) 1992-1996 by Andrey A. Chernov, Moscow, Russia.
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
#include "uupcmoah.h"

#include "swap.h"

int swap_flags;
unsigned swap_needed;

int configure_swap(void)
{
   char *cp;
   int i = 0;

   for (cp = swapto; cp != NULL && *cp; cp++)
	switch (toupper(*cp)) {
	case 'D':
		i |= USE_FILE|CHECK_NET|HIDE_FILE;
		break;
	case 'E':
		i |= USE_EMS;
		break;
	case 'X':
		i |= USE_XMS;
		if (!(i & USE_EMS))
			i |= XMS_FIRST;
		break;
	default:
		printmsg(0, "configure_swap: %c -- unknown SwapTo value, possible values are E,X,D", *cp);
		return 0;
	}
   if (i == 0) {
	swapto = NULL;
	swap_flags = USE_ALL|CHECK_NET|HIDE_FILE;
	swap_needed = 0;
   }
   else {
	swap_flags = i;
	swap_needed = 0xffff;
   }
   return 1;
}

int swap_spawnv(const char *name, char * const args[])
{
	char buf[256];
	char * const *ss;
	int rc;

	*buf = '\0';
	for (ss = args + 1; *ss; ss++) {
		if (strlen(*ss) + strlen(buf) + 1 > 128) {
			printmsg(0, "swap_spawnv: %s: arguments list too long", name);
			return -1;
		}
		if (ss > args + 1)
			strcat(buf, " ");
		strcat(buf, *ss);
	}
	rc = do_exec((char *)name, buf, swap_flags, swap_needed, environ);
	if (rc < 0x100)
		return rc;
	swaperror(name, rc);
	return -1;
}

int swap_system(const char *str)
{
	char buf[256];
	char *s, *p;
	int rc;

	if (strlen(str) >= sizeof(buf)) {
		printmsg(0, "swap_system: command line too long");
		return -1;
	}
	strcpy(buf, str);
	if ((s = strchr(buf, ' ')) != NULL)
		*s++ = '\0';
	else
		s = "";
	p=strpbrk(s,"<>");
	if (p == NULL)
		p = s+strlen(s);
	if (p-s > 127) {
		printmsg(0, "swap_system: %s: arguments list too long", buf);
		return -1;
	}
	rc = do_exec(buf, s, swap_flags, swap_needed, environ);
	if (rc < 0x100)
		return rc;
	swaperror(buf, rc);
	return -1;
}

void swaperror(const char *name, int rc)
{
	char *s = NULL;
	int err;

	switch(rc) {
	case 0x0101:       s="Error preparing for swap: no space for swapping";break;
	case 0x0102:       s="Error preparing for swap: program too low in memory";break;
	case 0x0200:       s="Program file not found";break;
	case 0x0201:       s="Program file: Invalid drive";break;
	case 0x0202:       s="Program file: Invalid path";break;
	case 0x0203:       s="Program file: Invalid name";break;
	case 0x0204:       s="Program file: Invalid drive letter";break;
	case 0x0205:       s="Program file: Path too long";break;
	case 0x0206:       s="Program file: Drive not ready";break;
	case 0x0207:       s="Batchfile/COMMAND: COMMAND.COM not found";break;
	case 0x0208:       s="Error allocating temporary buffer";break;
	case 0x0400:       s="Error allocating environment buffer";break;
	case 0x0500:       s="Swapping requested, but prep_swap has not";break;
	case 0x0501:       s="MCBs don't match expected setup";break;
	case 0x0502:       s="Error while swapping out";break;
	case 0x0600:       s="Redirection syntax error";break;
	}
	if (s == NULL) {
		err = rc & 0xff;
		switch (rc & 0xff00) {
		case 0x0300:
			printmsg(0, "swaperror: %s: DOS error %02Xh calling EXEC", name, err);
			break;
		case 0x0600:
			printmsg(0, "swaperror: %s: DOS error %02Xh on redirection", name, err);
			break;
		default:
			printmsg(0, "swaperror: %s: unknown error %04Xh", name, rc);
			break;
		}
	}
	else
		printmsg(0, "swaperror: %s: %s", name, s);
}
