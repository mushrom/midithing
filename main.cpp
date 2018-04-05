#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <midi/midi.h>
#include <midi/player.h>

int main(int argc, char *argv[]){
	if (argc < 3){
		puts("usage: midithing [action] [midi file]");
		puts("actions: play dump help");
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

		if (action == "dump") player.dump_tracks();
		if (action == "play") player.play();

	} catch (const char *errormsg) {
		printf("error: %s: %s\n", argv[1], errormsg);
	}

	return 0;
}
