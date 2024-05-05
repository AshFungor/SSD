// sockpp
#include "network/interfaces/i-message.hpp"
#include "network/sync-protocol.hpp"
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
#include <network/interfaces/i-protocol.hpp>

// protos
#include <protos/client/simple/simple.pb.h>
#include <protos/client/client-message.pb.h>

namespace srv {

    using byte = std::uint8_t;

    class ClientSession 
    : public std::enable_shared_from_this<ClientSession>
    , public laar::IProtocol::IReplyListener
    {
    private: struct Private { };
    public:

        static std::shared_ptr<ClientSession> instance(
            sockpp::tcp_socket&& sock,
            std::shared_ptr<laar::IStreamHandler> soundHandler
        );

        ClientSession(
            sockpp::tcp_socket&& sock, 
            std::shared_ptr<laar::IStreamHandler> soundHandler, 
            Private access
        );
        
        ~ClientSession();

        void terminate();
        bool update();
        bool open();
        void init();

        // IProtocol::IReplyListener
        virtual void onReply(std::unique_ptr<char[]> buffer, std::size_t size) override;
        virtual void onClose() override;

    private:
        void error(const std::string& errorMessage);
        void handleErrorState(sockpp::result<std::size_t> requestState);

    private:

        std::mutex sessionLock_;

        std::shared_ptr<laar::PlainBuffer> buffer_;
        std::shared_ptr<laar::IMessageReceiver> builder_;
        std::shared_ptr<laar::SyncProtocol> protocol_;

        sockpp::tcp_socket sock_;
    };

}