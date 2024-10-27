// #pragma once

// // STD
// #include <mutex>
// #include <memory>
// #include <optional>

// // Boost
// #include <boost/asio.hpp>
// #include <boost/asio/ip/tcp.hpp>
// #include <boost/asio/executor.hpp>
// #include <boost/asio/execution_context.hpp>

// // Abseil (google common libs)
// #include <absl/status/status.h>
// #include <absl/status/statusor.h>

// // laar
// #include <src/ssd/core/session.hpp>

// // codegen for protos
// #include <protos/service/base.pb.h>
// #include <protos/client/base.pb.h>


// namespace laar {

//     class Server 
//         : public Session::ISessionMaster
//         , public std::enable_shared_from_this<Server> {
//     public:

//         using tcp = boost::asio::ip::tcp;

//         static std::shared_ptr<Server> create(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port);
//         void init();
        
//         // Session::ISessionMaster implementation
//         virtual void abort(std::weak_ptr<Session> slave, std::optional<std::string> reason) override;
//         virtual void close(std::weak_ptr<Session> slave) override;

//     private:

//         Server() = delete;
//         Server(const Server&) = delete;
//         Server(Server&&) = delete;
//         Server& operator=(const Server&) = delete;
//         Server& operator=(Server&&) = delete;

//         Server(std::weak_ptr<IStreamHandler> handler, std::shared_ptr<boost::asio::io_context> context, std::uint32_t port);

//         void accept(std::shared_ptr<Session> session, const boost::system::error_code& error);
//         void onNetworkError(const boost::system::error_code& error, bool isCritical);

//     private:

//         std::once_flag init_;

//         SessionFactory factory_;
//         std::vector<std::shared_ptr<Session>> sessions_;

//         tcp::acceptor acceptor_;
//         std::shared_ptr<boost::asio::io_context> context_;
//         std::weak_ptr<IStreamHandler> handler_;

//     };

// }