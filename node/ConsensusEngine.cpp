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

   @file ConsensusEngine.cpp
   @author Stan Kladko
   @date 2022-
*/


#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/pem.h"


#include <curl/curl.h>

#include "Agent.h"
#include "Log.h"
#include "SkaleCommon.h"
#include "exceptions/FatalError.h"
#include "thirdparty/json.hpp"
#include "threads/GlobalThreadRegistry.h"
#include "oracle/OracleClient.h"

#include "zmq.h"


#pragma GCC diagnostic push
// Suppress warnings: "unknown option after ‘#pragma GCC diagnostic’ kind [-Wpragmas]".
// This is necessary because not all the compilers have the same warning options.
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wdeprecated-register"
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#pragma GCC diagnostic ignored "-Wmismatched-tags"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wtautological-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#pragma GCC diagnostic ignored "-Wunused-private-field"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wignored-attributes"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wchar-subscripts"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wreorder"

#include "bls.h"

#include "datastructures/CommittedBlock.h"
#include "datastructures/TransactionList.h"

#pragma GCC diagnostic pop

#include "dirent.h"

#define BOOST_STACKTRACE_USE_BACKTRACE

#include "boost/stacktrace.hpp"
#include <libBLS/bls/BLSPublicKeyShare.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <libff/common/profiling.hpp>


#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "chains/Schain.h"
#include "exceptions/EngineInitException.h"
#include "json/JSONFactory.h"
#include "libBLS/bls/BLSPrivateKeyShare.h"
#include "libBLS/bls/BLSPublicKey.h"
#include "libBLS/bls/BLSSignature.h"
#include "network/Sockets.h"
#include "network/Utils.h"
#include "network/ZMQSockets.h"
#include "protocols/ProtocolKey.h"
#include "protocols/binconsensus/BinConsensusInstance.h"

#include "tools/utils.h"

#include "exceptions/FatalError.h"

#include "ConsensusEngine.h"

#include "db/StorageLimits.h"

#include "ENGINE_VERSION"

using namespace boost::filesystem;


shared_ptr<spdlog::logger> ConsensusEngine::configLogger = nullptr;

recursive_mutex ConsensusEngine::logMutex;

atomic<uint64_t> ConsensusEngine::engineCounter;


void ConsensusEngine::logInit() {


    engineID = ++engineCounter;

    LOCK(logMutex)

    spdlog::flush_every(std::chrono::seconds(1));

    logThreadLocal_ = nullptr;


    if (dataDir.empty()) {
        char *d = std::getenv("DATA_DIR");

        if (d != nullptr) {
            dataDir = string(d);
            cerr << "Found data dir:" << dataDir << endl;
        }
    }

    if (logDir.empty()) {
        char *d = std::getenv("LOG_DIR");

        if (d != nullptr) {
            logDir = string(d);
            cerr << "Found log dir:" << logDir << endl;
        }
    }


    string logFileName;
    if (engineID > 1) {
        logFileName = "skaled." + to_string(engineID) + ".log";
    } else {
        logFileName = "skaled.log";
    }

    if (!logDir.empty()) {
        logFileNamePrefix = string(logDir + "/" + logFileName);
        logRotatingFileSync = make_shared<spdlog::sinks::rotating_file_sink_mt>(
                logFileNamePrefix, 10 * 1024 * 1024, 5);
    } else {
        logFileNamePrefix = "";
        logRotatingFileSync = nullptr;
    }

    if (!dataDir.empty()) {
        healthCheckDir = dataDir;
        dbDir = dataDir;
    } else {
        healthCheckDir = string("/tmp");
        dbDir = healthCheckDir;
    }


    configLogger = createLogger("config");
}


const string ConsensusEngine::getDataDir() {
    CHECK_STATE(!dataDir.empty())
    return dataDir;
}

const string ConsensusEngine::getLogDir() {
    CHECK_STATE(!logDir.empty());
    return logDir;
}

shared_ptr<spdlog::logger> ConsensusEngine::createLogger(const string &loggerName) {
    shared_ptr<spdlog::logger> logger = spdlog::get(loggerName);

    if (!logger) {
        if (!logFileNamePrefix.empty()) {
            logger = make_shared<spdlog::logger>(loggerName, logRotatingFileSync);
            logger->flush_on(debug);
        } else {
            logger = spdlog::stdout_color_mt(loggerName, spdlog::color_mode::never);
        }
        logger->set_pattern("%+", spdlog::pattern_time_type::utc);
    }

    CHECK_STATE(logger);

    return logger;
}


