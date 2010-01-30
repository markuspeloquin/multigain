/* ReplayGainAnalysis - analyzes input samples and give the recommended dB
 * change
 *
 * Copyright (C) 2010 Markus Peloquin
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
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * Outside this file it is LGPL.
 *
 */

/**
 * Pseudo-code to process an album:
 *
 *	double			l_samples[4096];
 *	double			r_samples[4096];
 *	multigain::Sample_accum	sample_accum;
 *
 *	std::auto_ptr<multigain::Analyzer>	rg;
 *
 *	try {
 *		rg.reset(new multigain::Analyzer(44100));
 *	} catch (const multigain::Bad_samplefreq &) {
 *		// handle error
 *	}
 *	for (unsigned i = 1; i <= num_songs; i++) {
 *		multigain::Sample	sample;
 *		size_t			num_samples;
 *
 *		while ((num_samples = getSongSamples(song[i],
 *		    l_samples, r_samples)) > 0) {
 *			rg->analyze_samples(l_samples, r_samples,
 *			    num_samples, 2);
 *		}
 *
 *		rg->pop(sample);
 *		sample_accum.add(sample);
 *
 *		try {
 *			double change = sample.adjustment();
 *			std::cout << "Recommended dB change for song " << i
 *			    << ": " << change << '\n';
 *		} catch (const multigain::Not_enough_samples &) {
 *			// ...
 *		}
 *	}
 *	try {
 *		double change = sample_accum.adjustment();
 *		std::cout << "Recommended dB change for whole album: "
 *		    << change << '\n';
 *	} catch (const multigain::Not_enough_samples &) {
 *		// ...
 *	}
 */

#ifndef GAIN_ANALYSIS_HPP
#define GAIN_ANALYSIS_HPP

#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>

#include <multigain/errors.hpp>

namespace multigain {

#include <multigain/gain_analysis.h>

/** A sample of a Replaygain calculation */
class Sample {
public:
	/** Real initialization comes from Analyzer::pop(). */
	Sample() {}

	/** How much to adjust by
	 *
	 * The result is undefined unless initialized with
	 * <code>Analyzer::pop()</code>.
	 *
	 * \return	The adjustment
	 * \throw Not_enough_samples	...
	 */
	double adjustment() const throw (Not_enough_samples)
	{
		double v = gain_adjustment(&_sample);
		if (v == GAIN_NOT_ENOUGH_SAMPLES) throw Not_enough_samples();
		return v;
	}

private:
	friend class Analyzer;
	friend class Sample_accum;

	struct replaygain_sample	_sample;
};

/** An accumulation of a number of samples */
class Sample_accum {
public:
	/** Construct and initialize to zero */
	Sample_accum()
	{	reset(); }

	/** Reset the sum to zero */
	void reset()
	{	memset(&_sum, 0, sizeof(_sum)); }

	/** Add a sample into the accumulation
	 *
	 * \param sample	The sample to add
	 */
	void add(const Sample &sample)
	{	*this += sample; }

	/** Add a sample into the accumulation
	 *
	 * \param sample	The sample to add
	 */
	Sample_accum &operator+=(const Sample &sample)
	{
		gain_sample_accum(&_sum, &sample._sample);
		return *this;
	}

	/** How much to adjust by
	 *
	 * The result is undefined unless initialized with
	 * <code>Analyzer::pop()</code>.
	 *
	 * \return	The adjustment
	 * \throw Not_enough_samples	...
	 */
	double adjustment() const throw (Not_enough_samples)
	{
		double v = gain_adjustment(&_sum);
		if (v == GAIN_NOT_ENOUGH_SAMPLES) throw Not_enough_samples();
		return v;
	}

private:
	struct replaygain_sample	_sum;
};

/** An analyzing context */
class Analyzer {
public:
	/** Construct the analyzer object
	 *
	 * \param samplefreq	The input sample frequency
	 */
	Analyzer(long samplefreq) throw (Bad_samplefreq) :
		_ctx(0)
	{
		enum replaygain_init_status	status;
		_ctx = gain_alloc_analysis(samplefreq, &status);
		switch (status) {
		case REPLAYGAIN_INIT_ERR_MEM:
			throw std::bad_alloc();
		case REPLAYGAIN_INIT_ERR_SAMPLEFREQ:
			throw Bad_samplefreq();
		case REPLAYGAIN_INIT_OK:
			break;
		}
	}

	~Analyzer()
	{
		free(_ctx);
	}

	/** Accumulate samples into a calculation
	 *
	 * The range of the samples should be is [-32767.0,32767.0].
	 *
	 * \param left_samples	Samples for the left (or mono) channel
	 * \param right_samples	Samples for the right channel; ignored for
	 *	single-channel
	 * \param num_samples	Number of samples
	 * \param num_channels	Number of channels
	 * \retval false	Bad number of channels or some exceptional
	 *	event
	 */
	bool add(const double *left_samples, const double *right_samples,
	    size_t num_samples, int num_channels)
	{
		enum replaygain_status	status;

		status = gain_analyze_samples(_ctx, left_samples,
		    right_samples, num_samples, num_channels);
		return status == REPLAYGAIN_OK;
	}

	/** Return current calculation, reset context
	 *
	 * \param[out] out	The accumulated Replaygain sample
	 */
	void pop(Sample *out)
	{
		gain_pop(_ctx, &out->_sample);
	}

private:
	// no copying
	Analyzer(const Analyzer &) {}
	void operator=(const Analyzer &) {}

	struct replaygain_ctx	*_ctx;
};

}

#endif
