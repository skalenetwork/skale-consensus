#include <flatbuffers/flatbuffers.h>
#include "Log.h"
#include "crypto/CryptoManager.h"
#include "bite/BiteManager.h"
#include "flatb/FlatBufferRequest.h"
#include "flatb/common_structures_generated.h"
#include "flatb/decryption_shares_generated.h"
#include "headers/BlockProposalHeader.h"
#include "crypto/AESKeyDecryptionShare.h"
#include "crypto/AESKeyDecryptionShareList.h"
#include "BiteAESDecryptionShareSerializer.h"



ptr<std::vector<uint8_t> > BiteAESDecryptionShareSerializer::serialize(
    ptr<AESKeyDecryptionShareList> _decryptionShareList) {
    CHECK_STATE(_decryptionShareList);

    // Preallocate ~1MB
    thread_local flatbuffers::FlatBufferBuilder builder(1024 * 1024);
    builder.Clear();

    // Serialize each DecryptionShare
    auto &decryptionShares = _decryptionShareList->getDecryptionShares();
    std::vector<flatbuffers::Offset<skale_fb::DecryptionShare> > decryptionShareVec;
    decryptionShareVec.reserve(decryptionShares.size());

    for (const auto &decryptionShare: decryptionShares) {
        uint32_t transactionIndex = (uint32_t) decryptionShare.first;
        const auto data = decryptionShare.second->toString(); // Assumes std::string or std::vector<uint8_t>
        auto dataOffset = builder.CreateVector(
            reinterpret_cast<const uint8_t *>(data.data()), data.size());

        decryptionShareVec.emplace_back(
            skale_fb::CreateDecryptionShare(builder, transactionIndex, dataOffset));
    }

    // Finalize the top-level table
    auto topOffset = skale_fb::CreateDecryptionShares(builder, (uint64_t) _decryptionShareList->getBlockId(),
                                                       (uint64_t) _decryptionShareList->getProposerIndex(),
                                                      (uint64_t) _decryptionShareList->getDecryptorIndex(),
                                                      builder.CreateVector(decryptionShareVec));
    builder.Finish(topOffset);

    // Copy into shared_ptr
    const uint8_t *raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    return std::make_shared<std::vector<uint8_t> >(raw, raw + size);
}


void BiteAESDecryptionShareSerializer::serializedSanityCheck(const ptr<vector<uint8_t> > &_serializedDecryptionShares) {
    // 🔍 Verify the resulting buffer before returning
    CHECK_STATE(_serializedDecryptionShares);
    flatbuffers::Verifier verifier(_serializedDecryptionShares->data(), _serializedDecryptionShares->size());
    CHECK_STATE(skale_fb::VerifyDecryptionSharesBuffer(verifier));
}


ptr<AESKeyDecryptionShareList> BiteAESDecryptionShareSerializer::deserialize(
    const ptr<vector<uint8_t> >  &_serializedDecryptionShares,
    const ptr<CryptoManager> &_manager, bool) {
    CHECK_ARGUMENT(_serializedDecryptionShares);
    CHECK_ARGUMENT(_manager);


    const skale_fb::DecryptionShares *fbDecryptionShares = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR(*_serializedDecryptionShares, DecryptionShares, fbDecryptionShares);

    block_id blockId = fbDecryptionShares->block_id();
    schain_index proposerIndex = fbDecryptionShares->proposer_index();
    schain_index decryptorIndex = fbDecryptionShares->decryptor_index();
    auto fbDecryptionSharesHandle = fbDecryptionShares->decryption_shares();

    CHECK_STATE(fbDecryptionSharesHandle);

    auto shares = make_shared<AESKeyDecryptionShareList>(blockId, proposerIndex, decryptorIndex);

    for (const auto *fbdecryptionShareHandle: *fbDecryptionSharesHandle) {
        CHECK_STATE(fbdecryptionShareHandle)
        auto rawData = fbdecryptionShareHandle->data()->data();
        CHECK_STATE(rawData);
        string decryptionShareStr(rawData, rawData + fbdecryptionShareHandle->data()->size());
        auto decryptionShare = BiteManager::createAESDecryptionShare(decryptionShareStr, decryptorIndex, fbdecryptionShareHandle->decryptionFailed());
        shares->addShare(fbdecryptionShareHandle->transaction_index(), decryptionShare);
    }

    shares->markComplete();

    return shares;

}
