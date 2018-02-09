#include <iostream>
#include <fstream>
#include <string>
#include "libircclient.h"
#include "libirc_rfcnumeric.h"
#include <cstdio>
#include <vector>
#include <regex>

#define REGEX_OPTIONS std::regex_constants::icase|std::regex_constants::optimize

const unsigned short PORT = 6667;
const char* ADDRESS = "irc.chat.twitch.tv";
const char* REALNAME = "Will Eccles";
std::vector<std::string> channels; // this will house all the channels we wish to join
const std::regex greetingPattern("^((hello|heyo*?|sup|howdy|what('?s)? up|hi)!*\\s+@?(cactus|cacus|cacy|will|tiny_cactus|tiny|ghosty)\\s*!*)", REGEX_OPTIONS); // TODO: deal with emotes and strange greetings
const std::regex packetsCommand("^?gibpackets", REGEX_OPTIONS); // should only work in #barbaricmustard
const std::regex mathCommand("^\\?\\s*(-?\\d+(\\.\\d+)?)\\s*([-*+/^%]|XOR|AND|NOT|OR)\\s*(-?\\d+(\\.\\d+)?)\\s*$", REGEX_OPTIONS); // matches math expressions
const std::regex sqrtCommand("^\\?\\s*sqrt\\s*((\\d+(\\.\\d+)?)|(\\(\\s*\\d+(\\.\\d+)?\\s*\\)))$", REGEX_OPTIONS); // parses ?sqrt command which square roots the given input

// when the server is connected to
void event_connect(irc_session_t* session, const char * event, const char * origin, const char ** params, unsigned int count) {
	// join the channels
	for (auto c : channels) {
		irc_cmd_join(session, c.c_str(), 0);
		std::printf("Joined %s\n", c.c_str());
	}
}

// RFC numeric codes, basically gonna ignore this one
void event_numeric(irc_session_t* session, unsigned int event, const char * origin, const char ** params, unsigned int count) {
}

// when a message shows up in a channel from anyone but me
void event_channel(irc_session_t* session, const char * event, const char * origin, const char ** params, unsigned int count) {
	if (!origin || count != 2)
		return;

	std::string message = params[1];
	std::string nick;
	char nickbuff[128];
	irc_target_get_nick(origin, nickbuff, sizeof(nickbuff));
	nick = std::string(nickbuff);
	std::string channel(params[0]);
	
	// send a message in the channel:
	// irc_cmd_msg(session, params[0], text);
	if ( std::regex_match(message, packetsCommand)) {
		// this will give everyone my packets
		if (channel == "#barbaricmustard") {
			irc_cmd_msg(session, params[0], std::string("!giveall " + nick).c_str());
			std::cout << "Gave all packets to " << nick << '\n';
		}
	}
}

int main(void) {
	// load up the config, and if there's no usable one, return 1
	std::ifstream conf("config");
	std::string username;
	std::string oauthpass;

	std::string line;
	for (int i = 0; std::getline(conf, line); i++) {
		if (i == 0)
			username = line;
		if (i == 1)
			oauthpass = line;
		if (i > 1) {
			if (line[0] == '#')
				channels.push_back(line);
			else
				channels.push_back("#" + line);
		}
	}

	if (channels.empty()) {
		std::cout << "No channels specified.\n";
		return 1;
	}

	irc_callbacks_t callbacks;
	memset(&callbacks, 0, sizeof(callbacks));

	// set the callbacks
	callbacks.event_connect = event_connect;
	callbacks.event_numeric = event_numeric;
	callbacks.event_channel = event_channel;

	irc_session_t *s = irc_create_session(&callbacks);
	if (!s) {
		std::cout << "Could not create IRC session\n";
		return 1;
	}

	irc_option_set(s, LIBIRC_OPTION_STRIPNICKS);
	
	// connect to server for twitch
	if (irc_connect(s, ADDRESS, PORT, oauthpass.c_str(), username.c_str(), username.c_str(), REALNAME)) {
		std::printf("Could not connect: %s\n", irc_strerror(irc_errno(s)));
		return 1;
	}

	// this loop will go forever, generating all the events
	if (irc_run(s)) {
		std::printf("Could not connect or IO error: %s\n", irc_strerror(irc_errno(s)));
		return 1;
	}

	return 0;
}
