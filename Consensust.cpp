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

    @file Skaled.cpp
    @author Stan Kladko
    @date 2018
*/

#define CATCH_CONFIG_MAIN

#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusEngine.h"

#include "iostream"
#include "time.h"

#include "stubclient.h"
#include "Consensust.h"

#ifdef GOOGLE_PROFILE
#include <gperftools/heap-profiler.h>
#endif

ConsensusEngine *engine;

class StartFromScratch {
public:
    StartFromScratch() {

        int i = system("rm -rf /tmp/*.db.*");
        i = system("rm -rf /tmp/*.db");
        i++; // make compiler happy
        Consensust::setConfigDirPath(boost::filesystem::system_complete("."));

#ifdef GOOGLE_PROFILE
        HeapProfilerStart("/tmp/consensusd.profile");
#endif

    };

    ~StartFromScratch() {
#ifdef GOOGLE_PROFILE
        HeapProfilerStop();
#endif
    }
};

uint64_t Consensust::getRunningTimeMS() {

    if (runningTimeMs == 0) {

        auto env = std::getenv("TEST_TIME_MS");

        if (env != NULL) {
            runningTimeMs = strtoul(env, NULL, 10);
        } else {
            runningTimeMs = DEFAULT_RUNNING_TIME_MS;
        }
    }

    return runningTimeMs;
}

uint64_t Consensust::runningTimeMs = 0;

fs_path Consensust::configDirPath;

const fs_path &Consensust::getConfigDirPath() {
    return configDirPath;
}

void Consensust::setConfigDirPath(const fs_path &_configDirPath) {
    Consensust::configDirPath = _configDirPath;
}


void testLog(const char *message) {
    printf("TEST_LOG: %s\n", message);
}

void basicRun() {
    try {

        REQUIRE(ConsensusEngine::getEngineVersion().size() > 0);

        engine = new ConsensusEngine();
        engine->parseTestConfigsAndCreateAllNodes(Consensust::getConfigDirPath());
        engine->slowStartBootStrapTest();
        sleep(Consensust::getRunningTimeMS()/1000); /* Flawfinder: ignore */

        REQUIRE(engine->nodesCount() > 0);
        REQUIRE(engine->getLargestCommittedBlockID() > 0);
        engine->exitGracefully();
        delete engine;
    } catch (Exception &e) {
        Exception::logNested(e);
        throw;
    }
}


TEST_CASE_METHOD(StartFromScratch, "Run basic consensus", "[consensus-basic]") {
    basicRun();
    SUCCEED();
}

TEST_CASE_METHOD(StartFromScratch, "Run two engines", "[consensus-two-engines]") {
    basicRun();
    basicRun();
    SUCCEED();
}


TEST_CASE_METHOD(StartFromScratch, "Use finalization download only", "[consensus-finalization-download]") {

    setenv("TEST_FINALIZATION_DOWNLOAD_ONLY", "1", 1);

    engine = new ConsensusEngine();
    engine->parseTestConfigsAndCreateAllNodes(Consensust::getConfigDirPath());
    engine->slowStartBootStrapTest();
    usleep(1000 * Consensust::getRunningTimeMS()); /* Flawfinder: ignore */

    REQUIRE(engine->nodesCount() > 0);
    REQUIRE(engine->getLargestCommittedBlockID() > 0);
    engine->exitGracefully();
    delete engine;
    SUCCEED();
}

bool success = false;

void exit_check() {
    sleep(STUCK_TEST_TIME);
    engine->exitGracefully();
}

TEST_CASE_METHOD(StartFromScratch, "Get consensus to stuck", "[consensus-stuck]") {
    testLog("Parsing configs");
    std::thread timer(exit_check);
    try {
        auto startTime = time(NULL);
        engine = new ConsensusEngine();
        engine->parseTestConfigsAndCreateAllNodes(Consensust::getConfigDirPath());
        engine->slowStartBootStrapTest();
        auto finishTime = time(NULL);
        if (finishTime - startTime < STUCK_TEST_TIME) {
            printf("Consensus did not get stuck");
            REQUIRE(false);
        }
    } catch (...) {
        timer.join();
    }
    engine->exitGracefully();
    delete engine;
    SUCCEED();
}

