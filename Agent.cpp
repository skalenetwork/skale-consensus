//
// Created by stan on 12.04.18.
//

#include "SkaleConfig.h"
#include "Log.h"
#include "Agent.h"

#include "thirdparty/json.hpp"
#include "node/Node.h"
#include "chains/Schain.h"
#include "network/Sockets.h"

#include "crypto/SHAHash.h"
#include "crypto/BLSSigShare.h"


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
    sChain->getNode()->getAgents().push_back(this);

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
}



