#ifndef MULTIGAIN_LAME_HPP
#define MULTIGAIN_LAME_HPP

#include <cstdarg>
#include <string>

#include <multigain/errors.hpp>

struct lame_global_struct;

namespace multigain {

class Lame_lib {
public:
	static struct lame_global_struct *init() throw (Lame_error)
	{
		if (!_instance._flags)
			_instance.do_init();
		return _instance._flags;
	}
	// not manditory to call
	static void destroy() throw (Lame_error);

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
	~Lame_lib();

	Lame_lib(const Lame_lib &) {}
	void operator=(const Lame_lib &) {}

	void do_init() throw (Lame_error);

	static Lame_lib _instance;

	struct lame_global_struct	*_flags;
	std::string			_last_error;
};

}

#endif
