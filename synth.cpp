#include <midi/midi.h>
#include <midi/player.h>
#include <midi/synth.h>

#include <stdio.h>
#include <unistd.h>
#include <math.h>

namespace midi {

synth::synth(player *play, uint32_t rate){
	sequencer = play;
	sample_rate = rate;
	tick = 0;
}

synth::~synth(){
	handle.join();
	puts("::: synth worker exited");
}

static inline double note(unsigned i){
	return pow(pow(2, 1/12.0), i);
}

int16_t synth::next_sample(void){
	static double increment = (1.0 / sample_rate) * (16.35 / 2) /* C0 */ * 2*M_PI;

	tick += increment;

	double sum = 0;
	unsigned voices = 0;

	//for (auto &ch : sequencer->channels){
	for (unsigned k = 0; k < 16; k++) {
		channel &ch = sequencer->channels[k];

		for (unsigned i = 0; i < 128; i++){
			if (!ch.active[i])
				break;

			uint8_t key = ch.active[i];
			uint8_t velocity = ch.notemap[key];

			//sum += sin(tick * note( ch.active[i] ));
			double x = sin(tick * note( ch.active[i] ));

			if (x > 0.4)  x =  1;
			if (x < -0.4) x = -1;
			//sum += x;
			// reduce abrasiveness of trebly notes
			if (key > 60) x *= 0.80;
			sum += x * (velocity / 127.0);

			voices += 1;
		}
	}

	double foo = voices? sum / voices : 0;

	//return 0x7fff * 0.40 * sin(tick * note(42));
	return 0x7fff * /*0.50 **/ foo;
}

// TODO: when this gets ported to run on some mcus, the loop contents will
//       be run from an interrupt routine, rather than running in it's own thread
void synth::worker(void){
	puts("::: synth started!");
	double foo = 0;

	// XXX: use proper alsa api in the future
	FILE *alsa = popen("aplay -f cd", "w");
	int16_t sample = 0;

	while (sequencer->state != PLAYER_STOPPED){
		fwrite(&sample, 2, 1, alsa);
		fwrite(&sample, 2, 1, alsa);

		sample = next_sample();
		// compute next sample /after/ writing avoid delays in next write
	}

	puts("::: synth worker done");
}

void synth::start(void){
	handle = std::thread(&synth::worker, this);
}

// namespace midi
}
