#pragma once

// standard
#include <exception>
#include <format>
#include <string>


namespace laar {

    // init() methods should be called once on each object,
    // this exception signals that specified rule was violated
    class LaarBadInit : std::exception {
    public:

        LaarBadInit() = default;
        LaarBadInit(std::string message) 
        : message_(message) 
        {}

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "init() called more than once on created object";
    };

    // init() must be called at least once
    class LaarNoInit : std::exception {
    public:

        LaarNoInit() = default;
        LaarNoInit(std::string message)
        : message_(message)
        {}

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "init() was not called although object is being used";
    };

    // critical overrun on any buffer or variable
    class LaarOverrun : std::exception {
    public:

        LaarOverrun() = default;
        LaarOverrun(std::string message)
        : message_(message)
        {}
        
        LaarOverrun(std::size_t by) { 
            message_ += std::vformat(": {} bytes left out of bounds", std::make_format_args(by));
        }

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "critical overrun on buffer or variable";

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