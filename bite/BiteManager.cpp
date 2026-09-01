#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include <vector>

#include "Log.h"
#include <chains/Schain.h>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include <crypto/AESKeyDecryptionShareList.h>
#include <crypto/MockupAESKeyDecryptionShareSet.h>
#include "bite/crypto/TransactionCiphertexts.h"
#include <crypto/ConsensusAESKeyDecryptionShare.h>
#include <crypto/ConsensusAESKeyDecryptionShareSet.h>
#include <crypto/CryptoManager.h>
#include <crypto/DecryptedAESKeyList.h>
#include "libBLS/threshold_encryption/ThresholdEncryption.h"

#include "bite/BiteManager.h"
#include "bite/BiteEngine.h"
#include "bite/BiteCodec.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"
#include "datastructures/TransactionCiphertextsMap.h"

#include <exceptions/ExitRequestedException.h>
#include <exceptions/SkaleException.h>
#include <monitoring/LivelinessMonitor.h>

#include "db/TEDecryptionDB.h"

#include "libBLS/bls/BLSPublicKey.h"
BiteManager::BiteManager(Schain& _schain)
  : schain(_schain), 
    biteEngine(
            BiteCore{_schain.getNode()->verifyRealSignatures() },
            BiteConfig{
                _schain.getRequiredSigners(), 
                _schain.getTotalSigners(),
                _schain.getNode()->isSgxEnabled()
            }
        )
{
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(getNumBiteValidationThreads());
}

BiteManager::~BiteManager() {
    stopAndDestroyThreadPoolExecutor();
}

// =============== Stage 1: Ciphertext Parsing =============== //
void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {

    BiteRuntimeContext runtimeCtx {
        .currentEpoch = schain.getNode()->getCurrentEpochId()
        , .isBite2PatchEnabled = schain.bite2Patch( schain.getLastCommittedBlockTimeStamp().getS() )
    };

    auto result = BiteEngine::parseAndCacheBITETransactions(*_proposal->getTransactionList(), 
                                                runtimeCtx);

    for (auto failedTxIdx : result.failedTransactions) {
        _proposal->getFailedTransactionsRef().emplace(failedTxIdx, 
            ConnectionSubStatus::CONNECTION_ERROR_CANT_PARSE_PROPOSAL_TRANSACTION);
    }

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }

    auto txsCiphertexts = make_shared<TransactionCiphertextsMap>(std::move(result.txsCiphertexts));
    _proposal->setTransactionCiphertexts(txsCiphertexts);
}

// =============== Stage 2: Ciphertext Validation =============== //

void BiteManager::computeAndValidateSGXAESKeyBatch(ptr<BlockProposal> _proposal) {
    if (!biteEngine.usingRealCrypto()) {
        return;
    }

    CHECK_STATE(_proposal);

    ptr<TransactionCiphertextsMap> txsCiphertexts = _proposal->getTransactionCiphertexts();
    auto& failedTransactionRef = _proposal->getFailedTransactionsRef();

    BiteRuntimeContext biteCtx;
    biteCtx.threadPoolExecutor = threadPoolExecutor;
    BiteEngine::CiphertextValidationResult validationResult =
        biteEngine.validateCiphertexts(*txsCiphertexts, biteCtx);

    if (!validationResult.allValid()) {
        for (auto invalidIdx : validationResult.invalidCiphertextIndices) {
            CONS_LOG(err, fmt::format("AES key encryption validation failed for transaction {}", (uint32_t)invalidIdx));
            failedTransactionRef.emplace(invalidIdx,
                                        ConnectionSubStatus::CONNECTION_ERROR_INVALID_AES_KEY_ENCRYPTION_IN_PROPOSAL_TRANSACTION);
        }
        return;
    }

    auto sgxAESKeyBatch = std::make_shared<std::vector<std::string>>(std::move(validationResult.publicDecryptionValues));
    _proposal->setSGXAESKeyBatch(sgxAESKeyBatch);
}

// =============== Stage 3: Compute Ciphertext shares =============== //

void BiteManager::ensureMyDecryptionSharesAreComputed(
        ptr<BlockProposal> _proposal) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime());

    CHECK_STATE(_proposal);
    if (!_proposal->tryBeginMyDecryptionSharesComputation()) {
        _proposal->waitUntilMyDecryptionSharesResolved();
        return;
    }

    computeOrLoadMyDecryptionShares(_proposal);
}

void BiteManager::scheduleSGXToCreateMyDecryptionSharesForProposalTransactions(
        ptr<BlockProposal> _proposal) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime());

    CHECK_STATE(_proposal);
    if (!_proposal->tryBeginMyDecryptionSharesComputation()) {
        return;
    }

    try {
        threadPoolExecutor->add([this, _proposal]() {
            logThreadLocal_ = schain.getNode()->getLog();
            computeOrLoadMyDecryptionShares(_proposal);
        });
    } catch (const std::exception &e) {
        CONS_LOG(err, "Could not schedule local decryption share computation: " << e.what());
        _proposal->markMyDecryptionSharesFailed();
    } catch (...) {
        CONS_LOG(err, "Could not schedule local decryption share computation");
        _proposal->markMyDecryptionSharesFailed();
    }
}

