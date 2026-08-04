/*
    Copyright (C) 2019- SKALE Labs

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
    @date 2019-
*/

#include "Log.h"
#include <exceptions/ConnectionRefusedException.h>
#include "exceptions/ExitRequestedException.h"


#include "abstracttcpserver/ConnectionStatus.h"

#include "chains/TestConfig.h"
#include "network/ClientSocket.h"
#include "network/IO.h"
#include "network/Network.h"
#include "node/Node.h"

#include "chains/Schain.h"
#include "crypto/TrivialSignature.h"
#include "datastructures/BlockProposalFragment.h"

#include "datastructures/CommittedBlock.h"
#include "datastructures/DAProof.h"
#include "db/BlockProposalDB.h"
#include "db/DAProofDB.h"
#include "exceptions/DoNotHaveProposalYetException.h"
#ifdef BITE
#include "db/TEDecryptionDB.h"
#endif
#include "headers/BlockFinalizeRequestHeader.h"
#include "headers/BlockProposalRequestHeader.h"
#include "monitoring/LivelinessMonitor.h"
#include "pendingqueue/PendingTransactionsAgent.h"
#include "utils/Time.h"
#include "chains/BlockErrorAnalyzer.h"
#include "BlockFinalizeDownloader.h"

#include <boost/endian/buffers.hpp>

#include "BlockFinalizeDownloaderThreadPool.h"
#include "bite/BiteManager.h"
#include "crypto/CryptoManager.h"


BlockFinalizeDownloader::BlockFinalizeDownloader(
    Schain *_sChain, block_id _blockId,
#ifdef  BITE
    epoch_id _epochId,
#endif
    schain_index _proposerIndex)
    : Agent(*_sChain, false, true),
      blockId(_blockId),
#ifdef  BITE
      epochId(_epochId),
#endif
      proposerIndex(_proposerIndex),
      fragmentList(_blockId, (uint64_t) _sChain->getNodeCount() - 1) {
#ifdef BITE
    needFragmentData = !getSchain()->haveProposal(_blockId, _proposerIndex);
#endif

    CHECK_ARGUMENT(_sChain)

    CHECK_STATE(_sChain->getNodeCount() > 1)

    try {
        CHECK_STATE(sChain)
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


nlohmann::json BlockFinalizeDownloader::readBlockFinalizeResponseHeader(
    const ptr<ClientSocket> &_socket) {
    MONITOR(__CLASS_NAME__, __FUNCTION__)
    CHECK_ARGUMENT(_socket)
    return getSchain()->getIo()->readJsonHeader(
        _socket->getDescriptor(), "Read BlockFinalize response", 10, _socket->getIP());
}


void BlockFinalizeDownloader::downloadFragment(
    schain_index _dstIndex, fragment_index _fragmentIndex) {
    CHECK_STATE(_fragmentIndex > 0);

    CONS_LOG(debug, "BLCK_FRG_DWNLD:" << to_string( _fragmentIndex ) << ":" << to_string( _dstIndex ));

    fragmentDownloadCounter++;

    if (fragmentDownloadCounter > getSchain()->getNodeCount() * 100) {
        // something is wrong, we are trying to download too many times
        CONS_LOG(err, "Fragment download got into infinite loop");
    }


    MONITOR(__CLASS_NAME__, __FUNCTION__)


    auto header = make_shared<BlockFinalizeRequestHeader>(
        *sChain, blockId,
#ifdef BITE
        epochId,
#endif

        proposerIndex, this->getNode()->getNodeID(), _fragmentIndex
#ifdef BITE
        , needDAProof()
        , needDecryptionShares(_dstIndex)
        , needFragmentData
#endif
    );
    CHECK_STATE(_dstIndex != ( uint64_t ) getSchain()->getSchainIndex())
    if (getSchain()->getDeathTimeMs((uint64_t) _dstIndex) + NODE_DEATH_INTERVAL_MS >
        Time::getCurrentTimeMs()) {
        BOOST_THROW_EXCEPTION(ConnectionRefusedException(
            "Dead node:" + to_string( _dstIndex ), 5, __CLASS_NAME__ ));
    }
    auto socket = make_shared<ClientSocket>(*sChain, _dstIndex, CATCHUP);

    auto io = getSchain()->getIo();

    try {
        io->writeMagic(socket);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException(
            "BlockFinalizec: Server disconnect sending magic", __CLASS_NAME__));
    }

    try {
        io->writeHeader(socket, header);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errString = "BlockFinalizec step 1: can not write BlockFinalize request";
        CONS_LOG(err, errString);
        throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
    }

    nlohmann::json response;

    try {
        response = readBlockFinalizeResponseHeader(socket);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errString = "BlockFinalizec step 2: can not read BlockFinalize response";
        CONS_LOG(err, errString);
        throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
    }


    auto status = (ConnectionStatus) Header::getUint64(response, "status");
    auto substatus = (ConnectionSubStatus) Header::getUint64(response, "substatus");

    if (status == CONNECTION_DISCONNECT && substatus == CONNECTION_FINALIZE_DONT_HAVE_PROPOSAL) {
        CONS_LOG(debug, "BLCK_FRG_DWNLD:NO_FRG:" << to_string( _fragmentIndex ) << ":"
            << to_string( _dstIndex ));
        throw DoNotHaveProposalYetException();
    }


    if (status != CONNECTION_PROCEED) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException(
            "Server error in BlockFinalize response:" + to_string( status ) + ":" +
            to_string(substatus), __CLASS_NAME__ ));
    }


    ptr<BlockProposalFragment> blockFragment;

    try {
        blockFragment =
                readBlockFragment(socket, response, _fragmentIndex, getSchain()->getNodeCount()
#ifdef BITE
                                  , proposerIndex, _dstIndex
#endif
                );
        CHECK_ARGUMENT(blockFragment)
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        auto errString = "BlockFinalizec step 3: can not read fragment";
        CONS_LOG(err, errString);
        throw_with_nested(NetworkProtocolException(errString, __CLASS_NAME__));
    }

