#pragma once
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>


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


class EventQueue {
public:
    void push(const Event &event);
    bool pop(Event &event);  // Blocking pop

private:
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<Event> eventQueue;
    std::mutex queueMutex;
};

