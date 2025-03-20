#pragma once

#include "DecryptedAES256Key.h"

class MockupDecryptedAES256Key : public DecryptedAES256Key {
    string s;

public:
    MockupDecryptedAES256Key(
        const string& _s, block_id _blockID, size_t _totalDecryptors, size_t _requiredDecryptors );

    string toString() override;

};


