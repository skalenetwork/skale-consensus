#pragma once

class Node;
class NodeInfo;

class ConsensusExtFace;
class ConsensusEngine;

class JSONFactory {

public:


    static Node *createNodeFromJson(const fs_path & jsonFile, set<node_id>& nodeIDs, ConsensusEngine*
    _engine);
    static Node *createNodeFromJsonObject(const nlohmann::json &j, set<node_id>& nodeIDs, ConsensusEngine*
    _engine);

    static void createAndAddSChainFromJson(Node &node, const fs_path &jsonFile, ConsensusEngine* _engine);
    static void createAndAddSChainFromJsonObject(Node &node, const nlohmann::json &j, ConsensusEngine *_engine);

    static void parseJsonFile(nlohmann::json &j, const fs_path &configFile);

};

