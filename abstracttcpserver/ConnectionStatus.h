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
    CONNECTION_PROCEED, CONNECTION_RETRY_LATER, CONNECTION_ERROR,
    CONNECTION_SERVER_ERROR, CONNECTION_DISCONNECT, CONNECTION_UNEXPECTED_SERVER_DISCONNECT,
    CONNECTION_SUCCESS, STATUS_DUMMY_HACK};

enum ConnectionSubStatus {
    CONNECTION_ERROR_UNKNOWN_SCHAIN_ID, CONNECTION_ERROR_UNKNOWN_SERVER_ERROR,
    CONNECTION_ERROR_DONT_KNOW_THIS_NODE, CONNECTION_ERROR_INVALID_NODE_INDEX,
    CONNECTION_ERROR_INVALID_NODE_ID, CONNECTION_ERROR_TIME_STAMP_IN_THE_FUTURE,
    CONNECTION_ERROR_TIME_STAMP_EARLIER_THAN_COMMITTED,
    CONNECTION_BLOCK_PROPOSAL_TOO_LATE,
    CONNECTION_BLOCK_PROPOSAL_IN_THE_FUTURE,
    CONNECTION_DOUBLE_PROPOSAL,
    CONNECTION_DONT_HAVE_BLOCK_YET,
    CONNECTION_DONT_HAVE_BLOCK,
    CONNECTION_DONT_HAVE_PROPOSAL,
    CONNECTION_DONT_HAVE_DA_PROOF_FOR_PROPOSAL,
    CONNECTION_INVALID_INDEX,
    CONNECTION_INVALID_HASH,
    CONNECTION_NO_NEW_BLOCKS,
    CONNECTION_ERROR_INVALID_REQUEST_TYPE,
    CONNECTION_ERROR_INVALID_FRAGMENT_INDEX,
    CONNECTION_ERROR_INVALID_PROPOSER_INDEX,
    CONNECTION_SIGNATURE_DID_NOT_VERIFY,
    CONNECTION_DONT_HAVE_THIS_PROPOSAL,
    CONNECTION_ALREADY_HAVE_DAP_PROOF,
    CONNECTION_ZERO_STATE_ROOT,
    SUBSTATUS_DUMMY_HACK };


