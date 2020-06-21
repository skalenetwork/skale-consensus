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

    @fileSkaleException.cpp
    @author Stan Kladko
    @date 2018
*/

#include "SkaleException.h"
#include "ExitRequestedException.h"
#include "SkaleCommon.h"
#include "Log.h"

void SkaleException::logNested(const std::exception &e, int level)
{
    string prefix;


    if (level == 0) {
        prefix = "!Exception: ";
    } else {
        prefix = "!Caused by: ";
    }

    if ((dynamic_cast<const ExitRequestedException*>(&e) != nullptr)) {
        LOG(info, string(level, ' ') + prefix + e.what());
    } if (dynamic_cast<const std::nested_exception*>(&e) == nullptr) {
        LOG(err, string(level, ' ') + prefix + e.what());
        return;
    } else {
        LOG(err, string(level, ' ') + prefix + e.what());
    }
    try {
        std::rethrow_if_nested(e);
    } catch(const std::exception& e) {
        logNested(e, level + 1);
    } catch(...) {
        LOG(err, string(level, ' ') + prefix + "Unknown throwable");
    }
}