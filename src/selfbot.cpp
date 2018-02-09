#include <iostream>
#include <fstream>
#include <string>
#include "libircclient.h"
#include <cstdio>
#include <vector>

const unsigned short PORT = 6667;
const char* ADDRESS = "irc.chat.twitch.tv";

int main(void) {
	// load up the config, and if there's no usable one, return 1
	std::ifstream conf("config");
	std::string username;
	std::string oauthpass;
	std::vector<std::string> channels; // channels should be listed with a # at the start, since no parsing will be done on them


	irc_callbacks_t callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	return 0;
}
