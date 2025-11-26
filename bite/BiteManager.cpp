#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

#include "Log.h"
#include <chains/Schain.h>
#include <crypto/AESKeyDecryptionShare.h>
#include <crypto/AESKeyDecryptionShareSet.h>
#include "crypto/MockupAESKeyDecryptionShare.h"
#include <crypto/AESKeyDecryptionShareList.h>
#include <crypto/MockupAESKeyDecryptionShareSet.h>
#include <crypto/TransactionCiphertexts.h>
#include <algorithm>
#include <string_view>
#include <vector>

#include "BiteCiphertext.h"
#include "BiteEngine.h"
#include "datastructures/BlockProposal.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "datastructures/TransactionCiphertextsMap.h"

#include "rlp/ParsedEthTransaction.h"

#include "BiteManager.h"
#include "node/ConsensusInterface.h"

#include <crypto/ConsensusAESKeyDecryptionShare.h>
#include <crypto/ConsensusAESKeyDecryptionShareSet.h>
#include <crypto/CryptoManager.h>
#include <crypto/DecryptedAESKeyList.h>
#include <monitoring/LivelinessMonitor.h>

#include "BLSPublicKey.h"
#include "db/TEDecryptionDB.h"
#include "rlp/RLPStream.h"
#include "bite/BiteCodec.h"

BiteManager::BiteManager(Schain& _schain)
  : biteEngine(
        BiteCore{_schain.getNode()->verifyRealSignatures() },
        BiteConfig{_schain.getRequiredSigners(), _schain.getTotalSigners()}
    ), schain(_schain)
{
    threadPoolExecutor = std::make_shared<folly::CPUThreadPoolExecutor>(NUM_BITE_VALIDATION_THREADS);
}



void BiteManager::parseBITETransactions(
    ptr<BlockProposal> _proposal) {

    BiteRuntimeContext runtimeCtx {
        .currentEpoch = _proposal->getEpochID()
    };

    auto result = biteEngine.parseAndCacheBITETransactions(*_proposal->getTransactionList(), 
                                                runtimeCtx);

    for (auto failedTxIdx : result.failedTransactions) {
        _proposal->getFailedTransactionsRef().emplace(std::move(failedTxIdx));
    }

    if (!_proposal->getFailedTransactionsRef().empty()) {
        return;
    }

    auto txsCiphertexts = make_shared<TransactionCiphertextsMap>(std::move(result.txsCiphertexts));
    _proposal->setTransactionCiphertexts(txsCiphertexts);
}

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

    auto shares = make_shared<vector<ptr<AESKeyDecryptionShares> > >();
    shares->reserve(ciphertexts->size()); // for number of txs

    // define how to add share depending on real or mockup crypto
    // EncryptedAESKey is the current ciphertext being processed (may be multiple per tx)
    // size_t is the global ciphertext index
    std::function<void(ptr<AESKeyDecryptionShares>&, EncryptedAESKey&, size_t)> addShareForCurrentTx;

    if (doRealCrypto) {
        // flatten out vec
        auto sgxAESKeyBatch = _proposal->getSGXAESKeyBatch();

        CHECK_STATE(sgxAESKeyBatch);
        CHECK_STATE(sgxAESKeyBatch->size() == ciphertexts->totalCiphertextCount());

        auto flatDecryptionShares = schain.getCryptoManager()->sgxDecryptAESKeyShareBatch(*sgxAESKeyBatch);
        addShareForCurrentTx = [flatDecryptionShares](ptr<AESKeyDecryptionShares>& decryptSharesForTx, EncryptedAESKey&, size_t globalCiphertextIdxForCurrTx) {
            decryptSharesForTx->push_back(flatDecryptionShares->at(globalCiphertextIdxForCurrTx));
        };
    } else {
        addShareForCurrentTx = [_decryptorIndex](ptr<AESKeyDecryptionShares>& decryptSharesForTx, EncryptedAESKey& ciphertext, size_t) {
            decryptSharesForTx->push_back(MockupAESKeyDecryptionShare::mockupDecrypt(ciphertext, _decryptorIndex));
        }; 
    }

    // unflatten the shares into per-transaction vectors
    size_t globalCiphertextIdx = 0;
    for (auto && [idx, txCiphertexts]: *ciphertexts) { // for each tx
        auto decryptSharesForTx = make_shared<AESKeyDecryptionShares>();
        for (size_t i = 0; i < txCiphertexts->count(); ++i) { // for each ciphertext within the tx
            auto currentCiphertextWithinTx = (*txCiphertexts)[i];
            addShareForCurrentTx(decryptSharesForTx, currentCiphertextWithinTx, globalCiphertextIdx);
            globalCiphertextIdx++;
        }
        shares->push_back(decryptSharesForTx);
    }

    return shares;
}

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