void BiteManager::computeOrLoadMyDecryptionShares(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal);

    try {
        auto savedShares = getSchain()->getNode()->getTEDecryptionDB()->getMyDecryptionShares(
            _proposal->getBlockID(), _proposal->getProposerIndex());

        if (savedShares) {
            _proposal->markMyDecryptionSharesReady(savedShares);
            return;
        }

        auto transactions = _proposal->getTransactionList()->getItems();

        CHECK_STATE(transactions);

        if (!_proposal->getFailedTransactionsRef().empty()) {
            _proposal->markMyDecryptionSharesFailed();
            return;
        }

        CHECK_STATE(_proposal->getTransactionCiphertexts());

        auto decryptionShareList = getDecryptionSharesForProposal(_proposal);
        if (!_proposal->getFailedTransactionsRef().empty() || !decryptionShareList) {
            _proposal->markMyDecryptionSharesFailed();
            return;
        }

        CHECK_STATE(
            decryptionShareList->totalCiphertextSharesCount() ==
            _proposal->getTransactionCiphertexts()->totalCiphertextCount());

        getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);
        _proposal->markMyDecryptionSharesReady(decryptionShareList);
    } catch (const ExitRequestedException &) {
        _proposal->markMyDecryptionSharesFailed();
    } catch (const std::exception &e) {
        CONS_LOG(err, "Could not compute local decryption shares for proposal "
                          << _proposal->getBlockID() << ":" << _proposal->getProposerIndex()
                          << ": " << e.what());
        SkaleException::logNested(e);
        _proposal->markMyDecryptionSharesFailed();
    } catch (...) {
        CONS_LOG(err, "Could not compute local decryption shares for proposal "
                          << _proposal->getBlockID() << ":" << _proposal->getProposerIndex());
        _proposal->markMyDecryptionSharesFailed();
    }
}


// =============== Stage 4: Share merging =============== //

std::shared_ptr<DecryptedAESKeyList> BiteManager::mergeAESKeys(
    block_id _blockId,
    TransactionCiphertextsMap& _txCiphertexts,
    const std::map<schain_index, std::shared_ptr<AESKeyDecryptionShareList>>& _decryptionShareMap,
    const std::vector<libBLS::TEPublicKeyShare>& _tePublicKeyShares,
    const BiteRuntimeContext& _runtimeCtx
) const {
    return biteEngine.mergeAESKeys(
        _blockId,
        _txCiphertexts,
        _decryptionShareMap,
        _tePublicKeyShares,
        _runtimeCtx
    );
}

// =============== Stage 5: Transaction Decryption =============== //

DecryptedTransactions BiteManager::verifyAndDecryptTransactionList(
        const TransactionList &_transactionList,
        const DecryptedAESKeyList &_aesKeys,
        epoch_id _epochId,
        bool _isBite2PatchEnabledForBlock
    ) {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    BiteRuntimeContext runtimeCtx {
        .currentEpoch = _epochId,
        .threadPoolExecutor = threadPoolExecutor, 
        .isBite2PatchEnabled = _isBite2PatchEnabledForBlock
    };

    return biteEngine.decryptTransactionsListInParallel(
            _transactionList,
            _aesKeys,
            runtimeCtx
    );
}


// =============== Helpers =============== //

ptr<AESKeyDecryptionShareList> BiteManager::getDecryptionSharesForProposal(ptr<BlockProposal> _proposal) {
    CHECK_STATE(_proposal)

    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    auto decryptionShareList = make_shared<AESKeyDecryptionShareList>(
            _proposal->getBlockID(),
            _proposal->getProposerIndex(), schain.getSchainIndex());

    ptr<vector<ptr<AESKeyDecryptionShares> > > decryptionSharesVector = getDecryptionSharesFromAESKeys(
            _proposal, schain.getSchainIndex());

    if (!decryptionSharesVector) {
        return nullptr;
    }

    CHECK_STATE(decryptionSharesVector->size() == _proposal->getTransactionCiphertexts()->size());

    auto arrayIndex = 0;
    for (auto &&[txIdx, ciphertexts]: *_proposal->getTransactionCiphertexts()) {
        auto AESKeyDecryptionShare = (*decryptionSharesVector)[arrayIndex];
        decryptionShareList->addShares(txIdx, decryptionSharesVector->at(arrayIndex));
        arrayIndex++;
    }

    return decryptionShareList;
}



