#pragma once

// boost
#include <boost/bind.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/io_context.hpp>

// laar
#include <src/ssd/macros.hpp>

// abseil
#include <absl/status/status.h>

// STD
#include <string>

// laar
#include <src/ssd/core/interfaces/i-context.hpp>

// proto
#include <protos/client/stream.pb.h>
#include <protos/server-message.pb.h>
#include <protos/client-message.pb.h>

namespace laar {

    class IStream {
    public:

        class IStreamMaster {
        public:
            // cannot recover session: abort and close connection, providing
            // optional reason why.
            virtual void abort(std::weak_ptr<IStream> slave, std::optional<std::string> reason) = 0;
            // close session on normal client exit
            virtual void close(std::weak_ptr<IStream> slave) = 0;

            inline void wrapClose(
                std::shared_ptr<boost::asio::io_context> ioc, 
                std::weak_ptr<IStream> stream
            ) {
                ENSURE_NOT_NULL(ioc);
                boost::asio::post(*ioc, boost::bind(&IStreamMaster::close, this, std::move(stream)));
            }

            inline void wrapAbort(
                std::shared_ptr<boost::asio::io_context> ioc, 
                std::weak_ptr<IStream> stream,
                std::optional<std::string> reason
            ) {
                ENSURE_NOT_NULL(ioc);
                boost::asio::post(*ioc, boost::bind(&IStreamMaster::abort, this, std::move(stream), reason));
            }

            virtual ~IStreamMaster() = default;

        };

        virtual void init() = 0;
        virtual operator bool() const = 0;

        virtual IContext::APIResult onClientMessage(NSound::NClient::TStreamMessage message) = 0;

        virtual ~IStream() = default;

    };

}