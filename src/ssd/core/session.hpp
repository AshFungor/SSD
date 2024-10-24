#pragma once

// laar
#include "src/ssd/core/header.hpp"
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <optional>
#include <src/ssd/core/session.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/server-message.pb.h>
#include <protos/client-message.pb.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <memory>
#include <mutex>
#include <cstdint>

namespace laar {

    namespace session_state {

        // selector
        inline constexpr std::uint32_t BUFFER = 0x1;
        inline constexpr std::uint32_t PROTOCOL = 0x2;

        namespace buffer {
            // check for closed
            inline constexpr std::uint32_t BUFFER_CLOSED = 0x1;
            // drained means that last call to drain was completed
            inline constexpr std::uint32_t BUFFER_DRAINED = 0x2;
            // same but for flush
            inline constexpr std::uint32_t BUFFER_FLUSHED = 0x4;
        }

        namespace protocol {
            // message header
            inline constexpr std::uint32_t PROTOCOL_HEADER = 0x1;
            // payload (main part)
            inline constexpr std::uint32_t PROTOCOL_PAYLOAD = 0x2;
        }
    }

    class Session 
        : public laar::IStreamHandler::IHandle::IListener
        , std::enable_shared_from_this<Session> {
    public:

        // aliases
        using tcp = boost::asio::ip::tcp;
        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;

        // --- SUPPORTING CLASSES ---
        // Session master should be available for communication
        // with its slaves (sessions).
        class ISessionMaster {
        public:
            // cannot recover session: abort and close connection, providing
            // optional reason why.
            virtual void abort(std::weak_ptr<Session> slave, std::optional<std::string> reason) = 0;
            // close session on normal client exit
            virtual void close(std::weak_ptr<Session> slave) = 0;
            virtual ~ISessionMaster() = default;
        };
        // API Result is generic result of API call.
        struct APIResult {

            inline static APIResult unimplemented(
                std::optional<std::string> message = std::nullopt
            ) {
                APIResult result;
                if (message.has_value()) {
                    result.status = absl::InternalError(message.value());
                } else {
                    result.status = absl::InternalError("API Call unimplemented");
                }
                result.response = std::nullopt;
                return result;
            }

            inline static APIResult misconfiguration(
                std::optional<std::string> message = std::nullopt
            ) {
                APIResult result;
                if (message.has_value()) {
                    result.status = absl::InternalError(message.value());
                } else {
                    result.status = absl::InternalError("API Call misconfigured");
                }
                result.response = std::nullopt;
                return result;
            }

            inline static APIResult make(
                    absl::Status status, 
                    std::optional<NSound::TServiceMessage> response
            ) {
                APIResult result;
                result.status = status;
                result.response = response;
                return result;
            }

            absl::Status status;
            std::optional<NSound::TServiceMessage> response;
        };
        // Session manages in state via a set of states of its components,
        // therefore there is a need to wrap them all in one single object.
        struct SessionState {
            std::uint32_t protocol = 0;
            std::uint32_t buffer = 0;
        };

        // Network state
        struct NetworkState {

            NetworkState(std::size_t bufferSize) 
                : bufferSize_(bufferSize)
                , buffer_(std::make_unique<std::uint8_t[]>(bufferSize))
            {}

            Header header;
            std::optional<NSound::TServiceMessage> result;
            // just network buffer :)
            const std::size_t bufferSize_;
            const std::unique_ptr<std::uint8_t[]> buffer_;
        };

        // --- CONFIGURATION & INITIALIZATION ---
        static std::shared_ptr<Session> configure(
            std::weak_ptr<ISessionMaster> master,
            std::shared_ptr<boost::asio::io_context> context, 
            std::shared_ptr<tcp::socket> socket,
            std::weak_ptr<IStreamHandler> handler,
            std::size_t bufferSize
        );
        absl::Status init(); 

        // --- INTERFACES ---
        // IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

        // --- MISC ---
        // check is session is ok
        operator bool() const;
        // generic state wrapper
        const SessionState& state() const;

        ~Session();

    private:

        // --- CTORS & ASSIGNMENT OPERATORS ---
        // ctors
        Session() = delete;
        Session(const Session&) = delete;
        Session(Session&&) = delete;
        // operators
        Session& operator=(const Session&) = delete;
        Session& operator=(Session&&) = delete;
        // one and only
        Session(
            std::weak_ptr<ISessionMaster> master,
            std::shared_ptr<boost::asio::io_context> context, 
            std::shared_ptr<tcp::socket> socket,
            std::weak_ptr<IStreamHandler> handler,
            std::size_t bufferSize
        );

        // --- PROTOBUF API ---
        // -- ROUTING --
        // client message routing
        APIResult onClientMessage(NSound::TClientMessage message);
        // base messages
        APIResult onBaseMessage(TBaseMessage message);
        // -- ACTUAL API CALLS --
        // Data processing
        APIResult onIOOperation(TBaseMessage::TPull message);
        APIResult onIOOperation(TBaseMessage::TPush message);
        APIResult onStreamConfiguration(TBaseMessage::TStreamConfiguration message);
        // Manipulation
        APIResult onDrain(TBaseMessage::TStreamDirective message);
        APIResult onFlush(TBaseMessage::TStreamDirective message);
        APIResult onClose(TBaseMessage::TStreamDirective message);

        // --- SESSION STATE MANAGEMENT ---
        // handle state management flags
        absl::Status onProtocolTransition(std::uint32_t state);
        void onCriticalSessionError(absl::Status status);
        // targeted setter & getter, use to set state in other methods
        void set(std::uint32_t flag, std::uint32_t state);
        void unset(std::uint32_t flag, std::uint32_t state);

        // --- NETWORK LOW LEVEL I/O ---
        // normal I/O handlers
        void read(const boost::system::error_code& error, std::size_t bytes);
        void write(const boost::system::error_code& error, std::size_t bytes);
        // additional parsing components
        absl::Status readHeader(std::size_t bytes);
        absl::Status readPayload(std::size_t bytes);
        // static wrappers to preserve tokens' lifetime reqs
        static void sRead(
            std::shared_ptr<NetworkState> state, 
            std::weak_ptr<Session> session, 
            const boost::system::error_code& error, 
            std::size_t bytes
        );
        static void sWrite(
            std::shared_ptr<NetworkState> state, 
            std::weak_ptr<Session> session, 
            const boost::system::error_code& error, 
            std::size_t bytes
        );

    private:

        std::shared_ptr<NetworkState> networkState_;

        // Syncronization & thread safety
        std::mutex lock_;
        std::once_flag init_;

        // Common state
        SessionState state_;

        // Client & server owning data
        std::shared_ptr<tcp::socket> socket_;
        std::shared_ptr<IStreamHandler::IHandle> handle_;
        std::shared_ptr<boost::asio::io_context> context_;

        // Client & server non-owning data
        std::weak_ptr<ISessionMaster> master_;
        std::weak_ptr<IStreamHandler> handler_;

        // just stream config :)
        std::optional<TBaseMessage::TStreamConfiguration> streamConfig_;

    };

    class SessionFactory {
    public:

        SessionFactory() = default;

        // builder stages
        SessionFactory& withBuffer(std::size_t size);
        SessionFactory& withMaster(std::weak_ptr<Session::ISessionMaster> master);
        SessionFactory& withHandler(std::weak_ptr<IStreamHandler> handler);
        SessionFactory& withContext(std::shared_ptr<boost::asio::io_context> context);

        std::pair<std::shared_ptr<boost::asio::ip::tcp::socket>, std::shared_ptr<Session>> AssembleAndReturn();

    private:

        struct CurrentState {
            std::size_t size;
            std::weak_ptr<Session::ISessionMaster> master;
            std::weak_ptr<IStreamHandler> handler;
            std::shared_ptr<boost::asio::io_context> context;
        } state_;

    };

}