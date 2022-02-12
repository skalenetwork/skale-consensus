/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file BlockDecryptionSet.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "DataStructure.h"

class PartialHashesList;
class Schain;
class BlockAesKeyDecryptionShares;
class BLAKE3Hash;
class BooleanProposalVector;
class DAProof;
class BlockDecryptedAesKeys;
class BlockEncryptedAesKeys;

class BlockAesKeyDecryptionSet : public DataStructure {

    map< uint64_t, ptr< BlockAesKeyDecryptionShares > > decryptions; // tsafe

    node_count nodeCount  = 0;

    uint64_t requiredDecryptionCount;

    block_id blockId  = 0;

    static atomic< int64_t > totalObjects;

    ptr<BlockEncryptedAesKeys> encryptedKeys;

    void mergeDecryptedAesKeyForTransaction(const Schain *_sChain,
                                            shared_ptr<map<uint64_t, ptr<vector<uint8_t>>>> &decryptedKeysMap,
                                            uint64_t transactionIndex);

    node_count getCount();


    ptr<BlockDecryptedAesKeys> mergeDecryptedKeyShares(const Schain *_sChain);

public:

    BlockAesKeyDecryptionSet(Schain* _sChain, block_id _blockId,
                             ptr<BlockEncryptedAesKeys> _encryptedKeys);

    ptr<BlockDecryptedAesKeys> add(const ptr<BlockAesKeyDecryptionShares>& _decryption,
                                   Schain* _sChain);

    static int64_t getTotalObjects() { return totalObjects; }

    bool isEnough();

    ~BlockAesKeyDecryptionSet() override;

};
