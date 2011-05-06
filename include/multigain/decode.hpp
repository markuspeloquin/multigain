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

#include <boost/scoped_array.hpp>

#include <multigain/errors.hpp>

struct mpg123_handle_struct;

namespace multigain {

class Frame {
public:
	Frame() :
		_samples(),
		_sample_ptrs(),
		_len(0),
		_frameno(0),
		_freq(0),
		_chan(0)
	{}

	void init(size_t len, uint8_t channels, int16_t freq)
	{
		assert(len && channels && freq);
		init_buf(len, channels);
		_freq = freq;
	}

private:
	void init_buf(size_t len, uint8_t channels)
	{
		if (len == _len && channels == _chan) return;

		size_t total = len * channels;
		size_t old_total = _len * _chan;

		boost::scoped_array<int16_t> new_samples;
		int16_t *samples;
		if (total != old_total) {
			new_samples.reset(new int16_t[total]);
			samples = new_samples.get();
		} else
			samples = _samples.get();

		boost::scoped_array<int16_t *> new_sample_ptrs;
		int16_t **sample_ptrs;
		if (channels != _chan) {
			new_sample_ptrs.reset(new int16_t *[channels]);
			sample_ptrs = new_sample_ptrs.get();
		} else
			sample_ptrs = _sample_ptrs.get();

		size_t off = 0;
		for (size_t i = 0; i < channels; i++) {
			sample_ptrs[i] = samples + off;
			off += len;
		}

		if (new_sample_ptrs.get())
			boost::swap(_sample_ptrs, new_sample_ptrs);
		if (new_samples.get())
			boost::swap(_samples, new_samples);
		_len = len;
		_chan = channels;
	}

public:
	int16_t **samples()
	{
		return _sample_ptrs.get();
	}
	const int16_t *const *samples() const
	{
		return _sample_ptrs.get();
	}
	uint32_t frameno() const
	{
		return _frameno;
	}
	void frameno(uint32_t frameno)
	{
		_frameno = frameno;
	}
	size_t len() const
	{
		return _len;
	}
	uint16_t frequency() const
	{
		return _freq;
	}
	uint8_t channels() const
	{
		return _chan;
	}

	boost::scoped_array<int16_t>	_samples;
	boost::scoped_array<int16_t *>	_sample_ptrs;
	size_t		_len;
	uint32_t	_frameno;
	uint16_t 	_freq;
	uint8_t 	_chan;
};

class Decoder {
public:
	virtual ~Decoder() {}

	virtual std::pair<size_t, size_t> decode_frame(Frame *)
	    throw (Decode_error, Disk_error) = 0;
};

/** MPEG audio frame-by-frame decoder */
class Mpeg_decoder : public Decoder{
public:
	const static uint16_t MAX_SAMPLES = 1152;

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
	std::pair<size_t, size_t> decode_frame(Frame *)
	    throw (Decode_error, Disk_error);

private:
	Mpeg_decoder(const Mpeg_decoder &_) : _file(_._file)
	{ throw std::exception(); }
	void operator=(const Mpeg_decoder &)
	{ throw std::exception(); }

	// 448 kbps, 8 kHz, padded
	static const size_t MAX_FRAME_LEN = 8065;

	size_t next_frame(uint8_t[MAX_FRAME_LEN]) throw (Disk_error);

	std::ifstream			&_file;
	off_t				_pos;
	off_t				_end;
	struct mpg123_handle_struct	*_hdl;
};

class Mpeg_frame_header {
public:
	struct Bad_header : std::exception {
	};

	enum version_type {
		VERS_2_5 = 0,
		VERS_RESERVED = 1,
		VERS_2 = 2,
		VERS_1 = 3
	};
	enum layer_type {
		LAYER_RESERVED = 0,
		LAYER_3 = 1,
		LAYER_2 = 2,
		LAYER_1 = 3
	};
	enum channel_mode_type {
		CHAN_STEREO = 0,
		CHAN_JOINT_STEREO = 1,
		CHAN_DUAL = 2,
		CHAN_MONO = 3
	};
	enum emphasis_type {
		EMPH_NONE = 0,
		EMPH_50_15_MS = 1,
		EMPH_RESERVED = 2,
		EMPH_CCIT_J17 = 3
	};

	// must call init() before anything else
	Mpeg_frame_header() {}

	Mpeg_frame_header(const uint8_t header[4], bool minimal=false)
	    throw (Bad_header)
	{
		init(header, minimal);
	}

	// not exception-safe
	void init(const uint8_t[4], bool minimal=false) throw (Bad_header);

	enum version_type version() const
	{	return _version; }

	enum layer_type layer() const
	{	return _layer; }

	bool has_crc() const
	{	return _has_crc; }

	uint32_t bitrate() const
	{	return _bitrate; }

	uint16_t sampling_rate() const
	{	return _frequency; }

	bool padded() const
	{	return _padded; }

	bool priv() const
	{	return _priv; }

	enum channel_mode_type channel_mode() const
	{	return _chan_mode; }

	unsigned channels() const
	{	return _chan_mode == CHAN_MONO ? 1 : 2; }

	std::pair<uint8_t, uint8_t> intensity_bands() const
	{
		if (_layer != LAYER_3 && _chan_mode == CHAN_JOINT_STEREO)
			return std::make_pair<uint8_t, uint8_t>(
			    _intensity_band, 31);
		else
			return std::make_pair<uint8_t, uint8_t>(0, 0);
	}

	bool intensity_stereo() const
	{	return _intensity_stereo; }

	bool ms_stereo() const
	{	return _ms_stereo; }

	bool copyright() const
	{	return _copyright; }

	bool original() const
	{	return _original; }

	enum emphasis_type emphasis() const
	{	return _emphasis; }

	uint16_t size() const
	{	return _size; }

private:
	uint32_t		_bitrate;
	uint16_t		_frequency;
	uint16_t		_size;
	enum channel_mode_type	_chan_mode;
	enum emphasis_type	_emphasis;
	enum layer_type		_layer;
	enum version_type	_version;
	uint8_t			_intensity_band;
	bool			_copyright;
	bool			_has_crc;
	bool			_intensity_stereo;
	bool			_ms_stereo;
	bool			_original;
	bool			_padded;
	bool			_priv;
};

}

#endif
