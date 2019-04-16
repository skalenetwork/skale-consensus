//
// Created by stan on 11.12.18.
//
#include "../SkaleConfig.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"

#include "../thirdparty/json.hpp"
#include "../node/Node.h"
#include "../abstracttcpserver/ConnectionStatus.h"
#include "CatchupRequestHeader.h"


//
// Created by stan on 18.03.18.
//


#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../chains/Schain.h"



using namespace std;

CatchupRequestHeader::CatchupRequestHeader() : Header() {

}

CatchupRequestHeader::CatchupRequestHeader(Schain &_sChain, schain_index _dstIndex) :
        Header() {


    this->srcNodeID = _sChain.getNode()->getNodeID();
    this->srcSchainIndex = _sChain.getSchainIndex();
    this->dstSchainIndex = _dstIndex;
    this->dstNodeID = _sChain.getNode()->getNodeInfoByIndex(_dstIndex)->getNodeID();
    this->schainID = _sChain.getSchainID();
    this->blockID = _sChain.getCommittedBlockID();

    ASSERT(_sChain.getNode()->getNodeInfosByIndex().count(_dstIndex) > 0);

    complete = true;

}

void CatchupRequestHeader::addFields(nlohmann::basic_json<> &j) {

    j["schainID"] = (uint64_t ) schainID;

    j["srcNodeID"] = (uint64_t ) srcNodeID;

    j["dstNodeID"] = (uint64_t )  dstNodeID;

    j["srcSchainIndex"] = (uint64_t ) srcSchainIndex;

    j["dstSchainIndex"] = (uint64_t ) dstSchainIndex;

    j["blockID"] = (uint64_t ) blockID;

}


