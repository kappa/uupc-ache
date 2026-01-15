/*-
 * Copyright (C) 1992-2001 by Andrey A. Chernov, Moscow, Russia.
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
#include <stdlib.h>
#include <time.h>
#include <share.h>
#include "lib.h"
#include "hlib.h"
#include "address.h"
#include "pcmail.h"

currentfile();

static char * altdate(void);
static void putaddress(char *,char *,char *,long, char *);

static FILE * F;

boolean
init_stat(void) {
	static char buf[FILENAME_MAX];

	if (!*buf)
		mkfilename(buf,spooldir,"mailstat");
	if ((F = _fsopen(buf,"at",SH_DENYNO)) == nil(FILE)) {
		printerr("init_stat", buf);
		printmsg(0,"init_stat: can't open stat file %s",buf);
		return FALSE;
	}
	fd_lock(buf, fileno(F));
	return TRUE;
}

void write_stat(fromuser,fromnode,addrlist,hispath,sz)
char ** addrlist, * fromuser, * fromnode, * hispath;
long sz;
{
	char *s, *pd;
	char user[MAXADDR];
	boolean NotSame = FALSE;

	if (!init_stat())
		return;

	if ((s = strchr(fromuser, '!')) != NULL) {
		*s = '\0';
		NotSame = (strcmp(nodename, fromuser) != 0);
		*s = '!';
	}
	else
		NotSame = TRUE;
	if (NotSame) {
		char *subway = NULL;

		if ((s = strchr(fromuser, '@')) != NULL) {
			if (strchr(fromuser, '%') == NULL)
				*s = '%';
			else {
				*s = '\0';
				subway = s + 1;
			}
		}
		sprintf(user, "%s!%s%s%s", fromnode,
				subway != NULL ? subway : "",
				subway != NULL ? "!" : "",
				fromuser);
		if (subway != NULL)
			*s = '@';
	}
	else
		strcpy(user, fromuser);
	pd = altdate();
	for (; *addrlist; addrlist++) {
		putaddress(user, *addrlist, hispath, sz, pd);
	}

	fclose(F);
}

static
void putaddress(fromuser,user,hispath,sz,pd)
char * user, * fromuser, * hispath, *pd;
long sz;
{
	char *s, *quser;
	boolean NotSame = TRUE;

	fprintf(F,"%s\t",fromuser);
	if (remote_address(user) == D_REMOTE) {
		if ((s = strchr(user, '!')) != NULL) {
			*s = '\0';
			NotSame = (strcmp(hispath, user) != 0);
			*s = '!';
		}
	}
	quser = calloc(strlen(user)*2 + 3, 1);
	checkref(quser);
	qstrcat(quser, user);
	if (NotSame) {
		char *subway = NULL;

		if ((s = strchr(quser, '@')) != NULL) {
			if (strchr(quser, '%') == NULL)
				*s = '%';
			else {
				*s = '\0';
				subway = s + 1;
			}
		}
		fprintf(F, "%s!%s%s%s\t%ld\t%s\n", hispath,
				subway != NULL ? subway : "",
				subway != NULL ? "!" : "",
				quser,sz,pd);
		if (subway != NULL)
			*s = '@';
	}
	else
		fprintf(F,"%s\t%ld\t%s\n",quser,sz,pd);
	free(quser);
}

static
char *altdate(void)
{
	static char dout[40];
	time_t now;
	struct tm *t;

	time(&now);
	t = localtime(&now);
	sprintf(dout, "%02d:%02d:%02d %02d.%02d.%04d",
			 t->tm_hour, t->tm_min, t->tm_sec,
			 t->tm_mday, t->tm_mon+1, t->tm_year + 1900 );
	return dout;
}
