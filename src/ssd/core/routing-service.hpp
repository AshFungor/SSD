#pragma once

// protos
#include <protos/services/sound-router.grpc.pb.h>

// STD


namespace laar {

    class RoutingService 
        : public NSound::NServices::SoundStreamRouter::Service {
    public:

        // SoundStreamRouter implementation
        virtual grpc::Status RouteStream(
            grpc::ServerContext* context, 
            grpc::ServerReaderWriter<NSound::TServiceMessage, NSound::TClientMessage>* stream
        ) override;

    };

}