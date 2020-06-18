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

    @file JSONFactory.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

class Node;

class NodeInfo;

class ConsensusExtFace;

class ConsensusEngine;

class JSONFactory {

public:


    static ptr<Node> createNodeFromJson(const fs_path &jsonFile, set<node_id> &nodeIDs, ConsensusEngine *
    _consensusEngine, bool _useSGX = false, ptr<string> _keyName = nullptr, ptr<vector<string>> _publicKeys = nullptr);

    static ptr<Node> createNodeFromJsonObject(const nlohmann::json &j, set<node_id> &nodeIDs, ConsensusEngine *
    _engine,  bool _useSGX = false, ptr<string> _keyName = nullptr, ptr<vector<string>> _publicKeys = nullptr);

    static void createAndAddSChainFromJson(ptr<Node> _node, const fs_path &_jsonFile, ConsensusEngine *_engine);

    static void createAndAddSChainFromJsonObject(ptr<Node> &_node, const nlohmann::json &j, ConsensusEngine *_engine);

    static void parseJsonFile(nlohmann::json &j, const fs_path &configFile);

    static tuple< ptr< vector< string > >,
        ptr<vector<string>>,
        ptr< vector< string > >,
        ptr<vector<array<string,4>>>> parseTestKeyNamesFromJson(const fs_path &configFile,
        uint64_t _totalNodes);

    static pair<ptr<vector<string>>, ptr<vector<string>>> parseAllTestKeyNames(const fs_path & _dir);



};

