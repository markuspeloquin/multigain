#include <cassert>
#include <iostream>
#include <memory>

#include <multigain/decode.hpp>
#include <multigain/gain_analysis.hpp>
#include <multigain/tag_locate.hpp>

int
main(int argc, char **argv)
{
	using namespace multigain;

	assert(argc > 1);

	std::string path = argv[1];
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	std::list<tag_info> tags;
	find_tags(file, tags);

	off_t pos_begin = -1;
	off_t pos_end = -1;

	dump_tags(tags);

	for (std::list<tag_info>::const_iterator i = tags.begin();
	    i != tags.end(); ++i)
		if (i->type == TAG_MPEG) {
			pos_begin = i->start;
			pos_end = pos_begin + i->size;
			break;
		}

	if (pos_begin == -1) {
		std::cerr << "no media found\n";
		return 1;
	}

	Mpeg_decoder		decoder(file, pos_begin, pos_end);
	std::auto_ptr<Analyzer>	analyzer;
	uint32_t		frequency = 0;

	for (;;) {
		struct decode_info	info;
		double		left[Mpeg_decoder::MAX_SAMPLES];
		double		right[Mpeg_decoder::MAX_SAMPLES];
		off_t		consumed;
		std::pair<size_t, size_t> counts;

		consumed = static_cast<off_t>(file.tellg()) - pos_begin;
		counts = decoder.decode_frame(left, right, &info);
		if (!counts.second) break;
		if (!frequency) {
			frequency = info.frequency;
			analyzer.reset(new Analyzer(frequency));
		} else if (frequency != info.frequency) {
			// it is possible, I just don't want to do it; it
			// requires multiple Analyzer objects or maybe
			// modifying the replaygain context object
			std::cerr
			    << "changing frequencies is not supported\n";
			return 1;
		}

		std::cout << counts.first << '\t' << counts.second << '\n';

		if (!analyzer->add(left, right, counts.second,
		    info.channels)) {
			std::cerr << "what\n";
			return 1;
		}
	}

	if (!analyzer.get()) {
		std::cerr << "failed to read anything\n";
		return 1;
	}

	Sample sample;
	analyzer->pop(&sample);
	std::cout << "gain: " << sample.adjustment() << "dB\n";

	return 0;
}
