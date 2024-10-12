// laar
#include "protos/service/base.pb.h"
#include <cstdint>
#include <src/ssd/core/session.hpp>
#include <src/ssd/core/routing-service.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// grpc
#include <grpcpp/support/status.h>

// STD
#include <memory>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// Abseil
#include <absl/status/status.h>

// protos
#include <protos/client/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/server-message.pb.h>
#include <protos/services/sound-router.grpc.pb.h>

using namespace laar;

namespace {

    NSound::TServiceMessage makeDirectiveResponse(std::uint32_t flag) {
        NSound::TServiceMessage message;

        if (flag & laar::state::DRAINED) {
            message.mutable_basemessage()->mutable_directivesatisfied()->set_directive(
                NSound::NClient::NBase::TBaseMessage::TStreamDirective::DRAIN
            );
        } else if (flag & laar::state::CLOSED) {
            message.mutable_basemessage()->mutable_directivesatisfied()->set_directive(
                NSound::NClient::NBase::TBaseMessage::TStreamDirective::FLUSH
            );
        } else if (flag & laar::state::FLUSHED) {
            message.mutable_basemessage()->mutable_directivesatisfied()->set_directive(
                NSound::NClient::NBase::TBaseMessage::TStreamDirective::CLOSE
            );
        }

        return message;
    }

}

std::shared_ptr<RoutingService> RoutingService::configure(std::weak_ptr<laar::IStreamHandler> soundHandler) {
    return std::shared_ptr<RoutingService>(new RoutingService(std::move(soundHandler)));
}

grpc::Status RoutingService::RouteStream(
    grpc::ServerContext* context, 
    grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
) {
    std::shared_ptr<Session> session = laar::Session::find(context->peer(), sessions_);

    NSound::TClientMessage message;
    laar::Session::TAPIResult APIResult;

    if (!session) {
        // new client
        session = laar::Session::make(context->peer());
        sessions_[context->peer()] = session;  

        if (auto status = session->init(soundHandler_); !status.ok()) {
            onCriticalError(status, context->peer());
            return grpc::Status::CANCELLED;
        }
    }

    // serve one message at a time
    while (stream->Read(&message)) {
        if (message.has_basemessage()) {
            // base message set
            TBaseMessage baseMessage = std::move(*message.mutable_basemessage());
            if (baseMessage.has_streamconfiguration()) {
                APIResult = session->onStreamConfiguration(std::move(*baseMessage.mutable_streamconfiguration()));
                if (absl::Status result = session->init(soundHandler_); !result.ok()) {
                    onCriticalError(result, context->peer());
                    return grpc::Status::CANCELLED;
                }
            } else if (baseMessage.has_directive()) {
                switch (baseMessage.directive().type()) {
                    case TBaseMessage::TStreamDirective::DRAIN:
                        APIResult = session->onDrain(std::move(*baseMessage.mutable_directive()));
                        break;
                    case TBaseMessage::TStreamDirective::CLOSE:
                        APIResult = session->onClose(std::move(*baseMessage.mutable_directive()));
                        break;
                    case TBaseMessage::TStreamDirective::FLUSH:
                        APIResult = session->onFlush(std::move(*baseMessage.mutable_directive()));
                        break;
                    default:
                        return grpc::Status::CANCELLED;
                }
            } else if (baseMessage.has_push()) {
                APIResult = session->onIOOperation(std::move(*baseMessage.mutable_push()));
            } else if (baseMessage.has_pull()) {
                APIResult = session->onIOOperation(std::move(*baseMessage.mutable_pull()));
            }
        }

        if (APIResult.first.ok()) {
            if (APIResult.second.has_value()) {
                stream->Write(APIResult.second.value());
            }
        } else {
            onCriticalError(APIResult.first, context->peer());
            return grpc::Status::CANCELLED;
        }

        // if handle is dead leave
        if (session->isAlive()) {
            PLOG(plog::info) << "session " << session.get() << " is dead, removing it from sessions";
            sessions_.extract(sessions_.find(context->peer()));
        }
    }

    // check for updates on session
    auto state = session->state();
    if (state & laar::state::DRAINED) {
        stream->Write(makeDirectiveResponse(laar::state::DRAINED));
    } if (state & laar::state::FLUSHED) {
        stream->Write(makeDirectiveResponse(laar::state::FLUSHED));
    } if (state & laar::state::CLOSED) {
        stream->Write(makeDirectiveResponse(laar::state::CLOSED));
    }

    return grpc::Status::OK;
}

RoutingService::RoutingService(
    std::weak_ptr<laar::IStreamHandler> soundHandler
)
    : soundHandler_(soundHandler)
{}

void RoutingService::onRecoverableError(absl::Status error, absl::string_view session) {
    // do nothing
}

void RoutingService::onCriticalError(absl::Status error, absl::string_view session) {
    PLOG(plog::error) << "error on peer: " << session << "; message: " << error.ToString();
    if (auto iter = sessions_.find(session.data()); iter != sessions_.end()) {
        auto node = sessions_.extract(iter);
        return;
    }
    std::abort();
}