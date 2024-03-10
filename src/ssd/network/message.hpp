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
        virtual void react(const laar::PartialReceive& event);
        static std::size_t wantMore();
        static NSound::TClientMessage get();
    protected:
        static std::size_t size;
        static std::size_t needs;
        static std::size_t received;
        static std::unique_ptr<char[]> buffer;
        static NSound::TClientMessage message;
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