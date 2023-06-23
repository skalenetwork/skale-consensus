//
// Created by kladko on 04.03.21.
//

#include "StatusServer.h"


/*************************************************************************
 * libjson-rpc-cpp
 *************************************************************************
 * @file    stubserver.cpp
 * @date    02.05.2013
 * @author  Peter Spiess-Knafl <dev@spiessknafl.at>
 * @license See attached LICENSE.txt
 ************************************************************************/
#include <iostream>


using namespace jsonrpc;
using namespace std;


StatusServer::StatusServer(
    Schain* _sChain, AbstractServerConnector& _connector, serverVersion_t _type )
    : AbstractStatusServer( _connector, _type ) {
    CHECK_ARGUMENT( _sChain );
    sChain = _sChain;
}

string StatusServer::consensus_getTPSAverage() {
    CHECK_STATE( sChain );
    return to_string( sChain->getTpsAverage() );
}


string StatusServer::consensus_getBlockSizeAverage() {
    CHECK_STATE( sChain );
    return to_string( sChain->getBlockSizeAverage() );
};
string StatusServer::consensus_getBlockTimeAverageMs() {
    CHECK_STATE( sChain );
    return to_string( sChain->getBlockTimeAverageMs() );
};