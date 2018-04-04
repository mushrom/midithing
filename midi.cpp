/* notes:
 *    - see TODO comments
 *    - currently a lot of fields are recomputed every time they're accessed,
 *      if CPU usage becomes more important than memory usage here then it
 *      would make sense to cache these values
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <sstream>

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
};

const char *midi_event_strings[] {
	"event_unknown",

	"event_midi_note_on",
	"event_midi_note_off",
	"event_midi_poly_pressure",
	"event_midi_ctrl_change",
	"event_midi_proc_change",
	"event_midi_chan_pressure",
	"event_midi_pitch_wheel",

	"event_midi_chan_mode",

	"event_meta_text",
	"event_meta_sequence",
	"event_meta_track_end",
	"event_meta_tempo",
	"event_meta_time_sig",
	"event_meta_key_sig",
	"event_meta_seq_specific",
};

const char *header_magic = "MThd";
const char *track_magic  = "MTrk";

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

// abstract class for different event types
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

	protected:
		const void *data;
};

uint32_t event::debug_bytes(void){
	return b32_field(evdata);
}

uint32_t event::length(void){
	//return delta_time().length + 2;
	uint32_t delta = delta_time().length;

	switch (type()) {
		case EVENT_MIDI_NOTE_ON:
		case EVENT_MIDI_NOTE_OFF:
		case EVENT_MIDI_CHAN_MODE:
		case EVENT_MIDI_PITCH_WHEEL:
		case EVENT_META_TRACK_END:
			return delta + 3;

		case EVENT_META_TEXT:
		case EVENT_META_SEQ_SPECIFIC:
			return delta + 2 + var_field(evdata + 2).length + var_field(evdata + 2).num;

		case EVENT_META_KEY_SIG:
			return delta + 5;

		case EVENT_META_TEMPO:
			//return delta + 3 + var_field(evdata + 3).length;
			return delta + 6;

		case EVENT_META_TIME_SIG:
			return delta + 7;

		case EVENT_MIDI_PROC_CHANGE:
		case EVENT_MIDI_CHAN_PRESSURE:
		default:
			return delta + 2;
	}
	// TODO: actually get the length
}

uint32_t event::type(void){
	uint8_t status = *evdata;

	// channel voice messages
	if ((status & 0xf0) != 0xf0) {
		//printf("    (got here, status: %02x)\n", status >> 4);
		switch (status >> 4) {
			case 0x8: return EVENT_MIDI_NOTE_OFF;
			case 0x9: return EVENT_MIDI_NOTE_ON;
			case 0xb: return EVENT_MIDI_CHAN_MODE;
			case 0xc: return EVENT_MIDI_PROC_CHANGE;
			case 0xe: return EVENT_MIDI_PITCH_WHEEL;
			default: break;
		}
	}

	// meta messages
	else if (status == 0xff){
		uint8_t metatype = *(evdata + 1);

		if (metatype >= 0x1 && metatype <= 0xf){
			return EVENT_META_TEXT;
		}

		else if (metatype == 0x0 && *(evdata + 2) == 0x2){
			return EVENT_META_SEQUENCE;
		}

		else if (metatype == 0x2f){
			return EVENT_META_TRACK_END;
		}

		else if (metatype == 0x51){
			return EVENT_META_TEMPO;
		}

		else if (metatype == 0x58){
			return EVENT_META_TIME_SIG;
		}

		else if (metatype == 0x59){
			return EVENT_META_KEY_SIG;
		}

		else if (metatype == 0x7f){
			return EVENT_META_SEQ_SPECIFIC;
		}

		else {
			printf("    unknown metatype %02x\n", metatype);
		}
	}

	else if ((status & 0xf0) == 0xf0) {
		puts("    (got system common message)");
	}

	return EVENT_UNKNOWN;
}

// NOTE: events have basically entirely variable length fields, so there's no
//       underlying struct here, offsets have to be computed dynamically
class event_stream {
	public:
		event_stream(const void *ptr);

		// TODO: wrap this up in an iterator
		event get_event(void);
		void next(void);

	private:
		const void *data;
};

event event_stream::get_event(void){
	return event(data);
}

void event_stream::next(void){
	event ev = get_event();

	if (ev.type() == EVENT_META_TRACK_END){
		return;
	}

	data = (const uint8_t *)data + ev.length();
}

class track {
	public:
		track(const void *ptr);
		static bool valid(const void *ptr);
		uint32_t length(void);
		event_stream events(void);

	private:
		const void *data;
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
		const void *data;
};

event_stream::event_stream(const void *ptr){
	data = ptr;
}

varint_t event::delta_time(void){
	return var_field((const uint8_t *)data);
}

bool track::valid(const void *ptr){
	return memcmp(ptr, track_magic, 4) == 0;
}

track::track(const void *ptr){
	//if (memcmp(ptr, track_magic, 4) != 0){
	if (!track::valid(ptr)) {
		track_t *track = (track_t *)ptr;
		printf("actual magic: %02x %02x %02x %02x\n",
		       track->magic[0], track->magic[1], track->magic[2], track->magic[3]);
		throw "bad track magic";
	}

	data = ptr;
}

uint32_t track::length(void){
	return b32_field(((track_t *)data)->length);
}

event_stream track::events(void){
	return event_stream((const uint8_t *)data + sizeof(track_t));
}

bool file::valid(const void *ptr){
	return memcmp(ptr, header_magic, 4) == 0;
}

file::file(const void *ptr){
	if (!file::valid(ptr)){
		throw "bad header magic";
	}

	data = ptr;

	printf(
		"length: %u\n"
		"format: %u\n"
		"tracks: %u\n"
		"div.:   %u\n\n",
		length(), format(), tracks(), division()
	);

	for (unsigned i = 0; i < tracks(); i++) {
		track temp = get_track(i);
		event_stream stream = temp.events();
		event ev = stream.get_event();

		printf("have track with length %u\n", temp.length());
		printf("  first event type: %02x (%s)\n",
				ev.type(),
				midi_event_strings[ev.type()]);
		printf("  first event length: %u\n", ev.length());
		printf("  events:\n");

		while (ev.type() != EVENT_UNKNOWN && ev.type() != EVENT_META_TRACK_END) {
			// TODO: wrap this up in an iterator
			stream.next();
			ev = stream.get_event();

			printf("    %s (after %u)\n",
					midi_event_strings[ev.type()],
					ev.delta_time().num );

			if (ev.type() == EVENT_UNKNOWN) {
				printf("    event length: %u\n", ev.length());
				printf("    debug: %08x\n", ev.debug_bytes());
			}
		}
	}
}

uint32_t file::length(void){
	return b32_field(((file_t*)data)->length);
}

uint16_t file::format(void){
	return b16_field(((file_t*)data)->format);
}

uint16_t file::tracks(void){
	return b16_field(((file_t*)data)->tracks);
}

uint16_t file::division(void){
	return b16_field(((file_t*)data)->division);
}

track file::get_track(uint32_t id){
	size_t offset = sizeof(file_t) + length() - 6;
	track_t *foo = NULL;

	for (unsigned i = 0; i <= id; i++) {
		foo = (track_t *)((uint8_t *)data + offset);

		if (!track::valid(foo)) {
			throw "bad track identifier";
		}

		offset += b32_field(foo->length);
		offset += sizeof(track_t);
	}

	return track(foo);
}

// namespace midi
}

int main(int argc, char *argv[]){
	if (argc < 2){
		puts("usage: midithing [midi file]");
		return 1;
	}

	std::ifstream asdf(argv[1]);
	std::stringstream stream;
	stream << asdf.rdbuf();
	std::string in = stream.str();

	try {
		midi::file thing(in.c_str());

	} catch (const char *errormsg) {
		printf("error: %s: %s\n", argv[1], errormsg);
	}
}
