// sockpp
#include <asm-generic/errno.h>
#include <sockpp/tcp_connector.h>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/result.h>

// standard
#include <stdexcept>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>

// local
#include "message.hpp"
#include "session.hpp"

using namespace srv;


ClientSession::ClientSession(sockpp::tcp_socket&& sock) 
: sock_(std::move(sock))
, isMessageBeingReceived_(false)
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
    if (isMessageBeingReceived_) {
        auto res = sock_.recv(buffer_.data(), std::min(laar::Message::wantMore(), buffer_.size()));
        laar::Message::dispatch(laar::PartialReceive(buffer_.data(), res.value()));

        if (laar::Message::is_in_state<laar::EndChunk>()) {
            auto msg = laar::Message::get();
            onClientMessage(std::move(msg));
            isMessageBeingReceived_ = false;
        }
    } else {
        laar::Message::reset();
        isMessageBeingReceived_ = true;
    }
}

void ClientSession::handleErrorState(sockpp::result<std::size_t> requestState) {
    if (requestState.error().value() != EWOULDBLOCK) {
        error("Error on socket read: " + requestState.error_message());
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