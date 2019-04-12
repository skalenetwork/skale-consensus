//
// Created by kladko on 3/29/19.
//

#ifndef SKALED_BLSSIGNATURE_SHARE_H
#define SKALED_BLSSIGNATURE_SHARE_H


constexpr size_t BLS_COMPONENT_LEN = 80;
constexpr size_t BLS_SIG_LEN = 160;


namespace libff{
    class alt_bn128_G1;
}

class BLSSigShare {

    ptr<libff::alt_bn128_G1> sig;
    block_id blockId;
    schain_index signerIndex;
    node_id signerNodeId;


public:

    const ptr<libff::alt_bn128_G1> &getSig() const;

    ptr< string > toString();


    BLSSigShare(ptr<string> _s, block_id _blockID, schain_index _signerIndex,
                      node_id _nodeID);

    BLSSigShare(ptr<libff::alt_bn128_G1>& _s, block_id _blockID, schain_index _signerIndex, node_id _nodeID);

    const block_id &getBlockId() const;

    const schain_index &getSignerIndex() const;

    const node_id &getSignerNodeId() const;


};


#endif //SKALED_BLSSignatureShare_H


