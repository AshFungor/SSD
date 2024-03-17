#pragma once

// standard
#include <exception>


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
        virtual const char* what() const noexcept override {
            return "chunk received too much data, buffer is overrun";
        }
    };

}