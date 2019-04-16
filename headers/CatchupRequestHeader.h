#pragma  once



#include "Header.h"

class SHAHash;
class NodeInfo;
class Schain;

class CatchupRequestHeader : public Header{



    node_id srcNodeID;
    node_id dstNodeID;
    schain_id schainID;
    schain_index srcSchainIndex;
    schain_index dstSchainIndex;
    block_id blockID;

public:


    CatchupRequestHeader();

    CatchupRequestHeader(Schain &_sChain, schain_index _dstIndex);


    void addFields(nlohmann::basic_json<> &j) override;

};