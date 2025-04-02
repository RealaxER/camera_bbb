
#include <p2p.h>

void P2P::SetStunServer(const std::string& stun_server) {
    config.iceServers.push_back({stun_server});
}

void P2P::SetTurnServer(const std::string& turn_server) {
    config.iceServers.push_back({turn_server});
}

void P2P::CreatePeerConnection() {
    pc = std::make_shared<rtc::PeerConnection>(config);
    
    pc->onLocalDescription([this](rtc::Description description) {
        localDescription = std::string(description);
        LOG(INFO) << "Local Description: " ;
        LOG(INFO) << localDescription ;
    });

    pc->onLocalCandidate([this](rtc::Candidate candidate) {
        localCandidate.push_back(std::string(candidate));
        LOG(INFO) << "Local Candidate: " ;
        LOG(INFO) << std::string(candidate);
    });

    pc->onStateChange([this](rtc::PeerConnection::State state) { 
        LOG(INFO) << "[State: " << static_cast<int>(state) << "]" ;
        localState = state;
    });

    pc->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) { 
        LOG(INFO) << "[Gathering State: " << static_cast<int>(state) << "]" ;
    });
}

void P2P::CreateDataChannel(const std::string& label) {
    if(pc != nullptr){
        auto dc = pc->createDataChannel(label);
        dc->onOpen([&]() { 
            LOG(INFO) << "[DataChannel open: " << dc->label() << "]";
        });

        dc->onClosed([&]() { 
            LOG(INFO) << "[DataChannel closed: " << dc->label() << "]";
        });

        dc->onMessage([](auto data) {
            if (std::holds_alternative<std::string>(data)) {
                LOG(INFO) << "[Received: " << std::get<std::string>(data) << "]" ;
            }
        });

        dataChannels.push_back(dc);
    }
}

void P2P::setRemoteDescription(std::string des) {
    std::string sdp;
    sdp += des;
    sdp += "\r\n";
    pc->setRemoteDescription(sdp);
}

void P2P::addRemoteCandidate(std::string candidate) {
    pc->addRemoteCandidate(candidate);
}
			

void P2P::sendMessageToChannel(const std::string& label, const std::string& message) {
    for (const auto& dc : dataChannels) {
        if (dc->label() == label) {
            if (dc->isOpen()) {
                dc->send(message);
                LOG(INFO) << "[Message sent to " << label << "]: " << message;
            } else {
                LOG(ERROR) << "DataChannel " << label << " is not open **";
            }
            return;
        }
    }
    LOG(ERROR) << "DataChannel " << label << " not found **";
}

// void P2P::HandleIncomingDataChannel() {
//     pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> rv) {
//         std::cout << "[Got a DataChannel with label: " << rv->label() << "]" << std::endl;

//         reviceChannels[rv->label()] = rv;

//         rv->onClosed([this, label = rv->label()]() {
//             std::cout << "[DataChannel closed: " << label << "]" << std::endl;
//             reviceChannels.erase(label);
//         });

//         rv->onMessage([label = rv->label()](auto data) {
//             if (std::holds_alternative<std::string>(data)) {
//                 std::cout << "[Received message on DataChannel " << label << "]: "
//                           << std::get<std::string>(data) << std::endl;
//             }
//         });
//     });
// }
