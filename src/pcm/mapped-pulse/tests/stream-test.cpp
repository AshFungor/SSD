// pulse
#include <pulse/def.h>
#include <pulse/stream.h>
#include <pulse/context.h>
#include <pulse/xmalloc.h>
#include <pulse/context.h>
#include <pulse/mainloop.h>
#include <pulse/operation.h>
#include <pulse/channelmap.h>
#include <pulse/mainloop-api.h>

// protos
#include <protos/holder.pb.h>
#include <protos/server-message.pb.h>
#include <protos/client-message.pb.h>
#include <protos/client/context.pb.h>
#include <protos/common/stream-configuration.pb.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>
#include <src/pcm/mapped-pulse/context/common.hpp>

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// gtest
#include <gtest/gtest.h>

// STD
#include <vector>
#include <chrono>
#include <cerrno>
#include <memory>
#include <thread>
#include <cstdint>
#include <cstring>

// abseil
#include <absl/strings/str_format.h>

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

#define SYSCALL_FALLBACK()                                                      \
    pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);               \
    ENSURE_FAIL()

using namespace std::chrono;

namespace {

    std::uint32_t id;
    NSound::NCommon::TStreamConfiguration commonConfig;

    enum EMessage {
        ACK, TRAIL, STREAM_OPEN_CONFIRMAL
    };

    struct Socket {
        int cfd;
        sockaddr_in addr;
        unsigned int addrlen;
    };

    laar::Message getMessage(EMessage type, std::shared_ptr<laar::MessageFactory> factory) {
        NSound::THolder holder;
        switch (type) {
            case ACK:
                return factory->withType(laar::message::type::SIMPLE)
                    .withPayload(laar::ACK)
                    .construct()
                    .constructed();
            case TRAIL:
                return factory->withType(laar::message::type::SIMPLE)
                    .withPayload(laar::TRAIL)
                    .construct()
                    .constructed();
            case STREAM_OPEN_CONFIRMAL:
                holder.mutable_server()->mutable_stream_message()->mutable_connect_confirmal()->set_opened(true);
                holder.mutable_server()->mutable_stream_message()->mutable_connect_confirmal()->mutable_configuration()->CopyFrom(commonConfig);
                holder.mutable_server()->mutable_stream_message()->set_stream_id(id);
                return factory->withType(laar::message::type::PROTOBUF)
                    .withPayload(std::move(holder))
                    .construct()
                    .constructed();
        }

        ENSURE_FAIL();
    }

    std::string formatPretty(EMessage type) {
        switch (type) {
            case ACK:
                return absl::StrFormat("sending ACK");
            case TRAIL:
                return absl::StrFormat("sending TRAIL");
            case STREAM_OPEN_CONFIRMAL:
                return absl::StrFormat("sending STREAM_OPEN_CONFIRMAL");
        }

        ENSURE_FAIL();
    }

    void writeBack(
        Socket socket, 
        int& received, 
        std::shared_ptr<laar::MessageFactory> factory, 
        std::unique_ptr<std::uint8_t[]>& buffer,
        std::vector<EMessage> messages
    ) {

        auto cur = messages.begin();
        laar::Message message;

        while(received--) {
            pcm_log::log(absl::StrFormat("preparing to send message back, received: %d", received), pcm_log::ELogVerbosity::INFO);

            message = getMessage(*cur, factory);
            pcm_log::log(formatPretty(*cur), pcm_log::ELogVerbosity::INFO);
            message.writeToArray(buffer.get(), laar::NetworkBufferSize);

            if (int written = write(socket.cfd, buffer.get(), laar::Message::Size::total(&message)); written < 0) {
                SYSCALL_FALLBACK();
            }

            ++cur;
        }

        received = 0;
    }

    void dispatchIncomingStream(
        Socket socket, 
        int& received, 
        std::shared_ptr<laar::MessageFactory> factory, 
        std::unique_ptr<std::uint8_t[]>& buffer
    ) {
        // intermidiate data for parsing messages
        laar::Message msg;
        std::size_t size = laar::NetworkBufferSize;
        std::size_t offset = 0;
       

        while (true) {
            pcm_log::log(absl::StrFormat("requesting for bytes: %d", factory->next()), pcm_log::ELogVerbosity::INFO);
            if (int acquired = read(socket.cfd, buffer.get() + offset, factory->next()); acquired < static_cast<int>(factory->next())) {
                if (acquired >= 0) {
                    pcm_log::log(
                        absl::StrFormat("requesting for bytes: %d; bytes received: %d", factory->next(), acquired), 
                        pcm_log::ELogVerbosity::INFO
                    );
                }
                SYSCALL_FALLBACK();
            } else {
                factory->parse(buffer.get() + offset, size);
                offset += acquired;
                pcm_log::log(absl::StrFormat("part of message received, size is %d", size), pcm_log::ELogVerbosity::INFO);
            }

            if (factory->isParsedAvailable()) {
                ++received;
                msg = factory->parsed();
                pcm_log::log("message received", pcm_log::ELogVerbosity::INFO);

                if (msg.type() == laar::message::type::PROTOBUF) {
                    auto clientMessage = std::move(*laar::messagePayload<laar::message::type::PROTOBUF>(msg).mutable_client());
                    if (clientMessage.has_stream_message()) {
                        commonConfig = std::move(*clientMessage.mutable_stream_message()->mutable_connect()->mutable_configuration());
                        id = 1;
                    }
                }

                if (msg.type() == laar::message::type::SIMPLE && laar::messagePayload<laar::message::type::SIMPLE>(msg) == laar::TRAIL) {
                    break;
                }
            }

        }

        pcm_log::log("received TRAIL", pcm_log::ELogVerbosity::INFO);
    }

