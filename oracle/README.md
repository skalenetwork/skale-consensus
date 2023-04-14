# Oracle API


## Intro

SKALE Oracle is used to retrieve trusted info from websites and blockchains. 

The following two JSON-RPC calls are implemented by ```skaled```

```oracle_submitRequest``` - this one is used to submit the initial initial Oracle request. 
It returns a receipt object. 

```oracle_checkResult``` - this should be called periodically by passing the receipt 
(we recommend once per second) to check if the result is ready.

## oracle_submitRequest

```string oracle_submitRequest( string oracleRequestSpect )```

This API call takes OracleRequestSpec string as input, and returns a string receipt, that
can be used in ```oracle_checkResult```

In case of an error, an error is returned (see Appendix A Errors)

## oracle_checkResult


```string oracle_checkResult( string receipt )```

This API call takes string receipt as result, and return a OracleResult
string if the result is ready. 


```ORACLE_RESULT_NOT_READY``` error is returned if result if not ready 

```ORACLE_TIMEOUT``` is returned if the result could not be obtained 
from the endpoint and the timeout was reached


```ORACLE_NO_CONSENSUS``` is returned is the endpoint returned different
values to different SKALE nodes, so no consensus could be reached on the value

## OracleRequestSpec.

### Oracle request spec description

```OracleRequestSpec``` is a JSON string that is used by client to initiate an Oracle request.

It has the following parameters:

Required elements:

* ```cid```, uint64 - chain ID
* ```uri```, string - Oracle endpoint.
_If uri starts with http:// or https:// then the information is obtained from the corresponding http:// or https:// endpoint_. 
_If uri is eth:// then information is obtained from the geth server that the SKALE node is connected to_.
 _Max length of uri string is 1024 bytes._
* ```time```, uint64 - Linux time of request in ms.
  * ```jsps```, array of strings - list of string JSON pointers to the data elements to be picked from server response. 
                The array must have from 1 to 32 elements. Max length of each pointer 1024 bytes.
                 Note: this element is required for web requests, and shall not be present for EthAPI requests.  
  _See https://json.nlohmann.me/features/json_pointer/ for intro to JSON pointers._
* ```encoding```, string - the only currently supported encoding is```json```. ```abi``` will be supported in future releases. 
* ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks. 
  _Note: PoW must be the last element in JSON_




Optional elements:

* ```trims```, uint64 array - this is an array of trim values.
   It is used to trim endings of the strings in Oracle result.
   If ```trims``` array is provided, it has to provide trim value for
   each JSON pointer requested. The array size is then identical to ```jsps``` array size. For each ```jsp``` the trim value specifies how many characters are trimmed from the end of the string returned.

* ```post```, string
_if this element, then Oracle with use HTTP POST instead of HTTP GET (default).
   The value of the post element will be posted to the endpoint. This element shall not be present in ethApi calls_ 

