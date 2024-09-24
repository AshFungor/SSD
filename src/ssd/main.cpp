// grpc
#include "grpcpp/server_builder.h"
#include <grpcpp/grpcpp.h>

// standard
#include <memory>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>
#include <plog/Initializers/RollingFileInitializer.h>

// laar
#include <src/common/thread-pool.hpp>
#include <src/common/callback-queue.hpp>
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/audio-handler.hpp>
#include <src/ssd/core/routing-service.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 1024, 2);

    auto sharedCallbackQueue = laar::CallbackQueue::configure({});
    PLOG(plog::debug) << "module created: " << "SharedCallbackQueue; instance: " << sharedCallbackQueue.get();
    auto configHandler = laar::ConfigHandler::configure("", sharedCallbackQueue);
    PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

    sharedCallbackQueue->init();
    configHandler->init();

    auto soundHandler = laar::SoundHandler::configure(configHandler, sharedCallbackQueue);
    soundHandler->init();

    PLOG(plog::debug) << "module created: " << "SoundHandler; instance: " << soundHandler.get();

    auto routingService = std::make_shared<laar::RoutingService>(soundHandler);
    grpc::ServerBuilder builder;

    builder.AddListeningPort("localhost:7777", grpc::InsecureServerCredentials());
    builder.RegisterService(routingService.get());

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    if (!server) {
        PLOG(plog::error) << "failed to start server, builder returned nullptr";
        return 1;
    } else {
        PLOG(plog::info) << "server started, blocking main thread";
    }

    server->Wait();

    return 0;
}