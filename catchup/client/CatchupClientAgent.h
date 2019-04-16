#pragma once




class CommittedBlockList;
class ClientSocket;
class Schain;
class CatchupClientThreadPool;
class CatchupResponseHeader;

class CatchupClientAgent : public Agent{

    int connection;

public:


    atomic<uint64_t> threadCounter;

    ptr<CatchupClientThreadPool> catchupClientThreadPool = nullptr;


    CatchupClientAgent(Schain& subChain_);


    void sync(schain_index _dstIndex);


    static void workerThreadItemSendLoop(CatchupClientAgent *agent);

    nlohmann::json readCatchupResponseHeader(ptr<ClientSocket> _socket);


    ptr<CommittedBlockList> readMissingBlocks(ptr<ClientSocket> _socket, nlohmann::json responseHeader);


    size_t parseBlockSizes(nlohmann::json _responseHeader, ptr<vector<size_t>> _blockSizes);

    static schain_index nextSyncNodeIndex(const CatchupClientAgent *agent, schain_index _destinationSubChainIndex);
};

