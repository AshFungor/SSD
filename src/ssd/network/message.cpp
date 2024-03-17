// standard
#include <cstdlib>
#include <cstring>

// laar
#include <common/exceptions.hpp>

// proto
#include <memory>
#include <protos/client/client-message.pb.h>

// tiny-fsm
#include <tinyfsm.hpp>

// local
#include "message.hpp"

using namespace laar;


FSM_INITIAL_STATE(laar::Message, laar::SizeChunk);

void Message::entry() {}
void Message::exit() {}
void Message::react(const tinyfsm::Event& event) {}
void Message::react(const laar::PartialReceive& event) {}

std::size_t Message::wantMore() { return shared.needs - shared.received; }
NSound::TClientMessage Message::get() { return shared.message; }

void SizeChunk::react(const laar::PartialReceive& event) {
    if (shared.received + event.size > shared.needs) {
        throw laar::LaarOverrun();
    }

    std::memcpy(&shared.size + shared.received, event.buffer, event.size);
    shared.received += event.size;

    if (shared.received == shared.needs) {
        transit<DataChunk>();
    }
}

void SizeChunk::entry() {
    shared.received = shared.size = 0;
    shared.needs = sizeof shared.size;
}

void DataChunk::react(const laar::PartialReceive& event) {
    if (shared.received + event.size > shared.needs) {
        throw laar::LaarOverrun();
    }

    memcpy(shared.buffer.get() + shared.received, event.buffer, event.size);
    shared.received += event.size;

    if (shared.received == shared.needs) {
        transit<EndChunk>();
    }
}

void DataChunk::entry() {
    shared.received = 0;
    shared.needs = shared.size;
    shared.buffer = std::make_unique<char[]>(shared.size);
}

void EndChunk::react(const laar::PartialReceive& event) {
    throw laar::LaarOverrun();
}

void EndChunk::entry() {
    shared.received = shared.needs = 0;
    shared.message.ParseFromArray(shared.buffer.get(), shared.size);
}