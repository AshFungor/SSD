// sockcpp
#include <iterator>
#include <sockpp/tcp_connector.h>
#include <sockpp/inet_address.h>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/tcp_socket.h>

// json
#include <nlohmann/json_fwd.hpp>

// standard
#include <cstdint>
#include <cerrno>
#include <memory>
#include <chrono>
#include <mutex>

// util
#include <plog/Severity.h>
#include <common/thread-pool.hpp>

// plog
#include <plog/Log.h>
#include <thread>

// local
#include "server.hpp"

using namespace srv;
using namespace std::chrono;


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
{}

void Server::subscribeOnConfigs() {
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

        // parse timeouts for server read
        if (config.contains("timeouts") && config["timeouts"].is_array()) {
            settings_.timeouts.reserve(config["timeouts"].size());
            for (const auto& value : config["timeouts"]) {
                if (!value.is_number_unsigned()) {
                    settings_.timeouts.clear();
                    break;
                }
                settings_.timeouts.push_back(std::chrono::milliseconds(value.get<std::size_t>()));
            }
        }

        if (settings_.timeouts.empty()) {
            settings_.timeouts = {1ms, 10ms, 100ms, 200ms, 500ms};
        }

        settings_.address = sockpp::inet_address(hostname, port);

        PLOG(plog::debug) 
            << "default config for server received, running "
            << "server on " << settings_.address.to_string();

        settings_.init = true;
        cv_.notify_one();
    }, weak_from_this());
}

void Server::parseDynamicConfig(const nlohmann::json& config) {
    // Do nothing
}

void Server::init() {
    subscribeOnConfigs();
    if (!settings_.init) {
        std::unique_lock<std::mutex> lock(settingsLock_);
        PLOG(plog::debug) << "locking settings, waiting for config to init() server";
        // wait until settings are available 
        cv_.wait(lock, [this]() {
            return settings_.init;
        });
    }

    acc_ = sockpp::tcp_acceptor(settings_.address);
    acc_.set_non_blocking();

    PLOG(plog::info) << "running server with address = " << acc_.address().to_string();
}

void Server::run() {
    while (true) {
        // Accept a new client connection
        auto result = acc_.accept();

        if (result.is_error()) {
            if (result.error().value() != EWOULDBLOCK) {
                PLOG(plog::error) << "Error accepting peer: " << result.error_message();
            }
            // FIXME: check for work on all sessions
            if (result.error().value() == EWOULDBLOCK) {
                std::this_thread::sleep_for(*currentTimeout_);
                if (settings_.timeouts.end() - currentTimeout_ > 1) {
                    std::advance(currentTimeout_, 1);
                }
            }
        }
        else {
            sockpp::tcp_socket sock = result.release();
            PLOG(plog::info) << "accepting new client " << sock.peer_address();
            sessions_.emplace_back(srv::ClientSession::instance(std::move(sock)));
            sessions_.back()->init();
        }

        for (auto& session : sessions_) {
            threadPool_->query([&]() {
                session->update();
            }, weak_from_this());
            if (!session->open()) {
                std::swap(session, sessions_.back());
                sessions_.pop_back();
            }
        }
    }   
}

Server::~Server() {
    for (auto& session : sessions_) {
        session->terminate();
    }
}