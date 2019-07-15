//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"


#include "PriceDB.h"




PriceDB::PriceDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string PriceDB::getFormatVersion() {
    return "1.0";
}
