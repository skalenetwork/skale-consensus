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


        if ( d.HasMember( "err" ) ) {
            CHECK_STATE2( d["err"].IsUint64(), "Error is not uint64_t" );
            error = d["err"].GetUint64();
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
    try {
        // now encode and sign result.


        CHECK_STATE( _oracleRequestSpec )
        CHECK_STATE( _oracleRequestSpec->getEncoding() == "json" );
        // if (encoding == "json") {
        parseResultAsJson();
        //} else {
        // parseResultAsAbi();
        //}

        if ( oracleRequestSpec->isEthApi() ) {
            CHECK_STATE2( false, "Geth Method not allowed:" );
        }

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


const string& OracleResult::getOracleResult() const {
    return oracleResult;
}

uint64_t OracleResult::getError() const {
    return error;
}

const ptr< vector< ptr< string > > > OracleResult::getResults() const {
    return results;
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
    return sig;
}


void OracleResult::trimResults() {
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
                ( *results )[i] = res;
            }
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::appendElementsFromTheSpecAsJson() {
    try {
        oracleResult = "{";
        oracleResult.append(
            string( "\"cid\":" ) + to_string( oracleRequestSpec->getChainId() ) + "," );
        oracleResult.append( string( "\"uri\":\"" ) + oracleRequestSpec->getUri() + "\"," );
        oracleResult.append( string( "\"jsps\":[" ) );

        for ( uint64_t j = 0; j < oracleRequestSpec->getJsps().size(); j++ ) {
            oracleResult.append( "\"" );
            oracleResult.append( oracleRequestSpec->getJsps().at( j ) );
            oracleResult.append( "\"" );
            if ( j + 1 < oracleRequestSpec->getJsps().size() )
                oracleResult.append( "," );
        }


        oracleResult.append( "]," );
        oracleResult.append( "\"trims\":[" );

        for ( uint64_t j = 0; j < oracleRequestSpec->getTrims().size(); j++ ) {
            oracleResult.append( to_string( oracleRequestSpec->getTrims().at( j ) ) );
            if ( j + 1 < oracleRequestSpec->getTrims().size() )
                oracleResult.append( "," );
        }

        oracleResult.append( "]," );
        oracleResult.append(
            string( "\"time\":" ) + to_string( oracleRequestSpec->getTime() ) + "," );

        if ( !oracleRequestSpec->getPost().empty() ) {
            oracleResult.append( string( "\"post\":" ) + oracleRequestSpec->getPost() + "," );
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

void OracleResult::appendResultsAsJson() {
    try {
        // append results
        oracleResult.append( "\"rslts\":[" );

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

OracleResult::OracleResult( ptr< OracleRequestSpec > _oracleSpec, uint64_t _status,
    string& _serverResponse, ptr< CryptoManager > _cryptoManager )
    : oracleRequestSpec( _oracleSpec ) {
    try {
        CHECK_ARGUMENT( _oracleSpec );

        error = _status;

        if ( _status == ORACLE_SUCCESS ) {
            results = extractResults( _serverResponse );
        }

        if ( !results ) {
            error = ORACLE_INVALID_JSON_RESPONSE;
        } else {
            trimResults();
        }


        // now encode and sign result.
        // if (encoding == "json") {
        CHECK_STATE( oracleRequestSpec->getEncoding() == "json" );
        encodeAndSignResultAsJson( _cryptoManager );
        //}
        // else {
        //    encodeAndSignResultAsAbi(_cryptoManager);
        //}
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

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


ptr< vector< ptr< string > > > OracleResult::extractResults( string& _response ) {
    auto rs = make_shared< vector< ptr< string > > >();


    try {
        auto j = nlohmann::json::parse( _response );
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
                rs->push_back( make_shared< string >( strVal ) );
            } catch ( ... ) {
                rs->push_back( nullptr );
            }
        }

    } catch ( exception& _e ) {
        return nullptr;
    } catch ( ... ) {
        return nullptr;
    }

    return rs;
}

const string OracleResult::getUnsignedOracleResult() const {
    return unsignedOracleResult;
}
const ptr< OracleRequestSpec >& OracleResult::getOracleRequestSpec() const {
    return oracleRequestSpec;
}
