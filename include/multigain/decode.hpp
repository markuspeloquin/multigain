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

#ifndef MULTIGAIN_DECODE_HPP
#define MULTIGAIN_DECODE_HPP

#include <fstream>
#include <tr1/cstdint>

#include <multigain/errors.hpp>

struct mpg123_handle_struct;

namespace multigain {

/** Meta information of a set of samples */
struct decode_info {
	uint16_t	frequency;
	uint8_t		channels;
};

/** MPEG audio frame-by-frame decoder */
class Mpeg_decoder {
public:
	/** The most samples returned by a call to
	 * <code>decode_frame()</code>. */
	const static size_t MAX_SAMPLES = 1152;

	/** Create a decoder that operates on some region of a file
	 *
	 * \param file	The opened MPEG audio file
	 * \param media_begin	The offset of the starting MPEG audio frame
	 * \param media_end	The offset of the first byte following the
	 *	final frame (currently ignored, the decoding library is
	 *	depended on to detect this)
	 * \throw Disk_error	Problem seeking to <code>media_begin</code>
	 * \throw Mpg123_error	The mpg123 library has some error
	 */
	Mpeg_decoder(std::ifstream &file, off_t media_begin, off_t media_end)
	    throw (Disk_error, Mpg123_error);
	~Mpeg_decoder();

	/** Decode a single frame
	 *
	 * \param[out] left	Samples of the stereo-left or mono channel
	 * \param[out] right	Samples of the stereo-right channel
	 * \param[out] info	Optional.  Basic information about the samples
	 *	just returned
	 * \throw Decode_error	So far, this really should not happen
	 * \throw Disk_error	Seek or read error
	 * \throw Mpg123_error	The mpg123 library has some error
	 * \return	Input bytes consumed, samples decoded
	 */
	std::pair<size_t, size_t> decode_frame(
	    double left[MAX_SAMPLES], double right[MAX_SAMPLES],
	    struct decode_info *info=0)
	    throw (Decode_error, Disk_error, Mpg123_error);

private:
	static const size_t MAX_FRAME = 6921;

	size_t next_frame(uint8_t[MAX_FRAME]) throw (Disk_error);

	Mpeg_decoder(const Mpeg_decoder &_) : _file(_._file)
	{ throw std::exception(); }
	void operator=(const Mpeg_decoder &)
	{ throw std::exception(); }

	std::ifstream			&_file;
	off_t				_pos;
	off_t				_end;
	struct mpg123_handle_struct	*_hdl;

	uint32_t	_last_frequency;
	uint8_t		_last_channels;
};

}

#endif
