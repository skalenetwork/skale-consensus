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

    @file Agent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "Agent.h"

#include "thirdparty/json.hpp"
#include "node/Node.h"
#include "chains/Schain.h"
#include "network/Sockets.h"

#include "crypto/SHAHash.h"
#include "crypto/ConsensusBLSSigShare.h"


void Agent::notifyAllConditionVariables() {
    dispatchCond.notify_all();
    messageCond.notify_all();

    for (auto &&item : queueCond) {
        item.second->notify_all();
    }
}


Schain *Agent::getSchain() const {
    return sChain;
}


Agent::Agent(Schain &_sChain, bool _isServer, bool _isSchain) : isServer(_isServer), sChain(&_sChain) {

    if (_isSchain)
        return;
    lock_guard<recursive_mutex> lock(_sChain.getMainMutex());
    sChain->getNode()->registerAgent(this);

}


Node *Agent::getNode() {
    return getSchain()->getNode();
}

void Agent::waitOnGlobalStartBarrier() {
    if (isServer)
        getSchain()->getNode()->waitOnGlobalServerStartBarrier(this);
    else
        getSchain()->getNode()->waitOnGlobalClientStartBarrier(this);
}

Agent::~Agent() {
    cerr << "Agent destroyed" << endl;
}



