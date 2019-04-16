//
// Created by stan on 11.03.18.
//

#include "../SkaleConfig.h"
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
