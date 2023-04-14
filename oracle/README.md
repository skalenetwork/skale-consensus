# Dynamic Oracle API

## 1. Intro

Dynamic Oracle is implemented by each SKL chain. It is used to retrieve trusted data from websites and blockchains,
and then sign this data by multiple SKL nodes.

### 1.1  Oracle request flow

When a user submit an Oracle request to retrieve data from a network endpoint:

* all nodes in a SKL chain issue requests to retrieve data
* nodes compare data received and verify it is identical
* node create an OracleResult object
* at least 6 nodes must to sign the object
* OracleResult object is returned to the user (typically browser or mobile app)
* OracleResult can then be submitted to the SKL chain and verified in Solidity.

### 1.2 JSON-RPC calls

The following two JSON-RPC calls are implemented by SKL node

* ```oracle_submitRequest``` - this one is used to submit the initial initial Oracle request. 
It returns a receipt object. 

* ```oracle_checkResult``` - this should be called periodically by passing the receipt 
(we recommend once per second) to check if the result is ready.

## 2. oracle_submitRequest

```string oracle_submitRequest( string oracleRequestSpect )```

This API call:
* takes OracleRequestSpec string as input
* returns a string receipt, that shall be used in ```oracle_checkResult```
* In case of an error, an error is returned (see Appendix A Errors)

## 3. oracle_checkResult

```string oracle_checkResult( string receipt )```

This API call

* takes string receipt as input
* returns OracleResult string if the result is ready

Otherwise, one of the following errors is returned


* ```ORACLE_RESULT_NOT_READY``` - OracleResult has not yet been produced 

* ```ORACLE_TIMEOUT``` OracleResult could not be obtained from the endpoint 
* and the timeout was reached


* ```ORACLE_NO_CONSENSUS``` - the endpoint returned different
data to different SKL nodes, so no consensus could be reached on the data

## 4. OracleRequestSpec JSON format.

```OracleRequestSpec``` is a JSON string that is used by the client to 
initiate an Oracle request to a SKL node.

There are two types of request specs. 

* Web spec is used to retrieve info from
web endpoints (http or https), 
* EthApi spec is used to retrieve info from Ethereum API.

Max size of OracleRequestSpec is 1024 bytes.

### 4.1.1 Web request spec

Web request spec is a JSON string that has the following elements

Required elements:

* ```cid```, uint64 - chain ID
* ```uri```, string - Oracle endpoint. Needs to start with "http://" or "https"

If uri starts with http:// or https:// then the information is obtained from the corresponding http:// or https:// endpoint. 
If uri is eth:// then information is obtained from the geth server that the SKALE node is connected to.
Max length of uri  is 1024 bytes.
* ```time```, uint64 - Linux time of request in ms.
* ```jsps```, array of strings - list of string JSON pointers to the data elements to be picked from server response. 
              The array must have from 1 to 32 elements. Max length of each pointer 1024 bytes. 
_See https://json.nlohmann.me/features/json_pointer/ for intro to JSON pointers.


* ```encoding```, string - the only currently supported encoding is```json```. ```abi``` will be supported in future releases. 

Optional elements:

* ```trims```, uint64 array - this is an array of trim values.
   It is used to trim endings of the strings in Oracle result.
   If ```trims``` array is provided, it has to provide trim value for
   each JSON pointer requested. The array size is then identical to ```jsps``` array size. For each ```jsp``` the trim value specifies how many characters are trimmed from the end of the string returned.

* ```post```, string
_if this element, then Oracle with use HTTP POST instead of HTTP GET (default).
   The value of the post element will be posted to the endpoint.

The SPEC shall also include the following element which must be the last element.

* ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks.

Note: for each JSON pointer specified in the request, the Oracle
will

- pick the corresponding element from the endpoint response
- transform it to a string.
- If no such element exists, ```null``` will be returned.

### 4.1.2 EthApi request spec

EthApi request spec represents a JSON-API request to Ethereum API. 
Currently only ```eth_call``` is supported.

EthApi request spec is a JSON string that has the following elements

Required elements:

