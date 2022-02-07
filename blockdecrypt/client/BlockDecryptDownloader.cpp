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
#include "datastructures/BlockAesKeyDecryptionSet.h"
#include "datastructures/BlockDecryptedAesKeys.h"
#include "datastructures/BlockEncryptedArguments.h"
#include "headers/BlockDecryptRequestHeader.h"
#include "monitoring/LivelinessMonitor.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "utils/Time.h"

#include "datastructures/BlockAesKeyDecryptionShare.h"
#include "datastructures/BlockEncryptedAesKeys.h"
#include "datastructures/BlockProposal.h"
#include "BlockDecryptDownloader.h"
#include "BlockDecryptDownloaderThreadPool.h"


BlockDecryptDownloader::BlockDecryptDownloader(Schain *_sChain, ptr<BlockProposal> _proposal)
        : Agent(*_sChain, false, true) {

    CHECK_STATE(_proposal);
    CHECK_STATE(_sChain);

    decryptionSet = make_shared<BlockAesKeyDecryptionSet>(_sChain, _proposal->getBlockID());
    auto encryptedArgs = _proposal->getEncryptedArguments(
            _sChain->getNode()->getEncryptedTransactionAnalyzer());

    encryptedKeys = encryptedArgs->getEncryptedAesKeys();

    blockId = _proposal->getBlockID();
    proposerIndex = _proposal->getProposerIndex();

    CHECK_STATE(encryptedKeys);
    CHECK_STATE(proposerIndex > 0);

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

    while (!node->isExitRequested() && !_agent->decryptionSet->isEnough()) {
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


    try {

        auto header = make_shared<BlockDecryptRequestHeader>(*sChain, blockId, proposerIndex,
                                                             this->getNode()->getNodeID(), (uint64_t) _dstIndex,
                                                             encryptedKeys->getEncryptedKeys());
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
                    NetworkProtocolException("BlockDecryptC: Server disconnect sending magic", __CLASS_NAME__));
        }

        try {
            io->writeHeader(socket, header);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockDecryptc step 1: can not write BlockDecrypt request";
            LOG(err, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }

        nlohmann::json response;

        try {
            response = readBlockDecryptResponseHeader(socket);
        } catch (ExitRequestedException &) { throw; } catch (...) {
            auto errString = "BlockDecryptc step 2: can not read BlockDecrypt response";
            LOG(err, errString);
            throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
        }


        auto status = (ConnectionStatus) Header::getUint64(response, "status");

        if (status == CONNECTION_DISCONNECT) {
            LOG(info, "BLCK_DECR_DWNLD:DISCONNECT:" + to_string(_dstIndex) + ":" +
                      to_string(_dstIndex));
            return false;
        }

        if (status != CONNECTION_SUCCESS) {
            BOOST_THROW_EXCEPTION(NetworkProtocolException(
                                          "Server error in BlockDecrypt response:" +
                                          to_string(status), __CLASS_NAME__ ));
        }


        LOG(info, "BLCK_DECR_DWNLD:SUCCESS:" + to_string(_dstIndex));

        auto decryptionShares = Header::getIntegerStringMap(response, "decryptionShares");

        auto decryptionShare = make_shared<BlockAesKeyDecryptionShare>(getBlockId(),
                getSchain()->getTotalSigners(), (te_share_index) (uint64_t ) _dstIndex,
                decryptionShares);

        CHECK_STATE(decryptionShare);

        this->decryptionSet->add(decryptionShare);

        return true;

    } catch (ExitRequestedException &e) { throw; } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }

}

ptr<BlockDecryptedAesKeys> BlockDecryptDownloader::downloadDecryptedKeys() {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    try {

        threadPool = make_shared<BlockDecryptDownloaderThreadPool>((uint64_t) getSchain()->getNodeCount(), this);
        threadPool->startService();
        threadPool->joinAll();

        if (this->decryptionSet->isEnough()) {
            // GLUE
        }

    } catch (ExitRequestedException &) { throw; } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
    }
}