void ConsensusEngine::setConfigLogLevel(string &_s) {
    auto configLogLevel = SkaleLog::logLevelFromString(_s);
    CHECK_STATE(configLogger != nullptr);
    configLogger->set_level(configLogLevel);
}

void ConsensusEngine::logConfig(
        level_enum _severity, const string &_message, const string &_className) {
    CHECK_STATE(configLogger != nullptr);
    configLogger->log(_severity, _className + ": " + _message);
}

void ConsensusEngine::log(
        level_enum _severity, const string &_message, const string &_className) {
    if (logThreadLocal_ == nullptr) {
        CHECK_STATE(configLogger != nullptr);
        configLogger->log(_severity, _message);
    } else {
        auto engine = logThreadLocal_->getEngine();
        CHECK_STATE(engine);

        string fullMessage =
                to_string((uint64_t) engine->getLargestCommittedBlockID()) + ":" + _message;

        logThreadLocal_->loggerForClass(_className.c_str())->log(_severity, fullMessage);
    }
}


void ConsensusEngine::parseFullConfigAndCreateNode(const string &configFileContents, const string &_gethURL) {
    try {
        nlohmann::json j = nlohmann::json::parse(configFileContents);

        std::set<node_id> dummy;

        ptr<Node> node = nullptr;

        string gethURL = _gethURL;


        node = JSONFactory::createNodeFromJsonObject(j["skaleConfig"]["nodeInfo"], dummy, this,
                                                     this->isSGXEnabled, sgxServerUrl, sgxSSLKeyFileFullPath,
                                                     sgxSSLCertFileFullPath,
                                                     ecdsaKeyName, ecdsaPublicKeys, blsKeyName,
                                                     blsPublicKeys, blsPublicKey, gethURL, previousBlsPublicKeys,
                                                     historicECDSAPublicKeys, historicNodeGroups);

        JSONFactory::createAndAddSChainFromJsonObject(node, j["skaleConfig"]["sChain"], this);

        nodes[node->getNodeID()] = node;

    } catch (SkaleException &e) {
        SkaleException::logNested(e);
        throw;
    }
}

ptr<Node> ConsensusEngine::readNodeTestConfigFileAndCreateNode(const string path,
                                                               set<node_id> &_nodeIDs, bool _useSGX,
                                                               string _sgxSSLKeyFileFullPath,
                                                               string _sgxSSLCertFileFullPath, string _ecdsaKeyName,
                                                               ptr<vector<string> > _ecdsaPublicKeys,
                                                               string _blsKeyName,
                                                               ptr<vector<ptr<vector<string> > > > _blsPublicKeys,
                                                               ptr<BLSPublicKey> _blsPublicKey,
                                                               ptr<map<uint64_t, ptr<BLSPublicKey> > > _previousBlsPublicKeys,
                                                               ptr<map<uint64_t, string> > _historicECDSAPublicKeys,
                                                               ptr<map<uint64_t, vector<uint64_t> > > _historicNodeGroups) {

    _previousBlsPublicKeys = make_shared<map<uint64_t, ptr<BLSPublicKey> > >();
    _historicECDSAPublicKeys = make_shared<map<uint64_t, string>>();
    _historicNodeGroups = make_shared<map<uint64_t, vector<uint64_t>>>();

    try {
        if (_useSGX) {
            CHECK_ARGUMENT(!_ecdsaKeyName.empty() && _ecdsaPublicKeys);
            CHECK_ARGUMENT(!_blsKeyName.empty() && _blsPublicKeys);
            CHECK_ARGUMENT(_blsPublicKey);
        }


        fs_path nodeFileNamePath(path);

        nodeFileNamePath /= string(SkaleCommon::NODE_FILE_NAME);

        checkExistsAndFile(nodeFileNamePath.string());

        fs_path schainDirNamePath(path);

        schainDirNamePath /= string(SkaleCommon::SCHAIN_DIR_NAME);

        checkExistsAndDirectory(schainDirNamePath.string());

        auto node = JSONFactory::createNodeFromTestJsonFile(sgxServerUrl, nodeFileNamePath.string(),
                                                            _nodeIDs, this, _useSGX, _sgxSSLKeyFileFullPath,
                                                            _sgxSSLCertFileFullPath, _ecdsaKeyName,
                                                            _ecdsaPublicKeys, _blsKeyName, _blsPublicKeys,
                                                            _blsPublicKey, _previousBlsPublicKeys,
                                                            _historicECDSAPublicKeys, _historicNodeGroups);


        if (node == nullptr) {
            return nullptr;
        }

        readSchainConfigFiles(node, schainDirNamePath.string());

        CHECK_STATE(nodes.count(node->getNodeID()) == 0);

        nodes[node->getNodeID()] = node;
        return node;

    } catch (SkaleException &e) {
        SkaleException::logNested(e);
        throw;
    }
}


