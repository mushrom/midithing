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

static inline double d_note(double d){
	return pow(pow(2, 1/12.0), d);
}

static inline double clipped_sin(double in, double clip){
	double x = sin(in);

	if (x >  clip) x =  1;
	if (x < -clip) x = -1;

	return x;
}

static double amplify(double in, double amount){
	double x = in;

	if (x > 0) x += amount;
	if (x < 0) x -= amount;

	if (x >  1) x =  1;
	if (x < -1) x = -1;

	return x;
}

static inline double squarewave(double in){
	return clipped_sin(in, 0);
}

// TODO: consider implementing an lfsr, although this sounds decent as is
unsigned prand(void){
	static unsigned x = 0x7ff7ff7f;
	x = x * 1103515245 + 12345;
	return ((x / 0xffff) % 32767);
}

static inline double white_noise(void){
	double a = prand() / 32767.0;
	double b = prand() / 32767.0;

	return (double)(a - b);
}

static uint16_t percussion_buf[0x40];

static double kick(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / 4000.0);
	double a = clipped_sin(note(70), 0.4);
	double b = clipped_sin(d_note(80 - foo * 8), foo);

	return amplify((a + b) / 2, 0.5);
	//return 0;
	//return (a + b) / 2;
}

static double snare(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / 4000.0);

	//double a = white_noise() * (foo / 2);
	double a = clipped_sin(note(84), foo);
	double b = clipped_sin(d_note(92 - foo * 12), foo);

	//return (a + b) / 2;
	return amplify((a + b) / 2, 0.5);
	//return 0;
	//return (a + b) / 2;
}

static double tom(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / 4000.0);

	//double a = white_noise() * (foo / 2);
	double a = clipped_sin(note(78), foo);
	double b = clipped_sin(d_note(84 - foo * 10), foo);

	return amplify((a + b) / 2, 0.5);
}

static double hihat(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = (1 - (percussion_buf[index] / 4000.0)) / 2;

	double a = white_noise() * foo;
	//double b = clipped_sin(tick * d_note(64 - foo * 8), 0.1);

	return a;
	//return (a + b) / 2;
	//return 0;
	//return (a + b) / 2;
}

static double do_percussion(unsigned index, double tick){
	switch (index) {
		case 0:
		case 1:  return kick(index, tick);

		case 2:
		case 3:
		case 4:
		case 5:  return snare(index, tick);

		/* closed hi hat */
		case 7:
		case 9:
		case 11:
		case 14:
		case 16: return hihat(index, tick);

		/* Low floor tom, TODO */
		case 6:
		case 8:
		case 10:
		case 12:
		case 13:
		case 15: return tom(index, tick);

		default: return 0;
	}
}

int16_t synth::next_sample(void){
	static double increment = (1.0 / sample_rate) * (16.35 / 2) /* C0 */ * 2*M_PI;
	static double meh = 0.6;

	tick += increment;

	double sum = 0;
	unsigned voices = 0;

	for (unsigned k = 0; k < 16; k++) {
		channel &ch = sequencer->channels[k];

		if (k == 9) {
			// XXX: need to tie in the synth to the sequencer,
			//      percussion needs to be edge-triggered rather than level-triggered
			//      like the instruments. This will do for now, but uses noticably
			//      more CPU scanning this each sample
			for (unsigned key = 35; key <= 81; key++) {
				if (!ch.notemap[key]){
					percussion_buf[key - 35] = 0;
				}
			}
		}

		for (unsigned i = 0; i < 128; i++){
			if (!ch.active[i])
				break;

			uint8_t key = ch.active[i];

			// handle percussion channel
			if (k == 9){
				if (key >= 35 && key <= 81 && !percussion_buf[key - 35]){
					percussion_buf[key - 35] = 4000;
				}
				continue;
			}

			uint8_t velocity = ch.notemap[key];

			double foo = meh + sin(tick / 100)/7;
			double x = clipped_sin(tick * note(ch.active[i]), foo);
			//double x = clipped_sin(tick * note(ch.active[i]), 0.7);

			sum += x * (velocity / 127.0);
			voices += 1;
		}
	}

	for (unsigned k = 0; k < 0x40; k++) {
		if (percussion_buf[k] > 2) {
			sum += do_percussion(k, tick);
			voices += 1;
		}
	}

	double foo = voices? sum / voices : 0;

	return 0x7fff * foo;
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

		// compute next sample /after/ writing avoid delays in next write
		sample = next_sample();
	}

	pclose(alsa);
	puts("::: synth worker done");
}

void synth::start(void){
	handle = std::thread(&synth::worker, this);
}

// namespace midi
}
