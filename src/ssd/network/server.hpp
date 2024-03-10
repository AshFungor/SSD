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
        Server();
        ~Server();

        void init();
        void run();

    private:
        // Event Handlers
        void onNewEndpointConnected();

    private:
        sockpp::tcp_acceptor acc_;
        std::vector<ClientSession> sessions_;
    };

}