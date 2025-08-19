#pragma once


#include "Agent.h"

class Schain;
class DecryptedAESKeyList;

class BlockProposal;

class AESKeyDecryptionShareList;
class EncryptedAESKey;

using EncryptedAESKeyList = boost::container::flat_map<transaction_index, ptr<EncryptedAESKey> >;


#include "CacheLevelDB.h"


class TEDecryptionDB : public CacheLevelDB {
    recursive_mutex teDecryptionMutex;

    map<block_id, map<schain_index, ptr< AESKeyDecryptionShareList>>> decryptionsStore;
    shared_mutex decryptionSetsMutex;

public:
    explicit TEDecryptionDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    void addDecryptionShares(const ptr<AESKeyDecryptionShareList> &_decryptionShareList);

    bool haveDecryptionShares(block_id _blockID, schain_index _decryptorIndex);

     ptr<DecryptedAESKeyList> mergeAESKeys(block_id _blockId, ptr<EncryptedAESKeyList> _encryptedAESKeyList);


    void addMyDecryptionShares(const ptr<AESKeyDecryptionShareList> &_decryptionShareList);

    ptr<AESKeyDecryptionShareList> getMyDecryptionShares(block_id _blockId, schain_index _proposerIndex);

    const string& getFormatVersion();


    ptr<AESKeyDecryptionShareList> deserializeDecryptionShareFromString(string decryptions);

    bool isEnoughDecryptions( block_id _blockID );

    bool isEnoughForeignShares( block_id _blockID );

    uint64_t getDecryptionsCount( block_id _blockID );
};
