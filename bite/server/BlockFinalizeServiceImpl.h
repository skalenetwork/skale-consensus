//
// Created by kladko on 07-03-2025.
//

#ifndef SKALED_BLOCKFINALIZESERVICEIMPL_H
#define SKALED_BLOCKFINALIZESERVICEIMPL_H


#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "flatbuffers/block_finalize.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using block_finalize::BlockFinalizeService;
using block_finalize::BlockFinalizeRequest;
using block_finalize::BlockFinalizeResponse;
using block_finalize::SuccessResponse;
using block_finalize::ErrorResponse;

class BlockFinalizeServiceImpl final : public BlockFinalizeService::Service {

    std::unique_ptr<Server> server;

public:
    Status GetBlockFinalizeInfo(ServerContext* context,
                                const BlockFinalizeRequest* request,
                                BlockFinalizeResponse* response) override {
        std::cout << "Received request for blockID: " << request->blockid() << std::endl;

        if (request->blockid() == 0) {
            // Return an error if blockID is invalid
            auto* error = response->mutable_error();
            error->set_status(400);
            error->set_substatus(1);
            error->set_message("Invalid block ID");
        } else {
            // Return a successful response
            auto* success = response->mutable_success();
            success->set_blockhash("abcd1234ef567890");
            success->set_blocksize(1024);

            if (request->needdaproof()) {
                success->set_daproofsig("signature123");
            }

            if (request->has_fragmentindex()) {
                success->set_fragment("block_fragment_data");
            }
        }

        return Status::OK;
    }

    BlockFinalizeServiceImpl() {
    }

};





#endif //SKALED_BLOCKFINALIZESERVICEIMPL_H
