/*
    Copyright (C) 2020- SKALE Labs

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

    @file StorageLimits.h
    @author Stan Kladko
    @date 2022
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "boost/tokenizer.hpp"

#include "VersionUpdateHistory.h"

/*
 * Version update history recorded in DB as a list of comma separated values
 */
VersionUpdateHistory::VersionUpdateHistory(string &_serializedHistory) {
    using namespace boost;
    if (_serializedHistory.empty())
        return;
    char_separator<char> sep{","};
    tokenizer tok{_serializedHistory, sep};
    for (const auto &version : tok) {
        history.push_back(version);
    }
}

const vector<string> &VersionUpdateHistory::getHistory() const {
    return history;
}

const string VersionUpdateHistory::serialize() {
    string serializedVersion = "";
    for (uint64_t i = 0; i < serializedVersion.size(); i++) {
        serializedVersion.append(history.at(i));
        if (i < serializedVersion.size() - 1)
            serializedVersion.append(",");
    }
    return serializedVersion;
}


