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

#include <fstream>
#include <list>
#include <tr1/cstdint>

#include <multigain/errors.hpp>

namespace multigain {

enum tag_type {
	TAG_UNDEFINED = 0,
	TAG_APE_1,		/**< APE-1.0 */
	TAG_APE_2,		/**< APE-2.0 */
	TAG_APE_UNDEFINED,	/**< An APE-x tag with a known size */
	TAG_ID3_1,		/**< ID3-1.0 */
	TAG_ID3_1_1,		/**< ID3-1.1 */
	TAG_ID3_2_3,		/**< ID3-2.3 */
	TAG_ID3_2_4,		/**< ID3-2.4 */
	TAG_ID3_2_UNDEFINED,	/**< An ID3-2.x tag with a known size */
	TAG_MPEG		/**< Not a tag, but an MPEG frame */
};

/** Type and boundary of a tag */
struct tag_info {
	tag_info(enum tag_type type, off_t start, size_t size) :
		start(start), size(size), type(type)
	{}

	off_t		start;
	size_t		size;
	enum tag_type	type;
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
void	find_tags(std::ifstream &in, std::list<tag_info> &out)
	    throw (Disk_error, Unsupported_tag);

void	dump_tags(const std::list<tag_info> &);

}

#endif
