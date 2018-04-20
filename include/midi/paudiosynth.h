#pragma once

namespace midi {
	class paudiosynth;
}

#include <midi/midi.h>
#include <midi/player.h>
#include <midi/synth.h>
#include <stdio.h>

namespace midi {

class paudiosynth : public synth {
	public:
		paudiosynth(player *play, uint32_t rate);
		~paudiosynth();

		virtual void wait(uint32_t usecs);

	private:
		FILE *paudio;
};

// namespace midi
}
