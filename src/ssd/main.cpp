// server
#include <network/server.hpp>

// plog
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>
#include <plog/Log.h>

// laar
#include <common/callback-queue.hpp>
#include <sounds/audio-handler.hpp>
#include <common/thread-pool.hpp>
#include <util/config-loader.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 1024, 2);

    auto SharedCallbackQueue = laar::CallbackQueue::configure({});
    PLOG(plog::debug) << "module created: " << "SharedCallbackQueue; instance: " << SharedCallbackQueue.get();
    auto configHandler = laar::ConfigHandler::configure("", SharedCallbackQueue);
    PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

    SharedCallbackQueue->init();
    configHandler->init();

    auto threadPool = laar::ThreadPool::configure({});
    threadPool->init();
    
    PLOG(plog::debug) << "module created: " << "ThreadPool; instance: " << threadPool.get();

    auto soundHandler = laar::SoundHandler::configure(configHandler);
    soundHandler->init();

    PLOG(plog::debug) << "module created: " << "SoundHandler; instance: " << soundHandler.get();

    auto server = srv::Server::configure(SharedCallbackQueue, threadPool, configHandler, soundHandler);
    PLOG(plog::debug) << "module created: " << "Server; instance: " << server.get();


    server->init();
    server->run();

    return 0;
}