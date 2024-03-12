// server
#include "common/thread-pool.hpp"
#include <network/server.hpp>

// plog
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>
#include <plog/Log.h>

// laar
#include <common/callback-queue.hpp>
#include <util/config-loader.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 8, 2);

    auto cbQueue = laar::CallbackQueue::configure({});
    PLOG(plog::debug) << "module created: " << "CallbackQueue; instance: " << cbQueue.get();
    auto configHandler = laar::ConfigHandler::configure("", cbQueue);
    PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

    cbQueue->init();
    configHandler->init();

    auto threadPool = laar::ThreadPool::configure({});
    PLOG(plog::debug) << "module created: " << "ThreadPool; instance: " << threadPool.get();

    auto server = srv::Server::configure(cbQueue, threadPool, configHandler);
    PLOG(plog::debug) << "module created: " << "Server; instance: " << server.get();

    server->init();
    server->run();

    return 0;
}