void ConsensusEngine::readSchainConfigFiles(const ptr<Node> &_node, const fs_path &_dirPath) {
    CHECK_ARGUMENT(_node);

    try {
        checkExistsAndDirectory(_dirPath);

        directory_iterator itr(_dirPath);

        auto end = directory_iterator();


        // cycle through the directory

        for (; itr != end; ++itr) {
            fs_path jsonFile = itr->path();
            LOG(debug, "Parsing file:" + jsonFile.string());
            JSONFactory::createAndAddSChainFromJson(_node, jsonFile, this);
            break;
        }

    } catch (...) {
        throw_with_nested(FatalError(__FUNCTION__, __CLASS_NAME__));
    }
}


void ConsensusEngine::checkExistsAndDirectory(const fs_path &_dirPath) {
    if (!exists(_dirPath)) {
        BOOST_THROW_EXCEPTION(FatalError("Not found: " + _dirPath.string()));
    }

    if (!is_directory(_dirPath)) {
        BOOST_THROW_EXCEPTION(FatalError("Not a directory: " + _dirPath.string()));
    }
}


void ConsensusEngine::checkExistsAndFile(const fs_path &_filePath) {
    if (!exists(_filePath)) {
        BOOST_THROW_EXCEPTION(FatalError("Not found: " + _filePath.string()));
    }

    if (is_directory(_filePath)) {
        BOOST_THROW_EXCEPTION(
                FatalError("Path is a direcotry, regular file is required " + _filePath.string()));
    }
}


void ConsensusEngine::parseTestConfigsAndCreateAllNodes(const fs_path &dirname, bool _useBlockIDFromConsensus) {
    try {
        checkExistsAndDirectory(dirname);

        // cycle through the directory

        uint64_t nodeCount = 0;

        directory_iterator itr(dirname);

        auto end = directory_iterator();


        for (; itr != end; itr++) {
            if (!is_directory(itr->path())) {
                BOOST_THROW_EXCEPTION(
                        FatalError("Junk file found. Remove it: " + itr->path().string()));
            }

            if (itr->path().filename().string().find("corrupt") == string::npos)
                nodeCount++;
        };


        bool sgxDirExists = is_directory(dirname.string() + "/../../run_sgx_test/sgx_data");

        string filePath;

        if (sgxDirExists) {
            filePath = dirname.string() + "/../../run_sgx_test/sgx_data" + "/" +
                       to_string(nodeCount) + "node.json";
            if (is_regular_file(filePath)) {
                CHECK_STATE(nodeCount % 3 == 1);
                sgxServerUrl = string("http://localhost:1029");

                sgxSSLKeyFileFullPath =
                        "/d/skale-consensus/run_sgx_test/sgx_data/cert_data/SGXServerCert.key";
                sgxSSLCertFileFullPath =
                        "/d/skale-consensus/run_sgx_test/sgx_data/cert_data/SGXServerCert.crt";

                this->setTestKeys(sgxServerUrl, filePath, nodeCount, nodeCount - 1 / 3);
            }
        }


        if (isSGXEnabled) {
            CHECK_STATE(ecdsaPublicKeys);
            CHECK_STATE(ecdsaKeyNames);
            CHECK_STATE(blsKeyNames);
            CHECK_STATE(blsPublicKeys);
            CHECK_STATE(blsPublicKey);
            CHECK_STATE(nodeCount == ecdsaPublicKeys->size());
        }

        directory_iterator itr2(dirname);


        vector<string> dirNames;

        for (; itr2 != end; itr2++) {
            if (!is_directory(itr2->path())) {
                BOOST_THROW_EXCEPTION(
                        FatalError("Junk file found. Remove it: " + itr2->path().string()));
            }

            if (itr2->path().filename().string().find("corrupt") != string::npos) {
                continue;
            }

            if (!is_directory(itr2->path())) {
                BOOST_THROW_EXCEPTION(
                        FatalError("Junk file found. Remove it: " + itr2->path().string()));
            }

            if (itr2->path().filename().string().find("corrupt") != string::npos) {
                continue;
            }

            dirNames.push_back(itr2->path().string());
        }

        sort(dirNames.begin(), dirNames.end());

        for (uint64_t j = 0; j < dirNames.size(); j++) {
            string ecdsaKey = "";
            string blsKey = "";
            if (isSGXEnabled) {
                CHECK_STATE(j < ecdsaKeyNames->size());
                CHECK_STATE(j < blsKeyNames->size());
                ecdsaKey = string(ecdsaKeyNames->at(j));
                blsKey = string(blsKeyNames->at(j));
            }

            // cert and key file name for tests come from the config
            readNodeTestConfigFileAndCreateNode(dirNames.at(j), nodeIDs, isSGXEnabled, sgxSSLKeyFileFullPath,
                                                sgxSSLCertFileFullPath,
                                                ecdsaKey, ecdsaPublicKeys, blsKey, blsPublicKeys, blsPublicKey);
        };

        if (nodes.size() == 0) {
            BOOST_THROW_EXCEPTION(FatalError("No valid node dirs found"));
        }

        CHECK_STATE(nodeCount == nodes.size());

        BinConsensusInstance::initHistory(nodes.begin()->second->getSchain()->getNodeCount());

        LOG(trace,
            "Parsed configs and created " + to_string(ConsensusEngine::nodesCount()) + " nodes");
    } catch (exception &e) {
        SkaleException::logNested(e);
        throw;
    }

    if (_useBlockIDFromConsensus)
        this->lastCommittedBlockID = getLargestCommittedBlockIDInDb();
}

