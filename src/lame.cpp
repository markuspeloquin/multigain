#include <cstdio>
#include <iostream>

#include "lame.hpp"

namespace {

extern "C" void
lame_errorf(const char *fmt, va_list ap)
{
	int len;
	if ((len = vsnprintf(0, 0, fmt, ap)) == -1) {
		std::cerr << "vasprintf() failed\n";
		return;
	}

	char *str = new char[len+1];
	if ((len = vsnprintf(str, len+1, fmt, ap)) == -1) {
		std::cerr << "vasprintf() failed\n";
		return;
	}

	multigain::Lame_lib::last_error(str);
	delete[] str;
}

}

void
multigain::Lame_lib::do_init() throw (Lame_error)
{
	int ret;

	if (!(_flags = lame_init()))
		throw Lame_error("initializing library",
		    LAME_NOMEM);

	if ((ret = lame_set_errorf(_flags, lame_errorf)))
		throw Lame_error("setting error function", ret);
}

multigain::Lame_lib multigain::Lame_lib::_instance;
