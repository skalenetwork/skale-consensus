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


enum ProtocolType { BLOCK_CONSENSUS, BIN_CONSENSUS, COMMONCOIN};

enum ProtocolStatus { STATUS_ACTIVE, STATUS_COMPLETED};
enum ProtocolOutcome { OUTCOME_UNKNOWN, OUTCOME_SUCCESS, OUTCOME_FAILURE, OUTCOME_KILLED};




class ProtocolInstance {


    Schain &  sChain;

    const ProtocolType protocolType; // unused










protected:


    ProtocolStatus status = STATUS_ACTIVE;

    ProtocolOutcome outcome = OUTCOME_UNKNOWN;



    /**
     * Instance ID
     */
    instance_id instanceID;

    /**
     * Counter for messages sent by this instance of the protocol
     */
    msg_id messageCounter;



    void setStatus(ProtocolStatus status);


    void setOutcome(ProtocolOutcome outcome);


public:


    ProtocolInstance(ProtocolType _protocolType, Schain& _schain);


    msg_id  createNetworkMessageID();

    Schain *getSchain() const;



    ProtocolStatus getStatus() const {
        return status;
    }

    ProtocolOutcome getOutcome() const;


    virtual ~ProtocolInstance();


    static uint64_t getTotalObjects() {
        return totalObjects;
    }

private:


    static atomic<uint64_t>  totalObjects;

};


