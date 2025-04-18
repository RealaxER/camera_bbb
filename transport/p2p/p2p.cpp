
#include <p2p.h>


std::string GatheringStateToString(rtc::PeerConnection::GatheringState state) {
    switch (state) {
        case rtc::PeerConnection::GatheringState::New: return "New";
        case rtc::PeerConnection::GatheringState::InProgress: return "InProgress";
        case rtc::PeerConnection::GatheringState::Complete: return "Complete";
        default: return "Unknown";
    }
}

std::string StateToString(rtc::PeerConnection::State state) {
    switch (state) {
        case rtc::PeerConnection::State::New: return "New";
        case rtc::PeerConnection::State::Connecting: return "Connecting";
        case rtc::PeerConnection::State::Connected: return "Connected";
        case rtc::PeerConnection::State::Disconnected: return "Disconnected";
        case rtc::PeerConnection::State::Failed: return "Failed";
        case rtc::PeerConnection::State::Closed: return "Closed";
        default: return "Unknown";
    }
}


void P2P::SetStunServer(const std::string& stun_server) {
    config.iceServers.emplace_back(stun_server);
}

void P2P::SetTurnServer(const std::string& turn_server) {
    config.iceServers.push_back({turn_server});
}


void P2P::pushEvent(Event event){
    std::lock_guard<std::mutex> lock(queueMutex);
    eventQueue.push(event);  
}

void P2P::SetMaxMessageSize(size_t maxByte) {
    config.maxMessageSize = maxByte;
}
void P2P::CreatePeerConnection() {
    pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onLocalDescription([this](rtc::Description description) {
        localDescription = std::string(description);
        Event event = {Event::Type::LocalDescription, localDescription};
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(event);
        }
        LOG(INFO) << "Local Description: " << localDescription;
    });

    pc->onLocalCandidate([this](rtc::Candidate candidate) {
        localCandidate.push_back(std::string(candidate));
        Event event = {Event::Type::LocalCandidate, std::string(candidate)};
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(event);  
        }
        LOG(INFO) << "Local Candidate: " << std::string(candidate);
    });

    pc->onStateChange([this](rtc::PeerConnection::State state) { 
        localState = state;
        Event event = {Event::Type::StateChange, StateToString(state)};
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(event);  
        }
        LOG(INFO) << "[State: " << state << "]";
    });

    // Sự kiện cho Gathering State Change
    pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) { 
        Event event = {Event::Type::GatheringStateChange, GatheringStateToString(state)};
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(event); 
        }
        LOG(INFO) << "[Gathering State: " << static_cast<int>(state) << "]";
    });
}
void P2P::CreateDataChannel(const std::string& label) {
    if (pc != nullptr) {
        LOG(INFO) << "Creating data channel: " << label;
        auto dc = pc->createDataChannel(label);
        LOG(INFO) << "Data channel created: " << dc->label();

        dc->onOpen([dc]() { 
            LOG(INFO) << "[DataChannel open: " << dc->label() << "]";
        });

        dc->onClosed([dc]() { 
            LOG(INFO) << "[DataChannel closed: " << dc->label() << "]";
        });

        dc->onMessage([](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                LOG(INFO) << "[Received data: " << std::get<std::string>(data) << "]" ;
            }
        });

        dataChannels.push_back(dc);
    } else {
        LOG(ERROR) << "PeerConnection is null!";
    }
}

void P2P::setRemoteDescription(std::string des) {
    std::string sdp;
    sdp += des;
    sdp += "\r\n";
    try {
        pc->setRemoteDescription(sdp);  
    } catch (const std::exception& e) {
        LOG(ERROR) << "Error setting remote description: " << e.what();
    }
}

void P2P::addRemoteCandidate(std::string candidate) {
    pc->addRemoteCandidate(candidate);
}
			

void P2P::sendMessageToChannel(const std::string& label, const std::string& message) {
    if(dataChannels.size() <= 0){
        LOG(INFO) << "No dataChannel available";
    }
    for (const auto& dc : dataChannels) {
        if (dc->label().compare(label) == 0) {
            if (dc->isOpen()) {
                dc->send(message);
                LOG(INFO) << "[Message sent to " << label << "]: " << message;
            } else {
                LOG(ERROR) << "DataChannel " << label << " is not open";
            }
            return;
        }
    }
    LOG(ERROR) << "DataChannel " << label << " not found";
}


bool P2P::streamBuffereToChannel(const std::string& label, const uint8_t* data, size_t size) {
    if (dataChannels.size() <= 0) {
        LOG(ERROR) << "No dataChannel available";
        return false;
    }

    for (const auto& dc : dataChannels) {
        if (dc->label().compare(label) == 0) {
            if (dc->isOpen()) {
                const rtc::byte* byteData = reinterpret_cast<const rtc::byte*>(data);
                
                if (dc->send(byteData, size)) {
                    LOG(INFO) << "[Message sent to " << label << ": " << size << " byte" << "]";  
                    return true;
                } else {
                    LOG(ERROR) << "Failed to send data to DataChannel " << label;
                }
            } else {
                LOG(ERROR) << "DataChannel " << label << " is not open";
            }
            return false;
        }
    }
    LOG(ERROR) << "DataChannel " << label << " not found";
    return false;
}


void P2P::HandleIncomingDataChannel() {
    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> rv) {
        LOG(INFO) << "[Got a DataChannel with label: " << rv->label() << "]";

        reviceChannels[rv->label()] = rv;

        rv->onClosed([this, label = rv->label()]() {
            LOG(INFO) << "[DataChannel closed: " << label << "]";
            reviceChannels.erase(label);
        });

        rv->onMessage([this](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                LOG(INFO) << "[Received string data: " << std::get<std::string>(data) << "]" << std::endl;
            }
            else if (std::holds_alternative<rtc::binary>(data)) {
                const rtc::binary& binaryData = std::get<rtc::binary>(data);
                const uint8_t* dataPtr = reinterpret_cast<const uint8_t*>(binaryData.data());
                size_t dataSize = binaryData.size();

                LOG(INFO) << "[Received binary data, size: " << dataSize << "]";
            }
        });
    });
}