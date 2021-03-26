/*
    Copyright (C) 2021 SKALE Labs

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

    @file Timestamp.cpp
    @author Stan Kladko
    @date 2021
*/



#include "SkaleCommon.h"
#include "Log.h"
#include "utils/Time.h"
#include "TimeStamp.h"


TimeStamp::TimeStamp( uint64_t _s, uint64_t _ms ) : s( _s ), ms( _ms ) {}

uint64_t TimeStamp::getS() const {
    return s;
}
uint64_t TimeStamp::getMs() const {
    return ms;
}


bool TimeStamp::operator<( const TimeStamp& r ) const {
    if ( s < r.s )
        return true;
    if ( s > r.s )
        return false;
    return ( ms < r.ms );
}

string TimeStamp::toString() {
    return to_string( s ) + "." + to_string( ms );
}

TimeStamp TimeStamp::getCurrentTimeStamp() {
    auto time = Time::getCurrentTimeMs();

    auto timeS = time / 1000;
    auto timeMS = time % 1000;

    return TimeStamp( timeS, timeMS );
}

TimeStamp  TimeStamp::incrementByMs() {
    uint64_t sec = this->s;
    uint64_t msec = this->ms;

    // Set time for an empty block to be 1 ms more than previous block
    if ( msec == 999 ) {
        sec++;
        msec = 0;
    } else {
        msec++;
    }

    return TimeStamp ( sec, msec );
}
