#include <endian.h>

#include <cassert>
#include <fstream>
#include <list>
#include <tr1/cstdint>

#include <lame/lame.h>

namespace {

enum tag_type {
	TAG_UNDEFINED = 0,
	TAG_APE_1,
	TAG_APE_2,
	TAG_ID3_1,
	TAG_ID3_1_1,
	TAG_ID3_2_3,
	TAG_ID3_2_4,
	TAG_MEDIA
};

enum mpg_frame_type {
	MPG_1_1,
	MPG_1_2,
	MPG_1_3,
	MPG_2_1,
	MPG_2_2,
	MPG_2_3,
	MPG_UNDEFINED
};

/*
uint16_t MPG_BITRATE[6][16] = {
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
    {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 0},
    {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
    {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
    {0,  8, 16, 24,  32,  64,  80,  56,  64, 128, 160, 112, 128, 256, 320, 0}
};
uint32_t MPG_FREQ[2][4] = {
    { 44100, 48000, 32000, 0 },
    { 22050, 24000, 16000, 0 }
};
*/

struct tag_info {
	tag_info(enum tag_type type, off_t start, size_t size) :
		start(start), size(size), type(type)
	{}

	off_t		start;
	size_t		size;
	enum tag_type	type;
};

inline uint16_t
buf_safe16(const uint8_t buf[2])
{
	return buf[0] << 7 | buf[1];
}
inline uint16_t
buf_unsafe16(const uint8_t buf[2])
{
	return buf[0] << 8 | buf[1];
}
inline uint32_t
buf_safe32(const uint8_t buf[4])
{
	return buf[0] << 21 | buf[1] << 14 | buf[2] << 7 | buf[3];
}
inline uint32_t
buf_unsafe32(const uint8_t buf[4])
{
	return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
}

bool
skip_id3_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags)
{
	const size_t	SZ = 10;
	uint8_t		buf[SZ];
	off_t		pos;
	uint32_t	size;
	enum tag_type	type;
	bool		footer;

	pos = in.tellg();

	if (reversed) {
		in.seekg(-10, std::ios_base::cur);
		in.read(reinterpret_cast<char *>(buf), 10);

		assert(std::equal(buf, buf + 3, "3DI"));
	} else {
		in.read(reinterpret_cast<char *>(buf), 10);

		assert(std::equal(buf, buf + 3, "ID3"));
	}

	if (buf[3] == 3 && buf[4] == 0)
		type = TAG_ID3_2_3;
	else if (buf[3] == 4 && buf[4] == 0)
		type = TAG_ID3_2_4;
	else
		return false;

	footer = buf[6] & 0x10;
	size = buf_safe32(buf + 6) + 10;
	if (footer) size += 10;

	if (reversed) {
		out_tags.push_back(tag_info(type, pos - size, size));
		in.seekg(pos - size, std::ios_base::beg);
	} else {
		out_tags.push_back(tag_info(type, pos, size));
		in.seekg(pos + size, std::ios_base::beg);
	}
	return true;
}

bool
skip_ape_1(std::ifstream &in, std::list<tag_info> &out_tags)
{
	const size_t	SZ = 16;
	off_t		pos;

	pos = in.tellg();

	for (;;) {
		uint8_t		buf[SZ];
		uint32_t	len;
		char		c;

		in.read(reinterpret_cast<char *>(buf), 8);
		if (std::equal(buf, buf + 8, "APETAGX")) {
			uint32_t	size;
			uint32_t	vers;
			enum tag_type	type;

			in.read(reinterpret_cast<char *>(buf), 16);
			std::copy(buf, buf + 4,
			    reinterpret_cast<uint8_t *>(&vers));
			std::copy(buf + 4, buf + 8,
			    reinterpret_cast<uint8_t *>(&size));
			vers = le32toh(vers);
			size = le32toh(size);

			switch (vers) {
			case 1000: type = TAG_APE_1; break;
			case 2000: type = TAG_APE_2; break;
			default: return false;
			}

			out_tags.push_back(tag_info(type, pos, size));
			break;
		}

		// read len
		std::copy(buf, buf + 4, reinterpret_cast<uint8_t *>(&len));
		len = le32toh(len);

		// ignore flags; skip key and terminating 0x00
		while ((c = in.get()));

		// skip value
		in.seekg(len, std::ios_base::cur);
	}

	return true;
}

