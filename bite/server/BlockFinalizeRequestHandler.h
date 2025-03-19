#pragma once

#include <iostream>
#include <memory>
#include <string>

#include "proxygen//httpserver/RequestHandler.h"
#include "BlockFinalizeRequestObject.h"

class BlockFinalizeService : public proxygen::RequestHandler {

public:


    BlockFinalizeService() {
    }

    void sendResponse(std::unique_ptr<folly::IOBuf> bodyIn) {
        std::cout << "Task " << i << " done by thread " << std::this_thread::get_id() << std::endl;
    }



    void onBody(std::unique_ptr<folly::IOBuf> body) noexcept override {
        std::string requestData = body->moveToFbString().toStdString();

        // First, do an async DB query (non-blocking)
        workerThreadPool.add([this, requestData]() {


                // Send response from the event loop
                folly::EventBase* evb = folly::EventBaseManager::get()->getEventBase();
                evb->runInEventBaseThread([this, requestData]() {
                    ResponseBuilder(downstream_)
                        .status(200, "OK")
                        .body(processe Resul=)
                        .sendWithEOM();
            });
        });
    }



};