DecryptedTransactions BiteManager::verifyAndDecryptTransactionList(
        TransactionList &_transactionList,
        DecryptedAESKeyList &_aesKeys) {

    MONITOR(__CLASS_NAME__, __FUNCTION__);

    return decryptTransactionsListInParallel(
            _transactionList,
            _aesKeys,
            schain.getNode()->getCurrentEpochId(),
            threadPoolExecutor,
            doRealCrypto
    );
}

ptr<vector<uint8_t> > BiteManager::encryptRegularTx(const vector<uint8_t> &_data, const vector<uint8_t> &_to) {
    auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
    CHECK_STATE(primaryKey);
    auto blsKey = primaryKey->getPublicKey();
    libBLS::TEPublicKey teKey(blsKey);
    return std::make_shared<vector<uint8_t>>(BiteCodec::buildRegularTxData(teKey, _data, _to, doRealCrypto));
}

#ifdef BITE2
ptr<vector<uint8_t> > BiteManager::generateEncryptedCATData() {
    static size_t numberOfCiphertexts = 2;

    // Keep ciphertext count between 3 and 5 (inclusive)
    numberOfCiphertexts++;
    if (numberOfCiphertexts % 6 == 0) {
        numberOfCiphertexts = 2;
    }

    auto [primaryKey, secondaryKey] = schain.getCryptoManager()->getSgxBlsPublicKey();
    CHECK_STATE(primaryKey);
    auto blsKey = primaryKey->getPublicKey();
    libBLS::TEPublicKey teKey(blsKey);

    return std::make_shared<vector<uint8_t>>(BiteCodec::buildCATData(teKey, numberOfCiphertexts, doRealCrypto));
}
#endif


// Helper function to split string_view by commas
std::vector<std::string_view> splitByComma(std::string_view s) {
    std::vector<std::string_view> out;

    while (!s.empty()) {
        size_t pos = s.find(',');
        if (pos == std::string_view::npos) {
            out.push_back(s);
            break;
        }
        out.push_back(s.substr(0, pos));
        s.remove_prefix(pos + 1);
    }

    return out;
}


ptr<AESKeyDecryptionShares> BiteManager::createAESDecryptionShares(
        const string& _aesKeyDecryptionShares, schain_index _decryptorIndex, bool _decryptionFailed) {
    
    ptr<AESKeyDecryptionShares> decryptionShares = make_shared<AESKeyDecryptionShares>();
    auto decryptionSharesStrs = splitByComma(_aesKeyDecryptionShares);
    for (const auto& shareStr : decryptionSharesStrs) {
        std::string shareString(shareStr);
        if (doRealCrypto) {
            decryptionShares->push_back(
                make_shared<ConsensusAESKeyDecryptionShare>(
                    shareString, _decryptorIndex, _decryptionFailed));
        } else {
            decryptionShares->push_back(
                make_shared<MockupAESKeyDecryptionShare>(
                    shareString, _decryptorIndex, _decryptionFailed));
        }
    }
    return decryptionShares;
}


ptr<AESKeyDecryptionShareSet> BiteManager::createAESDecryptionShareSet(
        block_id _blockId, transaction_index _transactionIndex, size_t numberOfCiphertexts) {
    if (doRealCrypto) {
        return make_shared<ConsensusAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, numberOfCiphertexts, schain.getTotalSigners(), schain.getRequiredSigners());
    } else {
        return make_shared<MockupAESKeyDecryptionShareSet>(
                _blockId, _transactionIndex, schain.getTotalSigners(), schain.getRequiredSigners());
    }
}

bool BiteManager::isRealCryptoEnabled() const {
    return doRealCrypto;
}
