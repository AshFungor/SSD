// sockpp
#include <sockpp/tcp_acceptor.h>
#include <sockpp/tcp_socket.h>

// standard
#include <array>


// plog
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>

namespace srv {

    using byte = std::uint8_t;
    constexpr std::size_t MaxBufferSize = 1024;

    class ClientSession {
    public:
        ClientSession(sockpp::tcp_socket&& sock);
        ~ClientSession();

        void terminate();
        void update();
        void init();

    private:
        void onStreamConfigMessage(const NSound::TClientMessage::TStreamConfiguration& message);
        void onClientMessage(const NSound::TClientMessage& message);
        void error(const std::string& errorMessage);
        void handleErrorState(sockpp::result<std::size_t> requestState);
        void reply(/* server message */);

    private:
        bool isMessageBeingReceived_;
        std::array<char, 1024> buffer_;
        NSound::TClientMessage::TStreamConfiguration sessionConfig_;
        sockpp::tcp_socket sock_;
    };

}