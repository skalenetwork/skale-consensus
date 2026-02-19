#pragma once
#include <array>
#include <map>
#include <vector>
#include <memory>
#include <optional>
#include "SkaleCommon.h"

// ====== Common Types ======

using TxId = uint64_t;
using AddressBytes = std::array<uint8_t, 20>;
using Bytes = std::vector<uint8_t>;

// Contains the needed decrypted fields of a regular transaction.
// - data: original plaintext calldata
// - to:   original plaintext 'to' address (20 bytes)
struct DecryptedRegularTxFields {
    Bytes data;
    AddressBytes to;
};

struct DecryptedCTXArgs {
    std::vector<Bytes> args;
};

// For both maps, an entry will be marked as std::nullopt if decryption failed for that tx.
using DecryptedRegularTxsMap = std::map< TxId, std::optional<DecryptedRegularTxFields> >;
using DecryptedCTXTxsMap = std::map< TxId, std::optional<DecryptedCTXArgs> >;

// Used to return both regular txs and CAT txs decryption results
struct DecryptedTransactions {
#ifdef BITE2
    std::shared_ptr<DecryptedCTXTxsMap> ctxTxsMap;
#endif
    std::shared_ptr<DecryptedRegularTxsMap> regularTxsMap;

    DecryptedTransactions(
#ifdef BITE2
        std::shared_ptr<DecryptedCTXTxsMap> _ctxTxsMap,
#endif
        std::shared_ptr<DecryptedRegularTxsMap> _regularTxsMap) {
#ifdef BITE2
        CHECK_STATE(_ctxTxsMap);
        ctxTxsMap = _ctxTxsMap;
#endif
        CHECK_STATE(_regularTxsMap);
        regularTxsMap = _regularTxsMap;
    }

    DecryptedTransactions() :
#ifdef BITE2
        ctxTxsMap(std::make_shared<DecryptedCTXTxsMap>()),
#endif
          regularTxsMap(std::make_shared<DecryptedRegularTxsMap>()) {}
};


typedef std::vector<Bytes > transactions_vector;
