#pragma once

// STD
#include <string>
#include <type_traits>

namespace laar {

    class IErrorHolder {
    public:
        virtual bool holds() const = 0;
        virtual std::string describe() const = 0;

    };

    class SimpleErrorHolder 
        : IErrorHolder {
    public:
        SimpleErrorHolder() : holds_(false) {}
        SimpleErrorHolder(const std::string& what) : what_(what), holds_(true) {}
        virtual bool holds() const override { return holds_; }
        virtual std::string describe() const override { return (holds_) ? what_ : "no error"; }

    private:
        bool holds_;
        std::string what_;

    };

    template<typename ErrorHolder>
    class IDispatcher {
    public:

        static_assert(std::is_base_of<IErrorHolder, ErrorHolder>(), "Error Holder must be derived from IErrrorHolder");

        virtual IErrorHolder dispatch(void* in, void* out) = 0;
        virtual ~IDispatcher() = default;

    };

}
