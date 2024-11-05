#pragma once

// boost
#include <boost/asio/post.hpp>
#include <boost/asio/io_context.hpp>

// pulse error codes
#include <pulse/def.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>

// abseil
#include <absl/status/status.h>

// STD
#include <string>
#include <memory>
#include <cstdint>
#include <optional>

namespace laar {

    class IContext {
    public:

        using ResponseType = std::variant<MessageProtobufPayloadType, MessageSimplePayloadType>;
        using OptionalResponseType = std::optional<ResponseType>;

        struct APIResult {

            inline explicit APIResult(absl::Status status, OptionalResponseType response = std::nullopt)
                : status(std::move(status))
                , response(std::move(response))
            {}

            inline static APIResult unimplemented(std::optional<std::string> message = std::nullopt) {
                return APIResult{
                    absl::InternalError((message.has_value()) ? message.value() : "API Call unimplemented"),
                    laar::ERROR
                };
            }

            inline static APIResult misconfiguration(std::optional<std::string> message = std::nullopt) {
                return APIResult{
                    absl::InternalError((message.has_value()) ? message.value() : "API Call misconfigured"),
                    laar::ERROR
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
                boost::asio::post(*ioc, boost::bind(&IContextMaster::notification, this, std::move(context), reason));
            }

            // notify master about reason
            // call this function with wrapper only!
            virtual void notification(std::weak_ptr<IContext> context, EReason reason) = 0;

        };

        virtual void init() = 0;
        virtual ~IContext() = default;

    };

}