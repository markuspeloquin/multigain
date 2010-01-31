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
    uint8_t &out_channels,
    uint16_t &out_size)
{
	int32_t			frequency;
	int32_t			bitrate;
	uint8_t			chan_mode;
	enum mpeg_layer		layer;
	enum mpeg_version	version;
	bool			padded;

	// verify frame sync
	if (header[0] != 0xff || (header[1] & 0xe0) != 0xe0)
		return false;

	// -------- ---VVLL- BBBBFFP- CC------
	version = static_cast<enum mpeg_version>((header[1] >> 3) & 0x3);
	layer = static_cast<enum mpeg_layer>((header[1] >> 1) & 0x3);
	bitrate = header[2] >> 4;
	frequency = (header[2] >> 2) & 0x3;
	padded = (header[2] & 0x2) != 0;
	chan_mode = header[3] >> 6;

	// translate bitrate, frequency
	bitrate = MPEG_BITRATE[mpeg_bitrate_tab(version, layer)][bitrate];
	frequency = MPEG_FREQ[version][frequency];

	if (bitrate <= 0) return false;
	if (frequency <= 0) return false;

	switch (chan_mode) {
	case 0x3:
		out_channels = 1;
		break;
	default:
		out_channels = 2;
	}

	out_bps = bitrate *= 1000;
	out_frequency = frequency;
	if (layer == MPEG_L_1)
		out_size = ( 12 * bitrate / frequency + padded) * 4;
	else
		out_size =  144 * bitrate / frequency + padded;
	return true;
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

} // end anon
} // end multigain

multigain::Mpeg_decoder::Mpeg_decoder(std::ifstream &file,
    off_t media_begin, off_t media_end) throw (Disk_error, Mpg123_error) :
	_file(file),
	_pos(media_begin),
	_end(media_end),
	_last_frequency(0),
	_last_channels(0)
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

size_t
multigain::Mpeg_decoder::next_frame(uint8_t frame[MAX_FRAME])
    throw (Disk_error)
{
	char		*buf = reinterpret_cast<char *>(frame);
	uint32_t	bps;
	uint16_t	frequency;
	uint16_t	size;
	uint8_t		channels;

	// read/parse header

	// if no bytes left (even if _end != filesize) assume nothing left
	if (_end == _pos) return 0;

	if (!_file.read(buf, 4)) {
		// no room for frame header
		_end = _pos;
		_file.seekg(_pos);
		return 0;
	}

	if (!mpeg_parse_frame_header(frame, bps, frequency, channels, size)) {
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
multigain::Mpeg_decoder::decode_frame(
    double left[MAX_SAMPLES], double right[MAX_SAMPLES],
    struct decode_info *info)
    throw (Decode_error, Disk_error, Mpg123_error)
{
	// encoded data
	uint8_t		frame[MAX_FRAME];

	// decoded data; 2: channels
	int16_t		block[MAX_SAMPLES * 2];
	int		errval;
	uint16_t	bytes;
	uint16_t	samples;
	// number of attempts made at end (some frame(s) read, no samples
	// returned); I would be lying if I claimed to understand libmpg123
	uint8_t		attempts;

	attempts = 0;
	bytes = 0;
	samples = 0;
	do {
		size_t		bytes_out;
		uint16_t	block_size;
		uint16_t	size;

		// read next MPEG frame
		size = next_frame(frame);
		bytes += size;

		// send to decoder
		if ((errval = mpg123_feed(_hdl, frame, size)) != MPG123_OK) {
			// undo next_frame()'s damage
			_pos -= size;
			_file.seekg(_pos);
			throw Mpg123_error(errval);
		}

		// first time, this will be zero, which is okay; it will be
		// sorted out below
		block_size = _last_channels == 2 ?
		    sizeof(block) : sizeof(block) / 2;

		// get decoded samples

		while ((errval = mpg123_read(_hdl,
		    reinterpret_cast<uint8_t *>(block), block_size,
		    &bytes_out)) == MPG123_NEW_FORMAT) {
			// normally, this block will be executed only after
			// the first frame; frequencies and channels don't
			// ever really switch mid-file

			long	frequency;
			int	channels;
			int	encoding;

			if ((errval = mpg123_getformat(_hdl,
			    &frequency, &channels, &encoding)) != MPG123_OK)
				throw Mpg123_error(errval);

			if (encoding && encoding != MPG123_ENC_SIGNED_16)
				throw Decode_error("unexpected encoding");

			_last_frequency = frequency;
			_last_channels = channels;

			block_size = _last_channels == 2 ?
			    sizeof(block) : sizeof(block) / 2;

/*
			// reset format
			if (rate) {
				if ((errval = mpg123_format(_hdl, frequency,
				    channels, MPG123_ENC_FLOAT_64)) !=
				    MPG123_OK)
					throw Mpg123_error(errval);
			}
*/
		}
		switch (errval) {
		case MPG123_DONE:
		case MPG123_NEED_MORE:
			if (bytes) attempts++;
			if (!bytes || attempts == 10)
				return std::make_pair(0, 0);
			break;
		case MPG123_OK:
			// 2: bytes per frame per channel
			assert(_last_channels);
			samples = bytes_out / (_last_channels * 2);
			break;
		default:
			throw Mpg123_error(errval);
		}
	} while (!samples);

	if (info) {
		info->frequency = _last_frequency;
		info->channels = _last_channels;
	}

	if (_last_channels == 2) {
		sample_translate(block, samples, 2, left);
		sample_translate(block + 1, samples, 2, right);
	} else
		sample_translate(block, samples, 1, left);

	return std::make_pair(bytes, samples);
}
