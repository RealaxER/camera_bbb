
#include <p2p.h>
#include <android/log.h>

#define LOG_TAG "P2P-CPP"
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define LOG(...) std::cout

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


void P2P::SetMaxMessageSize(size_t maxByte) {
    config.maxMessageSize = maxByte;
}

void P2P::CreatePeerConnection() {
    try {
        pc = std::make_shared<rtc::PeerConnection>(config);
        if (!pc) {
            throw std::runtime_error("PeerConnection is null after creation.");
        }

        pc->onLocalDescription([this](rtc::Description description) {
            localDescription = std::string(description);
            Event event = {Event::Type::LocalDescription, localDescription};
            {
                pushEvent(event);
            }
            ALOGI("Local Description: %s", localDescription.c_str());
        });

        pc->onLocalCandidate([this](rtc::Candidate candidate) {
            std::string candStr = std::string(candidate);
            localCandidate.push_back(candStr);
            Event event = {Event::Type::LocalCandidate, candStr};
            {
                pushEvent(event);  
            }
            ALOGI("Local Candidate: %s", candStr.c_str());
        });

        pc->onStateChange([this](rtc::PeerConnection::State state) { 
            localState = state;
            std::string stateStr = StateToString(state);
            Event event = {Event::Type::StateChange, stateStr};
            {
                pushEvent(event);  
            }
            ALOGI("[State: %s]", stateStr.c_str());
        });

        pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) { 
            std::string stateStr = GatheringStateToString(state);
            Event event = {Event::Type::GatheringStateChange, stateStr};
            {
                pushEvent(event); 
            }
            ALOGI("[Gathering State: %d]", static_cast<int>(state));
        });

        ALOGD("Peer connection created.");

    } catch (const std::exception& e) {
        ALOGE("Exception in CreatePeerConnection: %s", e.what());
        Event event = {Event::Type::Error, std::string(e.what())};
        {
            pushEvent(event);
        }
    }
}

void P2P::CreateDataChannel(const std::string& label) {
    try {
        if (pc != nullptr) {
            ALOGD("Creating data channel: %s", label.c_str());
            auto dc = pc->createDataChannel(label);
            ALOGI("Data channel created: %s", dc->label().c_str());

            dc->onOpen([dc]() {
                ALOGI("[DataChannel open: %s]", dc->label().c_str());
            });

            dc->onClosed([dc]() {
                ALOGI("[DataChannel closed: %s]", dc->label().c_str());
            });

            dc->onMessage([](auto data) {
                if (std::holds_alternative<std::string>(data)) {
                    ALOGI("[Received data: %s]", std::get<std::string>(data).c_str());
                }
            });

            dataChannels.push_back(dc);
        } else {
            ALOGE("PeerConnection is null!");
        }
    } catch (const std::exception& e) {
        ALOGE("Exception while creating DataChannel: %s", e.what());
    } catch (...) {
        ALOGE("Unknown exception while creating DataChannel!");
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
			

void P2P::sendMessageByLabel(const std::string& label, const std::string& message) {
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


bool P2P::sendBufferByLabel(const std::string& label, const uint8_t* data, size_t size) {
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
