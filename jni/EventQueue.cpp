#include "EventQueue.h"

void EventQueue::push(const std::string &event) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(event);
    cond_var_.notify_one();
}

bool EventQueue::pop(std::string &event) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty(); });

    event = queue_.front();
    queue_.pop();
    return true;
}
