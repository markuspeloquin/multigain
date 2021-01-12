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

#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

#include <lame/lame.h>

#include <multigain/decode.hpp>
#include <multigain/gain_analysis.hpp>
#include <multigain/tag_locate.hpp>
#include "lame.hpp"

namespace multigain {
namespace {

/** For indexing, see <code>mpeg_bitrate_tab()</code> */
int16_t MPEG_BITRATE[5][16] = {
//  0: 'free' bitrate, unsupported by this code
// -1: invalid
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1},
    {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256, -1},
    {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160, -1}
};
/** Index by mpeg_version */
int32_t MPEG_FREQ[4][4] = {
//  0: reserved
// -1: invalid
    { 11025, 12000,  8000,  0 },
    {    -1,    -1,    -1, -1 },
    { 22050, 24000, 16000,  0 },
    { 44100, 48000, 32000,  0 }
};

uint8_t MPEG_INTENSITY_BAND[4] = {
	4, 8, 12, 16
};


#if 0
bool		is_lame(const uint8_t *, size_t);
#endif
inline uint8_t	mpeg_bitrate_tab(Mpeg_frame_header::version_type,
		    Mpeg_frame_header::layer_type);
#if 0
void		sample_translate(const int16_t *, size_t, uint8_t,
		    double *out);
#endif


/** Returns an index into <code>MPEG_BITRATE</code> */
inline uint8_t
mpeg_bitrate_tab(Mpeg_frame_header::version_type version,
    Mpeg_frame_header::layer_type layer) {
	using layer_type = Mpeg_frame_header::layer_type;
	using version_type = Mpeg_frame_header::version_type;

	switch (version) {
	case version_type::V1:
		switch (layer) {
		case layer_type::L1: return 0;
		case layer_type::L2: return 1;
		case layer_type::L3: return 2;
		default: assert(0);
		}
	case version_type::V2:
	case version_type::V2_5:
		switch (layer) {
		case layer_type::L1: return 3;
		case layer_type::L2: return 4;
		case layer_type::L3: return 4;
		default: assert(0);
		}
	default: assert(0);
	}
	return 0xff;
}

#if 0
void
sample_translate(const int16_t *samples, size_t count, uint8_t step,
    double *out) {
	for (size_t i = 0; i < count; i += step) {
		int16_t sample = samples[i];
		if (sample < 0)
			// scale down ever so slightly; certainly pointless
			*out++ = sample * 32767.0 / 32768;
		else
			*out++ = sample;
	}
}
#endif

#if 0
// these are usually LAME tags, which should be skipped over for replaygain
// analysis
bool
is_lame(const uint8_t *frame, size_t sz) {
	// http://gabriel.mp3-tech.org/mp3infotag.html

	//const size_t INFO_OFFSET = 0x24;
	// the LAME tag would start at 0x9c, though it's not required to (it
	// could just be absent)
	const size_t MIN_SIZE = 0x9c;

	const uint8_t *begin = frame;
	while (!*begin) ++begin;
	if (begin == frame) return false;

	// I really wonder how plain MP3 players find their way around this;
	// 'Xing' is for VBR, 'Info' for CBR
	return sz >= MIN_SIZE && (
	    std::equal(begin, begin + 4,
	    reinterpret_cast<const uint8_t *>("Xing")) ||
	    std::equal(begin, begin + 4,
	    reinterpret_cast<const uint8_t *>("Info")));
}
#endif

} // end anon
} // end multigain

