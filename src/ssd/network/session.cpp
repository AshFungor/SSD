// sockpp
#include <asm-generic/errno.h>
#include <mutex>
#include <sockpp/tcp_connector.h>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/result.h>

// standard
#include <stdexcept>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>

// local
#include "common/exceptions.hpp"
#include "common/shared-buffer.hpp"
#include "message.hpp"
#include "protos/client/simple/simple.pb.h"
#include "session.hpp"

using namespace srv;


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

    if (open()) {
        throw laar::LaarBadInit();
    }
    
    PLOG(plog::debug) << "Checking connection on client: "
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
        return true;
    }

    std::unique_lock<std::mutex> locked (sessionLock_);
    isUpdating_ = true;

    if (!sock_.is_open()) {
        PLOG(plog::debug) << "Socket " << sock_.address() << " was closed by peer";
        return false;
    }
    if (isMessageBeingReceived_) {
        auto requestedBytes = message_.requestedBytes();
        auto res = sock_.recv(buffer_->getRawWriteCursor(0), requestedBytes);

        if (res.is_error()) {
            if (res.error().value() != EWOULDBLOCK) {
                handleErrorState(std::move(res));
            } else {
                return false;
            }
        }

        laar::Receive event (res.value());
        buffer_->getRawWriteCursor(event.size);
        message_.handle(event);

        if (message_.isInMessageTrailState()) {
            NSound::TClientMessage clientMessage;
            *clientMessage.mutable_simple_message() = message_.getPayload();
            onClientMessage(clientMessage);
        }
        
    } else {
        message_.reset();
        isMessageBeingReceived_ = true;
    }
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
    }
}

void ClientSession::onStreamConfigMessage(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message) {
    // handle double config on same stream
    PLOG(plog::debug) 
        << "config for peer: " << sock_.peer_address()
        << " is " << message.DebugString();
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