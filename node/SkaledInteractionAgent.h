/*
    Copyright (C) 2021- SKALE Labs

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

    @file SkaledInteractionAgent.h
    @author Stan Kladko
    @date 2021-
*/

#pragma once

class Schain;


#include "sgxwallet/third_party/concurrentqueue.h"
#include "sgxwallet/third_party/readerwriterqueue.h"
#include "Agent.h"


class SkaledInteractionThread;
class CommittedBlock;

using namespace moodycamel;

class SkaledInteractionAgent : public Agent {

    ptr< SkaledInteractionThread > skaledInteractionThread = nullptr;

    BlockingReaderWriterQueue< shared_ptr< CommittedBlock > >
        incomingBlockQueue;

    BlockingReaderWriterQueue< shared_ptr< CommittedBlock > >
        outgoingTransactionsQueue;




public:
    SkaledInteractionAgent( Schain& _schain );

    virtual ~SkaledInteractionAgent(){};


    static void workerThreadItemSendLoop( SkaledInteractionAgent* _agent );

};
