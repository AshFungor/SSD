// pulse
#include <pulse/def.h>
#include <pulse/context.h>
#include <pulse/xmalloc.h>
#include <pulse/context.h>
#include <pulse/operation.h>
#include <pulse/mainloop.h>
#include <pulse/mainloop-api.h>

// protos
#include <protos/holder.pb.h>
#include <protos/client-message.pb.h>
#include <protos/client/context.pb.h>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/pcm/mapped-pulse/context/common.hpp>

#ifdef __linux__
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// gtest
#include <gtest/gtest.h>

// STD
#include <cerrno>
#include <memory>
#include <thread>
#include <cstring>

// abseil
#include <absl/strings/str_format.h>

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

#define SYSCALL_FALLBACK()                                                      \
    pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);               \
    ENSURE_FAIL()

namespace {

    void reflect(int fd) {

        // prepare single socket data
        struct Socket {
            int cfd;
            sockaddr_in addr;
            unsigned int addrlen;
        } socket;

        if (int err = socket.cfd = accept(fd, reinterpret_cast<sockaddr*>(&socket.addr), &socket.addrlen); err < 0) {
            SYSCALL_FALLBACK();
        }

        pcm_log::log("connecting client, preparing to get a stream", pcm_log::ELogVerbosity::INFO);

        // intermidiate data for parsing messages
        laar::Message msg;
        std::size_t received = 0;
        std::size_t size = laar::NetworkBufferSize;
        std::size_t offset = 0;
        auto factory = laar::MessageFactory::configure();
        auto buffer = std::make_unique<std::uint8_t[]>(laar::NetworkBufferSize);

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

                if (msg.type() == laar::message::type::SIMPLE && laar::messagePayload<laar::message::type::SIMPLE>(msg) == laar::TRAIL) {
                    break;
                }
            }

        }
        pcm_log::log("received TRAIL, sending acks back", pcm_log::ELogVerbosity::INFO);

        // we write only single message, so
        auto message = factory->withType(laar::message::type::SIMPLE)
            .withPayload(laar::ACK)
            .construct()
            .constructed();
        message.writeToArray(buffer.get(), laar::NetworkBufferSize);

        while(received--) {

            if (!received) {
                message = factory->withType(laar::message::type::SIMPLE)
                    .withPayload(laar::TRAIL)
                    .construct()
                    .constructed();
                message.writeToArray(buffer.get(), laar::NetworkBufferSize);
                pcm_log::log("sending TRAIL", pcm_log::ELogVerbosity::INFO);
            } else {
                pcm_log::log("sending ACK", pcm_log::ELogVerbosity::INFO);
            }

            if (int written = write(socket.cfd, buffer.get(), laar::Message::Size::total(&message)); written < 0) {
                SYSCALL_FALLBACK();
            }
        }

        close(socket.cfd);
        close(fd);
    }

    class ContextTest : public ::testing::Test {
    public:

        void SetUp() override {
            m = pa_mainloop_new();
            a = pa_mainloop_get_api(m);

            // setup connection
            if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                SYSCALL_FALLBACK();
            }

            sockaddr_in addr;
            addr.sin_port = htons(laar::Port);
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

            server = std::make_unique<std::thread>(reflect, fd);
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

TEST_F(ContextTest, TestNormalSetup) {
    pa_context* c = pa_context_new(a, "kek");

    auto watcher = [](pa_context* c, void* userdata) {
        UNUSED(userdata);

        switch (pa_context_get_state(c)) {
            case PA_CONTEXT_UNCONNECTED:
                pcm_log::log("context entered state: UNCONNECTED", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_AUTHORIZING:
                pcm_log::log("context entered state: AUTHORIZING", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_CONNECTING:
                pcm_log::log("context entered state: CONNECTING", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_FAILED:
                pcm_log::log("context entered state: FAILED", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_SETTING_NAME:
                pcm_log::log("context entered state: SETTING NAME", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_READY:
                pcm_log::log("context entered state: READY", pcm_log::ELogVerbosity::INFO);
                return;
            case PA_CONTEXT_TERMINATED:
                pcm_log::log("context entered state: TERMINATED", pcm_log::ELogVerbosity::INFO);
                return;
        }
    };

    auto killer = [](pa_context* c, void* userdata) {
        UNUSED(c);

        pcm_log::log("Drain complete, no more work on context", pcm_log::ELogVerbosity::INFO);
        pa_mainloop_api* a = reinterpret_cast<pa_mainloop_api*>(userdata);
        a->quit(a, 0);
    };

    pa_context_set_state_callback(c, watcher, a);
    pa_context_connect(c, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    pa_context_drain(c, killer, a);

    pa_mainloop_run(m, nullptr);

    pa_context_unref(c);
}