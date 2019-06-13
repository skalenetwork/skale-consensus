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

    @file Agent.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

#include "SkaleCommon.h"
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


    map< schain_index, ptr< condition_variable > > queueCond; // converted

    map< schain_index, ptr<mutex > > queueMutex; // converted



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
