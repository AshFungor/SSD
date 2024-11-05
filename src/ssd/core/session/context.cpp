// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/core/session/stream.hpp>
#include <src/ssd/core/session/context.hpp>
#include <src/ssd/core/interfaces/i-context.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// boost
#include <boost/asio/post.hpp>
#include <boost/bind/bind.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/placeholders.hpp>

// protos
#include <protos/holder.pb.h>
#include <protos/client/stream.pb.h>
#include <protos/client/context.pb.h>
#include <protos/common/directives.pb.h>

// Abseil
#include <absl/status/status.h>
#include <absl/strings/str_format.h>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// STD
#include <mutex>
#include <memory>
#include <utility>
#include <cstdint>
#include <variant>

using namespace laar;

namespace {

    template<typename Functor, typename... Args>
    auto bindCall(Functor f, Args&&... args) -> decltype(
        boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred)
    ) {
        return boost::bind(f, args..., boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    }

    boost::asio::mutable_buffer makeResponse(laar::Message message, std::unique_ptr<std::uint8_t[]>& buffer, std::size_t maxBytes) {
        message.writeToArray(buffer.get(), maxBytes);
        return boost::asio::buffer(buffer.get(), laar::Message::Size::total(&message));
    }

}

std::shared_ptr<Context> Context::configure(
    std::weak_ptr<IContext::IContextMaster> master,
    std::shared_ptr<boost::asio::io_context> context, 
    std::shared_ptr<tcp::socket> socket,
    std::weak_ptr<IStreamHandler> handler,
    std::size_t bufferSize
) {
    return std::shared_ptr<Context>{new Context{std::move(master), std::move(context), std::move(socket), std::move(handler), bufferSize}};
}

Context::Context(
    std::weak_ptr<IContext::IContextMaster> master,
    std::shared_ptr<boost::asio::io_context> context, 
    std::shared_ptr<tcp::socket> socket,
    std::weak_ptr<IStreamHandler> handler,
    std::size_t bufferSize
)
    : networkState_(std::make_shared<NetworkState>(bufferSize))
    , socket_(std::move(socket))
    , factory_(laar::MessageFactory::configure())
    , context_(std::move(context))
    , master_(std::move(master))
    , handler_(std::move(handler))
{}

void Context::init() {
    std::call_once(init_, [this]() mutable {
        socket_->async_read_some(
        boost::asio::mutable_buffer(networkState_->buffer.get(), factory_->next()),
        bindCall(&Context::sRead, networkState_, weak_from_this())
        );
    });
}

void Context::read(const boost::system::error_code& error, std::size_t bytes) {
    PLOG(plog::debug) << "[context] received " << bytes;

    if (error) {
        onCriticalSessionError(absl::InternalError(error.what()));
        return;
    }

    std::size_t size = networkState_->bufferSize;
    factory_->parse(networkState_->buffer.get(), size);

    if (factory_->isParsedAvailable()) {
        auto message = factory_->parsed();
        // check on message here

        PLOG(plog::debug) << "[context] message ready, routing it";
        if (message.type() == laar::message::type::PROTOBUF) {
            PLOG(plog::debug) << "[context] received proto message";
            // parse and handle protos accordingly
            NSound::THolder holder = laar::messagePayload<laar::message::type::PROTOBUF>(message);
            if (!holder.has_client()) {
                onCriticalSessionError(absl::InternalError("[context] got holder with no client message"));
            }

            if (holder.client().has_context_message()) {
                PLOG(plog::debug) << "[context] message belongs to context, running matching";
                NSound::NClient::TContextMessage message = std::move(*holder.mutable_client()->mutable_context_message());
                if (message.has_connect()) {
                    PLOG(plog::info) << "[context] connecting new context with name: " << message.connect().name();
                    acknowledge();
                }
            } 

            if (holder.client().has_stream_message()) {
                // only new stream connections are partially handled in context
                PLOG(plog::debug) << "[context] message belongs to stream, checking on it";
                // check on stream id. UINT32 is invalid and is signal for new stream
                NSound::NClient::TStreamMessage message = std::move(*holder.mutable_client()->mutable_stream_message());
                std::shared_ptr<Stream> selected = nullptr;
                std::uint32_t id = 0;
                if (message.stream_id() == UINT32_MAX) {
                    PLOG(plog::debug) << "[context] received UINT32_MAX, appending new stream; current count: " << streams_.size();
                    streams_.push_back(laar::Stream::configure(context_, weak_from_this(), handler_));
                    selected = streams_.back();
                    id = streams_.size() - 1;
                } else if (message.stream_id() > streams_.size()) {
                    onCriticalSessionError(absl::InternalError(absl::StrFormat("received stream index out of bounds: %d, count: %d", message.stream_id(), streams_.size())));
                } else {
                    PLOG(plog::debug) << "[context] selected index: " << message.stream_id();
                    selected = streams_[message.stream_id()];
                    id = streams_.size() - message.stream_id();
                }

                PLOG(plog::debug) << "[context] sending message down to stream";
                patch(selected->onClientMessage(std::move(message)), id);
            }
        }

        if (message.type() == laar::message::type::SIMPLE) {
            PLOG(plog::debug) << "[context] received simple message";
            MessageSimplePayloadType code = laar::messagePayload<laar::message::type::SIMPLE>(message);
            if (code == laar::TRAIL) {
                PLOG(plog::debug) << "[context] received trail";
                // end of stream, switch to write and begin writing responses
                if (networkState_->responses.size()) {
                    laar::Message message = std::move(networkState_->responses.front());
                    networkState_->responses.pop();
                    socket_->async_write_some(
                        makeResponse(std::move(message), networkState_->buffer, networkState_->bufferSize),
                        bindCall(&Context::sWrite, networkState_, weak_from_this())
                    );
                    trail();
                    return;
                } else {
                    onCriticalSessionError(absl::InternalError("no messages to write back"));
                }
            } 
        }

    }

    socket_->async_read_some(
        boost::asio::mutable_buffer(networkState_->buffer.get(), factory_->next()),
        bindCall(&Context::sRead, networkState_, weak_from_this())
    );
}

