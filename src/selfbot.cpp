#include <iostream>
#include <fstream>
#include <string>
#include "libircclient.h"
#include "libirc_rfcnumeric.h"
#include "tinyexpr.h"
#include <cstdio>
#include <vector>
#include <set>
#include <regex>
#include <iterator>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#define REGEX_FLAGS std::regex_constants::icase|std::regex_constants::optimize

std::string username;
std::string oauthpass;
const unsigned short PORT = 6667;
const char* ADDRESS = "irc.chat.twitch.tv";
const char* REALNAME = "Will Eccles";
std::set<std::string> channels; // this will house all the channels we wish to join
std::set<std::string> modifiedchannels; // this will house all the channels added or removed
bool channelsModified = false;
const std::regex greetingPattern("^((hello|heyo*?|sup|howdy|what('?s)? up|hi)!*\\s+@?(cactus|cacus|will|tiny_cactus|tiny|ghosty)\\s*!*)", REGEX_FLAGS); // TODO: deal with emotes and strange greetings
const std::regex packetsCommand("^\\?gibpackets", REGEX_FLAGS); // should only work in #barbaricmustard
const std::regex mathCommand("^\\?c(alc(ulate)?)?[-^*+/.0-9a-z()%\\s]+", REGEX_FLAGS); // for math calculations, prefix command with ?c and then type in your math
const std::regex addChannelCommand("^\\?(a(dd)?|j(oin)?)c(hannel)?\\s+#?[a-zA-Z0-9][\\w]{3,24}", REGEX_FLAGS);
const std::regex removeChannelCommand("^\\?(r(emove)?|l(eave)?)c(hannel)?\\s+#?[a-zA-Z0-9][\\w]{3,24}", REGEX_FLAGS);
const std::regex saveChannelsCommand("^\\?s(save)?c(hannels)?", REGEX_FLAGS);
const std::regex listChannelsCommand("^\\?(list|ls)c(hannels)?", REGEX_FLAGS);
const std::regex oodleCommand("^\\?oodle\\s+.+", REGEX_FLAGS);
const std::regex oodleReplaceUpper("[AEIOU]", std::regex_constants::optimize);
const std::regex oodleReplaceLower("[aeiou]", std::regex_constants::optimize);

// used to clean commands to just their arguments/parameters, removing the command itself and any leading/trailing spaces
const std::regex cleanCommand("(^\\?\\w+\\s+)|(\\s+$)", REGEX_FLAGS);

void toLower(std::string& str) {
	for (size_t i = 0; i < str.size(); i++) {
		if (str[i] >= 'A' && str[i] <= 'Z')
			str[i] += ('a' - 'A');
	}
}

void updateModifiedChannels(std::string moddedChannel) {
	if (modifiedchannels.find(moddedChannel) == modifiedchannels.end()) {
		modifiedchannels.insert(moddedChannel);
	} else {
		modifiedchannels.erase(moddedChannel);
	}

	channelsModified = (modifiedchannels.size() != 0);
}

int saveConfig(std::string& status) {
	if (!channelsModified) {
		status = "Channels not modified.";
		return 1;
	}

	std::ofstream conf("config", std::ios::out | std::ios::trunc);
	if (!conf) {
		status = "Could not open config for writing.";
		return 2;
	}

	conf << username << '\n';
	conf << oauthpass << '\n';
	for (auto &c : channels) {
		conf << c << '\n';
	}

	conf.close();
	modifiedchannels.clear();
	channelsModified = false;
	status = "Channels saved successfully.";
	return 0;
}

// when the server is connected to
void event_connect(irc_session_t* session, const char * event, const char * origin, const char ** params, unsigned int count) {
	// join the channels
	for (auto c : channels) {
		int error = irc_cmd_join(session, c.c_str(), 0);
		if (!error)
			std::printf("Joined %s\n", c.c_str());
		else
			std::printf("Error joining %s\n", c.c_str());
	}
}

// RFC numeric codes, basically gonna ignore this one
void event_numeric(irc_session_t* session, unsigned int event, const char * origin, const char ** params, unsigned int count) {
}

