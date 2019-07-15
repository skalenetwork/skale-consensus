//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"


#include "RandomDB.h"




RandomDB::RandomDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string RandomDB::getFormatVersion() {
    return "1.0";
}
