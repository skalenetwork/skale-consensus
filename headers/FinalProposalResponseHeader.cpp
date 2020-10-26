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

    @file FinalProposalResponseHeader.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "thirdparty/json.hpp"

#include "FinalProposalResponseHeader.h"

using namespace std;

FinalProposalResponseHeader::FinalProposalResponseHeader(const string& _sigShare,
    const string & _signature, const string &_publicKey, const string& _publicKeySig)
        : Header(SIG_SHARE_RSP) , sigShare(_sigShare), signature(_signature),
          publicKey(_publicKey), publicKeySig(_publicKeySig)  {
    CHECK_ARGUMENT(!_sigShare.empty())
    setStatusSubStatus(CONNECTION_SUCCESS, CONNECTION_OK);
    complete = true;
}

void FinalProposalResponseHeader::addFields(nlohmann::basic_json<> &_j) {
    Header::addFields(_j);
    _j["sss"] = sigShare;
    _j["sig"] = signature;
    _j["pk"] = publicKey;
    _j["pks"] = publicKeySig;

}

FinalProposalResponseHeader::FinalProposalResponseHeader(ConnectionStatus _status, ConnectionSubStatus _substatus)
        : Header(SIG_SHARE_RSP) {
    CHECK_ARGUMENT(_status != CONNECTION_SUCCESS)
    this->setStatusSubStatus(_status, _substatus);
}
const string& FinalProposalResponseHeader::getSignature() const {
    return signature;
}


const string& FinalProposalResponseHeader::getSigShare() const {
    CHECK_STATE(!sigShare.empty())
    return sigShare;
}


