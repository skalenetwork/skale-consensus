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

    @file FastMessageLedger.cpp
    @author Stan Kladko
    @date 2021
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "SkaleCommon.h"
#include "Log.h"

#include "chains/Schain.h"

#include "FastMessageLedger.h"

FastMessageLedger::FastMessageLedger(Schain *_schain, string  _dirFullPath) :
        schain(_schain) {
    CHECK_STATE(schain);
    CHECK_STATE(_dirFullPath.size() > 2);

    CHECK_STATE(_dirFullPath.back() != '/');

    ledgerFileFullPath= _dirFullPath + "/cons_incoming_msg_ledger_" +
            to_string(schain->getSchainIndex());

    this->fd = open(ledgerFileFullPath.c_str(), O_CREAT| O_RDONLY, S_IRWXU);

    CHECK_STATE2(fd > 0,ledgerFileFullPath + " file read open failed with errno:" +
        string(strerror(errno)));

    close(this->fd);

    this->fd = open(ledgerFileFullPath.c_str(), O_CREAT| O_TRUNC | O_WRONLY, S_IRWXU);
    CHECK_STATE2(fd > 0, ledgerFileFullPath + " file write open failed with errno:" +
         string(strerror(errno)));


}

ptr<vector<ptr<Message>>> FastMessageLedger::retrieveAndClearPreviosRunMessages() {
    auto result = previousRunMessages;
    previousRunMessages = nullptr;
    CHECK_STATE(result);
    return result;
}
