// sockpp
#include "protos/client/simple/simple.pb.h"
#include <memory>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/tcp_socket.h>

// standard
#include <array>
#include <atomic>

// plog
#include <plog/Log.h>

// protos
#include <protos/client/client-message.pb.h>

namespace srv {

    using byte = std::uint8_t;

    class ClientSession 
    : std::enable_shared_from_this<ClientSession>
    {
    private: struct Private { };
    public:
        static std::shared_ptr<ClientSession> instance(sockpp::tcp_socket&& sock);
        ClientSession(sockpp::tcp_socket&& sock, Private access);
        ~ClientSession();

        void terminate();
        bool updating();
        void update();
        bool open();
        void init();

    private:
        void onStreamConfigMessage(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message);
        void onClientMessage(const NSound::TClientMessage& message);
        void error(const std::string& errorMessage);
        void handleErrorState(sockpp::result<std::size_t> requestState);
        void reply(/* server message */);

    private:
        bool isMessageBeingReceived_;
        std::atomic_bool isBeingUpdated_;

        std::array<char, 1024> buffer_;
        NSound::NSimple::TSimpleMessage::TStreamConfiguration sessionConfig_;
        sockpp::tcp_socket sock_;
    };

}