#ifdef BITE
    auto teDB = getSchain()->getNode()->getTEDecryptionDB();
    if (needDecryptionShares(_dstIndex)) {
        auto decryptionShares = blockFragment->getDecryptionShares();
        CHECK_STATE2(decryptionShares, "The finalization response did not include decryptionshares");

        try {
            teDB->addDecryptionShares(decryptionShares);
        }
        CATCH_LOG_AND_RETHROW_ANY_EXCEPTION(err, "Could not add decryption shares to DB");
    }

    if (!needFragmentData) {
        return;
    }
#endif

    fragmentList.addFragment(blockFragment);
}

uint64_t BlockFinalizeDownloader::readFragmentSize(nlohmann::json _responseHeader) {
    uint64_t result = Header::getUint64(_responseHeader, "fragmentSize");

    if (result == 0) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException( "fragmentSize == 0", __CLASS_NAME__ ));
    }

    return result;
}

uint64_t BlockFinalizeDownloader::readBlockSize(nlohmann::json _responseHeader) {
    uint64_t result = Header::getUint64(_responseHeader, "blockSize");

    if (result == 0) {
        BOOST_THROW_EXCEPTION(NetworkProtocolException( "blockSize == 0", __CLASS_NAME__ ));
    }

    return result;
}

string BlockFinalizeDownloader::readBlockHash(nlohmann::json _responseHeader) {
    auto result = Header::getString(_responseHeader, "blockHash");
    return result;
}

string BlockFinalizeDownloader::readDAProofSig(nlohmann::json _responseHeader) {
    if (getSchain()->verifyDASigsPatch(getSchain()->getLastCommittedBlockTimeStamp().getS())) {
        return Header::getString(_responseHeader, "daSig");
    } else {
        return Header::maybeGetString(_responseHeader, "daSig");
    }
}


