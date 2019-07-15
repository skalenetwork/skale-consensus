//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_PRICEDB_H
#define SKALED_PRICEDB_H


#include "LevelDB.h"

class PriceDB : public LevelDB{

    const string getFormatVersion();

public:

    PriceDB(string& filename, node_id nodeId);

};



#endif //SKALED_RANDOMDB_H
