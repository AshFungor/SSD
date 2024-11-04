// STD
#include <mutex>
#include <memory>

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
#include <src/ssd/core/server.hpp>
#include <src/ssd/core/session/context.hpp>
#include <src/ssd/core/interfaces/i-context.hpp>


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

void Server::notification(std::weak_ptr<IContext> context, EReason reason) {
    switch (reason) {
        case EReason::ABORTING:
            PLOG(plog::warning) << "[server] context requested abortion";
            break;
        case EReason::CLOSING:
            PLOG(plog::warning) << "[server] context requested close";
            break;
    }

    if (auto slave = context.lock()) {
        if (auto iter = std::find(contexts_.begin(), contexts_.end(), slave); iter != contexts_.end()) {
            std::iter_swap(std::next(contexts_.end(), -1), iter);
            contexts_.pop_back();
            PLOG(plog::info) << "[server] slave context " << slave.get() << " exited";
        } else {
            PLOG(plog::warning) << "[server] slave session " << slave.get() << " wants to exit, "
                << "but it was not found, ignoring";
        }
    } else {
        PLOG(plog::warning) << "[server] slave session wants to exit, but it is expired already (server does not own it?)";
    }
}

void Server::accept(std::shared_ptr<IContext> context, const boost::system::error_code& error) {
    if (error) {
       onNetworkError(error, false);
    }

    context->init();
    contexts_.emplace_back(std::move(context));
    PLOG(plog::info) << "[server] connecting new client";

    auto pair = factory_.AssembleAndReturn();
    acceptor_.async_accept(
        *pair.first, 
        boost::bind(&Server::accept, this, pair.second, boost::asio::placeholders::error)
    );
}

void Server::onNetworkError(const boost::system::error_code& error, bool isCritical) {
    if (isCritical) {
        throw std::runtime_error(absl::StrFormat("[server] critical error in server: %s", error.message()));
    }

    PLOG(plog::warning) << "[server] non-critical error in server: " << error.message();
}

ContextFactory& ContextFactory::withBuffer(std::size_t size) {
    state_.size = size;
    return *this;
}

ContextFactory& ContextFactory::withMaster(std::weak_ptr<IContext::IContextMaster> master) {
    state_.master = std::move(master);
    return *this;
}

ContextFactory& ContextFactory::withHandler(std::weak_ptr<IStreamHandler> handler) {
    state_.handler = std::move(handler);
    return *this;
}

ContextFactory& ContextFactory::withContext(std::shared_ptr<boost::asio::io_context> context) {
    state_.context = std::move(context);
    return *this;
}

std::pair<std::shared_ptr<boost::asio::ip::tcp::socket>, std::shared_ptr<IContext>> ContextFactory::AssembleAndReturn() {
    if (state_.context) {
        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(*state_.context);
        return std::make_pair(socket, Context::configure(state_.master, state_.context, socket, state_.handler, state_.size));
    }
    std::abort();
}