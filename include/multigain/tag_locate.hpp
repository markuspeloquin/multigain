#ifndef MULTIGAIN_TAG_LOCATE_HPP
#define MULTIGAIN_TAG_LOCATE_HPP

#include <fstream>
#include <list>
#include <tr1/cstdint>

#include <multigain/errors.hpp>

namespace multigain {

enum tag_type {
	TAG_UNDEFINED = 0,
	TAG_APE_1,
	TAG_APE_2,
	TAG_APE_UNDEFINED,
	TAG_ID3_1,
	TAG_ID3_1_1,
	TAG_ID3_2_3,
	TAG_ID3_2_4,
	TAG_ID3_2_UNDEFINED,
	TAG_MPEG
};

struct tag_info {
	tag_info(enum tag_type type, off_t start, size_t size) :
		start(start), size(size), type(type)
	{}

	off_t		start;
	size_t		size;
	enum tag_type	type;
};

void	find_tags(std::ifstream &, std::list<tag_info> &out)
	    throw (Disk_error, Unsupported_tag);

}

#endif
