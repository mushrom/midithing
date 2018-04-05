#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace midi {

enum midi_event_types {
	EVENT_UNKNOWN,

	// Channel voice messages
	EVENT_MIDI_NOTE_ON,
	EVENT_MIDI_NOTE_OFF,
	EVENT_MIDI_POLY_PRESSURE,
	EVENT_MIDI_CTRL_CHANGE,
	EVENT_MIDI_PROC_CHANGE,
	EVENT_MIDI_CHAN_PRESSURE,
	EVENT_MIDI_PITCH_WHEEL,

	// Channel mode messages
	EVENT_MIDI_CHAN_MODE,

	// meta messages
	EVENT_META_TEXT,
	EVENT_META_SEQUENCE,
	EVENT_META_TRACK_END,
	EVENT_META_TEMPO,
	EVENT_META_TIME_SIG,
	EVENT_META_KEY_SIG,
	EVENT_META_SEQ_SPECIFIC,
	EVENT_META_MIDI_PORT,
	EVENT_META_CHAN_PREFIX,

	EVENT_END,
};

const char *midi_event_string(unsigned event);

typedef struct {
	uint8_t magic[4];
	uint8_t length[4];
	uint8_t format[2];
	uint8_t tracks[2];
	uint8_t division[2];
} __attribute__((packed)) file_t;

typedef struct {
	uint8_t magic[4];
	uint8_t length[4];
} __attribute__((packed)) track_t;

typedef struct {
	uint32_t num;
	size_t   length;
} varint_t;

static inline uint32_t b32_field(const uint8_t *x){
	return (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
}

static inline uint16_t b16_field(const uint8_t *x){
	return (x[0] << 8) | x[1];
}

static inline varint_t var_field(const uint8_t *x){
	varint_t ret = {.num = 0, .length = 1};

	for (; *x & 0x80; x++) {
		ret.num |=  *x & 0x7f;
		ret.num <<= 7;
		ret.length++;
	}

	ret.num |= *x & 0x7f;

	return ret;
}

class event {
	public:
		event(const void *ptr){
			data = ptr;
			evdata = (const uint8_t *)ptr + delta_time().length;
		};

		uint32_t length(void);
		uint32_t type(void);
		// TODO: remove after things are working
		uint32_t debug_bytes(void);

		varint_t delta_time(void);
		const uint8_t *evdata;

		// various accessor functions for underlying data
		bool     is_midi(void);
		uint8_t  midi_channel(void);
		uint16_t midi_data(void);

	protected:
		const void *data = NULL;
};

// NOTE: events have basically entirely variable length fields, so there's no
//       underlying struct here, offsets need to be computed dynamically
class event_stream {
	public:
		event_stream(){};
		event_stream(const void *ptr);

		// TODO: wrap this up in an iterator
		event get_event(void);
		void next(void);

	private:
		const void *data = NULL;
};

class track {
	public:
		track(const void *ptr);
		static bool valid(const void *ptr);
		uint32_t length(void);
		event_stream events(void);

	private:
		const void *data = NULL;
};

class file {
	public:
		file(const void *ptr);
		static bool valid(const void *ptr);
		uint32_t length(void);
		uint16_t format(void);
		uint16_t tracks(void);
		uint16_t division(void);

		track get_track(uint32_t id);

	private:
		const void *data = NULL;
};

// namespace midi
}
