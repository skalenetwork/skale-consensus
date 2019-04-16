#pragma once





#include "boost/filesystem.hpp"
#include "../Agent.h"
#include "../thirdparty/json.hpp"
#include "../threads/WorkerThreadPool.h"
#include "ConsensusInterface.h"
#include "Node.h"

class BLSPublicKey;
class BLSPrivateKey;


/**
 * Through this interface Consensus interacts with the rest of the system
 */
class ConsensusExtFace {
public:
    typedef std::vector< std::vector< uint8_t > > transactions_vector;

    // Returns hashes and bytes of new transactions; blocks if there are no txns
    virtual transactions_vector pendingTransactions( size_t _limit ) = 0;
    // Creates new block with specified transactions AND removes them from the queue
    virtual void createBlock(const transactions_vector &_approvedTransactions, uint64_t _timeStamp, uint64_t _blockID) = 0;
    virtual ~ConsensusExtFace() = default;

    virtual void terminateApplication() {};
};


class ConsensusEngine : public ConsensusInterface {

    map< node_id, Node* > nodes;


    static void checkExistsAndDirectory( const fs_path& dirname );

    static void checkExistsAndFile( const fs_path& filename );

    Node* readNodeConfigFileAndCreateNode( const fs_path& path, set< node_id >& nodeIDs );

    node_count nodesCount();


    void readSchainConfigFiles( Node& node, const fs_path& dirname );

    ConsensusExtFace* extFace = nullptr;

    block_id lastCommittedBlockID = 0;

    uint64_t lastCommittedBlockTimeStamp = 0;

    string blsPublicKey1;
    string blsPublicKey2;
    string blsPublicKey3;
    string blsPublicKey4;
    string blsPrivateKey;



public:
    ConsensusEngine();

    ~ConsensusEngine() override;

    ConsensusEngine(ConsensusExtFace &_extFace, uint64_t _lastCommittedBlockID,
                    uint64_t _lastCommittedBlockTimeStamp, const string &_blsSecretKey = "",
                    const string &_blsPublicKey1 = "", const string &_blsPublicKey2 = "",
                    const string &_blsPublicKey3 = "", const string &_blsPublicKey4 = "");

    ConsensusExtFace* getExtFace() const;


    void startAll() override;

    void parseFullConfigAndCreateNode( const string& fullPathToConfigFile ) override;

    // used for standalone debugging

    void parseConfigsAndCreateAllNodes(const fs_path &dirname);

    void exitGracefully() override;

    void bootStrapAll() override;

    // tests

    void slowStartBootStrapTest();

    void init() const;

    void joinAllThreads() const;

    const string &getBlsPublicKey1() const;
    const string &getBlsPublicKey2() const;
    const string &getBlsPublicKey3() const;
    const string &getBlsPublicKey4() const;
    const string &getBlsPrivateKey() const;
};
