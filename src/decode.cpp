#include <cassert>
#include <iostream>
#include <memory>

#include <mpg123.h>

#include <multigain/decode.hpp>
#include <multigain/gain_analysis.hpp>
#include <multigain/tag_locate.hpp>

namespace multigain {
namespace {

enum mpeg_version {
	MPEG_V_2_5 = 0x0,
	MPEG_V_RESERVED = 0x1,
	MPEG_V_2 = 0x2,
	MPEG_V_1 = 0x3
};

enum mpeg_layer {
	MPEG_L_RESERVED = 0x0,
	MPEG_L_3 = 0x1,
	MPEG_L_2 = 0x2,
	MPEG_L_1 = 0x3
};

/** For indexing, see <code>mpeg_bitrate_tab()</code> */
int16_t MPEG_BITRATE[5][16] = {
//  0: 'free' bitrate, unsupported by this code
// -1: invalid
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1},
    {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1},
    {0,  8, 16, 24,  32,  64,  80,  56,  64, 128, 160, 112, 128, 256, 320, -1}
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

/** Returns an index into <code>MPEG_BITRATE</code> */
inline uint8_t
mpeg_bitrate_tab(enum mpeg_version version, enum mpeg_layer layer)
{
	switch (version) {
	case MPEG_V_1:
		switch (layer) {
		case MPEG_L_1: return 0;
		case MPEG_L_2: return 1;
		case MPEG_L_3: return 2;
		default: assert(0);
		}
	case MPEG_V_2:
	case MPEG_V_2_5:
		switch (layer) {
		case MPEG_L_1: return 3;
		case MPEG_L_2: return 4;
		case MPEG_L_3: return 4;
		default: assert(0);
		}
	default: assert(0);
	}
}

/* Non-error return values in range [26,6913] */
bool
mpeg_parse_frame_header(const uint8_t header[4],
    uint32_t &out_bps,
    uint16_t &out_frequency,
    bool &out_stereo,
    uint16_t &out_size)
{
	int32_t			frequency;
	int16_t			bitrate;
	enum mpeg_layer		layer;
	enum mpeg_version	version;
	bool			padded;

	// verify frame sync
	if (header[0] != 0xff || (header[1] & 0xe0) != 0xe0)
		return false;

	// -------- ---VVLL- BBBBFFFFP- --------
	version = static_cast<enum mpeg_version>((header[1] >> 3) & 0x3);
	layer = static_cast<enum mpeg_layer>((header[1] >> 1) & 0x3);
	bitrate = header[2] >> 4;
	frequency = (header[2] >> 2) & 0x3;
	padded = (header[2] & 0x2) != 0;

	// translate bitrate, frequency
	bitrate = MPEG_BITRATE[mpeg_bitrate_tab(version, layer)][bitrate];
	frequency = MPEG_FREQ[version][frequency];

	if (bitrate <= 0) return false;
	if (frequency <= 0) return false;

	switch (header[3] >> 6) {
	case 2: case 3:
		out_stereo = true;
		break;
	default:
		out_stereo = false;
	}

	out_bps = static_cast<uint32_t>(bitrate) * 1000;
	out_frequency = frequency;
	if (layer == MPEG_L_1)
		out_size = ( 12000 * bitrate / frequency) * 4 + padded;
	else
		out_size = (144000 * bitrate / frequency)     + padded;
	return true;
}

void
sample_translate(const int16_t *samples, size_t count, uint8_t step,
    double *out)
{
	for (size_t i = 0; i < count; i += step) {
		int32_t sample = samples[i];
		if (sample < 0)
			// scale down; why are they using -32768???
			*out++ = sample * 32767.0 / 32768;
		else
			*out++ = sample;
	}
}

} // end anon
} // end multigain