* ```cid```, uint64 - chain ID
* ```uri```, string - Oracle endpoint.
  _If uri starts with http:// or https:// then the information is obtained from the corresponding
   network endpoint. It is assumed that it supports ETH Api.
  _If uri is eth:// then information is obtained from the geth server that the SKALE node is connected to_.
  _Max length of uri string is 1024 bytes._
* ```time```, uint64 - Linux time of request in ms.
* ```ethApi```, string - this has to have value ```eth_call```
* ```params```, string - these are params to ```eth_call```
* ```encoding```, string - the only currently supported encoding is```json```. ```abi``` will be supported in future releases.
* ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks.
  _Note: PoW must be the last element in JSON_


The SPEC shall also include the following element which must be the last element.

* ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks.

Note: the ```params``` element  is a json array of two elements 

The first element of this array is an object
that must consist of the following four elements:

* ```from``` - from address
* ```to``` - to address
* ```data``` - data
* ```gas``` - gas limit

The second element of the array is string block number, which can
eiher be ```latest``` or a hex string

Here is an example of ```params``` element

```
"params":[{"to":"0x5FbDB2315678afecb367f032d93F642f64180aa3",
"from":"0x9876543210987654321098765432109876543210",
"data":"0x893d20e8", "gas":0x100000},"latest"]
```


## 5. Examples of request specs

### 5.1 Web request using http GET 

HTTP get request that obtains current unix time and
day of the year from worldtimeapi.org.

```json
{
    "cid": 1, "uri": "http://worldtimeapi.org/api/timezone/Europe/Kiev",
    "jsps":["/unixtime", "/day_of_year", "/xxx"],
    "trims":[1,1,1],
    "time":9234567,
    "encoding":"json",
    "pow":53458
}
```

Description:

- contact worldtimeapi.org endpoint.
- From the JSON result, pick ```unixtime```, ```day_of_year``` and ```xxx```
  elements.
- Convert each element to string.
- Trim one character from the end of each string.

### 5.2 Web request using http POST

HTTP post request that posts some data to endpoint

```json
{
    "cid": 1, "uri": "https://reqres.in/api/users", 
    "jsps":["/id"],   
    "time":9234567, 
     "post":"some data",
     "encoding":"json",
     "pow":1735
}
```


### 5.3 EthApi request 

EthApi request doing ```eth_call``` on a smart contract

```json
{
   "cid":1,
   "uri":"https://mygeth.com:1234",
   "ethApi":"eth_call",
   "params":[{"from":"0x9876543210987654321098765432109876543210",
              "to":"0x5FbDB2315678afecb367f032d93F642f64180aa3",
              "data":"0x893d20e8",
              "gas":"0x100000"},
              "latest"],
    "encoding":"json",
    "time":1681494451895,
    "pow":61535
}
```    


## 6. Proof of work computation

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


## 7. OracleResult format

OracleResult copies JSON elements from the corresponding
OracleRequestSpec, stripping away the ```pow``` element.

It then appends to the following elements

1. ```rslts ``` - array of string results. Note for EthAPI ```results``` is a single element array that includes
                  the hex encoded ```DATA``` string which is returned by eth_call. 
2. ```sigs``` - array of ECDSA signatures where ```t``` signatures are not null.


Max size of OracleResult is 3072 bytes.

### 7.1 OracleResult example for Web request. 

An example of Oracle result is provided below

```json
{
  "cid":1, 
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
          null,null,null,null,null,null,null,null,null,null]
}
```

### 7.1 OracleResult example for EthApi request.
```JSON
{
  "cid":1,
  "uri":"https://mygeth.com:1234",,
  "ethApi":"eth_call",
  "params":[
    { "from":"0x9876543210987654321098765432109876543210",
      "to":"0x5FbDB2315678afecb367f032d93F642f64180aa3",
      "data":"0x893d20e8",
      "gas":"0x100000"},
    "latest"],
  "encoding":"json",
  "time":1681494451895, 
  "rslts":["0x000000000000000000000000f39fd6e51aad88f6f4ce6ab8827279cfffb92266"],
  "sigs":["6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    "7d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    "8d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    "9d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    "1050daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    "6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
    null,null,null,null,null,null,null,null,null,null]
}
```
# Appendix A: list of Oracle error codes.

