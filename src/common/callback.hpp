#pragma once

// standard
#include <memory>
#include <chrono>
#include <concepts>
#include <functional>

/* Callbacks are essential when there is a need for async execution.
 * This file contains some generic interfaces and specifications for use
 * within common threading primitives.
 */

namespace laar {

    using hs_clock = std::chrono::high_resolution_clock;

    // Generic callback.
    template<typename... Args>
    class ICallback {
    public:
        // Convenience method
        template<typename TCallback, typename... TInitialArgs>
        static std::unique_ptr<TCallback> makeCallback(TInitialArgs... args) 
            requires std::derived_from<TCallback, ICallback<Args...>>
        {
            return std::make_unique<TCallback>(args...);
        }
        // Calls underlying lambda or function
        virtual void execute(Args... args) = 0;
        // Same as execute(Args...)
        virtual void operator()(Args... args) = 0;
        virtual ~ICallback() = default;
    };

    // Conditional callbacks need additional validation
    // before being called.
    template<typename... Args>
    class IConditionalCallback : public ICallback<Args...> {
    public:
        // additional method to validate callback
        virtual bool isValid() const = 0;
        virtual ~IConditionalCallback() = default;
    };

    // Persistent callback is always valid
    template<typename... Args>
    class PersistentCallback : public IConditionalCallback<Args...> {
    public:
        PersistentCallback(std::function<void()> task);
        ~PersistentCallback() override = default;
        // IConditionalCallback implementation
        virtual bool isValid() const override;
        // ICallback implementation
        virtual void execute(Args... args) override;
        virtual void operator()(Args... args) override;

    private:
        std::function<void(Args...)> task_;
    };

    // Timed callback needs to be called at or after desired ts
    template<typename... Args>
    class TimedCallback : public IConditionalCallback<Args...> {
    public:
        TimedCallback(std::function<void()> task, std::chrono::milliseconds timeout);
        TimedCallback(std::function<void()> task, hs_clock::time_point expirationTs);
        ~TimedCallback() override = default;
        // IConditionalCallback implementation
        virtual bool isValid() const override;
        // ICallback implementation
        virtual void execute(Args... args) override;
        virtual void operator()(Args... args) override;
        // Get ts when this callback is valid
        hs_clock::time_point expires() const;

    private:
        std::function<void(Args...)> task_;
        hs_clock::time_point endTs_;
    };

    // Bound callback is linked to target resource and needs to check it existence first
    template<typename... Args>
    class BoundCallback : public IConditionalCallback<Args...> {
    public:
        BoundCallback(std::function<void()> task, std::weak_ptr<void> binding);
        ~BoundCallback() override = default;
        // IConditionalCallback implementation
        virtual bool isValid() const override;
        // ICallback implementation
        virtual void execute(Args... args) override;
        virtual void operator()(Args... args) override;

    private:
        std::function<void(Args...)> task_;
        std::weak_ptr<void> binding_;
    };


    template<typename... Args>
    PersistentCallback<Args...>::PersistentCallback(std::function<void()> task)
    : task_(std::move(task))
    {}

    template<typename... Args>
    TimedCallback<Args...>::TimedCallback(std::function<void()> task, std::chrono::milliseconds timeout) 
    : task_(std::move(task))
    , endTs_(hs_clock::now() + timeout)
    {}

    template<typename... Args>
    TimedCallback<Args...>::TimedCallback(std::function<void()> task, hs_clock::time_point endTs) 
    : task_(std::move(task))
    , endTs_(std::move(endTs))
    {}

    template<typename... Args>
    BoundCallback<Args...>::BoundCallback(std::function<void()> task, std::weak_ptr<void> binding) 
    : task_(std::move(task))
    , binding_(std::move(binding))
    {}

    template<typename... Args>
    bool PersistentCallback<Args...>::isValid() const {
        return true;
    }

    template<typename... Args>
    bool TimedCallback<Args...>::isValid() const {
        return hs_clock::now() >= endTs_;
    }

    template<typename... Args>
    bool BoundCallback<Args...>::isValid() const {
        return binding_.lock() != nullptr;
    }

    template<typename... Args>
    void PersistentCallback<Args...>::execute(Args... args) {
        task_(args...);
    }

    template<typename... Args>
    void TimedCallback<Args...>::execute(Args... args) {
        task_(args...);
    }

    template<typename... Args>
    void BoundCallback<Args...>::execute(Args... args) {
        task_(args...);
    }

    template<typename... Args>
    void PersistentCallback<Args...>::operator()(Args... args) {
        execute(args...);
    }

    template<typename... Args>
    void TimedCallback<Args...>::operator()(Args... args) {
        execute(args...);
    }

    template<typename... Args>
    void BoundCallback<Args...>::operator()(Args... args) {
        execute(args...);
    }

    template<typename... Args>
    hs_clock::time_point TimedCallback<Args...>::expires() const {
        return endTs_;
    }

} // namespace laar