void BlockFinalizeDownloader::processDAProofSig(nlohmann::json _responseHeader, string h) {
    auto sig = readDAProofSig(_responseHeader); {
        LOCK(m)

        // if we did not received da sig yet, set it.
        if (!this->daSig && !sig.empty()) {
            auto blakeHash = BLAKE3Hash::fromHex(h);
            this->daSig = getSchain()->getCryptoManager()->verifyDAProofThresholdSig(
                blakeHash, sig, blockId, uint64_t(-1));
        }
    }
}

bool BlockFinalizeDownloader::needDAProof() {
    // we need DA proof sig if we did not download it yet
    return !getSchain()->haveDAProof(blockId, proposerIndex) && std::atomic_load(&daSig) == nullptr;
}


#ifdef BITE
bool BlockFinalizeDownloader::needDecryptionShares(schain_index _decryptorIndex) {
    auto teDB = getNode()->getTEDecryptionDB();
    return !teDB->isEnoughForeignShares(blockId) && !teDB->haveDecryptionShares(blockId, _decryptorIndex);
}
#endif


ptr<BlockProposalFragment> BlockFinalizeDownloader::readBlockFragment(
    const ptr<ClientSocket> &_socket, nlohmann::json _responseHeader,
    fragment_index _fragmentIndex, node_count _nodeCount
#ifdef BITE
    , schain_index _proposerIndex
    , schain_index _destinationIndex
#endif
) {
    CHECK_ARGUMENT(_socket)

    CHECK_ARGUMENT(_responseHeader > 0)

    MONITOR(__CLASS_NAME__, __FUNCTION__)

    auto fragmentSize = readFragmentSize(_responseHeader);
    auto blockSize = readBlockSize(_responseHeader);
    auto h = readBlockHash(_responseHeader);

    CHECK_STATE(!h.empty())
    
    {
    LOCK(m)
     
    // if we did not receive block hash yet, set it. Otherwise, compare it to the known hash
    if (this->blockHash.empty()) {
        this->blockHash = h;
    } else {
        if (this->blockHash != h) {
            getSchain()->addBlockErrorAnalyzer(make_shared<BlockErrorAnalyzer>());
            CHECK_STATE(h == blockHash);
        }
    }
    }

#ifdef BITE
    if (needDAProof()) {
#endif
        processDAProofSig(_responseHeader, h);
#ifdef BITE
    }
#endif

    auto serializedFragment = make_shared<vector<uint8_t> >(fragmentSize);

    try {
        getSchain()->getIo()->readBytes(
            _socket->getDescriptor(), serializedFragment, msg_len(fragmentSize), 30);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(NetworkProtocolException("Could not read blocks", __CLASS_NAME__));
    }

    ptr<BlockProposalFragment> fragment = nullptr;

    try {
        fragment = make_shared<BlockProposalFragment>(blockId,
#ifdef BITE
                                                      _proposerIndex,
                                                      _destinationIndex,
                                                      getSchain()->getBiteManager(),
#endif

                                                      (uint64_t) _nodeCount - 1,
                                                      _fragmentIndex, serializedFragment, blockSize, blockHash);
    } catch (ExitRequestedException &) {
        throw;
    } catch (...) {
        throw_with_nested(
            NetworkProtocolException("Could not parse block fragment", __CLASS_NAME__));
    }

    return fragment;
}

bool BlockFinalizeDownloader::exitDownloadLoop(uint64_t
#ifdef BITE
            _nextFragmentToDownload
#endif
) {
    if (downloadCompleted) {
        // we already completed the download and notified waiting threads
        return true;
    }

    if (completeAndNeedToExitAllThreads()) {
        // the downloader has completed its
        if (!downloadCompleted.exchange(true)) {
            downLoadCompletedBaton.post();
        }
        return true;
    };

#ifdef BITE
    // if this thread has no more work  but the the downloader completed its work
    // it means that other threads are still downloading their decryption shares
    // exit this thread without signalling that the download is completed
    if (_nextFragmentToDownload == 0) {
        return true;
    }
#endif

    return false;
}


