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

    @file InternalInfoDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "exceptions/InvalidStateException.h"
#include "VersionUpdateHistory.h"
#include "LevelDBOptions.h"
#include "InternalInfoDB.h"


InternalInfoDB::InternalInfoDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId,
    uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix, _nodeId, _maxDBSize,
          LevelDBOptions::getInternalInfoDBOptions(), false) {
}


static string VERSION_HISTORY_KEY = "versionHistory";

const string &InternalInfoDB::getFormatVersion() {
    static const string version = "1.0";
    return version;
}


string InternalInfoDB::readVersionUpdateHistory() {
    LOCK(m)
    return this->readString(VERSION_HISTORY_KEY);
}

/*
 * This is called each time consensus starts
 */
void InternalInfoDB::updateInternalChainInfo(block_id _blockId) {

    LOCK(m)

    auto currentVersion = getSchain()->getNode()->getConsensusEngine()->getEngineVersion();

    if (_blockId == 0) {
        // new chain, init with current version
        this->writeString(VERSION_HISTORY_KEY, currentVersion, true);
    } else {

        // updating version number for the existing chain
        auto storedVersionHistory = readVersionUpdateHistory();

        // first process the legacy case - no version string at all in an existing db
        if (storedVersionHistory.empty()) {
            // LEGACY
            auto version = "1.87";
            this->writeString(VERSION_HISTORY_KEY, version, true);
            return;
        } else {

            auto h = make_shared<VersionUpdateHistory>(storedVersionHistory);

            if (h->getHistory().back() != currentVersion) {
                storedVersionHistory.append(",");
                storedVersionHistory.append(currentVersion);
                this->writeString(VERSION_HISTORY_KEY, storedVersionHistory, true);
            }
        }
    }


    auto h = readVersionUpdateHistory();
    CHECK_STATE(!h.empty());
    LOG(info, "Version update history: " + h);

}

ptr<VersionUpdateHistory> InternalInfoDB::getVersionUpdateHistory() {
    LOCK(m)
    if (cachedVersionUpdateHistory)
        return cachedVersionUpdateHistory;

    auto storedVersionHistory = this->readString(VERSION_HISTORY_KEY);

    cachedVersionUpdateHistory = make_shared<VersionUpdateHistory>(storedVersionHistory);

    return cachedVersionUpdateHistory;

}

bool InternalInfoDB::isLegacy() {
    auto h = getVersionUpdateHistory()->getHistory();
    return (h.size() == 0) && (h.front() < "2.0");
}





