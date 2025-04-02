#pragma once

#include <glog/logging.h>
#include <rtc/rtc.hpp>
#include <vector>

class P2P {
public:
    void SetStunServer(const std::string& stun_server);
    void SetTurnServer(const std::string& turn_server);
    void CreatePeerConnection();

    void CreateDataChannel(const std::string& label);
    void setRemoteDescription(std::string des);
    void addRemoteCandidate(std::string candidate);
    void sendMessageToChannel(const std::string& label, const std::string& message);
    void HandleIncomingDataChannel();
    std::string GetLocalDescription() const { return localDescription; }
    std::vector<std::string> GetLocalCandidate() const { return localCandidate; }
    
private:
    rtc::Configuration config;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::string localDescription;
    std::vector<std::string> localCandidate;
    rtc::PeerConnection::State localState;
    std::vector<std::shared_ptr<rtc::DataChannel>> dataChannels;
    std::map<std::string, std::shared_ptr<rtc::DataChannel>> reviceChannels;
};