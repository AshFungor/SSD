// laar
#include <boost/asio/buffer.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/bind/bind.hpp>
#include <cstdint>
#include <iomanip>
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

    template<typename Functor, typename... Args>
    auto bindCall(Functor f, Args&&... args) -> decltype(
        boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    ) {
        return boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    }

    std::string makeUnimplementedMessage(std::string file, std::string function, std::size_t line) {
        return absl::StrFormat(
            "Unimplemented API; File: \"%s\"; Function: \"%s\"; Line: %d", file, function, line
        );
    }

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

Session::Session(
    std::weak_ptr<ISessionMaster> master,
    std::shared_ptr<boost::asio::io_context> context, 
    std::shared_ptr<tcp::socket> socket,
    std::weak_ptr<IStreamHandler> handler,
    std::size_t bufferSize
)
    : networkState_(std::make_shared<NetworkState>(bufferSize))
    , socket_(socket)
    , context_(context)
    , master_(master)
    , handler_(handler)
{}

absl::Status Session::init() {
    absl::Status status;
    std::call_once(init_, [this, &status]() mutable {
        status.Update(onProtocolTransition(session_state::protocol::PROTOCOL_HEADER));
        socket_->async_read_some(
            boost::asio::mutable_buffer(networkState_->buffer_.get(), Header::getHeaderSize()),
            boost::bind(
                &Session::sRead,
                networkState_,
                weak_from_this(), 
                boost::asio::placeholders::error, 
                boost::asio::placeholders::bytes_transferred
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

Session::APIResult Session::onClientMessage(NSound::TClientMessage message) {
    if (message.has_basemessage()) {
        return onBaseMessage(std::move(*message.mutable_basemessage()));
    }

    return APIResult::unimplemented(makeUnimplementedMessage(__FILE_NAME__, __FUNCTION__, __LINE__));
}

Session::APIResult Session::onBaseMessage(TBaseMessage message) {
    if (message.has_streamconfiguration()) {
        return onStreamConfiguration(std::move(*message.mutable_streamconfiguration()));
    }

    if (!streamConfig_.has_value()) {
        return APIResult::misconfiguration("expecting stream config before any other API call");
    }
    
    if (message.has_directive()) {
        TBaseMessage::TStreamDirective directive = std::move(*message.mutable_directive());
        switch (directive.type()) {
            case TBaseMessage::TStreamDirective::CLOSE:
                return onClose(std::move(directive));
            case TBaseMessage::TStreamDirective::FLUSH:
                return onFlush(std::move(directive));
            case TBaseMessage::TStreamDirective::DRAIN:
                return onDrain(std::move(directive));
            default:
                return APIResult::unimplemented(makeUnimplementedMessage(__FILE_NAME__, __FUNCTION__, __LINE__));
        }
    } else if (message.has_pull()) {
        return onIOOperation(std::move(*message.mutable_pull()));
    } else if (message.has_push()) {
        return onIOOperation(std::move(*message.mutable_push()));
    }

    return APIResult::unimplemented(makeUnimplementedMessage(__FILE_NAME__, __FUNCTION__, __LINE__));
}

Session::APIResult Session::onIOOperation(TBaseMessage::TPull message) {
    if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::PLAYBACK) {
        return APIResult::misconfiguration("received wrong IO operation: read");
    } else if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::RECORD) {
        auto buffer = std::make_unique<char[]>(laar::getSampleSize(streamConfig_->samplespec().format()) * message.size());
        auto status = handle_->read(buffer.get(), message.size());
        PLOG(plog::debug) << "write status: " << status.status().ToString();
        return APIResult::make(absl::OkStatus(), std::nullopt);
    }

    return APIResult::unimplemented(makeUnimplementedMessage(__FILE_NAME__, __FUNCTION__, __LINE__));
}

Session::APIResult Session::onIOOperation(TBaseMessage::TPush message) {
    if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::PLAYBACK) {
        auto status = handle_->write(message.data().c_str(), message.datasamplesize());
        PLOG(plog::debug) << "write status: " << status.status().ToString();
        return APIResult::make(absl::OkStatus(), std::nullopt);
    } else if (streamConfig_->direction() == TBaseMessage::TStreamConfiguration::RECORD) {
        return APIResult::misconfiguration("received wrong IO operation: write");
    }

    return APIResult::unimplemented(makeUnimplementedMessage(__FILE_NAME__, __FUNCTION__, __LINE__));
}

Session::APIResult Session::onStreamConfiguration(TBaseMessage::TStreamConfiguration message) {
    PLOG(plog::info) << "config received: " << dumpStreamConfig(message);
    if (streamConfig_.has_value()) {
        return APIResult::misconfiguration("stream config already received");
    }

    streamConfig_ = std::move(message);
    return APIResult::make(absl::OkStatus(), std::nullopt);
}

Session::APIResult Session::onDrain(TBaseMessage::TStreamDirective message) {
    absl::Status status = handle_->drain();
    return APIResult::make(status, std::nullopt);
}

Session::APIResult Session::onFlush(TBaseMessage::TStreamDirective message) {
    absl::Status status = handle_->flush();
    return APIResult::make(status, std::nullopt);
}

Session::APIResult Session::onClose(TBaseMessage::TStreamDirective message) {
    handle_->abort();
    return APIResult::make(absl::OkStatus(), std::nullopt);
}

Session::~Session() {
    socket_->cancel();
}

absl::Status Session::onProtocolTransition(std::uint32_t state) {
    if (
        state & session_state::protocol::PROTOCOL_HEADER 
        && state_.protocol & session_state::protocol::PROTOCOL_PAYLOAD
    ) {
        set(state, session_state::PROTOCOL);
        unset(~state, session_state::PROTOCOL);
        return absl::OkStatus();
    } else if (
        state & session_state::protocol::PROTOCOL_PAYLOAD 
        && state_.protocol & session_state::protocol::PROTOCOL_HEADER
    ) {
        set(state, session_state::PROTOCOL);
        unset(~state, session_state::PROTOCOL);
        return absl::OkStatus();
    }

    return absl::InternalError("protocol transition is not possible");
}

void Session::set(std::uint32_t flag, std::uint32_t state) {
    std::unique_lock<std::mutex> locked (lock_);

    switch(state) {
        case session_state::BUFFER:
            state_.buffer |= state;
            break;
        case session_state::PROTOCOL:
            state_.protocol |= state;
            break;
    }
}

void Session::unset(std::uint32_t flag, std::uint32_t state) {
    std::unique_lock<std::mutex> locked (lock_);

    switch(state) {
        case session_state::BUFFER:
            state_.buffer &= ~state;
            break;
        case session_state::PROTOCOL:
            state_.protocol &= ~state;
            break;
    }
}

void Session::read(const boost::system::error_code& error, std::size_t bytes) {
    if (error) {
        onCriticalSessionError(absl::InternalError(error.what()));
        return;
    }

    absl::Status status;
    switch (state_.protocol) {
        case session_state::protocol::PROTOCOL_HEADER:
            status = readHeader(bytes);
            break;
        case session_state::protocol::PROTOCOL_PAYLOAD:
            status = readPayload(bytes);
            break;
    }

    if (!status.ok()) {
        onCriticalSessionError(std::move(status));
    }

    switch (state_.protocol) {
        case session_state::protocol::PROTOCOL_HEADER:
            // header is only possible if we looped the read process, se we put here write instead
            // (if needed by API)
            if (networkState_->result.has_value()) {
                // think
            } else {
                socket_->async_read_some(
                    boost::asio::mutable_buffer(networkState_->buffer_.get(), Header::getHeaderSize()),
                    bindCall(&Session::sRead, networkState_, weak_from_this())
                );
            }
        case session_state::protocol::PROTOCOL_PAYLOAD:
            socket_->async_read_some(
                boost::asio::mutable_buffer(networkState_->buffer_.get(), networkState_->header.getPayloadSize()),
                bindCall(&Session::sRead, networkState_, weak_from_this())
            );
            break;
    }
}

void Session::write(const boost::system::error_code& error, std::size_t bytes) {
    if (error) {
        onCriticalSessionError(absl::InternalError(error.what()));
        return;
    }

    // place next call to read
    socket_->async_read_some(
        boost::asio::mutable_buffer(networkState_->buffer_.get(), Header::getHeaderSize()),
        bindCall(&Session::sRead, networkState_, weak_from_this())
    );
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