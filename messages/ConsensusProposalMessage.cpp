/*
    Copyright (C) 2018-2019 SKALE Labs

    This file is part of skale-consensus.

    skale-consensus is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    skale-consensus is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with skale-consensus.  If not, see <https://www.gnu.org/licenses/>.

    @file ConsensusProposalMessage.cpp
    @author Stan Kladko
    @date 2018

*/

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h" // for stringify JSON


#include "chains/Schain.h"
#include "headers/BasicHeader.h"

#include "protocols/ProtocolKey.h"
#include "datastructures/BooleanProposalVector.h"
#include "ConsensusProposalMessage.h"

ConsensusProposalMessage::ConsensusProposalMessage(Schain &_sChain, const block_id &_blockID,
                                                   const ptr<BooleanProposalVector> _proposals) : Message(
        _sChain.getSchainID(), MSG_CONSENSUS_PROPOSAL,
        msg_id(0), node_id(0), _blockID,
        schain_index(1)) {

    CHECK_ARGUMENT(_proposals);
    this->proposals = _proposals;

}


const ptr<BooleanProposalVector> ConsensusProposalMessage::getProposals() const {
    CHECK_STATE(proposals);
    return proposals;
}

using namespace rapidjson;

// Serialize only recording the most important info
string ConsensusProposalMessage::serializeToStringLite() {

    StringBuffer sb;
    Writer<StringBuffer> writer(sb);

    writer.StartObject();
    writer.String("cv");
    writer.String(this->getProposals()->toString().c_str());
    writer.EndObject();
    writer.Flush();
    string s(sb.GetString());

    cerr << s;


    return s;

}

using namespace rapidjson;

ptr<ConsensusProposalMessage> ConsensusProposalMessage::parseMessageLite(const string &_header, Schain *_sChain) {

    string proposalsStr;

    CHECK_ARGUMENT(!_header.empty());
    CHECK_ARGUMENT(_sChain);




    cerr << _header << endl;

    CHECK_STATE(_header.size() > 2);

    try {

        Document d;
        
        d.Parse(_header.data());

        CHECK_STATE(!d.HasParseError());
        CHECK_STATE(d.IsObject())
        proposalsStr = BasicHeader::getStringRapid(d, "cv");
    } catch (...) {
        throw_with_nested(InvalidStateException("Could not parse message", __CLASS_NAME__));
    }

    try {

        auto vector = make_shared<BooleanProposalVector>(_sChain->getNodeCount(), proposalsStr);

        auto msg = make_shared<ConsensusProposalMessage>(*_sChain,
                                                         _sChain->getLastCommittedBlockID() + 1,
                                                         vector);
        return msg;

    } catch (...) {
        throw_with_nested(InvalidStateException("Could not create message of type:", __CLASS_NAME__));
    }
}