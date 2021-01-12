#include <cstdio>
#include <iostream>

#include <lame/lame.h>

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

multigain::Lame_lib::~Lame_lib() noexcept
{
	try {
		destroy();
	} catch (const Lame_error &e) {
		std::cerr << "destroying Lame_lib: " << e.what() << '\n';
	}
}

void
multigain::Lame_lib::destroy() throw (Lame_error)
{
	int ret;
	if (_instance._flags) {
		ret = lame_close(_instance._flags);
		if (ret)
			throw Lame_error("destroying library", ret);
		_instance._flags = 0;
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

	if (lame_init_params(_flags) == -1) {
		try {
			destroy();
		} catch (...) {
		}
		throw Lame_error("initializing parameters", -1);
	}
}

multigain::Lame_lib multigain::Lame_lib::_instance;
