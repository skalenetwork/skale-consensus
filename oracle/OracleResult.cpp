//
// Created by kladko on 14.01.22.
//

#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/document.h"
#include "thirdparty/rapidjson/prettywriter.h"  // for stringify JSON

#include "Log.h"
#include "OracleRequestSpec.h"
#include "OracleResult.h"
#include "SkaleCommon.h"
#include "crypto/CryptoManager.h"
#include "exceptions/OracleException.h"
#include "network/Utils.h"
#include "rlp/RLP.h"


void OracleResult::parseResultAsJson() {
    // first get the piece which signed by ECDSA
    try {
        auto commaPosition = oracleResult.find_last_of( "," );
        CHECK_STATE( commaPosition != string::npos );
        unsignedOracleResult = oracleResult.substr( 0, commaPosition + 1 );
    } catch ( ... ) {
        throw_with_nested(
            InvalidStateException( "Invalid oracle result" + oracleResult, __CLASS_NAME__ ) );
    }


    try {
        rapidjson::Document d;

        d.Parse( oracleResult.data() );

        CHECK_STATE2( !d.HasParseError(), "Unparsable Oracle result:" + oracleResult );


        CHECK_STATE2( d.HasMember( "sig" ), "No sig in Oracle result:" + oracleResult );

        sig = d["sig"].GetString();

        CHECK_STATE2( d.HasMember( "rslts" ) || d.HasMember( "err" ),
            "No rslts or err in Oracle result:" + oracleResult );

        if ( d.HasMember( "err" ) ) {
            CHECK_STATE2( d["err"].IsInt64(), "Error is not int64_t" );
            error = d["err"].GetInt64();
            return;
        }

        auto resultsArray = d["rslts"].GetArray();

        results = make_shared< vector< ptr< string > > >();
        for ( auto&& item : resultsArray ) {
            if ( item.IsString() ) {
                results->push_back( make_shared< string >( item.GetString() ) );
            } else if ( item.IsNull() ) {
                results->push_back( nullptr );
            } else {
                CHECK_STATE2( false, "Unknown item in results:" + oracleResult )
            }
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException(
            string( __FUNCTION__ ) + "Could not parse JSON result:" + oracleResult,
            __CLASS_NAME__ ) );
    }
}

