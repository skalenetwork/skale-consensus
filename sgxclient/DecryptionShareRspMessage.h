/*
  Copyright (C) 2018- SKALE Labs

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

  @file DecryptionShareRspMessage.h
  @author Oleh Nikolaiev
  @date 2022
*/

#ifndef SGXWALLET_DECRYPTIONSHARERSPMESSAGE_H
#define SGXWALLET_DECRYPTIONSHARERSPMESSAGE_H

#include "sgxclient/SgxZmqMessage.h"

class GetDecryptionSharesRspMessage : public SgxZmqMessage {
public:
    GetDecryptionSharesRspMessage( shared_ptr< rapidjson::Document >& _d ) : SgxZmqMessage( _d ) {}

    vector<string> getShares();
};


#endif  // SGXWALLET_DECRYPTIONSHARERSPMESSAGE_H
