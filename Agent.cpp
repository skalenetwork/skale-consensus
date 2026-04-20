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


#include "Agent.h"

#include <exceptions/ConnectionRefusedException.h>

#include "SkaleCommon.h"
#include "Log.h"


#include "node/Node.h"
#include "chains/Schain.h"
#include "network/Sockets.h"
#include "crypto/ConsensusBLSSigShare.h"
#include <utils/Time.h>

void Agent::notifyAllConditionVariables() {
    messageCond.notify_all();

    for ( auto&& item : queueCond ) {
        item.second->notify_all();
    }
}


Schain* Agent::getSchain() const {
    CHECK_STATE( sChain );
    return sChain;
}


Agent::Agent( Schain& _sChain, bool _isServer, bool _dontRegister )
    : isServer( _isServer ), sChain( &_sChain ) {
    if ( _dontRegister )
        return;
    sChain->getNode()->registerAgent( this );
}

Agent::Agent() {}  // empty constructor is used by tests}


ptr< Node > Agent::getNode() {
    return getSchain()->getNode();
}

void Agent::waitOnGlobalStartBarrier() {
    if ( isServer )
        getSchain()->getNode()->waitOnGlobalServerStartBarrier( this );
    else
        getSchain()->getNode()->waitOnGlobalClientStartBarrier();
}

Agent::~Agent() {}

ptr< GlobalThreadRegistry > Agent::getThreadRegistry() {
    return sChain->getNode()->getConsensusEngine()->getThreadRegistry();
}


void Agent::logConnectionRefused( ConnectionRefusedException& _e, schain_index _index, const char* _function) {
    if ( _e.triedConnect() ) {
        CONS_LOG(err, string("Connection refused connecting to node:" + to_string(_index) + " in " + _function));
        SkaleException::logNested( _e );
    }
}