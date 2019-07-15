//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_RANDOMDB_H
#define SKALED_RANDOMDB_H


#include "LevelDB.h"

class RandomDB : public LevelDB{

    const string getFormatVersion();

public:

    RandomDB(string& filename, node_id nodeId);

};



#endif //SKALED_RANDOMDB_H