void ConsensusEngine::startAll() {
    cout << "Starting consensus engine ...";

    try {
        for (auto &&it: nodes) {
            if (it.second->isExitRequested()) {
                return;
            }
            CHECK_STATE(it.second);
            it.second->startServers();
            LOG(info, "Started servers" + to_string(it.second->getNodeID()));
        }


        for (auto &&it: nodes) {
            if (it.second->isExitRequested()) {
                return;
            }
            CHECK_STATE(it.second);
            it.second->startClients();
            LOG(info, "Started clients" + to_string(it.second->getNodeID()));
        }

        LOG(info, "Started node");
    }

    catch (SkaleException &e) {
        SkaleException::logNested(e);
        for (auto &&it: nodes) {
            CHECK_STATE(it.second);
            if (!it.second->isExitRequested()) {
                it.second->exitOnFatalError(e.what());
            }
        }

        throw_with_nested(EngineInitException("Start all failed", __CLASS_NAME__));
    }
}

void ConsensusEngine::slowStartBootStrapTest() {
    for (auto &&it: nodes) {
        CHECK_STATE(it.second);
        it.second->startServers();
    }

    for (auto &&it: nodes) {
        CHECK_STATE(it.second);
        it.second->startClients();
        it.second->getSchain()->bootstrap(lastCommittedBlockID,
                                          lastCommittedBlockTimeStamp->getS(), lastCommittedBlockTimeStamp->getMs());
    }

    LOG(info, "Started all nodes");
}

void ConsensusEngine::bootStrapAll() {
    try {
        for (auto &&it: nodes) {
            LOG(info, "ConsensusEngine: bootstrapping node");
            CHECK_STATE(it.second);
            it.second->getSchain()->bootstrap(lastCommittedBlockID,
                                              lastCommittedBlockTimeStamp->getS(),
                                              lastCommittedBlockTimeStamp->getMs());
            LOG(info, "ConsensusEngine: bootstrapped node");
        }
    } catch (exception &e) {
        for (auto &&it: nodes) {
            CHECK_STATE(it.second);
            if (!it.second->isExitRequested()) {
                it.second->exitOnFatalError(e.what());
            }
        }

        SkaleException::logNested(e);

        throw_with_nested(
                EngineInitException("Consensus engine: bootstrap failed", __CLASS_NAME__));
    }
}


node_count ConsensusEngine::nodesCount() {
    return node_count(nodes.size());
}


