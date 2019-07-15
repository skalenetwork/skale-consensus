//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_COMMITTEDTRANSACTIONDB_H
#define SKALED_COMMITTEDTRANSACTIONDB_H


#include "LevelDB.h"

class SigDB : public SigDB{

    node_id nodeId;

    const string getFormatVersion();

public:

    SigDB(node_id nodeId,  string& filename );

};



#endif //SKALED_COMMITTEDTRANSACTIONDB_H
