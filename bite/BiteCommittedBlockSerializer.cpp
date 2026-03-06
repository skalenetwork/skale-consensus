#include <flatbuffers/flatbuffers.h>
#include "Log.h"
#include "crypto/DecryptedAESKeyList.h"
#include "bite/BiteManager.h"
#include "crypto/CryptoManager.h"

#include "flatb/FlatBufferRequest.h"
#include "flatb/common_structures_generated.h"
#include "flatb/committed_bloc_generated.h"
#include "headers/BlockProposalHeader.h"
#include "bite/serde/BiteAESKeySerializer.h"
#include "exceptions/ParsingException.h"
#include "network/Buffer.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "headers/CommittedBlockHeader.h"
#include "BiteCommittedBlockSerializer.h"


ptr<std::vector<uint8_t> > BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
    BasicHeader& _blockHeader, const TransactionList& transactionList,
    const DecryptedAESKeyList& _decryptedAesKeyList) {
    static_assert(BITE_AES_KEY_LEN == 32, "FlatBuffer AesKey requires 32-byte AES keys");


    auto buf = _blockHeader.toBuffer();
    CHECK_STATE(buf);

    auto *rawHeaderBuf = buf->getBuf()->data();
    auto headerSize = buf->getCounter();

    CHECK_STATE(rawHeaderBuf[sizeof(uint64_t)] == '{');
    CHECK_STATE(rawHeaderBuf[headerSize - 1] == '}');


    // Preallocate ~1MB (tune as needed)
    thread_local flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    builder.Clear();


    // Serialize each transaction
    auto &items = *transactionList.getItems();
    std::vector<flatbuffers::Offset<skale_fb::Transaction> > transactionsVec;
    transactionsVec.reserve(items.size());

    for (const auto &tx: items) {
        const auto &txDataVec = *tx->getData();
        auto txData = builder.CreateVector(txDataVec.data(), txDataVec.size());
        transactionsVec.emplace_back(skale_fb::CreateTransaction(builder, txData));
    }

    // Serialize block header buffer directly. It starts with uint64_t size and then
    // includes the actual header. We do not need the size
    auto headerOffset = builder.CreateVector(
        buf->getBuf()->data() + sizeof(uint64_t), buf->getCounter() - sizeof(uint64_t));
    auto transactionsOffset = builder.CreateVector(transactionsVec);


    auto aesKeysVec = BiteAESKeySerializer::serialize(_decryptedAesKeyList);
    auto aesKeysOffset = builder.CreateVectorOfStructs(aesKeysVec);

    auto emptyVec = builder.CreateVector(std::vector<uint8_t>{});

    // Finalize block
    auto blockOffset = skale_fb::CreateCommittedBlock(
        builder, headerOffset, transactionsOffset, emptyVec, emptyVec, aesKeysOffset);

    builder.Finish(blockOffset);


    uint8_t *raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    // Slightly faster than resize+memcpy
    auto buffer = std::make_shared<std::vector<uint8_t> >(raw, raw + size);

    serializedSanityCheck(buffer);

    return buffer;
}


void BiteCommittedBlockSerializer::serializedSanityCheck(const ptr<vector<uint8_t> > &_serializedBlock) {
    // 🔍 Verify the resulting buffer before returning
    CHECK_STATE(_serializedBlock);
    flatbuffers::Verifier verifier(_serializedBlock->data(), _serializedBlock->size());
    CHECK_STATE(skale_fb::VerifyCommittedBlockBuffer(verifier));
}