    void dummyStreamConnector(int fd) {

        // prepare single socket data
        Socket socket;

        if (int err = socket.cfd = accept(fd, reinterpret_cast<sockaddr*>(&socket.addr), &socket.addrlen); err < 0) {
            SYSCALL_FALLBACK();
        }

        auto factory = laar::MessageFactory::configure();
        auto buffer = std::make_unique<std::uint8_t[]>(laar::NetworkBufferSize);

        int received = 0;
        // dispatch context connect first
        pcm_log::log("connecting client, accepting context", pcm_log::ELogVerbosity::INFO);
        dispatchIncomingStream(socket, received, factory, buffer);
        writeBack(socket, received, factory, buffer, {ACK, STREAM_OPEN_CONFIRMAL, TRAIL});

        // dispatch some data (one cycle)
        pcm_log::log("dispatching data cycle 1", pcm_log::ELogVerbosity::INFO);
        dispatchIncomingStream(socket, received, factory, buffer);

        std::vector<EMessage> messages;
        for (int i = 0; i < received - 1; ++i) {
            messages.push_back(ACK);
        }
        messages.push_back(TRAIL);
        writeBack(socket, received, factory, buffer, messages);

        // dispatch some data (one cycle)
        pcm_log::log("dispatching data cycle 2", pcm_log::ELogVerbosity::INFO);
        dispatchIncomingStream(socket, received, factory, buffer);

        for (int i = 0; i < received - 1; ++i) {
            messages.push_back(ACK);
        }
        messages.push_back(TRAIL);
        writeBack(socket, received, factory, buffer, messages);

        // dispatch some data (one cycle)
        pcm_log::log("dispatching data cycle 3", pcm_log::ELogVerbosity::INFO);
        dispatchIncomingStream(socket, received, factory, buffer);

        for (int i = 0; i < received - 1; ++i) {
            messages.push_back(ACK);
        }
        messages.push_back(TRAIL);
        writeBack(socket, received, factory, buffer, messages);

        close(socket.cfd);
        close(fd);
    }

    class StreamTest : public ::testing::Test {
    public:

        void SetUp() override {
            m = pa_mainloop_new();
            a = pa_mainloop_get_api(m);

            // setup connection
            if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                SYSCALL_FALLBACK();
            }

            sockaddr_in addr;
            addr.sin_port = laar::Port;
            addr.sin_family = AF_INET;

            int retries = 120;
            int delay = 1;

            if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) < 0) {
                SYSCALL_FALLBACK();
            }

            for (int i = 0; i < retries; ++i) {
                if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                    break;
                }

                if (i == retries - 1) {
                    close(fd);
                    pcm_log::log("failed", pcm_log::ELogVerbosity::WARNING);
                    std::abort();
                }

                pcm_log::log(absl::StrFormat("Bind failed, try: %d", i + 1), pcm_log::ELogVerbosity::WARNING);
                std::this_thread::sleep_for(std::chrono::seconds(delay));
            }

            if (listen(fd, 1) < 0) {
                SYSCALL_FALLBACK();
            }

            server = std::make_unique<std::thread>(dummyStreamConnector, fd);
        }

        void TearDown() override {
            pa_mainloop_free(m);
            if (server->joinable()) {
                server->join();
            }
        }

        // socket
        int fd;
        pa_mainloop* m;
        pa_mainloop_api* a;
        std::unique_ptr<std::thread> server;

    };

}

TEST_F(StreamTest, StreamOpenAndSimpleWrite) {
    pa_context* c = pa_context_new(a, "kek");
    pa_context_connect(c, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    auto watcher = [](pa_stream* s, void* userdata) {
        pa_mainloop_api* a = reinterpret_cast<pa_mainloop_api*>(userdata);

        switch (pa_stream_get_state(s)) {
            case PA_STREAM_CREATING:
                pcm_log::log("stream entered state: CREATING", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_STREAM_UNCONNECTED:
                pcm_log::log("stream entered state: UNCONNECTED", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_STREAM_READY:
                pcm_log::log("stream entered state: READY", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_STREAM_FAILED:
                pcm_log::log("stream entered state: FAILED", pcm_log::ELogVerbosity::INFO);
                pa_stream_unref(s);
                a->quit(a, 0);
                return;
            case PA_STREAM_TERMINATED:
                pcm_log::log("stream entered state: TERMINATED", pcm_log::ELogVerbosity::INFO);
                pa_stream_unref(s);
                a->quit(a, 0);
                return;
        }
    };

    auto writer = [](pa_stream* s, unsigned long size, void* userdata) {
        UNUSED(userdata);
        pcm_log::log(absl::StrFormat("receiving write request for %d bytes", size), pcm_log::ELogVerbosity::INFO);

        std::size_t total = size;
        void* data;
        pa_stream_begin_write(s, &data, &total);
        
        std::memset(data, 0, total);

        pa_stream_write(s, data, total, nullptr, 0, PA_SEEK_RELATIVE);

        // request exit right after write
        pcm_log::log(absl::StrFormat("written %d bytes", total), pcm_log::ELogVerbosity::INFO);
        pa_stream_disconnect(s);
    };

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_S32LE;
    auto attr = std::make_unique<pa_buffer_attr>();
    attr->prebuf = spec->rate;
    auto map = std::make_unique<pa_channel_map>();

    pa_stream* s = pa_stream_new(c, "lol", spec.get(), map.get());
    pa_stream_connect_playback(s, nullptr, attr.get(), PA_STREAM_NOFLAGS, nullptr, nullptr);
    pa_stream_ref(s);

    pa_stream_set_state_callback(s, watcher, a);
    pa_stream_set_write_callback(s, writer, nullptr);

    pa_mainloop_run(m, nullptr);

    pa_stream_ref(s);
    pa_context_unref(c);
}