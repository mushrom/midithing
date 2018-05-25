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

		double kick(unsigned index, double tick);
		double snare(unsigned index, double tick);
		double tom(unsigned index, double tick);
		double hihat(unsigned index, double tick);
		double do_percussion(unsigned index, double tick);
		double perc_time;

	private:
		player *sequencer;
		double tick;
};

// namespace midi
}
