// laar
#include <src/ssd/core/session.hpp>
#include <src/ssd/core/routing-service.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// grpc
#include <grpcpp/support/status.h>

// STD
#include <protos/client/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/services/sound-router.grpc.pb.h>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// Abseil
#include <absl/status/status.h>

// protos
#include <memory>

using namespace laar;

std::shared_ptr<RoutingService> RoutingService::configure(std::weak_ptr<laar::IStreamHandler> soundHandler) {
    return std::shared_ptr<RoutingService>(new RoutingService(std::move(soundHandler)));
}

grpc::Status RoutingService::RouteStream(
    grpc::ServerContext* context, 
    grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
) {
    std::shared_ptr<Session> session = laar::Session::find(context->peer(), sessions_);

    // serve one message at a time
    NSound::TClientMessage message;
    laar::Session::TAPIResult APIResult;
    stream->Read(&message);

    if (!session) {
        // new client
        session = laar::Session::make(context->peer());       
    }

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
        stream->Write(APIResult.second);
        return grpc::Status::OK;
    }

    onCriticalError(APIResult.first, context->peer());
    return grpc::Status::CANCELLED;
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
    PLOG(plog::error) << "error on peer: " << session << "; message: " << error.message();
    if (auto iter = sessions_.find(session.data()); iter != sessions_.end()) {
        sessions_.extract(iter);
        return;
    }
    std::abort();
}