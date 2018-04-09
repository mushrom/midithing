#include <midi/alsasynth.h>

namespace midi {

alsasynth::alsasynth(player *play, uint32_t rate)
	: synth(play, rate)
{
	// XXX: use proper alsa api in the future
	alsa = popen("aplay -f cd", "w");
	//alsa = popen("aplay -f S16_LE -c 2 -r 16000", "w");
}

alsasynth::~alsasynth(){
	pclose(alsa);
}

void alsasynth::wait(uint32_t usecs){
	uint32_t samples = sample_rate * (usecs / 1000000.0);

	for (uint32_t i = 0; i < samples; i++) {
		int16_t sample = next_sample();

		fwrite(&sample, 2, 1, alsa);
		fwrite(&sample, 2, 1, alsa);
	}
}

// namespace midi
}
