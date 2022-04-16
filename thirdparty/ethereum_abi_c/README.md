# ethereum-abi-c

A util for interacting with [Ethereum ABI](https://docs.soliditylang.org/en/develop/abi-spec.html) data.

This repo contains only a limited number of tools specific to what is needed by GridPlus, but it may be useful for someone else
as we could not find any C tooling for handling ABI data in Ethereum.

## Using an ABI Schema

The main use case of this library is to extract a piece of data from an ABI-encoded payload using the corresponding ABI schema, which is
encoded as an aray of `ABI_t` params:

```
typedef struct {
  ABIAtomic_t type;                     // The underlying, atomic type
  bool isArray;                         // Whether this is an array of the atomic type
  size_t arraySz;                       // Non-zero implies fixed size array and describes the size. 
                                        // (Only 1D fixed size arrays allowed)
} ABI_t;
```

Each `ABI_t` corresponds to a param in a function. It can be used to describe any type that we may encounter. Take the following example:

```
myFunction(uint32, uint[3], string[], uint16)
```

We would describe this schema with the following `ABI_t` set:

```
{
  {
    .type = ABI_UINT32
  },
  {
    .type = ABI_UINT,
    .isArray = true,
    .arraySz = 3,
  },
  {
    .type = ABI_STRING,
    .isArray = true,
  },
  {
    type: ABI_UINT16,
  }
}
```

Some notes about this example schema:

* The first and last params are considered "elementary" types (which always fit in a 32 byte word) and they are *not* arrays.
* The second param is also an "elementary" type, but it is an array. `arraySz` being a positive number indicates this ia "fixed-size" array. This means we expect 96 bytes of data for this param (3x 32-byte words).
* The third param is a "dynamic" type (string). This means we do not know how big the param we want will be, but the length will prefix it. This is also an array and, because `arraySz=0`, it is considered a "variable-size" array. So this contains an unknown number of elements, each of which is of unknown size.

> NOTE: We currently only support 1D arrays. Larger dimension *variable size* arrays (not fixed size) are described in the ABI spec, but they introduce too many edge cases and too much complexity and they are hardly ever used, certainly not by any well-utilized contracts of which I am aware.


## Fetching a Parameter

Once we know our ABI schema, we can fetch any parameter therein using an instance of `ABISelector_t`. We configure this selector instance to search for a *single* parameter in the
ABI encoded data. Importantly, this means we cannot request an entire array -- we can only request **one** element at a time!

Let's go through some examples of selecting data from the above schema.

1. Select an elementary type from a non-array

```
ABISelector_t ctx = { .typeIdx = 0, }; // selects parameter 0 (uint32)
ABISelector_t ctx = { .typeIdx = 3, }; // selects parameter 3 (uint16)
```

These are pretty straight forward. We just need to put in the `typeIdx` of the param we want to select. These params are one word each (32 bytes) so there is no extra configuration needed.

2. Select an elementary type from a fixed size array

```
ABISelector_t ctx = { 
  .typeIdx = 1,         // selects second type (index=1) in definition (fixed-size uint array)
  .arrIdx = 1,          // selects item with index 1 in the 0-indexed array, i.e. the second element
};
```

Here we use `arrIdx` to specify the index of the element in the array we want to fetch. This will fetch the `uint` (1 word, i.e. 32 bytes) at index `1` in the 3-element array.
Note that any `arrIdx` value larger than `2` will fail to fetch any data, since the ABI schema defines this param as being a 3-element array.

3. Select a dynamic type from a variable size array

```
ABISelector_t ctx = { 
  .typeIdx = 2,         // selects third type (index=2) in definition (variable-size string array)
  .subIdx = 1,
};
```

This is functionally the same as the previous selection, except that the result will end up being of unknown size. Similar to fixed size arrays, the selection will fail if we overrun the array size, which is encoded in the data, i.e. we cannot select element 5 if the array size is only 4.

## Getting Param and Array Sizes

We also include a few convenience methods to get more information about data sizes. See the API section for more information.

1. `abi_get_param_sz` - Get the size of a dynamic-type param before fetching the data. This is useful if you don't know how big your output buffer should be.
2. `abi_get_array_sz` - Get the size of a variable-size array. This is useful if you don't know how big a loop should be when fetching data.

## API

The following functionality is exposed via the `abi.h` API:

```
// Ensure we have a valid ABI schema being passed. We check the following:
// * Is each atomic type a valid ABI type? (e.g. uint32, string)
// * Is each type an single element or array (fixed or dynamic)?
// Note that for arrays, we only support all fixed or all dynamic dimensions,
// meaning things like `string[3][3]` and `string[]` are allowed, but
// `string[3][]` are not. This is because the ABI spec is pretty loose about
// defining these encodings, so we will just be strict and reject combinations.
// @param `tyes`      - array of types making up the schema
// @param `numTypes`  - number of types in the schema
// @return            - true if we can handle every type in this schema
bool abi_is_valid_schema(ABI_t * types, size_t numTypes);

// Fetch the array size of a specific dimension of an array. The array must
// be variable-size, since fixed-size arrays may only have one dimension and
// the size is defined in the type.
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `info`      - information about the data to be selected
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - Size of array dimension; 0 on error.
size_t abi_get_array_sz(ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        void * in,
                        size_t inSz);

// Get the element size of a particular parameter whose type is dynamic.
// This is equivalent to `abi_decode_param`, but does not copy data to an output
// buffer and only works with dynamic types.
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `info`      - information about the data to be selected
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - number of bytes written to `out`; 0 on error.
size_t abi_get_param_sz(ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        void * in, 
                        size_t inSz);

// Decode and return a param's data in `out` given a set of ABI types and an `in` buffer.
// Note that padding is stripped from elementary types, which are encoded in 32-byte words regardless
// of the underlying data size. For example, a single ABI_BOOL would be the last byte of a
// 32 byte word. Dynamic types are returned in full, as there is no padding.
// @param `out`       - output buffer to be written
// @param `outSz`     - size of output buffer to be written
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `info`      - information about the data to be selected
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - number of bytes written to `out`; 0 on error.
size_t abi_decode_param(void * out, 
                        size_t outSz, 
                        ABI_t * types, 
                        size_t numTypes, 
                        ABISelector_t info, 
                        void * in,
                        size_t inSz);
```

## Testing

We provide a number of tests based on the [examples in the ABI spec](https://docs.soliditylang.org/en/develop/abi-spec.html#examples).
You can run the tests with:

```
make test && ./test
```