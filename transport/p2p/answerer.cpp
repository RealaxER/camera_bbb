/**
 * Copyright (c) 2019 Paul-Louis Ageneau
 * Copyright (c) 2019 Murat Dogan
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "rtc/rtc.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

int main(int argc, char **argv) {
	rtc::InitLogger(rtc::LogLevel::Warning);

	rtc::Configuration config;
	config.iceServers.emplace_back("stun.l.google.com:19302");

	auto pc = std::make_shared<rtc::PeerConnection>(config);

	pc->onLocalDescription([](rtc::Description description) {
		std::cout << "Local Description (Paste this to the other peer):" << std::endl;
		std::cout << std::string(description) << std::endl;
	});

	pc->onLocalCandidate([](rtc::Candidate candidate) {
		std::cout << "Local Candidate (Paste this to the other peer after the local description):"
		          << std::endl;
		std::cout << std::string(candidate) << std::endl << std::endl;
	});

	pc->onStateChange([](rtc::PeerConnection::State state) {
		std::cout << "[State: " << state << "]" << std::endl;
	});
	pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
		std::cout << "[Gathering State: " << state << "]" << std::endl;
	});

	std::map<std::string, std::shared_ptr<rtc::DataChannel>> dataChannels;

	pc->onDataChannel([&](std::shared_ptr<rtc::DataChannel> _dc) {
		std::cout << "[Got a DataChannel with label: " << _dc->label() << "]" << std::endl;

		dataChannels[_dc->label()] = _dc;

		_dc->onClosed([&]() {
			std::cout << "[DataChannel closed: " << _dc->label() << "]" << std::endl;
			
			dataChannels.erase(_dc->label());
		});

		_dc->onMessage([](auto data) {
			if (std::holds_alternative<std::string>(data)) {
				std::cout << "[Received message on DataChannel with label: "
						<< std::get<std::string>(data) << "]" << std::endl;
			}
		});
	});

	bool exit = false;
	while (!exit) {
		std::cout
		    << std::endl
		    << "**********************************************************************************"
		       "*****"
		    << std::endl
		    << "* 0: Exit /"
		    << " 1: Enter remote description /"
		    << " 2: Enter remote candidate /"
		    << " 3: Send message /"
		    << " 4: Print Connection Info *" << std::endl
		    << "[Command]: ";

		int command = -1;
		std::cin >> command;
		std::cin.ignore();

		switch (command) {
		case 0: {
			exit = true;
			break;
		}
		case 1: {
			// Parse Description
			std::cout << "[Description]: ";
			std::string sdp, line;
			while (getline(std::cin, line) && !line.empty()) {
				sdp += line;
				sdp += "\r\n";
			}
			std::cout << sdp;
			pc->setRemoteDescription(sdp);
			break;
		}
		case 2: {
			// Parse Candidate
			std::cout << "[Candidate]: ";
			std::string candidate;
			getline(std::cin, candidate);
			pc->addRemoteCandidate(candidate);
			break;
		}
		default: {
			std::cout << "** Invalid Command ** " << std::endl;
			break;
		}
		}
	}


}
