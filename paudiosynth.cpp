#include <midi/paudiosynth.h>
#include <portaudio.h>

namespace midi {

static inline PaError throw_if_error(PaError x){
	if (x != paNoError){
		throw "error in portaudio call!";
	}

	return x;
}

paudiosynth::paudiosynth(player *play, uint32_t rate)
	: synth(play, rate)
{
	throw_if_error(Pa_Initialize());

	audio_params.device       = Pa_GetDefaultOutputDevice();
	audio_params.channelCount = 1;
	audio_params.sampleFormat = paInt16;
	audio_params.suggestedLatency = Pa_GetDeviceInfo(audio_params.device)->defaultHighOutputLatency;
	audio_params.hostApiSpecificStreamInfo = NULL;

	throw_if_error(
		Pa_OpenStream(
			&stream,
			NULL, /* no input device */
			&audio_params,
			rate,
			paFramesPerBufferUnspecified,
			paClipOff, /* no clipping, synth handles that */
			NULL, /* no callback */
			NULL /* no callback, so no userData */
			));

	Pa_StartStream(stream);
}

paudiosynth::~paudiosynth(){
	throw_if_error(Pa_StopStream(stream));
	throw_if_error(Pa_CloseStream(stream));
}

#define BUFFER_SIZE (512)

void paudiosynth::wait(uint32_t usecs){
	static int16_t buffer[BUFFER_SIZE];
	uint32_t samples = sample_rate * (usecs / 1000000.0);

	while (samples > 0){
		unsigned n = (samples >= BUFFER_SIZE)? BUFFER_SIZE : samples;
		samples -= n;

		for (unsigned i = 0; i < n; i++)
			buffer[i] = next_sample();

		PaError err = Pa_WriteStream(stream, buffer, n);

		if (err != paNoError){
			fprintf(stderr, "midithing: error during Pa_WriteStream(): %d\n", err);
		}
	}
}

// namespace midi
}
