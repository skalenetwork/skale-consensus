#pragma  once

#include "Header.h"

class NodeInfo;
class BlockProposal;
class Schain;
class CommittedBlock;

class CommittedBlockHeader : public Header {


    schain_id schainID;
    schain_index proposerIndex;
    node_id proposerNodeID;

    block_id blockID;
    ptr<SHAHash> blockHash;
    list<uint32_t> transactionSizes;
    uint64_t timeStamp = 0;

public:

    const schain_id &getSchainID() const;

    const block_id &getBlockID() const;

    CommittedBlockHeader();

    CommittedBlockHeader(CommittedBlock& _block);

    const ptr<SHAHash> &getBlockHash() const {
        return blockHash;
    }

    void addFields(nlohmann::basic_json<> &j) override;


};



