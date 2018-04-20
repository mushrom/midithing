#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <midi/midi.h>
#include <midi/player.h>
#include <midi/synth.h>
#include <midi/wavsynth.h>
#include <midi/paudiosynth.h>

int main(int argc, char *argv[]){
	if (argc < 3){
		puts("usage:");
		puts("    midithing help");
		puts("    midithing dump [midi file]");
		puts("    midithing play [midi file]");
		puts("    midithing loop [midi file] [[loops]]");
		puts("    midithing wav  [midi file] [output .wav]");

		return 1;
	}

	std::string action = argv[1];
	std::string fname  = argv[2];

	std::ifstream asdf(fname);
	std::stringstream stream;
	stream << asdf.rdbuf();
	std::string in = stream.str();

	try {
		midi::file   thing(in.c_str());
		midi::player player(thing);

		if (action == "dump"){
			player.dump_tracks();
		}

		else if (action == "play"){
			midi::paudiosynth syn(&player, 44100);

			player.set_synth(&syn);
			player.play();
		}

		else if (action == "loop"){
			unsigned loops = UINT_MAX;
			midi::paudiosynth syn(&player, 44100);

			if (argc >= 4){
				loops = atoi(argv[3]);
			}

			player.set_synth(&syn);
			player.loop(loops);
		}

		else if (action == "wav"){
			if (argc < 4) {
				throw "need output file name (try `midithing help`)";
			}

			std::string outfile = argv[3];
			midi::wavsynth wav(&player, 44100, outfile);

			player.set_synth(&wav);
			player.play();
		}

	} catch (const char *errormsg) {
		printf("error: %s: %s\n", argv[1], errormsg);
	}

	return 0;
}
