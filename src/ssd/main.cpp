// server
#include <network/server.hpp>

// plog
#include <plog/Initializers/RollingFileInitializer.h>

// util
#include <plog/Severity.h>
#include <util/thread-pool.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 8, 2);

    auto& server = srv::Server::getInstance();
    server.init();
    server.run();

    return 0;
}