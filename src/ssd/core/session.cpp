// laar
#include <src/ssd/core/session.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// grpc
#include <grpcpp/support/status.h>

// STD
#include <protos/client/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/services/sound-router.grpc.pb.h>

// Abseil
#include <absl/status/status.h>

// protos
#include <memory>

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

std::shared_ptr<Session> Session::make(absl::string_view client) {
    return std::shared_ptr<Session>(new Session{client});
}

Session::Session(absl::string_view client) 
    : client_(client)
{}

absl::Status Session::init(std::weak_ptr<IStreamHandler> soundHandler) {
    absl::Status result = absl::OkStatus();
    std::call_once(init_, [this, soundHandler, &result]() {
        if (auto locked = soundHandler.lock()) {
            switch (streamConfig_.direction()) {
                case NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::PLAYBACK:
                    handle_ = locked->acquireWriteHandle(streamConfig_, weak_from_this());
                    break;
                case NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::RECORD:
                    handle_ = locked->acquireReadHandle(streamConfig_, weak_from_this());
                    break;
                default:
                    result = absl::InvalidArgumentError("Steam direction is unsupported");
            }
        }
    });

    return result;
}

laar::Session::TAPIResult Session::onIOOperation(TBaseMessage::TPull message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

laar::Session::TAPIResult Session::onIOOperation(TBaseMessage::TPush message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

laar::Session::TAPIResult Session::onStreamConfiguration(TBaseMessage::TStreamConfiguration message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

laar::Session::TAPIResult Session::onDrain(TBaseMessage::TStreamDirective message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

laar::Session::TAPIResult Session::onFlush(TBaseMessage::TStreamDirective message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

laar::Session::TAPIResult Session::onClose(TBaseMessage::TStreamDirective message) {
    return std::make_pair(absl::OkStatus(), NSound::TServiceMessage::default_instance());
}

void Session::onBufferDrained(int status) {
    
}

void Session::onBufferFlushed(int status) {
     
}