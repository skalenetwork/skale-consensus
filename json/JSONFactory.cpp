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

    @file JSONFactory.cpp
    @author Stan Kladko
    @date 2018
*/

#include "../SkaleCommon.h"
#include "../Log.h"
#include "../exceptions/FatalError.h"
#include "../thirdparty/json.hpp"

#include "../network/TransportNetwork.h"
#include "../chains/SchainTest.h"

#include "../chains/Schain.h"
#include "../protocols/InstanceGarbageCollectorAgent.h"

#include "../node/NodeInfo.h"
#include "../node/Node.h"
#include "../node/ConsensusEngine.h"
#include "../network/Sockets.h"
#include "../Log.h"
#include "../exceptions/ParsingException.h"
#include "../exceptions/FatalError.h"

#include "JSONFactory.h"

Node *JSONFactory::createNodeFromJson(const fs_path &jsonFile, set<node_id> &nodeIDs, ConsensusEngine *
_consensusEngine) {

    try {

        nlohmann::json j;

        parseJsonFile(j, jsonFile);

        return createNodeFromJsonObject(j, nodeIDs, _consensusEngine);
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__ + to_string(__LINE__), __CLASS_NAME__));
    }


}

Node *JSONFactory::createNodeFromJsonObject(const nlohmann::json &j, set<node_id> &nodeIDs, ConsensusEngine *
_engine) {

    if (j.find("transport") != j.end()) {
        ptr<string> transport = make_shared<string>(j.at("transport").get<string>());
        TransportNetwork::setTransport(TransportType::ZMQ);
    }


    if (j.find("logLevelConfig") != j.end()) {
        ptr<string> logLevel = make_shared<string>(j.at("logLevelConfig").get<string>());
        Log::setConfigLogLevel(*logLevel);
    }

    uint64_t nodeID = j.at("nodeID").get<uint64_t>();

    Node *node = nullptr;

    if (nodeIDs.empty() || nodeIDs.count(node_id(nodeID)) > 0) {
        try {
            node = new Node(j, _engine);
        } catch (...) {
            throw_with_nested(FatalError("Could not init node", __CLASS_NAME__));
        }
    }

    return node;

}

void JSONFactory::createAndAddSChainFromJson(Node &node, const fs_path &jsonFile, ConsensusEngine *_engine) {
    try {


        nlohmann::json j;

        parseJsonFile(j, jsonFile);

        createAndAddSChainFromJsonObject(node, j, _engine);

        if (j.find("blockProposalTest") != j.end()) {

            string test = j["blockProposalTest"].get<string>();

            if (test == SchainTest::NONE) {

                node.getSchain()->setBlockProposerTest(SchainTest::NONE);
            } else if (test == SchainTest::SLOW) {
                node.getSchain()->setBlockProposerTest(SchainTest::SLOW);
            } else {
                BOOST_THROW_EXCEPTION(
                        ParsingException("Unknown test type parsing schain config:" + test, __CLASS_NAME__));
            }
        }
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}

void JSONFactory::createAndAddSChainFromJsonObject(Node &node, const nlohmann::json &j, ConsensusEngine *_engine) {

    try {

        string prefix = "/";

        if (j.count("skaleConfig") > 0) {
            prefix = "/skaleConfig/nodeInfo/";

        }

        string schainName = j[nlohmann::json::json_pointer(prefix + "schainName")].get<string>();

        schain_id schainID(j[nlohmann::json::json_pointer(prefix + "schainID")].get<uint64_t>());

        ptr<NodeInfo> localNodeInfo = nullptr;

        vector<ptr<NodeInfo>> remoteNodeInfos;

        nlohmann::json nodes = j[nlohmann::json::json_pointer(prefix + "nodes")];

        for (auto it = nodes.begin(); it != nodes.end(); ++it) {

            node_id nodeID((*it)["nodeID"].get<uint64_t>());

            Log::logConfig(trace, to_string(node.getNodeID()) + ": Adding node:" + to_string(nodeID), __CLASS_NAME__);

            ptr<string> ip = make_shared<string>((*it).at("ip").get<string>());

            network_port port((*it)["basePort"].get<int>());

            schain_index schainIndex((*it)["schainIndex"].get<uint64_t>());


            auto rni = make_shared<NodeInfo>(nodeID, ip, port, schainID, schainIndex);

            if (nodeID == node.getNodeID())
                localNodeInfo = rni;

            remoteNodeInfos.push_back(rni);
        }

        ASSERT(localNodeInfo);

        node.initSchain(localNodeInfo, remoteNodeInfos, _engine->getExtFace());
    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }

}


void JSONFactory::parseJsonFile(nlohmann::json &j, const fs_path &configFile) {


    Log::logConfig(debug, "Parsing json file: " + configFile.string(), __CLASS_NAME__);

    ifstream f(configFile.c_str());

    if (f.good()) {
        f >> j;
    } else {
        BOOST_THROW_EXCEPTION(FatalError("Could not find config file: JSON file does not exist " + configFile.string(),
                                         __CLASS_NAME__));
    }
}
