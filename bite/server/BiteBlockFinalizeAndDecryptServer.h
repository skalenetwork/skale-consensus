/*
    Copyright (C) 2018-2019 SKALE Labs

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

    @file TransportNetwork.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


// remove conflict with Google LOG macro definition
#pragma push_macro("LOG")
#undef LOG
#include <proxygen/httpserver/RequestHandler.h>
#pragma pop_macro("LOG")


#include "Agent.h"

class Schain;
class Node;

using namespace proxygen;
using namespace folly;


class BiteBlockFinalizeAndDecryptServer : public Agent, RequestHandler {

public:

    explicit BiteBlockFinalizeAndDecryptServer( Schain& _sChain );

    ~BiteBlockFinalizeAndDecryptServer() override;


        void onRequest(std::unique_ptr<HTTPMessage> ) noexcept override {
            // Handle initial request headers
        }

        void onBody(std::unique_ptr<IOBuf> _body) noexcept override;

        void onEOM() noexcept override {
            // Handle end of message
        }

        void onUpgrade(UpgradeProtocol ) noexcept override {}

        void requestComplete() noexcept override {
            delete this;
        }

        void onError(ProxygenError ) noexcept override {
            delete this;
        }
};

