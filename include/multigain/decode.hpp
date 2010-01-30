#ifndef MULTIGAIN_DECODE_HPP
#define MULTIGAIN_DECODE_HPP

#include <fstream>
#include <tr1/cstdint>

#include <multigain/errors.hpp>

struct mpg123_handle_struct;

namespace multigain {

struct decode_info {
	uint32_t	bps;
	uint16_t	frequency;
	bool		stereo;
};

class Mpeg_decoder {
public:
	const static size_t MAX_FRAMES = 1152;
	Mpeg_decoder(std::ifstream &file, off_t media_begin, off_t media_end)
	    throw (Disk_error, Mpg123_error);
	~Mpeg_decoder();

	std::pair<size_t, size_t> decode_frame(
	    double left[MAX_FRAMES], double right[MAX_FRAMES],
	    struct decode_info *info=0)
	    throw (Decode_error, Disk_error, Mpg123_error);

private:
	Mpeg_decoder(const Mpeg_decoder &_) : _file(_._file)
	{ throw std::exception(); }
	void operator=(const Mpeg_decoder &)
	{ throw std::exception(); }

	std::ifstream			&_file;
	off_t				_pos;
	off_t				_end;
	struct mpg123_handle_struct	*_hdl;
};

}

#endif
