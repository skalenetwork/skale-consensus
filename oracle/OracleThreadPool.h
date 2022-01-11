/*
    Copyright (C) 2021- SKALE Labs

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

    @file OraclePusherThreadPool.h
    @author Stan Kladko
    @date 2021
*/

#pragma once


#include "threads/WorkerThreadPool.h"

class OracleThreadPool : public WorkerThreadPool {
public:
    OracleThreadPool(Agent* _agent );

    void createThread( uint64_t _number ) override;
};