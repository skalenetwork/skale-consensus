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

    @file ConsensusProposalMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma once
#include <vector>

#include "thirdparty/rapidjson/document.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h"  // for stringify JSON


#include "Message.h"
#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

class Schain;
class BooleanProposalVector;

class ConsensusProposalMessage : public Message {
    ptr< BooleanProposalVector > proposals;

public:
    ConsensusProposalMessage(
        Schain& _sChain, const block_id& _blockID, const ptr< BooleanProposalVector > _proposals );

    [[nodiscard]] const ptr< BooleanProposalVector > getProposals() const;

    string serializeToStringLite();

    static ptr< ConsensusProposalMessage > parseMessageLite(
        const string& _header, Schain* _sChain );
};
