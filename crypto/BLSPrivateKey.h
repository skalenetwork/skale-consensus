//
// Created by kladko on 3/29/19.
//


#ifndef SKALED_BLSPRIVATEKEY_H
#define SKALED_BLSPRIVATEKEY_H 1

#include "BLSSigShare.h"

class BLSPrivateKey {
private:
    size_t nodeCount;

    ptr< libff::alt_bn128_Fr > sk;


public:
    BLSPrivateKey( const string& k, node_count _nodeCount );

    ptr< BLSSigShare > sign(
        ptr< string > _msg, block_id _blockId, schain_index _signerIndex, node_id _signerNodeId );

    ptr< string > convertSigToString( const libff::alt_bn128_G1& signature ) const;
};


#endif


