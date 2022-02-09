# Threshold Encryption Spec

This specification describes threshold encryption (TE) of arguments to ETH
smart contract calls. Arguments are encrypted on the client side, and decrypted 
after the transaction is committed to the blockchain,  but before the transaction is executed.


## Encrypting Solidity Arguments.

In order to support TE, a Solidity function needs to include an argument 
that takes a byte array.

Here is an example of solidity code that accepts an encrypted byte array, decrypts
it and prints the decrypted value to the blockchain.

```javascript

event PrintString(byte[] data)

function printSecret(byte[] decryptedSecret) {
    emit PrintEvent(decrypted);
}
```

Note 1: the decryption happens before the function is executed, so inside the function the 
secret is already decrypted.

Note 2: for efficiency purposes only one encrypted argument per function is supported.

If a user needs to pass more than one encrypted value, the user is encouraged to 
pack the values into a JSON, and then pass this JSON as an encrypted argument.
The function will then parse the JSON inside the function code. 

Note 3: if more than one encrypted arguments are passed to a function, only the 
first one is decrypted and the rest are left as is

## Client argument encryption  procedure.

The client (browser or mobile device), encrypts the argument ```plaintext``` before submitting the 
transaction as follows:

1. The client generates a random 128-bit AES key ```aesKey``` as a byte array.
2. The client generates a random 256-bit AES initialization vector ```aesIv```.
3. The client encrypts plaintext using AES in CBC mode to a receive byte array ```ciphertext```.
4. The client retrieves the current TE public key ```tePubKey``` from blockchain and encrypts  
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


## Skaled argument decryption procedure.

Once a transaction has been committed to the blockchain, skaled will use the 
following procedure to decrypt the Solidity argument.

1. The bytes of the transaction is scanned for segments that start with
   T











