// laar
#include <boost/asio/buffer.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind/bind.hpp>
#include <mutex>
#include <optional>
#include <src/ssd/core/session.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <src/ssd/core/header.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/service/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/server-message.pb.h>

// Abseil
#include <absl/status/status.h>
#include <absl/strings/str_format.h>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// STD
#include <memory>
#include <utility>

using namespace laar;

namespace {

    std::string dumpStreamConfig(const NSound::NClient::NBase::TBaseMessage::TStreamConfiguration& message) {
        std::stringstream ss;
        ss << "Message parsed: NSound::NClient::NBase::TBaseMessage::TStreamConfiguration\n";
        if (message.has_bufferconfig()) {
            const auto& msg = message.bufferconfig();
            ss << "- TBufferConfig: present\n";
            ss << "  size: " << msg.size() << "\n";
            ss << "  prebuffing_size: " << msg.prebuffingsize() << "\n";
            ss << "  min_request_size: " << msg.minrequestsize() << "\n";
            ss << "  fragment_size: " << msg.fragmentsize() << "\n";
        } else {
            ss << "- TBufferConfig: missing\n";
        }
        if (message.has_channelmap()) {
            ss << "- TChannelMap: present\n";
            for (const auto& channel : message.channelmap().mappedchannel()) {
                ss << "  Channel value:\n";
                if (channel.has_position()) {
                    ss << "  Position: " << channel.position() << "\n";
                }
                if (channel.has_aux()) {
                    ss << "  AUX: " << channel.aux() << "\n"; 
                }
            }
        } else {
            ss << "- TChannelMap: missing\n";
        }
        if (message.has_clientname()) {
            ss << "- ClientName: " << message.clientname() << "\n";
        } else {
            ss << "- ClientName: missing\n"; 
        }
        if (message.has_streamname()) {
            ss << "- StreamName: " << message.streamname() << "\n";
        } else {
            ss << "- StreamName: missing\n"; 
        }
        if (message.has_samplespec()) {
            const auto& msg = message.samplespec();
            ss << "- TSampleSpec: present\n";
            ss << "  Format: " << msg.format() << "\n";
            ss << "  SampleRate: " << msg.samplerate() << "\n";
            ss << "  Channels: " << msg.channels() << "\n";
        } else {
            ss << "- TSampleSpec: missing\n";
        }
        ss << "- Direction: " << message.direction() << "\n";
        ss << "Parsing ended.";
        return ss.str();
    }

}

std::shared_ptr<Session> Session::configure(
    std::weak_ptr<ISessionMaster> master,
    std::shared_ptr<boost::asio::io_context> context, 
    std::shared_ptr<tcp::socket> socket,
    std::weak_ptr<IStreamHandler> handler,
    std::size_t bufferSize
) {
    return std::shared_ptr<Session>(new Session(std::move(master), std::move(context), std::move(socket), std::move(handler), bufferSize));
}

absl::Status Session::init() {
    absl::Status status;
    std::call_once(init_, [this, &status]() mutable {
        status.Update(onProtocolTransition(session_state::protocol::PROTOCOL_HEADER));
        socket_->async_read_some(
            boost::asio::mutable_buffer(buffer_.get(), Header::getHeaderSize()),
            boost::bind(
                &Session::sRead, weak_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred
            )
        );
    });

    return status;
}

void Session::onBufferDrained(int status) {
    set(session_state::buffer::BUFFER_DRAINED, session_state::BUFFER);
}

void Session::onBufferFlushed(int status) {
    set(session_state::buffer::BUFFER_FLUSHED, session_state::BUFFER);
}

Session::operator bool() const {
    bool result = true;
    // buffer state should be empty
    result &= state_.buffer == 0x0;
    // protocol state should have one single bit set
    result &= 
        (state_.protocol == session_state::protocol::PROTOCOL_HEADER) 
        || (state_.protocol == session_state::protocol::PROTOCOL_PAYLOAD);
    return result; 
}

const Session::SessionState& Session::state() const {
    return state_;
}

Session::~Session() {
    socket_->cancel();
}

