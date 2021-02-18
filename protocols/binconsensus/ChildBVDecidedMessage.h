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

    @file ChildBVDecidedMessage.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


#include "messages/ChildMessage.h"

class ChildBVDecidedMessage : public ChildMessage {
    bool value;

    bin_consensus_round round = 0;

    uint64_t maxProcessingTimeMs = 0;
    uint64_t maxLatencyTimeMs = 0;

public:
    uint64_t getMaxLatencyTimeMs() const;

    bool getValue();


    bin_consensus_round getRound();

    uint64_t getMaxProcessingTimeMs();

    ChildBVDecidedMessage( bool _value, ProtocolInstance& _srcProtocolInstance,
        const ptr< ProtocolKey >& _key, bin_consensus_round _round, uint64_t _maxProcessingTimeMs,
        uint64_t _maxLatencyTimeMs );
};