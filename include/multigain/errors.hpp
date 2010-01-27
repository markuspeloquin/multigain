#ifndef MULTIGAIN_ERRORS_HPP
#define MULTIGAIN_ERRORS_HPP

#include <exception>
#include <string>

namespace multigain {

struct Disk_error : std::exception {
	Disk_error(const std::string &msg) : _msg("Disk error: ")
	{	_msg += msg; }
	~Disk_error() throw () {}
	const char *what() const throw ()
	{	return _msg.c_str(); }
	std::string _msg;
};

struct Unsupported_tag : std::exception {
	Unsupported_tag(const std::string &msg) :
		_msg("Unsupported tag type: ")
	{	_msg += msg; }
	~Unsupported_tag() throw () {}
	const char *what() const throw ()
	{	return _msg.c_str(); }
	std::string _msg;
};

}

#endif
