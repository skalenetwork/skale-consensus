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

    @file ProtocolInstance.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "SkaleLog.h"
#include "abstracttcpserver/ConnectionStatus.h"
#include "blockproposal/pusher/BlockProposalClientAgent.h"
#include "chains/Schain.h"
#include "db/BlockProposalDB.h"
#include "exceptions/FatalError.h"
#include "messages/ParentMessage.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/BinConsensusInstance.h"
#include "thirdparty/json.hpp"


msg_id ProtocolInstance::createNetworkMessageID() {
    this->messageCounter+=1;
    return messageCounter;
}


ProtocolInstance::ProtocolInstance(ProtocolType _protocolType, Schain& _sChain)
    : sChain(&_sChain) , protocolType(_protocolType) , messageCounter(0) {
    totalObjects++;
}

Schain *ProtocolInstance::getSchain() const {
    return sChain;
}

ProtocolInstance::~ProtocolInstance() {
    totalObjects--;
}

atomic<int64_t>  ProtocolInstance::totalObjects(0);