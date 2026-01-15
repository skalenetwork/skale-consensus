#pragma once
#include <optional>
#include <memory>
#include "EthTransaction.h"
#include "SkaleCommon.h"

class ParsedEthTransaction;
class BiteManager;


class EthTransactionEncoder {

public:
    static std::shared_ptr< std::vector< uint8_t > >  rlpEncodeWithoutSig(ParsedEthTransaction& _transaction);

    inline static std::vector< uint8_t > generateRandomPrivateKey();


    /**
     * @brief Generates a sample transaction of one of the three types (Legacy, Type1, Type2).
     * Loops through the three types on each call following the order: Legacy -> Type1 -> Type2.
     * The transaction returned is unsigned.
     */
    static std::unique_ptr<EthTransaction> generateSampleTx();

    /**
     * @brief Encrypts the transaction using BITE1 encryption. Meaning it will encrypt both
     * the 'to' and 'data' fields of the transaction.
     * Substitutes the 'to' field with the BITE magic number, and the 'data' field with the encrypted data,
     * all in-place.
     */
    static void encryptRegularTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager);

    static void encryptCATTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager);

    static void encryptEmptyCATTransaction(std::unique_ptr<EthTransaction>& tx, std::shared_ptr<BiteManager> _biteManager);

    static std::shared_ptr<std::vector<uint8_t>> signAndEncodeTx(const std::unique_ptr<EthTransaction>& tx);


};