bool
skip_ape_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags)
{
	const size_t	SZ = 24;
	uint8_t		buf[SZ];
	off_t		pos;
	uint32_t	size;
	uint32_t	vers;
	enum tag_type	type;

	pos = in.tellg();

	if (reversed)
		in.seekg(-24, std::ios_base::cur);

	in.read(reinterpret_cast<char *>(buf), 24);
	assert(std::equal(buf, buf + 8, "APETAGX"));

	std::copy(buf + 8, buf + 12, reinterpret_cast<uint8_t *>(&vers));
	std::copy(buf + 12, buf + 16, reinterpret_cast<uint8_t *>(&size));
	vers = le32toh(vers);
	size = le32toh(size);

	switch (vers) {
	case 1000: type = TAG_APE_1; break;
	case 2000: type = TAG_APE_2; break;
	default: return false;
	}

	// word 6, bit 30 (little endian)
	if (buf[23] & 0x40)
		// add footer size
		size += 32;

	if (reversed) {
		out_tags.push_back(tag_info(type, pos - size, size));
		in.seekg(pos - size, std::ios_base::beg);
	} else {
		out_tags.push_back(tag_info(type, pos, size));
		in.seekg(pos + size, std::ios_base::beg);
	}
	return true;
}

void
determine_tagging(std::ifstream &in, std::list<tag_info> &out_tags)
{
	const size_t	SZ = 32;
	uint8_t		buf[SZ];
	off_t		pos;

	std::list<tag_info>::iterator	iter_media;

	for (pos = 0;;) {
		in.seekg(pos, std::ios_base::beg);
		in.read(reinterpret_cast<char *>(buf), 8);
		in.seekg(pos, std::ios_base::beg);

		if (buf[0] == 0xff && (buf[1] & 0xf0) == 0xf0) {
			// start of MP3 data
			out_tags.push_back(tag_info(TAG_MEDIA, pos, 0));
			--(iter_media = out_tags.end());
			break;

			/*
			enum mpg_frame_type	frame_type;
			uint32_t	frequency;
			uint16_t	bit_rate;
			bool		padded;

			switch ((buf[1] >> 1) & 0x7) {
			case 0x1: frame_type = MPG_1_1; break;
			case 0x2: frame_type = MPG_1_2; break;
			case 0x3: frame_type = MPG_1_3; break;
			case 0x5: frame_type = MPG_2_1; break;
			case 0x6: frame_type = MPG_2_2; break;
			case 0x7: frame_type = MPG_2_3; break;
			default: return;
			}

			bit_rate = MPG_BITRATE[frame_type][buf[2] >> 4] *
			    1000;
			if (!bit_rate) return;

			frequency = (buf[2] >> 2) & 0x3;
			frequency = MPG_FREQ[
			    frame_type < MPG_2_1 ? 0 : 1][frequency];
			if (!frequency) return;

			padded = buf[2] & 0x2 != 0;

			//size = (144 * bit_rate / sample_rate) + padded;
			*/
		} else if (std::equal(buf, buf + 3, "ID3")) {
			if (!skip_id3_2(in, false, out_tags)) {
				return;
			}
			pos += out_tags.back().size;
		} else if (std::equal(buf, buf + 8, "APETAGEX")) {
			if (!skip_ape_1(in, out_tags)) {
				return;
			}
			pos += out_tags.back().size;
		} else {
			// maybe APE 1?  if it isn't, go on a wild ride
			if (!skip_ape_1(in, out_tags)) {
				return;
			}
			pos += out_tags.back().size;
		}
	}

	in.seekg(0, std::ios_base::end);
	pos = in.tellg();

	for (;;) {
		in.seekg(pos - 10, std::ios_base::beg);
		in.read(reinterpret_cast<char *>(buf), 3);
		if (std::equal(buf, buf + 3, "3DI")) {
			in.seekg(pos, std::ios_base::beg);
			if (!skip_id3_2(in, true, out_tags)) {
				return;
			}
			pos -= out_tags.back().size;
			continue;
		}

		in.seekg(pos - 32, std::ios_base::beg);
		in.read(reinterpret_cast<char *>(buf), 8);
		if (std::equal(buf, buf + 8, "APETAGEX")) {
			in.seekg(pos, std::ios_base::beg);
			if (!skip_ape_2(in, true, out_tags)) {
				return;
			}
			pos -= out_tags.back().size;
			continue;
		}

		in.seekg(pos - 128, std::ios_base::beg);
		in.read(reinterpret_cast<char *>(buf), 128);
		if (std::equal(buf, buf + 3, "TAG")) {
			enum tag_type	type;
			if (!buf[125] && buf[126])
				type = TAG_ID3_1_1;
			else
				type = TAG_ID3_1;
			out_tags.push_back(tag_info(type, pos - 128, 128));
			pos -= 128;
			continue;
		}

		// no other tags found, so must be complete
		break;
	}

	iter_media->size = pos - iter_media->start;
}

}

int
decode_mp3(const std::string &path)
{
	static bool _lame_init = false;
	if (!_lame_init) {
		if (lame_decode_init() == -1) {
			fprintf(stderr, "lame failed to initialize\n");
			return -1;
		}
		_lame_init = true;
	}

	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);

	return -1;
}
