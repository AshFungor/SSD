#pragma once

// protos
#include "protos/client/base.pb.h"
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <absl/status/status.h>
#include <memory>
#include <protos/services/sound-router.grpc.pb.h>

// STD


namespace laar {

    class RoutingService 
        : public NSound::NServices::SoundStreamRouter::Service
        , public laar::IStreamHandler::IHandle::IListener
        , public std::enable_shared_from_this<RoutingService> {
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;

        RoutingService(
            std::weak_ptr<laar::IStreamHandler> soundHandler
        );

        // SoundStreamRouter implementation
        virtual grpc::Status RouteStream(
            grpc::ServerContext* context, 
            grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
        ) override;

        // IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

    private:

        // Data routing
        void onIOOperation(TBaseMessage::TPull message);
        void onIOOperation(TBaseMessage::TPush message);
        void onStreamConfiguration(TBaseMessage::TStreamConfiguration message);
        // Manipulation
        void onDrain(TBaseMessage::TStreamDirective message);
        void onFlush(TBaseMessage::TStreamDirective message);
        void onClose(TBaseMessage::TStreamDirective message);
        // error fallback
        void onRecoverableError(absl::Status error);
        void onCriticalError(absl::Status error);

    };

}