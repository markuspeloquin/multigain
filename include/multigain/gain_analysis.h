/*
 * ReplayGainAnalysis - analyzes input samples and give the recommended dB
 * change
 *
 * Copyright (C) 2001 David Robinson and Glen Sawyer, 2010 Markus Peloquin
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Concept and filter values by David Robinson <David@Robinson.org>
 *	(blame him if you think the idea is flawed)
 * Original coding by Glen Sawyer <mp3gain@hotmail.com>
 *	735 W 255 N, Orem, UT 84057-4505 USA
 *	(blame him if you think this runs too slowly)
 * Refactored by Markus Peloquin <markus@cs.wisc.edu>
 *	(blame him if you don't like the interface)
 *
 *  For an explanation of the concepts and the basic algorithms involved, go to:
 *    http://www.replaygain.org/
 */

/**
 * Pseudo-code to process an album:
 *
 *	double				l_samples[4096];
 *	double				r_samples[4096];
 *	struct replaygain_value	sample_accum;
 *
 *	struct replaygain_ctx		*ctx;
 *	enum replaygain_init_status	init_status;
 *
 *	if (!(ctx = gain_alloc_analysis(44100, &init_status))) {
 *		// handle error
 *	}
 *	memset(&sample_accum, 0, sizeof(sample_accum));
 *
 *	for (unsigned i = 1; i <= num_songs; i++) {
 *		struct replaygain_value	sample;
 *		size_t				num_samples;
 *
 *		while ((num_samples = getSongSamples(ctx, song[i],
 *		    l_samples, r_samples)) > 0) {
 *			gain_analyze_samples(ctx, l_samples, r_samples,
 *			    num_samples, 2);
 *		}
 *
 *		gain_pop(ctx, &sample);
 *		gain_accum(&sample_accum, &sample);
 *
 *		printf("Recommended dB change for song %2d: %+6.2f dB\n",
 *		    i, gain_adjustment(&sample));
 *	}
 *	printf("Recommended dB change for whole album: %+6.2f dB\n",
 *	    gain_adjustment(&sample_accum));
 *	free(ctx);
 */

#ifndef MULTIGAIN_GAIN_ANALYSIS_H
#define MULTIGAIN_GAIN_ANALYSIS_H

#include <features.h>

#ifdef __cplusplus
#	include <cstddef>
#else
#	include <stddef.h>
#endif

const double	GAIN_NOT_ENOUGH_SAMPLES = -24601.;

#ifdef __cplusplus
	/** Not to be used directly. */
	const unsigned	STEPS_PER_DB =	100;

	/* Table entries for 0...MAX_dB (normal max. values are 70...80 dB) */
	/** Not to be used directly. */
	const unsigned	MAX_DB =	120;

	/** Not to be used directly. */
	const size_t	ANALYZE_SIZE =	STEPS_PER_DB * MAX_DB;

#	define	__INLINE	inline
#else
#	define		STEPS_PER_DB	100
#	define		MAX_DB		120
#	define		ANALYZE_SIZE	STEPS_PER_DB * MAX_DB

#	define	__INLINE	static inline
#endif

enum replaygain_status {
	REPLAYGAIN_ERROR,
	REPLAYGAIN_OK
};

enum replaygain_init_status {
	REPLAYGAIN_INIT_ERR_MEM,	/**< Out of memory */
	REPLAYGAIN_INIT_ERR_SAMPLEFREQ,	/**< Unsupported sampling frequency */
	REPLAYGAIN_INIT_OK
};

struct replaygain_ctx;

/** The accumulated value of a set of samples */
struct replaygain_value {
	uint32_t value[ANALYZE_SIZE];
};

__BEGIN_DECLS

/** Initialize the analyzing context
 *
 * \param[in] samplefreq	The sampling frequency
 * \param[out] out_status	An error/success indicator
 * \retval NULL	An error occurred; check <code>out_status</code> for an
 *	explanation
 * \return	The context, which must be passed to <code>free()</code> when
 *	finished
 */
struct replaygain_ctx *
		gain_alloc_analysis(long samplefreq,
		    enum replaygain_init_status *out_status);

/** Accumulate samples into a calculation
 *
 * The range of the samples should be is [-32767.0,32767.0].
 *
 * \param ctx	Analyzing context
 * \param left_samples	Samples for the left (or mono) channel
 * \param right_samples	Samples for the right channel; ignored for
 *	single-channel
 * \param num_samples	Number of samples
 * \param num_channels	Number of channels
 * \retval REPLAYGAIN_ERROR	Bad number of channels or some exceptional
 *	error
 */
enum replaygain_status
		gain_analyze_samples(struct replaygain_ctx *ctx,
		    const double *left_samples, const double *right_samples,
		    size_t num_samples, int num_channels);

/** Return current calculation, reset context
 *
 * \param ctx	Analyzing context
 * \param[out] out	The accumulated Replaygain sample
 */
void		gain_pop(struct replaygain_ctx *ctx,
		    struct replaygain_value *out);

/** Combine result of one sample with another
 *
 * \param sum	The accumulated value
 * \param addition	The value to add to <code>sum</code>
 */
__INLINE void	gain_sample_accum(struct replaygain_value *sum,
		    const struct replaygain_value *addition);

/** Decibal adjustment for a sample
 *
 * \param value	A value calculation
 * \return	How many decibals to adjust by
 */
double		gain_adjustment(const struct replaygain_value *value);





__INLINE void
gain_sample_accum(struct replaygain_value *sum,
    const struct replaygain_value *addition)
{
	for (size_t i = 0; i < ANALYZE_SIZE; i++)
		sum->value[i] += addition->value[i];
}

__END_DECLS

#undef __INLINE

#endif /* GAIN_ANALYSIS_H */
