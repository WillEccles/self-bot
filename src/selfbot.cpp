#include <iostream>
#include <fstream>
#include <string>
#include "libircclient.h"
#include "libirc_rfcnumeric.h"
#include "tinyexpr.h"
#include <cstdio>
#include <vector>
#include <regex>
#include <iterator>
#include <cmath>

#define REGEX_FLAGS std::regex_constants::icase|std::regex_constants::optimize

const unsigned short PORT = 6667;
const char* ADDRESS = "irc.chat.twitch.tv";
const char* REALNAME = "Will Eccles";
std::vector<std::string> channels; // this will house all the channels we wish to join
const std::regex greetingPattern("^((hello|heyo*?|sup|howdy|what('?s)? up|hi)!*\\s+@?(cactus|cacus|cacy|will|tiny_cactus|tiny|ghosty)\\s*!*)", REGEX_FLAGS); // TODO: deal with emotes and strange greetings
const std::regex packetsCommand("^\\?gibpackets", REGEX_FLAGS); // should only work in #barbaricmustard
const std::regex mathCommand("^\\?\\s*(-?\\d+(\\.\\d+)?)\\s*[-*+/^%]\\s*(-?\\d+(\\.\\d+)?)\\s*$", REGEX_FLAGS); // matches math expressions
const std::regex sqrtCommand("^\\?\\s*sqrt\\s*((\\d+(\\.\\d+)?)|(\\(\\s*\\d+(\\.\\d+)?\\s*\\)))$", REGEX_FLAGS); // parses ?sqrt command which square roots the given input

// these are used for parsing, but constructed here so they aren't a colossal waste of memory and speed
std::regex cleanCommand("[\\s?]", REGEX_FLAGS);
std::regex mathParts("(-?\\d+(\\.\\d+)?)|([-*+/^])", REGEX_FLAGS);

void toLower(std::string& str) {
	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] >= 'A' && str[i] <= 'Z')
			str[i] += ('a' - 'A');
	}
}

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
	if (std::regex_match(message, packetsCommand)) {
		// this will give everyone my packets
		if (channel == "#barbaricmustard") {
			irc_cmd_msg(session, params[0], std::string("!giveall " + nick).c_str());
			std::cout << "Gave all packets to " << nick << '\n';
		}
	} else if (std::regex_match(message, mathCommand)) {
		//std::cout << "Got '" << message << "' from " << nick << " in " << channel << '\n';
		// parse the math expression given to the command and then do the math and put it in chat
		std::regex_replace(message, cleanCommand, "");
		// now that the string has no whitespace or ? at the beginning, parse the math
		// it should now be in the format of <+/-number><operation><+/-number>
		// use a regex iterator to find the parts
		std::vector<std::string> parts;
		auto words_begin = std::sregex_iterator(message.begin(), message.end(), mathParts);
		auto words_end = std::sregex_iterator();
		for (std::sregex_iterator i = words_begin; i != words_end; i++) {
			std::smatch match = *i;
			parts.push_back(match.str());
		}
		// this happens when there is an expression such as 5-2, and makes it 5+(-2)
		// this is due to an inherent flaw, that the regex in C++ doesn't allow lookbehinds
		if (parts.size() == 2) {
			parts.push_back(parts[1]);
			parts[1] = "+";
		}
		toLower(parts[1]);

		// DEBUG:
		//for (auto s: parts)
		//	std::cout << s << '\n';
		
		// this is the part where we make it do math
		long double n1 = std::stold(parts[0]);
		long double n2 = std::stold(parts[2]);
		long double r;
		std::string op = parts[1];
		if (op == "+") {
			r = n1 + n2;
		} else if (op == "-") {
			r = n1 - n2;
		} else if (op == "/") {
			r = n1 / n2;
		} else if (op == "*") {
			r = n1 * n2;
		} else if (op == "^") {
			r = pow(n1, n2);
		}
		
		irc_cmd_msg(session, params[0], std::string("/me " + nick + "-> " + std::to_string(r)).c_str());
	}
}

int main(void) {
	// load up the config, and if there's no usable one, return 1
	std::ifstream conf("config");
	if (!conf) {
		std::cout << "Error opening config.\n";
		return 1;
	}

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
