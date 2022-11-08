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

    @file InternalInfoDB.h
    @author Stan Kladko
    @date 2019
*/


#ifndef SKALED_INTERNAL_INFO_DB_H
#define SKALED_INTERNAL_INFO_DB_H

class CommittedBlock;



#include "CacheLevelDB.h"


class CryptoManager;
class VersionUpdateHistory;

/**
 * Stored internal info such as history of installed versions
 */

class InternalInfoDB : public CacheLevelDB {

    recursive_mutex m;

    ptr<VersionUpdateHistory> cachedVersionUpdateHistory = nullptr;

public:

    /**
     * Create DB
     */
    InternalInfoDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize);


    const string& getFormatVersion() override ;

    /**
     * Get the current history of  version updates.
     */
    ptr<VersionUpdateHistory> getVersionUpdateHistory();

    /*
     * Called each time the chain starts
     */

    void updateInternalChainInfo(block_id _blockId);

    string readVersionUpdateHistory();

    bool isLegacy();
};


#endif 