void Context::write(const boost::system::error_code& error, std::size_t bytes) {
    UNUSED(bytes);
    PLOG(plog::debug) << "[context] writing message";

    if (error) {
        onCriticalSessionError(absl::InternalError(error.what()));
        return;
    }

    std::size_t offset = 0;
    if (networkState_->responses.size()) {
        while (networkState_->responses.size()) {
            laar::Message message = std::move(networkState_->responses.front());
            PLOG(plog::debug) << "[context] writing " << laar::Message::Size::total(&message);
            networkState_->responses.pop();
            message.writeToArray(networkState_->buffer.get() + offset, networkState_->bufferSize);
            offset += laar::Message::Size::total(&message);
        }
        socket_->async_write_some(
            boost::asio::buffer(networkState_->buffer.get(), offset),
            bindCall(&Context::sWrite, networkState_, weak_from_this())
        );
        return;
    }

    PLOG(plog::debug) << "[context] switching to read";
    socket_->async_read_some(
        boost::asio::mutable_buffer(networkState_->buffer.get(), factory_->next()),
        bindCall(&Context::sRead, networkState_, weak_from_this())
    );
}

void Context::sRead(std::shared_ptr<NetworkState> state, std::weak_ptr<Context> context, const boost::system::error_code& error, std::size_t bytes) {
    UNUSED(state);
    if (auto that = context.lock()) {
        that->read(error, bytes);
    }
}

void Context::sWrite(std::shared_ptr<NetworkState> state, std::weak_ptr<Context> context, const boost::system::error_code& error, std::size_t bytes) {
    UNUSED(state);
    if (auto that = context.lock()) {
        that->write(error, bytes);
    }
}

void Context::onCriticalSessionError(absl::Status status) {
    PLOG(plog::error) << "[context] error: " << status.ToString();
    if (auto master = master_.lock()) {
        master->wrapNotification(context_, weak_from_this(), IContext::IContextMaster::EReason::ABORTING);
    }
}

void Context::patch(IContext::APIResult result, std::uint32_t id) {
    if (!result.status.ok()) {
        PLOG(plog::error) << "[context] API failed: " << result.status.ToString();
        acknowledgeWithCode(laar::ERROR);
    }

    if (result.response.has_value()) {
        auto variant = result.response.value();
        if (std::holds_alternative<MessageSimplePayloadType>(variant)) {
            acknowledgeWithCode(std::get<MessageSimplePayloadType>(variant));
        } else if (std::holds_alternative<MessageProtobufPayloadType>(variant)) {
            auto proto = std::move(get<MessageProtobufPayloadType>(variant));
            proto.mutable_server()->mutable_stream_message()->set_stream_id(id);
            acknowledgeWithProtobuf(std::move(proto));
        } else {
            PLOG(plog::error) << "[context] response holds no value, although is was added";
            acknowledgeWithCode(laar::ERROR);
        }
        return;
    }

    acknowledge();
}

void Context::acknowledge() {
    acknowledgeWithCode(laar::ACK);
}

void Context::trail() {
    acknowledgeWithCode(laar::TRAIL);
}

void Context::acknowledgeWithCode(laar::MessageSimplePayloadType simple) {
    laar::Message message = factory_->withType(message::type::SIMPLE)
        .withPayload(simple)
        .construct()
        .constructed();
    networkState_->responses.push(std::move(message));
}

void Context::acknowledgeWithProtobuf(laar::MessageProtobufPayloadType protobuf) {
    laar::Message message = factory_->withType(message::type::PROTOBUF)
        .withPayload(std::move(protobuf))
        .construct()
        .constructed();
    networkState_->responses.push(std::move(message));
}

void Context::abort(std::weak_ptr<IStream> slave, std::optional<std::string> reason) {
    if (reason.has_value()) {
        PLOG(plog::error) << "[context] stream aborting: " << reason.value();
    } else {
        PLOG(plog::error) << "[context] stream aborting: no reason provided.";
    }

    if (auto stream = slave.lock()) {
        if (auto iter = std::find(streams_.begin(), streams_.end(), stream); iter != streams_.end()) {
            iter->reset();
            PLOG(plog::info) << "[context] slave stream " << stream.get() << " exited";
        } else {
            PLOG(plog::warning) << "[context] slave stream " << stream.get() << " wants to exit, "
                << "but it was not found, ignoring";
        }
    } else {
        PLOG(plog::warning) << "[context] slave stream wants to exit, but it is expired already (server does not own it?)";
    }
}

void Context::close(std::weak_ptr<IStream> slave) {
    if (auto stream = slave.lock()) {
        if (auto iter = std::find(streams_.begin(), streams_.end(), stream); iter != streams_.end()) {
            iter->reset();
            PLOG(plog::info) << "[context] slave stream " << stream.get() << " exited";
        } else {
            PLOG(plog::warning) << "[context] slave stream " << stream.get() << " wants to exit, "
                << "but it was not found, ignoring";
        }
    } else {
        PLOG(plog::warning) << "[context] slave stream wants to exit, but it is expired already (server does not own it?)";
    }
}