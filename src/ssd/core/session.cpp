// STD
#include "protos/client/base.pb.h"
// #include "src/ssd/sound/interfaces/i-audio-handler.hpp"
#include <absl/status/status.h>
#include <memory>
#include <unordered_map>
#include <src/ssd/core/session.hpp>

using namespace laar;

std::shared_ptr<Session> Session::find(
    std::string client, 
    std::unordered_map<std::string, std::shared_ptr<Session>>& sessions
) {
    auto iter = sessions.find(client);
    if (iter != sessions.end()) {
        return iter->second;
    }
    return nullptr;
}

