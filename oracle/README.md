   # Dynamic Oracle API
   
The following two JSON-RPC calls impelemented by ```skaled``` agent provide access to Oracle.

## oracle_submitRequest

This API call is used to submit Oracle request

### Parameters

1. ```SPEC```, string - request specification

### Parameters example 1

HTTP get request that obtains current unix time and 
day of year from worldtimeapi.org.

```json
    {
    "cid": 1, "uri": "http://worldtimeapi.org/api/timezone/Europe/Kiev",
    "jsps":["/unixtime", "/day_of_year", "/xxx"],
    "trims":[1,1,1],
    "time":9234567,
    "pow":53458}



```

Human description: contact worldtimeapi.org endpoint. From the
JSON result, pick ```unixtime```, ```day_of_year``` and ```xxx```
elements. Convert each element to string. Trim one character
from the end from each string.

### Parameters example 2

HTTP post request that posts some data to endpoint

```json
    {
    "cid": 1, "uri": "https://reqres.in/api/users", 
    "jsps":["/id"],   
    "time":9234567, 
     "post":"some_data",
     "pow":1735}
```

### Parameters JSON elements description

Required elements:

1. ```cid```, uint64 - chain ID
2. ```uri```, string - Oracle endpoint (http or https)
3. ```time```, uint64 - Linux time of request in ms
4. ```jsps```, array of strings - list of JSON pointer to the data elements to be picked from server response.
5. ```pow```, string - uint64 proof of work that is used to protect against denial of service attacks 

Please see https://json.nlohmann.me/features/json_pointer/ for intro to
JSON pointers.

Note: for each JSON pointer specified in the request, the Oracle 
will pick the corresponding element from the endpoint response 
and transform it to string. If no such element exists, ```null``` will
be returned.

Optional elements:

1. ```trims```, uint64 - this is an array of trim values. 
It is used to trim endings of the strings in Oracle result.
If ```trims``` array is provided, it has to provide trim value for
each JSON pointer requested.


1. ```post```, string - if this element is provided, the 
Oracle with use HTTP POST instead of HTTP GET (default).
The value of the ```post``` will be POSTed to the endpoint.

## Returns

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

### Geth 

To get information from SKALE network geth servers, one need to 
use ```geth://``` in URI".

The following JSON-RPC endpoint are available in the first release:

```
geth://eth_call
geth://eth_gasPrice
geth://eth_blockNumber
geth://eth_getBlockByNumber
geth://eth_getBlockByHash
```


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
2. ```sigs``` - array of ECDSA signatures where ```t+1``` signatures are not null. In case of 16 nodes, 6 sigs are not null.
In case of 4 nodes, 2 sigs are not null.

Note: each signature is in the format of concatenation of hex represenation of  v, r, and s.

Note: there is no "0x" at the beginning.

```
v:r:s
```



### Oracle Result Example 

An example of Oracle result is provided below

```
{"cid":1,
 "uri":"http://worldtimeapi.org/api/timezone/Europe/Kiev",
  "jsps":["/unixtime", "/day_of_year", "/xxx"],
  "trims":[1,1,1],"time":1642521456593,
  "rslts":["164252145","1",null],
```

### Oracle Signature Verification in Solidity.

To verify Oracle signature:

1. Take ```ORACLE_RESULT``` string.

2. Trim away the "sigs" part, so the remaining last character is comma. Call the resulting string ```TRIMMED_ORACLE_RESULT```


Example of ```TRIMMED_ORACLE_RESULT```


```
{"cid":1,
 "uri":"http://worldtimeapi.org/api/timezone/Europe/Kiev",
  "jsps":["/unixtime", "/day_of_year", "/xxx"],
  "trims":[1,1,1],"time":1642521456593,
  "rslts":["164252145","1",null],
   "sigs":["1:6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:9d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "0:7d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:10d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "1:8d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:1150daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "0:9d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:1250daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "1:050daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:1350daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
           "1:6d50daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f:1450daf908d97d947fdcd387ed4bdc76149b11766f455b31c86d5734f4422c8f",
```


3. Calculate 256 bit SHA-3 hash of ```TRIMMED_ORACLE_RESULT```

```
HASH = SHA-3(TRIMMED_ORACLE_RESULT_AS_BYTES)
```

4. For each non-null element ```i``` in "sigs", do 

```
VERIFY_ECDSA_SIGNATURE(PUBLIC_KEY_FOR_SCHAIN_INDEX(i)), HASH);
```

Note: signature is created in C++ code by using SGX ```ecdsaSignMessageHash```.

```
ecdsaSignMessageHash(const std::string& keyName,
    const std::string& messageHash,) {
    Json::Value p;
    p["type"] = SgxZmqMessage::ECDSA_SIGN_REQ;
    p["base"] = 16;
    p["keyName"] = keyName;
    p["messageHash"] = messageHash;
    auto result = dynamic_pointer_cast< ECDSASignRspMessage >(
    doRequestReply( p, _throwExceptionOnTimeout ) );
    return result->getSignature();
}
```



# List of Oracle error codes.

`````
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
