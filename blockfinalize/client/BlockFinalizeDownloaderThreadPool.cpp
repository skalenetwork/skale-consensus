/*
    Copyright (C) 2019 SKALE Labs

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

    @file BlockFinalizeDownloaderThreadPool.cpp
    @author Stan Kladko
    @date 2019
*/


#include "SkaleCommon.h"
#include "Log.h"
#include "thirdparty/json.hpp"


#include "Agent.h"

#include "chains/Schain.h"
#include "node/Node.h"
#include "exceptions/FatalError.h"


#include "abstracttcpserver/ConnectionStatus.h"
#include "network/ServerConnection.h"

#include "thirdparty/json.hpp"

#include "BlockFinalizeDownloader.h"
#include "BlockFinalizeDownloaderThreadPool.h"

BlockFinalizeDownloaderThreadPool::BlockFinalizeDownloaderThreadPool(
        num_threads numThreads, Agent *_params) : WorkerThreadPool(numThreads,
                                                                  _params, false) {
}


void BlockFinalizeDownloaderThreadPool::createThread(uint64_t threadIndex ) {

    auto downloader = (BlockFinalizeDownloader*)agent;

    CHECK_STATE( downloader );

    // thread numbering starts with 0 and schain indexes start with 1
    uint64_t destinationIndex = threadIndex + 1;

    // the node does not download from itself
    if ( destinationIndex == downloader->getSchain()->getSchainIndex())
        return;

    this->threadpool.push_back(make_shared<thread>(
            BlockFinalizeDownloader::workerThreadFragmentDownloadLoop, downloader, destinationIndex ));

}


void BlockFinalizeDownloaderThreadPool::startService() {

    for (uint64_t i = 0; i < (uint64_t )numThreads; i++) {
        createThread(i);
    }

}

BlockFinalizeDownloaderThreadPool::~BlockFinalizeDownloaderThreadPool() {

    if (!joined) {
        cerr << "Destroying non-joined BlockFinalizeDownloaderThreadPool" << "/n";
    }

    threadpool.clear();

}

