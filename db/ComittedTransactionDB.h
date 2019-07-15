//
// Created by kladko on 7/12/19.
//

#ifndef SKALED_COMMITTEDTRANSACTIONDB_H
#define SKALED_COMMITTEDTRANSACTIONDB_H


#include "LevelDB.h"

class CommittedTransactionDB : public CommittedTransactionDB{

    const string getFormatVersion();

public:

    CommittedTransactionDB(string& filename, node_id nodeId);

};



#endif //SKALED_COMMITTEDTRANSACTIONDB_H
