//
// Created by stan on 18.03.18.
//

#pragma once





#include <mutex>
#include <queue>
#include <condition_variable>
#include "../../abstracttcpserver/ConnectionStatus.h"
#include "../../abstracttcpserver/AbstractServerAgent.h"

#include "../../headers/Header.h"
#include "../../network/Connection.h"
#include "../../datastructures/PartialHashesList.h"
#include "../../headers/Header.h"

#include "CatchupWorkerThreadPool.h"
#include "../../Agent.h"

class CommittedBlock;
class CommittedBlockList;
class CatchupResponseHeader;

class CatchupServerAgent : public AbstractServerAgent {

   ptr<CatchupWorkerThreadPool> catchupWorkerThreadPool;


public:
    CatchupServerAgent(Schain &_schain, ptr<TCPServerSocket> _s);
    ~CatchupServerAgent() override;

    CatchupWorkerThreadPool *getCatchupWorkerThreadPool() const;

    ptr<vector<uint8_t>> createCatchupResponseHeader(ptr<Connection> _connectionEnvelope,
                                nlohmann::json _jsonRequest, ptr<CatchupResponseHeader> _responseHeader);

    void processNextAvailableConnection(ptr<Connection> _connection) override;

    ptr<vector<uint8_t>> getSerializedBlock(uint64_t i) const;
};
