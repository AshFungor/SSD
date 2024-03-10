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
void Message::react(const tinyfsm::Event& event) {}
void Message::react(const laar::PartialReceive& event) {}

std::size_t Message::wantMore() { return needs - received; }
NSound::TClientMessage Message::get() { return message; }

void SizeChunk::react(const laar::PartialReceive& event) {
    if (received + event.size > needs) {
        throw laar::LaarOverrun();
    }

    memcpy((char*) size + received, event.buffer, event.size);
    received += event.size;

    if (received == needs) {
        transit<DataChunk>();
    }
}

void SizeChunk::entry() {
    received = size = 0;
    needs = sizeof size;
}

void DataChunk::react(const laar::PartialReceive& event) {
    if (received + event.size > needs) {
        throw laar::LaarOverrun();
    }

    memcpy(buffer.get() + received, event.buffer, event.size);
    received += event.size;

    if (received == needs) {
        transit<EndChunk>();
    }
}

void DataChunk::entry() {
    received = 0;
    needs = size;
    buffer = std::make_unique<char[]>(size);
}

void EndChunk::react(const laar::PartialReceive& event) {
    throw laar::LaarOverrun();
}

void EndChunk::entry() {
    received = needs = 0;
    message.ParseFromArray(buffer.get(), size);
}