/*
    Copyright (C) 2019 SKALE Labs

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

    @file TestConfig.cpp
    @author Stan Kladko
    @date 2019
*/

#include "SkaleCommon.h"
#include "Log.h"

#include "TestConfig.h"

bool TestConfig::isFinalizationDownloadOnly() const {
    return finalizationDownloadOnly;
}

bool TestConfig::isBlockFinalizeZmqClientEnabled() const {
    return blockFinalizeZmqClientEnabled;
}

bool TestConfig::isBlockFinalizeZmqServerEnabled() const {
    return blockFinalizeZmqServerEnabled;
}

bool TestConfig::isBlockFinalizeTransportStatsEnabled() const {
    return blockFinalizeTransportStatsEnabled;
}

TestConfig::TestConfig( nlohmann::json cgf ) {
    auto option = std::getenv( "TEST_FINALIZATION_DOWNLOAD_ONLY" );
    finalizationDownloadOnly = ( option != nullptr );

    if ( cgf.is_object() ) {
        blockFinalizeZmqClientEnabled =
            cgf.value( "testBlockFinalizeZmqClientEnabled", true );
        blockFinalizeZmqServerEnabled =
            cgf.value( "testBlockFinalizeZmqServerEnabled", true );
        blockFinalizeTransportStatsEnabled =
            cgf.value( "testBlockFinalizeTransportStatsEnabled", false );
    }

    if ( finalizationDownloadOnly ) {
        CONS_LOG( info, "Testing the case of only finalization download" );
    }

    if ( !blockFinalizeZmqClientEnabled ) {
        CONS_LOG( info, "Testing BlockFinalize with ZMQ client disabled" );
    }

    if ( !blockFinalizeZmqServerEnabled ) {
        CONS_LOG( info, "Testing BlockFinalize with ZMQ server disabled" );
    }

    if ( blockFinalizeTransportStatsEnabled ) {
        CONS_LOG( info, "Testing BlockFinalize transport stats collection enabled" );
    }
}
