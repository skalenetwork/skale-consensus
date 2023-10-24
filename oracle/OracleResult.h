//
// Created by kladko on 14.01.22.
//

#ifndef SKALED_ORACLERESULT_H
#define SKALED_ORACLERESULT_H

#include "SkaleCommon.h"
class OracleRequestSpec;

class CryptoManager;


class OracleResult {
    ptr< OracleRequestSpec > oracleRequestSpec;

public:
    const ptr< OracleRequestSpec >& getOracleRequestSpec() const;

private:
    string unsignedOracleResult;  // this is exactly the piece which is signed
    string oracleResult;

    int64_t error = ORACLE_SUCCESS;
    ptr< vector< ptr< string > > > results;
    string sig;

    void extractWebResults( string& _response );

    void extractEthCallResults( string& _response );


    void parseResultAsJson();

    // void parseResultAsAbi();

    void trimWebResults();

    void appendElementsFromTheSpecAsJson();

    void appendResultsAsJson();

    void signResultAsJson( ptr< CryptoManager > _cryptoManager );

    void appendErrorAsJson();

    void encodeAndSignResultAsJson( ptr< CryptoManager > _cryptoManager );


public:
    const string getUnsignedOracleResult() const;

    OracleResult( ptr< OracleRequestSpec > _oracleSpec, int64_t _status, string& _serverResponse,
        ptr< CryptoManager > _cryptoManager );


    OracleResult( string& _oracleResult, ptr< OracleRequestSpec > _requestSpec );

    static ptr< OracleResult > parseResult(
        string& _oracleResult, ptr< OracleRequestSpec > _requestSpec );

    const string& getSig() const;


    const string& toString() const;


    uint64_t getTime() const;


    bool isGeth();
};


#endif  // SKALED_ORACLERESULT_H
