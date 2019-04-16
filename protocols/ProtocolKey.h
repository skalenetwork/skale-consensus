#pragma  once

class NetworkMessage;


class ProtocolKey  {


public:

    block_id getBlockID() const;

    schain_index getBlockProposerIndex() const;

private:

    const block_id blockID;

    const schain_index  blockProposerIndex;

public:

    ProtocolKey(block_id _blockId, schain_index _blockProposerIndex);


    ProtocolKey(const ProtocolKey& key);

    virtual ~ProtocolKey() {
    }

};

bool operator<(const ProtocolKey &l, const ProtocolKey &r);


