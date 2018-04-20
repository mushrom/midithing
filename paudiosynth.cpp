#include <midi/paudiosynth.h>

namespace midi {

paudiosynth::paudiosynth(player *play, uint32_t rate)
	: synth(play, rate)
{
	// XXX: use proper paudio api in the future
	paudio = popen("aplay -f cd", "w");
	//paudio = popen("aplay -f S16_LE -c 2 -r 16000", "w");
}

paudiosynth::~paudiosynth(){
	pclose(paudio);
}

void paudiosynth::wait(uint32_t usecs){
	uint32_t samples = sample_rate * (usecs / 1000000.0);

	for (uint32_t i = 0; i < samples; i++) {
		int16_t sample = next_sample();

		fwrite(&sample, 2, 1, paudio);
		fwrite(&sample, 2, 1, paudio);
	}
}

// namespace midi
}
