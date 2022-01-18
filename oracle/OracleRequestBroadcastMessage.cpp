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

    @file OracleRequestBroadcastMessage.cpp
    @author Stan Kladko
    @date 2021-
*/

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON

#include "SkaleCommon.h"

#include "exceptions/FatalError.h"

#include "messages/NetworkMessage.h"

#include "chains/Schain.h"
#include "utils/Time.h"
#include "protocols/ProtocolKey.h"
#include "OracleClient.h"
#include "OracleServerAgent.h"
#include "OracleRequestSpec.h"
#include "OracleRequestBroadcastMessage.h"

OracleRequestBroadcastMessage::OracleRequestBroadcastMessage(string& _requestSpec, block_id _blockID,
                                                             uint64_t _timeMs,
                                                             OracleClient &sourceProtocolInstance)
        : NetworkMessage(MSG_ORACLE_REQ_BROADCAST, _blockID, 0, 0, 0, _timeMs,
                         sourceProtocolInstance), requestSpec(_requestSpec) {
    printPrefix = "o";

    parsedSpec = OracleRequestSpec::parseSpec(_requestSpec);
    CHECK_STATE2(parsedSpec->getChainid() == sourceProtocolInstance.getSchain()->getSchainID(),
                 "Invalid schain id in oracle spec:" + to_string(parsedSpec->getChainid()));

    CHECK_STATE(parsedSpec->getTime() + ORACLE_TIMEOUT_MS > Time::getCurrentTimeMs())
    CHECK_STATE(parsedSpec->getTime()  < Time::getCurrentTimeMs() + ORACLE_FUTURE_JITTER_MS)
}


OracleRequestBroadcastMessage::OracleRequestBroadcastMessage(string& _requestSpec, node_id _srcNodeID, block_id _blockID,
                                                             uint64_t _timeMs,
                                                             schain_id _schainId, msg_id _msgID,
                                                             schain_index _srcSchainIndex,
                                                             const string &_ecdsaSig, const string &_publicKey,
                                                             const string &_pkSig, Schain *_sChain)
        : NetworkMessage(
        MSG_ORACLE_REQ_BROADCAST, _srcNodeID, _blockID, 0, 0, 0, _timeMs, _schainId, _msgID,
        "", _ecdsaSig, _publicKey, _pkSig,
        _srcSchainIndex, _sChain->getCryptoManager()), requestSpec(_requestSpec) {
    CHECK_STATE(_requestSpec.front() == '{' && _requestSpec.back() == '}')
    printPrefix = "o";

    parsedSpec = OracleRequestSpec::parseSpec(_requestSpec);
    CHECK_STATE2(parsedSpec->getChainid() == _schainId,
                 "Invalid schain id in oracle spec:" + to_string(_schainId));
    CHECK_STATE(parsedSpec->getTime() + ORACLE_TIMEOUT_MS > Time::getCurrentTimeMs())
    CHECK_STATE(parsedSpec->getTime()  < Time::getCurrentTimeMs() + ORACLE_FUTURE_JITTER_MS)


}

void OracleRequestBroadcastMessage::updateWithChildHash(blake3_hasher& _hasher) {
    uint32_t  requestLen = requestSpec.size();
    HASH_UPDATE(_hasher, requestLen)
    if (requestLen > 0) {
        blake3_hasher_update(&_hasher, (unsigned char *) requestSpec.data(), requestLen);
    }
}


void OracleRequestBroadcastMessage::serializeToStringChild(rapidjson::Writer<rapidjson::StringBuffer>& _writer) {
    _writer.String("spec");
    _writer.String(requestSpec.data(), requestSpec.size());
}

const ptr<OracleRequestSpec> &OracleRequestBroadcastMessage::getParsedSpec() const {
    return parsedSpec;
}

const string &OracleRequestBroadcastMessage::getRequestSpec() const {
    return requestSpec;
}
