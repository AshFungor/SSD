// sockpp
#include <sockpp/tcp_connector.h>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/result.h>

// standard
#include <sstream>
#include <stdexcept>
#include <memory>
#include <cerrno>
#include <mutex>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// laar
#include <common/exceptions.hpp>
#include <common/ring-buffer.hpp>
#include <network/message.hpp>

// local
#include "session.hpp"
#include "common/plain-buffer.hpp"

using namespace srv;

namespace {

    std::string dumpStreamConfig(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message) {
        std::stringstream ss;
        ss << "Message parsed: NSound::NSimple::TSimpleMessage::TStreamConfiguration\n";
        if (message.has_buffer_config()) {
            const auto& msg = message.buffer_config();
            ss << "- TBufferConfig: present\n";
            ss << "  size: " << msg.size() << "\n";
            ss << "  prebuffing_size: " << msg.prebuffing_size() << "\n";
            ss << "  min_request_size: " << msg.min_request_size() << "\n";
            ss << "  fragment_size: " << msg.fragment_size() << "\n";
        } else {
            ss << "- TBufferConfig: missing\n";
        }
        if (message.has_channel_map()) {
            ss << "- TChannelMap: present\n";
            for (const auto& channel : message.channel_map().mapped_channel()) {
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
        if (message.has_client_name()) {
            ss << "- ClientName: " << message.client_name() << "\n";
        } else {
            ss << "- ClientName: missing\n"; 
        }
        if (message.has_stream_name()) {
            ss << "- StreamName: " << message.has_stream_name() << "\n";
        } else {
            ss << "- StreamName: missing\n"; 
        }
        if (message.has_sample_spec()) {
            const auto& msg = message.sample_spec();
            ss << "- TSampleSpec: present\n";
            ss << "  Format: " << msg.format() << "\n";
            ss << "  SampleRate: " << msg.sample_rate() << "\n";
            ss << "  Channels: " << msg.channels() << "\n";
        } else {
            ss << "- TSampleSpec: missing\n";
        }
        ss << "- Direction: " << message.direction() << "\n";
        ss << "Parsing ended.";
        return ss.str();
    }

} // anonymous namespace

ClientSession::ClientSession(sockpp::tcp_socket&& sock, Private access) 
: sock_(std::move(sock))
, buffer_(std::make_shared<laar::PlainBuffer>(4096))
, builder_(laar::MessageBuilder::configure(buffer_))
{}

ClientSession::~ClientSession() {
    terminate();
}

std::shared_ptr<ClientSession> ClientSession::instance(sockpp::tcp_socket&& sock) {
    return std::make_shared<ClientSession>(std::move(sock), Private());
}

void ClientSession::init() {
    std::unique_lock<std::mutex> locked (sessionLock_);

    if (!sock_.is_open()) {
        throw laar::LaarBadInit();
    }
    
    PLOG(plog::debug) 
        << "Checking connection on client: "
        << sock_.peer_address();
    if (!sock_.peer_address().is_set()) {
        error("Peer is not connected, passed socket is invalid");
    }
    sock_.set_non_blocking();
}

void ClientSession::terminate() {
    std::unique_lock<std::mutex> locked (sessionLock_);
    auto result = sock_.close();
    if (result.is_ok()) {
        return;
    }
    PLOG(plog::error) << "Error closing the socket: close() returned " << result.error_message();
}

bool ClientSession::update() {
    // try to read message, return if transmission is not completed yet
    std::unique_lock<std::mutex> locked (sessionLock_);

    if (!sock_.is_open()) {
        PLOG(plog::debug) << "Socket " << sock_.address() << " was closed by peer";
        return false;
    }

    auto requested = builder_->poll();
    auto res = sock_.recv(buffer_->wPosition(), requested);
    
    if (res.is_error() || !res.value()) {
        if (!res.value()) {
            return false;
        }
        if (res.error().value() != EWOULDBLOCK) {
            handleErrorState(std::move(res));
        } else {
            return false;
        }
    }

    buffer_->advance(laar::PlainBuffer::EPosition::WPOS, res.value());
    laar::MessageBuilder::Receive event (res.value());
    builder_->handle(event);

    if (!builder_->valid()) {
        // reset connection
    }

    if (*builder_) {
        auto result = builder_->fetch();
        if (result->payloadType() == laar::IResult::EPayloadType::STRUCTURED) {
            onClientMessage(std::move(result->cast<laar::MessageBuilder::StructuredResult>().payload()));
        } else {
            // not implemented
        }
        // reset buffer to clear space for next message (risking overflow otherwise)
        buffer_->reset();
    }

    return true;
}

void ClientSession::onReply(std::unique_ptr<char[]> buffer, std::size_t size) {
    std::unique_lock<std::mutex> locked (sessionLock_);

    // reply to user
}

void ClientSession::handleErrorState(sockpp::result<std::size_t> requestState) {
    error("Error on socket read: " + requestState.error_message());
}

void ClientSession::onClientMessage(const NSound::TClientMessage& message) {
    if (message.has_simple_message()) {
        const auto& simple = message.simple_message();
        if (simple.has_stream_config()) {
            PLOG(plog::debug) << "Received message: TStreamConfig";
            onStreamConfigMessage(simple.stream_config());
        }
        if (simple.has_close()) {
            // close connection and terminate session
        }
        if (simple.has_push()) {
            // push data to buffer
        }
        if (simple.has_pull()) {
            // send data to user
        }
        if (simple.has_drain()) {
            // lock till buffer is free
        }
    }
}

void ClientSession::onStreamConfigMessage(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message) {
    // handle double config on same stream
    PLOG(plog::debug) << "config for peer: " << sock_.peer_address() << " received, dumping it to logs";
    PLOG(plog::debug) << dumpStreamConfig(message);
}

bool ClientSession::open() {
    std::unique_lock<std::mutex> locked (sessionLock_);
    return sock_.is_open();
}

void ClientSession::error(const std::string& errorMessage) {
    PLOG(plog::error) << "ClientSession error: " + errorMessage;
    throw std::runtime_error(errorMessage);
}