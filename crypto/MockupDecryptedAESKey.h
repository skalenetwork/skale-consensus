#pragma once

#include "DecryptedAESKey.h"

class MockupDecryptedAESKey : public DecryptedAESKey {
    string s;

public:
    MockupDecryptedAESKey(
        const string& _s, block_id _blockID, size_t _totalDecryptors, size_t _requiredDecryptors );

    string toString() override;

};


