/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with skale-consensus.  If not, see <http://www.gnu.org/licenses/>.

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

    ConnectionStatus status;
    ConnectionSubStatus substatus;


    bool complete = false;

public:
    bool isComplete() const;

protected:
    ptr< SHAHash > blockProposalHash;

public:
    Header();

    virtual ~Header();

    void setComplete() { complete = true; }


    static void nullCheck( nlohmann::json& js, const char* name );


    ptr< Buffer > toBuffer();


    virtual void addFields(nlohmann::json & /*j*/ ) { };

    static uint64_t getUint64( nlohmann::json& _js, const char* _name );

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



