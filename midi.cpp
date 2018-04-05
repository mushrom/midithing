/* notes:
 *    - see TODO comments
 *    - currently a lot of fields are recomputed every time they're accessed,
 *      if CPU usage becomes more important than memory usage here then it
 *      would make sense to cache these values
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <midi/midi.h>

namespace midi {

const char *header_magic = "MThd";
const char *track_magic  = "MTrk";

const char *event_strings[] {
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
	"event_meta_midi_port",
	"event_meta_chan_prefix",

	"event_end (shouldn't get here)",
};

const char *midi_event_string(unsigned event){
	if (event < EVENT_END){
		return event_strings[event];
	}

	return event_strings[EVENT_UNKNOWN];
}

// event class implementations
uint32_t event::debug_bytes(void){
	return b32_field(evdata);
}

varint_t event::delta_time(void){
	return var_field((const uint8_t *)data);
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

		case EVENT_META_MIDI_PORT:
		case EVENT_META_CHAN_PREFIX:
			return delta + 4;

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

		else if (metatype == 0x20){
			return EVENT_META_CHAN_PREFIX;
		}

		else if (metatype == 0x21){
			return EVENT_META_MIDI_PORT;
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

bool event::is_midi(void){
	unsigned t = type();

	return t >= EVENT_MIDI_NOTE_ON && t <= EVENT_MIDI_CHAN_MODE;
}

uint8_t event::midi_channel(void){
	return *evdata & 0xf;
}

uint16_t event::midi_data(void){
	switch (type()) {
		case EVENT_MIDI_NOTE_ON:
		case EVENT_MIDI_NOTE_OFF:
		case EVENT_MIDI_POLY_PRESSURE:
		case EVENT_MIDI_CTRL_CHANGE:
		case EVENT_MIDI_PITCH_WHEEL:
		case EVENT_MIDI_CHAN_MODE:
			return (evdata[2] << 7) | evdata[1];

		case EVENT_MIDI_PROC_CHANGE:
		case EVENT_MIDI_CHAN_PRESSURE:
			return evdata[1];

		default:
			return 0;
	}
}

// event_stream class implementations
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

event_stream::event_stream(const void *ptr){
	data = ptr;
}

// track class implementations
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

// file class implementations
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
