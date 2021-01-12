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

#include <endian.h>

#include <cassert>
#include <algorithm>
#include <iostream>
#include <memory>

#include <multigain/tag_locate.hpp>
#include <multigain/decode.hpp>

namespace multigain {
namespace {

// also an ape footer
struct ape_header {
	// all uint32_t are LE
/*00*/	uint8_t		id[8];
/*08*/	uint32_t	version;
/*0c*/	uint32_t	size;
/*10*/	uint32_t	items;
/*14*/	uint32_t	flags;
/*18*/	uint8_t		reserved[8];
/*20*/
};

const size_t SZ_ID3_2_HEADER = 10;
const size_t SZ_ID3_2_FOOTER = 10;

// never use sizeof() with this structure; depending on the architecture's
// alignment, it may be 10 or 12; use SZ_ID3_2_*
struct id3_2_header {
/*00*/	uint8_t		id[0x3];
/*03*/	uint8_t		version[0x2];
/*05*/	uint8_t		flags;
	// BE, synchsafe
/*06*/	uint8_t		size[0x4];
/*0a*/
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

#if 0
inline uint16_t buf_safe16(const uint8_t buf[2]) {
	return buf[0] << 7 | buf[1];
}
#endif


#if 0
inline uint16_t buf_unsafe16(const uint8_t buf[2]) {
	return buf[0] << 8 | buf[1];
}
#endif


inline uint32_t buf_safe32(const uint8_t buf[4]) {
	return buf[0] << 21 | buf[1] << 14 | buf[2] << 7 | buf[3];
}


#if 0
inline uint32_t buf_unsafe32(const uint8_t buf[4]) {
	return buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3];
}
#endif

inline void
find_skip_amounts(const uint8_t *info, tag_info *tag_info)
{
	// LAME tag at 0x78
	const uint8_t *skip = info + 0x78 + 0x15;
	tag_info->extra.info.skip_front =
	    (static_cast<uint16_t>(skip[0]) << 4) | (skip[1] >> 4);
	tag_info->extra.info.skip_back =
	    (static_cast<uint16_t>(skip[1] & 0xf) << 8) | skip[2];
}

/// \throw Disk_error
/// \throw Unsupported_tag
void
skip_id3_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags) {
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

#if 0
// return false iff tag unrecognized; if tag had an unknown version, it is
// recorded as TAG_APE_UNDEFINED in 'out_tags', but APE is concistent enough
// that the size can be known
/// \throw Disk_error
/// \throw Unsupported_tag
bool
skip_ape_1(std::ifstream &in, std::list<tag_info> &out_tags) {
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
		if (!in.seekg(le32toh(buf.len), std::ios_base::cur)) {
			// either the tag is corrupt, or this wasn't an APE
			// tag to begin with
			in.clear();
			return false;
		}
	}

	return true;
}
#endif

/// \throw Disk_error
/// \throw Unsupported_tag)
void
skip_ape_2(std::ifstream &in, bool reversed, std::list<tag_info> &out_tags)
	noexcept(false)
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
{
	struct id3_1_tag	tag31;
	// big enough for the longest id: 'APETAGEX'
	uint8_t			buf[8];
	off_t			pos;

	std::list<tag_info>::iterator	iter_media;

	std::unique_ptr<uint8_t> mpeg_frame;
	size_t sz_frame = 0;

	// prefix tags
	pos = 0;
	for (;;) {
		if (!in.seekg(pos, std::ios_base::beg))
			throw Disk_error("seek error");
		if (!in.read(reinterpret_cast<char *>(buf), 8)) {
			if (!out_tags.empty() &&
			    out_tags.back().type == TAG_MPEG && in.eof())
				break;
			throw Disk_error("read error");
		}

		if (buf[0] == 0xff && (buf[1] & 0xf0) == 0xf0) {
			// MPEG frame
			uint16_t	size;

			try {
				Mpeg_frame_header frame_header(buf, true);
				size = frame_header.size();
			} catch (const Mpeg_frame_header::Bad_header &e) {
				throw Unsupported_tag("bad MPEG frame");
			}

			// allocate a buffer for MPEG frames
			if (sz_frame < size) {
				sz_frame = size;
				mpeg_frame.reset(new uint8_t[size]);
			}

			// read the whole frame
			if (!in.seekg(pos, std::ios_base::beg))
				throw Disk_error("seek error");
			if (!in.read(
			    reinterpret_cast<char *>(mpeg_frame.get()), size))
				throw Disk_error("read error");

			// this should advance at least once; I've seen a
			// standard claim there are 0x20 bytes of padding, but
			// ref_pink.mp3 has just 0x11 bytes
			uint8_t *info = mpeg_frame.get() + 4;
			while (!*info) ++info;
			bool some_zeros = info != mpeg_frame.get() + 4;

			if (some_zeros &&
			    std::equal(info, info + 4, "Xing")) {
				// MP3 Xing tag
				// add back frame header
				out_tags.push_back(
				    tag_info(TAG_MP3_XING, pos, size));

				find_skip_amounts(info, &out_tags.back());

				pos += size;
			} else if (some_zeros &&
			    std::equal(info, info + 4, "Info")) {
				// MP3 Info tag
				// add back frame header
				out_tags.push_back(
				    tag_info(TAG_MP3_INFO, pos, size));

				find_skip_amounts(info, &out_tags.back());

				pos += size;
			} else {
				// start of MP3 data
				if (!out_tags.empty() &&
				    out_tags.back().type == TAG_MPEG)
					iter_media->extra.count++;
				else {
					out_tags.push_back(
					    tag_info(TAG_MPEG, pos, 0));
					out_tags.back().extra.count = 0;
					--(iter_media = out_tags.end());
				}
				pos += size;
			}
		} else {
			if (!out_tags.empty() &&
			    out_tags.back().type == TAG_MPEG)
				break;
			else if (std::equal(buf, buf + 3, "ID3")) {
				if (!in.seekg(pos, std::ios_base::beg))
					throw Disk_error("seek error");
				skip_id3_2(in, false, out_tags);
				pos += out_tags.back().size;
			} else if (std::equal(buf, buf + 8, "APETAGEX")) {
				if (!in.seekg(pos, std::ios_base::beg))
					throw Disk_error("seek error");
				skip_ape_2(in, false, out_tags);
				pos += out_tags.back().size;
			} else
				throw Unsupported_tag(
				    "completely unrecognized");
		}
	}

	// end of MPEG data, the rest should be
	// scanned in reverse
	iter_media->size = pos - iter_media->start;
	if (in.eof()) {
		in.clear();
		return;
	}

	// seek to end to check for trailing tags; I'm certainly abusing the
	// seek function a bit from here on out, but the alternative is
	// writing a bunch of other code with its own overhead
	if (!in.seekg(0, std::ios_base::end))
		throw Disk_error("seek error");
	pos = in.tellg();

	// suffix tags
	// check for tag types from longest to shortest, to take advantage of
	// any caching (provided the C++ file buffering prevails, there is
	// only one read() system call)
	for (;;) {
		// ID3-1 / ID3-1.1
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

		// APE-x
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

		// ID3-2.x
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

		// no other tags found, so must have found all
		break;
	}
}

void
multigain::dump_tags(const std::list<tag_info> &tags)
{
	for (std::list<tag_info>::const_iterator i = tags.begin();
	    i != tags.end(); ++i) {
		const char *name;
		switch (i->type) {
		case TAG_UNDEFINED:
			name = "TAG_UNDEFINED";
			break;
		case TAG_APE_1:
			name = "TAG_APE_1";
			break;
		case TAG_APE_2:
			name = "TAG_APE_2";
			break;
		case TAG_APE_UNDEFINED:
			name = "TAG_APE_UNDEFINED";
			break;
		case TAG_ID3_1:
			name = "TAG_ID3_1";
			break;
		case TAG_ID3_1_1:
			name = "TAG_ID3_1_1";
			break;
		case TAG_ID3_2_3:
			name = "TAG_ID3_2_3";
			break;
		case TAG_ID3_2_4:
			name = "TAG_ID3_2_4";
			break;
		case TAG_ID3_2_UNDEFINED:
			name = "TAG_ID3_2_UNDEFINED";
			break;
		case TAG_MPEG:
			name = "TAG_MPEG";
			break;
		case TAG_MP3_INFO:
			name = "TAG_MP3_INFO";
			break;
		case TAG_MP3_XING:
			name = "TAG_MP3_XING";
			break;
		default:
			name = "";
			assert(0);
		}

		std::cout << name << ": " << i->start << ", "
		    << i->size << '\n';
	}
}
