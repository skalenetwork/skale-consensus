#pragma once
#include <array>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include "SkaleCommon.h"

// ====== Common Types ======

using TxId = uint64_t;
using Address = std::array<uint8_t, 20>;
using Bytes = std::vector<uint8_t>;

// Contains the needed decrypted fields of a regular transaction.
// - data: original plaintext calldata
// - to:   original plaintext 'to' address (20 bytes)
struct DecryptedRegularTxFields {
    Bytes data;
    Address to;
};

// Contains all decrypted arguments of a CAT transaction.
// Will only be filled if decryption was successful.
// Else, the map will contain std::nullopt for that transaction.
struct DecryptedCATArgs {
    std::vector<Bytes> args;
};

// For both maps, an entry will be marked as std::nullopt if decryption failed for that tx.
using DecryptedRegularTxsMap = std::map< TxId, std::optional<DecryptedRegularTxFields> >;
using DecryptedCATxsMap = std::map< TxId, std::optional<DecryptedCATArgs> >;

// Used to return both regular txs and CAT txs decryption results
struct DecryptedTransactions {
#ifdef BITE2
    std::shared_ptr<DecryptedCATxsMap> catTxsMap;
#endif
    std::shared_ptr<DecryptedRegularTxsMap> regularTxsMap;

    DecryptedTransactions(
#ifdef BITE2
        std::shared_ptr<DecryptedCATxsMap> _catTxsMap,
#endif
        std::shared_ptr<DecryptedRegularTxsMap> _regularTxsMap) {
#ifdef BITE2
        CHECK_STATE(_catTxsMap);
        catTxsMap = _catTxsMap;
#endif
        CHECK_STATE(_regularTxsMap);
        regularTxsMap = _regularTxsMap;
    }

    DecryptedTransactions() = default;
};


typedef std::vector<Bytes > transactions_vector;