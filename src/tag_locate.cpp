#include <endian.h>

#ifdef _TEST
#	include <iostream>
#endif

#include <cassert>
#include <algorithm>

#include <lame/lame.h>

#include <multigain/tag_locate.hpp>

namespace multigain {
namespace {

#ifdef _TEST
enum mpg_frame_type {
	MPG_1_1,
	MPG_1_2,
	MPG_1_3,
	MPG_2_1,
	MPG_2_2,
	MPG_2_3,
	MPG_UNDEFINED
};
#endif

// also an ape footer
struct ape_header {
	// all uint32_t are LE
	uint8_t		id[8];
	uint32_t	version;
	uint32_t	size;
	uint32_t	items;
	uint32_t	flags;
	uint8_t		reserved[8];
};

// use these instead of sizeof(id3_2_header), since
// [sizeof(id3_2_header) != 0 mod 4]:
const size_t SZ_ID3_2_HEADER = 10;
const size_t SZ_ID3_2_FOOTER = 10;

struct id3_2_header {
	uint8_t		id[0x3];
	uint8_t		version[0x2];
	uint8_t		flags;
	uint8_t		size[0x4];
};

struct id3_1_tag {
/*00*/	uint8_t				id    [0x03];
/*03*/	uint8_t				title [0x1e];
/*21*/	uint8_t				artist[0x1e];
/*3f*/	uint8_t				album [0x1e];
/*5d*/	uint8_t				year  [0x04];
	union {
		struct {
/*61*/			uint8_t		comment[0x1e];
		} id3_1;
		struct {
/*61*/			uint8_t		comment[0x1c];
/*7d*/			uint8_t		__padding;
/*7e*/			uint8_t		track;
		} id3_1_1;
	} ct; // comment and/or track
/*7f*/	uint8_t				genre;
/*80*/
};

#ifdef _TEST
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

