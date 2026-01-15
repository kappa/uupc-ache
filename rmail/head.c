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

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "lib.h"
#include "pcmail.h"

#ifdef	__TURBOC__
#define	MSDOS
#endif

#define NOSTR NULL

struct headline {
	char    *l_from;        /* The name of the sender */
	char    *l_tty;         /* His tty string (if any) */
	char    *l_date;        /* The entire date string */
};

static void parse(char *line, struct headline *hl, char *pbuf);
static int isdate(char *date);
static char *copyin(char *src, char **space);
static int cmatch(char *str, char *temp);
static char *nextword(char *, char *);
static char *copy(char *str1, char *str2);
static int any(char c, char *f);

boolean isFrom_(char *cp)
{
	return (*cp == 'F' && strncmp("From ", cp, 5) == 0);
}
/*
 * See if the passed line buffer is a mail header.
 * Return true if yes.  Note the extreme pains to
 * accomodate all funny formats.
 */

boolean ishead(char *linebuf)
{
	register char *cp;
	struct headline hl;
	static char parbuf[LINESIZE];

	cp = linebuf;
	if (!isFrom_(cp))
		return(0);
	parse(cp, &hl, parbuf);
	if (hl.l_from == NOSTR || hl.l_date == NOSTR)
		return(0);
	if (!isdate(hl.l_date))
		return(0);

	/*
	 * I guess we got it!
	 */

	return(1);
}

/*
 * Split a headline into its useful components.
 * Copy the line into dynamic string space, then set
 * pointers into the copied line in the passed headline
 * structure.  Actually, it scans.
 */

static void parse(char *line, struct headline *hl, char *pbuf)
{
	register char *cp, *dp;
	char *sp;
	static char word[LINESIZE];

	hl->l_from = NOSTR;
	hl->l_tty = NOSTR;
	hl->l_date = NOSTR;
	cp = line;
	sp = pbuf;

	/*
	 * Skip the first "word" of the line, which should be "From"
	 * anyway.
	 */

	cp = nextword(cp, word);
	dp = nextword(cp, word);
	if (*word != '\0')
		hl->l_from = copyin(word, &sp);
	if (dp != NOSTR) {
		if (strncmp(dp, "tty", 3) == 0) {
			cp = nextword(dp, word);
			hl->l_tty = copyin(word, &sp);
			if (cp != NOSTR)
				hl->l_date = copyin(cp, &sp);
		}
		else
			hl->l_date = copyin(dp, &sp);
	}
}

/*
 * Copy the string on the left into the string on the right
 * and bump the right (reference) string pointer by the length.
 * Thus, dynamically allocate space in the right string, copying
 * the left string into it.
 */

static char *copyin(char *src, char **space)
{
	register char *cp, *top;
	register int s;

	s = strlen(src);
	cp = *space;
	top = cp;
	strcpy(cp, src);
	cp += s + 1;
	*space = cp;
	return(top);
}

/*
 * Test to see if the passed string is a ctime(3) generated
 * date string as documented in the manual.  The template
 * below is used as the criterion of correctness.
 * Also, we check for a possible trailing time zone using
 * the auxtype template.
 */

#define L       1               /* A lower case char */
#define S       2               /* A space */
#define D       3               /* A digit */
#define O       4               /* An optional digit or space */
#define C       5               /* A colon */
#define N       6               /* A new line */
#define U       7               /* An upper case char */
#define Z       8               /* Plus or minus */
#define A       9               /* Alpha char (upper || lower) */
#define B       10              /* Break at this case */

	       /*W e d   D e c   2 5   1 2 : 1 2 : 1 2   {1990,MSK,...}*/
char ctypes[] = {A,L,L,S,A,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,B};

static int isdate(char *date)
{
	register char *cp;

	cp = date;
	return cmatch(cp, ctypes);
}

/*
 * Match the given string against the given template.
 * Return 1 if they match, 0 if they don't
 */

static int cmatch(char *str, char *temp)
{
	register char *cp, *tp;
	register unsigned char c;

	cp = str;
	tp = temp;
	while (*cp != '\0' && *tp != 0) {
		c = *cp++;
		switch (*tp++) {
		case Z:
			if (c != '+' && c != '-')
				return(0);
			break;

		case L:
			if (!islower(c))
				return(0);
			break;

		case U:
			if (!isupper(c))
				return(0);
			break;

		case A:
			if (!isalpha(c))
				return(0);
			break;

		case S:
			if (!isspace(c))
				return(0);
			break;

		case D:
			if (!isdigit(c))
				return(0);
			break;

		case O:
			if (!isspace(c) && !isdigit(c))
				return(0);
			break;

		case C:
			if (c != ':')
				return(0);
			break;

		case N:
			if (c != '\n')
				return(0);
			break;

		case B:
			return(1);
		}
	}
	if (*cp != '\0' || *tp != 0)
		return(0);
	return(1);
}

/*
 * Collect a liberal (space, tab delimited) word into the word buffer
 * passed.  Also, return a pointer to the next word following that,
 * or NOSTR if none follow.
 */

static char *nextword(char *wp, char *wbuf)
{
	register char *cp, *cp2;

	if ((cp = wp) == NOSTR) {
		copy("", wbuf);
		return(NOSTR);
	}
	cp2 = wbuf;
	while (*cp && !any(*cp, " \t")) {
		if (*cp == '"') {
			*cp2++ = *cp++;
			while (*cp != '\0' && *cp != '"')
				*cp2++ = *cp++;
			if (*cp == '"')
				*cp2++ = *cp++;
		} else
			*cp2++ = *cp++;
	}
	*cp2 = '\0';
	while (any(*cp, " \t"))
		cp++;
	if (*cp == '\0')
		return(NOSTR);
	return(cp);
}

/*
 * Copy str1 to str2, return pointer to null in str2.
 */

static char *copy(char *str1, char *str2)
{
	register char *s1, *s2;

	s1 = str1;
	s2 = str2;
	while (*s1)
		*s2++ = *s1++;
	*s2 = 0;
	return(s2);
}

/*
 * Is ch any of the characters in str?
 */

static int any(char c, char *f)
{
	while (*f)
		if (c == *f++)
			return(1);
	return(0);
}

