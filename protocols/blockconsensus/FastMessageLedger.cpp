/*
    Copyright (C) 2021- SKALE Labs

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

    @file FastMessageLedger.cpp
    @author Stan Kladko
    @date 2021
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"
#include "Log.h"

#include "chains/Schain.h"

#include "FastMessageLedger.h"

FastMessageLedger::FastMessageLedger(Schain *_schain, string  _dirFullPath, block_id _blockId) :
        schain(_schain), blockId(_blockId) {

    previousRunMessages = make_shared<vector<ptr<Message>>>();
    CHECK_STATE(schain);
    CHECK_STATE(_dirFullPath.size() > 2);

    CHECK_STATE(_dirFullPath.back() != '/');

    ledgerFileFullPath= _dirFullPath + "/cons_incoming_msg_ledger_" +
            to_string(schain->getSchainIndex());


    std::ifstream infile(ledgerFileFullPath);

    // if file exist, read and parse

    if (infile.is_open()) {
        string line;
        uint64_t lineCount = 0;
        while (std::getline(infile, line))
        {
            lineCount++;
            if (lineCount == 1) {
                uint64_t bid = parseFirstLine(line);
                if (bid != _blockId) {
                    LOG(warn, "Fast ledger block id does not match");
                    return;
                }

                continue; // skip first line
            }
            auto nextMessage = parseLine(line);
            CHECK_STATE(nextMessage);
            previousRunMessages->push_back(nextMessage);
        }
    }
    startNewBlock(_blockId);
    CHECK_STATE(this->fd);
}


uint64_t FastMessageLedger::parseFirstLine(string _line) {
    try {

        rapidjson::Document d;

        d.Parse(_line.data());

        CHECK_STATE(!d.HasParseError());
        CHECK_STATE(d.IsObject())
        auto bid  = BasicHeader::getUint64Rapid(d, "bi");
        return bid;
    } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message", __CLASS_NAME__));
    }

}



ptr<Message> FastMessageLedger::parseLine(string& _line) {
    try {
    if (_line.size() < 15  && _line.find("\"cv\"") != string::npos) {
        return ConsensusProposalMessage::parseMessageLite(_line, schain);
    } else {
        return NetworkMessage::parseMessage(_line, schain, true);
    }
    } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message:" + _line, __CLASS_NAME__));
    }
}

ptr<vector<ptr<Message>>> FastMessageLedger::retrieveAndClearPreviosRunMessages() {
    auto result = previousRunMessages;
    previousRunMessages = nullptr;
    CHECK_STATE(result);
    return result;
}

void FastMessageLedger::writeProposalMessage(ptr<ConsensusProposalMessage> _message) {
    CHECK_STATE(_message);
    auto msg = _message->serializeToStringLite();
    LOCK(m)
    CHECK_STATE(this->fd > 0);
    writeLine(msg);
    cerr << msg;
}

void FastMessageLedger::writeNetworkMessage(ptr<NetworkMessage> _message) {
    CHECK_STATE(_message);
    auto msg = _message->serializeToStringLite();
    LOCK(m)
    writeLine(msg);
    cerr << msg;
}

void FastMessageLedger::closeFd() {
    LOG(info, "Close");
    if (this->fd > 0) {
        close(this->fd);
        this->fd = -1;
    }
}

void FastMessageLedger::writeLine(string& _str) {
    CHECK_STATE(_str.size() > 0);
    CHECK_STATE(this->fd > 0);
    int64_t written = 0;
    int64_t result = -1;
    do {
        result  = write(fd, _str.c_str() + written, _str.length() - written);
        if (result < 0) {
            LOG(err, "Write failed with errno:" + string(strerror(errno)) );
        }
        CHECK_STATE(result >= 0);
        written += result;
    } while (written < (int64_t) _str.size());

    do {
        result  = write(fd, "\n", 1);
        CHECK_STATE(result >= 0);
    } while (result == 0);
}

void FastMessageLedger::startNewBlock(block_id _blockId) {
    LOCK(m)
    blockId = _blockId;
    closeFd();
    this->fd = open(ledgerFileFullPath.c_str(), O_CREAT| O_TRUNC | O_WRONLY, S_IRWXU);
    CHECK_STATE2(fd > 0, ledgerFileFullPath + " file write open failed with errno:" +
                         string(strerror(errno)));

    string header = "{\"bi\":" + to_string((uint64_t) _blockId) + "}";

    writeLine(header);
}
