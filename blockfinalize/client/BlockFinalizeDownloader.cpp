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

    @file BlockFinalizeDownloader.cpp
    @author Stan Kladko
    @date 2019
*/

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

    @file BlockFinalizeDownloader.cpp
    @author Stan Kladko
    @date 2018
*/

#include <exceptions/ConnectionRefusedException.h>
#include "SkaleCommon.h"

#include "SkaleLog.h"
#include "exceptions/ExitRequestedException.h"
#include "exceptions/FatalError.h"
#include "exceptions/InvalidStateException.h"


#include "thirdparty/json.hpp"

#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/TestConfig.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "node/Node.h"

#include "chains/Schain.h"
#include "crypto/SHAHash.h"
#include "datastructures/CommittedBlock.h"
#include "datastructures/CommittedBlockList.h"
#include "datastructures/BlockProposalFragment.h"
#include "datastructures/BlockProposalFragmentList.h"
#include "datastructures/BlockProposalSet.h"
#include "datastructures/BlockProposal.h"
#include "db/BlockProposalDB.h"
#include "db/DAProofDB.h"
#include "monitoring/LivelinessMonitor.h"
#include "exceptions/NetworkProtocolException.h"
#include "headers/BlockProposalRequestHeader.h"
#include "headers/BlockFinalizeRequestHeader.h"
#include "headers/BlockFinalizeResponseHeader.h"
#include "pendingqueue/PendingTransactionsAgent.h"

#include "BlockFinalizeDownloader.h"
#include "BlockFinalizeDownloaderThreadPool.h"


