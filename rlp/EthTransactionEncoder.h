#pragma once
#include <optional>
#include <memory>
#include "EthTransaction.h"
#include "SkaleCommon.h"

class ParsedEthTransaction;
class BiteManager;


class EthTransactionEncoder {

public:
    static std::shared_ptr<std::vector<uint8_t>> generateSampleTx(bool _isBite, ptr<BiteManager> _biteManager);

    static std::shared_ptr< std::vector< uint8_t > >  rlpEncodeWithoutSig(ParsedEthTransaction& _transaction);

    inline static std::vector< uint8_t > generateRandomPrivateKey();

    static std::shared_ptr<std::vector<uint8_t>> signAndEncodeTx(const EthTransaction& tx);


};
