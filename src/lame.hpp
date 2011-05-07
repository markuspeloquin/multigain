#ifndef MULTIGAIN_LAME_HPP
#define MULTIGAIN_LAME_HPP

#include <cstdarg>
#include <string>

#include <lame/lame.h>

#include <multigain/errors.hpp>

namespace multigain {

class Lame_lib {
public:
	static lame_global_flags *init() throw (Lame_error)
	{
		if (!_instance._flags)
			_instance.do_init();
		return _instance._flags;
	}
	// not manditory to call
	static void destroy() throw (Lame_error)
	{
		int ret;
		if (_instance._flags) {
			ret = lame_close(_instance._flags);
			if (ret)
				throw Lame_error("destroying library", ret);
			_instance._flags = 0;
		}
	}


	static void last_error(const char *str)
	{
		_instance._last_error = str;
	}
	static const std::string &last_error()
	{
		return _instance._last_error;
	}

private:
	Lame_lib() : _flags(0) {}
	~Lame_lib()
	{
		int ret;
		if (_instance._flags)
			ret = lame_close(_instance._flags);
	}

	Lame_lib(const Lame_lib &) {}
	void operator=(const Lame_lib &) {}

	void do_init() throw (Lame_error);

	static Lame_lib _instance;

	lame_global_flags	*_flags;
	std::string		_last_error;
};

}

#endif
