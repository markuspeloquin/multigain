/* Copyright (C) 2010 Markus Peloquin <markus@cs.wisc.edu>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <sstream>

#include <lame/lame.h>

#include <multigain/errors.hpp>
#include "lame.hpp"

namespace {

const char *lame_strerror(int status)
{
	switch (status) {
	case LAME_OKAY:			return "okay";
	case LAME_GENERICERROR:		return "generic error";
	case LAME_NOMEM:		return "no memory";
	case LAME_BADBITRATE:		return "bad bitrate";
	case LAME_BADSAMPFREQ:		return "bad sample frequency";
	case LAME_INTERNALERROR:	return "internal error";

	case FRONTEND_READERROR:	return "[frontend] read error";
	case FRONTEND_WRITEERROR:	return "[frontend] write error";
	case FRONTEND_FILETOOLARGE:	return "[frontend] file too large";

	default:			return "unknown";
	}
}

} // end anon

multigain::Lame_error::Lame_error(const std::string &msg, int errval)
{
	std::ostringstream out;
	out << msg << ": LAME error: " << lame_strerror(errval);
	const std::string &last = Lame_lib::last_error();
	if (!last.empty())
		out << " (" << last << ')';
	_msg = out.str();
}

multigain::Lame_error::Lame_error(int errval)
{
	std::ostringstream out;
	out << "LAME error: " << lame_strerror(errval);
	const std::string &last = Lame_lib::last_error();
	if (!last.empty())
		out << " (" << last << ')';
	_msg = out.str();
}
