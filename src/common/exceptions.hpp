#pragma once

// standard
#include <exception>
#include <string>


namespace laar {

    // init() methods should be called once on each object,
    // this exception signals that specified rule was violated
    class LaarBadInit : public std::exception {
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
    class LaarNoInit : public std::exception {
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
    class LaarOverrun : public std::exception {
    public:

        LaarOverrun() = default;
        LaarOverrun(std::string message)
        : message_(message)
        {}
        
        LaarOverrun(std::size_t by) { 
            message_ += "";
        }

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "critical overrun on buffer or variable";

    };

    // critical underrun on any buffer or variable
    class LaarDryRun : public std::exception {
    public:

        LaarDryRun() = default;
        LaarDryRun(std::string message)
        : message_(message)
        {}
        
        LaarDryRun(std::size_t by) { 
            message_ += "";
        }

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "critical underrun on buffer or variable";

    };

    class LaarSemanticError : public std::exception {
    public:

        LaarSemanticError() = default;
        LaarSemanticError(std::string message)
        : message_(message)
        {}

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "semantic error caused by invalid program state";

    };

    class LaarProtocolError : public std::exception {
    public:

        LaarProtocolError() = default;
        LaarProtocolError(std::string additional)
        {
            message_ = "";
        }

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    private:
        std::string message_ = "protocol message reading error: {}";

    };

    class LaarSoundHandlerError : public std::exception {
    public:
        LaarSoundHandlerError(std::string message) : message_(std::move(message)) {}

        virtual const char* what() const noexcept override {
            return message_.data();
        }

    protected:
        std::string message_;
    };

}