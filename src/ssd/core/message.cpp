// STD
#include <memory>
#include <cstddef>
#include <cstdint>

// Plog
#include <plog/Log.h>
#include <plog/Severity.h>

// Abseil
#include <absl/base/internal/endian.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <variant>

using namespace laar;

namespace {

    // masks
    constexpr std::uint8_t VERSION_MASK     = 0x0F;
    constexpr std::uint8_t TYPE_MASK        = 0xF0;
    // shifts
    constexpr std::uint8_t VERSION_SHIFT    = 0;
    constexpr std::uint8_t TYPE_SHIFT       = 4;

    template<typename Integer>
    void readDataFromArray(Integer& integer, void* data) {
        std::memcpy(&integer, data, sizeof(integer));
        if constexpr (sizeof(integer) <= sizeof(std::uint16_t)) {
            absl::gntohs(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint32_t)) {
            absl::gntohl(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint64_t)) {
            absl::gntohll(integer);
        } else {
            static_assert(false, "failed to find suitable conversion function");
        }
    }

    template<typename Integer>
    void writeDataToArray(Integer integer, void* data) {
        if constexpr (sizeof(integer) <= sizeof(std::uint16_t)) {
            absl::ghtons(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint32_t)) {
            absl::ghtonl(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint64_t)) {
            absl::ghtonll(integer);
        } else {
            static_assert(false, "failed to find suitable conversion function");
        }
        std::memcpy(data, &integer, sizeof(integer));
    }

    MessageProtobufPayloadType readProtobufMessage(void* data, std::size_t size) {
        MessageProtobufPayloadType message;
        if (!message.ParseFromArray(data, size)) {
            ENSURE_FAIL();
        }
        return message;
    }

    MessageSimplePayloadType readSimpleMessage(void* data, std::size_t size) {
        UNUSED(size);

        MessageSimplePayloadType message;
        readDataFromArray(message, data);
        return message;
    }

    void writeProtobufMessage(void* data, std::size_t size, MessageProtobufPayloadType message) {
        if (!message.SerializeToArray(data, size)) {
            ENSURE_FAIL();
        }
    }

    void writeSimpleMessage(void* data, std::size_t size, MessageSimplePayloadType message) {
        UNUSED(size);

        writeDataToArray(message, data);
    }

    Message popQueue(std::queue<Message>& source) {
        Message parsed = std::move(source.front());
        source.pop();
        return parsed;
    }

}

// --- Message ---
void Message::readFromArray(void* data, std::size_t maxBytes) {
    std::size_t shift = 0;
    auto array = reinterpret_cast<std::uint8_t*>(data);

    readHeaderFromArray(array + shift, maxBytes - shift);
    shift += Message::Size::header();
    readVariableFromArray(array + shift, maxBytes - shift);
    shift += Message::Size::variable(this);
    readPayloadFromArray(array + shift, maxBytes - shift);
}

void Message::readHeaderFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::header(), size);
#else
    UNUSED(size);
#endif

    readDataFromArray(header_, data);

#ifdef SSD_RUNTIME_CHECKS
    // unimplemented fallback
    switch (version()) {
        case message::version::FIRST:
            goto type;
        default:
            ENSURE_FAIL();
    }

    type:
    switch(type()) {
        case message::type::PROTOBUF:
            goto end;
        case message::type::SIMPLE:
            goto end;
        default:
            ENSURE_FAIL();
    }

    end:
    return;
#endif
}

void Message::readVariableFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::variable(this), size);
#else
    UNUSED(size);
#endif

    switch (type()) {
        case message::type::PROTOBUF:
            readDataFromArray(size_, data);
            return;
        case message::type::SIMPLE:
            size_ = sizeof(MessageSimplePayloadType);
            return;
    }

    ENSURE_FAIL();
}

void Message::readPayloadFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::payload(this), size);
#else
    UNUSED(size);
#endif

    switch(type()) {
        case message::type::PROTOBUF:
            payload_ = readProtobufMessage(data, size_);
            return;
        case message::type::SIMPLE:
            payload_ = readSimpleMessage(data, size_);
            return;
    }

    ENSURE_FAIL();
}

void Message::writeToArray(void* data, std::size_t maxBytes) const {
    std::size_t shift = 0;
    auto array = reinterpret_cast<std::uint8_t*>(data);

    writeHeaderToArray(array + shift, maxBytes - shift);
    shift += Message::Size::header();
    writeVariableToArray(array + shift, maxBytes - shift);
    shift += Message::Size::variable(this);
    writePayloadToArray(array + shift, maxBytes - shift);
}

void Message::writeHeaderToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::header(), size);
#else
    UNUSED(size);
#endif

    writeDataToArray(header_, data);
}

void Message::writeVariableToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::variable(this), size);
#else
    UNUSED(size);
#endif

    switch (type()) {
        case message::type::PROTOBUF:
            writeDataToArray(size_, data);
            return;
        case message::type::SIMPLE:
            return;
    }

    ENSURE_FAIL();
}

void Message::writePayloadToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::payload(this), size);
#else
    UNUSED(size);
#endif

    switch(type()) {
        case message::type::PROTOBUF:
            writeProtobufMessage(data, size, std::get<MessageProtobufPayloadType>(payload_));
            return;
        case message::type::SIMPLE:
            writeSimpleMessage(data, size, std::get<MessageSimplePayloadType>(payload_));
            return;
    }

    ENSURE_FAIL();
}

std::uint8_t Message::version() const {
    return (header_ & VERSION_MASK) >> VERSION_SHIFT;
}

