#ifndef MULTIGAIN_ERRORS_HPP
#define MULTIGAIN_ERRORS_HPP

#include <exception>
#include <string>

namespace multigain {

struct Bad_samplefreq : public std::exception {
	const char *what() const throw ()
	{	return "Bad sample frequency"; }
	virtual ~Bad_samplefreq() throw () {}
};

struct Decode_error : std::exception {
	Decode_error(const std::string &msg) : _msg("Decode error: ")
	{	_msg += msg; }
	~Decode_error() throw () {}
	const char *what() const throw ()
	{	return _msg.c_str(); }
	std::string _msg;
};

/** A read/write/seek/etc. error. */
struct Disk_error : std::exception {
	Disk_error(const std::string &msg) : _msg("Disk error: ")
	{	_msg += msg; }
	~Disk_error() throw () {}
	const char *what() const throw ()
	{	return _msg.c_str(); }
	std::string _msg;
};

struct Mpg123_error : public std::exception {
	Mpg123_error(int errval);
	const char *what() const throw ()
	{	return _msg.c_str(); }
	virtual ~Mpg123_error() throw () {}
	std::string _msg;
};

struct Not_enough_samples : public std::exception {
	const char *what() const throw ()
	{	return "Not enough samples to calculate with"; }
	virtual ~Not_enough_samples() throw () {}
};

/** A tag is unsupported and/or highly questionable
 *
 * There are two cases where this is used.  (1) If a tag version is
 * unsupported (e.g. ID3-2.7) and there are reserved bits set in the header.
 * (2) If some sort of tag appears to exist, but is unrecognizable.
 */
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