	// fragment to get the size of a frame
	/*
	enum mpg_frame_type	frame_type;
	uint32_t		frequency;
	uint16_t		bit_rate;
	uint16_t		size;
	bool			padded;

	switch ((buf[1] >> 1) & 0x7) {
	case 0x1: frame_type = MPG_2_3; break;
	case 0x2: frame_type = MPG_2_2; break;
	case 0x3: frame_type = MPG_2_1; break;
	case 0x5: frame_type = MPG_1_3; break;
	case 0x6: frame_type = MPG_1_2; break;
	case 0x7: frame_type = MPG_1_1; break;
	default: return;
	}

	bit_rate = MPG_BITRATE[frame_type][buf[2] >> 4] *
	    1000;
	if (!bit_rate) return;

	frequency = (buf[2] >> 2) & 0x3;
	frequency = MPG_FREQ[
	    frame_type < MPG_1_1 ? 1 : 0][frequency];
	if (!frequency) return;

	padded = buf[2] & 0x2 != 0;

	size = (144 * bit_rate / sample_rate) + padded;
	*/
#endif

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

void
skip_id3_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags)
    throw (Disk_error, Unsupported_tag)
{
	struct id3_2_header	header;
	off_t			pos;
	uint32_t		size;
	enum tag_type		type;

	pos = in.tellg();

	// read full header
	if (reversed) {
		if (!in.seekg(-SZ_ID3_2_FOOTER, std::ios_base::cur))
			throw Disk_error("seek error");
		if (!in.read(reinterpret_cast<char *>(&header),
		    SZ_ID3_2_FOOTER))
			throw Disk_error("read error");

		assert(std::equal(header.id, header.id + 3, "3DI"));
	} else {
		if (!in.read(reinterpret_cast<char *>(&header),
		    SZ_ID3_2_HEADER))
			throw Disk_error("read error");

		assert(std::equal(header.id, header.id + 3, "ID3"));
	}

	// check version
	if (header.version[0] == 3 && header.version[1] == 0)
		type = TAG_ID3_2_3;
	else if (header.version[0] == 4 && header.version[1] == 0)
		type = TAG_ID3_2_4;
	else {
		// ensure all unknown flags are zero
		if (header.flags & 0x0f)
			throw Unsupported_tag("ID3-2.x with unknown flags");
		type = TAG_ID3_2_UNDEFINED;
	}

	// get size for whole tag
	size = buf_safe32(header.size) + SZ_ID3_2_HEADER;
	if (header.flags & 0x10) size += SZ_ID3_2_FOOTER;

	if (reversed)
		pos -= size;
	out_tags.push_back(tag_info(type, pos, size));
}

// return false iff tag unrecognized; if tag had an unknown version, it is
// recorded as TAG_APE_UNDEFINED in 'out_tags', but APE is concistent enough
// that the size can be known
bool
skip_ape_1(std::ifstream &in, std::list<tag_info> &out_tags)
    throw (Disk_error, Unsupported_tag)
{
	union {
		struct ape_header	footer;	// if footer
		uint32_t		len;	// if tag item
	} buf;

	off_t			pos;
	struct ape_header	*footer = &buf.footer;

	pos = in.tellg();

	for (;;) {
		char	c;

		if (!in.read(reinterpret_cast<char *>(&buf), 8))
			throw Disk_error("read error");
		if (std::equal(footer->id, footer->id + 8, "APETAGX")) {
			uint32_t	flags;
			uint32_t	size;
			enum tag_type	type;

			// read remainder
			if (!in.read(reinterpret_cast<char *>(&buf) + 8,
			    sizeof(ape_header) - 8))
				throw Disk_error("read error");

			flags = le32toh(footer->flags);

			switch (le32toh(footer->version)) {
			case 1000: type = TAG_APE_1; break;
			case 2000: type = TAG_APE_2; break;
			default:
				// ensure all unknown flags are zero
				if (flags & 0x1ffffff8 ||
				    std::max_element(footer->reserved,
				    footer->reserved +
				    sizeof(footer->reserved)) != 0)
					throw Unsupported_tag(
					    "APE with unknown flags");
				type = TAG_APE_UNDEFINED;
			}

			// footer + all of tag items (no header)
			size = le32toh(footer->size);

			if (flags & 0x80000000)
				// add header size
				size += sizeof(ape_header);

			if (pos + size != in.tellg())
				// okay, so by some 'random' chance, we
				// started before the APE tag, but ended up
				// finding one beyond all odds
				return false;

			out_tags.push_back(tag_info(type, pos, size));
			break;
		}

		// ignore flags (second word of tag item, read in above)

		// skip key and terminating 0x00
		while ((c = in.get()));

		// skip value
		if (!in.seekg(le32toh(buf.len), std::ios_base::cur))
			// either the tag is corrupt, or this wasn't an APE
			// tag to begin with
			return false;
	}

	return true;
}

void
skip_ape_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags)
    throw (Disk_error, Unsupported_tag)
{
	struct ape_header	footer;
	off_t			pos;
	uint32_t		flags;
	uint32_t		size;
	enum tag_type		type;

	pos = in.tellg();

	if (reversed)
		if (!in.seekg(-sizeof(footer), std::ios_base::cur))
			throw Disk_error("seek error");

	// read all but last two words
	if (!in.read(reinterpret_cast<char *>(&footer), sizeof(footer) - 8))
		throw Disk_error("read error");
	assert(std::equal(footer.id, footer.id + 8, "APETAGX"));

	flags = le32toh(footer.flags);
	switch (le32toh(footer.version)) {
	case 1000: type = TAG_APE_1; break;
	case 2000: type = TAG_APE_2; break;
	default:
		// ensure all unknown flags are zero
		if (flags & 0x1ffffff8 || std::max_element(footer.reserved,
		    footer.reserved + sizeof(footer.reserved)) != 0)
			throw Unsupported_tag("APE with unknown flags");
		type = TAG_APE_UNDEFINED;
	}

	// footer + all tag items (no header)
	size = le32toh(footer.size);

	if (flags & 0x80000000)
		// add header size
		size += sizeof(ape_header);

	if (reversed) {
		out_tags.push_back(tag_info(type, pos - size, size));
		if (!in.seekg(pos - size, std::ios_base::beg))
			throw Disk_error("seek error");
	} else {
		out_tags.push_back(tag_info(type, pos, size));
		if (!in.seekg(pos + size, std::ios_base::beg))
			throw Disk_error("seek error");
	}
}

} // end anon
} // end multigain

