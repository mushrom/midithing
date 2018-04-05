#include <midi/midi.h>
#include <midi/player.h>

#include <stdio.h>
#include <limits.h>
#include <unistd.h>

namespace midi {

void channel::note_on(uint16_t midi_data){
	uint8_t key = midi_data & 0x7f;
	uint8_t velocity = midi_data >> 7;

	notemap[key] = velocity;

	printf("::: channel: key %u on, velocity %u\n", key, velocity);
}

void channel::note_off(uint16_t midi_data){
	uint8_t key = midi_data & 0x7f;
	uint8_t velocity = midi_data >> 7;

	notemap[key] = 0;

	printf("::: channel: key %u off, velocity %u\n", key, velocity);
}

player::player(file f){
	// default tempo of 120 bpm
	// XXX: tempo is not actually 120 right now, for debugging
	usecs_per_tick = 20000000 / 120 / 24;
	load_tracks(f);
}

void player::load_tracks(file f){
	for (unsigned i = 0; i < f.tracks(); i++) {
		track temp = f.get_track(i);
		event_stream stream = temp.events();

		tracks.push_back(player_track(stream));

		printf("have track with length %u\n", temp.length());
	}

	puts("note: we're in player::load_tracks()");
}

void player::dump_tracks(void){
	for (auto &x : tracks) {
		event ev = x.stream.get_event();
		printf("  events:\n");

		while (ev.type() != EVENT_UNKNOWN && ev.type() != EVENT_META_TRACK_END) {
			// TODO: wrap this up in an iterator

			printf("    %s (after %u) ",
				   midi_event_string(ev.type()),
				   ev.delta_time().num );

			if (ev.is_midi()) {
				uint8_t  channel = ev.midi_channel();
				uint16_t data    = ev.midi_data();

				printf("chan. %x: %04x", channel, data);
			}

			printf("\n");

			if (ev.type() == EVENT_UNKNOWN) {
				printf("    event length: %u\n", ev.length());
				printf("    debug: %08x\n", ev.debug_bytes());
			}

			x.stream.next();
			ev = x.stream.get_event();
		}
	}
}

unsigned player::tracks_active(void){
	unsigned ret = 0;

	for (auto &x : tracks){
		if (x.active){
			ret++;
		}
	}

	return ret;
}

void print_event(event &ev){
	printf("::: event: %s (after %u) ",
		   midi_event_string(ev.type()),
		   ev.delta_time().num );

	if (ev.is_midi()) {
		uint8_t  channel = ev.midi_channel();
		uint16_t data    = ev.midi_data();

		printf("chan. %x: %04x", channel, data);
	}

	printf("\n");
}

void player::play(void){
	while (tracks_active() > 0) {
		bool any_ready = false;
		uint32_t min_next = UINT_MAX;

		for (auto &x : tracks) {

	reinterpret:
			if (!x.active){
				continue;
			}

			// TODO: This assumes the first event has a delta of zero, which might
			//       not be the case, and will throw off timing it's not
			if (x.next_tick <= tick) {
				any_ready = true;
				event ev = x.stream.get_event();

				do {
					ev = x.stream.get_event();
					print_event(ev);
					interpret(ev);
					x.stream.next();
				} while (ev.delta_time().num == 0 && ev.type() != EVENT_META_TRACK_END);

				x.next_tick = tick + ev.delta_time().num;

				if (ev.type() == EVENT_META_TRACK_END){
					x.active = false;
				}

				goto reinterpret;

			} else if (x.next_tick < min_next) {
				min_next = x.next_tick;
			}
		}

		if (min_next != UINT_MAX){
			uint32_t delta = min_next - tick;

			usleep(delta * usecs_per_tick);
			tick += delta;
		}
	}
}

void player::interpret(event &ev){
	switch (ev.type()) {
		case EVENT_MIDI_NOTE_ON:
			channels[ev.midi_channel()].note_on(ev.midi_data());
			break;

		case EVENT_MIDI_NOTE_OFF:
			channels[ev.midi_channel()].note_off(ev.midi_data());
			break;

		default:
			// ignore other events
			break;
	}
}

void player::stop(void){

}

// namespace midi
}
