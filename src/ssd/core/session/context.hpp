#pragma once

// laar
#include <src/ssd/core/session/stream.hpp>
#include <src/ssd/core/message.hpp>
#include <src/ssd/core/interfaces/i-stream.hpp>
#include <src/ssd/core/interfaces/i-context.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// boost
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/detail/error_code.hpp>

// protos
#include <protos/common/directives.pb.h>
#include <protos/common/stream-configuration.pb.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <memory>
#include <cstdint>


namespace laar {

    class Context
        : public IContext
        , public IStream::IStreamMaster
        , public std::enable_shared_from_this<laar::Context> {
    public:
    
        using tcp = boost::asio::ip::tcp;

        static std::shared_ptr<Context> configure(
            std::weak_ptr<IContext::IContextMaster> master,
            std::shared_ptr<boost::asio::io_context> context, 
            std::shared_ptr<tcp::socket> socket,
            std::weak_ptr<IStreamHandler> handler,
            std::size_t bufferSize
        );

        // Network state
        struct NetworkState {

            NetworkState(std::size_t bufferSize) 
                : bufferSize(bufferSize)
                , buffer(std::make_unique<std::uint8_t[]>(bufferSize))
            {}

            const std::size_t bufferSize;
            std::unique_ptr<std::uint8_t[]> buffer;

            std::queue<laar::Message> responses;
        };
        
        // IContext implementation
        virtual void init() override;

        // IStream::IStreamMaster implementation
        virtual void abort(std::weak_ptr<IStream> slave, std::optional<std::string> reason) override;
        virtual void close(std::weak_ptr<IStream> slave) override;

        virtual ~Context() override = default;

    private:

        // --- CTORS & ASSIGNMENT OPERATORS ---
        // ctors
        Context() = delete;
        Context(const Context&) = delete;
        Context(Context&&) = delete;
        // operators
        Context& operator=(const Context&) = delete;
        Context& operator=(Context&&) = delete;
        // one and only
        Context(
            std::weak_ptr<IContext::IContextMaster> master,
            std::shared_ptr<boost::asio::io_context> context, 
            std::shared_ptr<tcp::socket> socket,
            std::weak_ptr<IStreamHandler> handler,
            std::size_t bufferSize
        );

        void onCriticalSessionError(absl::Status status);

        void patch(IContext::APIResult result, std::uint32_t id);
        void trail();
        void acknowledge();
        void acknowledgeWithCode(laar::MessageSimplePayloadType simple);
        void acknowledgeWithProtobuf(laar::MessageProtobufPayloadType protobuf);

        // --- NETWORK LOW LEVEL I/O ---
        // normal I/O handlers
        void read(const boost::system::error_code& error, std::size_t bytes);
        void write(const boost::system::error_code& error, std::size_t bytes);
        // static wrappers to preserve tokens' lifetime reqs
        static void sRead(
            std::shared_ptr<NetworkState> state, 
            std::weak_ptr<Context> session, 
            const boost::system::error_code& error, 
            std::size_t bytes
        );
        static void sWrite(
            std::shared_ptr<NetworkState> state, 
            std::weak_ptr<Context> session, 
            const boost::system::error_code& error, 
            std::size_t bytes
        );

    private:

        std::shared_ptr<NetworkState> networkState_;

        // Syncronization & thread safety
        std::mutex lock_;
        std::once_flag init_;

        // Client & server owning data
        std::shared_ptr<tcp::socket> socket_;
        std::shared_ptr<MessageFactory> factory_;
        std::shared_ptr<boost::asio::io_context> context_;

        // Client & server non-owning data
        std::weak_ptr<IContext::IContextMaster> master_;
        std::weak_ptr<IStreamHandler> handler_;

        std::vector<std::shared_ptr<Stream>> streams_;

    };

}