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


    std::map< schain_index, ptr< std::condition_variable > > queueCond;

    std::map< schain_index, ptr< std::mutex > > queueMutex;


    virtual void notifyAllConditionVariables();




public:
    std::recursive_mutex& getMainMutex() { return m; }

protected:
    Schain* sChain;

    std::recursive_mutex m;


public:
    Agent( Schain& _sChain, bool isServer, bool _isSchain = false );


    Node* getNode();


    Schain* getSchain() const;

    void waitOnGlobalStartBarrier();

    void waitOnGlobalClientStartBarrier();
};
