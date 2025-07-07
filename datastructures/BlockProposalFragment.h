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

    @file BlockProposalFragment.h
    @author Stan Kladko
    @date 2019
*/
#ifndef SKALED_BLOCKPROPOSALFRAGMENT_H
#define SKALED_BLOCKPROPOSALFRAGMENT_H


#ifdef BITE
#include <flatb/committed_block_fragment_generated.h>
class AESKeyDecryptionShare;
class AESKeyDecryptionShareList;
class BiteManager;

namespace skale_fb {
    struct CommittedBlockFragment;
}
#endif

class BlockProposalFragment {
    ptr<vector<uint8_t> > data; // tsafe

    const block_id blockId = 0;
#ifdef BITE
    const schain_index proposerIndex = 0;
    const schain_index decryptorIndex = 0;
    ptr<AESKeyDecryptionShareList> decryptionShares;
#endif
    const uint64_t blockSize = 0;
    string blockHash;

    const uint64_t totalFragments = 0;
    const fragment_index fragmentIndex = 0;

#ifdef BITE
    const skale_fb::CommittedBlockFragment *fbBlockFragment = nullptr;
    ptr<vector<uint8_t> > _fbSerializedBlockFragment;

    void deserializeFromFlatBuffer(ptr<BiteManager> _biteManager);

#endif

public:
    // used to deserialze fragment coming from the network
    BlockProposalFragment(const block_id &_blockId,
#ifdef BITE
                          const schain_index _proposerIndex, const schain_index _decryptorIndex,
                          ptr<BiteManager> _biteManager,
#endif
                          uint64_t _totalFragments, const fragment_index &fragmentIndex,
                          const ptr<vector<uint8_t> > &_data, uint64_t _blockSize, const string &_blockHash);


    // used to create and serialize a fragment to be sent to the network

    BlockProposalFragment(const block_id &_blockId,
#ifdef BITE
                          schain_index _proposerIndex, schain_index _decryptorIndex,
                          ptr<AESKeyDecryptionShareList> _decryptionShares,
#endif
                          uint64_t _totalFragments, const fragment_index &_fragmentIndex,
                          const ptr<vector<uint8_t> > &_data, uint64_t _blockSize,
                          const string &_blockHash);

#ifdef BITE
    [[nodiscard]] uint64_t size() const {
        CHECK_STATE(this->fbBlockFragment);
        auto d = fbBlockFragment->data();
        if (!d) {
            return 0;
        } else {
            return data->size();
        }
    }

    auto getFBSerializedData() { return fbBlockFragment->data(); }

    const ptr<AESKeyDecryptionShareList> &getDecryptionShares() const { return decryptionShares; }
#endif

    [[nodiscard]] block_id getBlockId() const;

    [[nodiscard]] uint64_t getTotalFragments() const;

    [[nodiscard]] fragment_index getIndex() const;

    [[nodiscard]] ptr<vector<uint8_t> > serialize(
#ifdef BITE
    bool needDecryptionShares,
    bool needFragment
#endif
    );

    [[nodiscard]] uint64_t getBlockSize() const;

    [[nodiscard]] string getBlockHash() const;
};


#endif  // SKALED_BLOCKPROPOSALFRAGMENT_H
