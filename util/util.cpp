/**
 * \file
 * \brief Contains miscellaneous functions that don't belong to any particular
 *        subsystem of UQBT.
 *
 * \authors
 * Copyright (C) 2000-2001, The University of Queensland
 * \authors
 * Copyright (C) 2001, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2002, Trent Waddington
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "util.h"

#include <unistd.h>
#include <fcntl.h>

#include <iomanip>      // For std::setw, std::setfill
#include <string>
#include <sstream>

#include <cctype>
#include <cstdio>
#include <cstring>

/**
 * \brief           Append an int to a string.
 * \param[in] s     The string to append to.
 * \param[in] i     The integer whose ascii representation is to be appended.
 * \returns         A copy of the modified string.
 */
std::string
operator +(const std::string &s, int i)
{
	static char buf[50];
	std::string ret(s);

	sprintf(buf, "%d", i);
	return ret.append(buf);
}

int
lockFileRead(const std::string &fname)
{
	int fd = open(fname.c_str(), O_RDONLY);  /* get the file descriptor */
	struct flock fl;
	fl.l_type   = F_RDLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
	fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
	fl.l_start  = 0;        /* Offset from l_whence         */
	fl.l_len    = 0;        /* length, 0 = to EOF           */
	fl.l_pid    = getpid(); /* our PID                      */
	fcntl(fd, F_SETLKW, &fl);  /* set the lock, waiting if necessary */
	return fd;
}

int
lockFileWrite(const std::string &fname)
{
	int fd = open(fname.c_str(), O_WRONLY);  /* get the file descriptor */
	struct flock fl;
	fl.l_type   = F_WRLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
	fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
	fl.l_start  = 0;        /* Offset from l_whence         */
	fl.l_len    = 0;        /* length, 0 = to EOF           */
	fl.l_pid    = getpid(); /* our PID                      */
	fcntl(fd, F_SETLKW, &fl);  /* set the lock, waiting if necessary */
	return fd;
}

void
unlockFile(int fd)
{
	struct flock fl;
	fl.l_type   = F_UNLCK;  /* tell it to unlock the region */
	fcntl(fd, F_SETLK, &fl); /* set the region to unlocked */
	close(fd);
}

void
escapeXMLChars(std::string &s)
{
	std::string bad = "<>&";
	const char *replace[] = { "&lt;", "&gt;", "&amp;" };
	for (std::string::size_type i = 0; i < s.size(); ++i) {
		std::string::size_type n = bad.find(s[i]);
		if (n != bad.npos) {
			s.replace(i, 1, replace[n]);
		}
	}
}

/**
 * \brief           Turn things like newline, return, tab into \\n, \\r, \\t etc.
 * \note            Assumes a C or C++ back end...
 */
char *
escapeStr(const char *str)
{
	std::ostringstream out;
	const char unescaped[] = "ntvbrfa\"";
	const char escaped[] = "\n\t\v\b\r\f\a\"";
	bool escapedSucessfully;

	// test each character
	for (; *str; ++str) {
		if (isprint(*str) && *str != '\"') {
			// it's printable, so just print it
			out << *str;
		} else { // in fact, this shouldn't happen, except for "
			// maybe it's a known escape sequence
			escapedSucessfully = false;
			for (size_t i = 0; escaped[i] && !escapedSucessfully; ++i) {
				if (*str == escaped[i]) {
					out << "\\" << unescaped[i];
					escapedSucessfully = true;
				}
			}
			if (!escapedSucessfully) {
				// it isn't so just use the \xhh escape
				out << "\\x" << std::hex << std::setfill('0') << std::setw(2) << (int)*str;
				out << std::setfill(' ');
			}
		}
	}

	auto ret = new char[out.str().size() + 1];
	strcpy(ret, out.str().c_str());
	return ret;
}
