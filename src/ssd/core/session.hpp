#pragma once

// laar
#include "protos/service/base.pb.h"
#include <queue>
#include <src/ssd/core/session.hpp>
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

    class Session 
        : public laar::IStreamHandler::IHandle::IListener
        , std::enable_shared_from_this<Session> {
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;
        using TAPIResult = std::pair<absl::Status, NSound::TServiceMessage>;

        static std::shared_ptr<Session> find(
            std::string client, 
            std::unordered_map<std::string, std::shared_ptr<Session>>& sessions
        );

        static std::shared_ptr<Session> make(absl::string_view client);

        absl::Status init(std::weak_ptr<IStreamHandler> soundHandler); 

        // Data routing
        TAPIResult onIOOperation(TBaseMessage::TPull message);
        TAPIResult onIOOperation(TBaseMessage::TPush message);
        TAPIResult onStreamConfiguration(TBaseMessage::TStreamConfiguration message);

        // Manipulation
        TAPIResult onDrain(TBaseMessage::TStreamDirective message);
        TAPIResult onFlush(TBaseMessage::TStreamDirective message);
        TAPIResult onClose(TBaseMessage::TStreamDirective message);

        // IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

    private:

        Session(absl::string_view client);

    private:

        enum class EState {
            NORMAL, DRAINED, FLUSHED, CLOSED 
        } state_;

        std::once_flag init_;

        std::string client_;
        std::shared_ptr<IStreamHandler::IHandle> handle_;
        
        std::optional<TBaseMessage::TStreamConfiguration> streamConfig_;

    };

}