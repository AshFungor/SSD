// sockpp
#include <atomic>
#include <mutex>
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
#include <chrono>

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
        : public std::enable_shared_from_this<Server>
    {
    private: struct Private { };
    public:
        static std::shared_ptr<Server> configure(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool,
            std::shared_ptr<laar::ConfigHandler> configHandler,
            std::shared_ptr<laar::IStreamHandler> soundHandler
        );
        Server(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool,
            std::shared_ptr<laar::ConfigHandler> configHandler,
            std::shared_ptr<laar::IStreamHandler> soundHandler,
            Private access
        );
        ~Server();

        void init();
        void run();
        void terminate();

    private:
        void subscribeOnConfigs(); 
        void parseDefaultConfig(const nlohmann::json& config);
        void parseDynamicConfig(const nlohmann::json& config); 
        // Event Handlers
        void onNewEndpointConnected();

    private:
        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        std::shared_ptr<laar::ThreadPool> threadPool_;
        std::shared_ptr<laar::ConfigHandler> configHandler_;
        std::shared_ptr<laar::IStreamHandler> soundHandler_;

        struct Settings {
            bool init = false;
            sockpp::inet_address address;
            std::vector<std::chrono::milliseconds> timeouts;
        } settings_;

        std::atomic<bool> abort_;
        std::atomic<bool> hasWork_;
        std::vector<std::chrono::milliseconds>::const_iterator currentTimeout_;

        std::mutex serverLock_; 
        std::mutex settingsLock_;
        std::condition_variable cv_;

        sockpp::tcp_acceptor acc_;
        std::vector<std::shared_ptr<ClientSession>> sessions_;
    };

}