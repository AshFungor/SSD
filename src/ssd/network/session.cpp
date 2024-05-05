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
#include "network/sync-protocol.hpp"

using namespace srv;

ClientSession::ClientSession(
    sockpp::tcp_socket&& sock, 
    std::shared_ptr<laar::IStreamHandler> soundHandler,
    Private access) 
: sock_(std::move(sock))
, buffer_(std::make_shared<laar::PlainBuffer>(4096))
, builder_(laar::MessageBuilder::configure(buffer_))
, protocol_(laar::SyncProtocol::configure(weak_from_this(), std::move(soundHandler)))
{}

ClientSession::~ClientSession() {
    terminate();
}

std::shared_ptr<ClientSession> ClientSession::instance(
    sockpp::tcp_socket&& sock,
    std::shared_ptr<laar::IStreamHandler> soundHandler) 
{
    return std::make_shared<ClientSession>(std::move(sock), std::move(soundHandler), Private());
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
        protocol_->onClientMessage(std::move(result));
        // reset buffer to clear space for next message (risking overflow otherwise)
        buffer_->reset();
    }

    return true;
}

void ClientSession::onReply(std::unique_ptr<char[]> buffer, std::size_t size) {
    std::unique_lock<std::mutex> locked (sessionLock_);

    // reply to user
}

void ClientSession::onClose() {
    // Do nothing
}

void ClientSession::handleErrorState(sockpp::result<std::size_t> requestState) {
    error("Error on socket read: " + requestState.error_message());
}

bool ClientSession::open() {
    std::unique_lock<std::mutex> locked (sessionLock_);
    return sock_.is_open();
}

void ClientSession::error(const std::string& errorMessage) {
    PLOG(plog::error) << "ClientSession error: " + errorMessage;
    throw std::runtime_error(errorMessage);
}