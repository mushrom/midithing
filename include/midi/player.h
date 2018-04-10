#pragma once

namespace midi {
	class channel;
	class player_track;
	class player;
}

#include <midi/midi.h>
#include <midi/synth.h>
#include <vector>

namespace midi {

enum {
	PLAYER_INITIALIZED,
	PLAYER_STARTED,
	PLAYER_STOPPED,
};

class channel {
	public:
		channel();
		void note_on(uint16_t midi_data);
		void note_off(uint16_t midi_data);

		uint8_t instrument;
		// map of active notes and their current volumes
		uint8_t notemap[128];

		// active note buffer, so that the synth doesn't need to rescan
		// the notemap for each sample
		uint8_t active[128];
		void update(void);
	
	private:
		bool changed = false;
		void regen_active(void);
};

class player_track {
	public:
		player_track(track trk){
			ptrack = trk;
			stream = ptrack.events();
		}

		void reset(void){
			stream = ptrack.events();
			active = true;
		}

		track ptrack;
		event_stream stream;
		uint32_t next_tick = 0;
		bool active = true;
};

class player {
	public:
		player(file f);
		channel channels[16];
		std::vector<player_track> tracks;

		void load_tracks(file f);
		void dump_tracks(void);
		void interpret(event &ev);
		unsigned tracks_active(void);
		void set_synth(synth *syn);

		void play(void);
		void loop(unsigned loops);
		void stop(void);
	
		uint32_t usecs_per_tick;
		uint32_t tick;
		unsigned state = PLAYER_INITIALIZED;
		synth *synthesizer = NULL;
};

// namespace midi
}
