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

    @file OutgoingMsgDB.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "crypto/SHAHash.h"
#include "chains/Schain.h"
#include "exceptions/InvalidStateException.h"
#include "messages/NetworkMessage.h"

#include "OutgoingMsgDB.h"
#include "network/Buffer.h"
#include "CacheLevelDB.h"


OutgoingMsgDB::OutgoingMsgDB(Schain *_sChain, string &_dirName, string &_prefix, node_id _nodeId, uint64_t _maxDBSize)
        : CacheLevelDB(_sChain, _dirName, _prefix,
                       _nodeId, _maxDBSize, false) {
}


bool
OutgoingMsgDB::saveMsg(ptr<NetworkMessage> _outgoingMsg) {

    static atomic<uint64_t> msgCounter = 0;

    lock_guard<recursive_mutex> lock(m);

    try {


        CHECK_STATE(_outgoingMsg);


        auto s = _outgoingMsg->serializeToString();

        auto currentCounter = msgCounter++;

        auto key = createKey(_outgoingMsg->getBlockID(), currentCounter);

        auto previous = readString(*key);

        if (previous == nullptr) {
            writeString(*key, *s);
            return true;
        }

        return (*previous == *s);

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<vector<ptr<NetworkMessage>>> OutgoingMsgDB::getMessages(block_id _blockID) {


    lock_guard<recursive_mutex> lock(m);

    try {

        auto messages = readStringsForBlock(_blockID);

        if (!messages)
            return nullptr;

        //for (auto&& message : *messages) {
           // return make_shared<NetworkMessage>(getSchain()->getNodeCount(), message);
        //}


/*
        auto key = createKey(_blockID);

        auto value = readString(*key);

        if (value == nullptr) {
            return nullptr;
        }
        return make_shared<NetworkMessage>(getSchain()->getNodeCount(), value);
        */
        return nullptr;

    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

const string OutgoingMsgDB::getFormatVersion() {
    return "1.0";
}





