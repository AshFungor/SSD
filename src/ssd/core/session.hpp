#pragma once

// STD
#include "protos/client/base.pb.h"
#include "src/ssd/sound/interfaces/i-audio-handler.hpp"
#include <absl/status/status.h>
#include <absl/strings/string_view.h>
#include <memory>
#include <unordered_map>

namespace laar {

    class Session 
        : public laar::IStreamHandler::IHandle::IListener
        , std::enable_shared_from_this<Session> {
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;

        static std::shared_ptr<Session> find(
            std::string client, 
            std::unordered_map<std::string, std::shared_ptr<Session>>& sessions
        );

        static std::shared_ptr<Session> make(absl::string_view client);

        absl::Status init(std::weak_ptr<IStreamHandler> soundHandler); 

        // Data routing
        absl::Status onIOOperation(TBaseMessage::TPull message);
        absl::Status onIOOperation(TBaseMessage::TPush message);
        absl::Status onStreamConfiguration(TBaseMessage::TStreamConfiguration message);

        // Manipulation
        absl::Status onDrain(TBaseMessage::TStreamDirective message);
        absl::Status onFlush(TBaseMessage::TStreamDirective message);
        absl::Status onClose(TBaseMessage::TStreamDirective message);

        // IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

    private:

        Session(absl::string_view client);

    private:
        std::string client_;
        std::weak_ptr<IStreamHandler::IHandle> handle_;
        
        TBaseMessage::TStreamConfiguration streamConfig_;

    };

}