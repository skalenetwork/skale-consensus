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

    @file Header.h
    @author Stan Kladko
    @date 2018
*/

#pragma once






class Buffer;
class Connection;
class SHAHash;

#include "../abstracttcpserver/ConnectionStatus.h"



class Header {
protected:

    const char* type = nullptr;
    ConnectionStatus status = CONNECTION_SERVER_ERROR;
    ConnectionSubStatus substatus = CONNECTION_ERROR_UNKNOWN_SERVER_ERROR;

    bool complete = false;

public:
    bool isComplete() const;

    static constexpr const char *BLOCK_PROPOSAL_REQ = "BlockProposalReq";
    static constexpr const char *BLOCK_PROPOSAL_RSP = "BlockProposalRsp";
    static constexpr const char *BLOCK_FINALIZE_REQ = "BlockFinalizeReq";
    static constexpr const char *BLOCK_FINALIZE__RSP = "BlockFinalizeRsp";
    static constexpr const char *DA_PROOF_REQ = "DAProof";
    static constexpr const char *DA_PROOF_RSP = "DAProofRsp";
    static constexpr const char *CATCHUP_REQ = "Catchup";
    static constexpr const char *CATCHUP_RSP = "CatchupRsp";
    static constexpr const char *COMMITTED_BLOCK = "CommittedBlock";
    static constexpr const char *MISSING_TRANSACTIONS_REQ = "MissingTransactionsReq";
    static constexpr const char *MISSING_TRANSACTIONS_RSP = "MissingTransactionsRsp";


    Header(const char *_type);

    virtual ~Header();

    void setComplete() { complete = true; }


    static void nullCheck( nlohmann::json& js, const char* name );


    ptr< Buffer > toBuffer();


    virtual void addFields(nlohmann::json & /*j*/ ) { };

    static uint64_t getUint64( nlohmann::json& _js, const char* _name );

    static uint32_t getUint32( nlohmann::json& _js, const char* _name );

    static ptr< string > getString( nlohmann::json& _js, const char* _name );


    ConnectionStatus getStatus() { return status; }

    ConnectionSubStatus getSubstatus() { return substatus; }

    void setStatusSubStatus( ConnectionStatus _status, ConnectionSubStatus _substatus ) {
        this->status = _status;
        this->substatus = _substatus;
    }


    void setStatus( ConnectionStatus _status ) { this->status = _status; }

    static uint64_t getTotalObjects() {
        return totalObjects;
    }

private:


    static atomic<uint64_t>  totalObjects;


};