/*
ssize_t
multigain::mpeg_decode_frame(std::ifstream &in, off_t max_size,
    bool &out_stereo, uint16_t &out_frequency,
    double left[1152*2], double right[1152*2])
{
	static bool _lame_init = false;
	if (!_lame_init) {
		if (lame_decode_init() == -1) {
			fprintf(stderr, "lame failed to initialize\n");
		}
		_lame_init = true;
	}

	struct {
		uint8_t		header[4];
		uint8_t		data[6913];
	} buf;
	short			int_l[1152 * 2];
	short			int_r[1152 * 2];
	int			nout;
	uint16_t		frequency;
	uint16_t		size;
	bool			stereo;

	for (;;) {
		if ((max_size -= 4) < 0)
			// not enough room for a header
			return 0;
		if (!in.read(reinterpret_cast<char *>(buf.header), 4))
			throw Disk_error("read error");

		if (!mpeg_parse_frame_header(buf.header,
		    frequency, stereo, size)) {
			// not a real frame header
			in.seekg(-4);
			return 0;
		}

		if ((max_size -= size - 4) < 0) {
			std::cerr
			    << "frame data overlaps supposed tag data\n";
			return -1;
		}

		if (!in.read(reinterpret_cast<char *>(buf.data), size - 4))
			throw Disk_error("read error");

		nout = lame_decode(buf.header, size, int_l, int_r);
		if (nout == -1) {
			std::cerr << "decoding error\n";
			return -1;
		} else if (nout)
			break;
		// else continue
	}

	out_stereo = stereo;
	out_frequency = frequency;
	sample_translate(int_l, int_l + nout, left);
	if (stereo)
		sample_translate(int_r, int_r + nout, right);
	return nout;
}
*/

multigain::Mpeg_decoder::Mpeg_decoder(std::ifstream &file,
    off_t media_begin, off_t media_end) throw (Disk_error, Mpg123_error) :
	_file(file),
	_pos(media_begin),
	_end(media_end)
{
	static bool	mpg123_initialized = false;
	int		errval;

	if (!_file.seekg(_pos))
		throw Disk_error("seek error");

	if (!mpg123_initialized) {
		if ((errval = mpg123_init()) != MPG123_OK)
			throw Mpg123_error(errval);
		mpg123_initialized = true;
	}

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

std::pair<size_t, size_t>
multigain::Mpeg_decoder::decode_frame(
    double left[MAX_FRAMES], double right[MAX_FRAMES],
    struct decode_info *info)
    throw (Decode_error, Disk_error, Mpg123_error)
{
	struct {
		uint8_t	header[4];
		uint8_t	data[6913];
	} buf;

	int16_t		block[MAX_FRAMES * 2];
	uint32_t	bps;
	int		errval;
	uint16_t	bytes;
	uint16_t	frames;
	uint16_t	frequency;
	bool		stereo;

	bytes = 0;
	frames = 0;
	do {
		size_t		done;
		uint16_t	block_size;
		uint16_t	size;
#if 0
		long		*rate;
		int		*channels;
		int		*encoding;
#endif

		// read header

		if (_end - _pos < 4)
			// not enough room for a header
			return std::make_pair(0, 0);
		if (!_file.read(reinterpret_cast<char *>(buf.header), 4))
			throw Disk_error("read error");
		if (!mpeg_parse_frame_header(buf.header,
		    bps, frequency, stereo, size)) {
			// not a real frame header
			_file.seekg(_pos);
			return std::make_pair(0, 0);
		}

		// send to decoder

		if (_end - _pos < size) {
			_file.seekg(_pos);
			throw Decode_error(
			    "frame data overlaps supposed tag data\n");
		}
		if (!_file.read(reinterpret_cast<char *>(buf.data),
		    size - 4)) {
			_file.seekg(_pos);
			throw Disk_error("read error");
		}
		if ((errval = mpg123_feed(_hdl, buf.header, size)) !=
		    MPG123_OK) {
			_file.seekg(_pos);
			throw Mpg123_error(errval);
		}
		_pos += size;
		bytes += size;

		if (stereo)
			block_size = MAX_FRAMES * 4;
		else
			block_size = MAX_FRAMES * 2;

#if 0
		// see what mpg123 has to say
		if ((errval = mpg123_getformat(_hdl,
		    &rate, &channels, &encoding)) != MPG123_OK)
			throw Mpg123_error(errval);

		if (rate != frequency)
			throw Decode_error("frequency confusion");
		if (channels != stereo + 1)
			throw Decode_error("stereo confusion");
#endif

		// receive back from decode

		switch ((errval = mpg123_read(_hdl,
		    reinterpret_cast<uint8_t *>(block), block_size, &done))) {
		// I definitely didn't write my code to take advantage of
		// this wealth of status information; my code's already aware
		// of this stuff
		case MPG123_NEW_FORMAT:
		case MPG123_DONE:
		case MPG123_NEED_MORE:
		case MPG123_OK:
			frames = done / ((stereo + 1) * 2);
			break;
		default:
			throw Mpg123_error(errval);
		}
	} while (!frames);

	if (info) {
		info->bps = bps;
		info->frequency = frequency;
		info->stereo = stereo;
	}

	if (stereo) {
		sample_translate(block, frames, 2, left);
		sample_translate(block + 1, frames, 2, right);
	} else
		sample_translate(block, frames, 1, left);

	return std::make_pair(bytes, frames);
}
