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

    @file SkaledInteractionAgent.cpp
    @author Stan Kladko
    @date 2021-
*/

#include <curl/curl.h>


#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/ExitRequestedException.h"
#include "chains/Schain.h"
#include "SkaledInteractionThread.h"
#include "SkaledInteractionAgent.h"


SkaledInteractionAgent::SkaledInteractionAgent( Schain& _schain )
    : Agent( _schain, true ) {

    try {
        LOG( info, "Constructing SkaledInteractionThread" );

        this->skaledInteractionThread = make_shared< SkaledInteractionThread >( this );
        skaledInteractionThread->startService();
    } catch ( ExitRequestedException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
};


void SkaledInteractionAgent::workerThreadItemSendLoop( SkaledInteractionAgent* _agent ) {
    CHECK_STATE( _agent )

    LOG( info, "Starting skaled interaction thread");

    _agent->waitOnGlobalStartBarrier();

    LOG( info, "Started skaled interaction thread ");

    auto agent = ( Agent* ) _agent;

    while ( !agent->getSchain()->getNode()->isExitRequested() ) {
        try {

            ptr<CommittedBlock> block = nullptr;

            auto success = _agent->incomingBlockQueue
                               .wait_dequeue_timed( block, 1000 * ORACLE_QUEUE_TIMEOUT_MS );
            if ( !success )
                continue;

            if (block) {
                cerr << "haha";
            } else {
                // null block in incoming block queue means a request for transactions vector
                cerr << "hihi";
            }

        } catch ( ExitRequestedException& e ) {
            return;
        } catch ( exception& e ) {
            SkaleException::logNested( e );
        } catch ( ... ) {
            LOG( err, "Error in Oracle loop, unknown object is thrown" );
        }
    }
}

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t WriteMemoryCallback( void* contents, size_t size, size_t nmemb, void* userp ) {
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = ( struct MemoryStruct* ) userp;

    char* ptr = ( char* ) realloc( mem->memory, mem->size + realsize + 1 );
    CHECK_STATE( ptr );
    mem->memory = ptr;
    memcpy( &( mem->memory[mem->size] ), contents, realsize );
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

using namespace nlohmann;


