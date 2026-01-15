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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "lib.h"
#include "hlib.h"
#include "pcmail.h"

extern char *extcharset;
unsigned char ext2pc[128];
unsigned char pc2ext[128];
unsigned char raw_table[128];
struct charset_struct chartab[sizeof(charset)/sizeof(charset[0])+3];
char * charset[9];

int init_charset(boolean rmail)
{
	FILE *fp;
	int  i, j, k;
	char *p;

	for (i=0; i<128; i++) raw_table[i] = i | 0x80;

	if (extcharset == NULL || !*extcharset) {
		printmsg(0, "init_charset: EXTCHARSET variable must be set");
		return 0;
	}
	if (rmail) {
		if (extsetname == NULL || !*extsetname) {
			printmsg(0, "init_charset: EXTSETNAME variable must be set");
			return 0;
		}
		if (intsetname == NULL || !*intsetname) {
			printmsg(0, "init_charset: INTSETNAME variable must be set");
			return 0;
		}
        }
	if ((fp = FOPEN(extcharset, "r", BINARY)) == nil(FILE)) {
		printerr("init_charset", extcharset);
		printmsg(0, "init_charset: Cannot open charset conversion file %s", extcharset);
		return 0;
	}
	if (fread(ext2pc, sizeof(ext2pc), 1, fp) != 1) {
		printmsg(0, "init_charset: Error reading ext2pc charset table");
		return 0;
	}
	if (fread(pc2ext, sizeof(pc2ext), 1, fp) != 1) {
		printmsg(0, "init_charset: Error reading pc2ext charset table");
		return 0;
	}
	fclose(fp);

	chartab[0].charsetname = intsetname;
	chartab[0].table = raw_table;
	chartab[0].exttable = pc2ext;
	chartab[1].charsetname = extsetname;
	chartab[1].table = ext2pc;
	chartab[1].exttable = raw_table;

	for (i=0, k=2; i<sizeof(charset)/sizeof(charset[0]); i++) {
		if (charset[i] == NULL) continue;
		for (p=charset[i]; *p && !isspace(*p); p++);
		if (*p == 0) {
			printmsg(0, "Incorrect charset setting: %s", charset[i]);
			continue;
		}
		*p++='\0';
		while (isspace(*p)) p++;
		for (j=0; j<k; j++)
			if (stricmp(chartab[j].charsetname, charset[i]) == 0)
				break;
		if (j<k) continue;
		chartab[k].charsetname = charset[i];
		chartab[k].fname = p;
		k++;
	}
	chartab[k].charsetname = NULL;

	return 1;
}

void xt_ext(unsigned char* line) {
	while(*line) {
		if (*line & 0x80)
			*line = pc2ext[*line & 0x7F];
		line++;
	}
}

void xf_ext(unsigned char* line) {
	while(*line) {
		if (*line & 0x80)
			*line = ext2pc[*line & 0x7F];
		line++;
	}
}

