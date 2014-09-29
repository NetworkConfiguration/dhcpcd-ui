/*
 * dhcpcd-decode
 * Copyright 2014 Roy Marples <roy@marples.name>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcpcd.h"

static void
process(char *src, ssize_t (*decode)(char *, size_t, const char *))
{
	char *buf;
	size_t buflen;
	ssize_t dl;

	buflen = strlen(src) + 1;
	buf = malloc(buflen);
	if (buf == NULL)
		err(EXIT_FAILURE, "malloc");
	if ((dl = decode(buf, buflen, src)) == -1) {
		free(buf);
		err(EXIT_FAILURE, "decode");
	}
	if (fwrite(buf, 1, (size_t)dl, stdout) != (size_t)dl) {
		free(buf);
		err(EXIT_FAILURE, "fwrite");
	}
	free(buf);
	fputc('\n', stdout);
}

static void
usage(char *progname)
{

	fprintf(stderr, "usage: %s [-sx] [data ...]\n", basename(progname));
}

int
main(int argc, char **argv)
{
	int opt;
	ssize_t (*decode)(char *, size_t, const char *);

	decode = dhcpcd_decode;
	while ((opt = getopt(argc, argv, "sx")) != -1) {
		switch (opt) {
		case 's':
			decode = dhcpcd_decode_shell;
			break;
		case 'x':
			decode = dhcpcd_decode_hex;
			break;
		case '?':
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc && isatty(fileno(stdin))) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	for (; optind < argc; optind++)
		process(argv[optind], decode);

	if (!isatty(fileno(stdin))) {
		char *arg;
		size_t len;
		ssize_t argl;

		arg = NULL;
		len = 0;
		while ((argl = getline(&arg, &len, stdin)) != -1) {
			if (arg[argl - 1] == '\n')
				arg[argl - 1] = '\0';
			process(arg, decode);
		}
		free(arg);
	}

	return EXIT_SUCCESS;
}
