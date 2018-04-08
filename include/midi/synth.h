#pragma once

namespace midi {
	class synth;
}

#include <midi/midi.h>
#include <midi/player.h>
#include <thread>

#include <stdio.h>

namespace midi {

class synth {
	public:
		synth(player *play, uint32_t rate);
		~synth();
		void wait(uint32_t usecs);

	private:
		int16_t next_sample(void);

		double tick;
		player *sequencer;
		uint32_t sample_rate;
		FILE *alsa;
};

// namespace midi
}