// when a message shows up in a channel
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
		message = std::regex_replace(message, cleanCommand, "");
		
		// now that the string has no whitespace or ?c at the beginning, parse the math
		int error = 0;
		double output = te_interp(message.c_str(), &error);
		if (error != 0)
			irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> invalid expression").c_str());
		else
			irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> " + std::to_string(output)).c_str());
	} else if (std::regex_match(message, oodleCommand)) {
		message = std::regex_replace(message, cleanCommand, "");
		message = std::regex_replace(message, oodleReplaceUpper, "OODLE");
		message = std::regex_replace(message, oodleReplaceLower, "oodle");

		irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> " + message).c_str());
	} else if (std::regex_match(message, addChannelCommand)) {
		if (nick != "tiny_cactus") return;
		message = std::regex_replace(message, cleanCommand, "");
		if (message[0] != '#')
			message = "#" + message;

		// join the channel and add it to the set
		if (channels.find(message) == channels.end()) {
			int error = irc_cmd_join(session, message.c_str(), 0);
			if (!error) {
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Attempted to join channel " + message + ".").c_str());
				channels.insert(message);
				std::printf("Joining channel %s.\n", message.c_str());
				updateModifiedChannels(message);
			} else {
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Error trying to join channel " + message + ".").c_str());
			}
		} else {
			if (modifiedchannels.find(message) != modifiedchannels.end())
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Already joined channel " + message + ".").c_str());
			else
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Already in channel " + message + ".").c_str());
		}
	} else if (std::regex_match(message, removeChannelCommand)) {
		if (nick != "tiny_cactus") return;
		message = std::regex_replace(message, cleanCommand, "");
		if (message[0] != '#')
			message = "#" + message;

		if (message == "#" + username) {
			irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Cannot remove own channel.").c_str());
			return;
		}

		// remove the channel
		if (channels.find(message) != channels.end())
		{
			int error = irc_cmd_part(session, message.c_str());
			if (!error)
			{
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Attempted to leave channel " + message + ".").c_str());
				channels.erase(message);
				std::printf("Leaving channel %s.\n", message.c_str());
				updateModifiedChannels(message);
			}
			else
			{
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Error trying to leave channel " + message + ".").c_str());
			}
		}
		else
		{
			if (modifiedchannels.find(message) != modifiedchannels.end()) {
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Already left channel " + message + ".").c_str());
			} else {
				irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> Not in channel " + message + ".").c_str());
			}
		}
	} else if (std::regex_match(message, saveChannelsCommand)) {
		if (nick != "tiny_cactus") return;
		
		// save the channels file
		std::string status;
		int error = saveConfig(status);
		std::printf("%s\n", status.c_str());
		irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> " + status).c_str());
	} else if (std::regex_match(message, listChannelsCommand)) {
		if (nick != "tiny_cactus" || channel != "#tiny_cactus") return;

		std::string current = "Current channels:";
		size_t i = 0;
		for (auto it = channels.begin(); it != channels.end(); it++, i++)
		{
			current += " " + (*it);
			if (i != channels.size()-1)
				current += ",";
		}

		size_t modsize = modifiedchannels.size();
		irc_cmd_msg(session, params[0], std::string("/me " + nick + " -> " + current + ". There " + (modsize == 1? "is" : "are") + " " + (modsize == 0 ? "no" : std::to_string(modsize)) + " modified channel" + (modsize == 1 ? "" : "s") + "." + (modsize == 0? "" : " Use ?savechannels or ?sc to save changes.")).c_str());
	}
}

int main(void) {
	// load up the config, and if there's no usable one, return 1
	std::ifstream conf("config");
	if (!conf) {
		std::cout << "Error opening config.\n";
		return 1;
	}

	std::string line;
	for (int i = 0; std::getline(conf, line); i++) {
		if (i == 0)
			username = line;
		if (i == 1)
			oauthpass = line;
		if (i > 1) {
			if (line[0] == '#')
				channels.insert(line);
			else
				channels.insert("#" + line);
		}
	}

	conf.close();

	if (channels.find("#" + username) == channels.end()) {
		channels.insert("#" + username);
		std::printf("Own channel not in list, added to channels.\n");
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
	
	for (;;) {
		std::printf("Attempting to connect...\n");
		// connect to server for twitch
		if (irc_connect(s, ADDRESS, PORT, oauthpass.c_str(), username.c_str(), username.c_str(), REALNAME)) {
			std::printf("Could not connect to twitch. Check credentials.\n");
		} else {
			std::printf("Connected to twitch.\n");
			// this loop will go forever, generating all the events
			if (irc_run(s))
			{
				std::printf("Could not connect or IO error: %s\n", irc_strerror(irc_errno(s)));
			}
		}
		std::printf("Restarting bot in 3 seconds...\n");
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	return 0;
}
