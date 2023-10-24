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

    @file Buffer.h
    @author Stan Kladko
    @date 2018
*/

#pragma once

class Buffer {
    size_t size = 0;

    uint64_t counter = 0;

    ptr< vector< uint8_t > > buf;

public:
    void write( void* data, size_t dataLen );

    explicit Buffer( size_t size );

    [[nodiscard]] ptr< vector< uint8_t > > getBuf() const;

    void read( void* data, size_t dataLen );

    void consume( char c );

    [[nodiscard]] uint64_t getCounter() const;

    [[nodiscard]] size_t getSize() const;
};
