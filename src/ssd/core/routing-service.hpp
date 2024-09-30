#pragma once

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

// Abseil
#include <absl/status/status.h>

// protos
#include <memory>

namespace laar {

    class RoutingService 
        : public NSound::NServices::SoundStreamRouter::Service
        , public std::enable_shared_from_this<RoutingService> {
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;

        static std::shared_ptr<RoutingService> configure(std::weak_ptr<laar::IStreamHandler> soundHandler);

        // SoundStreamRouter implementation
        virtual grpc::Status RouteStream(
            grpc::ServerContext* context, 
            grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
        ) override;

    private:

        RoutingService(
            std::weak_ptr<laar::IStreamHandler> soundHandler
        );

        // error fallback
        void onRecoverableError(absl::Status error, absl::string_view session);
        void onCriticalError(absl::Status error, absl::string_view session);

    private:
        std::weak_ptr<laar::IStreamHandler> soundHandler_;
        std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

    };

}