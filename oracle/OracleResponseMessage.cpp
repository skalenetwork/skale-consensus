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

    @file OracleResponseBroadcastMessage.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"

#include "exceptions/FatalError.h"

#include "messages/NetworkMessage.h"

#include "chains/Schain.h"
#include "protocols/ProtocolKey.h"

#include "protocols/binconsensus/BinConsensusInstance.h"
#include "OracleClient.h"
#include "OracleResult.h"
#include "OracleServerAgent.h"


#include "OracleResponseMessage.h"

OracleResponseMessage::OracleResponseMessage(string& _oracleResult, string& _receipt, block_id _blockID,
                                             uint64_t _timeMs,
                                             OracleClient &sourceProtocolInstance)
        : NetworkMessage(MSG_ORACLE_RSP, _blockID, 0, 0, 0, _timeMs,
                         sourceProtocolInstance), oracleResultStr(_oracleResult), receipt(_receipt)  {
    printPrefix = "r";
    oracleResult = OracleResult::parseResult(_oracleResult);

    CHECK_STATE2(oracleResult->getChainId() == sourceProtocolInstance.getSchain()->getSchainID(),
                 "Invalid schain id in oracle spec:" + to_string(oracleResult->getChainId()));
}


OracleResponseMessage::OracleResponseMessage(string& _oracleResult, string& _receipt, node_id _srcNodeID, block_id _blockID,
                                                             uint64_t _timeMs,
                                                             schain_id _schainId, msg_id _msgID,
                                                             schain_index _srcSchainIndex,
                                                             const string &_ecdsaSig, const string &_publicKey,
                                                             const string &_pkSig, Schain *_sChain)
        : NetworkMessage(
        MSG_ORACLE_RSP, _srcNodeID, _blockID, 0, 0, 0, _timeMs, _schainId, _msgID,
        "", _ecdsaSig, _publicKey, _pkSig,
        _srcSchainIndex, _sChain->getCryptoManager()), oracleResultStr(_oracleResult), receipt(_receipt) {
    printPrefix = "r";
    oracleResult = OracleResult::parseResult(_oracleResult);
};


void OracleResponseMessage::serializeToStringChild(rapidjson::Writer<rapidjson::StringBuffer>& _writer) {
    _writer.String("rslt");
    _writer.String(oracleResultStr.data(), oracleResultStr.size());

    _writer.String("rcpt");
    _writer.String(receipt.data(), receipt.size());
}


void OracleResponseMessage::updateWithChildHash(blake3_hasher& _hasher) {
    uint32_t  resultLen = oracleResultStr.size();
    HASH_UPDATE(_hasher, resultLen)
    if (resultLen > 0) {
        blake3_hasher_update(&_hasher, (unsigned char *) oracleResultStr.data(), resultLen);
    }

    uint32_t  receiptLen = receipt.size();
    HASH_UPDATE(_hasher, receiptLen)
    if (receiptLen > 0) {
        blake3_hasher_update(&_hasher, (unsigned char *) receipt.data(), receiptLen);
    }
}


const string &OracleResponseMessage::getReceipt() const {
    return receipt;
}

const ptr<OracleResult> &OracleResponseMessage::getOracleResult() const {
    return oracleResult;
}

const string &OracleResponseMessage::getOracleResultStr() const {
    return oracleResultStr;
}

const string OracleResponseMessage::getUnsignedOracleResultStr() const {
    auto commaPosition = oracleResultStr.find_last_of(",");
    CHECK_STATE(commaPosition != string::npos);
    auto res = oracleResultStr.substr(0, commaPosition + 1);
    cerr << res << endl;
    return res;
}