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
#include <future>
#include <thread>
#include <mutex>
#include <chrono>


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

    for ( const auto& decryptionShare : decryptionShares ) {
        uint32_t transactionIndex = ( uint32_t ) decryptionShare.first;
        const auto data =
            decryptionShare.second->toString();  // Assumes std::string or std::vector<uint8_t>
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
    // 🔍 Verify the resulting buffer before returning
    CHECK_STATE( _serializedDecryptionShares );
    flatbuffers::Verifier verifier(
        _serializedDecryptionShares->data(), _serializedDecryptionShares->size() );
    CHECK_STATE( skale_fb::VerifyDecryptionSharesBuffer( verifier ) );
}


ptr< AESKeyDecryptionShareList > BiteAESDecryptionShareSerializer::deserialize(
    const ptr< vector< uint8_t > >& _serializedDecryptionShares,
    const ptr< CryptoManager >& _manager, bool ) {
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
        _manager->getSchain()->getBiteManager());
}


shared_ptr< AESKeyDecryptionShareList > BiteAESDecryptionShareSerializer::getDecryptionShares(
    const block_id _blockId, const schain_index _proposerIndex, const schain_index _decryptorIndex,
    const flatbuffers::Vector< ::flatbuffers::Offset< skale_fb::DecryptionShare > >*
        _fbDecryptionSharesHandle, ptr<BiteManager> _biteManager) {
    CHECK_STATE(_biteManager)
    
    const size_t numShares = _fbDecryptionSharesHandle->size();
    auto shares = make_shared< AESKeyDecryptionShareList >( _blockId, _proposerIndex, _decryptorIndex );
    
    // Use parallel processing for large numbers of decryption shares
    const size_t PARALLEL_THRESHOLD = 100; // Process in parallel if more than 100 shares
    
    if (numShares <= PARALLEL_THRESHOLD) {
        // Sequential processing for small numbers of shares
        for ( const auto* fbdecryptionShareHandle : *_fbDecryptionSharesHandle ) {
            processSingleDecryptionShare(fbdecryptionShareHandle, _decryptorIndex, _biteManager, shares);
        }
    } else {
        // Parallel processing for large numbers of shares
        auto processingStartTime = std::chrono::high_resolution_clock::now();
        
//        std::mutex sharesMutex;
        const size_t numThreads = 8;
        
        std::vector<std::thread> threads;
        threads.reserve(numThreads);
        
        const size_t chunkSize = (numShares + numThreads - 1) / numThreads;
        
        // Thread-local storage for results to minimize lock contention
        std::vector<std::vector<std::pair<transaction_index, ptr<AESKeyDecryptionShare>>>> threadLocalShares(numThreads);
        
        for (size_t threadId = 0; threadId < numThreads; ++threadId) {
            size_t startIdx = threadId * chunkSize;
            size_t endIdx = std::min(startIdx + chunkSize, numShares);
            
            if (startIdx >= endIdx)
                break;
            
            threads.emplace_back([threadId, startIdx, endIdx, _fbDecryptionSharesHandle, _decryptorIndex,
                                _biteManager, &threadLocalShares]() {
                auto threadStartTime = std::chrono::high_resolution_clock::now();
                size_t processedCount = 0;
                
                // Reserve space for this thread's shares
                threadLocalShares[threadId].reserve(endIdx - startIdx);
                
                for (size_t i = startIdx; i < endIdx; ++i) {
                    try {
                        const auto* fbdecryptionShareHandle = _fbDecryptionSharesHandle->Get(i);
                        CHECK_STATE( fbdecryptionShareHandle )
                        
                        auto rawData = fbdecryptionShareHandle->data()->data();
                        CHECK_STATE( rawData );
                        
                        string decryptionShareStr( rawData, rawData + fbdecryptionShareHandle->data()->size() );
                        auto decryptionShare = _biteManager->createAESDecryptionShare(
                            decryptionShareStr, _decryptorIndex, fbdecryptionShareHandle->decryption_failed() );
                        
                        threadLocalShares[threadId].emplace_back(
                            fbdecryptionShareHandle->transaction_index(), decryptionShare);
                        processedCount++;
                    } catch (const std::exception &e) {
                        LOG(err, fmt::format("Error processing decryption share {}: {}", i, e.what()));
                    }
                }
                
                auto threadEndTime = std::chrono::high_resolution_clock::now();
                auto threadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(threadEndTime - threadStartTime);
                
                LOG(info, fmt::format("Decryption share thread {} processed {} shares (indices {}-{}) in {} ms (avg: {:.2f} ms per share)", 
                                      threadId, 
                                      processedCount,
                                      startIdx,
                                      endIdx - 1,
                                      threadDuration.count(),
                                      processedCount > 0 ? static_cast<double>(threadDuration.count()) / processedCount : 0.0));
            });
        }
        
        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
        
        // Merge results from all threads (single-threaded, no locks needed)
        for (size_t threadId = 0; threadId < numThreads; ++threadId) {
            for (auto& shareEntry : threadLocalShares[threadId]) {
                shares->addShare(shareEntry.first, shareEntry.second);
            }
        }
        
        auto processingEndTime = std::chrono::high_resolution_clock::now();
        auto processingDuration = std::chrono::duration_cast<std::chrono::milliseconds>(processingEndTime - processingStartTime);
        
        LOG(info, fmt::format("Decryption share processing took {} ms for {} shares (avg: {:.2f} ms per share)", 
                              processingDuration.count(), 
                              numShares,
                              static_cast<double>(processingDuration.count()) / numShares));
    }

    return shares;
}

void BiteAESDecryptionShareSerializer::processSingleDecryptionShare(
    const skale_fb::DecryptionShare* fbdecryptionShareHandle,
    schain_index _decryptorIndex,
    ptr<BiteManager> _biteManager,
    ptr<AESKeyDecryptionShareList> shares) {
    
    CHECK_STATE( fbdecryptionShareHandle )
    auto rawData = fbdecryptionShareHandle->data()->data();
    CHECK_STATE( rawData );
    string decryptionShareStr( rawData, rawData + fbdecryptionShareHandle->data()->size() );
    auto decryptionShare = _biteManager->createAESDecryptionShare(
        decryptionShareStr, _decryptorIndex, fbdecryptionShareHandle->decryption_failed() );
    shares->addShare( fbdecryptionShareHandle->transaction_index(), decryptionShare );
}
