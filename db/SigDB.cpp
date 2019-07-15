//
// Created by kladko on 7/12/19.
//

#include "../SkaleCommon.h"
#include "../Log.h"

#include "../datastructures/CommittedBlock.h"

#include "SigDB.h"




SigDB::SigDB(string& filename, node_id _nodeId ) : LevelDB( filename, _nodeId ) {}


const string SigDB::getFormatVersion() {
    return "1.0";
}
