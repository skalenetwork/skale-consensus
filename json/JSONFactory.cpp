#include "../SkaleConfig.h"
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

    nlohmann::json j;

    parseJsonFile(j, jsonFile);

    return createNodeFromJsonObject(j, nodeIDs, _consensusEngine);


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
        node = new Node(j, _engine);
    }

    return node;

}

void JSONFactory::createAndAddSChainFromJson(Node &node, const fs_path &jsonFile, ConsensusEngine *_engine) {

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
            BOOST_THROW_EXCEPTION(ParsingException("Unknown test type parsing schain config:" + test, __CLASS_NAME__));
        }
    }

}

void JSONFactory::createAndAddSChainFromJsonObject(Node &node, const nlohmann::json &j, ConsensusEngine *_engine) {

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

        LOG(info, "Adding node:" + to_string(nodeID));


        ptr<string> ip = make_shared<string>((*it).at("ip").get<string>());

        network_port port((*it)["basePort"].get<int>());

        schain_index schainIndex((*it)["schainIndex"].get<uint64_t>());


        auto rni = make_shared<NodeInfo>(nodeID, ip, port, schainID, schainIndex);

        if (nodeID == node.getNodeID()) {
            localNodeInfo = rni;
            LOG(debug, "Comparing node info to information in schain ");
            ASSERT(*rni->getBaseIP() == *node.getBindIP());
            ASSERT(rni->getPort() == node.getBasePort());
        };

        remoteNodeInfos.push_back(rni);
    }

    ASSERT(localNodeInfo);

    node.initSchain(localNodeInfo, remoteNodeInfos, _engine->getExtFace());

}


void JSONFactory::parseJsonFile(nlohmann::json &j, const fs_path &configFile) {


    LOG(debug, "Parsing json file: " + configFile.string());

    ifstream f(configFile.c_str());

    if (f.good()) {
        f >> j;
    } else {
        BOOST_THROW_EXCEPTION(FatalError("Could not find config file: JSON file does not exist " + configFile.string(),
                                         __CLASS_NAME__));
    }
}
