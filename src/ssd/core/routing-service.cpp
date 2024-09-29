// protos
#include "src/ssd/core/routing-service.hpp"
#include "grpcpp/support/status.h"
#include "protos/client/base.pb.h"
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <absl/status/status.h>
#include <memory>
#include <protos/services/sound-router.grpc.pb.h>

// STD

using namespace laar;

std::shared_ptr<RoutingService> RoutingService::configure(std::weak_ptr<laar::IStreamHandler> soundHandler) {
    return std::shared_ptr<RoutingService>(new RoutingService(std::move(soundHandler)));
}

grpc::Status RoutingService::RouteStream(
    grpc::ServerContext* context, 
    grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
) {
    return grpc::Status::OK;
}

RoutingService::RoutingService(
    std::weak_ptr<laar::IStreamHandler> soundHandler
)
    : soundHandler_(soundHandler)
{}

void RoutingService::onRecoverableError(absl::Status error, std::shared_ptr<laar::Session> session) {
    
}

void RoutingService::onCriticalError(absl::Status error, std::shared_ptr<laar::Session> session) {

}