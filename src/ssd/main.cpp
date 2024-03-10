// server
#include <network/server.hpp>

// plog
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>

// laar
#include <common/callback-queue.hpp>
#include <util/config-loader.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 8, 2);

    auto cbQueue = laar::CallbackQueue::configure({});
    auto configHandler = laar::ConfigHandler::configure("./", cbQueue);

    cbQueue->init();
    configHandler->init();

    auto& server = srv::Server::getInstance();
    server.init();
    server.run();

    return 0;
}