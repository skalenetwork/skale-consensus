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

    @file ProtocolInstance.h
    @author Stan Kladko
    @date 2018
*/

#pragma  once



class Node;
class Schain;
class MessageEnvelope;
class NetworkMessageEnvelope;
class InternalMessageEnvelope;
class InternalMessage;
class ChildMessage;
class ParentMessage;
class NetworkMessage;
class ProtocolKey;
class BlockConsensusAgent;
class BinConsensusInstance;


enum ProtocolType { BLOCK_SIGN, BIN_CONSENSUS};

enum ProtocolStatus { STATUS_ACTIVE, STATUS_COMPLETED};
enum ProtocolOutcome { OUTCOME_UNKNOWN, OUTCOME_SUCCESS, OUTCOME_FAILURE, OUTCOME_KILLED};




class ProtocolInstance {


    static atomic<int64_t>  totalObjects;

    Schain*  sChain;

    const ProtocolType protocolType; // unused

protected:


    /**
     * Instance ID
     */
    instance_id instanceID;

    /**
     * Counter for messages sent by this instance of the protocol
     */
    msg_id messageCounter;




public:


    ProtocolInstance(ProtocolType _protocolType, Schain& _schain);


    msg_id  createNetworkMessageID();

    Schain *getSchain() const;





    ProtocolOutcome getOutcome() const;


    virtual ~ProtocolInstance();


    static int64_t getTotalObjects() {
        return totalObjects;
    }
};


