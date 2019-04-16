//
// Created by stan on 18.03.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "MissingTransactionsResponseHeader.h"

using namespace std;

MissingTransactionsResponseHeader::MissingTransactionsResponseHeader() {

}

MissingTransactionsResponseHeader::MissingTransactionsResponseHeader(ptr<vector<uint64_t>> _missingTransactionSizes)
        : missingTransactionSizes(_missingTransactionSizes) {
    complete = true;

}

void MissingTransactionsResponseHeader::addFields(nlohmann::basic_json<> &_j) {


    list<uint64_t> l(missingTransactionSizes->begin(), missingTransactionSizes->end());

    _j["sizes"] = l;

}


