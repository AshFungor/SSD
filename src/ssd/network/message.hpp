// standard
#include <cstddef>
#include <memory>

// proto
#include <protos/client/client-message.pb.h>

// tiny-fsm
#include <tinyfsm.hpp>


namespace laar {

    struct PartialReceive : public tinyfsm::Event {
        PartialReceive(char* buffer, std::size_t size) 
        : buffer(buffer)
        , size(size)
        {} 

        std::size_t size;
        char* buffer;
    };

    class Message : public tinyfsm::Fsm<Message> {
    public:
        void react(const tinyfsm::Event& event);
        virtual void entry();
        void exit();
        virtual void react(const laar::PartialReceive& event);
        static std::size_t wantMore();
        static NSound::TClientMessage get();
    protected:
        struct Shared {
            std::size_t size;
            std::size_t needs;
            std::size_t received;
            std::unique_ptr<char[]> buffer;
            NSound::TClientMessage message;
        } inline static shared {
            .size = 0,
            .needs = 0,
            .received = 0
        };
    };

    class SizeChunk : public Message {
    public:
        void entry() override;
        void react(const laar::PartialReceive& event) override;
    };

    class DataChunk : public Message {
    public:
        void entry() override;
        void react(const laar::PartialReceive& event) override;
    };

    class EndChunk : public Message {
    public:
        void entry() override;
        void react(const laar::PartialReceive& event) override;
    };

}