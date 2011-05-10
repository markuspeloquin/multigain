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

#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

#include <lame/lame.h>

#include <multigain/decode.hpp>
#include <multigain/gain_analysis.hpp>
#include <multigain/tag_locate.hpp>
#include "lame.hpp"

namespace {

inline double
sample_i2d(int16_t sample)
{
	/*
	if (sample < 0)
		return sample * 32767.0 / 32768;
	*/
	return sample;
}

} // end anon

int
main(int argc, char **argv)
{
	using namespace multigain;

	assert(argc > 1);

	std::string path = argv[1];
	std::ifstream file;

	off_t pos_begin = -1;
	off_t pos_end = -1;
	uint16_t skip_front = -1;
	uint16_t skip_back = 0;
	uint32_t frames;

	file.open(path.c_str(), std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << *argv << ": failed to open file\n";
		return 1;
	}

	std::list<tag_info> tags;
	find_tags(file, tags);
	dump_tags(tags);

	for (std::list<tag_info>::const_iterator i = tags.begin();
	    i != tags.end(); ++i)
		switch (i->type) {
		case TAG_MPEG:
			pos_begin = i->start;
			pos_end = pos_begin + i->size;
			frames = i->extra.count;
			break;
		case TAG_MP3_INFO:
		case TAG_MP3_XING:
			skip_front = i->extra.info.skip_front;
			skip_back = i->extra.info.skip_back;
			break;
		default:
			;
		}

	if (skip_front == -1) {
		lame_global_flags *lame = Lame_lib::init();
		skip_front = lame_get_encoder_delay(lame) + 528 + 1;
	} else {
		skip_front += 528 + 1;
		skip_back -= 528 + 1;
		if (skip_back < 0) skip_back = 0;
	}

	if (pos_begin == -1) {
		std::cerr << "no media found\n";
		return 1;
	}

	boost::scoped_array<int16_t> leftbuf;
	boost::scoped_array<int16_t> rightbuf;
	boost::scoped_array<double> leftbuf_double;
	boost::scoped_array<double> rightbuf_double;
	size_t	buf_cap = std::max(skip_front, skip_back) +
		    Mpeg_decoder::MAX_SAMPLES;
	size_t	buf_cnt = 0;

	leftbuf.reset(new int16_t[buf_cap]);
	rightbuf.reset(new int16_t[buf_cap]);
	leftbuf_double.reset(new double[buf_cap]);
	rightbuf_double.reset(new double[buf_cap]);

	Mpeg_decoder		decoder(file, pos_begin, pos_end);
	Frame			frame;
	std::auto_ptr<Analyzer>	analyzer;
	std::pair<size_t, size_t> counts;
	uint16_t		frequency;

	int16_t max = 0;
	int16_t min = 0;

	for (;;) {
		off_t			consumed;

		consumed = static_cast<off_t>(file.tellg()) - pos_begin;
		counts = decoder.decode_frame(&frame);
		uint16_t freq = frame.frequency();
		if (!counts.second) break;
		if (!analyzer.get()) {
			frequency = freq;
			analyzer.reset(new Analyzer(freq));
		} else if (frequency != freq) {
			frequency = freq;
			analyzer->reset_sample_frequency(frequency);
		}

		uint8_t channels = frame.channels();
		const int16_t *left = frame.samples()[0];
		const int16_t *right = channels == 1 ?
		    frame.samples()[0] : frame.samples()[1];

		if (skip_front) {
			left += skip_front;
			right += skip_front;
			if (counts.second >= skip_front) {
				// ignore first 'skip_front' samples
				counts.second -= skip_front;
				skip_front = 0;
			} else {
				// ignore whole frame
				skip_front -= counts.second;
				counts.second = 0;
			}
		}

		// copy into buffers, analyze the samples if there's enough
		while (counts.second) {
			size_t copy_amt = buf_cap - buf_cnt;
			if (counts.second < copy_amt)
				copy_amt = counts.second;

			// copy samples into the large buffers
			std::copy(left, left + copy_amt,
			    leftbuf.get() + buf_cnt);
			std::copy(right, right + copy_amt,
			    rightbuf.get() + buf_cnt);
			buf_cnt += copy_amt;
			counts.second -= copy_amt;

			// not enough to guarantee skip_back samples are
			// ignored
			if (buf_cnt < skip_back)
				break;

			// amount of samples to guarantee skip_back samples
			// are ignored
			size_t usable = buf_cnt - skip_back;

			// convert to doubles
			for (size_t i = 0; i < usable; i++) {
				int16_t min_ = std::min(leftbuf[i], rightbuf[i]);
				int16_t max_ = std::max(leftbuf[i], rightbuf[i]);
				min = std::min(min_, min);
				max = std::max(max_, max);

				leftbuf_double[i] = sample_i2d(leftbuf[i]);
				rightbuf_double[i] = sample_i2d(rightbuf[i]);
			}

			if (!analyzer->add(leftbuf_double.get(),
			    rightbuf_double.get(), usable, 2)) {
				std::cerr << "what\n";
				return 1;
			}

			// move data left (skip_back samples are moved)
			// usable is definitely <= buf_cnt
			std::copy(leftbuf.get() + usable,
			    leftbuf.get() + buf_cnt, leftbuf.get());
			std::copy(rightbuf.get() + usable,
			    rightbuf.get() + buf_cnt, rightbuf.get());
			buf_cnt -= usable;
		}
	}

	assert(buf_cnt == skip_back);

	if (!analyzer.get()) {
		std::cerr << "failed to read anything\n";
		return 1;
	}

	Sample sample;
	analyzer->pop(&sample);
	std::cout << "gain: " << sample.adjustment() << " dB\n";

	return 0;
}
