// sockpp
#include "network/interfaces/i-message.hpp"
#include <sockpp/tcp_acceptor.h>
#include <sockpp/tcp_socket.h>

// standard
#include <memory>
#include <atomic>

// plog
#include <plog/Log.h>

// laar
#include <network/message.hpp>
#include <common/ring-buffer.hpp>

// protos
#include <protos/client/simple/simple.pb.h>
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
        bool update();
        bool open();
        void init();

    private:
        void onStreamConfigMessage(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message);
        void onClientMessage(const NSound::TClientMessage& message);
        void error(const std::string& errorMessage);
        void handleErrorState(sockpp::result<std::size_t> requestState);
        void reply(/* server message */);

    private:

        std::mutex sessionLock_;

        std::shared_ptr<laar::RingBuffer> buffer_;
        std::shared_ptr<laar::IMessageBuilder> builder_;

        NSound::NSimple::TSimpleMessage::TStreamConfiguration sessionConfig_;
        sockpp::tcp_socket sock_;
    };

}