#pragma once

namespace midi {
	class synth;
}

#include <midi/midi.h>
#include <midi/player.h>


namespace midi {

class synth {
	public:
		synth(player *play, uint32_t rate);
		~synth();
		virtual void wait(uint32_t usecs) = 0;

	protected:
		int16_t next_sample(void);
		uint32_t sample_rate;

	private:
		player *sequencer;
		double tick;
};

// namespace midi
}
