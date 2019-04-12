#pragma once


class SigShareSet;
class BLSSignature;
class Schain;

class ReceivedSigSharesDatabase : Agent {


    recursive_mutex sigShareDatabaseMutex;

    map<block_id, ptr<SigShareSet>> sigShareSets;

    map<block_id, ptr<BLSSignature>> blockSignatures;


    ptr<SigShareSet> getSigShareSet(block_id _blockID);

    ptr<BLSSignature> getBLSSignature(block_id _blockId);

public:



    ReceivedSigSharesDatabase(Schain &_sChain);

    bool addSigShare(ptr<BLSSigShare> _proposal);

    void mergeAndSaveBLSSignature(block_id _blockId);

    bool isTwoThird(block_id _blockID);
};



