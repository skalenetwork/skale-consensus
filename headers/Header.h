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



