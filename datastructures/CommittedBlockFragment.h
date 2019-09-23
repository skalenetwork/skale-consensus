//
// Created by kladko on 19.09.19.
//

#ifndef SKALED_COMMITTEDBLOCKFRAGMENT_H
#define SKALED_COMMITTEDBLOCKFRAGMENT_H



class CommittedBlockFragment {

    const block_id blockId;

    const uint64_t totalFragments;
    const fragment_index fragmentIndex;

    const ptr<vector<uint8_t>> data;


public:

    CommittedBlockFragment(const block_id &blockId, const uint64_t totalFragments, const fragment_index &fragmentIndex,
                           const ptr<vector<uint8_t>> &data);

    block_id getBlockId() const;

    uint64_t getTotalFragments() const;

    fragment_index getIndex() const;

    ptr<vector<uint8_t>> serialize() const;

};


#endif //SKALED_COMMITTEDBLOCKFRAGMENT_H