TEST_CASE_METHOD(StartFromScratch, "Issue different proposals to different nodes", "[corrupt-proposal]") {
    setenv("CORRUPT_PROPOSAL_TEST", "1", 1);

    try {
        engine = new ConsensusEngine();
        engine->parseTestConfigsAndCreateAllNodes(Consensust::getConfigDirPath());
        engine->slowStartBootStrapTest();
        usleep(1000 * Consensust::getRunningTimeMS()); /* Flawfinder: ignore */

        REQUIRE(engine->nodesCount() > 0);
        REQUIRE(engine->getLargestCommittedBlockID() == 0);
        engine->exitGracefully();
        delete engine;
    } catch (Exception &e) {
        Exception::logNested(e);
        throw;
    }

    unsetenv("CORRUPT_PROPOSAL_TEST");
    SUCCEED();
}

TEST_CASE_METHOD(StartFromScratch, "Test sgx server connection", "[sgx]") {


    const std::string VALID_CHARS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<int> distribution(0,VALID_CHARS.size() - 1);
    std::string random_string;
    std::generate_n(std::back_inserter(random_string), 10, [&]()
    {
        return VALID_CHARS[distribution(generator)];
    });
    system("bash -c rm -f /tmp/csr /tmp/key /tmp/cert");
    system(("/usr/bin/openssl req -new -sha256 -nodes -out /tmp/csr  -newkey rsa:2048 -keyout /tmp/key -subj /CN="
      + random_string).data());
    string str, csr;
    ifstream file;
    file.open("/tmp/csr");
    while (getline(file, str)) {
        csr.append(str);
        csr.append("\n");
    }

    jsonrpc::HttpClient client("http://localhost:1027");
    StubClient c(client, jsonrpc::JSONRPC_CLIENT_V2);

    auto result = c.SignCertificate(csr);
    int64_t status = result["status"].asInt64();
    REQUIRE(status == 0);
    string hash = result["hash"].asString();
    result = c.GetCertificate(hash);
    status = result["status"].asInt64();
    REQUIRE(status == 0);
    string signedCert = result["cert"].asString();
    ofstream outFile;
    outFile.open("/tmp/cert");
    outFile << signedCert;

    jsonrpc::HttpClient::setKeyFileFullPath("/tmp/key");
    jsonrpc::HttpClient::setCertFileFullPath("/tmp/cert");
    jsonrpc::HttpClient::setSslClientPort(SGX_SSL_PORT);

    setenv("sgxKeyFileFullPath", "/tmp/key", 1);
    setenv("certFileFullPath", "/tmp/key", 1);

    jsonrpc::HttpClient client2("https://localhost:1026");
    StubClient c2(client2, jsonrpc::JSONRPC_CLIENT_V2);

    for (int i = 1; i <= 4; i++) {

        result = c2.generateECDSAKey();

        status = result["status"].asInt64();
        REQUIRE(status == 0);

        vector<string> keyNames;
        vector<string> publicKeys;

        cerr << result << endl;

        string keyName = result["keyName"].asString();
        string publicKey = result["publicKey"].asString();
        REQUIRE(keyName.size() > 10);
        REQUIRE(publicKey.size() > 10);
        REQUIRE(keyName.find("NEK") !=  -1);
        cerr << keyName << endl;
        cerr << publicKey << endl;

        setenv(("sgxECDSAKeyName." + to_string(i)).data(), keyName.data(), 1);
        setenv(("sgxECDSAPublicKey." + to_string(i)).data(), publicKey.data(), 1);

        keyNames.push_back(keyName);
        publicKeys.push_back(publicKey);
    }

    // basicRun();
    SUCCEED();

}