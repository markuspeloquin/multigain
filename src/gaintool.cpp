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
#include "lame.hpp"

namespace {

inline double
sample_i2d(int16_t sample) {
	/*
	if (sample < 0)
		return sample * 32767.0 / 32768;
	*/
	return sample;
}

} // end anon

int
main(int argc, char **argv) {
	using namespace multigain;

	const size_t SAMPLES = 4096;

	assert(argc > 1);

	std::string path = argv[1];
	std::ifstream file;

	file.open(path.c_str(), std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << *argv << ": failed to open file\n";
		return 1;
	}

	Mpeg_decoder		decoder(file);
	Audio_buffer		audio_buf(SAMPLES);
	std::unique_ptr<Analyzer>	analyzer;
	std::pair<size_t, size_t>	counts;
	uint16_t		frequency;

	std::unique_ptr<double> dbuf(new double[2 * SAMPLES]);
	double *ldbuf = dbuf.get();
	double *rdbuf = dbuf.get() + SAMPLES;

	uint32_t total = 0;

	for (;;) {
		counts = decoder.decode(&audio_buf);
		uint16_t freq = audio_buf.frequency();
		if (!counts.second)
			break;
		else if (!analyzer) {
			frequency = freq;
			analyzer = std::make_unique<Analyzer>(freq);
		} else if (frequency != freq) {
			frequency = freq;
			analyzer->reset_sample_frequency(frequency);
		}

		total += counts.second;

		uint8_t channels = audio_buf.channels();
		const int16_t *lsamp = audio_buf.samples()[0];
		const int16_t *rsamp = channels == 1 ?
		    audio_buf.samples()[0] : audio_buf.samples()[1];

		// convert to doubles
		for (size_t i = 0; i < counts.second; i++)
			ldbuf[i] = sample_i2d(lsamp[i]);
		if (channels != 1)
			for (size_t i = 0; i < counts.second; i++)
				rdbuf[i] = sample_i2d(rsamp[i]);

		if (!analyzer->add(ldbuf, rdbuf, counts.second, channels)) {
			std::cerr << "what\n";
			return 1;
		}
	}

	if (!analyzer) {
		std::cerr << "failed to read anything\n";
		return 1;
	}

	Sample sample;
	analyzer->pop(&sample);
	std::cout << "gain: " << sample.adjustment() << " dB\n";

	return 0;
}
