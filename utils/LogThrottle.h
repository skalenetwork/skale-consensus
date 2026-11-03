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
*/


#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>

// Single-threaded, event-driven throttle. Each shouldLog() call records one event
// and consumes a reporting slot if due; no report is emitted when events stop.
class LogThrottle {
public:
    explicit LogThrottle( std::chrono::steady_clock::duration _interval )
        : interval( _interval ) {
        if ( interval <= std::chrono::steady_clock::duration::zero() )
            throw std::invalid_argument( "LogThrottle interval must be positive" );
    }

    bool shouldLog() {
        return shouldLog( std::chrono::steady_clock::now() );
    }

    // Explicit timestamps must be nondecreasing, as with steady_clock::now().
    bool shouldLog( std::chrono::steady_clock::time_point now ) {
        ++events;
        if ( nextReport && now < *nextReport )
            return false;

        lastReportEvents = events;
        events = 0;
        nextReport = now + interval;
        return true;
    }

    std::uint64_t lastReportEventCount() const noexcept { return lastReportEvents; }

private:
    std::chrono::steady_clock::duration interval;
    std::optional< std::chrono::steady_clock::time_point > nextReport;
    std::uint64_t events = 0;
    std::uint64_t lastReportEvents = 0;
};
