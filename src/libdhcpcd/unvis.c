/*	$NetBSD: unvis.c,v 1.44 2014/09/26 15:43:36 roy Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#define IN_LIBDHCPCD
#include "dhcpcd.h"

#ifndef __arraycount
#define __arraycount(__x)       (sizeof(__x) / sizeof(__x[0]))
#endif

/*
 * decode driven by state machine
 */
#define	S_GROUND	0	/* haven't seen escape char */
#define	S_START		1	/* start decoding special sequence */
#define	S_META		2	/* metachar started (M) */
#define	S_META1		3	/* metachar more, regular char (-) */
#define	S_CTRL		4	/* control char started (^) */
#define	S_OCTAL2	5	/* octal digit 2 */
#define	S_OCTAL3	6	/* octal digit 3 */
#define	S_HEX		7	/* mandatory hex digit */
#define	S_HEX1		8	/* http hex digit */
#define	S_HEX2		9	/* http hex digit 2 */

#define	isoctal(c)	(((u_char)(c)) >= '0' && ((u_char)(c)) <= '7')
#define	xtod(c)		(isdigit(c) ? (c - '0') : ((tolower(c) - 'a') + 10))

#define _VIS_END        0x0800  /* for unvis */
#define UNVIS_END	_VIS_END

/*
 * unvis return codes
 */
#define	UNVIS_VALID	 1	/* character valid */
#define	UNVIS_VALIDPUSH	 2	/* character valid, push back passed char */
#define	UNVIS_NOCHAR	 3	/* valid sequence, no character produced */
#define	UNVIS_SYNBAD	-1	/* unrecognized escape sequence */
#define	UNVIS_ERROR	-2	/* decoder in unknown state (unrecoverable) */

/*
 * unvis - decode characters previously encoded by vis
 */
static int
unvis(char *cp, char c, int *astate, int flag)
{
	unsigned char uc = (unsigned char)c;
	unsigned char st;

/*
 * Bottom 8 bits of astate hold the state machine state.
 * Top 8 bits hold the current character in the http 1866 nv string decoding
 */
#define GS(a)		((a) & 0xff)
#define SS(a, b)	(((uint32_t)(a) << 24) | (b))
#define GI(a)		((uint32_t)(a) >> 24)

	st = GS(*astate);

	if (flag & UNVIS_END) {
		switch (st) {
		case S_OCTAL2:
		case S_OCTAL3:
		case S_HEX2:
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case S_GROUND:
			return UNVIS_NOCHAR;
		default:
			return UNVIS_SYNBAD;
		}
	}

	switch (st) {

	case S_GROUND:
		*cp = 0;
		if (c == '\\') {
			*astate = SS(0, S_START);
			return UNVIS_NOCHAR;
		}
		*cp = c;
		return UNVIS_VALID;

	case S_START:
		switch(c) {
		case '\\':
			*cp = c;
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			*cp = (c - '0');
			*astate = SS(0, S_OCTAL2);
			return UNVIS_NOCHAR;
		case 'M':
			*cp = (char)0200;
			*astate = SS(0, S_META);
			return UNVIS_NOCHAR;
		case '^':
			*astate = SS(0, S_CTRL);
			return UNVIS_NOCHAR;
		case 'n':
			*cp = '\n';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'r':
			*cp = '\r';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'b':
			*cp = '\b';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'a':
			*cp = '\007';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'v':
			*cp = '\v';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 't':
			*cp = '\t';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'f':
			*cp = '\f';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 's':
			*cp = ' ';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'E':
			*cp = '\033';
			*astate = SS(0, S_GROUND);
			return UNVIS_VALID;
		case 'x':
			*astate = SS(0, S_HEX);
			return UNVIS_NOCHAR;
		case '\n':
			/*
			 * hidden newline
			 */
			*astate = SS(0, S_GROUND);
			return UNVIS_NOCHAR;
		case '$':
			/*
			 * hidden marker
			 */
			*astate = SS(0, S_GROUND);
			return UNVIS_NOCHAR;
		default:
			if (isgraph(c)) {
				*cp = c;
				*astate = SS(0, S_GROUND);
				return UNVIS_VALID;
			}
		}
		goto bad;

	case S_META:
		if (c == '-')
			*astate = SS(0, S_META1);
		else if (c == '^')
			*astate = SS(0, S_CTRL);
		else 
			goto bad;
		return UNVIS_NOCHAR;

	case S_META1:
		*astate = SS(0, S_GROUND);
		*cp |= c;
		return UNVIS_VALID;

	case S_CTRL:
		if (c == '?')
			*cp |= 0177;
		else
			*cp |= c & 037;
		*astate = SS(0, S_GROUND);
		return UNVIS_VALID;

	case S_OCTAL2:	/* second possible octal digit */
		if (isoctal(uc)) {
			/*
			 * yes - and maybe a third
			 */
			*cp = (char)((*cp << 3) + (c - '0'));
			*astate = SS(0, S_OCTAL3);
			return UNVIS_NOCHAR;
		}
		/*
		 * no - done with current sequence, push back passed char
		 */
		*astate = SS(0, S_GROUND);
		return UNVIS_VALIDPUSH;

	case S_OCTAL3:	/* third possible octal digit */
		*astate = SS(0, S_GROUND);
		if (isoctal(uc)) {
			*cp = (char)((*cp << 3) + (c - '0'));
			return UNVIS_VALID;
		}
		/*
		 * we were done, push back passed char
		 */
		return UNVIS_VALIDPUSH;

	case S_HEX:
		if (!isxdigit(uc))
			goto bad;
		/*FALLTHROUGH*/
	case S_HEX1:
		if (isxdigit(uc)) {
			*cp = (char)xtod(uc);
			*astate = SS(0, S_HEX2);
			return UNVIS_NOCHAR;
		}
		/*
		 * no - done with current sequence, push back passed char
		 */
		*astate = SS(0, S_GROUND);
		return UNVIS_VALIDPUSH;

	case S_HEX2:
		*astate = S_GROUND;
		if (isxdigit(uc)) {
			*cp = (char)(xtod(uc) | (*cp << 4));
			return UNVIS_VALID;
		}
		return UNVIS_VALIDPUSH;

	default:
	bad:
		/*
		 * decoder in unknown state - (probably uninitialized)
		 */
		*astate = SS(0, S_GROUND);
		return UNVIS_SYNBAD;
	}
}

/*
 * strnunvisx - decode src into dst
 *
 *	Number of chars decoded into dst is returned, -1 on error.
 *	Dst is null terminated.
 */

int
dhcpcd_strnunvis(char *dst, size_t dlen, const char *src)
{
	char c;
	char t = '\0', *start = dst;
	int state = 0;

#define CHECKSPACE() \
	do { \
		if (dlen-- == 0) { \
			errno = ENOSPC; \
			return -1; \
		} \
	} while (/*CONSTCOND*/0)

	while ((c = *src++) != '\0') {
 again:
		switch (unvis(&t, c, &state, 0)) {
		case UNVIS_VALID:
			CHECKSPACE();
			*dst++ = t;
			break;
		case UNVIS_VALIDPUSH:
			CHECKSPACE();
			*dst++ = t;
			goto again;
		case 0:
		case UNVIS_NOCHAR:
			break;
		case UNVIS_SYNBAD:
			errno = EINVAL;
			return -1;
		default:
			errno = EINVAL;
			return -1;
		}
	}
	if (unvis(&t, c, &state, UNVIS_END) == UNVIS_VALID) {
		CHECKSPACE();
		*dst++ = t;
	}
	CHECKSPACE();
	*dst = '\0';
	return (int)(dst - start);
}
