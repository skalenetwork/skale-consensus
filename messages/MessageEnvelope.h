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

    @file MessageEnvelope.h
    @author Stan Kladko
    @date 2018
*/

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


    MessageEnvelope(MessageOrigin origin, const ptr<Message> &_message,
                    const ptr<NodeInfo> &_realSender);

    ptr<Message> getMessage() const;

    MessageOrigin getOrigin() const;


    ptr<NodeInfo> getSrcNodeInfo() const {
        return srcNodeInfo;
    }

    virtual ~MessageEnvelope() {};


};

