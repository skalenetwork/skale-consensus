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

    @file TimeStamp.h
    @author Stan Kladko
    @date 2018
*/




#ifndef SKALED_TIMESTAMP_H
#define SKALED_TIMESTAMP_H


class TimeStamp {
    uint64_t s;
    uint64_t ms;

public:
    TimeStamp( uint64_t _s, uint64_t _ms );
    uint64_t getS() const;
    uint64_t getMs() const;

    bool operator<(const TimeStamp& r) const;

    string toString();

    static shared_ptr<TimeStamp> getCurrentTimeStamp();

    ptr<TimeStamp> incrementByMs();

};


#endif  // SKALED_TIMESTAMP_H