multigain::Mpeg_decoder::Mpeg_decoder(std::ifstream &file) :
	_file(file),
	_end(-1),
	_pos(-1),
	_capacity(0),
	_samples(0),
	_skip_back(0),
	_skip_front(-1) {
	lame_global_flags *lame = Lame_lib::init();

	if (!(_gfp = hip_decode_init()))
		throw Lame_error("initializing decoder", LAME_NOMEM);

	std::list<tag_info> tags;
	find_tags(file, tags);
	//dump_tags(tags);

	for (std::list<tag_info>::const_iterator i = tags.begin();
	    i != tags.end(); ++i)
		switch (i->type) {
		case tag_type::MPEG:
			_pos = i->start;
			_end = _pos + i->size;
			// also i->extra.count;
			break;
		case tag_type::MP3_INFO:
		case tag_type::MP3_XING:
			_skip_front = i->extra.info.skip_front;
			_skip_back = i->extra.info.skip_back;
			break;
		default:;
		}

	if (_pos < 0)
		throw Bad_format("not an MPEG audio file");

	if (_skip_front == static_cast<uint16_t>(-1))
		_skip_front = lame_get_encoder_delay(lame) + 528 + 1;
		// leave _skip_back at 0
	else {
		_skip_front += 528 + 1;
		if (_skip_back < 528 + 1)
			_skip_back = 0;
		else
			_skip_back -= 528 + 1;
	}

	_capacity = MAX_SAMPLES + _skip_back;
	_sample_buf.reset(new short[_capacity * 2]);

	if (!_file.seekg(_pos))
		throw Disk_error("seek error");
}

multigain::Mpeg_decoder::~Mpeg_decoder() noexcept {
	/*int ret =*/ hip_decode_exit(_gfp);
}

std::shared_ptr<multigain::Mpeg_frame_header>
multigain::Mpeg_decoder::next_frame(uint8_t frame[MAX_FRAME_LEN]) {
	std::shared_ptr<Mpeg_frame_header> hdr;
	char *buf = reinterpret_cast<char *>(frame);

	// read/parse header

	// if no bytes left (even if _end != filesize) assume nothing left
	if (_end == _pos) return hdr;

	if (!_file.read(buf, 4)) {
		// no room for frame header
		_end = _pos;
		_file.seekg(_pos);
		return hdr;
	}

	try {
		hdr.reset(new Mpeg_frame_header(frame, true));
	} catch (const Mpeg_frame_header::Bad_header &e) {
		// not a real frame header
		_end = _pos;
		_file.seekg(_pos);
		return hdr;
	}

	// read remainder of frame

	if (!_file.read(reinterpret_cast<char *>(frame + 4),
	    hdr->size() - 4)) {
		// there should have been something
		_end = _pos;
		_file.seekg(_pos);
		throw Disk_error("read error");
	}

	return hdr;
}

std::pair<size_t, size_t>
multigain::Mpeg_decoder::decode(Audio_buffer *buf) {
	// encoded data
	uint8_t					mp3buf[MAX_FRAME_LEN];
	mp3data_struct				mp3data;
	std::shared_ptr<Mpeg_frame_header>	hdr;
	size_t		buf_len = 0;
	size_t		bytes_read = 0;
	int16_t		**sample_bufs = buf->samples();
	int16_t		*lout = sample_bufs ? sample_bufs[0] : 0;
	int16_t		*rout = sample_bufs ? sample_bufs[1] : 0;
	short		*lsamples = _sample_buf.get();
	short		*rsamples = _sample_buf.get() + _capacity;
	size_t		tot_samples = 0;
	uint8_t		channels = buf->channels();

	for (;;) {
		// decode frame
		int samples = hip_decode1_headers(_gfp, mp3buf, buf_len,
		    lsamples + _samples, rsamples + _samples, &mp3data); 
		if (samples > 0) {
			_samples += samples;
			if (_skip_front) {
				// move data to account for delay
				if (_skip_front < _samples) {
					std::copy(lsamples + _skip_front,
					    lsamples + _samples,
					    lsamples);
					std::copy(rsamples + _skip_front,
					    rsamples + _samples,
					    rsamples);
					_samples -= _skip_front;
					_skip_front = 0;
				} else {
					_skip_front -= _samples;
					_samples = 0;
				}
			}

			// changing # channels will reinit something in
			// 'buf'; only do so if 'buf' is empty

			if (channels != mp3data.stereo) {
				if (tot_samples)
					// # channels changed
					break;

				channels = mp3data.stereo;
				buf->init(channels, mp3data.samplerate);
				sample_bufs = buf->samples();
				lout = sample_bufs[0];
				rout = sample_bufs[1];
			}

			if (_samples > _skip_back) {
				// copy data from decoder buffers to output
				size_t amt = std::min(_samples - _skip_back,
				    buf->len() - tot_samples);
				std::copy(lsamples, lsamples + amt,
				    lout + tot_samples);
				std::copy(lsamples + amt, lsamples + _samples,
				    lsamples);
				if (channels > 1) {
					std::copy(rsamples, rsamples + amt,
					    rout + tot_samples);
					std::copy(rsamples + amt,
					    rsamples + _samples, rsamples);
				}
				tot_samples += amt;
				_samples -= amt;

				if (tot_samples == buf->len())
					// output buffers full
					break;
			}
		} else if (samples < 0)
			throw Lame_decode_error("decoding error", samples);

		// read next MPEG frame
		if (!samples) {
			hdr = next_frame(mp3buf);
			if (!hdr.get())
				// eof
				break;

			buf_len = hdr->size();
			bytes_read += buf_len;
		} else
			buf_len = 0;
	}

	if (!tot_samples)
		assert(_samples == _skip_back);

	return {bytes_read, tot_samples};
}

