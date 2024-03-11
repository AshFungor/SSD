// sockcpp
#include <cstdint>
#include <memory>
#include <mutex>
#include <nlohmann/json_fwd.hpp>
#include <sockpp/tcp_connector.h>
#include "sockpp/inet_address.h"
#include "sockpp/tcp_acceptor.h"
#include "sockpp/tcp_socket.h"

// util
#include <plog/Severity.h>
#include <common/thread-pool.hpp>

// plog
#include <plog/Log.h>

// local
#include "server.hpp"

using namespace srv;


namespace {
    constexpr auto SERVER_CONFIG = "server";
}

std::shared_ptr<Server> Server::configure(
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    std::shared_ptr<laar::ThreadPool> threadPool,
    std::shared_ptr<laar::ConfigHandler> configHandler)
{
    return std::make_shared<Server>(std::move(cbQueue), std::move(threadPool), std::move(configHandler), Private());
}

Server::Server(
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    std::shared_ptr<laar::ThreadPool> threadPool,
    std::shared_ptr<laar::ConfigHandler> configHandler,
    Private access)
    : threadPool_(std::move(threadPool))
    , configHandler_(std::move(configHandler))
    , cbQueue_(std::move(cbQueue))
{
    configHandler_->subscribeOnDefaultConfig(
        SERVER_CONFIG,
        [this](const nlohmann::json& config) {
            parseDefaultConfig(config);
        }, 
        weak_from_this()
    );
    configHandler_->subscribeOnDynamicConfig(
        SERVER_CONFIG, 
        [this](const nlohmann::json& config) {
            parseDynamicConfig(config);
        }, 
        weak_from_this()
    );
}

void Server::parseDefaultConfig(const nlohmann::json& config) {
    cbQueue_->query([this, config = config]() {
        std::unique_lock<std::mutex> lock(settingsLock_);
        std::string hostname = config.value<std::string>("hostname", "localhost");
        std::uint32_t port = config.value<std::uint32_t>("port", 5050);
        settings_.address = sockpp::inet_address(hostname, port);

        settings_.init = true;
        cv_.notify_one();
    }, weak_from_this());
}

void Server::parseDynamicConfig(const nlohmann::json& config) {
    // Do nothing
}

void Server::init() {
    {
        std::unique_lock<std::mutex> lock(settingsLock_);
        // wait until settings are available 
        cv_.wait(lock, [this]() {
            return settings_.init;
        });
    }

    acc_ = sockpp::tcp_acceptor(settings_.address);
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
            sessions_.emplace_back(srv::ClientSession::instance(std::move(sock)));
            sessions_.back()->init();
        }

        for (auto& session : sessions_) {
            threadPool_->query([&]() {
                session->update();
            });
        }
    }   
}

Server::~Server() {
    for (auto& session : sessions_) {
        session->terminate();
    }
}