void
multigain::find_tags(std::ifstream &in, std::list<tag_info> &out_tags)
    throw (Disk_error, Unsupported_tag)
{
	struct id3_1_tag	tag31;
	// big enough for the longest id: 'APETAGEX'
	uint8_t			buf[8];
	off_t			pos;

	std::list<tag_info>::iterator	iter_media;

	for (pos = 0;;) {
		if (!in.seekg(pos, std::ios_base::beg))
			throw Disk_error("seek error");
		if (!in.read(reinterpret_cast<char *>(buf), 8))
			throw Disk_error("read error");
		if (!in.seekg(pos, std::ios_base::beg))
			throw Disk_error("seek error");

		if (buf[0] == 0xff && (buf[1] & 0xf0) == 0xf0) {
			// start of MP3 data
			out_tags.push_back(tag_info(TAG_MEDIA, pos, 0));
			--(iter_media = out_tags.end());
			break;
		} else if (std::equal(buf, buf + 3, "ID3")) {
			skip_id3_2(in, false, out_tags);
			pos += out_tags.back().size;
		} else if (std::equal(buf, buf + 8, "APETAGEX")) {
			skip_ape_2(in, false, out_tags);
			pos += out_tags.back().size;
		} else {
			// maybe APE 1?  if it isn't, go on a wild ride
			if (!skip_ape_1(in, out_tags))
				throw Unsupported_tag(
				    "completely unrecognized");
			pos += out_tags.back().size;
		}
	}

	if (!in.seekg(0, std::ios_base::end))
		throw Disk_error("seek error");
	pos = in.tellg();

	for (;;) {
		if (!in.seekg(pos - sizeof(tag31), std::ios_base::beg))
			throw Disk_error("seek error");
		in.read(reinterpret_cast<char *>(&tag31),
		    sizeof(tag31));

		if (std::equal(tag31.id, tag31.id + 3, "TAG")) {
			enum tag_type	type;
			if (!tag31.ct.id3_1_1.__padding &&
			    tag31.ct.id3_1_1.track)
				type = TAG_ID3_1_1;
			else
				type = TAG_ID3_1;
			out_tags.push_back(tag_info(type, pos - sizeof(tag31),
			    sizeof(tag31)));
			pos -= sizeof(tag31);
			continue;
		}

		if (!in.seekg(pos - sizeof(struct ape_header),
		    std::ios_base::beg))
			throw Disk_error("seek error");
		if (!in.read(reinterpret_cast<char *>(buf), 8))
			throw Disk_error("read error");
		if (std::equal(buf, buf + 8, "APETAGEX")) {
			if (!in.seekg(pos, std::ios_base::beg))
				throw Disk_error("seek error");
			skip_ape_2(in, true, out_tags);
			pos -= out_tags.back().size;
			continue;
		}

		if (!in.seekg(pos - SZ_ID3_2_FOOTER, std::ios_base::beg))
			throw Disk_error("seek error");
		if (!in.read(reinterpret_cast<char *>(buf), 3))
			throw Disk_error("read error");
		if (std::equal(buf, buf + 3, "3DI")) {
			if (!in.seekg(pos, std::ios_base::beg))
				throw Disk_error("seek error");
			skip_id3_2(in, true, out_tags);
			pos -= out_tags.back().size;
			continue;
		}

		// no other tags found, so must be complete
		break;
	}

	iter_media->size = pos - iter_media->start;
}

/*
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
*/

#ifdef _TEST
int main(int argc, char **argv)
{
	using namespace multigain;

	assert(argc > 1);

	std::string path = argv[1];
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	std::list<tag_info> tags;
	find_tags(file, tags);

	for (std::list<tag_info>::const_iterator i = tags.begin();
	    i != tags.end(); ++i) {
		const char *name;
		switch (i->type) {
		case TAG_UNDEFINED:	name = "TAG_UNDEFINED";	break;
		case TAG_APE_1:		name = "TAG_APE_1";	break;
		case TAG_APE_2:		name = "TAG_APE_2";	break;
		case TAG_APE_UNDEFINED:	name = "TAG_APE_UNDEFINED";	break;
		case TAG_ID3_1:		name = "TAG_ID3_1";	break;
		case TAG_ID3_1_1:	name = "TAG_ID3_1_1";	break;
		case TAG_ID3_2_3:	name = "TAG_ID3_2_3";	break;
		case TAG_ID3_2_4:	name = "TAG_ID3_2_4";	break;
		case TAG_APE_UNDEFINED:	name = "TAG_ID3_2_UNDEFINED";	break;
		case TAG_MEDIA:		name = "TAG_MEDIA";	break;
		}

		std::cout << name << ": " << i->start << ", "
		    << i->size << '\n';
	}

	return 0;
}
#endif
