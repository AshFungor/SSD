// sockcpp
#include <sockpp/tcp_connector.h>
#include "sockpp/inet_address.h"
#include "sockpp/tcp_socket.h"

// util
#include <plog/Severity.h>
#include <util/thread-pool.hpp>

// plog
#include <plog/Log.h>

// local
#include "server.hpp"

using namespace srv;

Server& Server::getInstance() {
    if (!instance_) {
        instance_.reset(new Server);
    }
    return *instance_;
}

Server::Server() 
: acc_(5050)
{}

void Server::init() {
    acc_.set_non_blocking();
}

void Server::run() {
    while (true) {
        // Accept a new client connection
        auto result = acc_.accept();

        if (result.is_error()) {
            if (result.error().value() != EWOULDBLOCK) {
                PLOG(plog::debug) << "Error accepting message: " << result.error_message();
            }
        }
        else {
            sockpp::tcp_socket sock = result.release();
            ClientSession session (std::move(sock));
            session.init();
        }

        for (auto& session : sessions_) {
            util::ThreadPool::getInstance().query([&]() {
                session.update();
            });
        }
    }   
}

Server::~Server() {
    for (auto& session : sessions_) {
        session.terminate();
    }
}