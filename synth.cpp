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
	//sample_rate = 16000;
	tick = 0;

	puts("::: synth worker started");
}

synth::~synth(){
	puts("::: synth exited");
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
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(70), 0.3);
	double b = clipped_sin(d_note(80 - foo * 8), 0.4 + foo/2);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.4 + b*1.5 + c*0.20) / (3 - impulse_adjust), 0.55);
}

static double snare(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / 4000.0);
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(84), foo);
	double b = clipped_sin(d_note(92 - foo * 10), foo);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.35 + b*1.45 + c*0.20) / (3 - impulse_adjust), 0.7);
}

static double tom(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / 4000.0);
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(78), foo);
	double b = clipped_sin(d_note(84 - foo * 10), foo);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.4 + b*1.5 + c*0.20) / (3 - impulse_adjust), 0.7);
}

static double hihat(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = (1 - (percussion_buf[index] / 4000.0)) / 8;

	double a = white_noise() * foo;
	double b = squarewave(tick * note(24)) * (foo / 4);
	double x = (a + b) / 2;

	x = amplify(x, 0.7);
	x *= 0.4;

	return x;
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

static double synth_lead(double tick, unsigned key){
	return clipped_sin(tick * note(key), 0.4) * 0.80;
}

static double synth_pad(double tick, unsigned key){
	double foo = sin(tick / 100)/8;

	return clipped_sin(tick * note(key), 0.7 + foo) * 0.70;
}

static double synth_bass(double tick, unsigned key){
	return clipped_sin(tick * note(key), 0.8);
}

static double synth_guitar(double tick, unsigned key){
	double foo = sin(tick)/8;

	double a = clipped_sin(tick * note(key), 0.5);
	double b = clipped_sin(tick * note(key + 12), 0.8 - foo);

	return ((a + b) / 2) * 0.80;
}

static double synth_organ(double tick, unsigned key){
	double a = clipped_sin(tick * note(key), 0.9);
	double b = sin(tick * note(key - 12));

	return (a*1.20 + b*0.80) / 2;
}

static double synth_ensemble(double tick, unsigned key){
	double a = sin(tick * note(key));

	return amplify(a, 0.3);
}

static double gen_instrument(unsigned instrument, double tick, unsigned key){
	switch (instrument >> 3) {
		// Organ
		case 2: return synth_organ(tick, key);
		// Guitar
		case 3: return synth_guitar(tick, key);
		// Bass
		case 4: return synth_bass(tick, key);
		// Ensemble
		case 6: return synth_ensemble(tick, key);
		// Synth lead
		case 10: return synth_lead(tick, key);
		// Synth pad
		case 11: return synth_pad(tick, key);

		// Piano
		case 0:
		// Chromatic percussion
		case 1:
		// Strings
		case 5:
		// Brass
		case 7:
		// Reed
		case 8:
		// Pipe
		case 9:
		// Synth effects
		case 12:
		// Ethnic
		case 13:
		// Percussive
		case 14:
		// Sound effects
		case 15:
		default: return synth_pad(tick, key);
	}
}

static double ghetto_limiter(double x, double *thresh){
	if (*thresh > 1){
		*thresh -= 0.0001;
	}

	if (x > *thresh)  *thresh = x;
	if (x < -*thresh) *thresh = -x;

	return x / *thresh;
}

int16_t synth::next_sample(void){
	static double increment = (1.0 / sample_rate) * (16.35 / 2) /* C0 */ * 2*M_PI;

	tick += increment;

	double sum = 0;

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
			double x = gen_instrument(ch.instrument, tick, key);

			sum += x * (velocity / 127.0) * 0.3;
		}
	}

	for (unsigned k = 0; k < 0x40; k++) {
		if (percussion_buf[k] > 2) {
			uint8_t velocity = sequencer->channels[9].notemap[k + 35];

			sum += do_percussion(k, tick) * (velocity / 127.0) * 0.33;
		}
	}

	static double thresh_state = 1;
	return 0x7fff * ghetto_limiter(sum, &thresh_state);
}

// namespace midi
}
