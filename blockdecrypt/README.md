# Threshold Encryption  of Solidity Arguments Spec

This specification describes threshold encryption of Solidity arguments (TESA). 

Arguments are encrypted on the client side. They are decrypted  
by a committee of nodes after the transaction is committed to the blockchain,  
but before EVM execution.

As a result, front running and manipulation of transactions is provably eliminated,
since the block proposer does not have information about the sensitive data.

## Compatibility with existing ETH software

The goal of this spec if to enable argument encryption without making modification
to ETH transaction format, Solidity programming language as well as existing ETH software such as 
client libraries, wallets and development tools.

All methods needed to argument encryption can be provided by a small standalone 
client library.


## Encrypting Solidity Arguments

In order to support argument encryption, a Solidity function needs to include a byte array argument.

Here is an example of Solidity code that accepts an encrypted secret as a byte array, decrypts
it and prints the decrypted value to the blockchain.

```javascript

event PrintEvent(byte[] data)

function printSecret(byte[] decryptedSecret) {
    emit PrintEvent(decryptedSecret);
}
```

Note 1: the decryption happens before the function is executed. Inside the function the 
secret is already decrypted.

Note 2: for efficiency purposes only one encrypted argument per function is supported.

If a user needs to pass more than one encrypted value, the user is encouraged to 
pack the values into a JSON string, and then pass this JSON as an encrypted argument.
The Solidity function can then parse the JSON to retrieve the values. 

Note 3: if more than one encrypted arguments are passed to a function, only the 
first one is decrypted and the rest are left as is.

Note 4: in addition to encrypting the argument, in many cases it is also
necessary to hide which smart contract and which function is called.
This can be easily done by creating a single router contract that routes calls
to different contract or function depending on the encrypted argument.  
The very fact of the transaction can also be disguised by periodically
calling an empty function.


## Client argument encryption  procedure

The client (browser or mobile device), encrypts the argument ```plaintext``` before submitting the 
transaction as follows:

1. The client generates a random 128-bit AES key ```aesKey``` as a byte array.
2. The client generates a random 256-bit AES initialization vector ```aesIv```.
3. The client encrypts ```plaintext``` using AES in CBC mode to a receive byte array ```ciphertext```.
4. The client retrieves the current TE public key ```tePubKey``` from the blockchain and encrypts  
```aesKey``` using this key, to receive a hex string ```encryptedAesKeyAsHexString```.
5. The client obtain the uint64 current Linux time in ms ```currentTimeMs```.
6. The client creates a JSON header ```jsonHeader```as follows:

```json
{ "ts": $currentTimeMsAsUint64,
  "ek": $encryptedAesKeyAsHexString,
  "iv": $aesIvAsHexString
}
```

7. The client can now form the encrypted argument by concatenating 
bytes of the header with the cipherText.

```
byte[] encryptedArgument  = 
     TE_MAGIC_START + to_byte_array(jsonHeader) + cipherText + TE_MAGIC_END
```

Note: ```TE_MAGIC_START``` and ```TE_MAGIC_END``` are long pseudorandom sequences
used to match the encrypted values during decryption. Their values are
defined in Appendix 1.



8. Once the encrypted argument is formed, the client uses
   regular means (e.g. web3.js) to submit the transaction.


## Node argument decryption procedure.

Once a transaction has been committed to the blockchain, a node will use the 
following procedure to decrypt the Solidity argument.

1. The bytes of the transaction is scanned for segments that start with
   ```TE_MAGIC_START``` and end with ```TE_MAGIC_END``` to recover 
```encryptedArgument```. 

Note: if mode than one segment is found, the first is processed and the rest are ignored.

2. The ```encryptedArgument``` is then split into ```jsonHeader``` and  