ptr<vector<ptr<AESKeyDecryptionShares> > > BiteManager::getDecryptionSharesFromAESKeys(
        ptr<BlockProposal> _proposal, schain_index _decryptorIndex) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime())

    CHECK_STATE(_proposal);

    auto ciphertexts = _proposal->getTransactionCiphertexts();
    CHECK_STATE(ciphertexts);

    ptr<vector<ptr<AESKeyDecryptionShare> > > flattenedShares;

    if (biteEngine.usingRealCrypto()) {

        // flatten out vec
        auto sgxAESKeyBatch = _proposal->getSGXAESKeyBatch();

        CHECK_STATE(sgxAESKeyBatch);
        CHECK_STATE(sgxAESKeyBatch->size() == ciphertexts->totalCiphertextCount());

        flattenedShares = schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(*sgxAESKeyBatch);
    }

    return biteEngine.unflattenDecryptionShares(
        *ciphertexts,
        flattenedShares,
        _decryptorIndex
    );
}


ptr<AESKeyDecryptionShares> BiteManager::createAESDecryptionShares(
        const string& _aesKeyDecryptionShares, schain_index _decryptorIndex, bool _decryptionFailed, CryptographicValidationMode _validationMode ) {
    auto shareStrs = BiteCodec::splitShares(_aesKeyDecryptionShares);
    return biteEngine.createDecryptionSharesObjects(shareStrs, _decryptorIndex, _decryptionFailed, _validationMode);
}

ptr<AESKeyDecryptionShareSet> BiteManager::createAESDecryptionShareSet(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) {
    return biteEngine.createAESDecryptionShareSetObject(
            _blockId, _transactionIndex, numberOfCiphertexts);
}


// =============== Test Encryption Calls =============== //

ptr<vector<uint8_t> > BiteManager::encryptRegularTx(const vector<uint8_t> &_data, const vector<uint8_t> &_to, uint64_t epochId) {
    if (biteEngine.usingRealCrypto()) {
        auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
        CHECK_STATE(primaryKey);
        auto blsKey = primaryKey->getPublicKey();
        libBLS::TEPublicKey teKey(blsKey);
        return std::make_shared<vector<uint8_t>>(biteEngine.buildRegularTxData(teKey, _data, _to, epochId));
    }

    // mock path: avoid SGX keys and use mockup encrypt + epoch wrapping
    auto payload = BiteCodec::encodeRegularTxPayload(_data, _to);
    auto cipher = libBLS::ThresholdEncryption::mockupEncrypt(payload);
    auto serialized = BiteCodec::encodeEpochedBiteData(cipher, epochId);
    return std::make_shared<vector<uint8_t>>(std::move(serialized));

}

ptr<vector<uint8_t>> BiteManager::generateEncryptedCTXData(uint64_t epochId, const std::optional<AddressBytes>& scAddressAadTE) {
    constexpr size_t numberOfCiphertexts = 2;

    if (biteEngine.usingRealCrypto()) {
        auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
        CHECK_STATE(primaryKey);
        auto blsKey = primaryKey->getPublicKey();
        libBLS::TEPublicKey teKey(blsKey);
        return std::make_shared<vector<uint8_t>>(biteEngine.buildCTXData(teKey, numberOfCiphertexts, epochId, scAddressAadTE));
    }

    // mock path: build ciphertexts using mockup encryption + epoch wrapping
    // Note: mockup path does not use AAD since mockupEncrypt doesn't support it
    std::vector<std::vector<uint8_t>> encryptedSerializedArgs;
    encryptedSerializedArgs.reserve(numberOfCiphertexts);
    for (size_t i = 0; i < numberOfCiphertexts; ++i) {
        std::vector<uint8_t> rndData(numberOfCiphertexts * 10);
        auto cipher = libBLS::ThresholdEncryption::mockupEncrypt(rndData);
        encryptedSerializedArgs.push_back(BiteCodec::encodeEpochedBiteData(cipher, epochId));
    }

    std::vector<std::vector<uint8_t>> plainArgs;
    const size_t numPlaintexts = numberOfCiphertexts - 1;
    plainArgs.reserve(numPlaintexts);
    for (size_t i = 0; i < numPlaintexts; ++i) {
        plainArgs.emplace_back(numberOfCiphertexts * 5);
    }

    return std::make_shared<vector<uint8_t>>(BiteCodec::encodeCTXData(encryptedSerializedArgs, plainArgs));
}

ptr<vector<uint8_t> > BiteManager::generateEmptyCTXData(uint64_t epochId) {
    (void)epochId;  // epochId not needed since there are no ciphertexts
    
    // No encrypted arguments (empty ciphertexts)
    std::vector<std::vector<uint8_t>> encryptedSerializedArgs;
    
    // Some plain arguments to include in the CTX
    std::vector<std::vector<uint8_t>> plainArgs;
    plainArgs.emplace_back(std::vector<uint8_t>{0x01, 0x02, 0x03});
    plainArgs.emplace_back(std::vector<uint8_t>{0x04, 0x05});
    
    return std::make_shared<vector<uint8_t>>(BiteCodec::encodeCTXData(encryptedSerializedArgs, plainArgs));
}

void BiteManager::stopAndDestroyThreadPoolExecutor() {
    if ( !threadPoolExecutor )
        return;
    threadPoolExecutor->stop();
    threadPoolExecutor->join();
}