std::string ConsensusEngine::exec(const char *cmd) {
    CHECK_ARGUMENT(cmd);

    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        BOOST_THROW_EXCEPTION(std::runtime_error("popen() failed!"));
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


int ConsensusEngine::getOpenDescriptors() {
    int fd_count = 0;
    char buf[64];
    memset(buf, 0, 64);
    struct dirent *dp = 0;

    snprintf(buf, 64, "/proc/%i/fd/", getpid());

    DIR *dir = opendir(buf);
    CHECK_STATE(dir);
    while ((dp = readdir(dir)) != NULL) {
        fd_count++;
    }
    closedir(dir);
    return fd_count;
}


void ConsensusEngine::systemHealthCheck() {

    string ulimit;
    try {
        ulimit = exec("/bin/bash -c \"ulimit -n\"");
    } catch (...) {
        const char *errStr = "Execution of /bin/bash -c ulimit -n failed";
        throw_with_nested(EngineInitException(errStr, __CLASS_NAME__));
    }
    int noFiles = std::strtol(ulimit.c_str(), NULL, 10);

    noUlimitCheck = std::getenv("NO_ULIMIT_CHECK") != nullptr;
    onTravis = std::getenv("CI_BUILD") != nullptr;

    if (noFiles < 65535 && !noUlimitCheck && !onTravis) {
        const char *error =
                "File descriptor limit (ulimit -n) is less than 65535. Set it to 65535 or more as "
                "described"
                " in https://bugs.launchpad.net/ubuntu/+source/lightdm/+bug/1627769\n";
        cerr << error;
        BOOST_THROW_EXCEPTION(EngineInitException(error, __CLASS_NAME__));
    }

    Utils::checkTime();
}

void ConsensusEngine::init() {
    cout << "Consensus engine init(): version:" + ConsensusEngine::getEngineVersion() << endl;

    libff::inhibit_profiling_counters = true;

    libBLS::ThresholdUtils::initCurve();

    threadRegistry = make_shared<GlobalThreadRegistry>();

    logInit();

    sigset_t sigpipe_mask;
    sigemptyset(&sigpipe_mask);
    sigaddset(&sigpipe_mask, SIGPIPE);
    sigset_t saved_mask;
    if (pthread_sigmask(SIG_BLOCK, &sigpipe_mask, &saved_mask) == -1) {
        BOOST_THROW_EXCEPTION(FatalError("Could not block SIGPIPE"));
    }

    systemHealthCheck();
}


ConsensusEngine::ConsensusEngine(block_id _lastId, uint64_t _totalStorageLimitBytes) : prices(256),
                                                                                       exitRequested(false) {


    cout << "Constructing consensus engine:LAST_BLOCK:" << (uint64_t) _lastId << ":TOTAL_STORAGE_LIMIT:" <<
         _totalStorageLimitBytes << endl;


    curl_global_init(CURL_GLOBAL_ALL);


    storageLimits = make_shared<StorageLimits>(_totalStorageLimitBytes);

    lastCommittedBlockTimeStamp = make_shared<TimeStamp>(0, 0);

    try {
        init();

        lastCommittedBlockID = _lastId;
    } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(EngineInitException("Engine construction failed", __CLASS_NAME__));
    }


}

ConsensusEngine::ConsensusEngine(ConsensusExtFace &_extFace, uint64_t _lastCommittedBlockID,
                                 uint64_t _lastCommittedBlockTimeStamp, uint64_t _lastCommittedBlockTimeStampMs,
                                 uint64_t _totalStorageLimitBytes)
        : prices(256), exitRequested(false) {


    std::time_t lastCommitedBlockTimestamp = _lastCommittedBlockTimeStamp;
    cout << "Constructing consensus engine: " << ""
                                                 "Last block in skaled: " << (uint64_t) _lastCommittedBlockID << ' ' <<
         "Last block in skaled timestamp: " << (uint64_t) _lastCommittedBlockTimeStamp << ' ' <<
         "Last block in skaled human readable timestamp: " << std::asctime(std::gmtime(&lastCommitedBlockTimestamp)) <<
         "\n Total storage limit for consensus: " << _totalStorageLimitBytes <<
         endl;


    storageLimits = make_shared<StorageLimits>(_totalStorageLimitBytes);

    // for the first block time stamp shall allways be zero

    CHECK_STATE(
            (uint64_t) _lastCommittedBlockID != 0 ||
            ((_lastCommittedBlockTimeStamp == 0) && (_lastCommittedBlockTimeStampMs == 0)))

    try {
        init();

        CHECK_STATE(_lastCommittedBlockTimeStamp < (uint64_t) 2 * MODERN_TIME);


        extFace = &_extFace;
        lastCommittedBlockID = block_id(_lastCommittedBlockID);

        CHECK_STATE2((_lastCommittedBlockTimeStamp >= MODERN_TIME || _lastCommittedBlockID == 0),
                     "Invalid last committed block time stamp ");


        lastCommittedBlockTimeStamp = make_shared<TimeStamp>(
                _lastCommittedBlockTimeStamp, _lastCommittedBlockTimeStampMs);

    } catch (exception &e) {
        SkaleException::logNested(e);
        throw_with_nested(EngineInitException("Engine construction failed", __CLASS_NAME__));
    }
};