```
 ORACLE_SUCCESS  0
 ORACLE_UNKNOWN_RECEIPT  1
 ORACLE_TIMEOUT 2
 ORACLE_NO_CONSENSUS  3
 ORACLE_UNKNOWN_ERROR  4
 ORACLE_RESULT_NOT_READY 5
 ORACLE_DUPLICATE_REQUEST 6
 ORACLE_COULD_NOT_CONNECT_TO_ENDPOINT 7
 ORACLE_ENDPOINT_JSON_RESPONSE_COULD_NOT_BE_PARSED 8
 ORACLE_INTERNAL_SERVER_ERROR 9
 ORACLE_INVALID_JSON_REQUEST 10
 ORACLE_TIME_IN_REQUEST_SPEC_TOO_OLD 11
 ORACLE_TIME_IN_REQUEST_SPEC_IN_THE_FUTURE 11
 ORACLE_INVALID_CHAIN_ID 12
 ORACLE_REQUEST_TOO_LARGE 13
 ORACLE_RESULT_TOO_LARGE 14
 ORACLE_ETH_METHOD_NOT_SUPPORTED 15
 ORACLE_URI_TOO_SHORT 16
 ORACLE_URI_TOO_LONG 17
 ORACLE_UNKNOWN_ENCODING 18
 ORACLE_INVALID_URI_START 19
 ORACLE_INVALID_URI 20
 ORACLE_USERNAME_IN_URI 21
 ORACLE_PASSWORD_IN_URI 22
 ORACLE_IP_ADDRESS_IN_URI 23
 ORACLE_UNPARSABLE_SPEC 24
 ORACLE_NO_CHAIN_ID_IN_SPEC 25
 ORACLE_NON_UINT64_CHAIN_ID_IN_SPEC 26
 ORACLE_NO_URI_IN_SPEC 27
 ORACLE_NON_STRING_URI_IN_SPEC 28
 ORACLE_NO_ENCODING_IN_SPEC 29
 ORACLE_NON_STRING_ENCODING_IN_SPEC 30
 ORACLE_TIME_IN_SPEC_NO_UINT64 31
 ORACLE_POW_IN_SPEC_NO_UINT64 32
 ORACLE_POW_DID_NOT_VERIFY 33
 ORACLE_ETH_API_NOT_STRING 34
 ORACLE_ETH_API_NOT_PROVIDED 35
 ORACLE_JSPS_NOT_PROVIDED  36
 ORACLE_JSPS_NOT_ARRAY  37
 ORACLE_JSPS_EMPTY  38
 ORACLE_TOO_MANY_JSPS  39
 ORACLE_JSP_TOO_LONG  40
 ORACLE_JSP_NOT_STRING  41
 ORACLE_TRIMS_ITEM_NOT_STRING  42
 ORACLE_JSPS_TRIMS_SIZE_NOT_EQUAL 43
 ORACLE_POST_NOT_STRING 44
 ORACLE_POST_STRING_TOO_LARGE 45
 ORACLE_NO_PARAMS_ETH_CALL 46
 ORACLE_PARAMS_ARRAY_INCORRECT_SIZE 47
 ORACLE_PARAMS_ARRAY_FIRST_ELEMENT_NOT_OBJECT 48
 ORACLE_PARAMS_INVALID_FROM_ADDRESS 49
 ORACLE_PARAMS_INVALID_TO_ADDRESS 50
  ORACLE_PARAMS_ARRAY_INCORRECT_COUNT 51
 ORACLE_BLOCK_NUMBER_NOT_STRING 52
 ORACLE_INVALID_BLOCK_NUMBER 53
 ORACLE_MISSING_FIELD 54
 ORACLE_INVALID_FIELD 55
 ORACLE_EMPTY_JSON_RESPONSE 56
 ORACLE_COULD_NOT_PROCESS_JSPS_IN_JSON_RESPONSE 57
 ORACLE_NO_TIME_IN_SPEC 58
 ORACLE_NO_POW_IN_SPEC 59
 ORACLE_HSPS_TRIMS_SIZE_NOT_EQUAL 60
 ORACLE_PARAMS_NO_ARRAY 61
 ORACLE_PARAMS_GAS_NOT_UINT64 62
```
