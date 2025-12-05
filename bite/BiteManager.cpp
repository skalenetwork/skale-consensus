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
#include <crypto/TransactionCiphertexts.h>
#include <crypto/ConsensusAESKeyDecryptionShare.h>
#include <crypto/ConsensusAESKeyDecryptionShareSet.h>
#include <crypto/CryptoManager.h>
#include <crypto/DecryptedAESKeyList.h>

#include "bite/BiteManager.h"
#include "bite/BiteEngine.h"
#include "bite/BiteCodec.h"

#include "datastructures/BlockProposal.h"
#include "datastructures/TransactionList.h"
#include "datastructures/TransactionCiphertextsMap.h"

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
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(NUM_BITE_VALIDATION_THREADS);
}

// =============== Stage 1: Ciphertext Parsing =============== //

void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {

    BiteRuntimeContext runtimeCtx {
        .currentEpoch = _proposal->getEpochID()
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
    CHECK_STATE(_proposal);

    ptr<TransactionCiphertextsMap> txsCiphertexts = _proposal->getTransactionCiphertexts();
    auto failedTransactionRef = _proposal->getFailedTransactionsRef();

    BiteEngine::CiphertextValidationResult validationResult = biteEngine.validateCiphertexts(*txsCiphertexts);

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

void BiteManager::callSGXToCreateMyDecryptionSharesForProposalTransactions(
        ptr<BlockProposal> _proposal) {
    MONITOR2(__CLASS_NAME__, __FUNCTION__, schain.getMaxExternalBlockProcessingTime());

    CHECK_STATE(_proposal);
    // check we are not verifying twice

    auto savedShares = getSchain()->getNode()->getTEDecryptionDB()->getMyDecryptionShares(_proposal->getBlockID(),
                                                                                          _proposal->getProposerIndex());

    if (savedShares) {
        // we already successfully parsed and decrypted shares
        _proposal->setMyDecryptionShares(savedShares);
        return;
    }

    auto transactions = _proposal->getTransactionList()->getItems();

    CHECK_STATE(transactions);

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }

    CHECK_STATE(_proposal->getTransactionCiphertexts());


    // this function will not throw exception
    auto decryptionShareList = getDecryptionSharesForProposal(_proposal);
    if (!_proposal->getFailedTransactionsRef().empty()) {
        // the block includes invalid transactions, and at this point we know
        // each of them. So we just return them
        return;
    }

    CHECK_STATE(decryptionShareList);
    CHECK_STATE(decryptionShareList->totalCiphertextSharesCount() == _proposal->getTransactionCiphertexts()->totalCiphertextCount());
    // no we know that the decryption shares are valid, we can set them to the proposal
    // now we set the decryption shares list to the block proposal so it is committed to the
    // database when proposal is committed
    _proposal->setMyDecryptionShares(decryptionShareList);

    getSchain()->getNode()->getTEDecryptionDB()->addMyDecryptionShares(decryptionShareList);
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
        TransactionList &_transactionList,
        DecryptedAESKeyList &_aesKeys) {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    BiteRuntimeContext runtimeCtx {
        .currentEpoch = schain.getNode()->getCurrentEpochId(),
        .threadPoolExecutor = threadPoolExecutor
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

        auto flatDecryptionShares = schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(*sgxAESKeyBatch);
    }

    return biteEngine.unflattenDecryptionShares(
        *ciphertexts,
        flattenedShares,
        _decryptorIndex
    );
}


ptr<AESKeyDecryptionShares> BiteManager::createAESDecryptionShares(
        const string& _aesKeyDecryptionShares, schain_index _decryptorIndex, bool _decryptionFailed) {
    auto shareStrs = BiteCodec::splitShares(_aesKeyDecryptionShares);
    return biteEngine.createDecryptionSharesObjects(shareStrs, _decryptorIndex, _decryptionFailed);
}

ptr<AESKeyDecryptionShareSet> BiteManager::createAESDecryptionShareSet(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) {
    return biteEngine.createAESDecryptionShareSetObject(
            _blockId, _transactionIndex, numberOfCiphertexts);
}


// =============== Test Encryption Calls =============== //

ptr<vector<uint8_t> > BiteManager::encryptRegularTx(const vector<uint8_t> &_data, const vector<uint8_t> &_to) {
    auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
    CHECK_STATE(primaryKey);
    auto blsKey = primaryKey->getPublicKey();
    libBLS::TEPublicKey teKey(blsKey);
    return std::make_shared<vector<uint8_t>>(biteEngine.buildRegularTxData(teKey, _data, _to));
}

#ifdef BITE2
ptr<vector<uint8_t> > BiteManager::generateEncryptedCATData() {
    static size_t numberOfCiphertexts = 2;

    // Keep ciphertext count between 2 and 5 (inclusive)
    numberOfCiphertexts++;
    if (numberOfCiphertexts % 6 == 0) {
        numberOfCiphertexts = 2;
    }

    auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
    CHECK_STATE(primaryKey);
    auto blsKey = primaryKey->getPublicKey();
    libBLS::TEPublicKey teKey(blsKey);

    return std::make_shared<vector<uint8_t>>(biteEngine.buildCATData(teKey, numberOfCiphertexts));
}
#endif
