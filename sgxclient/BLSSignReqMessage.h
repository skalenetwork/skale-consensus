/*
  Copyright (C) 2018-2019 SKALE Labs

  This file is part of libBLS.

  libBLS is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  libBLS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libBLS.  If not, see <https://www.gnu.org/licenses/>.

  @file BLSReqSignMessage.h
  @author Stan Kladko
  @date 2020
*/

#ifndef SGXWALLET_BLSSIGNREQMSG_H
#define SGXWALLET_BLSSIGNREQMSG_H

#include "sgxclient/SgxZmqMessage.h"

class BLSSignReqMessage : public SgxZmqMessage {

public:

    BLSSignReqMessage(shared_ptr<rapidjson::Document>& _d) : SgxZmqMessage(_d) {};

};


#endif //SGXWALLET_BLSSIGNREQMSG_H
