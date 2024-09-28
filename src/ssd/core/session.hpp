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
        : std::enable_shared_from_this<Session> {
    private: struct Private {};
    public:

        using TBaseMessage = NSound::NClient::NBase::TBaseMessage;

        static std::shared_ptr<Session> find(
            std::string client, 
            std::unordered_map<std::string, std::shared_ptr<Session>>& sessions
        );

        static std::shared_ptr<Session> make(absl::string_view client);
        Session(absl::string_view client, Private access);

        // Read/write IO handle
        absl::Status acquireHandle(std::weak_ptr<laar::IStreamHandler> handler);
        std::weak_ptr<laar::IStreamHandler::IHandle> getHandle() noexcept;

        // setters
        absl::Status setConfig(TBaseMessage::TStreamConfiguration configuration);

        // getters
        std::string getStreamName() const noexcept;
        std::string getClientName() const noexcept;
        TBaseMessage::TStreamConfiguration::TStreamDirection getDirection() const noexcept;

    private:
        // empty

    private:
        std::string client_;
        std::weak_ptr<IStreamHandler::IHandle> handle_;
        
        TBaseMessage::TStreamConfiguration streamConfig_;

    };

}