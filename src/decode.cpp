#include <lame/lame.h>

#include <multigain/decode.hpp>
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

} // end anon
} // end multigain

void
multigain::decode_mp3_frame(std::ifstream &in, double *left, double *right)
{
	static bool _lame_init = false;
	if (!_lame_init) {
		if (lame_decode_init() == -1) {
			fprintf(stderr, "lame failed to initialize\n");
		}
		_lame_init = true;
	}
}

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
		case TAG_MPEG:		name = "TAG_MPEG";	break;
		}

		std::cout << name << ": " << i->start << ", "
		    << i->size << '\n';
	}

	return 0;
}
#endif
