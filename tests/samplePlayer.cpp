#include "../src/core/channels/samplePlayer.h"
#include <catch2/catch.hpp>

TEST_CASE("SamplePlayer")
{
	using namespace giada;

	constexpr int BUFFER_SIZE  = 1024;
	constexpr int NUM_CHANNELS = 2;

	// Wave values: [1..BUFFERSIZE*4]
	m::Wave wave(0);
	wave.getBuffer().alloc(BUFFER_SIZE * 4, NUM_CHANNELS);
	wave.getBuffer().forEachFrame([](float* f, int i) {
		f[0] = static_cast<float>(i + 1);
		f[1] = static_cast<float>(i + 1);
	});

	m::ChannelShared channelShared(BUFFER_SIZE);
	m::Resampler     resampler(m::Resampler::Quality::LINEAR, NUM_CHANNELS);

	m::SamplePlayer samplePlayer(&resampler);
	samplePlayer.onLastFrame = [] {};

	SECTION("Test initialization")
	{
		REQUIRE(samplePlayer.hasWave() == false);
	}

	SECTION("Test rendering")
	{
		samplePlayer.loadWave(channelShared, &wave);

		REQUIRE(samplePlayer.hasWave() == true);
		REQUIRE(samplePlayer.begin == 0);
		REQUIRE(samplePlayer.end == wave.getBuffer().countFrames() - 1);

		REQUIRE(channelShared.tracker.load() == 0);
		REQUIRE(channelShared.playStatus.load() == ChannelStatus::OFF);
		REQUIRE(channelShared.rewinding == false);

		SECTION("Pitch != 1.0")
		{
			constexpr float PITCH = 0.5f;

			samplePlayer.pitch = PITCH;

			SECTION("Sub-range [M, N)")
			{
				constexpr int RANGE_BEGIN = 16;
				constexpr int RANGE_END   = 48;

				samplePlayer.begin = RANGE_BEGIN;
				samplePlayer.end   = RANGE_END;
				samplePlayer.render(channelShared);

				int numFramesWritten = 0;
				channelShared.audioBuffer.forEachFrame([&numFramesWritten](float* f, int) {
					if (f[0] != 0.0)
						numFramesWritten++;
				});

				REQUIRE(numFramesWritten == (RANGE_END - RANGE_BEGIN) / PITCH);
			}

			SECTION("Rewinding")
			{
				// Point in audio buffer where the rewind takes place
				const int OFFSET = 256;

				channelShared.rewinding = true;
				channelShared.offset    = OFFSET;

				samplePlayer.render(channelShared);

				// Rendering should start over again at buffer[OFFSET]
				REQUIRE(channelShared.audioBuffer[OFFSET][0] == 1.0f);
				REQUIRE(channelShared.rewinding == false);
				REQUIRE(channelShared.offset == 0);
			}
		}
	}
}