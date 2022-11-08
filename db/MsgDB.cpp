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

    @file MsgDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "chains/Schain.h"
#include "crypto/BLAKE3Hash.h"
#include "exceptions/InvalidStateException.h"
#include "messages/NetworkMessage.h"

#include "MsgDB.h"
#include "network/Buffer.h"
#include "CacheLevelDB.h"


MsgDB::MsgDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix,
                       _nodeId, _maxDBSize, false) {
}


bool
MsgDB::saveMsg(const ptr<NetworkMessage>& _msg) {

    static atomic<uint64_t> msgCounter = 0;

    CHECK_ARGUMENT(_msg)

    try {

        auto serialized = _msg->serializeToString();

        CHECK_STATE(!serialized.empty())

        auto currentCounter = msgCounter++;

        auto key = createKey(_msg->getBlockID(), currentCounter);

        CHECK_STATE(!key.empty())

        auto previous = readString(key);

        if (previous.empty()) {
            writeString(key, serialized );
            return true;
        }

        return (previous == serialized );

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<vector<ptr<NetworkMessage>>> MsgDB::getMessages(block_id _blockID) {

    auto result = make_shared<vector<ptr<NetworkMessage>>>();


    try {


        string prefix = getFormatVersion() + ":" + to_string(_blockID);

        auto messages = readPrefixRange(prefix);

        if (!messages)
            return result;

        for (auto&& message : *messages) {
            result->push_back(NetworkMessage::parseMessage(message.second, getSchain()));
        }

        return result;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

const string& MsgDB::getFormatVersion() {
    static const string version = "1.0";
    return  version;
}





