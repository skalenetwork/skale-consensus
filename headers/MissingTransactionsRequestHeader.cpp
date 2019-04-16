//
// Created by stan on 18.03.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "../crypto/SHAHash.h"
#include "../datastructures/BlockProposal.h"
#include "../chains/Schain.h"

#include "MissingTransactionsRequestHeader.h"



using namespace std;

MissingTransactionsRequestHeader::MissingTransactionsRequestHeader() = default;


MissingTransactionsRequestHeader::MissingTransactionsRequestHeader(ptr<map<uint64_t, ptr<partial_sha_hash>>> missingMessages_)
     {

     this->missingTransactionsCount = missingMessages_->size();
     complete = true;

}

void MissingTransactionsRequestHeader::addFields(nlohmann::basic_json<> &j_) {
        j_["count"] = missingTransactionsCount;
}

uint64_t MissingTransactionsRequestHeader::getMissingTransactionsCount() const {
    return missingTransactionsCount;
}

void MissingTransactionsRequestHeader::setMissingTransactionsCount(uint64_t missingTransactionsCount) {
    MissingTransactionsRequestHeader::missingTransactionsCount = missingTransactionsCount;
}


