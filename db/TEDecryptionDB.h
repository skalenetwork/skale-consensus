#pragma once


#include "Agent.h"

class Schain;
class DecryptedAESKeyList;

class BlockProposal;

class AESKeyDecryptionShareList;

#include "CacheLevelDB.h"


class TEDecryptionDB : public CacheLevelDB {
    recursive_mutex teDecryptionMutex;

public:
    explicit TEDecryptionDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    ptr<DecryptedAESKeyList> addDecryptionShares(const ptr<AESKeyDecryptionShareList> &_decryptionShareList);

    const string& getFormatVersion();

    bool haveDecryptions( const ptr< BlockProposal >& _proposal );

    ptr<AESKeyDecryptionShareList> deserializeDecryptionShareFromString(string decryptions);

    bool isEnoughDecryptions( block_id _blockID );

    ptr<AESKeyDecryptionShareList> getDecryptions( block_id _blockId, schain_index _decryptorIndex );
};
