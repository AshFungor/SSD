#pragma once

// standard
#include <exception>
#include <optional>
#include <string>


namespace laar {

    class LaarBadInit : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "init() called more than once on created object";
        }
    };

    class LaarNoInit : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "init() was not called although object is about to be destroyed";
        }
    };

    class LaarOverrun : std::exception {
    public:

        LaarOverrun() { init(); }
        LaarOverrun(std::size_t by) : by_(by) { init(); }

        virtual const char* what() const noexcept override {
            return errorMessage_.c_str();
        }
    
    protected:
        void init() {
            errorMessage_ = std::string("received too much data, buffer is overrun by ") + std::to_string((by_.has_value()) ? by_.value() : 0);
        }

    protected:
        std::string errorMessage_;
        std::optional<size_t> by_;
    };

    class LaarBadGet : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "data requested by getter cannot be accessed";
        }
    };

    class LaarBadReceive : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "data contains less bytes than expected";
        }
    };

    class LaarValidatorError : std::exception {
    public:
        virtual const char* what() const noexcept override {
            return "state validation failed";
        }
    };

    class LaarSoundHandlerError : std::exception {
    public:
        LaarSoundHandlerError(std::string message) : message_(std::move(message)) {}

    protected:
        std::string message_;
    };

}