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

    @file Connection.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleCommon.h"
#include "Log.h"
#include "exceptions/FatalError.h"

#include "ServerConnection.h"

using namespace std;

atomic< int64_t > ServerConnection::totalObjects = 0;

ServerConnection::ServerConnection( Socket _descriptor, const string& _ip )
    : descriptor( std::move( _descriptor ) ), ip( _ip ) {
    totalObjects++;
}

file_descriptor ServerConnection::getDescriptor() {
    return file_descriptor{ descriptor.get() };
}

string ServerConnection::getIP() {
    CHECK_STATE( !ip.empty() );
    return ip;
}

ServerConnection::~ServerConnection() {
    totalObjects--;
}


uint64_t ServerConnection::getTotalObjects() {
    return totalObjects;
};
