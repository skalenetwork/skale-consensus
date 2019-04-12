//
// Created by kladko on 3/29/19.
//

#ifndef SKALED_BLSSIGNATURE_H
#define SKALED_BLSSIGNATURE_H



class BLSSignature {


    ptr<libff::alt_bn128_G1> sig;
    block_id blockId;

public:

    BLSSignature(ptr<string> s);


    ptr<string> toString();

    BLSSignature(ptr<string> _s, block_id _blockID);

    BLSSignature(ptr<libff::alt_bn128_G1>& _s, block_id _blockID);

    const block_id &getBlockId() const;


    const ptr<libff::alt_bn128_G1>& getSig() const;

};


#endif //SKALED_BLSSIGNATURE_H


