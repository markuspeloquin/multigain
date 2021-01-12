#ifndef MULTIGAIN_LAME_HPP
#define MULTIGAIN_LAME_HPP

#include <cstdarg>
#include <string>

#include <multigain/errors.hpp>

struct lame_global_struct;

namespace multigain {

class Lame_lib {
public:
	// return type is lame_global_flags*
	/// \throw Lame_error
	static struct lame_global_struct *init() {
		if (!_instance._flags)
			_instance.do_init();
		return _instance._flags;
	}

	// not manditory to call
	/// \throw Lame_error
	static void destroy();

	static void last_error(const char *str) {
		_instance._last_error = str;
	}
	static const std::string &last_error() {
		return _instance._last_error;
	}

private:
	Lame_lib() : _flags(0) {}
	~Lame_lib() noexcept;

	Lame_lib(const Lame_lib &) = delete;
	void operator=(const Lame_lib &) = delete;

	/// \throw Lame_error
	void do_init();

	static Lame_lib _instance;

	struct lame_global_struct	*_flags;
	std::string			_last_error;
};

}

#endif
