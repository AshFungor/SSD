#include <iostream>
#include <memory>

// protos
#include <protos/client/simple/simple.pb.h>
#include <protos/client/client-message.pb.h>

// sockpp
#include <sockpp/tcp_connector.h>

int main() {

    std::cout << "starting PCM";

    sockpp::tcp_connector conn;
    int16_t port = 5050;

    if (auto res = conn.connect(sockpp::inet_address("localhost", port))) {
        std::cout << "result: " << res.error_message() << "\n";
    }

    NSound::TClientMessage message;
    *message.mutable_simple_message()->mutable_stream_config() = NSound::NSimple::TSimpleMessage::TStreamConfiguration::default_instance();
    message.mutable_simple_message()->mutable_stream_config()->mutable_buffer_config()->set_fragment_size(12);
    std::size_t len = message.ByteSizeLong();

    std::cout << "sending message with " << len << " bytes\n";

    auto buffer = std::make_unique<char[]>(len);
    message.SerializeToArray(buffer.get(), len);
    conn.write_n((char*) &len, sizeof len);
    conn.write_n(buffer.get(), len);

    conn.close();
    return 0;
}