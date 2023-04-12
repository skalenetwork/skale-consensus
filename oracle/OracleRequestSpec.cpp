//
// Created by kladko on 11.01.22.
//


#include "thirdparty/LUrlParser.h"
#include "thirdparty/json.hpp"
#include "thirdparty/rapidjson/prettywriter.h"  // for stringify JSON

#include "Log.h"
#include "SkaleCommon.h"

#include "crypto/CryptoManager.h"
#include "headers/BasicHeader.h"
#include "network/Utils.h"

#include "OracleRequestSpec.h"
#include "rlp/RLP.h"
#include "utils/Time.h"


ptr< OracleRequestSpec > OracleRequestSpec::parseSpec( const string& _spec ) {

    try {
        auto spec = make_shared< OracleRequestSpec >( _spec );

        return spec;
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void OracleRequestSpec::checkEncoding( const string& _encoding ) {
    ORACLE_CHECK_STATE3( _encoding == "json",
        /// || _encoding == "abi",
        "Unknown encoding " + encoding, ORACLE_UNKNOWN_ENCODING );
}


void OracleRequestSpec::checkEthApi( const string& _ethApi ) {
    if ( _ethApi == string( "eth_call" ) ) {
    } else {
        ORACLE_CHECK_STATE3( false, "Eth Method is not supported:" + _ethApi, ORACLE_ETH_METHOD_NOT_SUPPORTED );
    }
}


bool OracleRequestSpec::isIpAddress(const string& _address) {
    struct in_addr addr;
    int result = inet_pton(AF_INET, _address.c_str(), &addr);
    return (result == 1);
}

void OracleRequestSpec::checkURI( const string& _uri ) {
    ORACLE_CHECK_STATE3( _uri.size() > 5, "Uri too short:" + _uri, ORACLE_URI_TOO_SHORT );
    ORACLE_CHECK_STATE3( _uri.size() <= ORACLE_MAX_URI_SIZE, "Uri too long:" + _uri, ORACLE_URI_TOO_LONG );
    // allow IP based URIs on test networks where real crypto is not used

    ORACLE_CHECK_STATE3( _uri.find( ORACLE_HTTP_START ) == 0 ||
                      _uri.find( ORACLE_HTTPS_START == 0 || _uri == ORACLE_ETH_URL),
        "Invalid URI:" + _uri, ORACLE_INVALID_URI_START);
    if ( _uri != "eth://" && !testMode) {
        auto result = LUrlParser::ParseURL::parseURL( uri );
        ORACLE_CHECK_STATE3( result.isValid(), "URI invalid:" + uri, ORACLE_INVALID_URI );
        ORACLE_CHECK_STATE3( result.userName_.empty(), "Non empty username", ORACLE_USERNAME_IN_URI );
        ORACLE_CHECK_STATE3( result.password_.empty(), "Non empty password", ORACLE_PASSWORD_IN_URI );
        auto host = result.host_;
        ORACLE_CHECK_STATE3(!isIpAddress(host), "IP addresses not allowed in Oracle uris" + _uri,
                            ORACLE_IP_ADDRESS_IN_URI);
    }
}

OracleRequestSpec::OracleRequestSpec( const string& _spec ) : spec( _spec ) {
    try {

        ORACLE_CHECK_STATE3(_spec.size() <= MAX_ORACLE_SPEC_LEN, "Oracle spec too large:" + _spec,
             ORACLE_SPEC_TOO_LARGE);

        // generate receipt
        receipt = CryptoManager::hashForOracle( spec.data(), spec.size() );

        // no parse

        rapidjson::Document d;
        d.Parse( spec.data() );
        ORACLE_CHECK_STATE3(!d.HasParseError(), "Unparsable Oracle spec:" + _spec, ORACLE_UNPARSABLE_SPEC );


        // first check elements required for all calls

        ORACLE_CHECK_STATE3( d.HasMember( "cid" ), "No chainid in Oracle spec:" + _spec,
                             ORACLE_NO_CHAIN_ID_IN_SPEC);
        ORACLE_CHECK_STATE3( d["cid"].IsUint64(), "ChainId in Oracle spec is not uint64_t" + _spec,
                             ORACLE_NON_UINT64__CHAIN_ID_IN_SPEC);
        chainid = d["cid"].GetUint64();

        ORACLE_CHECK_STATE3( d.HasMember( "uri" ), "No URI in Oracle spec:" + _spec,
                             ORACLE_NO_URI_IN_SPEC);
        ORACLE_CHECK_STATE3( d["uri"].IsString(), "Uri in Oracle spec is not string:" + _spec,
                             ORACLE_NON_STRING_URI_IN_SPEC);
        uri = d["uri"].GetString();
        checkURI( uri );


        ORACLE_CHECK_STATE3( d.HasMember( "encoding" ), "No encoding in Oracle spec:" + _spec,
                             ORACLE_NO_ENCODING_IN_SPEC);

        ORACLE_CHECK_STATE3( d["encoding"].IsString(), "Encoding in Oracle spec is not string:" + _spec,
                             ORACLE_NON_STRING_ENCODING_IN_SPEC);
        encoding = d["encoding"].GetString();
        checkEncoding( encoding );


        ORACLE_CHECK_STATE3( d.HasMember( "time" ), "No time pointer in Oracle spec:" + _spec,
                             ORACLE_NO_TIME_IN_SPEC);
        ORACLE_CHECK_STATE3( d["time"].IsUint64(), "time in Oracle spec is not uint64:" + _spec,
                             ORACLE_TIME_IN_SPEC_NO_UINT64)
        requestTime = d["time"].GetUint64();

        ORACLE_CHECK_STATE( requestTime > 0 );


        ORACLE_CHECK_STATE3( d.HasMember( "pow" ), "No  pow in Oracle spec:" + _spec,
                             ORACLE_NO_POW_IN_SPEC);
        ORACLE_CHECK_STATE3( d["pow"].IsUint64(), "Pow in Oracle spec is not uint64:" + _spec,
                             ORACLE_POW_IN_SPEC_NO_UINT64);
        pow = d["pow"].GetUint64();


        ORACLE_CHECK_STATE3( verifyPow( spec ), "PoW did not verify", ORACLE_POW_DID_NOT_VERIFY );


        // no check if ETH or WEB call

        if ( d.HasMember( "ethApi" ) ) {
            ORACLE_CHECK_STATE3( d["ethApi"].IsString(), "ethAPI in Oracle spec is not string:" + _spec,
                                 ORACLE_ETH_API_NOT_STRING);
            ethApi = d["ethApi"].GetString();
            checkEthApi( ethApi );
            parseEthApiRequestSpec( d, _spec );
        } else {
            ORACLE_CHECK_STATE3( uri != "eth://", "No valid eth API method is provided for eth:// URI",
                                 ORACLE_ETH_API_NOT_PROVIDED);
            parseWebRequestSpec( d, _spec );
        }
    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}


void OracleRequestSpec::parseWebRequestSpec( rapidjson::Document& d, const string& _spec ) {
    ORACLE_CHECK_STATE3( d.HasMember( "jsps" ), "No json pointer in Oracle spec:" + _spec,
                         ORACLE_JSPS_NOT_PROVIDED);
    ORACLE_CHECK_STATE3( d["jsps"].IsArray(), "Jsps in Oracle spec is not array:" + _spec,
                         ORACLE_JSPS_NOT_ARRAY);

    auto array = d["jsps"].GetArray();
    ORACLE_CHECK_STATE3( !array.Empty(), "Jsps array is empty.:" + _spec,
                         ORACLE_JSPS_EMPTY);
    ORACLE_CHECK_STATE3( array.Size() <= ORACLE_MAX_JSPS, "Too many elements in JSP array:" + _spec,
                         ORACLE_TOO_MANY_JSPS);

    for ( auto&& item : array ) {
        ORACLE_CHECK_STATE3( item.IsString(), "Jsps array item is not string:" + _spec,
                             ORACLE_JSP_NOT_STRING);
        auto jsp = ( string ) item.GetString();
        ORACLE_CHECK_STATE3( jsp.size() <= ORACLE_MAX_JSP_SIZE, "JSP too long:" + _spec,
                             ORACLE_JSP_TOO_LONG);
        jsps.push_back( jsp );
    }

    // now check optional elements


    if ( d.HasMember( "trims" ) ) {
        auto trimArray = d["trims"].GetArray();
        for ( auto&& item : trimArray ) {
            ORACLE_CHECK_STATE3( item.IsUint64(), "Trims array item is not uint64:" + _spec,
                                 ORACLE_TRIMS_ITEM_NOT_STRING);
            trims.push_back( item.GetUint64() );
        }
        ORACLE_CHECK_STATE3(
            jsps.size() == trims.size(), "hsps array size not equal tp trims array size:" + _spec,
                ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL);
    } else {
        for ( uint64_t i = 0; i < jsps.size(); i++ ) {
            trims.push_back( 0 );
        }
    }

    if ( d.HasMember( "post" ) ) {
        ORACLE_CHECK_STATE3( d["post"].IsString(), "Post in Oracle spec is not a string:" + _spec,
                             ORACLE_POST_NOT_STRING);
        post = d["post"].GetString();
        ORACLE_CHECK_STATE3( post.size() <= ORACLE_MAX_POST_SIZE,
            "Post string is larger than max allowed:" + _spec, ORACLE_POST_STRING_TOO_LARGE );
    }
}


bool OracleRequestSpec::isHexEncodedUInt64( const string& _s ) {
    if ( _s.size() < 3 || _s.size() > 18 ) {
        return false;
    }


    if ( _s.substr( 0, 2 ) != "0x" ) {
        return false;
    }

    auto tmp = _s.substr( 2 );

    for ( const auto& c : tmp ) {
        if ( !std::isxdigit( c ) ) {
            return false;
        }
    }

    return true;
}


bool OracleRequestSpec::isValidEthHexAddressString( const string& _address ) {
    if ( _address.size() != 42 || _address.substr( 0, 2 ) != "0x" ) {
        return false;
    }

    for ( size_t i = 2; i < _address.size(); ++i ) {
        if ( !std::isxdigit( _address[i] ) ) {
            return false;
        }
    }

    return true;
}

void OracleRequestSpec::parseEthApiRequestSpec( rapidjson::Document& d, const string& _spec ) {
    ORACLE_CHECK_STATE3( d.HasMember( "params" ),
        "eth_call request shall include params element, which could be an empty array" + _spec,
        ORACLE_NO_PARAMS_ETH_CALL);

    ORACLE_CHECK_STATE3( d["params"].IsArray(),
        "eth_call request params element is not array" + _spec, ORACLE_PARAMS_NO_ARRAY);

    auto params = d["params"].GetArray();

    ORACLE_CHECK_STATE3( params.Size() == 2, "Params array size must be 2 " + _spec ,
                         ORACLE_PARAMS_ARRAY_INCORRECT_SIZE);


    ORACLE_CHECK_STATE3(
        params[0].IsObject(), "The first element in params array must be object " + _spec,
        ORACLE_PARAMS_ARRAY_FIRST_ELEMENT_NOT_OBJECT);

    this->from = checkAndGetParamsField( params, "from", _spec );
    ORACLE_CHECK_STATE3( isValidEthHexAddressString( from ),
        "From in params array is not a valid Eth address string " + _spec,
                         ORACLE_PARAMS_INVALID_FROM_ADDRESS);
    this->to = checkAndGetParamsField( params, "to", _spec );
    ORACLE_CHECK_STATE3( isValidEthHexAddressString( from ),
        "To in params array is not a valid Eth address string " + _spec,
                         ORACLE_PARAMS_INVALID_TO_ADDRESS);

    this->data = checkAndGetParamsField( params, "data", _spec );

    this->gas = checkAndGetParamsField( params, "gas", _spec );

    ORACLE_CHECK_STATE3( isHexEncodedUInt64( gas ),
        "Gas in params array is not a valid hex encoded uint64_t string " + _spec,
                         ORACLE_PARAMS_GAS_NOT_UINT64);

    ORACLE_CHECK_STATE3( params[0].MemberCount() == 4,
        "The first element in params array must be four elements:"
        " from, to, data and gas", ORACLE_PARAMS_ARRAY_INCORRECT_COUNT );
    ORACLE_CHECK_STATE3( params[1].IsString(),
        "The second element in params array must be a string block number" + _spec,
        ORACLE_BLOCK_NUMBER_NOT_STRING);
    this->blockId = params[1].GetString();
    if ( blockId != "latest" ) {
        ORACLE_CHECK_STATE3( isHexEncodedUInt64( blockId ),
            "The second element in params array must be a hex string block number or latest" +
                _spec, ORACLE_INVALID_BLOCK_NUMBER);
    }
}

string OracleRequestSpec::checkAndGetParamsField(
    const rapidjson::GenericValue< rapidjson::UTF8<> >::Array& params, const string& _fieldName,
    const string& _spec ) {
    ORACLE_CHECK_STATE3( params[0].HasMember( _fieldName.c_str() ),
        "The first element in params array must include " + _fieldName + " field " + _spec,
             ORACLE_MISSING_FIELD );
    ORACLE_CHECK_STATE3(
        params[0][_fieldName.c_str()].IsString(), _fieldName + " field must be string " + _spec,
        ORACLE_INVALID_FIELD);
    return params[0][_fieldName.c_str()].GetString();
}

const string& OracleRequestSpec::getSpec() const {
    return spec;
}

const string& OracleRequestSpec::getUri() const {
    return uri;
}


uint64_t OracleRequestSpec::getTime() const {
    return requestTime;
}

const uint64_t& OracleRequestSpec::getPow() const {
    return pow;
}

const vector< string >& OracleRequestSpec::getJsps() const {
    return jsps;
}

const vector< uint64_t >& OracleRequestSpec::getTrims() const {
    return trims;
}

uint64_t OracleRequestSpec::getChainId() const {
    return chainid;
}


const string& OracleRequestSpec::getPost() const {
    return post;
}

string OracleRequestSpec::whatToPost() {
    if ( isEthApi() ) {
        return createEthCallPostString();
    } else {
        return post;
    }
}


bool OracleRequestSpec::isPost() {
    return ( isEthApi() || !post.empty() );
}

bool OracleRequestSpec::isEthApi() {
    return ( !ethApi.empty() );
}

string OracleRequestSpec::getReceipt() {
    return receipt;
}


bool OracleRequestSpec::verifyPow( string& _spec ) {
    try {
        auto hash = CryptoManager::hashForOracle( _spec.data(), _spec.size() );

        u256 binaryHash( "0x" + hash );

        if ( ~u256( 0 ) / binaryHash > u256( 10000 ) ) {
            return true;
        }
        { return false; }

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

ptr< OracleRequestSpec > OracleRequestSpec::makeSpec( uint64_t _chainId, const string& _uri,
    const vector< string >& _jsps, const vector< uint64_t >& _trims, const string& _post,
    const string& _ethApi, const string& _from, const string& _to, const string& _data,
    const string& _gas, const string& _blockId, const string& _encoding, uint64_t _time ) {
    string spec;

    try {
        // iterate over pow until you get the correct number
        for ( uint64_t pow = 0;; pow++ ) {
            spec = tryMakingSpec( _chainId, _uri, _jsps, _trims, _post, _ethApi, _from, _to, _data,
                _gas, _blockId, _encoding, _time, pow );
            if ( verifyPow( spec ) ) {
                // found the correct value of pow. return spec object
                return make_shared< OracleRequestSpec >( spec );
            }
        }

    } catch ( ... ) {
        throw_with_nested( InvalidStateException( __FUNCTION__, __CLASS_NAME__ ) );
    }
}

string OracleRequestSpec::tryMakingSpec( uint64_t _chainId, const string& _uri,
    const vector< string >& _jsps, const vector< uint64_t >& _trims, const string& _post,
    const string& _ethApi, const string& _from, const string& _to, const string& _data,
    const string& _gas, const string& _blockId, const string& _encoding, uint64_t _time,
    uint64_t _pow ) {
    auto specStr = makeSpecStart( _chainId, _uri );

    if ( _ethApi.empty() ) {
        // web spec
        appendWebPart( specStr, _jsps, _trims, _post );
    } else {
        // ethApi
        CHECK_STATE( _ethApi == "eth_call" )
        specStr.append( string( +"\"ethApi\":\"" ) + _ethApi + "\"," );
        appendEthCallPart( specStr, _from, _to, _data, _gas, _blockId );
    }

    appendSpecEnd( specStr, _encoding, _time, _pow );

    return specStr;
}

void OracleRequestSpec::appendEthCallPart( string& _specStr, const string& _from, const string& _to,
    const string& _data, const string& _gas, const string& _blockId ) {
    _specStr.append( "\"params\":[{" );
    _specStr.append( "\"from\":\"" + _from + "\"," );
    _specStr.append( "\"to\":\"" + _to + "\"," );
    _specStr.append( "\"data\":\"" + _data + "\"," );
    _specStr.append( "\"gas\":\"" + _gas + "\"");
    _specStr.append( "},\"" + _blockId + "\"" );
    _specStr.append( "]," );
}

string OracleRequestSpec::createEthCallPostString() {
    string postString = "{\"jsonrpc\":\"2.0\",\"method\":\"eth_call\",";
    appendEthCallPart( postString, from, to, data, gas, blockId );
    ethCallCounter++;
    postString.append( "\"id\":" + to_string( ethCallCounter ) + "}" );
    return postString;
}


void OracleRequestSpec::appendWebPart( string& _specStr, const vector< string >& _jsps,
    const vector< uint64_t >& _trims, const string& _post ) {
    _specStr.append( string( "\"jsps\":[" ) );

    for ( uint64_t j = 0; j < _jsps.size(); j++ ) {
        _specStr.append( "\"" );
        string jsp = _jsps.at( j );
        _specStr.append( jsp );
        _specStr.append( "\"" );

        // dont append comma to the last element
        if ( j + 1 < _jsps.size() ) {
            _specStr.append( "," );
        }
    }

    _specStr.append( "]," );

    if ( _trims.size() > 0 ) {
        CHECK_STATE2( _trims.size() == _jsps.size(), "Trims and jsps size are not equal");

        _specStr.append( "\"trims\":[" );

        for ( uint64_t j = 0; j < _trims.size(); j++ ) {
            _specStr.append( to_string( _trims.at( j ) ) );
            // dont append comma to the last element
            if ( j + 1 < _trims.size() )
                _specStr.append( "," );
        }

        _specStr.append( "]," );
    }

    if ( !_post.empty() ) {
        _specStr.append( string( "\"post\":\"" ) + _post + "\"," );
    }
}

void OracleRequestSpec::appendSpecEnd(
    string& specStr, const string& _encoding, uint64_t _time, uint64_t _pow ) {
    specStr.append( string( "\"encoding\":\"" ) + _encoding + "\"," );
    specStr.append( string( "\"time\":" ) + to_string( _time ) + "," );
    specStr.append( string( "\"pow\":" ) + to_string( _pow ) );
    specStr.append( "}" );
}

string OracleRequestSpec::makeSpecStart( uint64_t _chainId, const string& _uri ) {
    string specStr( "{" );
    specStr.append( string( "\"cid\":" ) + to_string( _chainId ) + "," );
    specStr.append( string( "\"uri\":\"" ) + _uri + "\"," );
    return specStr;
}

const string& OracleRequestSpec::getEncoding() const {
    return encoding;
}


bool OracleRequestSpec::isEthMainnet() const {
    return uri == "eth://";
}

const string& OracleRequestSpec::getEthApi() const {
    return ethApi;
}



ptr< OracleRequestSpec > OracleRequestSpec::makeWebSpec( uint64_t _chainId, const string& _uri,
    const vector< string >& _jsps, const vector< uint64_t >& _trims, const string& _post,
    const string& _encoding, uint64_t _time ) {
    return makeSpec(
        _chainId, _uri, _jsps, _trims, _post, "", "", "", "", "", "", _encoding, _time );
}


ptr< OracleRequestSpec > OracleRequestSpec::makeEthCallSpec( uint64_t _chainId, const string& _uri,
    const string& _from, const string& _to, const string& _data, const string& _gas,
    const string& _block, const string& _encoding, uint64_t _time ) {
    static vector< string > dummyJsps;
    static vector< uint64_t > dummyTrims;
    return makeSpec( _chainId, _uri, dummyJsps, dummyTrims, "", "eth_call", _from, _to, _data, _gas,
        _block, _encoding, _time );
}

bool OracleRequestSpec::testMode = false;

void OracleRequestSpec::setTestMode() {
    testMode = true;
}