void BlockFinalizeDownloader::waitAfterNetworkError() {
    // we have a refused connection
    // we will wait some time to try

    auto waitTimeAfterError = getNode()->getWaitAfterNetworkErrorMs();

    auto errorTime = Time::getCurrentTimeMs();

    while (Time::getCurrentTimeMs() < errorTime + waitTimeAfterError) {
        if (getNode()->isExitRequested()) {
            return;
        }

        if (downloadCompleted) {
            return;
        }
        usleep(static_cast<__useconds_t>(100 * 1000));
    }
}


void BlockFinalizeDownloader::waitAfterNoProposal() {
    // the peer does not have the proposal yet
    // we will wait 100 ms and try again
    usleep(static_cast<__useconds_t>(100 * 1000));
}


uint64_t BlockFinalizeDownloader::nextFragmentToDownload() {
#ifdef BITE
    if (!needFragmentData)
        return 0;
#endif
    return fragmentList.nextIndexToRetrieve();
}


uint64_t BlockFinalizeDownloader::computeFirstFragmentToDowload(schain_index _dstIndex, schain_index _mySchainIndex) {
    // since the node does not download from itself
    // and since the number of fragment is one less the number of
    // nodes, nodes that have sChainIndex more than current node, download _dstNodeIndex - 1
    // fragment
    if (_dstIndex > (uint64_t) _mySchainIndex) {
        return (uint64_t) _dstIndex - 1;
    } else {
        return (uint64_t) _dstIndex;
    }
}

void BlockFinalizeDownloader::workerThreadFragmentDownloadLoop(
    BlockFinalizeDownloader *_agent, schain_index _dstIndex) {
    CHECK_STATE(_agent)

    auto sChain = _agent->getSchain();
    auto node = sChain->getNode();
    auto proposalDB = node->getBlockProposalDB();
    auto daProofDB = node->getDaProofDB();
    auto mySchainIndex = sChain->getSchainIndex();


    logThreadLocal_ = node->getLog();
    setThreadName("BlckFinLoop", node->getConsensusEngine());

    node->waitOnGlobalClientStartBarrier();

    auto fragmentToDownload = computeFirstFragmentToDowload(_dstIndex, mySchainIndex);

    // we keep running the download loop until everything has been downloaded
    do {
        // if testFinalizationDownloadOnly is set to true we do full finalization
        try {
            _agent->downloadFragment(_dstIndex, fragmentToDownload);
            // we successfully downloaded the fragment
            // find out the next fragment to download
            fragmentToDownload = _agent->nextFragmentToDownload();
        } catch (DoNotHaveProposalYetException &) {
            // this is ok, we just do not have proposal yet on this destionation node
            // we keep trying to download the fragment until the node has the proposal
            _agent->waitAfterNoProposal();
        } catch (ExitRequestedException &) {
        } catch (ConnectionRefusedException &e) {
            // the node may be down. We will wait a little and try again
            _agent->logConnectionRefused(e, _dstIndex, __PRETTY_FUNCTION__);
            _agent->waitAfterNetworkError();
        } catch (exception &e) {
            CONS_LOG(err, "Error downloading fragment from:" + to_string(_dstIndex));
            // some unexpected error occured. We will wait a little and try again
            SkaleException::logNested(e);
            _agent->waitAfterNetworkError();
        } catch (...) {
            CONS_LOG(err, "Unknown error downloading fragment from:" + to_string(_dstIndex));
            _agent->waitAfterNetworkError();
        }
    } while (!_agent->exitDownloadLoop(fragmentToDownload));
}

void BlockFinalizeDownloader::joinAllThreads() {
    threadPool->joinAll();
}

