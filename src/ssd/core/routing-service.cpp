// STD
#include <mutex>
#include <memory>
#include <optional>

// Boost
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/execution_context.hpp>

// Abseil (google common libs)
#include <absl/status/status.h>
#include <absl/status/statusor.h>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/session.hpp>
#include <src/ssd/core/routing-service.hpp>

// codegen for protos
#include <protos/service/base.pb.h>
#include <protos/client/base.pb.h>

using namespace laar;

std::shared_ptr<Server> Server::create(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port) {
    return std::shared_ptr<Server>(new Server(std::move(handler), std::move(context), port));
}

Server::Server(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port)
    : acceptor_(*context, tcp::endpoint(tcp::v4(), port))
    , context_(std::move(context))
    , handler_(std::move(handler))
{}

void Server::init() {
    std::call_once(init_, [this]() {
        // this is first call to our server API - right place to init factory
        factory_
            .withContext(context_)
            .withBuffer(laar::NetworkBufferSize)
            .withHandler(handler_)
            .withMaster(weak_from_this());

        auto pair = factory_.AssembleAndReturn();
        acceptor_.async_accept(
            *pair.first,
            boost::bind(&Server::accept, this, pair.second, boost::asio::placeholders::error)
        );
    });
}

void Server::abort(std::weak_ptr<Session> slave, std::optional<std::string> reason) {
    if (auto pickedSlave = slave.lock()) {
        if (auto iter = std::find(sessions_.begin(), sessions_.end(), pickedSlave); iter != sessions_.end()) {
            PLOG(plog::info) 
                << "slave session " << pickedSlave.get() << " requested abortion, reason: "
                << ((reason.has_value()) ? reason.value() : "reason was not provided");
            std::iter_swap(std::next(sessions_.end(), -1), iter);
            sessions_.pop_back();
        } else {
            PLOG(plog::warning) 
                << "slave session " << pickedSlave.get() << " requested abortion, "
                << "but it was not found in sessions array, ignoring";
        }
    } else {
        PLOG(plog::warning) 
            << "slave session requested abortion, "
            << "but it could not be locked, ignoring";
    }
}

void Server::close(std::weak_ptr<Session> slave) {
    if (auto pickedSlave = slave.lock()) {
        if (auto iter = std::find(sessions_.begin(), sessions_.end(), pickedSlave); iter != sessions_.end()) {
            PLOG(plog::info) 
                << "slave session " << pickedSlave.get() << " exited normally";
            std::iter_swap(std::next(sessions_.end(), -1), iter);
            sessions_.pop_back();
        } else {
            PLOG(plog::warning) 
                << "slave session " << pickedSlave.get() << " requested close, "
                << "but it was not found in sessions array, ignoring";
        }
    } else {
        PLOG(plog::warning) 
            << "slave session requested close, "
            << "but it could not be locked, ignoring";
    }
}

void Server::accept(std::shared_ptr<Session> session, const boost::system::error_code& error) {
    if (error) {
       onNetworkError(error, false);
    }

    absl::Status status = session->init();
    if (!status.ok()) {
        PLOG(plog::info) << "failed to connect new client, session init() failed: " << status.ToString();
    } else {
        sessions_.emplace_back(std::move(session));
        PLOG(plog::info) << "connecting new client";
    }


    auto pair = factory_.AssembleAndReturn();
    acceptor_.async_accept(
        *pair.first, 
        boost::bind(&Server::accept, this, pair.second, boost::asio::placeholders::error)
    );
}

void Server::onNetworkError(const boost::system::error_code& error, bool isCritical) {
    if (isCritical) {
        throw std::runtime_error(absl::StrFormat("critical error in server: %s", error.message()));
    }

    PLOG(plog::warning) << "non-critical error in server: " << error.message();
}