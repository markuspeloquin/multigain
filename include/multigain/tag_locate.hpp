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

#ifndef MULTIGAIN_TAG_LOCATE_HPP
#define MULTIGAIN_TAG_LOCATE_HPP

#include <cstdint>
#include <fstream>
#include <list>

#include <multigain/errors.hpp>

namespace multigain {

enum class tag_type {
	UNDEFINED = 0,
	APE_1,		/**< APE-1.0 */
	APE_2,		/**< APE-2.0 */
	APE_UNDEFINED,	/**< An APE-x tag with a known size */
	ID3_1,		/**< ID3-1.0 */
	ID3_1_1,		/**< ID3-1.1 */
	ID3_2_3,		/**< ID3-2.3 */
	ID3_2_4,		/**< ID3-2.4 */
	ID3_2_UNDEFINED,	/**< An ID3-2.x tag with a known size */
	MPEG,		/**< Not a tag, but an MPEG frame */
	MP3_INFO,
	MP3_XING
};

/** Type and boundary of a tag */
struct tag_info {
	tag_info(tag_type type, off_t start, size_t size) :
		start(start), size(size), type(type)
	{}

	off_t		start;
	size_t		size;
	tag_type	type;

	union {
		struct {
			// only for MP3_INFO or MP3_XING
			uint16_t	skip_front;
			uint16_t	skip_back;
		} info;

		// only for MPEG
		uint32_t	count;
	} extra;
};

/** Find the types and boundaries of the tags in a file
 *
 * \param in	The media file
 * \param out	The tag types and boundaries
 * \throw Disk_error	A read/seek error
 * \throw Unsupported_tag	Either a tag is an unsupported version with
 *	reserved bits set, or a prefixing tag is unrecognized.  Note that if
 *	this is thrown and out.empty(), then it is reasonable to assume that
 *	this file is not at all supported.
 */
void	find_tags(std::ifstream &in, std::list<tag_info> &out) noexcept(false);

void	dump_tags(const std::list<tag_info> &);

}

#endif
