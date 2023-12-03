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

    @file StuckDetectionAgent.cpp
    @author Stan Kladko
    @date 2021
*/

#include "Log.h"
#include "SkaleCommon.h"
#include "datastructures/CommittedBlock.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include <db/BlockSigShareDB.h>
#include <db/ConsensusStateDB.h>
#include <db/DAProofDB.h>
#include <db/DASigShareDB.h>
#include <db/MsgDB.h>
#include <db/ProposalVectorDB.h>
#include <db/RandomDB.h>
#include <network/ClientSocket.h>
#include <network/IO.h>
#include <node/ConsensusEngine.h>
#include <crypto/CryptoManager.h>
#include "protocols/blockconsensus/BlockConsensusAgent.h"

#include "LivelinessMonitor.h"
#include "StuckDetectionAgent.h"
#include "StuckDetectionThreadPool.h"
#include "chains/Schain.h"
#include "node/Node.h"
#include "utils/Time.h"

#include "utils/Time.h"

StuckDetectionAgent::StuckDetectionAgent(Schain &_sChain) : Agent(_sChain, false, true) {
    try {
        logThreadLocal_ = _sChain.getNode()->getLog();
        this->sChain = &_sChain;
        // we only need one agent
        this->stuckDetectionThreadPool = make_shared<StuckDetectionThreadPool>(1, this);
        stuckDetectionThreadPool->startService();
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


void StuckDetectionAgent::StuckDetectionLoop(StuckDetectionAgent *_agent) {
    CHECK_ARGUMENT(_agent);
    setThreadName("StuckDetectionLoop", _agent->getSchain()->getNode()->getConsensusEngine());
    _agent->getSchain()->getSchain()->waitOnGlobalStartBarrier();

    LOG(info, "StuckDetection agent: started monitoring.");

    // determine if this is the first restart, or there we restarts
    // before
    auto numberOfPreviousRestarts = _agent->getNumberOfPreviousRestarts();

    if (numberOfPreviousRestarts > 0) {
        LOG(info, "Stuck detection engine: previous restarts detected:" << numberOfPreviousRestarts);
    }

    uint64_t restartIteration = numberOfPreviousRestarts + 1;
    uint64_t whenToRestart = 0;

    // loop until stuck is detected
    do {
        try {
            _agent->getSchain()->getNode()->exitCheck();
            usleep(_agent->getSchain()->getNode()->getStuckMonitoringIntervalMs() * 1000);
            // this will return non-zero if skaled needs to be restarted
            whenToRestart = _agent->doStuckCheck(restartIteration);
        } catch (ExitRequestedException &) {
            return;
        } catch (exception &e) {
            SkaleException::logNested(e);
        }
    } while (whenToRestart == 0);

    // Stuck detection loop detected stuck. Restart.
    try {
        LOG(info, "Stuck detection engine: restarting skaled because of stuck detected.");
        _agent->restart(whenToRestart, restartIteration);
    } catch (ExitRequestedException &) {
        return;
    }
}

uint64_t StuckDetectionAgent::getNumberOfPreviousRestarts() {
    // each time a restart happens, a file with a corresponding name
    // is created. To find out how many restarts already happened we
    // count these files
    uint64_t restartCounter = 0;
    while (boost::filesystem::exists(restartFileName(restartCounter + 1))) {
        restartCounter++;
    }
    return restartCounter;
}

void StuckDetectionAgent::join() {
    CHECK_STATE(stuckDetectionThreadPool);
    stuckDetectionThreadPool->joinAll();
}


bool StuckDetectionAgent::areTwoThirdsOfPeerNodesOnline() {
    LOG(info, "StuckDetectionEngine:: stuck detected. Checking network connectivity ...");

    std::unordered_set<uint64_t> connections;
    auto beginTime = Time::getCurrentTimeSec();
    auto nodeCount = getSchain()->getNodeCount();

    // check if can connect to 2/3 of peers. If yes, restart
    while (3 * (connections.size() + 1) < 2 * nodeCount) {
        if (Time::getCurrentTimeSec() - beginTime > 10) {
            LOG(info, "Stuck check Could not connect to 2/3 of nodes. Will not restart");
            return false;  // could not connect to 2/3 of peers
        }

        for (int i = 1; i <= nodeCount; i++) {
            if (i != (getSchain()->getSchainIndex()) && !connections.count(i)) {
                try {
                    if (getNode()->isExitRequested()) {
                        BOOST_THROW_EXCEPTION(ExitRequestedException( __CLASS_NAME__ ));
                    }
                    auto socket = make_shared<ClientSocket>(
                            *getSchain(), schain_index(i), port_type::PROPOSAL);
                    getSchain()->getIo()->writeMagic(socket, true);
                    connections.insert(i);
                } catch (ExitRequestedException &) {
                    throw;
                } catch (std::exception &e) {
                }
                usleep(50 * 1000);
            }
        }
    }
    LOG(info, "Stuck detection engine: could connect to 2/3 of nodes.");
    return true;
}


bool StuckDetectionAgent::stuckCheck(uint64_t _restartIntervalMs) {
    auto currentTimeMs = Time::getCurrentTimeMs();

    // note that when the consensus starts, lastCommitTimeMs is set to the consensus start time
    // consensus is deemed stuck if it did not produce a new block for _restartIntervalMs
    // provided that all nodes are alive and sgx server is down
    auto result = (currentTimeMs - getSchain()->getLastCommitTimeMs() > _restartIntervalMs) &&
                  (!sChain->getCryptoManager()->isSGXServerDown()) &&
                  areTwoThirdsOfPeerNodesOnline();

    return result;
}

uint64_t StuckDetectionAgent::doStuckCheck(uint64_t _restartIteration) {
    CHECK_STATE(_restartIteration >= 1);

    auto restartIntervalMs = getSchain()->getNode()->getStuckRestartIntervalMs();

    auto blockID = getSchain()->getLastCommittedBlockID();

    // do not restart for the first block
    if (blockID < 2)
        return 0;

    // check that the chain has not been doing much for a long time
    auto startTimeMs = Time::getCurrentTimeMs();
    while (Time::getCurrentTimeMs() - startTimeMs < 60000) {
        if (!stuckCheck(restartIntervalMs))
            return 0;
        usleep(5 * 1000 * 1000);
    }

    LOG(info, "Need for restart detected. Cleaning and restarting ");
    cleanupState();

    LOG(info, "Cleaned up state");

    auto lastCommittedBlockTimeStampS = getSchain()->getLastCommittedBlockTimeStamp().getS();

    return lastCommittedBlockTimeStampS * 1000 + restartIntervalMs + 120000;
}

void StuckDetectionAgent::restart(uint64_t _restartTimeMs, uint64_t _iteration) {
    CHECK_STATE(_restartTimeMs > 0);

    while (Time::getCurrentTimeMs() < _restartTimeMs) {
        try {
            usleep(100);
        } catch (...) {
        }

        getNode()->exitCheck();
    }

    createStuckRestartFile(_iteration + 1);

    LOG(err,
        "Consensus engine stuck detected, because no blocks were mined for a long time and "
        "majority of other nodes in the chain seem to be reachable on network. Restarting ...");

    exit(13);
}

string StuckDetectionAgent::restartFileName(uint64_t _iteration) {
    CHECK_STATE(_iteration >= 1);
    auto engine = getNode()->getConsensusEngine();
    CHECK_STATE(engine);
    string fileName = engine->getHealthCheckDir() + "/STUCK_RESTART";
    fileName.append("." + to_string(getNode()->getNodeID()));
    fileName.append("." + to_string(sChain->getLastCommittedBlockID()));
    fileName.append("." + to_string(_iteration));
    return fileName;
};

void StuckDetectionAgent::createStuckRestartFile(uint64_t _iteration) {
    CHECK_STATE(_iteration >= 1);
    auto fileName = restartFileName(_iteration);

    ofstream f;
    f.open(fileName, ios::trunc);
    f << " ";
    f.close();
}

void StuckDetectionAgent::cleanupState() {
    getSchain()->getNode()->getIncomingMsgDB()->destroy();
    getSchain()->getNode()->getOutgoingMsgDB()->destroy();
    getSchain()->getNode()->getBlockSigShareDB()->destroy();
    getSchain()->getNode()->getDaSigShareDB()->destroy();
    getSchain()->getNode()->getDaProofDB()->destroy();
    getSchain()->getNode()->getConsensusStateDB()->destroy();
    getSchain()->getNode()->getProposalVectorDB()->destroy();
    getSchain()->getNode()->getRandomDB()->destroy();
}
