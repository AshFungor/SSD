// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/header.hpp>
#include <src/ssd/core/session.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// boost
#include <boost/bind/bind.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/placeholders.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/service/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/server-message.pb.h>
#include <protos/common/directives.pb.h>

// Abseil
#include <absl/status/status.h>
#include <absl/strings/str_format.h>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// STD
#include <mutex>
#include <memory>
#include <utility>
#include <cstdint>
#include <optional>
#include <source_location>

using namespace laar;

namespace {

    template<typename Functor, typename... Args>
    auto bindCall(Functor f, Args&&... args) -> decltype(
        boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    ) {
        return boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    }

    boost::asio::const_buffer makeResponse(NSound::TServiceMessage message, const std::unique_ptr<std::uint8_t[]>& buffer) {
        auto header = Header::make(message.ByteSizeLong());
        // buffered data
        header.toArray(buffer.get());
        message.SerializeToArray(buffer.get() + header.getHeaderSize(), header.getPayloadSize());

        return boost::asio::const_buffer(buffer.get(), header.getPayloadSize() + header.getHeaderSize());
    }

    std::string makeUnimplementedMessage(std::source_location location = std::source_location::current()) {
        return absl::StrFormat(
            "Unimplemented API; File: \"%s\"; Function: \"%s\"; Line: %d", location.file_name(), location.function_name(), location.line()
        );
    }

    std::string dumpStreamConfig(const NSound::NCommon::TStreamConfiguration& message) {
        std::stringstream ss;
        ss << "Message parsed: NSound::NCommon::TStreamConfiguration\n";
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
            boost::asio::mutable_buffer(networkState_->buffer.get(), Header::getHeaderSize()),
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

void Session::onBufferDrained(int /* status */) {
    set(session_state::buffer::BUFFER_DRAINED, session_state::BUFFER);
}

void Session::onBufferFlushed(int /* status */) {
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

    return APIResult::unimplemented(makeUnimplementedMessage());
}

Session::APIResult Session::onBaseMessage(TBaseMessage message) {
    if (message.has_streamconfiguration()) {
        return onStreamConfiguration(std::move(*message.mutable_streamconfiguration()));
    }

    if (!streamConfig_.has_value()) {
        return APIResult::misconfiguration("expecting stream config before any other API call");
    }
    
    if (message.has_directive()) {
        NSound::NCommon::TStreamDirective directive = std::move(*message.mutable_directive());
        switch (directive.type()) {
            case NSound::NCommon::TStreamDirective::CLOSE:
                return onClose(std::move(directive));
            case NSound::NCommon::TStreamDirective::FLUSH:
                return onFlush(std::move(directive));
            case NSound::NCommon::TStreamDirective::DRAIN:
                return onDrain(std::move(directive));
            default:
                return APIResult::unimplemented(makeUnimplementedMessage());
        }
    } else if (message.has_pull()) {
        return onIOOperation(std::move(*message.mutable_pull()));
    } else if (message.has_push()) {
        return onIOOperation(std::move(*message.mutable_push()));
    }

    return APIResult::unimplemented(makeUnimplementedMessage());
}

Session::APIResult Session::onIOOperation(TBaseMessage::TPull message) {
    if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::PLAYBACK) {
        return APIResult::misconfiguration("received wrong IO operation: read");
    } else if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::RECORD) {
        auto buffer = std::make_unique<char[]>(laar::getSampleSize(streamConfig_->samplespec().format()) * message.size());
        auto status = handle_->read(buffer.get(), message.size());
        PLOG(plog::debug) << "write status: " << status.status().ToString();
        return APIResult::make(absl::OkStatus(), std::nullopt);
    }

    return APIResult::unimplemented(makeUnimplementedMessage());
}

Session::APIResult Session::onIOOperation(TBaseMessage::TPush message) {
    if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::PLAYBACK) {
        auto status = handle_->write(message.data().c_str(), message.datasamplesize());
        PLOG(plog::debug) << "write status: " << status.status().ToString();
        return APIResult::make(absl::OkStatus(), std::nullopt);
    } else if (streamConfig_->direction() == NSound::NCommon::TStreamConfiguration::RECORD) {
        return APIResult::misconfiguration("received wrong IO operation: write");
    }

    return APIResult::unimplemented(makeUnimplementedMessage());
}

Session::APIResult Session::onStreamConfiguration(NSound::NCommon::TStreamConfiguration message) {
    PLOG(plog::info) << "config received: " << dumpStreamConfig(message);
    if (streamConfig_.has_value()) {
        return APIResult::misconfiguration("stream config already received");
    }

    if (auto handler = handler_.lock(); handler) {
        switch (message.direction()) {
            case NSound::NCommon::TStreamConfiguration::PLAYBACK:
                handle_ = handler->acquireWriteHandle(message, weak_from_this());
                break;
            case NSound::NCommon::TStreamConfiguration::RECORD:
                handle_ = handler->acquireReadHandle(message, weak_from_this());
                break;
            default:
                return APIResult::unimplemented(makeUnimplementedMessage());
        }
    }

    streamConfig_ = std::move(message);
    NSound::TServiceMessage response;
    response.mutable_basemessage()->mutable_streamalteredconfiguration()->mutable_configuration()->CopyFrom(streamConfig_.value());
    return APIResult::make(absl::OkStatus(), std::move(response));
}

Session::APIResult Session::onDrain(NSound::NCommon::TStreamDirective /* message */) {
    absl::Status status = handle_->drain();
    return APIResult::make(status, std::nullopt);
}

Session::APIResult Session::onFlush(NSound::NCommon::TStreamDirective /* message */) {
    absl::Status status = handle_->flush();
    return APIResult::make(status, std::nullopt);
}

