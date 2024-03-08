// standard
#include <functional>
#include <chrono>
#include <memory>

// local
#include "callback.hpp"

using namespace laar;


Callback::Callback(std::function<void()> task) : task_(std::move(task)) {}

TimedCallback::TimedCallback(
    std::function<void()> task, 
    std::chrono::milliseconds timeout)
    : Callback(std::move(task))
    , timeout_(timeout)
    , startTs_(hs_clock::now())
{}

OptionalCallback::OptionalCallback(
    std::function<void()> task, 
    std::weak_ptr<void> lifetime) 
    : Callback(std::move(task))
    , lifetime_(std::move(lifetime))
{}

void Callback::execute() { 
    if (validate()) {
        task_(); 
        valid_ = false;
    }
}

bool Callback::locked() const { return !valid_; }
bool Callback::validate() const { return valid_; }
void Callback::operator()() { execute(); }

bool TimedCallback::validate() const { 
    return hs_clock::now() - startTs_ > timeout_ && valid_;
}

std::chrono::high_resolution_clock::time_point TimedCallback::goesOff() const {
    return startTs_ + timeout_;
}

bool OptionalCallback::validate() const {
    return lifetime_.lock().get() && valid_;
}
