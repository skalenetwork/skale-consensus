//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_SIGDB_H
#define SKALED_SIGDB_H


#include "LevelDB.h"

class SigDB : public LevelDB{

    node_id nodeId;

    const string getFormatVersion();

public:

    SigDB(string& filename, node_id nodeId );

};



#endif //SKALED_BLOCKDB_H
