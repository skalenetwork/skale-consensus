//
// Created by stan on 18.03.18.
//

#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"

#include "../abstracttcpserver/ConnectionStatus.h"
#include "MissingTransactionsRequestHeader.h"
#include "CatchupResponseHeader.h"


using namespace std;

CatchupResponseHeader::CatchupResponseHeader() {

}

void CatchupResponseHeader::setBlockSizes(ptr<list<uint64_t>> _blockSizes) {

    blockCount = _blockSizes->size();

    blockSizes = _blockSizes;

    complete = true;
}

void CatchupResponseHeader::addFields(nlohmann::basic_json<> &_j) {


    _j["count"] = blockCount;

    if (blockSizes != nullptr)
        _j["sizes"] = *blockSizes;


}

uint64_t CatchupResponseHeader::getBlockCount() const {
    return blockCount;
}

void CatchupResponseHeader::setBlockCount(uint64_t blockCount) {
    CatchupResponseHeader::blockCount = blockCount;
}



