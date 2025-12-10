#pragma once

#include <memory>
#include <boost/container/flat_map.hpp>
#include <SkaleCommon.h>
#include "crypto/TransactionCiphertexts.h"

/**
 * @brief A wrapper around a flat_map to keep track of total number of ciphertexts stored.
 * Each item of the map is a TransactionCiphertexts object, which can store one or more EncryptedAESKey ciphertexts.
 * This class keeps a cached total count of all ciphertexts across all transactions.
 */
class TransactionCiphertextsMap {

private:
    boost::container::flat_map<
        transaction_index,
        std::shared_ptr<TransactionCiphertexts>
    > map;

    // stores sum of ciphertext counts across all TransactionCiphertexts in the map
    std::size_t totalCount = 0;

public:
    // Iterators
    auto begin() { return map.begin(); }
    auto end()   { return map.end();   }
    auto begin() const { return map.begin(); }
    auto end()   const { return map.end();   }

    // Size of map
    std::size_t size() const { return map.size(); }

    // Cached total ciphertext count
    std::size_t totalCiphertextCount() const { return totalCount; }

    // Insert/Emplace
    template<typename... Args>
    auto emplace(Args&&... args) {
        auto [it, inserted] = map.emplace(std::forward<Args>(args)...);
        if (inserted) {
            totalCount += countOf(it->second);
        }
        return std::make_pair(it, inserted);
    }

    // Insert or assign
    auto insert_or_assign(const transaction_index& idx,
                          std::shared_ptr<TransactionCiphertexts> val) {
        auto it = map.find(idx);
        if (it == map.end()) {
            totalCount += countOf(val);
            return map.insert_or_assign(idx, std::move(val));
        } else {
            // subtract old, add new
            totalCount -= countOf(it->second);
            totalCount += countOf(val);
            return map.insert_or_assign(idx, std::move(val));
        }
    }

    // Erase
    std::size_t erase(const transaction_index& idx) {
        auto it = map.find(idx);
        if (it == map.end()) return 0;

        totalCount -= countOf(it->second);
        return map.erase(idx);
    }

    // Access operator
    std::shared_ptr<TransactionCiphertexts>& at(const transaction_index& idx) {
        return map.at(idx);
    }

private:
    // helper function - normalize count to 0 if pointer is null
    static std::size_t countOf(const std::shared_ptr<TransactionCiphertexts>& p) {
        return p ? p->count() : 0;
    }
};
