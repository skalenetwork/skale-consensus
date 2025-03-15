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

#include <memory>


namespace proxygen {
class HTTPServer;
}

namespace std {
class thread;
}


class Schain;

class BiteBlockFinalizeServer {
    Schain& sChain;
    std::unique_ptr< proxygen::HTTPServer > proxygenServerInstance;
    std::shared_ptr< std::thread > runServerThread;

    void runServer();

public:
    explicit BiteBlockFinalizeServer( Schain& _sChain );

    void startProxygenServer();

    void exitProxygenServer();

    ~BiteBlockFinalizeServer();
};
