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
    CONNECTION_SUCCESS = 0,
    CONNECTION_PROCEED = 1,
    CONNECTION_STATUS_UNKNOWN = 2,
    CONNECTION_RETRY_LATER = 3,
    CONNECTION_DISCONNECT = 4,
    CONNECTION_ERROR = 5,

};

enum ConnectionSubStatus {
    CONNECTION_OK = 0,
    CONNECTION_SUBSTATUS_UNKNOWN = 1,
    CONNECTION_ERROR_UNKNOWN_SCHAIN_ID = 2,
    CONNECTION_ERROR_UNKNOWN_SERVER_ERROR = 3,
    CONNECTION_ERROR_DONT_KNOW_THIS_NODE = 4,
    CONNECTION_ERROR_INVALID_NODE_INDEX = 5,
    CONNECTION_ERROR_INVALID_NODE_ID = 6,
    CONNECTION_ERROR_TIME_STAMP_IN_THE_FUTURE = 7,
    CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED = 8,
    CONNECTION_BLOCK_PROPOSAL_TOO_LATE = 9,
    CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE = 10,
    CONNECTION_DOUBLE_PROPOSAL = 11,
    CONNECTION_DONT_HAVE_DA_PROOF_FOR_PROPOSAL = 12,
    CONNECTION_INVALID_HASH = 13,
    CONNECTION_NO_NEW_BLOCKS = 14,
    CONNECTION_ERROR_INVALID_REQUEST_TYPE = 15,
    CONNECTION_ERROR_INVALID_FRAGMENT_INDEX = 16,
    CONNECTION_ERROR_INVALID_PROPOSER_INDEX = 17,
    CONNECTION_SIGNATURE_DID_NOT_VERIFY = 18,
    CONNECTION_ALREADY_HAVE_DA_PROOF = 19,
    CONNECTION_ZERO_STATE_ROOT = 20,
    CONNECTION_DONT_HAVE_PROPOSAL_FOR_THIS_DA_PROOF = 21,
    CONNECTION_FINALIZE_DONT_HAVE_PROPOSAL = 22,
    CONNECTION_CATCHUP_DONT_HAVE_THIS_BLOCK = 23,
    CONNECTION_ERROR_TIME_LESS_THAN_MODERN_DAY = 24,
    CONNECTION_ERROR_TIME_TOO_FAR_IN_THE_FUTURE = 25,
    CONNECTION_PROPOSAL_STATE_ROOT_DOES_NOT_MATCH = 26,
    CONNECTION_ALREADY_HAVE_ENOUGH_PROPOSALS_FOR_THIS_BLOCK_ID = 27

};