* ```params```, string array
  _this element shall only be present if eth_call is used. It specifies ```params``` element for ```eth_call```.
   See  [here for more info ](https://ethereum.org/en/developers/docs/apis/json-rpc/#eth_call)_
   
* ```ethApi``` - Ethereum API method to call.  If this element is present, an eth API RPC call will be performed against the endpoint. Valid values for this element are:

```
eth_call
```

### URI element

* If ```uri``` element in the spec starts with ```http://``` or ```https://```, Oracle will retrieve information by doing a http or https to a web endpoint specified by the uri. The endpoint must return a JSON string as a result.

* If ```uri``` element in the spec is equal to with ```eth://```, Oracle will perform a request against Ethereum mainnet.   For this each SKALE node will use the Ethereum mainnet node is is connected to.

## oracle_submitRequest

To submit an Oracle request to a SKALE node, the client submits a string spec 
to oracle_submitRequest API.

### Parameters

1. ```SPEC```, string - request specification

### Parameters example 1

HTTP get request that obtains current unix time and
day of the year from worldtimeapi.org.

```json
    {
    "cid": 1, "uri": "http://worldtimeapi.org/api/timezone/Europe/Kiev",
    "jsps":["/unixtime", "/day_of_year", "/xxx"],
    "trims":[1,1,1],
    "time":9234567,
    "encoding":"json",
    "pow":53458}
```

Description:

- contact worldtimeapi.org endpoint.
- From the JSON result, pick ```unixtime```, ```day_of_year``` and ```xxx```
  elements.
- Convert each element to string.
- Trim one character from the end of each string.

### Parameters example 2

HTTP post request that posts some data to endpoint

```json
    {
    "cid": 1, "uri": "https://reqres.in/api/users", 
    "jsps":["/id"],   
    "time":9234567, 
     "post":"some data",
     "encoding":"json",
     "pow":1735}
```




Note: for each JSON pointer specified in the request, the Oracle
will

- pick the corresponding element from the endpoint response
- transform it to a string.
- If no such element exists, ```null``` will be returned.


## Returned value

When ```oracle_submitRequest``` completes it returns a receipt string
that can be used to check later if the result is ready.

### Receipt example

Here is an example of a receipt:

```
ee188f09d94848ec07644e45bba4934d412f0bef7fa61a299e0b5fe3b2b703ec
```

### Proof of work

The proof of work is an integer value that is selected by
iterating from 0 until the following pseudocode returns true

```c++
bool verifyPow() {
   auto hash = SHA3_256(specString);
   return ~u256(0) / hash > u256(10000);
}
```

Here ~ is bitwise NOT and u256 is unsigned 256 bit number.

specStr is the full JSON spec string, starting from ```{``` and ending with
```}```



## OracleResult JSON String

A JSON string ```ORACLE_RESULT``` is returned, which provides
result signed by ```t + 1``` nodes.

This result can then be provided to a smartcontract for verification.

### Oracle Result JSON elements

Oracle result repeats JSON elements from the corresponding
Oracle request spec, plus includes a set of additional elements

1. ```rslts ``` - array of string results. Note for "eth_call" ```results``` is a single element array that includes
                  the hex encoded ```DATA``` string which is returned by eth_call. 
2. ```sigs``` - array of ECDSA signatures where ```t``` signatures are not null.

### Oracle Result Example

An example of Oracle result is provided below

```
{"cid":1,
 "uri":"http://worldtimeapi.org/api/timezone/Europe/Kiev",
  "jsps":["/unixtime", "/day_of_year", "/xxx"],
  "trims":[1,1,1],"time":1642521456593, "encoding":"json",
  "rslts":["164252145","1",null],
   "sigs":["6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "7d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "8d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "9d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "1050daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
          null,null,null,null,null,null,null,null,null,null]}
```

# Appendix A list of Oracle error codes.

```

#define ORACLE_SUCCESS  0
#define ORACLE_UNKNOWN_RECEIPT  1
#define ORACLE_TIMEOUT 2
#define ORACLE_NO_CONSENSUS  3
#define ORACLE_UNKNOWN_ERROR  4
#define ORACLE_RESULT_NOT_READY 5
#define ORACLE_DUPLICATE_REQUEST 6
#define ORACLE_COULD_NOT_CONNECT_TO_ENDPOINT 7
#define ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED 8
#define ORACLE_INTERNAL_SERVER_ERROR 9
#define ORACLE_INVALID_JSON_REQUEST 10
#define ORACLE_TIME_IN_REQUEST_SPEC_TOO_OLD 11
#define ORACLE_TIME_IN_REQUEST_SPEC_IN_THE_FUTURE 11
#define ORACLE_INVALID_CHAIN_ID 12
#define ORACLE_REQUEST_TOO_LARGE 13
#define ORACLE_RESULT_TOO_LARGE 14
#define ORACLE_ETH_METHOD_NOT_SUPPORTED 15
#define ORACLE_URI_TOO_SHORT 16
#define ORACLE_URI_TOO_LONG 17
#define ORACLE_UNKNOWN_ENCODING 18
#define ORACLE_INVALID_URI_START 19
#define ORACLE_INVALID_URI 20
#define ORACLE_USERNAME_IN_URI 21
#define ORACLE_PASSWORD_IN_URI 22
#define ORACLE_IP_ADDRESS_IN_URI 23
#define ORACLE_UNPARSABLE_SPEC 24
#define ORACLE_NO_CHAIN_ID_IN_SPEC 25
#define ORACLE_NON_UINT64_CHAIN_ID_IN_SPEC 26
#define ORACLE_NO_URI_IN_SPEC 27
#define ORACLE_NON_STRING_URI_IN_SPEC 28
#define ORACLE_NO_ENCODING_IN_SPEC 29
#define ORACLE_NON_STRING_ENCODING_IN_SPEC 30
#define ORACLE_TIME_IN_SPEC_NO_UINT64 31
#define ORACLE_POW_IN_SPEC_NO_UINT64 32
#define ORACLE_POW_DID_NOT_VERIFY 33
#define ORACLE_ETH_API_NOT_STRING 34
#define ORACLE_ETH_API_NOT_PROVIDED 35
#define ORACLE_JSPS_NOT_PROVIDED  36
#define ORACLE_JSPS_NOT_ARRAY  37
#define ORACLE_JSPS_EMPTY  38
#define ORACLE_TOO_MANY_JSPS  39
#define ORACLE_JSP_TOO_LONG  40
#define ORACLE_JSP_NOT_STRING  41
#define ORACLE_TRIMS_ITEM_NOT_STRING  42
#define ORACLE_JSPS_TRIMS_SIZE_NOT_EQUAL 43
#define ORACLE_POST_NOT_STRING 44
#define ORACLE_POST_STRING_TOO_LARGE 45
#define ORACLE_NO_PARAMS_ETH_CALL 46
#define ORACLE_PARAMS_ARRAY_INCORRECT_SIZE 47
#define ORACLE_PARAMS_ARRAY_FIRST_ELEMENT_NOT_OBJECT 48
#define ORACLE_PARAMS_INVALID_FROM_ADDRESS 49
#define ORACLE_PARAMS_INVALID_TO_ADDRESS 50
#define  ORACLE_PARAMS_ARRAY_INCORRECT_COUNT 51
#define ORACLE_BLOCK_NUMBER_NOT_STRING 52
#define ORACLE_INVALID_BLOCK_NUMBER 53
#define ORACLE_MISSING_FIELD 54
#define ORACLE_INVALID_FIELD 55
#define ORACLE_EMPTY_JSON_RESPONSE 56
#define ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE 57
#define ORACLE_NO_TIME_IN_SPEC 58
#define ORACLE_NO_POW_IN_SPEC 59
#define ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL 60
#define ORACLE_PARAMS_NO_ARRAY 61
#define ORACLE_PARAMS_GAS_NOT_UINT64 62
```
