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

    @file BasicHeader.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


class Buffer;
class ServerConnection;
class  BLAKE3Hash;

#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/document.h"
#include "abstracttcpserver/ConnectionStatus.h"

class BasicHeader {

protected:

    const char* type = nullptr;
    bool complete = false;

    static atomic<int64_t>  totalObjects;

public:


    static int64_t getTotalObjects();

    [[nodiscard]] bool isComplete() const;

    static constexpr const char *BLOCK_PROPOSAL_REQ = "BlckPrpslReq";
    static constexpr const char *BLOCK_PROPOSAL_RSP = "BlckPrpslRsp";
    static constexpr const char *BLOCK_FINALIZE_REQ = "BlckFinalizeReq";
    static constexpr const char * BLOCK_FINALIZE_RSP = "BlckFnlzRsp";
    static constexpr const char *BLOCK_DECRYPT_REQ = "BlckDecrReq";
    static constexpr const char * BLOCK_DECRYPT_RSP = "BlckDecrRsp";

    static constexpr const char *DA_PROOF_REQ = "DAPrfReq";
    static constexpr const char *DA_PROOF_RSP = "DAPrfRsp";
    static constexpr const char *BLOCK_CATCHUP_REQ = "BlckCatchupReq";
    static constexpr const char *BLOCK_CATCHUP_RSP = "BlckCatchupRsp";

    static constexpr const char *BLOCK = "Blck";
    static constexpr const char *MISSING_TRANSACTIONS_REQ = "MsngTxsReq";
    static constexpr const char *MISSING_TRANSACTIONS_RSP = "MsngTxsRsp";
    static constexpr const char *SIG_SHARE_RSP = "SigShareRsp";
    static constexpr const char *BV_BROADCAST = "B";
    static constexpr const char *AUX_BROADCAST = "A";
    static constexpr const char *BLOCK_SIG_BROADCAST = "S";
    static constexpr const char *ORACLE_REQUEST_BROADCAST = "O";
    static constexpr const char *ORACLE_RESPONSE = "R";
    static constexpr const char *CONSENSUS_PROPOSAL = "C";

    explicit BasicHeader(const char *_type);

    virtual ~BasicHeader();

    void setComplete() { complete = true; }

    static void nullCheck( nlohmann::json& js, const char* _name );


    virtual string serializeToString();

    ptr< Buffer > toBuffer();

    virtual void addFields(nlohmann::json & j ) = 0;

    static uint64_t getUint64( nlohmann::json& _js, const char* _name );

    static string getStringRapid(rapidjson::Document &_d, const char *_name);

    static uint64_t getUint64Rapid(rapidjson::Document &_d, const char *_name);

    static uint32_t getUint32( nlohmann::json& _js, const char* _name );

    static int32_t getInt32( nlohmann::json& _js, const char* _name );

    static string getString( nlohmann::json& _js, const char* _name );

    static ptr<vector<string>> getStringVector(nlohmann::json &_js, const char *_name);

};



