#pragma once

// laar
#include <src/ssd/core/message.hpp>
#include <src/ssd/core/interfaces/i-stream.hpp>
#include <src/ssd/core/interfaces/i-context.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// boost
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>
#include <boost/system/detail/error_code.hpp>

// protos
#include <protos/client/stream.pb.h>
#include <protos/common/directives.pb.h>
#include <protos/common/stream-configuration.pb.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <memory>


namespace laar {

    class Stream
        : public laar::IStream
        , public laar::IStreamHandler::IHandle::IListener
        , public std::enable_shared_from_this<Stream> {
    public:

        static std::shared_ptr<Stream> configure(
            std::shared_ptr<boost::asio::io_context> context,
            std::weak_ptr<IStream::IStreamMaster> master, 
            std::weak_ptr<IStreamHandler> handler
        );

        // IStream implementation
        virtual void init() override;
        virtual operator bool() const override;
        virtual IContext::APIResult onClientMessage(NSound::NClient::TStreamMessage message) override;

        // IStreamHandler::IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

    private:

        Stream() = delete;
        Stream(const Stream&) = delete;
        Stream(Stream&&) = delete;
        // operators
        Stream& operator=(const Stream&) = delete;
        Stream& operator=(Stream&&) = delete;

        Stream(
            std::shared_ptr<boost::asio::io_context> context,
            std::weak_ptr<IStream::IStreamMaster> master, 
            std::weak_ptr<IStreamHandler> handler
        );

        // --- PROTOBUF API ---
        IContext::APIResult onIOOperation(NSound::NClient::TStreamMessage::TPull message);
        IContext::APIResult onIOOperation(NSound::NClient::TStreamMessage::TPush message);
        IContext::APIResult onClose(NSound::NClient::TStreamMessage::TClose message);
        IContext::APIResult onStreamConfiguration(NSound::NCommon::TStreamConfiguration message);

        IContext::APIResult onDrain(NSound::NCommon::TStreamDirective message);
        IContext::APIResult onFlush(NSound::NCommon::TStreamDirective message);

    private:
        std::shared_ptr<boost::asio::io_context> context_;

        std::weak_ptr<IStreamHandler> handler_;
        std::weak_ptr<IStream::IStreamMaster> master_;
        std::shared_ptr<IStreamHandler::IHandle> handle_;

        std::optional<NSound::NCommon::TStreamConfiguration> streamConfig_;
    };

}

