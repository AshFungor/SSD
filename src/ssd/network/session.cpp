// sockpp
#include <asm-generic/errno.h>
#include <sockpp/tcp_connector.h>
#include <sockpp/tcp_acceptor.h>
#include "sockpp/result.h"

// standard
#include <stdexcept>


// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>

// local
#include "session.hpp"

using namespace srv;


ClientSession::ClientSession(sockpp::tcp_socket&& sock) 
: sock_(std::move(sock))
, lastMessageLen_(0)
, lastMessageBytesRead_(0)
{}

ClientSession::~ClientSession() {
    terminate();
}

void ClientSession::init() {
    PLOG(plog::debug) << "Checking connection on client: "
                      << sock_.peer_address();
    if (!sock_.peer_address().is_set()) {
        error("Peer is not connected, passed socket is invalid");
    }
    sock_.set_non_blocking();
}

void ClientSession::terminate() {
    auto result = sock_.close();
    if (result.is_ok()) {
        return;
    }
    PLOG(plog::error) << "Error closing the socket: close() returned " << result.error_message();
}

void ClientSession::update() {
    // try to read message, return if transmission is not completed yet
    sockpp::result<std::size_t> result;
    if (lastMessageLen_ && lastMessageLenBytesRead_ == sizeof lastMessageLen_) {
        // message len received, now goes protobuf
        result = sock_.read(buffer.data() + lastMessageBytesRead_, lastMessageLen_ - lastMessageBytesRead_);
        if (result.value() == lastMessageLen_) {
            // protobuf received, now session can parse it
            NSound::TClientMessage message;
            message.ParseFromArray(buffer.data(), lastMessageLen_);
            onClientMessage(std::move(message));
            lastMessageLen_ = lastMessageBytesRead_ = lastMessageLenBytesRead_ = 0;
        } else if (result.is_ok() && result.value() < lastMessageLen_) {
            // partial receive, skip this cycle and go to next one to
            // retrieve more data
            lastMessageBytesRead_ += result.value(); 
        } else {
            if (result.error().value() == EWOULDBLOCK) {
                // no data on this cycle, skip
                return;
            } else {
                error("Error on socket read: " + result.error_message());
            }
        }
    } else {
        result = sock_.read(&lastMessageLen_ + lastMessageLenBytesRead_, sizeof lastMessageLen_ - lastMessageLenBytesRead_);
        if (result.is_ok()) {
            lastMessageLenBytesRead_ += result.value(); 
        } else {
            if (result.error().value() == EWOULDBLOCK) {
                // no data on this cycle, skip
                return;
            } else {
                error("Error on socket read: " + result.error_message());
            }
        }
    }
}

void ClientSession::onClientMessage(const NSound::TClientMessage& message) {
    if (message.has_stream_config()) {
        PLOG(plog::debug) << "Received message: TStreamConfig";
        onStreamConfigMessage(message.stream_config());
    }
}

void ClientSession::onStreamConfigMessage(const NSound::TClientMessage::TStreamConfiguration& message) {
    sessionConfig_ = message;
}

void ClientSession::error(const std::string& errorMessage) {
    PLOG(plog::error) << "ClientSession error: " + errorMessage;
    throw std::runtime_error(errorMessage);
}