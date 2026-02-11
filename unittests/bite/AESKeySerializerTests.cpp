#include "thirdparty/catch.hpp"

#ifdef BITE

#include "bite/serde/BiteAESKeySerializer.h"
#include "crypto/DecryptedAESKeyList.h"
#include <flatbuffers/flatbuffers.h>

CATCH_TEST_CASE("BiteAESKeySerializer backward compatible with single-ciphertext txs", "[bite][serialize][aeskeys][backward]") {
    // Simulate BITE1 layout: one ciphertext per transaction, serialized in order
    DecryptedAESKeyList list;
    DecryptedAESKeys tx0;
    tx0.push_back(DecryptedAESKey({0x0A}));
    DecryptedAESKeys tx1;
    tx1.push_back(DecryptedAESKey({0x0B}));
    list.addKeys(0, tx0);
    list.addKeys(1, tx1);

    auto vec = BiteAESKeySerializer::serialize(list);
    flatbuffers::FlatBufferBuilder builder;
    auto offset = builder.CreateVectorOfStructs(vec);
    builder.Finish(offset);

    auto fbVec = flatbuffers::GetRoot<flatbuffers::Vector<const skale_fb::AesKey*>>(builder.GetBufferPointer());
    DecryptedAESKeyList out;
    BiteAESKeySerializer::deserialize(fbVec, out);

    CATCH_REQUIRE(out.getSize() == 2);
    CATCH_REQUIRE(out.totalDecryptedCiphertextsCount() == 2);
    CATCH_REQUIRE(out.getKeys(0));
    CATCH_REQUIRE(out.getKeys(0)->at(0).getAesKey()[0] == 0x0A);
    CATCH_REQUIRE(out.getKeys(1));
    CATCH_REQUIRE(out.getKeys(1)->at(0).getAesKey()[0] == 0x0B);
}

CATCH_TEST_CASE("BiteAESKeySerializer supports multiple ciphertexts per transaction", "[bite][serialize][aeskeys][bite2]") {
    // Two ciphertexts for a single tx and one ciphertext for another tx
    DecryptedAESKeyList list;
    DecryptedAESKeys tx0;
    tx0.push_back(DecryptedAESKey({0x11}));
    tx0.push_back(DecryptedAESKey({0x12}));
    DecryptedAESKeys tx1;
    tx1.push_back(DecryptedAESKey({0x21}));
    list.addKeys(5, tx0); // arbitrary tx index
    list.addKeys(6, tx1);

    auto vec = BiteAESKeySerializer::serialize(list);
    flatbuffers::FlatBufferBuilder builder;
    auto offset = builder.CreateVectorOfStructs(vec);
    builder.Finish(offset);

    auto fbVec = flatbuffers::GetRoot<flatbuffers::Vector<const skale_fb::AesKey*>>(builder.GetBufferPointer());
    DecryptedAESKeyList out;
    BiteAESKeySerializer::deserialize(fbVec, out);

    CATCH_REQUIRE(out.getSize() == 2);
    CATCH_REQUIRE(out.totalDecryptedCiphertextsCount() == 3);
    auto keys5 = out.getKeys(5);
    CATCH_REQUIRE(keys5);
    CATCH_REQUIRE(keys5->size() == 2);
    CATCH_REQUIRE(keys5->at(0).getAesKey()[0] == 0x11);
    CATCH_REQUIRE(keys5->at(1).getAesKey()[0] == 0x12);

    auto keys6 = out.getKeys(6);
    CATCH_REQUIRE(keys6);
    CATCH_REQUIRE(keys6->size() == 1);
    CATCH_REQUIRE(keys6->at(0).getAesKey()[0] == 0x21);
}

#endif
