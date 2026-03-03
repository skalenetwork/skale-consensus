//
// Created by kladko on 17.06.20.
//

#include "thirdparty/catch.hpp"
#include "Consensust.h"

CATCH_TEST_CASE_METHOD( StartFromScratch, "Run basic consensus", "[consensus-basic][end-to-end]" ) {
    basicRun();
    CATCH_SUCCEED();
}

CATCH_TEST_CASE_METHOD(
    DontCleanup, "Run basic consensus without cleanup", "[consensus-basic-no-cleanup][end-to-end]" ) {
    basicRun();
    CATCH_SUCCEED();
}

CATCH_TEST_CASE_METHOD(
    DontCleanup, "Continue running basic consensus where stopped", "[consensus-basic-continue][end-to-end]" ) {
    basicRun( -1 );
    CATCH_SUCCEED();
}


CATCH_TEST_CASE_METHOD( StartFromScratch, "Run two engines", "[consensus-two-engines][end-to-end]" ) {
    auto lastId = basicRun();
    basicRun( ( int64_t )( uint64_t ) lastId );
    CATCH_SUCCEED();
}

CATCH_TEST_CASE_METHOD( StartFromScratch, "Change schain index", "[change-schain-index][end-to-end]" ) {
    uint64_t lastId = ( uint64_t ) basicRun();
    Consensust::useCorruptConfigs();
    CATCH_REQUIRE_THROWS( basicRun( lastId ) );
    CATCH_SUCCEED();
}


CATCH_TEST_CASE_METHOD(
    StartFromScratch, "Use finalization download only", "[consensus-finalization-download][end-to-end]" ) {
    setenv( "TEST_FINALIZATION_DOWNLOAD_ONLY", "1", 1 );

    engine = new ConsensusEngine( 0, 100000000 );
    engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath() );
    engine->slowStartBootStrapTest();
    usleep( 1000 * Consensust::getRunningTimeS() ); /* Flawfinder: ignore */

    CATCH_REQUIRE( engine->nodesCount() > 0 );
    CATCH_REQUIRE( engine->getLargestCommittedBlockID() > 0 );
    engine->testExitGracefullyBlocking();
    delete engine;
    CATCH_SUCCEED();
}


CATCH_TEST_CASE_METHOD( StartFromScratch, "Get consensus to stuck", "[consensus-stuck][end-to-end]" ) {
    testLog( "Parsing configs" );
    std::thread timer( exit_check );
    try {
        auto startTime = time( NULL );
        engine = new ConsensusEngine( 0, 100000000 );
        engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath() );
        engine->slowStartBootStrapTest();
        auto finishTime = time( NULL );
        if ( finishTime - startTime < STUCK_TEST_TIME ) {
            printf( "Consensus did not get stuck" );
            CATCH_REQUIRE( false );
        }
    } catch ( ... ) {
        timer.join();
    }
    engine->testExitGracefullyBlocking();
    delete engine;
    CATCH_SUCCEED();
}

CATCH_TEST_CASE_METHOD(
    StartFromScratch, "Issue different proposals to different nodes", "[corrupt-proposal][end-to-end]" ) {
    setenv( "CORRUPT_PROPOSAL_TEST", "1", 1 );

    try {
        engine = new ConsensusEngine( 0, 1000000000 );
        engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath() );
        engine->slowStartBootStrapTest();
        usleep( 1000 * Consensust::getRunningTimeS() ); /* Flawfinder: ignore */

        CATCH_REQUIRE( engine->nodesCount() > 0 );
        CATCH_REQUIRE( engine->getLargestCommittedBlockID() == 0 );
        engine->testExitGracefullyBlocking();
        delete engine;
    } catch ( SkaleException& e ) {
        SkaleException::logNested( e );
        throw;
    }

    unsetenv( "CORRUPT_PROPOSAL_TEST" );
    CATCH_SUCCEED();
}

