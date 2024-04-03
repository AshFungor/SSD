// standard
#include <stdexcept>
#include <memory>
#include <cerrno>
#include <mutex>

// protos
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// laar
#include <network/message.hpp>

// msfsm
#include <msfsm.hpp>


namespace srv {

    class IProtocol : msfsm::Fsm<IProtocol> {
        virtual void start() = 0;
        virtual void terminate() = 0;
    };

    class SimpleProtocol : IProtocol {
        SimpleProtocol();

    private:
        NSound::NSimple::TSimpleMessage::TStreamConfiguration streamConfig_;

    };

}