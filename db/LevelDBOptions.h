/*
    Copyright (C) 2019 -  SKALE Labs

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

    @file StorageLimits.h
    @author Stan Kladko
    @date 2019-
*/

/*
 * Note, since consensus creates 13 databases, and each of them is a rotating DB corresponding
 * to 4 LevelDB objects, consensus keeps in memory 52 LevelDB objects
 *
 * LevelDB by default has max_open_files = 1024, cache_size = 8MB, and writebuffer size = 4 MB
 *
 * Therefore, one would need have 52,000 file descriptors, and 700 MB of memory if all caches are
 * full
 *
 * When consensus run, the caches are gradually filled out. This
 * is subjectively felt as leaking out memory.
 *
 * The solution is to decrease these parameters. Most databases in consensus write less than
 * 1000 bytes per block and do not need this huge cache or write buffer.
 *
 *
 * For two big databases (BlocksDB and BlockProposalDB) we disable levelDB cache totally, and
 * instead cache blocks and block proposals in consensus code on top of LevelDB.
 *
 *
 * In our tests, for a one node under heavy load of more than 2000 TPS consensus
 * memory grows to about 150M and then stays fixed.
 *
 */

#ifndef SKALED_LEVELDBOPTIONS_H
#define SKALED_LEVELDBOPTIONS_H

#include "leveldb/cache.h"
#include "leveldb/db.h"

class LevelDBOptions {
public:
    // Block DB already has cache implemented
    // in consensus code on top of LevelDB.
    // Therefore, LevelDB cache is not needed
    // Typically only latest proposal is ready from the
    // DB, so we can live with less openfiles in LevelD
    static leveldb::Options getBlockDBOptions() {
        leveldb::Options options;

        options.max_open_files = 64;

        // do not use levelDB read cache by setting cache size to 1 byte
        options.block_cache = leveldb::NewLRUCache( 1 );

        options.create_if_missing = true;

        return options;
    }


    // BlockProposal DB already has cache implemented
    // in consensus code von top of LevelDB.
    // Therefore, LevelDB cache is not needed
    // Typically only latest proposal is ready from the
    // DB, so we can live with less openfiles in LevelD
    static leveldb::Options getBlockProposalDBOptions() {
        leveldb::Options options;

        options.max_open_files = 64;

        // do not use levelDB read cache by setting cache size to 1 byte
        options.block_cache = leveldb::NewLRUCache( 1 );

        options.create_if_missing = true;

        return options;
    }


    // The databases below write less than 1000 bytes per block
    // for them we use smaller valued of LevelDB options
    static leveldb::Options getRandomDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getPriceDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getProposalVectorDBOptions() { return getSmallDBOptions(); }


    static leveldb::Options getDAProofDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getConsensusStateDBOptions() { return getSmallDBOptions(); }


    static leveldb::Options getSigDBOptions() { return getSmallDBOptions(); }


    static leveldb::Options getMsgDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getProposalHashDBOptions() { return getSmallDBOptions(); }


    static leveldb::Options getBlockSigShareDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getDASigShareDBOptions() { return getSmallDBOptions(); }


    static leveldb::Options getInternalInfoDBOptions() { return getSmallDBOptions(); }

    static leveldb::Options getSmallDBOptions() {

        leveldb::Options options;

        options.max_open_files = 16;

        options.block_cache = leveldb::NewLRUCache( 8198 );

        options.write_buffer_size = 16384;

        options.create_if_missing = true;

        return options;
    }
};


#endif  // SKALED_LEVELDBOPTIONS_H
