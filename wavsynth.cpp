#include <midi/wavsynth.h>
#include <string.h>
#include <stdio.h>

namespace midi {

wavsynth::wavsynth(player *play, uint32_t rate, std::string outfile)
	: synth(play, rate)
{
	output = outfile;
}

wavsynth::~wavsynth(){
	wav_header_t header;
	// TODO: change this once stereo is implemented
	unsigned channels = 1;

	// set chunk strings
	memcpy(&header.id, "RIFF", 4);
	memcpy(&header.wave.id, "WAVE", 4);
	memcpy(&header.wave.chunk_id, "fmt ", 4);
	memcpy(&header.pcm.id, "data", 4);

	// set values in WAVE chunk
	header.wave.chunk_size = 16;
	header.wave.format = WAVE_FORMAT_PCM;
	header.wave.channels = channels;
	header.wave.samples_per_sec = sample_rate;
	header.wave.bytes_per_sec = sample_rate * 2 * channels;
	header.wave.block_align = 2 * channels;
	header.wave.bits_per_sample = 16;

	// set values in data chunk
	header.pcm.size = 2 * samples.size() * channels;

	// set size of the top-level header
	header.size = 36 + header.pcm.size + (samples.size() % 2);

	// finally write the data
	FILE *fp = fopen(output.c_str(), "w");
	fwrite(&header, sizeof(header), 1, fp);

	for (int16_t x : samples){
		fwrite(&x, 2, 1, fp);
	}

	if (samples.size() % 2){
		uint8_t x = 0;
		fwrite(&x, 1, 1, fp);
	}

	fclose(fp);
}

void wavsynth::wait(uint32_t usecs){
	uint32_t num = sample_rate * (usecs / 1000000.0);

	for (uint32_t i = 0; i < num; i++) {
		int16_t sample = next_sample();

		samples.push_back(sample);
	}
}

// namespace midi
}
