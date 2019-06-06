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

    @file InstanceGarbageCollectorAgent.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "InstanceGarbageCollectorAgent.h"
#include "ProtocolInstance.h"

using namespace std;

queue< ProtocolInstance* > InstanceGarbageCollectorAgent::garbageQueue;

mutex InstanceGarbageCollectorAgent::garbageMutex;

thread* InstanceGarbageCollectorAgent::garbageThread;

bool InstanceGarbageCollectorAgent::exitRequested = false;

void InstanceGarbageCollectorAgent::scheduleForDeletion( ProtocolInstance& pi ) {
    lock_guard< mutex > lock( garbageMutex );
    garbageQueue.push( &pi );
}

void InstanceGarbageCollectorAgent::cleanQueue() {
    lock_guard< mutex > lock( garbageMutex );
    while ( !garbageQueue.empty() ) {
        delete garbageQueue.front();
        garbageQueue.pop();
    };
};

void InstanceGarbageCollectorAgent::garbageLoop() {
    while ( !exitRequested ) {
        cleanQueue();
        this_thread::sleep_for( chrono::milliseconds( 50 ) );
    }
}


InstanceGarbageCollectorAgent::InstanceGarbageCollectorAgent( Node& /*node_*/ )
//: node(&node_) // l_sergiy: clang did detected this as unused
{
    garbageThread = new thread( InstanceGarbageCollectorAgent::garbageLoop );
}

void InstanceGarbageCollectorAgent::exitGracefully() {
    InstanceGarbageCollectorAgent::exitRequested = true;
    if ( garbageThread->joinable() )  // TODO why it's special?
        garbageThread->join();        // TODO there can be many such threads!!
    // XXX there can be many such threads!!
}

InstanceGarbageCollectorAgent::~InstanceGarbageCollectorAgent() {
    delete garbageThread;
}
