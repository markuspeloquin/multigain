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

#include <mpg123.h>

#include <multigain/decode.hpp>
#include <multigain/gain_analysis.hpp>
#include <multigain/tag_locate.hpp>

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

class Mpg123_lib {
public:
	static void init() throw (Mpg123_error)
	{
		int ret;
		if ((ret = mpg123_init()) != MPG123_OK)
			throw Mpg123_error(ret);
		_instance._valid = true;
	}

private:
	Mpg123_lib() : _valid(false) {}
	~Mpg123_lib()
	{
		if (_valid) mpg123_exit();
	}

	Mpg123_lib(const Mpg123_lib &) {}
	void operator=(const Mpg123_lib &) {}

	static Mpg123_lib _instance;
	bool _valid;
};
Mpg123_lib Mpg123_lib::_instance;

/** Returns an index into <code>MPEG_BITRATE</code> */
inline uint8_t
mpeg_bitrate_tab(enum Mpeg_frame_header::version_type version,
    enum Mpeg_frame_header::layer_type layer)
{
	switch (version) {
	case Mpeg_frame_header::VERS_1:
		switch (layer) {
		case Mpeg_frame_header::LAYER_1: return 0;
		case Mpeg_frame_header::LAYER_2: return 1;
		case Mpeg_frame_header::LAYER_3: return 2;
		default: assert(0);
		}
	case Mpeg_frame_header::VERS_2:
	case Mpeg_frame_header::VERS_2_5:
		switch (layer) {
		case Mpeg_frame_header::LAYER_1: return 3;
		case Mpeg_frame_header::LAYER_2: return 4;
		case Mpeg_frame_header::LAYER_3: return 4;
		default: assert(0);
		}
	default: assert(0);
	}
	return 0xff;
}

void
sample_translate(const int16_t *samples, size_t count, uint8_t step,
    double *out)
{
	for (size_t i = 0; i < count; i += step) {
		int16_t sample = samples[i];
		if (sample < 0)
			// scale down ever so slightly; certainly pointless
			*out++ = sample * 32767.0 / 32768;
		else
			*out++ = sample;
	}
}

// these are usually LAME tags, which should be skipped over for replaygain
// analysis
bool
is_lame(const uint8_t *frame, size_t sz)
{
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

} // end anon
} // end multigain

multigain::Mpeg_decoder::Mpeg_decoder(std::ifstream &file,
    off_t media_begin, off_t media_end) throw (Disk_error, Mpg123_error) :
	_file(file),
	_pos(media_begin),
	_end(media_end)
{
	int		errval;

	Mpg123_lib::init();

	if (!_file.seekg(_pos))
		throw Disk_error("seek error");

	if (!(_hdl = mpg123_new(0, &errval)))
		throw Mpg123_error(errval);
	if ((errval = mpg123_open_feed(_hdl)) != MPG123_OK) {
		mpg123_delete(_hdl);
		throw Mpg123_error(errval);
	}
}

multigain::Mpeg_decoder::~Mpeg_decoder()
{
	mpg123_delete(_hdl);
}

size_t
multigain::Mpeg_decoder::next_frame(uint8_t frame[MAX_FRAME_LEN])
    throw (Disk_error)
{
	char		*buf = reinterpret_cast<char *>(frame);
	uint16_t	size;

	// read/parse header

	// if no bytes left (even if _end != filesize) assume nothing left
	if (_end == _pos) return 0;

	if (!_file.read(buf, 4)) {
		// no room for frame header
		_end = _pos;
		_file.seekg(_pos);
		return 0;
	}

	try {
		Mpeg_frame_header header(frame, true);
		size = header.size();
	} catch (Mpeg_frame_header::Bad_header) {
		// not a real frame header
		_end = _pos;
		_file.seekg(_pos);
		return 0;
	}

	// read remainder of frame

	if (!_file.read(reinterpret_cast<char *>(frame + 4), size - 4)) {
		// there should have been something
		_end = _pos;
		_file.seekg(_pos);
		throw Disk_error("read error");
	}

	return size;
}