ConsensusExtFace *ConsensusEngine::getExtFace() const {
    return extFace;
}


void ConsensusEngine::exitGracefullyBlocking() {


    LOG(info, "Consensus engine exiting: exitGracefullyBlocking called by skaled");

    cerr << "Here is exitGracefullyBlocking() stack trace for your information:" << endl;

    cerr << boost::stacktrace::stacktrace() << endl;


    // !! if we don't check this - exitGracefullyAsync()
    // will try to exit on deleted object!



    if (getStatus() != CONSENSUS_EXITED)
        exitGracefully();

    while (getStatus() != CONSENSUS_EXITED) {
        usleep(100000);
    }
}


void ConsensusEngine::exitGracefully() {

    LOG(info, "Consensus engine exiting: blocking exit exitGracefully called by skaled");

    cerr << "Here is exitGracefullyBlocking() stack trace for your information:" << endl;

    cerr << boost::stacktrace::stacktrace() << endl;



    // run and forget
    thread([this]() { exitGracefullyAsync(); }).detach();
}

consensus_engine_status ConsensusEngine::getStatus() const {
    return status;
}

void ConsensusEngine::exitGracefullyAsync() {


    LOG(info, "Consensus engine exiting: exitGracefullyAsync called by skaled");

    try {
        auto previouslyCalled = exitRequested.exchange(true);

        if (previouslyCalled) {
            return;
        }

        LOG(info, "exitGracefullyAsync running");


        for (auto &&it: nodes) {
            // run and forget

            auto node = it.second;

            CHECK_STATE(it.second);

            // run and forget

            thread([node]() {
                try {
                    LOG(info, "Node exit called");
                    node->exit();
                    LOG(info, "Node exit completed");
                } catch (exception &e) {
                    SkaleException::logNested(e);
                } catch (...) {
                };
            }).detach();
        }

        CHECK_STATE(threadRegistry);

        threadRegistry->joinAll();

        for (auto &&it: nodes) {
            it.second->getSchain()->joinMonitorAndTimeoutThreads();
        }
    } catch (exception &e) {
        SkaleException::logNested(e);
        status = CONSENSUS_EXITED;
    }
    status = CONSENSUS_EXITED;
}

ConsensusEngine::~ConsensusEngine() {


    exitGracefullyBlocking();

    nodes.clear();

    curl_global_cleanup();

    std::cerr << "ConsensusEngine terminated." << std::endl;

}


block_id ConsensusEngine::getLargestCommittedBlockID() {
    block_id id = 0;

    for (auto &&item: nodes) {
        CHECK_STATE(item.second);

        auto id2 = item.second->getSchain()->getLastCommittedBlockID();

        if (id2 > id) {
            id = id2;
        }
    }

    return id;
}

block_id ConsensusEngine::getLargestCommittedBlockIDInDb() {
    block_id id = 0;

    for (auto &&item: nodes) {
        CHECK_STATE(item.second);
        auto id2 = item.second->getSchain()->readLastCommittedBlockIDFromDb();
        if (id2 > id) {
            id = id2;
        }
    }

    return id;
}


u256 ConsensusEngine::getRandomForBlockId(uint64_t _blockId) const {

    CHECK_STATE(nodes.size() > 0);

    for (auto &&item: nodes) {
        CHECK_STATE(item.second);
        return item.second->getSchain()->getRandomForBlockId(_blockId);
    }
    return 0; // make compiler happy
}

u256 ConsensusEngine::getPriceForBlockId(uint64_t _blockId) const {
    CHECK_STATE(nodes.size() == 1);

    auto priceFromCache = prices.getIfExists(_blockId);

    if (priceFromCache.has_value()) {
        return any_cast<u256>(priceFromCache);
    }

    for (auto &&item: nodes) {
        CHECK_STATE(item.second);
        auto result = item.second->getSchain()->getPriceForBlockId(_blockId);
        prices.putIfDoesNotExist(_blockId, result);
        return result;
    }

    throw std::invalid_argument("Price not found");
}

