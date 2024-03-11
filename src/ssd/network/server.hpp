// sockpp
#include <sockpp/tcp_acceptor.h>
#include <sockpp/inet_address.h>

// json
#include <nlohmann/json_fwd.hpp>

// standard
#include <condition_variable>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

// proto
#include <protos/client/client-message.pb.h>

// laar
#include <common/thread-pool.hpp>
#include <common/callback-queue.hpp>
#include <util/config-loader.hpp>

// local
#include "session.hpp"

namespace srv {

    class Server 
    : std::enable_shared_from_this<Server>
    {
    private: struct Private { };
    public:
        static std::shared_ptr<Server> configure(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool,
            std::shared_ptr<laar::ConfigHandler> configHandler
        );
        Server(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool,
            std::shared_ptr<laar::ConfigHandler> configHandler,
            Private access
        );
        ~Server();

        void init();
        void run();

    private:
        void parseDefaultConfig(const nlohmann::json& config);
        void parseDynamicConfig(const nlohmann::json& config); 
        // Event Handlers
        void onNewEndpointConnected();

    private:
        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        std::shared_ptr<laar::ThreadPool> threadPool_;
        std::shared_ptr<laar::ConfigHandler> configHandler_;

        struct Settings {
            bool init = false;
            sockpp::inet_address address;
        } settings_;

        std::mutex settingsLock_;
        std::condition_variable cv_;

        sockpp::tcp_acceptor acc_;
        std::vector<std::shared_ptr<ClientSession>> sessions_;
    };

}