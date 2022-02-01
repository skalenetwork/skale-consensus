//
// Created by kladko on 01.02.22.
//

#include "SkaleCommon.h"
#include "Log.h"
#include "network/Utils.h"

#include "TestEncryptedTransactionAnalyzer.h"


TestEncryptedTransactionAnalyzer::TestEncryptedTransactionAnalyzer() {
    teMagicStart = make_shared<vector<uint8_t>>(TE_MAGIC_SIZE);
    teMagicEnd = make_shared<vector<uint8_t>>(TE_MAGIC_SIZE);
    Utils::cArrayFromHex(TE_MAGIC_START, teMagicStart->data(), TE_MAGIC_SIZE);
    Utils::cArrayFromHex(TE_MAGIC_END, teMagicEnd->data(), TE_MAGIC_SIZE);
}


shared_ptr<std::vector<uint8_t>> TestEncryptedTransactionAnalyzer::getLastSmartContractArgument(
        const std::vector<uint8_t> &_transaction) {
    try {


        int64_t startIndex = -1;


        for (int64_t i = 0; i < (int64_t) _transaction.size() - (int64_t) teMagicStart->size(); i++) {
            if (memcmp(_transaction.data() + i, teMagicStart->data(), teMagicStart->size()) == 0) {
                startIndex = i;
                break;
            }
        }

        if (startIndex == -1) {
            return nullptr;
        }

        int64_t endIndex = -1;

        auto segmentStart = startIndex + teMagicStart->size();


        for (int64_t i = segmentStart; i < (int64_t) _transaction.size() - (int64_t) teMagicEnd->size(); i++) {
            if (memcmp(_transaction.data() + i, teMagicEnd->data(), teMagicEnd->size()) == 0) {
                endIndex = i;
                break;
            }
        }

        if (endIndex < 0) {
            return nullptr;
        }


        CHECK_STATE(segmentStart < (uint64_t) endIndex);

        auto segmentSize = endIndex - segmentStart;

        auto result = make_shared<vector<uint8_t>>(segmentSize);

        memcpy(result->data(), _transaction.data() + segmentStart, result->size());

        return result;

    }

    catch (
            exception &e
    ) {
        throw_with_nested(InvalidStateException(__FUNCTION__, __CLASS_NAME__)
        );
    }
}

