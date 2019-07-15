//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"


#include "CommittedTransactionDB.h"




CommittedTransactionDB::CommittedTransactionDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string CommittedTransactionDB::getFormatVersion() {
    return "1.0";
}
