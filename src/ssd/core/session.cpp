// STD
#include "protos/client/base.pb.h"
// #include "src/ssd/sound/interfaces/i-audio-handler.hpp"
#include <absl/status/status.h>
#include <memory>
#include <unordered_map>
#include <src/ssd/core/session.hpp>

using namespace laar;

std::shared_ptr<Session> Session::find(
    std::string client, 
    std::unordered_map<std::string, std::shared_ptr<Session>>& sessions
) {
    auto iter = sessions.find(client);
    if (iter != sessions.end()) {
        return iter->second;
    }
    return nullptr;
}

std::shared_ptr<Session> Session::make(absl::string_view  client) {
    return std::shared_ptr<Session>(new Session{client});
}

absl::Status Session::init(std::weak_ptr<IStreamHandler> soundHandler) {
    return absl::OkStatus();
}

absl::Status Session::onIOOperation(TBaseMessage::TPull message) {
    return absl::OkStatus();
}

absl::Status Session::onIOOperation(TBaseMessage::TPush message) {
    return absl::OkStatus();
}

absl::Status Session::onStreamConfiguration(TBaseMessage::TStreamConfiguration message) {
    return absl::OkStatus();
}

absl::Status Session::onDrain(TBaseMessage::TStreamDirective message) {
    return absl::OkStatus();
}

absl::Status Session::onFlush(TBaseMessage::TStreamDirective message) {
    return absl::OkStatus();
}

absl::Status Session::onClose(TBaseMessage::TStreamDirective message) {
    return absl::OkStatus();
}

void Session::onBufferDrained(int status) {
    
}

void Session::onBufferFlushed(int status) {
     
}