std::pair<size_t, size_t>
multigain::Mpeg_decoder::decode_frame(Frame *frame)
    throw (Decode_error, Disk_error)
{
	// encoded data
	uint8_t		frame_encoded[MAX_FRAME_LEN];

	size_t bytes;
	size_t sz_sample = sizeof(int16_t) * frame->channels();
	size_t sz_buf = frame->len() * sz_sample;
	size_t sz_consumed = 0;
	size_t samples = 0;
	int ret;
	bool eof = false;

	do {
		off_t	frame_num;
		int16_t	*audio;

		// dereference frame->samples()
		ret = mpg123_decode_frame(_hdl, &frame_num,
		    reinterpret_cast<uint8_t **>(&audio), &bytes);
		if (bytes) {
			int16_t	**sample_bufs = frame->samples();
			samples = bytes / sz_sample;
			assert(samples * sz_sample == bytes);

			// copy decoded data into Frame structure
			if (frame->channels() == 1) {
				std::copy(audio, audio + samples,
				    sample_bufs[0]);
			} else {
				assert(frame->channels() == 2);

				int16_t *p = audio;
				for (size_t i = 0; i < samples; i++) {
					sample_bufs[0][i] = *p++;
					sample_bufs[1][i] = *p++;
				}
			}
			frame->frameno(frame_num);
			// overflow
			assert(frame->frameno() == frame_num);
		}

		switch (ret) {
		case MPG123_OK:
			break;
		case MPG123_NEW_FORMAT:
		{
			long rate;
			int channels;
			int encoding;

			if ((ret = mpg123_getformat(_hdl, &rate, &channels,
			    &encoding)))
				throw Mpg123_decode_error(
				    "on mpg123_getformat", ret);

			assert(rate > 0 &&
			    rate <= std::numeric_limits<uint16_t>::max());
			assert(channels > 0 &&
			    channels <= std::numeric_limits<uint8_t>::max());

			// reset encoding to int16_t
			if (encoding != MPG123_ENC_SIGNED_16 &&
			    (ret = mpg123_format(_hdl, rate, channels,
			    MPG123_ENC_SIGNED_16)))
				throw Mpg123_decode_error(
				    "on mpg123_format", ret);

			assert(!samples);
			frame->init(MAX_SAMPLES * 2, channels, rate);
			sz_sample = sizeof(int16_t) * channels;
			sz_buf = frame->len() * sz_sample;

			break;
		}
		case MPG123_NEED_MORE:
		{
			// read next MPEG frame
			size_t size = next_frame(frame_encoded);
			if (!size) {
				eof = true;
				break;
			}
			sz_consumed += size;

			// send to decoder
			if ((ret = mpg123_feed(_hdl, frame_encoded, size))
			    != MPG123_OK) {
				// undo next_frame()'s damage
				_file.seekg(_pos -= size);
				throw Mpg123_decode_error(ret);
			}
			break;
		}
		case MPG123_DONE:
			// shouldn't happen with feeding
		default:
			throw Mpg123_decode_error("on mpg123_read", ret);
		}
	} while (!eof && !samples);

	std::cout << "returning (" << sz_consumed << ", " << samples << ")\n";
	return std::make_pair(sz_consumed, samples);
}

void
multigain::Mpeg_frame_header::init(const uint8_t header[4], bool minimal)
    throw (Bad_header)
{
	// verify frame sync
	if (header[0] != 0xff || (header[1] & 0xe0) != 0xe0)
		throw Bad_header();

	// -------- ---VVLLP BBBBFFPp CCMMCOEE
	_version = static_cast<enum version_type>((header[1] >> 3) & 0x3);
	if (_version == VERS_RESERVED)
		throw Bad_header();
	_layer = static_cast<enum layer_type>((header[1] >> 1) & 0x3);
	if (_layer == LAYER_RESERVED)
		throw Bad_header();
	_bitrate = header[2] >> 4;
	_frequency = (header[2] >> 2) & 0x3;
	_padded = (header[2] & 0x2) == 0x2;

	// don't bother checking for invalid modes
	_chan_mode = static_cast<enum channel_mode_type>(header[3] >> 6);

	if (minimal) {
		_has_crc = false;
		_priv = false;
		_intensity_band = 0;
		_intensity_stereo = false;
		_ms_stereo = false;
		_copyright = false;
		_original = false;
		_emphasis = EMPH_NONE;
	} else {
		_has_crc = header[1] & 0x1;
		_priv = header[2] & 0x1;
		if (_chan_mode == CHAN_JOINT_STEREO) {
			if (_layer == LAYER_3) {
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
		_emphasis = static_cast<enum emphasis_type>(header[3] & 0x3);
	}

	// translate bitrate, frequency
	_bitrate = MPEG_BITRATE[mpeg_bitrate_tab(_version, _layer)][_bitrate];
	if (_bitrate <= 0)
		throw Bad_header();
	_bitrate *= 1000;

	_frequency = MPEG_FREQ[_version][_frequency];
	if (_frequency <= 0)
		throw Bad_header();

	if (_layer == LAYER_1)
		_size = (12 * _bitrate / _frequency + _padded) * 4;
	else
		_size = 144 * _bitrate / _frequency + _padded;
}
