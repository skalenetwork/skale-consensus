//
// Created by kladko on 17.06.20.
//


TEST_CASE_METHOD( StartFromScratch, "Run basic consensus", "[consensus-basic]" ) {
    basicRun();
    SUCCEED();
}

TEST_CASE_METHOD(
    DontCleanup, "Run basic consensus without cleanup", "[consensus-basic-no-cleanup]" ) {
    basicRun();
    SUCCEED();
}

TEST_CASE_METHOD(
    DontCleanup, "Continue running basic consensus where stopped", "[consensus-basic-continue]" ) {
    basicRun( -1 );
    SUCCEED();
}


TEST_CASE_METHOD( StartFromScratch, "Run two engines", "[consensus-two-engines]" ) {
    auto lastId = basicRun();
    basicRun( ( int64_t )( uint64_t ) lastId );
    SUCCEED();
}

TEST_CASE_METHOD( StartFromScratch, "Change schain index", "[change-schain-index]" ) {
    uint64_t lastId = ( uint64_t ) basicRun();
    Consensust::useCorruptConfigs();
    REQUIRE_THROWS( basicRun( lastId ) );
    SUCCEED();
}


TEST_CASE_METHOD(
    StartFromScratch, "Use finalization download only", "[consensus-finalization-download]" ) {
    setenv( "TEST_FINALIZATION_DOWNLOAD_ONLY", "1", 1 );

    engine = new ConsensusEngine( 0, 100000000 );
    engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath() );
    engine->slowStartBootStrapTest();
    usleep( 1000 * Consensust::getRunningTimeS() ); /* Flawfinder: ignore */

    REQUIRE( engine->nodesCount() > 0 );
    REQUIRE( engine->getLargestCommittedBlockID() > 0 );
    engine->testExitGracefullyBlocking();
    delete engine;
    SUCCEED();
}


TEST_CASE_METHOD( StartFromScratch, "Get consensus to stuck", "[consensus-stuck]" ) {
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
            REQUIRE( false );
        }
    } catch ( ... ) {
        timer.join();
    }
    engine->testExitGracefullyBlocking();
    delete engine;
    SUCCEED();
}

TEST_CASE_METHOD(
    StartFromScratch, "Issue different proposals to different nodes", "[corrupt-proposal]" ) {
    setenv( "CORRUPT_PROPOSAL_TEST", "1", 1 );

    try {
        engine = new ConsensusEngine( 0, 1000000000 );
        engine->parseTestConfigsAndCreateAllNodes( Consensust::getConfigDirPath() );
        engine->slowStartBootStrapTest();
        usleep( 1000 * Consensust::getRunningTimeS() ); /* Flawfinder: ignore */

        REQUIRE( engine->nodesCount() > 0 );
        REQUIRE( engine->getLargestCommittedBlockID() == 0 );
        engine->testExitGracefullyBlocking();
        delete engine;
    } catch ( SkaleException& e ) {
        SkaleException::logNested( e );
        throw;
    }

    unsetenv( "CORRUPT_PROPOSAL_TEST" );
    SUCCEED();
}
