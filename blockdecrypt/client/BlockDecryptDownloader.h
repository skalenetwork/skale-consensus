/*
    Copyright (C) 2022- SKALE Labs

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

    @file BlockDecryptDownloader.h
    @author Stan Kladko
    @date 2021
*/

/*
    Copyright (C) 2022- SKALE Labs

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

    @file BlockDecryptDownloader.h
    @author Stan Kladko
    @date 2022-
*/

#pragma once



class CommittedBlockList;
class ClientSocket;
class Schain;
class BlockDecryptResponseHeader;
class BlockDecryptionShares;
class BlockDecryptDownloaderThreadPool;
class BlockEncryptedAesKeys;
class BlockDecryptedAesKeys;


#include "sgxwallet/third_party/concurrentqueue.h"
#include "sgxwallet/third_party/readerwriterqueue.h"

#include "datastructures/BlockAesKeyDecryptionSet.h"

class BlockDecryptDownloader : public Agent {

    block_id blockId = 0;

    schain_index proposerIndex = 0;

    ptr<BlockEncryptedAesKeys> encryptedKeys;

    ptr<BlockAesKeyDecryptionSet> decryptionSet;

public:

    ptr<BlockDecryptDownloaderThreadPool> threadPool = nullptr;

    BlockDecryptDownloader(Schain *_sChain, ptr<BlockProposal> _proposal);

    ~BlockDecryptDownloader() override;

    uint64_t downloadDecryptionShare(schain_index _dstIndex);

    static void workerThreadDecryptionDownloadLoop(BlockDecryptDownloader* _agent, schain_index _dstIndex );

    nlohmann::json readBlockDecryptResponseHeader( const ptr< ClientSocket >& _socket );

    block_id getBlockId();

    ptr<BlockDecryptedAesKeys> downloadDecryptedKeys();

};