map< string, uint64_t > ConsensusEngine::getConsensusDbUsage() const {
    auto node = nodes.begin()->second;

    map< string, uint64_t > ret;
    ret["blocks.db_disk_usage"] = node->getBlockDBSize();
    ret["block_proposal.db_disk_usage"] = node->getBlockProposalDBSize();
    ret["block_sigshare.db_disk_usage"] = node->getBlockSigShareDBSize();
    ret["consensus_state.db_disk_usage"] = node->getConsensusStateDBSize();
    ret["da_proof.db_disk_usage"] = node->getDaProofDBSize();
    ret["da_sigshare.db_disk_usage"] = node->getDaSigShareDBSize();
    ret["incoming_msg.db_disk_usage"] = node->getIncomingMsgDBSize();
    ret["interna_info.db_disk_usage"] = node->getInternalInfoDBSize();
    ret["outgoing_msg.db_disk_usage"] = node->getOutgoingMsgDBSize();
    ret["price.db_disk_usage"] = node->getPriceDBSize();
    ret["proposal_hash.db_disk_usage"] = node->getProposalHashDBSize();
    ret["proposal_vector.db_disk_usage"] = node->getProposalVectorDBSize();
    ret["random.db_disk_usage"] = node->getRandomDBSize();

    return ret;
}


bool ConsensusEngine::onTravis = false;

bool ConsensusEngine::noUlimitCheck = false;

bool ConsensusEngine::isOnTravis() {
    return onTravis;
}

[[maybe_unused]] bool ConsensusEngine::isNoUlimitCheck() {
    return noUlimitCheck;
}


set<node_id> &ConsensusEngine::getNodeIDs() {
    return nodeIDs;
}


string ConsensusEngine::getEngineVersion() {
    static string engineVersion(ENGINE_VERSION);
    return engineVersion;
}

uint64_t ConsensusEngine::getEngineID() const {
    return engineID;
}

ptr<GlobalThreadRegistry> ConsensusEngine::getThreadRegistry() const {
    CHECK_STATE(threadRegistry);
    return threadRegistry;
}

const string &ConsensusEngine::getHealthCheckDir() const {
    CHECK_STATE(healthCheckDir != "");
    return healthCheckDir;
}

string ConsensusEngine::getDbDir() const {
    CHECK_STATE(dbDir != "");
    return dbDir;
}

void ConsensusEngine::setTestKeys(
        string _serverURL,
        string _configFile, uint64_t _totalNodes, uint64_t _requiredNodes) {
    CHECK_STATE(!useTestSGXKeys)
    CHECK_STATE(!isSGXEnabled)
    CHECK_STATE(!_serverURL.empty())

    sgxServerUrl = _serverURL;

    tie(ecdsaKeyNames, ecdsaPublicKeys, blsKeyNames, blsPublicKeys, blsPublicKey) =
            JSONFactory::parseTestKeyNamesFromJson(
                    sgxServerUrl, _configFile, _totalNodes, _requiredNodes);

    CHECK_STATE(ecdsaKeyNames);
    CHECK_STATE(ecdsaPublicKeys);
    CHECK_STATE(blsKeyNames);
    CHECK_STATE(blsPublicKeys);
    CHECK_STATE(blsPublicKey);

    isSGXEnabled = true;
    useTestSGXKeys = true;
}

void ConsensusEngine::setSGXKeyInfo(const string &_sgxServerURL, string &_sgxSSLKeyFileFullPath,
                                    string &_sgxSSLCertFileFullPath, string &_ecdsaKeyName, string &_blsKeyName) {
    CHECK_STATE(!_sgxServerURL.empty())
    CHECK_STATE(!_ecdsaKeyName.empty())
    CHECK_STATE(!_blsKeyName.empty())

    this->sgxServerUrl = _sgxServerURL;
    this->isSGXEnabled = true;
    this->useTestSGXKeys = false;

    setEcdsaKeyName(_ecdsaKeyName);
    setBlsKeyName(_blsKeyName);

    sgxSSLCertFileFullPath = _sgxSSLCertFileFullPath;
    sgxSSLKeyFileFullPath = _sgxSSLKeyFileFullPath;
}

void ConsensusEngine::setPublicKeyInfo(ptr<vector<string> > &_ecdsaPublicKeys,
                                       ptr<vector<ptr<vector<string> > > > &_blsPublicKeyShares,
                                       uint64_t _requiredSigners,
                                       uint64_t _totalSigners) {

    CHECK_STATE(_blsPublicKeyShares);
    CHECK_STATE(_ecdsaPublicKeys);
    CHECK_STATE(_ecdsaPublicKeys);
    CHECK_STATE(_totalSigners >= _requiredSigners);


    this->blsPublicKeys = _blsPublicKeyShares;
    this->ecdsaPublicKeys = _ecdsaPublicKeys;


    map<size_t, shared_ptr<BLSPublicKeyShare> > blsPubKeyShares;


    for (uint64_t i = 0; i < _requiredSigners; i++) {
        LOG(info, "Parsing BLS key share:" + blsPublicKeys->at(i)->at(0));

        BLSPublicKeyShare pubKey(blsPublicKeys->at(i), _requiredSigners, _totalSigners);

        blsPubKeyShares[i + 1] = make_shared<BLSPublicKeyShare>(pubKey);
    }

    // create pub key

    blsPublicKey = make_shared<BLSPublicKey>(
            make_shared<map<size_t, shared_ptr<BLSPublicKeyShare> > >(blsPubKeyShares),
            _requiredSigners, _totalSigners);

}


