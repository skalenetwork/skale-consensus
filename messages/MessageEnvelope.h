#pragma once

enum MessageOrigin {ORIGIN_NETWORK, ORIGIN_PARENT, ORIGIN_CHILD, ORIGIN_EXTERNAL};

class Message;
class NodeInfo;

class MessageEnvelope {

protected:

    MessageOrigin origin;

    ptr<Message> message;


protected:


    ptr<NodeInfo> srcNodeInfo;

public:


    MessageEnvelope(MessageOrigin origin, const ptr<Message> &message,
                    const ptr<NodeInfo> &realSender);

    const ptr<Message> getMessage() const;

    MessageOrigin getOrigin() const;


    const ptr<NodeInfo> getSrcNodeInfo() const {
        return srcNodeInfo;
    }

    virtual ~MessageEnvelope() {};


};

