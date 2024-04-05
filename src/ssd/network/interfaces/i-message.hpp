#pragma once

// standard
#include <initializer_list>
#include <cstddef>
#include <set>

// laar
#include <common/exceptions.hpp>

namespace laar {

    struct IEvent {
        // holder for payload
        virtual ~IEvent() {}
    };

    template<typename TDerivedMessage>
    class IMessage {
    public:

        class IMessageState {
        public:
            virtual ~IMessageState() = default;
            IMessageState(TDerivedMessage* message);
            IMessageState(const IMessageState&) = delete;
            IMessageState(IMessageState&&) = default;
            // generic state interface
            virtual void entry();
            virtual void exit();
            virtual void event(IEvent event);
            virtual std::size_t bytes() const;
            // transition function is always the same
            void transition(IMessageState* next);

            friend IMessage<TDerivedMessage>;

        protected:
            TDerivedMessage* const fsm_;
        };

        IMessage(std::initializer_list<const IMessageState*> states, IMessageState* initial);
        virtual ~IMessage() = default;

        virtual void reset();
        virtual void start();
        virtual std::size_t bytes();

        virtual void handle(IEvent event);
        const IMessageState* state() const;

        template<typename State>
        bool isInState();

        template<typename State>
        const State* getStateOrThrow() const;

    protected:
        void addStates(std::initializer_list<const IMessageState*> states);

    protected:
        IMessageState* current_ = nullptr;

    private:

        class StateValidator {
        public:
            bool present(const IMessageState* state) const;
            void addState(const IMessageState* state);
            void removeState(const IMessageState* state);

        private:
            std::set<const IMessageState*> states_;
        } stateValidator_;

    };

    template<typename TDerivedMessage>
    const IMessage<TDerivedMessage>::IMessageState* IMessage<TDerivedMessage>::state() const {
        return current_;
    }

    template<typename TDerivedMessage>
    template<typename State>
    bool IMessage<TDerivedMessage>::isInState() {
        return dynamic_cast<State*>(current_);
    }

    template<typename TDerivedMessage>
    template<typename State>
    const State* IMessage<TDerivedMessage>::getStateOrThrow() const {
        const auto casted = dynamic_cast<State*>(current_);
        if (casted) {
            return casted;
        }
        throw laar::LaarBadGet();
    }

    template<typename TDerivedMessage>
    IMessage<TDerivedMessage>::IMessage(std::initializer_list<const IMessageState*> states, IMessageState* initial) {
        addStates(std::move(states));
        if (stateValidator_.present(initial)) {
            current_ = initial;
        } else {
            throw laar::LaarValidatorError();
        }
    }

    template<typename TDerivedMessage>
    IMessage<TDerivedMessage>::IMessageState::IMessageState(TDerivedMessage* message) 
    : fsm_(message)
    {}

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::IMessageState::entry() {}

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::IMessageState::exit() {}

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::IMessageState::event(IEvent /* event */) {}

    template<typename TDerivedMessage>
    std::size_t IMessage<TDerivedMessage>::IMessageState::bytes() const { return 0; }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::start() {}

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::reset() {}

    template<typename TDerivedMessage>
    std::size_t IMessage<TDerivedMessage>::bytes() {
        return current_->bytes();
    }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::IMessageState::transition(IMessageState* next) {
        if (fsm_->stateValidator_.present(next)) {
            fsm_->current_->exit();
            fsm_->current_ = next;
            fsm_->current_->entry();
        } else {
            throw laar::LaarValidatorError();
        }
    }

    template<typename TDerivedMessage>
    bool IMessage<TDerivedMessage>::StateValidator::present(const IMessageState* state) const {
        return states_.contains(state) && state;
    }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::StateValidator::addState(const IMessageState* state) {
        states_.insert(state);
    }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::StateValidator::removeState(const IMessageState* state) {
        states_.erase(state);
    }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::addStates(std::initializer_list<const IMessageState*> states) {
        for (auto& state : states) {
            stateValidator_.addState(state);
        }
    }

    template<typename TDerivedMessage>
    void IMessage<TDerivedMessage>::handle(IEvent event) {
        current_->event(std::move(event));
    }
}