void ConsensusEngine::setRotationHistory(ptr<map<uint64_t, vector<string>>> _previousBLSKeys,
                                         ptr<map<uint64_t, string>> _historicECDSAKeys,
                                         ptr<map<uint64_t, vector<uint64_t>>> _historicNodeGroups) {
    CHECK_STATE(_previousBLSKeys);
    CHECK_STATE(_historicECDSAKeys);
    CHECK_STATE(_historicNodeGroups);

    map<uint64_t, ptr<BLSPublicKey> > _previousBlsPublicKeys;
    for (const auto &previousGroup: *_previousBLSKeys) {
        _previousBlsPublicKeys[previousGroup.first] = make_shared<BLSPublicKey>(
                make_shared<vector<string>>(previousGroup.second));
    }
    previousBlsPublicKeys = make_shared<map<uint64_t, ptr<BLSPublicKey> > >(_previousBlsPublicKeys);
    historicECDSAPublicKeys = _historicECDSAKeys;
    historicNodeGroups = _historicNodeGroups;
}

const string ConsensusEngine::getEcdsaKeyName() const {
    return ecdsaKeyName;
}

const string ConsensusEngine::getBlsKeyName() const {
    return blsKeyName;
}

void ConsensusEngine::setEcdsaKeyName(const string &_ecdsaKeyName) {
    CHECK_ARGUMENT(!_ecdsaKeyName.empty())
    CHECK_STATE(JSONFactory::splitString(_ecdsaKeyName)->size() == 2);
    ecdsaKeyName = _ecdsaKeyName;
}

void ConsensusEngine::setBlsKeyName(const string &_blsKeyName) {
    CHECK_ARGUMENT(!_blsKeyName.empty());
    CHECK_STATE(JSONFactory::splitString(_blsKeyName)->size() == 7);
    blsKeyName = _blsKeyName;
}


ptr<StorageLimits> ConsensusEngine::getStorageLimits() const {
    CHECK_STATE(storageLimits);
    return storageLimits;
}

tuple<ptr<ConsensusExtFace::transactions_vector>, uint32_t, uint32_t, u256, u256>
ConsensusEngine::getBlock(block_id _blockId) {
    CHECK_STATE(nodes.size() > 0)
    auto node = nodes.begin()->second;
    CHECK_STATE(node)

    auto schain = nodes.begin()->second->getSchain();

    CHECK_STATE(schain);

    auto committedBlock = schain->getBlock(_blockId);

    if (!committedBlock) {
        return {nullptr, 0, 0, 0, 0};
    }

    auto timeStampS = committedBlock->getTimeStampS();
    auto timeStampMs = committedBlock->getTimeStampMs();
    auto stateRoot = committedBlock->getStateRoot();
    auto currentPrice = schain->getPriceForBlockId((uint64_t) committedBlock->getBlockID() - 1);
    auto tv = committedBlock->getTransactionList()->createTransactionVector();

    return {tv, timeStampS, timeStampMs, currentPrice, stateRoot};
}


uint64_t ConsensusEngine::submitOracleRequest(const string &_spec, string &_receipt) {
    CHECK_STATE(nodes.size() > 0)
    auto node = nodes.begin()->second;
    CHECK_STATE(node)
    auto oracleClient = node->getSchain()->getOracleClient();

    CHECK_STATE(oracleClient);
    return oracleClient->submitOracleRequest(_spec, _receipt);
}

/*
 * Check if Oracle result has been derived.  This will return ORACLE_SUCCESS if
 * nodes agreed on result. The signed result will be returned in _result string.
 *
 * If no result has been derived yet, ORACLE_RESULT_NOT_READY is returned.
 *
 * In case of an error, an error is returned.
 */


uint64_t ConsensusEngine::checkOracleResult(const string &_receipt, string &_result) {
    CHECK_STATE(nodes.size() > 0)
    auto node = nodes.begin()->second;
    CHECK_STATE(node)
    auto oracleClient = node->getSchain()->getOracleClient();
    CHECK_STATE(oracleClient);
    return oracleClient->checkOracleResult(_receipt, _result);
}
