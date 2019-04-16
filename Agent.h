#pragma once

#include "SkaleConfig.h"
#include <condition_variable>
#include <mutex>


class Node;
class Schain;
class CommittedBlock;


class Agent {
public:


    bool isServer;

    virtual ~Agent();

    mutex dispatchMutex;


    /**
     *  Conditional variable that controls access to the inbox
     */
    condition_variable dispatchCond;


    /**
     * Mutex that controls access to inbox
     */
    mutex messageMutex;


    /**
     *  Conditional variable that controls access to the inbox
     */
    condition_variable messageCond;


    map< schain_index, ptr< condition_variable > > queueCond;

    map< schain_index, ptr<mutex > > queueMutex;



    virtual void notifyAllConditionVariables();



private:
    std::recursive_mutex m_mainMutex;
public:
    std::recursive_mutex & getMainMutex() { return m_mainMutex; }

protected:

    Schain* sChain;




public:

    Agent( Schain& _sChain, bool isServer, bool _isSchain = false );


    Node* getNode();



    Schain *getSchain() const;

    void waitOnGlobalStartBarrier();

    void waitOnGlobalClientStartBarrier();



};
