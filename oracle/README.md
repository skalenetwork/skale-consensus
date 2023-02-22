# Oracle API

The following two JSON-RPC calls are implemented by ```skaled```

## OracleRequestSpec.

### Oracle request spec description

$OracleRequestSpec$ is a JSON string that is used by client to initiate an Oracle request.

It has the following parameters:

Required elements:

* ```cid```, uint64 - chain ID
* ```uri```, string - Oracle endpoint (must start with http:// or https:// or eth://). Max length 1024 bytes. See a separate section for uri format.
* ```time```, uint64 - Linux time of request in ms
* ```jsps```, array of strings - list of string JSON pointers to the data elements to be picked from server response. Must have from 1 to 32 elements. Max length of each pointer 1024 bytes.
* ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks

See https://json.nlohmann.me/features/json_pointer/ for intro to
JSON pointers.


Optional elements:

* ```trims```, uint64 array - this is an array of trim values.
   It is used to trim endings of the strings in Oracle result.
   If ```trims``` array is provided, it has to provide trim value for
   each JSON pointer requested. Therefore, the array size needs to be identical to ```jsps``` array size.


* ```post```, string - if this element is provided, the
   Oracle with use HTTP POST instead of HTTP GET (default).
   The value of the ```post``` element will be POSTed to the endpoint. Max length 1024 bytes.


* ```encoding```, string - how to encode the result. Supported encodings
   are ```json``` and ```rlp```. JSON encoding is easy to analyze while rlp encoding is more efficient 
   from the point of view of Solidity verification. Is the element is not present, RLP encoding is used.
   
* ```params```, string - if ```eth_call``` or ```eth_getBalance``` are used, this element is required to provide params as described [here](https://ethereum.org/en/developers/docs/apis/json-rpc/#eth_call)
   
   
### URI element

* If ```uri``` element in the spec starts with ```http://``` or ```https://```, Oracle will retrieve information by doing a http or https to a web endpoint specified by the uri. The endpoint must return a JSON string as a result.

* If ```uri``` element in the spec starts with ```eth://``` Oracle will perform a request to the endpoint assuming that endpoint belongs to an eth compatible node. 


The following ETH API calls are supported



```
eth_call
eth_gasPrice
eth_blockNumber
eth_getBalance
```



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

### Geth

To get information from SKALE network geth servers, one need to
use ```geth://``` in URI".

The following JSON-RPC endpoint are available in the first release:


## oracle_checkResult

Check if Oracle result has been derived. By default the result is signed
by ```t+1``` nodes, where ```t``` is the max number of untruthful nodes.
Each node signs using its ETH wallet ECDSA key.

If no result has been derived yet, ```ORACLE_RESULT_NOT_READY``` is returned.

The client is supposed to wait 1 second and try again.

### Parameters

1. ```receipt```, string - receipt, returned by a call to ```oracle_submitRequest```

### Oracle Result JSON String

A JSON string ```ORACLE_RESULT``` is returned, which provides
result signed by ```t + 1``` nodes.

This result can then be provided to a smartcontract for verification.

### Oracle Result JSON elements

Oracle result repeats JSON elements from the corresponding
Oracle request spec, plus includes a set of additional elements

1. ```rslts ``` - array of string results
2. ```sigs``` - array of ECDSA signatures where ```t``` signatures are not null.

### Oracle Result Example

An example of Oracle result is provided below

```
{"cid":1,
 "uri":"http://worldtimeapi.org/api/timezone/Europe/Kiev",
  "jsps":["/unixtime", "/day_of_year", "/xxx"],
  "trims":[1,1,1],"time":1642521456593,
  "rslts":["164252145","1",null],
   "sigs":["6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "7d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "8d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "9d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "1050daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
          null,null,null,null,null,null,null,null,null,null]}
```

# List of Oracle error codes.

```
#define ORACLE_UNKNOWN_RECEIPT  1
#define ORACLE_TIMEOUT 2
#define ORACLE_NO_CONSENSUS  3
#define ORACLE_UNKNOWN_ERROR  4
#define ORACLE_RESULT_NOT_READY 5
#define ORACLE_DUPLICATE_REQUEST 6
#define ORACLE_COULD_NOT_CONNECT_TO_ENDPOINT 7
#define ORACLE_INVALID_JSON_RESPONSE 8
#define ORACLE_REQUEST_TIMESTAMP_IN_THE_FUTURE 9
```
