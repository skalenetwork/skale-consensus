/*
  Copyright (C) 2018- SKALE Labs

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

  @file ECDSARspSignMessage.cpp
  @author Stan Kladko
  @date 2020
*/


#include "ECDSASignRspMessage.h"


string ECDSASignRspMessage::getSignature() {
    string r = getStringRapid( "signature_r" );
    string v = getStringRapid( "signature_v" );
    string s = getStringRapid( "signature_s" );

    auto ret = v + ":" + r.substr( 2 ) + ":" + s.substr( 2 );

    return ret;
}
