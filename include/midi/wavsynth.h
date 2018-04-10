#pragma once

namespace midi {
	class wavsynth;
};

#include <midi/midi.h>
#include <midi/player.h>
#include <midi/synth.h>

#include <vector>
#include <string>
#include <stdint.h>

namespace midi {

class wavsynth : public synth {
	public:
		wavsynth(player *play, uint32_t rate, std::string outfile);
		~wavsynth();

		virtual void wait(uint32_t usecs);
	
	private:
		void write_header(void);
		size_t samples;
		//std::vector<int16_t> samples;
		FILE *fp;
};

enum {
	WAVE_FORMAT_PCM = 0x0001,
	WAVE_FORMAT_FLOAT = 0x0003,
	WAVE_FORMAT_EXTENSIBLE = 0xfffe,
};

/* all-in-one struct containing multiple chunks
 * to make generating a wave file easy */
typedef struct wav_header {
	uint8_t  id[4]; // "RIFF"
	uint32_t size;

	struct {
		uint8_t  id[4]; // "WAVE"
		uint8_t  chunk_id[4]; // "fmt "
		uint32_t chunk_size;

		uint16_t format;
		uint16_t channels;
		uint32_t samples_per_sec;
		uint32_t bytes_per_sec;
		uint16_t block_align;
		uint16_t bits_per_sample;
	} wave;

	struct {
		uint8_t  id[4]; // "data"
		uint32_t size;

		// PCM data follows this
	} pcm;

} __attribute__((packed)) wav_header_t;

// namespace midi
}
