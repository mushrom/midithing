#include <midi/midi.h>
#include <midi/player.h>

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

namespace midi {

channel::channel(){
	memset(notemap, 0, sizeof(notemap));
	memset(active, 0, sizeof(active));
}

void channel::note_on(uint16_t midi_data){
	uint8_t key = midi_data & 0x7f;
	uint8_t velocity = midi_data >> 7;

	notemap[key] = velocity;
	regen_active();

	printf("::: channel: key %u on, velocity %u\n", key, velocity);
}

void channel::note_off(uint16_t midi_data){
	uint8_t key = midi_data & 0x7f;
	uint8_t velocity = midi_data >> 7;

	notemap[key] = 0;
	regen_active();

	printf("::: channel: key %u off, velocity %u\n", key, velocity);
}

void channel::regen_active(void){
	unsigned k = 0;
	active[0] = 0;

	for (unsigned i = 0; i < 128; i++) {
		if (notemap[i]) {
			active[k++] = i;
		}
	}

	if (k && k < 128) {
		active[k] = 0;
	}
}

player::player(file f){
	// default tempo of 120 bpm
	usecs_per_tick = (60000.0 / (120 * f.division())) * 1000;
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

	if (ev.type() == EVENT_UNKNOWN) {
		printf("    event length: %u\n", ev.length());
		printf("    debug: %08x\n", ev.debug_bytes());
	}
}

void player::play(void){
	clock_t start = clock();

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

				if (ev.type() == EVENT_META_TRACK_END || ev.type() == EVENT_UNKNOWN){
					x.active = false;
				}

				goto reinterpret;

			} else if (x.next_tick < min_next) {
				min_next = x.next_tick;
			}
		}

		if (min_next != UINT_MAX){
			uint32_t delta = min_next - tick;

			// TODO: remove this
			fflush(stdout);
			//printf("::: time delta (approx): %u\n", clock() - start);

			clock_t fin = clock();
			clock_t run_adjust = (fin - start);

			usleep(delta * usecs_per_tick - run_adjust);
			//usleep(delta * usecs_per_tick);
			tick += delta;
			start = clock();
		}
	}

	// and there's nothing left to play
	// TODO: if looping is enabled, reset all tracks and start from the beginning
	state = PLAYER_STOPPED;
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