ptr<CommittedBlock> BiteCommittedBlockSerializer::deserialize(const ptr<vector<uint8_t> > &_serializedBlock,
                                                              const ptr<CryptoManager> &_cryptoManager,
                                                              const ptr<BiteManager> _biteManager, bool _verifySig ) {
    CHECK_ARGUMENT(_serializedBlock);
    CHECK_ARGUMENT(_cryptoManager);
    CHECK_ARGUMENT(_biteManager);


    const skale_fb::CommittedBlock *fbBlock = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR(*_serializedBlock, CommittedBlock, fbBlock);

    // Extract block header data
    auto fbHeaderVec = fbBlock->block_header();
    CHECK_STATE(fbHeaderVec);
    size_t headerSize = fbHeaderVec->size();
    CHECK_STATE(fbHeaderVec->data())
    std::string_view headerView(reinterpret_cast<const char *>(fbHeaderVec->data()), headerSize);

    ptr<CommittedBlockHeader> blockHeader;

    try {
        blockHeader = CommittedBlock::parseBlockHeader(headerView);
        CHECK_STATE(blockHeader);
    } catch (...) {
        throw_with_nested(ParsingException(
            "Could not parse committed block header: \n" + string(headerView), __CLASS_NAME__));
    }


    auto fbTransactions = fbBlock->transactions();
    CHECK_STATE(fbTransactions);

    auto transactions = make_shared<vector<ptr<Transaction> > >();
    transactions->reserve(fbTransactions->size());

    for (const auto *tx: *fbTransactions) {
        CHECK_STATE(tx)
        auto rawData = tx->data()->data();
        CHECK_STATE(rawData);
        auto txData = make_shared<vector<uint8_t> >(rawData,
                                                    rawData + tx->data()->size());
        auto txObj = make_shared<Transaction>(txData, false); // hypothetical
        transactions->push_back(txObj);
    }

    auto transactionList = std::make_shared<TransactionList>(transactions);

    // now deserialize AES keys

    ptr<DecryptedAESKeyList> decryptedAesKeyList = make_shared<DecryptedAESKeyList>();

    auto fbAesKeys = fbBlock->aes_keys();

    CHECK_STATE(fbAesKeys)

    if (fbAesKeys) {
        BiteAESKeySerializer::deserialize(fbAesKeys, *decryptedAesKeyList);
    }

#ifdef BITE2
    // Committed block format is self-describing for BITE2:
    // reencryption signature is present only for BITE2-format committed blocks.
    // FOr such block to have been comitted, it must have been after bite2PatchTimestamp condition
    bool isBite2PatchEnabledForBlock = blockHeader->getReencryptionThresholdSig().has_value();
#endif

    auto decryptedTransactionDataFields = _biteManager->verifyAndDecryptTransactionList(
        *transactionList, *decryptedAesKeyList, blockHeader->getEpochID()
#ifdef BITE2
        , isBite2PatchEnabledForBlock 
#endif
    );
    
    ptr<CommittedBlock> block = nullptr;

    try {
        block = CommittedBlock::make(blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
                                     blockHeader->getBlockID(),
#ifdef BITE
                                     blockHeader->getEpochID(),
#endif

                                     blockHeader->getProposerIndex(), transactionList,
                                     blockHeader->getStateRoot(), blockHeader->getTimeStamp(),
                                     blockHeader->getTimeStampMs(),
                                     blockHeader->getSignature(), blockHeader->getThresholdSig(),
#ifdef BITE2
                                     blockHeader->getReencryptionThresholdSig(),
#endif
                                     blockHeader->getDaSig()
#ifdef BITE
                                     , decryptedAesKeyList, decryptedTransactionDataFields
#endif
        );
    } catch (...) {
        throw_with_nested(InvalidStateException("Could not make block", __CLASS_NAME__));
    }

    block->setCachedSerializedBlock(_serializedBlock);

    CHECK_STATE(block);

    if (!_verifySig) {
        return block;
    }

    // now verify block proposer signature and block signature
    // default blocks are not ecdsa signed
    if ((blockHeader->getProposerIndex() != 0)) {
        try {
            _cryptoManager->verifyProposalECDSA(
                block, blockHeader->getBlockHash(), blockHeader->getSignature());
        } catch (...) {
            CONS_LOG(err, "Block ECDSA signature did not verify in deserialization");
            throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
        }
    }

    try {
        block->verifyBlockSig(_cryptoManager);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__,
                                                __CLASS_NAME__ +
                                                string(
                                                    " Block threshold signature did not verify in deserialization")));
    }

    try {
        block->verifyDaSig(_cryptoManager);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__,
                                                __CLASS_NAME__ + string(
                                                    " Block da signature did not verify in deserialization")));
    }

    return block;
}
