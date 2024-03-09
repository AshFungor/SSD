#pragma once

// standard
#include <functional>
#include <chrono>
#include <memory>

namespace laar {

    using hs_clock = std::chrono::high_resolution_clock;

    class ICallback {
    public:
        virtual bool validate() const = 0;
        virtual bool locked() const = 0;
        virtual void execute() = 0;
        virtual void operator()() = 0;
    };

    class PersistentCallback : public ICallback {
    public:
        PersistentCallback(std::function<void()> task);
        PersistentCallback(const PersistentCallback&) = delete;
        virtual bool validate() const override;
        virtual bool locked() const override;
        virtual void execute() override;
        virtual void operator()() override;
    private:
        std::function<void()> task_;
    };

    class Callback : public ICallback {
    public:
        Callback(std::function<void()> task);
        Callback(const Callback&) = delete;
        virtual bool validate() const override;
        virtual bool locked() const override;
        virtual void execute() override;
        virtual void operator()() override;
    protected:
        bool valid_ {true};
        std::function<void()> task_;
    };

    class TimedCallback : public Callback {
    public:
        TimedCallback(std::function<void()> task, std::chrono::milliseconds timeout);
        virtual bool validate() const override;
        hs_clock::time_point goesOff() const;
    private:
        std::chrono::milliseconds timeout_;
        std::chrono::high_resolution_clock::time_point startTs_;
    };

    class OptionalCallback : public Callback {
    public:
        OptionalCallback(std::function<void()> task, std::weak_ptr<void> lifetime);
        virtual bool validate() const override;
    private:
        std::weak_ptr<void> lifetime_;
    };

    template<typename Callback_t, typename... Args>
    std::unique_ptr<ICallback> makeCallback(Args... args) {
        return std::make_unique<Callback_t>(args...);
    }

    template<typename Callback_t>
    std::unique_ptr<Callback_t> specifyCallback(std::unique_ptr<ICallback> generic) {
        return std::unique_ptr<Callback_t>(dynamic_cast<Callback_t*>(generic.release()));
    }

    template<typename Callback_t>
    bool isCastDownPossible(const std::unique_ptr<ICallback>& generic) {
        return dynamic_cast<Callback_t*>(generic.get());
    }

} // namespace laar