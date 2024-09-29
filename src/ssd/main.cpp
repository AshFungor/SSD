// boost
#include <boost/asio.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>

// grpc
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>

// standard
#include <thread>
#include <memory>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>
#include <plog/Initializers/RollingFileInitializer.h>

// laar
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/audio-handler.hpp>
#include <src/ssd/core/routing-service.hpp>

int main() {

    plog::init(plog::Severity::debug, "server.log", 1024 * 1024, 2);

    std::size_t threads = std::min<std::size_t>(std::thread::hardware_concurrency() / 2, 1);
    auto context = std::make_shared<boost::asio::io_context>();
    auto tPool = std::make_shared<boost::asio::thread_pool>(threads);

    auto configHandler = laar::ConfigHandler::configure("", context);
    PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

    if (auto result = configHandler->init(); !result.ok()) {
        PLOG(plog::error) << "error: " << result.message();
        return 1;
    }

    auto soundHandler = laar::SoundHandler::configure(configHandler, context);
    soundHandler->init();

    PLOG(plog::debug) << "module created: " << "SoundHandler; instance: " << soundHandler.get();

    auto routingService = laar::RoutingService::configure(soundHandler);
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

    for (std::size_t tNum = 0; tNum < threads; ++tNum) {
        boost::asio::post(*tPool, [context]() {
            context->run();
        });
    }

    server->Wait();

    return 0;
}