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
	perc_time = 4000 * (sample_rate / 44100.0);
	tick = 0;

	puts("::: synth worker started");
}

synth::~synth(){
	puts("::: synth exited");
}

#ifndef NO_OPTIMIZATIONS
static double note_table[] = {
	1.000000000000, 1.059463094359, 1.122462048309, 1.189207115003,
	1.259921049895, 1.334839854170, 1.414213562373, 1.498307076877,
	1.587401051968, 1.681792830507, 1.781797436281, 1.887748625363,
	2.000000000000, 2.118926188719, 2.244924096619, 2.378414230005,
	2.519842099790, 2.669679708340, 2.828427124746, 2.996614153753,
	3.174802103936, 3.363585661015, 3.563594872561, 3.775497250727,
	4.000000000000, 4.237852377437, 4.489848193237, 4.756828460011,
	5.039684199579, 5.339359416680, 5.656854249492, 5.993228307507,
	6.349604207873, 6.727171322030, 7.127189745123, 7.550994501454,
	8.000000000000, 8.475704754874, 8.979696386475, 9.513656920022,
	10.079368399159, 10.678718833360, 11.313708498985, 11.986456615013,
	12.699208415746, 13.454342644059, 14.254379490245, 15.101989002907,
	16.000000000000, 16.951409509749, 17.959392772950, 19.027313840044,
	20.158736798318, 21.357437666721, 22.627416997970, 23.972913230027,
	25.398416831491, 26.908685288119, 28.508758980491, 30.203978005814,
	32.000000000000, 33.902819019498, 35.918785545900, 38.054627680087,
	40.317473596636, 42.714875333441, 45.254833995939, 47.945826460054,
	50.796833662983, 53.817370576238, 57.017517960982, 60.407956011629,
	64.000000000000, 67.805638038995, 71.837571091800, 76.109255360174,
	80.634947193272, 85.429750666882, 90.509667991878, 95.891652920108,
	101.593667325965, 107.634741152476, 114.035035921964, 120.815912023257,
	128.000000000000, 135.611276077990, 143.675142183600, 152.218510720349,
	161.269894386544, 170.859501333765, 181.019335983757, 191.783305840216,
	203.187334651930, 215.269482304952, 228.070071843928, 241.631824046515,
	256.000000000001, 271.222552155981, 287.350284367201, 304.437021440698,
	322.539788773089, 341.719002667530, 362.038671967514, 383.566611680432,
	406.374669303861, 430.538964609904, 456.140143687856, 483.263648093029,
	512.000000000002, 542.445104311962, 574.700568734402, 608.874042881396,
	645.079577546178, 683.438005335061, 724.077343935028, 767.133223360865,
	812.749338607722, 861.077929219808, 912.280287375712, 966.527296186059,
	1024.000000000005, 1084.890208623924, 1149.401137468804, 1217.748085762793,
	1290.159155092357, 1366.876010670122, 1448.154687870057, 1534.266446721730,
};

static inline double note(unsigned i){
	if (i < 128) {
		return note_table[i];
	}

	return 1;
}

#else
static inline double note(unsigned i){
	return pow(pow(2, 1/12.0), i);
}

// NO_OPTIMIZATIONS
#endif

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

double synth::kick(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / perc_time);
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(70), 0.3);
	double b = clipped_sin(d_note(80 - foo * 8), 0.4 + foo/2);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.4 + b*1.5 + c*0.20) / (3 - impulse_adjust), 0.55);
}

double synth::snare(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / perc_time);
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(84), foo);
	double b = clipped_sin(d_note(92 - foo * 10), foo);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.35 + b*1.45 + c*0.20) / (3 - impulse_adjust), 0.7);
}

double synth::tom(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = 1 - (percussion_buf[index] / perc_time);
	double impulse = foo / 32;
	double impulse_adjust = 1 - impulse;

	double a = clipped_sin(note(78), foo);
	double b = clipped_sin(d_note(84 - foo * 10), foo);
	double c = squarewave(tick * note(24)) * impulse;

	return amplify((a*1.4 + b*1.5 + c*0.20) / (3 - impulse_adjust), 0.7);
}

double synth::hihat(unsigned index, double tick){
	percussion_buf[index]--;
	double foo = (1 - (percussion_buf[index] / perc_time)) / 8;

	double a = white_noise() * foo;
	double b = squarewave(tick * note(24)) * (foo / 4);
	double x = (a + b) / 2;

	x = amplify(x, 0.7);
	x *= 0.4;

	return x;
}

double synth::do_percussion(unsigned index, double tick){
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

	return clipped_sin(tick * note(key), 0.3 + foo) * 0.70;
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
					percussion_buf[key - 35] = perc_time;
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
