#include <iostream>
#include <memory>

// protos
#include <protos/client/client-message.pb.h>

// sockpp
#include <sockpp/tcp_connector.h>

int main() {

    std::cout << "starting PCM";

    sockpp::tcp_connector conn;
    int16_t port = 5050;

    if (auto res = conn.connect(sockpp::inet_address("localhost", port)))
        std::cout << "error: " << res.error_message();

    NSound::TClientMessage message = NSound::TClientMessage::default_instance();
    *message.mutable_stream_config() = NSound::TClientMessage::TStreamConfiguration::default_instance();
    std::size_t len = message.ByteSizeLong();

    auto buffer = std::make_unique<char[]>(len);
    message.SerializeToArray(buffer.get(), len);
    conn.write_n((char*) &len, sizeof len);
    conn.write_n(buffer.get(), len);

    conn.close();
    return 0;
}