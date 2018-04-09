#pragma once

namespace midi {
	class alsasynth;
}

#include <midi/midi.h>
#include <midi/player.h>
#include <midi/synth.h>
#include <stdio.h>

namespace midi {

class alsasynth : public synth {
	public:
		alsasynth(player *play, uint32_t rate);
		~alsasynth();

		virtual void wait(uint32_t usecs);

	private:
		FILE *alsa;
};

// namespace midi
}
