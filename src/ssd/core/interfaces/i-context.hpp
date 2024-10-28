#pragma once

// boost
#include "protos/server-message.pb.h"
#include <boost/bind.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/io_context.hpp>

// pulse error codes
#include <pulse/def.h>

// laar
#include <optional>
#include <src/ssd/macros.hpp>

// abseil
#include <absl/status/status.h>

// STD
#include <string>
#include <memory>
#include <cstdint>

namespace laar {

    class IContext {
    public:

        using QuickResponseType = std::uint32_t;
        using ProtobufResponseType = NSound::TServiceMessage;
        using ResponseType = std::variant<ProtobufResponseType, QuickResponseType>;
        using OptionalResponseType = std::optional<ResponseType>;

        struct APIResult {

            inline explicit APIResult(absl::Status status, OptionalResponseType response = std::nullopt)
                : status(std::move(status))
                , response(std::move(response))
            {}

            inline static APIResult unimplemented(std::optional<std::string> message = std::nullopt) {
                return APIResult{
                    absl::InternalError((message.has_value()) ? message.value() : "API Call unimplemented"),
                    PA_ERR_INTERNAL
                };
            }

            inline static APIResult misconfiguartion(std::optional<std::string> message = std::nullopt) {
                return APIResult{
                    absl::InternalError((message.has_value()) ? message.value() : "API Call misconfigured"),
                    PA_ERR_NOTSUPPORTED
                };
            }

            absl::Status status;
            OptionalResponseType response;
        };

        class IContextMaster {
        public:

            enum class EReason : std::uint8_t {
                ABORTING, CLOSING
            };

            inline void wrapNotification(
                std::shared_ptr<boost::asio::io_context> ioc, 
                std::weak_ptr<IContext> context, 
                EReason reason
            ) {
                ENSURE_NOT_NULL(ioc);
                boost::asio::post(*ioc, boost::bind(&IContextMaster::notify, this, std::move(context), reason));
            }

            // notify master about reason
            // call this function with wrapper only!
            virtual void notify(std::weak_ptr<IContext> context, EReason reason) = 0;

            // getters
            virtual std::string name() = 0;
            virtual std::uint32_t index() = 0;
            virtual std::uint32_t version() = 0;

        };

        virtual void init() = 0;
        virtual ~IContext() = default;

    };

}