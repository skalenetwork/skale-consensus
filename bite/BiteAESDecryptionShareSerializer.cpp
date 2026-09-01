#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/futures/Future.h>
#include <folly/Unit.h>

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

#include <chains/Schain.h>


ptr< std::vector< uint8_t > > BiteAESDecryptionShareSerializer::serialize(
    ptr< AESKeyDecryptionShareList > _decryptionShareList ) {
    CHECK_STATE( _decryptionShareList );

    // Preallocate ~1MB
    thread_local flatbuffers::FlatBufferBuilder builder( 1024 * 1024 );
    builder.Clear();

    // Serialize each DecryptionShare
    auto& decryptionShares = _decryptionShareList->getDecryptionShares();
    std::vector< flatbuffers::Offset< skale_fb::DecryptionShare > > decryptionShareVec;
    decryptionShareVec.reserve( decryptionShares.size() );

    // run over all shares of all txs from this decryptor
    for ( const auto& [txId, shares] : decryptionShares ) {
        uint32_t transactionIndex = ( uint32_t ) txId;
        // convert all shares for this ciphertext into a single string
        std::string data;
        for ( size_t i = 0; i < shares->size(); i++ ) {
            CHECK_STATE( shares->at(i) );
            data += shares->at(i)->toString();
            if ( i != shares->size() - 1 ) {
                data += ",";
            }
        }

        auto dataOffset =
            builder.CreateVector( reinterpret_cast< const uint8_t* >( data.data() ), data.size() );

        decryptionShareVec.emplace_back(
            skale_fb::CreateDecryptionShare( builder, transactionIndex, dataOffset ) );
    }

    // Finalize the top-level table
    auto topOffset =
        skale_fb::CreateDecryptionShares( builder, ( uint64_t ) _decryptionShareList->getBlockId(),
            ( uint64_t ) _decryptionShareList->getProposerIndex(),
            ( uint64_t ) _decryptionShareList->getDecryptorIndex(),
            builder.CreateVector( decryptionShareVec ) );
    builder.Finish( topOffset );

    // Copy into shared_ptr
    const uint8_t* raw = builder.GetBufferPointer();
    size_t size = builder.GetSize();

    return std::make_shared< std::vector< uint8_t > >( raw, raw + size );
}


void BiteAESDecryptionShareSerializer::serializedSanityCheck(
    const ptr< vector< uint8_t > >& _serializedDecryptionShares ) {
    // Verify the resulting buffer before returning
    CHECK_STATE( _serializedDecryptionShares );
    flatbuffers::Verifier verifier(
        _serializedDecryptionShares->data(), _serializedDecryptionShares->size() );
    CHECK_STATE( skale_fb::VerifyDecryptionSharesBuffer( verifier ) );
}


ptr< AESKeyDecryptionShareList > BiteAESDecryptionShareSerializer::deserialize(
    const ptr< vector< uint8_t > >& _serializedDecryptionShares,
    const ptr< CryptoManager >& _manager, CryptographicValidationMode _validationMode ) {
    CHECK_ARGUMENT( _serializedDecryptionShares );
    CHECK_ARGUMENT( _manager );


    const skale_fb::DecryptionShares* fbDecryptionShares = nullptr;

    VERIFY_AND_PARSE_FLATBUFFER_FROM_VECTOR(
        *_serializedDecryptionShares, DecryptionShares, fbDecryptionShares );

    block_id blockId = fbDecryptionShares->block_id();
    schain_index proposerIndex = fbDecryptionShares->proposer_index();
    schain_index decryptorIndex = fbDecryptionShares->decryptor_index();
    auto fbDecryptionSharesHandle = fbDecryptionShares->decryption_shares();

    CHECK_STATE( fbDecryptionSharesHandle );

    return getDecryptionShares(blockId, proposerIndex, decryptorIndex, fbDecryptionSharesHandle,
        _manager->getSchain()->getBiteManager(), _validationMode );
}


shared_ptr< AESKeyDecryptionShareList > BiteAESDecryptionShareSerializer::getDecryptionShares(
    const block_id _blockId, const schain_index _proposerIndex, const schain_index _decryptorIndex,
    const flatbuffers::Vector< ::flatbuffers::Offset< skale_fb::DecryptionShare > >*
        _fbDecryptionSharesHandle, ptr<BiteManager> _biteManager, CryptographicValidationMode _validationMode) {
    CHECK_STATE(_biteManager)

    const size_t numShares = _fbDecryptionSharesHandle->size();
    auto shares = make_shared< AESKeyDecryptionShareList >( _blockId, _proposerIndex, _decryptorIndex );

    std::vector<folly::Future<folly::Unit>> futures;
    
    const size_t numThreads = getNumBiteValidationThreads();
    futures.reserve(numThreads);

    const size_t chunkSize = (numShares + numThreads - 1) / numThreads;

    // Thread-local storage for results to minimize lock contention
    std::vector<std::vector<std::pair<transaction_index, ptr<AESKeyDecryptionShares>>>>
            threadLocalShares(numThreads);

    for (size_t threadId = 0; threadId < numThreads; ++threadId) {
        size_t startIdx = threadId * chunkSize;
        size_t endIdx = std::min(startIdx + chunkSize, numShares);

        if (startIdx >= endIdx)
            break;

        auto future = folly::via(_biteManager->getExecutor().get(),
                                 [threadId, startIdx, endIdx, _fbDecryptionSharesHandle,
                                _decryptorIndex, _biteManager, &threadLocalShares,
                                _validationMode]() {
            threadLocalShares[threadId].reserve(endIdx - startIdx);

            for (size_t i = startIdx; i < endIdx; ++i) {
                const auto* fbdecryptionShareHandle = _fbDecryptionSharesHandle->Get(i);
                CHECK_STATE( fbdecryptionShareHandle );

                auto rawData = fbdecryptionShareHandle->data()->data();
                CHECK_STATE( rawData );

                string decryptionShareStr( rawData, rawData + fbdecryptionShareHandle->data()->size() );
                auto decryptionShares = _biteManager->createAESDecryptionShares(
                    decryptionShareStr, _decryptorIndex, 
                    fbdecryptionShareHandle->decryption_failed(), _validationMode );

                threadLocalShares[threadId].emplace_back(
                    fbdecryptionShareHandle->transaction_index(), decryptionShares);
            }

            return folly::unit;
        });

        futures.push_back(std::move(future));
    }

    folly::collectAll(futures).get();

    // Merge results from all threads
    shares->reserve( numShares );
    for (size_t threadId = 0; threadId < numThreads; ++threadId) {
        for (auto& [txId, decryptionShares] : threadLocalShares.at(threadId)) {
            shares->addShares(txId, decryptionShares);
        }
    }

    return shares;
}
