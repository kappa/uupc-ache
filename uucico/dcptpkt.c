/*-
 * Copyright (C) 1994 by Andrey A. Chernov, Moscow, Russia.
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
#include <time.h>

#ifdef __MSDOS__
#include "tcp.h"
#else
#include <utils.h>
#endif

#include "lib.h"
#include "dcptpkt.h"
#include "dcp.h"
#include "ulib.h"
#include "hostable.h"
#include "screen.h"

#define TMSGTIME 	120

#define TPACKSIZE	512

int topenpk(const boolean master)
{
   boolean m = master;	/* Satisfy compiler */

   m = m;
   pktsize = 1024;
   return OK;
}

int tclosepk(void)
{
   return OK;
}

int twrmsg(char *str)
{
	char bufr[MAX_PKTSIZE];
	register char *s = bufr;
	int len, slen, ret;

	while (*str)
		*s++ = *str++;
	if (*(s-1) == '\n')
		s--;
	*s = '\0';
	len = strlen(bufr);
	slen = (len / TPACKSIZE + 1) * TPACKSIZE;
	if (slen > sizeof(bufr)) {
		printmsg(0, "twrmsg: write buffer overflows");
		return FAILED;
	}
	memset(s, 0, slen - len);
	ret = swrite(bufr, slen);
	if (ret == S_LOST)
		return ret;
	if (ret < slen)
		return FAILED;
	return OK;
}

int trdmsg(char *str)
{
	int ret;
	char *s;

	for (s = str; s - str <= MAX_PKTSIZE - TPACKSIZE; ) {
		ret = sread(s, TPACKSIZE, TMSGTIME);
		if (ret == S_LOST)
			return ret;
		if (ret < TPACKSIZE)
			return FAILED;
		s += TPACKSIZE;
		if (*(s - 1) == '\0')
			return OK;
	}
	printmsg(0, "trdmsg: read buffer overflows");
	return FAILED;
}

/*
 *	htonl is a function that converts a long from host
 *		order to network order
 *	ntohl is a function that converts a long from network
 *		order to host order
 *
 *	network order is 		0 1 2 3 (bytes in a long)
 *	host order on a vax is		3 2 1 0
 *	host order on a pdp11 is	1 0 3 2
 *	host order on a 68000 is	0 1 2 3
 *	most other machines are		0 1 2 3
 */

int tgetpkt(char *packet, int *bytes)
{
	long Nbytes;
	int len;

	*bytes = 0;
	*packet = '\0';
	len = sread((char *)&Nbytes, sizeof(Nbytes), TMSGTIME);
	if (len == S_LOST)
		return len;
	if (len < sizeof(Nbytes))
		return FAILED;
	*bytes = (int) ntohl(Nbytes);
	if (*bytes == 0)
		return OK;
	if (*bytes > pktsize) {
		printmsg(0, "tgetpkt: packet too long");
		return FAILED;
	}
	len = sread(packet, *bytes, TMSGTIME);
	if (len == S_LOST)
		return len;
	if (len < *bytes)
		return FAILED;
	Spacket(0, 'D', 1);
	printmsg(7, "tgetpkt: data(%d)=|%.*s|", *bytes, *bytes , packet);
	return OK;
}

int tsendpkt(char *ip, int len)
{
	int ret;
	long nbytes;

	if (len > pktsize) {
		printmsg(0, "tsendpkt: packet too long");
		return FAILED;
	}
	nbytes = htonl((long)len);
	ret = swrite((char *)&nbytes, sizeof(nbytes));
	if (ret == S_LOST)
		return len;
	if (ret < sizeof(nbytes))
		return FAILED;
	ret = swrite(ip, len);
	if (ret == S_LOST)
		return ret;
	if (ret < len)
		return FAILED;
	Spacket(1, 'D', 1);
	return OK;
}

int tfilepkt( void)
{
   return OK;
}

int teofpkt( void )
{
	switch (tsendpkt("", 0)) {  /* Empty packet == EOF              */
		case S_LOST:
			return S_LOST;
		case OK:
			return OK;
		default:
			return FAILED;
	}
}

