#pragma once

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
        Error,
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
    void sendMessageByLabel(const std::string& label, const std::string& message);
    void HandleIncomingDataChannel();
    void SetMaxMessageSize(size_t maxByte);
    std::string GetLocalDescription() const { return localDescription; }
    std::vector<std::string> GetLocalCandidate() const { return localCandidate; }

    bool sendBufferByLabel(const std::string& label, const uint8_t *data, size_t size);

    std::shared_ptr<rtc::DataChannel> getDataChannelByIndex(size_t index) {
        if (index >= reviceChannels.size()) {
            return nullptr;  
        }

        auto it = reviceChannels.begin();
        std::advance(it, index);  

        return it->second; 
    }

    void sendMessageByChannel(std::shared_ptr<rtc::DataChannel> datachannel, std::string message) {
        datachannel->send(message);
    }
    

    std::shared_ptr<rtc::PeerConnection> pc;

    void pushEvent(const Event& event) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            eventQueue.push(event);
        }
        cv.notify_one(); 
    }

    bool popEvent(Event& event) {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [this](){ return !eventQueue.empty(); });

        event = eventQueue.front();
        eventQueue.pop();
        return true;
    }

private:
    std::condition_variable cv;
    std::queue<Event> eventQueue;
    std::mutex queueMutex;
    rtc::Configuration config;
    std::string localDescription;
    std::vector<std::string> localCandidate;
    rtc::PeerConnection::State localState;
    std::vector<std::shared_ptr<rtc::DataChannel>> dataChannels;
    std::map<std::string, std::shared_ptr<rtc::DataChannel>> reviceChannels;
};