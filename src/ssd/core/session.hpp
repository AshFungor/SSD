#pragma once

// laar
#include <src/ssd/core/session.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// grpc
#include <grpcpp/support/status.h>

// protos
#include <protos/client/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/services/sound-router.grpc.pb.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <memory>
#include <cstdint>

namespace laar {

    class Session 
        : public laar::IStreamHandler::IHandle::IListener
        , std::enable_shared_from_this<Session> {
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;
        using TAPIResult = std::pair<absl::Status, std::optional<NSound::TServiceMessage>>;

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

        bool isAlive();

        ~Session();

    private:
        
        enum class EState : std::uint32_t {
            NORMAL = 0x1, DRAINED = 0x2, FLUSHED = 0x3, CLOSED = 0x4 
        };

        Session(absl::string_view client);

        std::uint32_t emptyState();
        void updateState(EState flag);

    private:


        std::uint32_t state_;
        std::once_flag init_;

        std::string client_;
        std::shared_ptr<IStreamHandler::IHandle> handle_;
        
        std::optional<TBaseMessage::TStreamConfiguration> streamConfig_;

    };

}