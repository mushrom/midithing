#pragma once

#include <midi/midi.h>
#include <midi/player.h>
#include <thread>

namespace midi {

class synth {
	public:
		synth(player *play, uint32_t rate);
		~synth();
		void start(void);

	private:
		void worker(void);
		int16_t next_sample(void);

		player *sequencer;
		uint32_t sample_rate;
		std::thread handle;

		double tick;
};

// namespace midi
}
