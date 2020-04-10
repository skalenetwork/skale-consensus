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

    @file ConnectionStatus.h
    @author Stan Kladko
    @date 2018
*/

#pragma once


enum ConnectionStatus {
    CONNECTION_PROCEED = 0, CONNECTION_RETRY_LATER = 1, CONNECTION_ERROR = 2,
    CONNECTION_DISCONNECT = 3,
    CONNECTION_SUCCESS = 4};

enum ConnectionSubStatus {
    CONNECTION_ERROR_UNKNOWN_SCHAIN_ID = 1, CONNECTION_ERROR_UNKNOWN_SERVER_ERROR = 2,
    CONNECTION_ERROR_DONT_KNOW_THIS_NODE = 3, CONNECTION_ERROR_INVALID_NODE_INDEX = 4,
    CONNECTION_ERROR_INVALID_NODE_ID = 5, CONNECTION_ERROR_TIME_STAMP_IN_THE_FUTURE = 6,
    CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED = 7,
    CONNECTION_BLOCK_PROPOSAL_TOO_LATE = 8,
    CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE = 9,
    CONNECTION_DOUBLE_PROPOSAL = 10,
    CONNECTION_DONT_HAVE_DA_PROOF_FOR_PROPOSAL = 11,
    CONNECTION_INVALID_HASH = 12,
    CONNECTION_NO_NEW_BLOCKS = 13,
    CONNECTION_ERROR_INVALID_REQUEST_TYPE = 14,
    CONNECTION_ERROR_INVALID_FRAGMENT_INDEX = 15,
    CONNECTION_ERROR_INVALID_PROPOSER_INDEX = 16,
    CONNECTION_SIGNATURE_DID_NOT_VERIFY = 17,
    CONNECTION_ALREADY_HAVE_DA_PROOF = 18,
    CONNECTION_ZERO_STATE_ROOT = 19,
    CONNECTION_DONT_HAVE_PROPOSAL_FOR_THIS_DA_PROOF = 20,
    CONNECTION_FINALIZE_DONT_HAVE_PROPOSAL = 21};
