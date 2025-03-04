/*
    Copyright (C) 2018- SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file TransportBiteBlockFinalizeAndDecryptServer.cpp
    @author Stan Kladko
    @date 2018-
*/


// remove conflict with Google LOG macro definition
#pragma push_macro("LOG")
#undef LOG
#include <proxygen/httpserver/HTTPServer.h>
#include <proxygen/httpserver/RequestHandler.h>
#include <proxygen/httpserver/RequestHandlerFactory.h>
#include <proxygen/httpserver/ResponseBuilder.h>
#include <folly/io/IOBuf.h>
#pragma pop_macro("LOG")


#include "SkaleCommon.h"
#include "chains/Schain.h"
#include "exceptions/FatalError.h"
#include "node/Node.h"


#include "exceptions/ExitRequestedException.h"
#include "exceptions/InvalidMessageFormatException.h"


#include "BiteBlockFinalizeAndDecryptServer.h"


class FlatBufferHandlerFactory : public RequestHandlerFactory {
public:
    void onServerStart(folly::EventBase * /*evb*/) noexcept override {
    }

    void onServerStop() noexcept override {
    }

    RequestHandler *onRequest(RequestHandler *, HTTPMessage *) noexcept override {
        return nullptr;
    }
};


/*
int main1(int argc, char* ) {
  std::vector<HTTPServer::IPConfig> ipConfigs = {
      {SocketAddress("0.0.0.0", 8080, true), HTTPServer::Protocol::HTTP}}
  ;

  HTTPServer server(std::make_unique<FlatBufferHandlerFactory>());
  server.bind(ipConfigs);
  server.start();

  return 0;
}
*/


BiteBlockFinalizeAndDecryptServer::BiteBlockFinalizeAndDecryptServer(Schain &_sChain) {
    auto cfg = _sChain.getNode()->getCfg();
}

BiteBlockFinalizeAndDecryptServer::~BiteBlockFinalizeAndDecryptServer() {
}


void BiteBlockFinalizeAndDecryptServer::onBody(std::unique_ptr<IOBuf> _body) noexcept {
    // Parse incoming FlatBuffers message
    const uint8_t *data = _body->data();
    // auto requestMsg = GetRequestMessage(data);

    // Extract data from FlatBuffer (replace with your schema fields)
    //std::string message = requestMsg->message()->str();

    // Create response FlatBuffer
    //flatbuffers::FlatBufferBuilder builder;
    //auto responseMessage = builder.CreateString("Response to: " + message);
    //auto response = CreateResponseMessage(builder, responseMessage);
    //builder.Finish(response);

    //auto responseBuffer = IOBuf::copyBuffer(builder.GetBufferPointer(), builder.GetSize());
    auto responseBuffer = "";
    ResponseBuilder(downstream_)
            .status(200, "OK")
            .header("Content-Type", "application/octet-stream")
            .body(std::move(responseBuffer))
            .sendWithEOM();
}
