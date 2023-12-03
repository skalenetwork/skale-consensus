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

    @file StuckDetectionAgent.h
    @author Stan Kladko
    @date 2020
*/

#pragma once


class Schain;

class StuckDetectionThreadPool;
class LivelinessMonitor;

class StuckDetectionAgent : public Agent {
    ptr< StuckDetectionThreadPool > stuckDetectionThreadPool = nullptr;

public:
    explicit StuckDetectionAgent( Schain& _sChain );

    static void StuckDetectionLoop( StuckDetectionAgent* agent );

    void join();

    uint64_t doStuckCheck(uint64_t _restartIteration );

    void restart( uint64_t _baseRestartTimeMs, uint64_t _iteration );

    void createStuckRestartFile( uint64_t _iteration );

    void cleanupState();

    string restartFileName(uint64_t _iteration );

    bool areTwoThirdsOfPeerNodesOnline();

    bool stuckCheck( uint64_t _restartIntervalMs);

    uint64_t getNumberOfPreviousRestarts();
};
