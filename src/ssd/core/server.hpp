#pragma once

// STD
#include <mutex>
#include <memory>

// Boost
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/execution_context.hpp>

// Abseil (google common libs)
#include <absl/status/status.h>
#include <absl/status/statusor.h>

// laar
#include <src/ssd/core/interfaces/i-context.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>


namespace laar {

    class ContextFactory {
    public:

        ContextFactory() = default;
        ContextFactory(const ContextFactory&) = delete;
        ContextFactory(ContextFactory&&) = delete;

        // builder stages
        ContextFactory& withBuffer(std::size_t size);
        ContextFactory& withMaster(std::weak_ptr<IContext::IContextMaster> master);
        ContextFactory& withHandler(std::weak_ptr<IStreamHandler> handler);
        ContextFactory& withContext(std::shared_ptr<boost::asio::io_context> context);

        std::pair<std::shared_ptr<boost::asio::ip::tcp::socket>, std::shared_ptr<IContext>> AssembleAndReturn();

    private:

        struct CurrentState {
            std::size_t size = 0;
            std::weak_ptr<IContext::IContextMaster> master;
            std::weak_ptr<IStreamHandler> handler;
            std::shared_ptr<boost::asio::io_context> context = nullptr;
        } state_;

    };

    class Server 
        : public laar::IContext::IContextMaster
        , public std::enable_shared_from_this<Server> {
    public:

        using tcp = boost::asio::ip::tcp;

        static std::shared_ptr<Server> create(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port);
        void init();
        
        // laar::IContext::IContextMaster implementation
        virtual void notification(std::weak_ptr<IContext> context, EReason reason) override;

    private:

        Server() = delete;
        Server(const Server&) = delete;
        Server(Server&&) = delete;
        Server& operator=(const Server&) = delete;
        Server& operator=(Server&&) = delete;

        Server(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port);

        void accept(std::shared_ptr<IContext> context, const boost::system::error_code& error);
        void onNetworkError(const boost::system::error_code& error, bool isCritical);

    private:

        std::once_flag init_;

        ContextFactory factory_;
        std::vector<std::shared_ptr<IContext>> contexts_;

        tcp::acceptor acceptor_;
        std::shared_ptr<boost::asio::io_context> context_;
        std::weak_ptr<IStreamHandler> handler_;

    };

}