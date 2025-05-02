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

    void exitProxygenServer() noexcept;

    ~BiteBlockFinalizeServer();
};
