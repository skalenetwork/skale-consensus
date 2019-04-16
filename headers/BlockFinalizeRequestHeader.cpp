//
// Created by stan on 18.03.18.
//


#include "../SkaleConfig.h"
#include "../crypto/SHAHash.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../exceptions/InvalidArgumentException.h"

#include "../thirdparty/json.hpp"
#include "../abstracttcpserver/ConnectionStatus.h"

#include "../datastructures/BlockProposal.h"
#include "../datastructures/CommittedBlock.h"

#include "../node/Node.h"
#include "../node/NodeInfo.h"
#include "../chains/Schain.h"

#include "AbstractBlockRequestHeader.h"

#include "BlockFinalizeRequestHeader.h"

using namespace std;


BlockFinalizeRequestHeader::BlockFinalizeRequestHeader(Schain &_sChain, ptr<CommittedBlock> proposal, schain_index _proposerIndex) :
        AbstractBlockRequestHeader(_sChain, static_pointer_cast<BlockProposal>(proposal),
                FINALIZE_REQUEST_TYPE, _proposerIndex) {

    if (!proposal) {
        BOOST_THROW_EXCEPTION(InvalidArgumentException("Null proposal", __CLASS_NAME__));
    }

    if (proposal) {
        this->hash = proposal->getHash()->toHex();
    }
    this->proposerNodeID = _sChain.getNode()->getNodeID();
    complete = true;

}

void BlockFinalizeRequestHeader::addFields(nlohmann::basic_json<> &jsonRequest) {

    AbstractBlockRequestHeader::addFields(jsonRequest);

    if (hash)
        jsonRequest["hash"] = *hash;

}


