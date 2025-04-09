#include <flatbuffers/flatbuffers.h>
#include "Log.h"
#include "crypto/DecryptedAESKeyList.h"
#include "bite/BiteManager.h"
#include "crypto/CryptoManager.h"

#include "flatb/FlatBufferRequest.h"
#include "flatb/common_structures_generated.h"
#include "flatb/committed_bloc_generated.h"
#include "headers/BlockProposalHeader.h"
#include "exceptions/ParsingException.h"
#include "network/Buffer.h"
#include "datastructures/Transaction.h"
#include "datastructures/TransactionList.h"
#include "headers/CommittedBlockHeader.h"
#include "BiteCommittedBlockSerializer.h"


ptr<std::vector<uint8_t> > BiteCommittedBlockSerializer::serializeTransactionsAndCompleteSerialization(
    ptr<BasicHeader> _blockHeader, ptr<TransactionList> transactionList,
    ptr<DecryptedAESKeyList> _decryptedAesKeyList, schain_index _proposerIndex) {

    static_assert(BITE_AES_KEY_LEN == 32, "FlatBuffer AesKey requires 32-byte AES keys");

    CHECK_STATE(_blockHeader);
    CHECK_STATE(transactionList);

    if (_proposerIndex > 0) {
        CHECK_STATE(_decryptedAesKeyList)
    } else {
        CHECK_STATE(!_decryptedAesKeyList)
        _decryptedAesKeyList = make_shared<DecryptedAESKeyList>();
    }


    auto buf = _blockHeader->toBuffer();
    CHECK_STATE(buf);

    auto *rawHeaderBuf = buf->getBuf()->data();
    auto headerSize = buf->getCounter();

    CHECK_STATE(rawHeaderBuf[sizeof(uint64_t)] == '{');
    CHECK_STATE(rawHeaderBuf[headerSize - 1] == '}');


    // Preallocate ~1MB (tune as needed)
    thread_local flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    builder.Clear();


    // Serialize each transaction
    auto &items = *transactionList->getItems();
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


    // no do keys

    // ---- Serialize AES Keys ----
    std::vector<skale_fb::AesKey> aesKeysVec;

    CHECK_STATE(_decryptedAesKeyList);


    for (auto &&it: _decryptedAesKeyList->getKeys()) {
        auto key = it.second;

        auto rawKey = key.getAesKey(); // std::array<uint8_t, BITE_AES_KEY_LEN>
        aesKeysVec.push_back(skale_fb::AesKey{
            static_cast<uint32_t>(it.first),
            rawKey // rawKey is std::array<uint8_t, 32>
        });
    }


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
                                                              const ptr<CryptoManager> &_manager, bool _verifySig) {
    CHECK_ARGUMENT(_serializedBlock);
    CHECK_ARGUMENT(_manager);


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

    auto list = std::make_shared<TransactionList>(transactions);


    ptr<CommittedBlock> block = nullptr;

    try {
        block = CommittedBlock::make(blockHeader->getSchainID(), blockHeader->getProposerNodeId(),
                                     blockHeader->getBlockID(), blockHeader->getProposerIndex(), list,
                                     blockHeader->getStateRoot(), blockHeader->getTimeStamp(),
                                     blockHeader->getTimeStampMs(),
                                     blockHeader->getSignature(), blockHeader->getThresholdSig(),
                                     blockHeader->getDaSig()
#ifdef BITE
                                     , make_shared<DecryptedAESKeyList>(), make_shared<DecryptedTransactions>()
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
            _manager->verifyProposalECDSA(
                block, blockHeader->getBlockHash(), blockHeader->getSignature());
        } catch (...) {
            LOG(err, "Block ECDSA signature did not verify in deserialization");
            throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__));
        }
    }

    try {
        block->verifyBlockSig(_manager);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__,
                                                __CLASS_NAME__ +
                                                string(
                                                    " Block threshold signature did not verify in deserialization")));
    }

    try {
        block->verifyDaSig(_manager);
    } catch (...) {
        throw_with_nested(InvalidStateException(__FUNCTION__,
                                                __CLASS_NAME__ + string(
                                                    " Block da signature did not verify in deserialization")));
    }

    return block;
}