OracleResult::OracleResult( string& _result, ptr< OracleRequestSpec > _oracleRequestSpec )
    : oracleRequestSpec( _oracleRequestSpec ), oracleResult( _result ) {
    results = make_shared< vector< ptr< string > > >();

    try {
        // now encode and sign result.


        CHECK_STATE( _oracleRequestSpec )
        CHECK_STATE( _oracleRequestSpec->getEncoding() == "json" );
        // if (encoding == "json") {
        parseResultAsJson();
        //} else {
        // parseResultAsAbi();
        //}


    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


const string& OracleResult::toString() const {
    return oracleResult;
}


ptr< OracleResult > OracleResult::parseResult(
    string& _oracleResult, ptr< OracleRequestSpec > _requestSpec ) {
    CHECK_STATE( _requestSpec );
    return make_shared< OracleResult >( _oracleResult, _requestSpec );
}

const string& OracleResult::getSig() const {
    CHECK_STATE( !sig.empty() )
    return sig;
}


void OracleResult::trimWebResults() {
    try {
        CHECK_STATE( results->size() == oracleRequestSpec->getTrims().size() )
        for ( uint64_t i = 0; i < results->size(); i++ ) {
            auto trim = oracleRequestSpec->getTrims().at( i );
            auto res = results->at( i );
            if ( res && trim != 0 ) {
                if ( res->size() <= trim ) {
                    res = make_shared< string >( "" );
                } else {
                    res = make_shared< string >( res->substr( 0, res->size() - trim ) );
                }
                CHECK_STATE( results );
                ( *results )[i] = res;
            }
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::appendElementsFromTheSpecAsJson() {
    try {
        oracleResult = oracleRequestSpec->getSpec();

        // remove the last element, which is PoW
        try {
            auto commaPosition = oracleResult.find_last_of( "," );
            CHECK_STATE( commaPosition != string::npos );
            oracleResult = oracleResult.substr( 0, commaPosition + 1 );
        } catch ( ... ) {
            throw_with_nested(
                InvalidStateException( "Invalid oracle result" + oracleResult, __CLASS_NAME__ ) );
        }

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::appendResultsAsJson() {
    try {
        // append results
        oracleResult.append( "\"rslts\":[" );

        CHECK_STATE( results );

        for ( uint64_t i = 0; i < results->size(); i++ ) {
            if ( i != 0 ) {
                oracleResult.append( "," );
            }


            if ( results->at( i ) ) {
                oracleResult.append( "\"" );
                oracleResult.append( *results->at( i ) );
                oracleResult.append( "\"" );
            } else {
                oracleResult.append( "null" );
            }
        }

        oracleResult.append( "]," ); /**/
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void OracleResult::signResultAsJson( ptr< CryptoManager > _cryptoManager ) {
    try {
        CHECK_ARGUMENT( _cryptoManager )
        CHECK_STATE( oracleResult.at( oracleResult.size() - 1 ) == ',' )
        unsignedOracleResult = oracleResult;
        sig = _cryptoManager->signOracleResult( unsignedOracleResult );
        oracleResult.append( "\"sig\":\"" );
        oracleResult.append( sig );
        oracleResult.append( "\"}" );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::appendErrorAsJson() {
    try {
        oracleResult.append( "\"err\":" );
        oracleResult.append( to_string( error ) );
        oracleResult.append( "," );
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

OracleResult::OracleResult( ptr< OracleRequestSpec > _oracleSpec, int64_t _status,
    string& _serverResponse, ptr< CryptoManager > _cryptoManager )
    : oracleRequestSpec( _oracleSpec ) {
    results = make_shared< vector< ptr< string > > >();

    try {
        CHECK_ARGUMENT( _oracleSpec );

        error = _status;

        if ( _status == ORACLE_SUCCESS ) {
            if ( oracleRequestSpec->isEthApi() ) {
                extractEthCallResults( _serverResponse );
            } else {
                extractWebResults( _serverResponse );
            }

        } else {
            error = _status;
        }


        // now encode and sign result.
        // if (encoding == "json") {
        CHECK_STATE( oracleRequestSpec->getEncoding() == "json" );
        encodeAndSignResultAsJson( _cryptoManager );
        //}
        // else {
        //    encodeAndSignResultAsAbi(_cryptoManager);
        //}
    } catch ( OracleException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::encodeAndSignResultAsJson( ptr< CryptoManager > _cryptoManager ) {
    try {
        appendElementsFromTheSpecAsJson();

        if ( error != ORACLE_SUCCESS ) {
            appendErrorAsJson();
        } else {
            appendResultsAsJson();
        }

        signResultAsJson( _cryptoManager );
        ORACLE_CHECK_STATE3( oracleResult.size() <= MAX_ORACLE_RESULT_LEN,
            "Oracle result too large:" + oracleResult, ORACLE_RESULT_TOO_LARGE );

    } catch ( OracleException& ) {
        throw;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::extractEthCallResults( std::string& _response ) {
    results = make_shared< vector< ptr< string > > >();

    try {
        rapidjson::Document d;

        d.Parse( _response.data() );

        CHECK_STATE2( !d.HasParseError(), "Unparsable response:" + _response );

        CHECK_STATE2( d.HasMember( "result" ) || d.HasMember( "error" ),
            "No result or error in response:" + _response );

        if ( d.HasMember( "result" ) ) {
            CHECK_STATE2(
                d["result"].IsString(), "Result shall be string in response:" + _response );
            results->push_back( make_shared< string >( d["result"].GetString() ) );
        } else {  // error
            CHECK_STATE2( d["error"].IsObject(), "Error shall be object in response:" + _response );
            CHECK_STATE2(
                d["error"]["code"].IsInt64(), "Code shall be Int64 in response" + _response );
            error = d["error"]["code"].GetInt64();
            results->clear();
        }
    } catch ( exception& _e ) {
        results->clear();
        error = ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED;
        return;
    } catch ( ... ) {
        results->clear();
        error = ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED;
        return;
    }
}


void OracleResult::extractWebResults( string& _response ) {
    results = make_shared< vector< ptr< string > > >();

    if ( _response.empty() ) {
        error = ORACLE_EMPTY_JSON_RESPONSE;
        return;
    }

    nlohmann::json j;

    try {
        j = nlohmann::json::parse( _response );
    } catch ( exception& e ) {
        LOG( err, _response + " " + e.what() );
        error = ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED;
    }

    try {
        for ( auto&& jsp : oracleRequestSpec->getJsps() ) {
            auto pointer = nlohmann::json::json_pointer( jsp );
            try {
                auto val = j.at( pointer );
                CHECK_STATE( val.is_primitive() );
                string strVal;
                if ( val.is_string() ) {
                    strVal = val.get< string >();
                } else if ( val.is_number_integer() ) {
                    if ( val.is_number_unsigned() ) {
                        strVal = to_string( val.get< uint64_t >() );
                    } else {
                        strVal = to_string( val.get< int64_t >() );
                    }
                } else if ( val.is_number_float() ) {
                    strVal = to_string( val.get< double >() );
                } else if ( val.is_boolean() ) {
                    strVal = to_string( val.get< bool >() );
                }
                results->push_back( make_shared< string >( strVal ) );
            } catch ( ... ) {
                results->push_back( nullptr );
            }
        }

    } catch ( exception& _e ) {
        results->clear();
        error = ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE;
        LOG( err, "Invalid server response:" + _response + " " + _e.what() );
        return;
    } catch ( ... ) {
        results->clear();
        error = ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE;
        return;
    }

    trimWebResults();
}

const string OracleResult::getUnsignedOracleResult() const {
    return unsignedOracleResult;
}
const ptr< OracleRequestSpec >& OracleResult::getOracleRequestSpec() const {
    return oracleRequestSpec;
}