// absl::Status Session::init(std::weak_ptr<IStreamHandler> soundHandler) {
//     absl::Status result = absl::OkStatus();
//     std::call_once(init_, [this, soundHandler, &result]() {
//         if (!streamConfig_.has_value()) {
//             result = absl::InternalError("config was not received before init");
//             return;
//         }
//         TBaseMessage::TStreamConfiguration& config = streamConfig_.value();
//         if (auto locked = soundHandler.lock()) {
//             switch (config.direction()) {
//                 case NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::PLAYBACK:
//                     handle_ = locked->acquireWriteHandle(config, weak_from_this());
//                     break;
//                 case NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::RECORD:
//                     handle_ = locked->acquireReadHandle(config, weak_from_this());
//                     break;
//                 default:
//                     result = absl::InvalidArgumentError("Steam direction is unsupported");
//             }
//         }
//     });

//     return result;
// }

// laar::Session::TAPIResult Session::onIOOperation(TBaseMessage::TPull message) {
//     PLOG(plog::debug) << "received IO operation (pull) directive on protocol " << this << " effective size is " << message.size();

//     if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::PLAYBACK) {
//         return std::make_pair(absl::InternalError("received wrong IO operation: read"), std::nullopt);

//     } else if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::RECORD) {
//         auto buffer = std::make_unique<char[]>(laar::getSampleSize(streamConfig_->samplespec().format()) * message.size());
//         auto status = handle_->read(buffer.get(), message.size());
//         PLOG(plog::debug) << "write status: " << status.status().ToString();
//         return std::make_pair(absl::OkStatus(), std::nullopt);

//     }

//     return std::make_pair(absl::InternalError("received wrong IO operation: unknown"), std::nullopt);
// }

// laar::Session::TAPIResult Session::onIOOperation(TBaseMessage::TPush message) {
//     PLOG(plog::debug) << "received IO operation (push) directive on session " << this << " effective size is " << message.data().size();

//     if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::PLAYBACK) {
//         auto status = handle_->write(message.data().c_str(), message.datasamplesize());
//         PLOG(plog::debug) << "write status: " << status.status().ToString();
//         return std::make_pair(absl::OkStatus(), std::nullopt);
//     } else if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::RECORD) {
//         return std::make_pair(absl::InternalError("received wrong IO operation: write"), std::nullopt);
//     }

//     return std::make_pair(absl::InternalError("received wrong IO operation: unknown"), std::nullopt);
// }

// laar::Session::TAPIResult Session::onStreamConfiguration(TBaseMessage::TStreamConfiguration message) {
//     PLOG(plog::info) << "config received: " << dumpStreamConfig(message);
//     if (streamConfig_.has_value()) {
//         return std::make_pair(absl::InternalError("stream config already received"), NSound::TServiceMessage::default_instance());
//     }

//     streamConfig_ = std::move(message);
//     return std::make_pair(absl::OkStatus(), std::nullopt);
// }

// laar::Session::TAPIResult Session::onDrain(TBaseMessage::TStreamDirective message) {
//     absl::Status status = handle_->drain();
//     return std::make_pair(status, std::nullopt);
// }

// laar::Session::TAPIResult Session::onFlush(TBaseMessage::TStreamDirective message) {
//     absl::Status status = handle_->flush();
//     return std::make_pair(status, std::nullopt);
// }

// laar::Session::TAPIResult Session::onClose(TBaseMessage::TStreamDirective message) {
//     handle_->abort();
//     return std::make_pair(absl::OkStatus(), std::nullopt);
// }


// Session::~Session() {
//     if (handle_) {
//         handle_->abort();
//     }
// }

// const std::uint32_t Session::state() const {
//     return state_;
// }
// void Session::set(std::uint32_t flag) {
//     std::unique_lock<std::mutex> locked(lock_);
//     state_ &= flag;
// }

// void Session::unset(std::uint32_t flag) {
//     std::unique_lock<std::mutex> locked(lock_);
//     state_ &= ~flag;
// }

// bool Session::isAlive() {
//     return handle_->isAlive();
// }