BlockFinalizeDownloader::BlockFinalizeDownloader(Schain *_sChain, block_id _blockId, schain_index _proposerIndex)
        : Agent(*_sChain, false, true),
          blockId(_blockId),
          proposerIndex(_proposerIndex),
          fragmentList(_blockId, (uint64_t) _sChain->getNodeCount() - 1) {

    CHECK_ARGUMENT(_sChain != nullptr);

    ASSERT(_sChain->getNodeCount() > 1);

    try {
        logThreadLocal_ = _sChain->getNode()->getLog();

        CHECK_STATE(sChain != nullptr);

        threadCounter = 0;


    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


nlohmann::json BlockFinalizeDownloader::readBlockFinalizeResponseHeader(ptr<ClientSocket> _socket) {
    MONITOR(__CLASS_NAME__, __FUNCTION__);
    return getSchain()->getIo()->readJsonHeader(_socket->getDescriptor(), "Read BlockFinalize response");
}


uint64_t BlockFinalizeDownloader::downloadFragment(schain_index _dstIndex, fragment_index _fragmentIndex) {


    try {

        auto header = make_shared<BlockFinalizeRequestHeader>(*sChain, blockId, proposerIndex,
                this->getNode()->getNodeID(), _fragmentIndex);
        auto socket = make_shared<ClientSocket>(*sChain, _dstIndex, CATCHUP);
        auto io = getSchain()->getIo();


        try {
            io->writeMagic(socket);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            throw_with_nested(
                    NetworkProtocolException("BlockFinalizec: Server disconnect sending magic", __CLASS_NAME__));
        }

        try {
            io->writeHeader(socket, header);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockFinalizec step 1: can not write BlockFinalize request";
            LOG(debug, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }
        LOG(debug, "BlockFinalizec step 1: wrote BlockFinalize request");

        nlohmann::json response;

        try {
            response = readBlockFinalizeResponseHeader(socket);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockFinalizec step 2: can not read BlockFinalize response";
            LOG(debug, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }


        LOG(debug, "BlockFinalizec step 2: read BlockFinalize response header");

        auto status = (ConnectionStatus) Header::getUint64(response, "status");

        if (status == CONNECTION_DISCONNECT) {
            LOG(debug, "BlockFinalizec got response::no fragment");
            return fragmentList.nextIndexToRetrieve();
        }

        if (status != CONNECTION_PROCEED) {
            BOOST_THROW_EXCEPTION(NetworkProtocolException(
                                          "Server error in BlockFinalize response:" +
                                          to_string(status), __CLASS_NAME__ ));
        }


        ptr<BlockProposalFragment> blockFragment = nullptr;


        try {
            blockFragment = readBlockFragment(socket, response, _fragmentIndex, getSchain()->getNodeCount());
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockFinalizec step 3: can not read fragment";
            LOG(err, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }


        uint64_t next = 0;

        fragmentList.addFragment(blockFragment, next);

        LOG(debug, "BlockFinalizec success");

        return next;

    } catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

uint64_t BlockFinalizeDownloader::readFragmentSize(nlohmann::json _responseHeader) {

    uint64_t result = Header::getUint64(_responseHeader, "fragmentSize");

    if (result == 0) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("fragmentSize == 0", __CLASS_NAME__));
    }

    return result;
};

uint64_t BlockFinalizeDownloader::readBlockSize(nlohmann::json _responseHeader) {
    uint64_t result = Header::getUint64(_responseHeader, "blockSize");

    if (result == 0) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException("blockSize == 0", __CLASS_NAME__));
    }

    return result;
};

ptr<string> BlockFinalizeDownloader::readBlockHash(nlohmann::json _responseHeader) {
    auto result = Header::getString(_responseHeader, "blockHash");
    return result;
};

ptr<BlockProposalFragment>
BlockFinalizeDownloader::readBlockFragment(ptr<ClientSocket> _socket, nlohmann::json responseHeader,
                                           fragment_index _fragmentIndex, node_count _nodeCount) {

    CHECK_ARGUMENT(responseHeader > 0);

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    auto fragmentSize = readFragmentSize(responseHeader);
    auto blockSize = readBlockSize(responseHeader);
    auto blockHash = readBlockHash(responseHeader);

    auto serializedFragment = make_shared<vector<uint8_t> >(fragmentSize);

    try {
        getSchain()->getIo()->readBytes(_socket->getDescriptor(), serializedFragment,
                                        msg_len(fragmentSize));
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read blocks", __CLASS_NAME__));
    }

    ptr<BlockProposalFragment> fragment = nullptr;

    try {
        fragment = make_shared<BlockProposalFragment>(blockId, (uint64_t) _nodeCount - 1,
                                                      _fragmentIndex, serializedFragment,
                                                      blockSize, blockHash);
    } catch (ExitRequestedException &) { throw; } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not parse block fragment", __CLASS_NAME__));
    }

    return fragment;


}


void BlockFinalizeDownloader::workerThreadFragmentDownloadLoop(BlockFinalizeDownloader *agent, schain_index _dstIndex) {

    auto sChain = agent->getSchain();
    auto node = sChain->getNode();
    auto proposalDB = node->getBlockProposalDB();
    auto daProofDB = node->getDaProofDB();
    auto blockId = agent->getBlockId();
    auto proposerIndex = agent->getProposerIndex();
    auto sChainIndex = sChain->getSchainIndex();
    bool testFinalizationDownloadOnly = node->getTestConfig()->isFinalizationDownloadOnly();

    setThreadName("BlckFinLoop", node->getConsensusEngine());

    uint64_t next = (uint64_t) _dstIndex;

    node->waitOnGlobalClientStartBarrier();

    if (next > (uint64_t) sChainIndex)
        next--;

    try {

        while (!node->isExitRequested()) {

            if (!testFinalizationDownloadOnly) {
                // take into account that the block can
                //  be in parralel committed through catchup
                if (sChain->getLastCommittedBlockID() >= blockId) {
                    return;
                }

                // take into account that the proposal and da proof can arrive through
                // BlockproposalServerAgent

                if (proposalDB->proposalExists(blockId, proposerIndex)) {
                    auto proposal = proposalDB->getBlockProposal(agent->blockId, agent->proposerIndex);
                    if (daProofDB->haveDAProof(proposal)) {
                        return;
                    }
                }

            }

            try {
                next = agent->downloadFragment(_dstIndex, next);
                if (next == 0) {
                    return;
                }
            } catch (ExitRequestedException &) {
                return;
            } catch (ConnectionRefusedException &e) {
                agent->logConnectionRefused(e, _dstIndex);
                usleep(node->getWaitAfterNetworkErrorMs() * 1000);
            } catch (exception &e) {
                SkaleException::logNested(e);
                usleep(node->getWaitAfterNetworkErrorMs() * 1000);
            };
        };
    } catch (FatalError *e) {
        node->exitOnFatalError(e->getMessage());
    }
}

ptr<BlockProposal> BlockFinalizeDownloader::downloadProposal() {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    {


        threadPool = make_shared<BlockFinalizeDownloaderThreadPool>((uint64_t) getSchain()->getNodeCount(), this);
        threadPool->startService();
        threadPool->joinAll();

    }

    try {

        if (fragmentList.isComplete()) {
            auto block = BlockProposal::deserialize(fragmentList.serialize(), getSchain()->getCryptoManager());
            return block;
        } else {
            return nullptr;
        }
    } catch (ExitRequestedException &) { throw; } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

BlockFinalizeDownloader::~BlockFinalizeDownloader() {

}

block_id BlockFinalizeDownloader::getBlockId() {
    return blockId;
}

schain_index BlockFinalizeDownloader::getProposerIndex() {
    return proposerIndex;
}


