#pragma once

#include "Header.h"

class BlockFinalizeResponseHeader : public Header {

    ptr<string> sigShare;

public:

    void addFields(nlohmann::json &jsonRequest) override;
    void setSigShare(const ptr<string> &_sigShare);


};