std::uint8_t Message::type() const {
    return (header_ & TYPE_MASK) >> TYPE_SHIFT;
}

Message::SizeType Message::size() const {
    return size_;
}

void Message::setVersion(std::uint8_t version) {
    header_ &= ~VERSION_MASK;
    header_ |= version << VERSION_SHIFT;
}

void Message::setType(std::uint8_t type) {
    header_ &= ~TYPE_MASK;
    header_ |= type << TYPE_SHIFT;
}

void Message::setSize(SizeType size) {
    size_ = size;
}

bool Message::validVersion() const {
    std::uint8_t v = version();
    return v == message::version::FIRST;
}

bool Message::validType() const {
    std::uint8_t t = type();
    return t == message::type::PROTOBUF || t == message::type::SIMPLE;
}

bool Message::validSize() const {
    return size_ > 0;
}

bool Message::compareMetadata(const Message& other) const {
    return version() == other.version()
        && type() == other.type()
        && size() == other.size();
}

// --- Message Factory ---
std::shared_ptr<MessageFactory> MessageFactory::configure() {
    return std::shared_ptr<MessageFactory>(new MessageFactory{});
}

std::size_t MessageFactory::parse(void* data, std::size_t& available) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
#else
    UNUSED(available);
#endif

    std::size_t shift = 0;
    auto array = reinterpret_cast<std::uint8_t*>(data);
    switch (state_.stage) {
        case State::EStage::HEADER:
            // all messages are at least header + simple payload size (= size in structured), so request that
#ifdef SSD_RUNTIME_CHECKS
            ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::header() + sizeof(MessageAddedSize), available);
#endif
            state_.parsing.readHeaderFromArray(array + shift, available - shift);
            shift += Message::Size::header();
            state_.parsing.readVariableFromArray(array + shift, available - shift);
            shift += Message::Size::variable(&state_.parsing);
            if (state_.parsing.type() == message::type::SIMPLE) {
                state_.parsing.readPayloadFromArray(array + shift, available - shift);
                shift += Message::Size::payload(&state_.parsing);
                // expect next message
                finishOnState(available, shift, State::EStage::HEADER, true);
                return Message::Size::header() + sizeof(MessageAddedSize);
            }
            finishOnState(available, shift, State::EStage::PAYLOAD);
            return Message::Size::payload(&state_.parsing);
        
        case State::EStage::PAYLOAD:
#ifdef SSD_RUNTIME_CHECKS
            ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(Message::Size::payload(&state_.parsing), available);
#endif
            state_.parsing.readPayloadFromArray(array, available);
            finishOnState(available, Message::Size::payload(&state_.parsing), State::EStage::HEADER, true);
            return Message::Size::header() + sizeof(MessageAddedSize);
    }

    ENSURE_FAIL();
}

void MessageFactory::finishOnState(std::size_t& available, std::size_t shifted, State::EStage nextStage, bool renewMessage) {
    available -= shifted;
    state_.stage = nextStage;
    if (renewMessage) {
        parsed_.emplace(std::move(state_.parsing));
        std::construct_at(&state_.parsing);
    }
}

Message MessageFactory::parsed() {
    return popQueue(parsed_);
}

Message MessageFactory::constructed() {
    return popQueue(constructed_);
}

bool MessageFactory::isConstructedAvailable() const {
    return constructed_.size();
}

bool MessageFactory::isParsedAvailable() const {
    return parsed_.size();
}

MessageFactory& MessageFactory::withMessageVersion(std::uint8_t version, bool persistent) {
    if (persistent) {
        version_ = version;
    } else {
        state_.constructing.setVersion(version);
    }
    return *this;
}

MessageFactory& MessageFactory::withType(std::uint8_t type, bool persistent) {
    if (persistent) {
        type_ = type;
    } else {
        state_.constructing.setType(type);
    }
    return *this;
}

MessageFactory& MessageFactory::withPayload(MessageProtobufPayloadType payload) {
    state_.constructing.setSize(payload.ByteSizeLong());
    state_.constructing.payload_ = std::move(payload);
    return *this;
}

MessageFactory& MessageFactory::withPayload(MessageSimplePayloadType payload) {
    state_.constructing.setSize(sizeof(MessageSimplePayloadType));
    state_.constructing.payload_ = payload;
    return *this;
}

MessageFactory& MessageFactory::construct() {
    if (!state_.constructing.validVersion()) {
        state_.constructing.setVersion(version_);
    }
    if (!state_.constructing.validType()) {
        state_.constructing.setType(type_);
    }
    if (!state_.constructing.validSize()) {
        // invalid size means that payload was not set essentially
        ENSURE_FAIL();
    }

    // check for type match
    std::uint8_t type = state_.constructing.type();
    switch (type) {
        case message::type::SIMPLE:
            ENSURE_FAIL_UNLESS(std::holds_alternative<MessageSimplePayloadType>(state_.constructing.payload_));
            break;
        case message::type::PROTOBUF:
            ENSURE_FAIL_UNLESS(std::holds_alternative<MessageProtobufPayloadType>(state_.constructing.payload_));
            break;
    }

    constructed_.emplace(std::move(state_.constructing));
    std::construct_at(&state_.constructing);
    return *this;
}

std::size_t MessageFactory::next() const {
    switch(state_.stage) {
        case State::EStage::HEADER:
            return Message::Size::header() + sizeof(MessageAddedSize);
        case State::EStage::PAYLOAD:
            return Message::Size::payload(&state_.parsing);
    }

    ENSURE_FAIL();
}
