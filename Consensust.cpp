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

#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/oids.h>
#include <cryptopp/hex.h>


#include "thirdparty/catch.hpp"

#include "SkaleCommon.h"
#include "Log.h"
#include "node/ConsensusEngine.h"
#include "crypto/CryptoManager.h"

#include "iostream"
#include "time.h"
#include "crypto/SHAHash.h"

#include "stubclient.h"
#include <network/Utils.h>
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

    string certDir("/tmp");

    CryptoManager::generateSSLClientCertAndKey(certDir);

    auto certFilePath = certDir + "/cert";
    auto keyFilePath = certDir + "/key";

    CryptoManager::setSGXKeyAndCert(keyFilePath, certFilePath);

    setenv("sgxKeyFileFullPath", keyFilePath.data(), 1);
    setenv("certFileFullPath", certFilePath.data(), 1);

    jsonrpc::HttpClient client("https://localhost:" + to_string(SGX_SSL_PORT));
    auto c  = make_shared<StubClient>(client, jsonrpc::JSONRPC_CLIENT_V2);

    vector<ptr<string>> keyNames;
    vector<ptr<string>> publicKeys;

    using namespace CryptoPP;

    for (int i = 1; i <= 4; i++) {
        auto res = CryptoManager::generateSGXECDSAKey(c);
        auto keyName = res.first;
        auto publicKey = res.second;

        setenv(("sgxECDSAKeyName." + to_string(i)).data(), keyName->data(), 1);
        setenv(("sgxECDSAPublicKey." + to_string(i)).data(), publicKey->data(), 1);

        keyNames.push_back(keyName);
        publicKeys.push_back(publicKey);
    }




    CryptoManager cm( 16, 11, make_shared<string>("127.0.0.1"),
                      make_shared<string>(keyFilePath),
                      make_shared<string>("certFilePath"),
                      keyNames.at(0), publicKeys);

    auto msg = make_shared<vector<uint8_t>>();
    msg->push_back('1');
    auto hash = SHAHash::calculateHash(msg);
    auto sig = cm.sgxSignECDSA(hash,*keyNames[0],  c) ;
    //auto rawSig = Utils::carray2Hex(sig)
    cerr << sig << endl;

    auto key = CryptoManager::decodeSGXPublicKey(publicKeys[0]);

    CryptoPP::ECDSA<CryptoPP::ECP, CryptoPP::SHA256>::Verifier verifier;

    //verifier.VerifyMessage((unsigned char*)msg->data(), msg->length(), (const byte*)signature.data(), signature.size());

    // basicRun();
    SUCCEED();

}