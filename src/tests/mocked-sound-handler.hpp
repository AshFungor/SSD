#pragma once

// laar
#include "util/config-loader.hpp"
#include <sounds/converter.hpp>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <sounds/interfaces/i-audio-handler.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// gtest
#include <gmock/gmock.h>

// proto
#include <protos/client/simple/simple.pb.h>

namespace laar {

    class MockedSoundHandler : public IStreamHandler {
    public:

        class MockReadHandle : public IReadHandle {
        public:
            MockReadHandle(
                TSimpleMessage::TStreamConfiguration config, 
                std::weak_ptr<IListener> owner
            ) {}
            MOCK_METHOD(int, read, (char* src, std::size_t size), (override));
            MOCK_METHOD(int, write, (const std::int32_t* dest, std::size_t size), (override));
            MOCK_METHOD(int, flush, (), (override));
            MOCK_METHOD(ESampleType, format, (), (override, const));
            MOCK_METHOD(bool, alive, (), (override, const, noexcept));
        };

        class MockWriteHandle : public IWriteHandle {
        public:
            MockWriteHandle(
                TSimpleMessage::TStreamConfiguration config, 
                std::weak_ptr<IListener> owner
            ) {}
            MOCK_METHOD(int, write, (const char* src, std::size_t size), (override));
            MOCK_METHOD(int, read, (std::int32_t* dest, std::size_t size), (override));
            MOCK_METHOD(int, flush, (), (override));
            MOCK_METHOD(ESampleType, format, (), (override, const));
            MOCK_METHOD(bool, alive, (), (override, const, noexcept));
        };

        MockedSoundHandler(std::shared_ptr<laar::ConfigHandler> configHandler)
        {}

        MOCK_METHOD(void, init, (), (override));
        MOCK_METHOD(
            std::shared_ptr<IReadHandle>, 
            acquireReadHandle, 
            (TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IHandle::IListener> owner), 
            (override)
        );
        MOCK_METHOD(
            std::shared_ptr<IWriteHandle>, 
            acquireWriteHandle, 
            (TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IHandle::IListener> owner), 
            (override)
        );
    };

}