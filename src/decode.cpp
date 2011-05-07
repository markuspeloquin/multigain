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
    off_t media_begin, off_t media_end) throw (Disk_error, Lame_decode_error) :
	_file(file),
	_pos(media_begin),
	_end(media_end)
{
	Lame_lib::init();

	if (!_file.seekg(_pos))
		throw Disk_error("seek error");

	if (!(_gfp = hip_decode_init()))
		throw Lame_decode_error("initializing decoder", LAME_NOMEM);
}

multigain::Mpeg_decoder::~Mpeg_decoder()
{
	/*int ret =*/ hip_decode_exit(_gfp);
}

boost::shared_ptr<multigain::Mpeg_frame_header>
multigain::Mpeg_decoder::next_frame(uint8_t frame[MAX_FRAME_LEN])
    throw (Disk_error)
{
	boost::shared_ptr<Mpeg_frame_header> hdr;
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
	} catch (Mpeg_frame_header::Bad_header) {
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
multigain::Mpeg_decoder::decode_frame(Frame *frame)
    throw (Decode_error, Disk_error, Lame_decode_error)
{
	const bool EXTRA_COPY = sizeof(short) != sizeof(int16_t);

	// encoded data
	uint8_t		mp3buf[MAX_FRAME_LEN];
	mp3data_struct	mp3data;
	boost::shared_ptr<Mpeg_frame_header> hdr;
	int		samples = 0;

	boost::scoped_array<short> lbuf;
	boost::scoped_array<short> rbuf;

	for (;;) {
		int	enc_delay;
		int	enc_padding;

		// read next MPEG frame
		hdr = next_frame(mp3buf);
		if (!hdr.get())
			// eof
			break;
			//return std::make_pair(0, 0);

		frame->init(MAX_SAMPLES, hdr->channels(), hdr->frequency());
		int16_t	**sample_bufs = frame->samples();
		short *lsamples;
		short *rsamples;
		if (EXTRA_COPY) {
			if (!lbuf.get())
				lbuf.reset(new short[MAX_SAMPLES]);
			if (!rbuf.get() && hdr->channels() == 2)
				rbuf.reset(new short[MAX_SAMPLES]);
			lsamples = lbuf.get();
			rsamples = rbuf.get();
		} else {
			lsamples = sample_bufs[0];
			rsamples = sample_bufs[1];
		}

		// decode frame
		samples = hip_decode1_headersB(_gfp, mp3buf, hdr->size(),
		    lsamples, rsamples,
		    &mp3data, &enc_delay, &enc_padding); 
		if (samples < 0)
			throw Lame_decode_error("decoding error", samples);
		else if (!samples)
			continue;

		std::cout << "samples " << samples << '\n';

		assert(frame->channels() == mp3data.stereo);
		assert(frame->frequency() == mp3data.samplerate);

		if (enc_delay < 0) enc_delay = 0;
		if (enc_padding < 0) enc_padding = 0;

		if (EXTRA_COPY) {
			// copy back into sample_bufs
			std::copy(lbuf.get(), lbuf.get() + samples,
			    sample_bufs[0]);
			if (rbuf.get())
				std::copy(rbuf.get(), rbuf.get() + samples,
				    sample_bufs[1]);
		}

		break;
	}

	size_t bytes = hdr.get() ? hdr->size() : 0;
	std::cout << "returning (" << bytes << ", " << samples << ")\n";
	return std::make_pair(bytes, samples);
}

#undef EXTRA_COPY

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
