// zeromq
#include <sockpp/tcp_acceptor.h>

// standard
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

// proto
#include <protos/client/client-message.pb.h>

// local
#include "session.hpp"

namespace srv {

    using byte = std::uint8_t;

    class Server {
    public:
        // Singleton
        static Server& getInstance();
        Server(const Server&) = delete;
        Server(const Server&&) = delete;
        ~Server();

        void init();
        void run();

    private:
        // Singleton
        Server();

        // Event Handlers
        void onNewEndpointConnected();

    private:
        // Singleton
        inline static std::unique_ptr<Server> instance_;

        sockpp::tcp_acceptor acc_;
        std::vector<ClientSession> sessions_;
    };

}