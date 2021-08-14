/*
    Copyright (C) 2019 SKALE Labs

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

    @file BlockSigShare.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_BLOCK_SIG_SHARES_DB
#define SKALED_BLOCK_SIG_SHARES_DB

class CommittedBlock;
class ThresholdSignature;

#include "thirdparty/lrucache.hpp"
#include "CacheLevelDB.h"

class CryptoManager;
class ThresholdSigShare;

class BlockSigShareDB : public CacheLevelDB {

    cache::lru_cache<string,string> sigShares;

    recursive_mutex sigShareMutex;

    const string& getFormatVersion() override;



public:
    BlockSigShareDB(
        Schain* _sChain, string& _dirName, string& _prefix, node_id _nodeId, uint64_t _maxDBSize );

    ptr< ThresholdSignature > checkAndSaveShare1(
        const ptr< ThresholdSigShare >& _sigShare, const ptr< CryptoManager >& _cryptoManager );

    ptr< ThresholdSignature > checkAndSaveShareInMemory(
        const ptr< ThresholdSigShare >& _sigShare, const ptr< CryptoManager >& _cryptoManager );

    ptr< map< schain_index, string > > writeStringToSetInMemory(
        const string& _value, block_id _blockId, schain_index _index );


    };


#endif  // SKALED_BLOCK_SIG_SHARES_DB
