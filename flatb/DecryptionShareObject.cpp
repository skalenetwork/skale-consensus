//
// Created by kladko on 3/18/25.
//



#include "block_finalize_request_generated.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "FlatBufferRequest.h"
#include "DecryptionShareObject.h"

using namespace block_finalize;

// Constructor
DecryptionShareObject::DecryptionShareObject(uint32_t transactionIndex, ptr<vector<uint8_t>>& _data)
    : transactionIndex(transactionIndex), data(_data) {
    CHECK_STATE(_data->size() > 0);
}

// Deserialize and verify function
ptr<DecryptionShareObject> DecryptionShareObject::deserializeAndVerify(const DecryptionShare* _fbDecryptionShare) {

    if (!_fbDecryptionShare) {
        return nullptr;
    }

    // Extract transaction index
    uint32_t transactionIndex = _fbDecryptionShare->transaction_index();

    // Extract data
    auto fbData = _fbDecryptionShare->data();
    CHECK_STATE(fbData);

    auto data = make_shared<vector<uint8_t>>(fbData->begin(), fbData->end());

    return make_shared<DecryptionShareObject>(transactionIndex, data);
}
