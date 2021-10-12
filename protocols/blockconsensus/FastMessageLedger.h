/*
    Copyright (C) 2021- SKALE Labs

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

    @file FastMessageLedger.h
    @author Stan Kladko
    @date 2021
*/

#ifndef SKALED_FASTMESSAGELEDGER_H
#define SKALED_FASTMESSAGELEDGER_H

/*
 * Fast ledger for consensus messages.
 *
 * Use to ressurect consensus state after crash.
 */

#include "messages/ConsensusProposalMessage.h"
#include "messages/NetworkMessage.h"


class FastMessageLedger {

    Schain* schain = nullptr;
    string ledgerFileFullPath;
    ptr<vector<ptr<Message>>> previousRunMessages = nullptr;

    ptr<vector<ptr<Message>>> retrieveAndClearPreviosRunMessages();



private:
    int fd = -1;

    // reads all messages if any and resets the ledger to empty
    vector<ptr<Message>> readAllMessagesAndReset();

    // writes consensus proposal message to ledger
    void writeProposalMessage(ptr<ConsensusProposalMessage> _message);

    // writes consensus proposal message to ledger
    void writeNetworkMessage(ptr<NetworkMessage> _message);


public:
    FastMessageLedger(Schain *schain, string ledgerFileFullPath);

};


#endif //SKALED_FASTMESSAGELEDGER_H
