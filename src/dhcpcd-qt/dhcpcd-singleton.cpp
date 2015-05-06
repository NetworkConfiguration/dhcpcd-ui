/*
 * dhcpcd-qt
 * Copyright 2014-2015 Roy Marples <roy@marples.name>
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

#include <sys/file.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>
#include <string>

#include "dhcpcd.h"
#include "dhcpcd-singleton.h"

using namespace std;

DhcpcdSingleton::DhcpcdSingleton()
{

	fd = -1;
}

DhcpcdSingleton::~DhcpcdSingleton()
{

}

bool DhcpcdSingleton::lock()
{
	string file;
	const char *display;

	if (mkdir(DHCPCD_TMP_DIR, DHCPCD_TMP_DIR_PERM) == -1 &&
	    errno != EEXIST)
	{
		cerr << "dhcpcd-qt: " << "mkdir: " << DHCPCD_TMP_DIR << ": "
		    << strerror(errno) << endl;
		return false;
	}

	file = DHCPCD_TMP_DIR;
	file += "/dhcpcd-qt-";
	file += getlogin();
	display = getenv("DISPLAY");
	if (display && *display != '\0' && strchr(display, '/') == NULL) {
		file += '.';
		file += display;
	}
	file += ".lock";
	fd = open(file.c_str(), O_WRONLY | O_CREAT | O_NONBLOCK, 0664);
	if (fd == -1) {
		cerr << "dhcpcd-qt: " << "open: " << file << ": "
		    << strerror(errno) << endl;
		return false;
	}
	if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
		if (errno != EAGAIN)
			cerr << "dhcpcd-qt: " << "flock: " << file << ": "
			    << strerror(errno) << endl;
		return false;
	}
	return true;
}
