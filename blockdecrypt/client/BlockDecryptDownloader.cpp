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

    @file BlockDecryptDownloader.cpp
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

    @file BlockDecryptDownloader.cpp
    @author Stan Kladko
    @date 2018
*/

#include <exceptions/ConnectionRefusedException.h>

#include "exceptions/ExitRequestedException.h"


#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/TestConfig.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"

#include "chains/Schain.h"
#include "datastructures/ArgumentDecryptionSet.h"
#include "headers/BlockDecryptRequestHeader.h"
#include "monitoring/LivelinessMonitor.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "utils/Time.h"

#include "BlockDecryptDownloader.h"
#include "BlockDecryptDownloaderThreadPool.h"


BlockDecryptDownloader::BlockDecryptDownloader(Schain *_sChain, block_id _blockId)
        : Agent(*_sChain, false, true),
          blockId(_blockId),
          decryptionSet(_sChain, _blockId) {

    CHECK_ARGUMENT(_sChain)

    CHECK_STATE(_sChain->getNodeCount() > 1)

    try {
        logThreadLocal_ = _sChain->getNode()->getLog();
        CHECK_STATE(sChain)
    }
    catch (ExitRequestedException &) { throw; }
    catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


nlohmann::json BlockDecryptDownloader::readBlockDecryptResponseHeader(const ptr<ClientSocket> &_socket) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)
    CHECK_ARGUMENT(_socket)
    return getSchain()->getIo()->readJsonHeader(_socket->getDescriptor(),
                                                "Read BlockDecrypt response",
                                                10, _socket->getIP());
}


void
BlockDecryptDownloader::workerThreadDecryptionDownloadLoop(BlockDecryptDownloader *_agent, schain_index _dstIndex) {


    CHECK_STATE(_agent);

    auto sChain = _agent->getSchain();
    auto node = sChain->getNode();
    auto blockId = _agent->getBlockId();

    setThreadName("BlckDecrLoop", node->getConsensusEngine());

    node->waitOnGlobalClientStartBarrier();

    while (!node->isExitRequested() && !_agent->decryptionSet.isEnough()) {
        // take into account that the decrypted block can come through catchup
        if (sChain->getLastCommittedBlockID() >= blockId) {
            return;
        }

        try {
            if (_agent->downloadDecryptionShare(_dstIndex)) {
                // Decryption has been downloaded
                return;
            }
        } catch (ExitRequestedException &) {
            return;
        } catch (exception &e) {
            SkaleException::logNested(e);
            usleep(static_cast< __useconds_t >( node->getWaitAfterNetworkErrorMs() * 1000 ));
        };
    };
}

BlockDecryptDownloader::~BlockDecryptDownloader() {}

block_id BlockDecryptDownloader::getBlockId() {
    return blockId;
}


uint64_t BlockDecryptDownloader::downloadDecryptionShare(schain_index _dstIndex) {

    LOG(info, "BLCK_DECR_DWNLD:" + to_string(_dstIndex));

    try {

        auto encryptedKeys = vector<string>();
        encryptedKeys.push_back("haha");

        auto header = make_shared<BlockDecryptRequestHeader>(*sChain, blockId, proposerIndex,
                                                             this->getNode()->getNodeID(), (uint64_t )_dstIndex,
                                                             encryptedKeys);
        CHECK_STATE(_dstIndex != (uint64_t) getSchain()->getSchainIndex())
        if (getSchain()->getDeathTimeMs((uint64_t) _dstIndex) + NODE_DEATH_INTERVAL_MS > Time::getCurrentTimeMs()) {
            usleep(100000); // emulate timeout
            BOOST_THROW_EXCEPTION(ConnectionRefusedException("Dead node:" + to_string(_dstIndex),
                                                             5, __CLASS_NAME__));
        }
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
            LOG(err, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }

        nlohmann::json response;

        try {
            response = readBlockDecryptResponseHeader(socket);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockFinalizec step 2: can not read BlockFinalize response";
            LOG(err, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }


        auto status = (ConnectionStatus) Header::getUint64(response, "status");

        if (status == CONNECTION_DISCONNECT) {
            LOG(info, "BLCK_DECR_DWNLD:DICONNECT:" + to_string(_dstIndex) + ":" +
                      to_string(_dstIndex));
            return false;
        }

        if (status != CONNECTION_PROCEED) {
            BOOST_THROW_EXCEPTION(NetworkProtocolException(
                                          "Server error in BlockDecrypt response:" +
                                          to_string(status), __CLASS_NAME__ ));
        }


        ptr<ArgumentDecryptionShare> decryptionShare;

        return true;

    } catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}


map<uint64_t, ptr<vector<uint8_t>> BlockFinalizeDownloader::downloadDecryption() {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    {
        threadPool = make_shared<BlockFinalizeDownloaderThreadPool>((uint64_t) getSchain()->getNodeCount(), this);
        threadPool->startService();
        threadPool->joinAll();
    }

    try {

        if (fragmentList.isComplete()) {
            auto block = BlockProposal::deserialize(fragmentList.serialize(), getSchain()->getCryptoManager());
            CHECK_STATE(block);
            return block;
        } else {
            return nullptr;
        }
    } catch (ExitRequestedException &) { throw; } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}