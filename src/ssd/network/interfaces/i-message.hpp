#pragma once

// standard
#include <initializer_list>
#include <exception>
#include <cstddef>
#include <set>

// laar
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>

namespace laar {

    // Controls construction of messages
    template<typename FallbackHandle>
    class IMessageBuilder {
    public:

        // Payload for state machine
        struct Receive {
            // Constructs payload
            Receive(std::size_t size, laar::RingBuffer& buffer)
            : size(size)
            , buffer(buffer)
            {}
            
            // Received size
            std::size_t size;
            // Buffer with data
            laar::RingBuffer& buffer;
        };

        class IFallbackHandler {
        public:
            virtual void onError(IMessageBuilder* builder, std::exception error) = 0;
            virtual ~IFallbackHandler() = default;
        };

        class IResult {

        };

        // resets construction process and puts initial state into ready
        virtual void reset() = 0;
        // inits builder, might be empty
        virtual void init() = 0;
        // check how many bytes of data needed for current chunk
        virtual void poll() const = 0;
        // handler for incoming data, returns true if construction is completed
        virtual bool handle(Receive payload) = 0;
        // checks if message is ready
        virtual operator bool() const;
        // same as above
        virtual bool ready() const = 0;

        virtual ~IMessageBuilder() = default;
    };

    template<typename TDerivedMessage>
    class MessageBase {
    public:

        class MessageStateBase {
        public:
            virtual ~MessageStateBase() = default;
            MessageStateBase(TDerivedMessage* message);
            MessageStateBase(const MessageStateBase&) = delete;
            MessageStateBase(MessageStateBase&&) = default;
            // generic state interface
            virtual void entry();
            virtual void exit();
            virtual std::size_t bytes() const;
            // transition function is always the same
            void transition(MessageStateBase* next);

            friend MessageBase<TDerivedMessage>;

        protected:
            TDerivedMessage* const message_;
        };

        MessageBase(std::initializer_list<const MessageStateBase*> states, MessageStateBase* initial);
        virtual ~MessageBase() = default;

        virtual void reset();
        virtual void start();
        virtual std::size_t bytes();

        template<typename TEvent>
        void handle(TEvent event);

        const MessageStateBase* state() const;

    protected:
        void addStates(std::initializer_list<const MessageStateBase*> states);

    protected:
        MessageStateBase* current_ = nullptr;

    private:

        class StateValidator {
        public:
            bool present(const MessageStateBase* state) const;
            void addState(const MessageStateBase* state);
            void removeState(const MessageStateBase* state);

        private:
            std::set<const MessageStateBase*> states_;
        } stateValidator_;

    };

    template<typename TDerivedMessage>
    const MessageBase<TDerivedMessage>::MessageStateBase* MessageBase<TDerivedMessage>::state() const {
        return current_;
    }

    template<typename TDerivedMessage>
    MessageBase<TDerivedMessage>::MessageBase(std::initializer_list<const MessageStateBase*> states, MessageStateBase* initial) {
        addStates(std::move(states));
        if (stateValidator_.present(initial)) {
            current_ = initial;
        } else {
            throw laar::LaarValidatorError();
        }
    }

    template<typename TDerivedMessage>
    MessageBase<TDerivedMessage>::MessageStateBase::MessageStateBase(TDerivedMessage* message) 
    : message_(message)
    {}

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::MessageStateBase::entry() {}

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::MessageStateBase::exit() {}

    template<typename TDerivedMessage>
    std::size_t MessageBase<TDerivedMessage>::MessageStateBase::bytes() const { return 0; }

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::start() {}

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::reset() {}

    template<typename TDerivedMessage>
    std::size_t MessageBase<TDerivedMessage>::bytes() {
        return current_->bytes();
    }

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::MessageStateBase::transition(MessageStateBase* next) {
        if (message_->stateValidator_.present(next)) {
            message_->current_->exit();
            message_->current_ = next;
            message_->current_->entry();
        } else {
            throw laar::LaarValidatorError();
        }
    }

    template<typename TDerivedMessage>
    bool MessageBase<TDerivedMessage>::StateValidator::present(const MessageStateBase* state) const {
        return states_.contains(state) && state;
    }

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::StateValidator::addState(const MessageStateBase* state) {
        states_.insert(state);
    }

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::StateValidator::removeState(const MessageStateBase* state) {
        states_.erase(state);
    }

    template<typename TDerivedMessage>
    void MessageBase<TDerivedMessage>::addStates(std::initializer_list<const MessageStateBase*> states) {
        for (auto& state : states) {
            stateValidator_.addState(state);
        }
    }

    template<typename TDerivedMessage>
    template<typename TEvent>
    void MessageBase<TDerivedMessage>::handle(TEvent event) {
        static_cast<typename TDerivedMessage::IEventStateHandle*>(current_)->event(std::move(static_cast<TEvent>(event)));
    }
}