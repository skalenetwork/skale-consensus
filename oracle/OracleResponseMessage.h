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

    @file OracleResponseBroadcastMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "messages/NetworkMessage.h"


class OracleProtocolInstance;
class OracleResult;

class OracleResponseMessage : public NetworkMessage {

    string oracleResultStr;
    ptr<OracleResult> oracleResult;
    string receipt;

protected:

    void updateWithChildHash(blake3_hasher& _hasher) override;

    void serializeToStringChild(rapidjson::Writer<rapidjson::StringBuffer>& _writer) override;


public:
    const ptr<OracleResult> &getOracleResult() const;

    OracleResponseMessage(string& _oracleResult, string &_receipt, block_id _blockID, uint64_t _timeMs, OracleClient& sourceProtocolInstance );

    OracleResponseMessage(string& _oracleResult, string& _receipt, node_id _srcNodeID, block_id _blockID,  uint64_t _timeMs, schain_id _schainId,
                                  msg_id _msgID, schain_index _srcSchainIndex, const string & _ecdsaSig,
                                  const string & _publicKey, const string & _pkSig, Schain* _sChain );

    const string &getOracleResultStr() const;

    const string getUnsignedOracleResultStr() const;

    const string &getReceipt() const;


};
