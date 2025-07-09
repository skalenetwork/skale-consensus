/*
* Created by stan on 15-03-2025.
 */

#include <iostream>
#include <array>
#include <folly/io/IOBuf.h>
#include "block_finalize_response_generated.h"
#undef LOG // avoid conflict with folly
#include "SkaleCommon.h"
#include "Log.h"
#include "FlatBufferRequest.h"
#include "BlockFinalizeRequestObject.h"
#include "BlockFinalizeResponseObject.h"
#include "BlockFragmentObject.h"
#include "BlockHeaderObject.h"
#include "DecryptionShareObject.h"


using namespace skale_fb;

BlockFinalizeResponseObject::BlockFinalizeResponseObject(std::shared_ptr<BlockHeaderObject> &_blockHeader,
                                                         std::shared_ptr<vector<uint8_t> > &_blockSig,
                                                         std::shared_ptr<vector<uint8_t> > &_daProofSig,
                                                         std::shared_ptr<BlockFragmentObject> &_blockFragment,
                                                         std::shared_ptr<std::vector<ptr<DecryptionShareObject> > > &
                                                         _decryptionShares): blockHeader(_blockHeader),
                                                                             blockSig(_blockSig),
                                                                             daProofSig(_daProofSig),
                                                                             blockFragment(_blockFragment),
                                                                             decryptionShares(_decryptionShares) {
    CHECK_STATE2(_blockHeader, "Null block header in response");
    CHECK_STATE2(_blockSig, "Null block sig in response");
}

ptr<BlockFinalizeResponseObject> BlockFinalizeResponseObject::deserializeAndVerify(
    const folly::IOBuf &, ptr<BlockFinalizeRequestObject> &_request) {
    CHECK_STATE(_request);

    /*

    const BlockFinalizeResponse *response = nullptr;
    VERIFY_AND_PARSE_FLATBUFFER(_buffer, BlockFinalizeResponse, response);


    auto blockHeader = BlockHeaderObject::deserializeAndVerify(response->json_header());

    CHECK_STATE(blockHeader);

    auto blockSig = FlatBufferRequest::copyFbByteVector(response->block_sig());
    CHECK_STATE2(blockSig, "No blocksig in response");

    auto daProofSig = FlatBufferRequest::copyFbByteVector(response->da_proof_sig());
    CHECK_STATE2(daProofSig || !_request->getNeedDaProofSig(), "No requested da proof sig in response");


    auto blockFragment = BlockFragmentObject::deserializeAndVerify(response->block_fragment());
    CHECK_STATE2(blockFragment || !_request->getNeedFragment(), "No requested block fragment in response");


    auto fbDecryptionShares = response->decryption_shares();
    CHECK_STATE(fbDecryptionShares);

    auto encryptedTransactionsCount = blockHeader->getEncryptedTransactionIndices()->size();
    auto decryptionShares = make_shared<vector<ptr<DecryptionShareObject> > >();
    decryptionShares->reserve(encryptedTransactionsCount);
    CHECK_STATE2(fbDecryptionShares->size() == encryptedTransactionsCount,
                 "Decryption shares count does not match encrypted transactions count");


    for (auto &&fbShare: *fbDecryptionShares) {
        CHECK_STATE(fbShare);
        auto decryptionShare = DecryptionShareObject::deserializeAndVerify(fbShare);
        CHECK_STATE(decryptionShare);
        if (decryptionShares->size() > 0) {
            CHECK_STATE2(decryptionShare->getTransactionIndex() > decryptionShares->back()->getTransactionIndex(),
                         "Decryption shares in unsorted order");
        }
        decryptionShares->push_back(decryptionShare);
    }

    CHECK_STATE(decryptionShares->size() == encryptedTransactionsCount);

    for (uint64_t i = 0; i < encryptedTransactionsCount; i++) {
        CHECK_STATE2(blockHeader->getEncryptedTransactionIndices()->at(i) ==
                     decryptionShares->at(i)->getTransactionIndex(), "Decryption shares contain transaction index not "
                     "included in the header: " +
                     to_string(decryptionShares->at(i)->getTransactionIndex()));
    }



    return make_shared<BlockFinalizeResponseObject>(blockHeader, blockSig, daProofSig, blockFragment,
                                                         decryptionShares); */
    return nullptr;

}