void
multigain::Mpeg_frame_header::init(const uint8_t header[4], bool minimal) {
	// verify frame sync
	if (header[0] != 0xff || (header[1] & 0xe0) != 0xe0)
		throw Bad_header();

	// -------- ---VVLLP BBBBFFPp CCMMCOEE
	_version = static_cast<version_type>((header[1] >> 3) & 0x3);
	if (_version == version_type::RESERVED)
		throw Bad_header();
	_layer = static_cast<layer_type>((header[1] >> 1) & 0x3);
	if (_layer == layer_type::RESERVED)
		throw Bad_header();
	_bitrate = header[2] >> 4;
	_frequency = (header[2] >> 2) & 0x3;
	_padded = (header[2] & 0x2) == 0x2;

	// don't bother checking for invalid modes
	_chan_mode = static_cast<channel_mode_type>(header[3] >> 6);

	if (minimal) {
		_has_crc = false;
		_priv = false;
		_intensity_band = 0;
		_intensity_stereo = false;
		_ms_stereo = false;
		_copyright = false;
		_original = false;
		_emphasis = emphasis_type::NONE;
	} else {
		_has_crc = header[1] & 0x1;
		_priv = header[2] & 0x1;
		if (_chan_mode == channel_mode_type::JOINT_STEREO) {
			if (_layer == layer_type::L3) {
				_intensity_band = 0;
				_intensity_stereo = (header[3] >> 5) & 0x1;
				_ms_stereo = (header[3] >> 4) & 0x1;
			} else {
				_intensity_band = MPEG_INTENSITY_BAND[
				    (header[3] >> 4) & 0x3];
				_intensity_stereo = false;
				_ms_stereo = false;
			}
		} else {
			_intensity_band = 0;
			_intensity_stereo = false;
			_ms_stereo = false;
		}
		_copyright = (header[3] >> 3) == 0x1;
		_original = (header[3] >> 2) == 0x1;
		_emphasis = static_cast<emphasis_type>(header[3] & 0x3);
	}

	// translate bitrate, frequency
	_bitrate = MPEG_BITRATE[mpeg_bitrate_tab(_version, _layer)][_bitrate];
	if (_bitrate <= 0)
		throw Bad_header();
	_bitrate *= 1000;

	_frequency = MPEG_FREQ[static_cast<int>(_version)][_frequency];
	if (_frequency <= 0)
		throw Bad_header();

	if (_layer == layer_type::L1)
		_size = (12 * _bitrate / _frequency + _padded) * 4;
	else
		_size = 144 * _bitrate / _frequency + _padded;
}
