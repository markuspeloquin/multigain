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

#include <cstdint>
#include <fstream>
#include <memory>

#include <multigain/errors.hpp>

// hip_global_struct* == hip_t
struct hip_global_struct;

namespace multigain {

class Audio_buffer {
public:
	Audio_buffer(size_t len) :
		_samples(),
		_sample_ptrs(),
		_len(len),
		_freq(0),
		_chan(0)
	{}

	void init(uint8_t channels, int16_t freq) {
		using std::swap;

		assert(channels && freq);

		if (channels == _chan) return;

		size_t total = _len * channels;

		std::unique_ptr<int16_t> new_samples(new int16_t[total]);

		std::unique_ptr<int16_t *> new_sample_ptrs(
		    new int16_t *[channels]);

		size_t off = 0;
		for (size_t i = 0; i < channels; i++) {
			new_sample_ptrs.get()[i] = new_samples.get() + off;
			off += _len;
		}

		std::swap(_sample_ptrs, new_sample_ptrs);
		std::swap(_samples, new_samples);
		_chan = channels;
		_freq = freq;
	}

	int16_t **samples() {
		return _sample_ptrs.get();
	}

	const int16_t *const *samples() const {
		return _sample_ptrs.get();
	}

	size_t len() const {
		return _len;
	}

	uint16_t frequency() const {
		return _freq;
	}

	uint8_t channels() const {
		return _chan;
	}

	std::unique_ptr<int16_t>	_samples;
	std::unique_ptr<int16_t *>	_sample_ptrs;
	size_t		_len;
	uint16_t 	_freq;
	uint8_t 	_chan;
};

class Decoder {
public:
	virtual ~Decoder() noexcept {}

	/// \throw Decode_error
	/// \throw Disk_error
	virtual std::pair<size_t, size_t> decode(Audio_buffer *)
	    = 0;
};

class Flac_decoder : public Decoder {
public:
	/// \throw Disk_error
	/// \throw Lame_decode_error
	Flac_decoder(FILE *fp);
	~Flac_decoder() noexcept;

	/// \throw Decode_error
	/// \throw Disk_error
	/// \throw Lame_decode_error
	std::pair<size_t, size_t> decode(Audio_buffer *) override;
};

class Mpeg_frame_header;

/** MPEG audio frame-by-frame decoder */
class Mpeg_decoder : public Decoder {
public:
	const static uint16_t MAX_SAMPLES = 1152;

	/** Create an MPEG audio decoder
	 *
	 * \param file	The opened MPEG audio file
	 * \throw Bad_format	Not an MPEG audio file
	 * \throw Disk_error	Problem searching tags
	 * \throw Lame_error	The LAME library has some error
	 */
	Mpeg_decoder(std::ifstream &file);
	~Mpeg_decoder() noexcept;

	Mpeg_decoder(const Mpeg_decoder &) = delete;
	void operator=(const Mpeg_decoder &) = delete;

	/** Decode samples
	 *
	 * \param[out] left	Samples of the stereo-left or mono channel
	 * \param[out] right	Samples of the stereo-right channel
	 * \param[out] info	Optional.  Basic information about the samples
	 *	just returned
	 * \throw Decode_error	So far, this really should not happen
	 * \throw Disk_error	Seek or read error
	 * \throw Lame_error	The LAME library has some error
	 * \return	Input bytes consumed, samples decoded
	 */
	std::pair<size_t, size_t> decode(Audio_buffer *) override;

private:
	// 448 kbps, 8 kHz, padded
	static const size_t MAX_FRAME_LEN = 8065;

	/// \throw Disk_error
	std::shared_ptr<Mpeg_frame_header> next_frame(
	    uint8_t[MAX_FRAME_LEN]);

	std::ifstream			&_file;
	std::unique_ptr<int16_t>	_sample_buf;
	off_t				_end;
	off_t				_pos;
	struct hip_global_struct	*_gfp;
	// samples per channel that _sample_buf can hold
	size_t				_capacity;
	size_t				_samples;
	uint16_t			_skip_back;
	uint16_t			_skip_front;
};

class Mpeg_frame_header {
public:
	struct Bad_header : std::exception {};

	enum class version_type {
		V2_5 = 0,
		RESERVED = 1,
		V2 = 2,
		V1 = 3
	};
	enum class layer_type {
		RESERVED = 0,
		L3 = 1,
		L2 = 2,
		L1 = 3
	};
	enum class channel_mode_type {
		STEREO = 0,
		JOINT_STEREO = 1,
		DUAL = 2,
		MONO = 3
	};
	enum class emphasis_type {
		NONE = 0,
		MS_50_15 = 1,
		RESERVED = 2,
		CCIT_J17 = 3
	};

	// must call init() before anything else
	Mpeg_frame_header() {}

	/// \throw Bad_header
	Mpeg_frame_header(const uint8_t header[4], bool minimal=false) {
		init(header, minimal);
	}

	// not exception-safe
	/// \throw Bad_header
	void init(const uint8_t[4], bool minimal=false);

	version_type version() const {
		return _version;
	}

	layer_type layer() const {
		return _layer;
	}

	bool has_crc() const {
		return _has_crc;
	}

	uint32_t bitrate() const {
		return _bitrate;
	}

	uint16_t frequency() const {
		return _frequency;
	}

	bool padded() const {
		return _padded;
	}

	bool priv() const {
		return _priv;
	}

	channel_mode_type channel_mode() const {
		return _chan_mode;
	}

	unsigned channels() const {
		return _chan_mode == channel_mode_type::MONO ? 1 : 2;
	}

	std::pair<uint8_t, uint8_t> intensity_bands() const {
		if (_layer == layer_type::L3 ||
		    _chan_mode != channel_mode_type::JOINT_STEREO)
			return {0, 0};
		return {_intensity_band, 31};
	}

	bool intensity_stereo() const {
		return _intensity_stereo;
	}

	bool ms_stereo() const {
		return _ms_stereo;
	}

	bool copyright() const {
		return _copyright;
	}

	bool original() const {
		return _original;
	}

	emphasis_type emphasis() const {
		return _emphasis;
	}

	uint16_t size() const {
		return _size;
	}

private:
	uint32_t		_bitrate;
	uint16_t		_frequency;
	uint16_t		_size;
	channel_mode_type	_chan_mode;
	emphasis_type		_emphasis;
	layer_type		_layer;
	version_type		_version;
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