bool BlockFinalizeDownloader::downloadProposalDAProofAndDecryptions() {
    MONITOR(__CLASS_NAME__, __FUNCTION__) {
        threadPool = make_shared<BlockFinalizeDownloaderThreadPool>(
            (uint64_t) getSchain()->getNodeCount(), this);
        threadPool->startService();
        // wait until the download is complete
        downLoadCompletedBaton.wait();
    }

    try {
        // first check if we do not need to do anything because a block separately arrived in catchup

        if (getSchain()->getLastCommittedBlockID() > blockId)
            return false;

        if (getSchain()->haveAllElementsToFinalizeBlock(blockId, proposerIndex)) {
            return true;
        }

        ptr<BlockProposal> proposal = getNode()->getBlockProposalDB()->getBlockProposal(blockId, proposerIndex);

        // now we need to recombine the fragment list
        if (!proposal && fragmentList.isComplete()) {
#ifdef BITE
            CHECK_STATE(getNode()->getTEDecryptionDB()->isEnoughForeignShares(blockId));
#endif
            // Already parses all BITE txs
            proposal = BlockProposal::makeFromNetworkSerialized(
                fragmentList.serialize(), getSchain()->getCryptoManager());

            CHECK_STATE(proposal)
            CHECK_STATE(proposal->getProposerIndex() == ( uint64_t ) proposerIndex);
            {
                LOCK(m);
                if (!this->blockHash.empty()) {
                    auto h = BLAKE3Hash::fromHex(blockHash);
                    CHECK_STATE2(proposal->getHash().compare( h ) == 0, "Incorrect block hash");
                }
            }


#ifdef BITE


            CHECK_STATE(proposal->getTransactionCiphertexts());

            auto biteManager = getSchain()->getBiteManager();
            biteManager->computeAndValidateSGXAESKeyBatch(proposal);

            CHECK_STATE2(proposal->getFailedTransactionsRef().empty(),
                         "Proposal includes invalid format BITE transactions");

            biteManager->scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(proposal);
#endif

            getNode()->getBlockProposalDB()->addBlockProposal(proposal);
        }

        if (getNode()->getDaProofDB()->getDASig(blockId, proposerIndex).empty()) {
            CHECK_STATE(daSig);
            auto daProof = make_shared<DAProof>(proposal, daSig);
            getNode()->getDaProofDB()->addDAProof(daProof);
        }
#ifdef BITE
        CHECK_STATE(getNode()->getTEDecryptionDB()->isEnoughForeignShares(blockId))
#endif
        return true;
    } catch (ExitRequestedException &) {
        throw;
    } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(InvalidStateException(__PRETTY_FUNCTION__, __CLASS_NAME__));
    }
}

BlockFinalizeDownloader::~BlockFinalizeDownloader() {
    joinAllThreads();
}

block_id BlockFinalizeDownloader::getBlockId() {
    return blockId;
}

schain_index BlockFinalizeDownloader::getProposerIndex() {
    return proposerIndex;
}

ptr<ThresholdSignature> BlockFinalizeDownloader::getDaSig(uint64_t _timeStampS) {
    if (getSchain()->verifyDASigsPatch(_timeStampS))
        CHECK_STATE2(daSig,
                 "BlockFinalizeDownloader: block did not include DA sig:" + to_string( _timeStampS ));

    if (daSig)
        return daSig;
    else
        return make_shared<TrivialSignature>(
            getBlockId(), getSchain()->getTotalSigners(), getSchain()->getRequiredSigners());
}

bool BlockFinalizeDownloader::isFragmentDownloadComplete() {
#ifdef BITE
    if (!needFragmentData) {
        return true;
    }
#endif
    return fragmentList.isComplete();
}

bool BlockFinalizeDownloader::completeAndNeedToExitAllThreads() {
    if (getSchain()->getNode()->isExitRequested()) {
        return true;
    }

    if (getSchain()->getLastCommittedBlockID() >= blockId) {
        // we received block concurrently through catchup
        return true;
    }
    if (getSchain()->haveAllElementsToFinalizeBlock(blockId, proposerIndex)) {
        // received needed things concurrently through block proposal
        return true;
    }

    // check if we downloaded everything needed
    if (isFragmentDownloadComplete() && !needDAProof()
#ifdef BITE
        && getNode()->getTEDecryptionDB()->isEnoughForeignShares(blockId)
#endif
    ) {
        return true;
    }

    return false;
}
