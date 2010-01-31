#include <mpg123.h>

#include <multigain/errors.hpp>

multigain::Mpg123_error::Mpg123_error(int errval) : _msg("mpg123 error: ")
{
	_msg += mpg123_plain_strerror(errval);
}
