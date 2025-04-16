#pragma once

#include <glog/logging.h>
#include <rtc/rtc.hpp>
#include <vector>
#include <cstdint>

struct Event {
    enum class Type {
        LocalDescription,
        LocalCandidate,
        StateChange,
        GatheringStateChange,
        SetLocalDescription,
        SetLocalCandidate,
    };

    Type type;
    std::string data;
};

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
    void SetMaxMessageSize(size_t maxByte);
    std::string GetLocalDescription() const { return localDescription; }
    std::vector<std::string> GetLocalCandidate() const { return localCandidate; }

    std::shared_ptr<rtc::PeerConnection> pc;

    bool streamBuffereToChannel(const std::string& label, const uint8_t *data, size_t size);


    void pushEvent(Event event);

    bool popEvent(Event& event) {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (!eventQueue.empty()) {
            event = eventQueue.front();
            eventQueue.pop();
            return true;
        }
        return false;
    }
    std::shared_ptr<rtc::DataChannel> GetDataChannelByIndex(size_t index) {
        if (index >= reviceChannels.size()) {
            return nullptr;  
        }

        auto it = reviceChannels.begin();
        std::advance(it, index);  

        return it->second; 
    }

    void SendMessageReturnChannel(std::shared_ptr<rtc::DataChannel> datachannel, std::string message) {
        LOG(INFO) << "[Message return to " << datachannel->label() << "]: " << message;
        datachannel->send(message);
    }
    
private:
    std::queue<Event> eventQueue;
    std::mutex queueMutex;
    rtc::Configuration config;
    std::string localDescription;
    std::vector<std::string> localCandidate;
    rtc::PeerConnection::State localState;
    std::vector<std::shared_ptr<rtc::DataChannel>> dataChannels;
    std::map<std::string, std::shared_ptr<rtc::DataChannel>> reviceChannels;
};