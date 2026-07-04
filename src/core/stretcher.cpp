/* -----------------------------------------------------------------------------
 *
 * Giada - Your Hardcore Loopmachine
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (C) 2010-2026 Giovanni A. Zuliani | Monocasual Laboratories
 *
 * This file is part of Giada - Your Hardcore Loopmachine.
 *
 * Giada - Your Hardcore Loopmachine is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * Giada - Your Hardcore Loopmachine is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Giada - Your Hardcore Loopmachine. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * -------------------------------------------------------------------------- */

#include "src/core/stretcher.h"
#include "src/core/const.h"
#include <cassert>

namespace giada::m
{
Stretcher::Stretcher(int sampleRate)
: m_stretcher(RubberBand::RubberBandStretcher(
      sampleRate, G_MAX_IO_CHANS, RubberBand::RubberBandStretcher::OptionProcessRealTime))
{
}

/* -------------------------------------------------------------------------- */

void Stretcher::setTempo(double ratio) { m_stretcher.setTimeRatio(ratio); }

/* -------------------------------------------------------------------------- */

void Stretcher::setPitch(double ratio) { m_stretcher.setPitchScale(ratio); }

/* -------------------------------------------------------------------------- */

void Stretcher::last() { m_stretcher.reset(); }

/* -------------------------------------------------------------------------- */

Stretcher::Result Stretcher::process(const float* input, std::size_t inputLength,
    std::size_t inputStart, float* output, std::size_t outputLength,
    std::size_t outputStart, double timeRatio, double pitchRatio)
{
	assert(input != nullptr);
	assert(output != nullptr);
	assert(inputStart < inputLength);
	assert(outputStart < outputLength);

	m_stretcher.setTimeRatio(timeRatio);
	m_stretcher.setPitchScale(pitchRatio);

	std::size_t framesGenerated  = 0;
	std::size_t framesUsed       = 0;
	bool        outputIsEmpty    = true;
	bool        inputIsAvailable = true;

	while (outputIsEmpty && inputIsAvailable)
	{
		/* Rubber Band expects de-interleaved audio as an array of channel pointers
		(one pointer per channel), while Stretcher::process() uses a flat stereo buffer
		for a simpler API. So here we build temporary channel-pointer arrays that point
		into the flat input/output buffers:

		    - inputPtrs[0]  -> left channel
		    - inputPtrs[1]  -> right channel
		    - outputPtrs[0] -> left channel
		    - outputPtrs[1] -> right channel

		This is only an adapter for the Rubber Band API; the actual audio data remains
		flat and planar in the caller-facing interface. */

		const float* inputPtrs[G_MAX_IO_CHANS] = {
		    input + inputStart + framesUsed,
		    input + inputStart + inputLength + framesUsed};

		float* outputPtrs[G_MAX_IO_CHANS] = {
		    output + outputStart + framesGenerated,
		    output + outputLength + outputStart + framesGenerated};

		const std::size_t framesRequired  = m_stretcher.getSamplesRequired();
		const std::size_t framesRemaining = inputLength - inputStart - framesUsed;
		const std::size_t framesToProcess = std::min(framesRequired, framesRemaining);
		inputIsAvailable                  = framesUsed + framesToProcess < inputLength - inputStart;
		m_stretcher.process(inputPtrs, framesToProcess, !inputIsAvailable);

		const std::size_t framesAvailable  = m_stretcher.available();
		const std::size_t outputRemaining  = outputLength - outputStart - framesGenerated;
		const std::size_t framesToRetrieve = std::min(framesAvailable, outputRemaining);
		const std::size_t framesRetrieved  = m_stretcher.retrieve(outputPtrs, framesToRetrieve);

		framesGenerated += framesRetrieved;
		framesUsed += framesToProcess;
		outputIsEmpty = framesGenerated < (outputLength - outputStart);
	}

	if (!inputIsAvailable)
		m_stretcher.reset();

	return {
	    .used      = static_cast<long>(framesUsed),
	    .generated = static_cast<long>(framesGenerated)};
}
} // namespace giada::m
