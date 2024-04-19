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
#include <common/shared-buffer.hpp>
#include <network/message.hpp>

// local
#include "session.hpp"

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

ClientSession::ClientSession(sockpp::tcp_socket&& sock, std::shared_ptr<laar::SharedBuffer> buffer, Private access) 
: sock_(std::move(sock))
, isMessageBeingReceived_(false)
, isUpdating_(false)
, buffer_(buffer)
, message_(buffer)
{}

ClientSession::~ClientSession() {
    terminate();
}

std::shared_ptr<ClientSession> ClientSession::instance(sockpp::tcp_socket&& sock) {
    return std::make_shared<ClientSession>(std::move(sock), std::make_shared<laar::SharedBuffer>(1024), Private());
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
    message_.start();
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
    if (isUpdating_) {
        return false;
    }

    std::unique_lock<std::mutex> locked (sessionLock_);
    isUpdating_ = true;

    if (!sock_.is_open()) {
        PLOG(plog::debug) << "Socket " << sock_.address() << " was closed by peer";
        return false;
    }

    auto requestedBytes = message_.bytes();
    auto res = sock_.recv(buffer_->getRawWriteCursor(0), requestedBytes);
    
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

    laar::Receive event (res.value());
    buffer_->getRawWriteCursor(event.size);
    message_.handle<laar::Receive>(event);
    buffer_->advanceReadCursor(event.size);

    if (message_.isInTrail()) {
        auto result = message_.result();
        onClientMessage(std::move(result->payload));
        buffer_->clear();

        message_.reset();
    }

    isUpdating_ = false;

    return true;
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
    sessionConfig_ = message;
}

bool ClientSession::open() {
    std::unique_lock<std::mutex> locked (sessionLock_);
    return sock_.is_open();
}

bool ClientSession::updating() {
    return isUpdating_;
}

void ClientSession::error(const std::string& errorMessage) {
    PLOG(plog::error) << "ClientSession error: " + errorMessage;
    throw std::runtime_error(errorMessage);
}