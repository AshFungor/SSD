#pragma once

// protos
#include "protos/client/base.pb.h"
#include "src/ssd/core/session.hpp"
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <absl/status/status.h>
#include <memory>
#include <protos/services/sound-router.grpc.pb.h>

// STD


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
        void onRecoverableError(absl::Status error, std::shared_ptr<laar::Session> session);
        void onCriticalError(absl::Status error, std::shared_ptr<laar::Session> session);

    private:
        std::weak_ptr<laar::IStreamHandler> soundHandler_;
        std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

    };

}