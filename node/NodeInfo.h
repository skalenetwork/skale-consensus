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

    @file NodeInfo.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

class NodeInfo {
    node_id nodeID = 0;

    string ipAddress;

    network_port port = 0;

    schain_id schainID = 0;

    schain_index schainIndex = 0;

    ptr< sockaddr_in > socketaddr;

public:
    node_id getNodeID() const;

    schain_index getSchainIndex() const;

    network_port getPort() const;

    schain_id getSchainID() const;

    NodeInfo( node_id nodeID, const string& ip, network_port port, schain_id schainID,
        schain_index schainIndex );

    ptr< sockaddr_in > getSocketaddr();

    string getBaseIP();
};