Session::APIResult Session::onClose(NSound::NCommon::TStreamDirective /* message */) {
    handle_->abort();
    return APIResult::make(absl::OkStatus(), std::nullopt);
}

Session::APIResult Session::onSessionStatePoll(NSound::NCommon::TStreamStatePollResult /* message */) {
    NSound::TServiceMessage response;
    *response.mutable_basemessage()->mutable_statepollresult() = NSound::NCommon::TStreamStatePollResult::default_instance();
    if (state_.buffer & session_state::buffer::BUFFER_CLOSED) {
        response.mutable_basemessage()->mutable_statepollresult()->add_states(NSound::NCommon::TStreamStatePollResult::CLOSED);
    }
    if (state_.buffer & session_state::buffer::BUFFER_DRAINED) {
        response.mutable_basemessage()->mutable_statepollresult()->add_states(NSound::NCommon::TStreamStatePollResult::DRAINED);
    }
    if (state_.buffer & session_state::buffer::BUFFER_FLUSHED) {
        response.mutable_basemessage()->mutable_statepollresult()->add_states(NSound::NCommon::TStreamStatePollResult::FLUSHED);
    }
    return APIResult::make(absl::OkStatus(), std::move(response));
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

void Session::onCriticalSessionError(absl::Status status) {
    if (auto master = master_.lock(); master) {
        handle_->abort();
        master->abort(weak_from_this(), status.ToString());
    }
}

void Session::set(std::uint32_t flag, std::uint32_t state) {
    std::unique_lock<std::mutex> locked (lock_);

    switch(state) {
        case session_state::BUFFER:
            state_.buffer |= flag;
            break;
        case session_state::PROTOCOL:
            state_.protocol |= flag;
            break;
    }
}

void Session::unset(std::uint32_t flag, std::uint32_t state) {
    std::unique_lock<std::mutex> locked (lock_);

    switch(state) {
        case session_state::BUFFER:
            state_.buffer &= ~flag;
            break;
        case session_state::PROTOCOL:
            state_.protocol &= ~flag;
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
                socket_->async_write_some(
                    makeResponse(networkState_->result.value(), networkState_->buffer),
                    bindCall(&Session::sWrite, networkState_, weak_from_this())
                );
            } else {
                socket_->async_read_some(
                    boost::asio::mutable_buffer(networkState_->buffer.get(), Header::getHeaderSize()),
                    bindCall(&Session::sRead, networkState_, weak_from_this())
                );
            }
            break;
        case session_state::protocol::PROTOCOL_PAYLOAD:
            socket_->async_read_some(
                boost::asio::mutable_buffer(networkState_->buffer.get(), networkState_->header.getPayloadSize()),
                bindCall(&Session::sRead, networkState_, weak_from_this())
            );
            break;
    }
}

void Session::write(const boost::system::error_code& error, std::size_t /* bytes */) {
    if (error) {
        onCriticalSessionError(absl::InternalError(error.what()));
        return;
    }

    // place next call to read
    socket_->async_read_some(
        boost::asio::mutable_buffer(networkState_->buffer.get(), Header::getHeaderSize()),
        bindCall(&Session::sRead, networkState_, weak_from_this())
    );
}

absl::Status Session::readHeader(std::size_t /* bytes */) {
    std::stringstream iss {reinterpret_cast<char*>(networkState_->buffer.get()), std::ios::in | std::ios::binary};
    auto header = Header::readFromStream(iss);
    
    if (std::uint32_t total = header.getPayloadSize() + header.getHeaderSize(); total > laar::MaxBytesOnMessage) {
        return absl::InternalError(absl::StrFormat("exceeded hard limit on bytes for message: %d", total));
    }
    
    return onProtocolTransition(session_state::protocol::PROTOCOL_PAYLOAD);
}

absl::Status Session::readPayload(std::size_t /* bytes */) {
    NSound::TClientMessage message;
    if (bool status = message.ParseFromArray(networkState_->buffer.get(), networkState_->header.getPayloadSize()); !status) {
        return absl::InternalError("Failed to parse message");
    }

    APIResult result = onClientMessage(std::move(message));
    if (result.response.has_value()) {
        networkState_->result = std::move(result.response);
    }

    result.status.Update(onProtocolTransition(session_state::protocol::PROTOCOL_HEADER));
    return result.status;
}

void Session::sRead(std::shared_ptr<NetworkState> /* state */, std::weak_ptr<Session> session, const boost::system::error_code& error, std::size_t bytes) {
    if (auto that = session.lock(); that) {
        that->read(error, bytes);
    }
}

void Session::sWrite(std::shared_ptr<NetworkState> /* state */, std::weak_ptr<Session> session, const boost::system::error_code& error, std::size_t bytes) {
    if (auto that = session.lock(); that) {
        that->write(error, bytes);
    }
}

SessionFactory& SessionFactory::withBuffer(std::size_t size) {
    state_.size = size;
    return *this;
}

SessionFactory& SessionFactory::withMaster(std::weak_ptr<Session::ISessionMaster> master) {
    state_.master = std::move(master);
    return *this;
}

SessionFactory& SessionFactory::withHandler(std::weak_ptr<IStreamHandler> handler) {
    state_.handler = std::move(handler);
    return *this;
}

SessionFactory& SessionFactory::withContext(std::shared_ptr<boost::asio::io_context> context) {
    state_.context = std::move(context);
    return *this;
}

std::pair<std::shared_ptr<boost::asio::ip::tcp::socket>, std::shared_ptr<Session>> SessionFactory::AssembleAndReturn() {
    auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*state_.context);
    return std::make_pair(socket, Session::configure(state_.master, state_.context, socket, state_.handler, state_.size));
}