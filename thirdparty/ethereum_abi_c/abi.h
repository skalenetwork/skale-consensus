/**
 * Ethereum ABI encoder/decoder
 * https://github.com/GridPlus/ethereum-abi-c
 * 
 * This library implements the Ethereum ABI spec 
 * (https://docs.soliditylang.org/en/develop/abi-spec.html)
 * as it pertains to contract method calls. We support encoding and decoding
 * of Ethereum data types.
 *
 * MIT License
 *
 * Copyright (c) 2020 Aurash Kamalipour <afkamalipour@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ETHEREUM_ABI_H_
#define __ETHEREUM_ABI_H_

#include <stdbool.h>
#include <stdlib.h>

#define ABI_WORD_SZ 32
#define ABI_ARRAY_DEPTH_MAX 2
#pragma pack(push,1)
// Enumeration of ABI types.
// * ABI_BYTES and ABI_STRING are both dynamic types and can be any size
// * Everything else is an elementary type and must be packed into a 32 byte word
typedef enum {
  ABI_NONE = 0,

  // Supported fixed types
  ABI_ADDRESS,
  ABI_BOOL,
  ABI_UINT8,
  ABI_UINT16,
  ABI_UINT24,
  ABI_UINT32,
  ABI_UINT40,
  ABI_UINT48,
  ABI_UINT56,
  ABI_UINT64,
  ABI_UINT72,
  ABI_UINT80,
  ABI_UINT88,
  ABI_UINT96,
  ABI_UINT104,
  ABI_UINT112,
  ABI_UINT120,
  ABI_UINT128,
  ABI_UINT136,
  ABI_UINT144,
  ABI_UINT152,
  ABI_UINT160,
  ABI_UINT168,
  ABI_UINT176,
  ABI_UINT184,
  ABI_UINT192,
  ABI_UINT200,
  ABI_UINT208,
  ABI_UINT216,
  ABI_UINT224,
  ABI_UINT232,
  ABI_UINT240,
  ABI_UINT248,
  ABI_UINT256,
  ABI_INT8,
  ABI_INT16,
  ABI_INT24,
  ABI_INT32,
  ABI_INT40,
  ABI_INT48,
  ABI_INT56,
  ABI_INT64,
  ABI_INT72,
  ABI_INT80,
  ABI_INT88,
  ABI_INT96,
  ABI_INT104,
  ABI_INT112,
  ABI_INT120,
  ABI_INT128,
  ABI_INT136,
  ABI_INT144,
  ABI_INT152,
  ABI_INT160,
  ABI_INT168,
  ABI_INT176,
  ABI_INT184,
  ABI_INT192,
  ABI_INT200,
  ABI_INT208,
  ABI_INT216,
  ABI_INT224,
  ABI_INT232,
  ABI_INT240,
  ABI_INT248,
  ABI_INT256,
  ABI_UINT, // alias for UINT256
  ABI_INT,  // alian for INT256
  ABI_BYTES1,
  ABI_BYTES2,
  ABI_BYTES3,
  ABI_BYTES4,
  ABI_BYTES5,
  ABI_BYTES6,
  ABI_BYTES7,
  ABI_BYTES8,
  ABI_BYTES9,
  ABI_BYTES10,
  ABI_BYTES11,
  ABI_BYTES12,
  ABI_BYTES13,
  ABI_BYTES14,
  ABI_BYTES15,
  ABI_BYTES16,
  ABI_BYTES17,
  ABI_BYTES18,
  ABI_BYTES19,
  ABI_BYTES20,
  ABI_BYTES21,
  ABI_BYTES22,
  ABI_BYTES23,
  ABI_BYTES24,
  ABI_BYTES25,
  ABI_BYTES26,
  ABI_BYTES27,
  ABI_BYTES28,
  ABI_BYTES29,
  ABI_BYTES30,
  ABI_BYTES31,
  ABI_BYTES32,

  // Supported dynamic types
  ABI_BYTES,
  ABI_STRING,

  // Tuple types - the number corresponds to the count of sub-params
  // (Implicitly this means we only support tuples with up to 20 params)
  ABI_TUPLE1,
  ABI_TUPLE2,
  ABI_TUPLE3,
  ABI_TUPLE4,
  ABI_TUPLE5,
  ABI_TUPLE6,
  ABI_TUPLE7,
  ABI_TUPLE8,
  ABI_TUPLE9,
  ABI_TUPLE10,
  ABI_TUPLE11,
  ABI_TUPLE12,
  ABI_TUPLE13,
  ABI_TUPLE14,
  ABI_TUPLE15,
  ABI_TUPLE16,
  ABI_TUPLE17,
  ABI_TUPLE18,
  ABI_TUPLE19,
  ABI_TUPLE20,

  ABI_MAX,
} ABIAtomic_t;

// Full description of an ABI type.
// * `isArray` can only be true for an elementary type and indicates the type is in an array,
//    which means we should have a set of `N` 32-byte words, where each word contains an elementary type
// * `arraySz` is only used if `isArray==true`. If `arraySz==0`, it is a dynamic sized array
typedef struct {
  ABIAtomic_t type;                     // The underlying, atomic type
  bool isArray;                         // Whether this is an array of the atomic type
  size_t arraySz;                       // Non-zero implies fixed size array and describes the size. 
} ABI_t;

typedef struct {
  size_t typeIdx;                     // The index of the type param in the function definition
  size_t arrIdx;                      // The index of the item in an array, if applicable, to fetch
} ABISelector_t;
#pragma pack(pop)

// Helper to determine if this is a tuple type
bool is_tuple_type(ABI_t t);

// Helper to get the number of parameters in a tuple type
// @param `t`        - Parameter to inspect
// @return           - Number of tuple params; -1 if this is not a tuple
int get_tuple_sz(ABI_t t);

// Ensure we have a valid ABI schema being passed. We check the following:
// * Is each atomic type a valid ABI type? (e.g. uint32, string)
// * Is each type an single element or array (fixed or dynamic)?
// Note that for arrays, we only support all fixed or all dynamic dimensions,
// meaning things like `string[3][3]` and `string[]` are allowed, but
// `string[3][]` are not. This is because the ABI spec is pretty loose about
// defining these encodings, so we will just be strict and reject combinations.
// @param `types`     - array of types making up the schema
// @param `numTypes`  - number of types in the schema
// @return            - true if we can handle every type in this schema
bool abi_is_valid_schema(const ABI_t * types, size_t numTypes);

// Fetch the array size of an array item. The array must be variable-size, since 
// fixed-size arrays may only have one dimension and the size is defined in the type.
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `info`      - information about the data to be selected
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - Size of array dimension; -1 on error.
int abi_get_array_sz( const ABI_t * types, 
                      size_t numTypes, 
                      ABISelector_t info, 
                      const void * in,
                      size_t inSz);

// Get the array size of a type inside of a tuple. Must be a variable size array.
// @param `types`     - array of ABI type definitions
// @param `numTypes`  - the number of types in this ABI definition
// @param `tupleInfo` - information about the tuple item
// @param `paramInfo` - information about the parameter we want inside the tuple
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - Size of array dimension; -1 on error.
int abi_get_tuple_param_array_sz( const ABI_t * types, 
                                  size_t numTypes, 
                                  ABISelector_t tupleInfo,
                                  ABISelector_t paramInfo, 
                                  const void * in,
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
// @return            - number of bytes written to `out`; -1 on error.
int abi_decode_param( void * out, 
                      size_t outSz, 
                      const ABI_t * types, 
                      size_t numTypes, 
                      ABISelector_t info, 
                      const void * in,
                      size_t inSz);

// Perform `abi_decode_param` on a parameter nested in a tuple struct.
// Tuple data is encoded as if it is its own definition and is offset like dynamic data.
// @param `out`       - output buffer to be written
// @param `outSz`     - size of output buffer to be written
// @param `types`     - all types in the larger ABI definition
// @param `numTypes`  - number of types in the larger ABI definition
// @param `tupleInfo` - information about the tuple param (i.e. one of the root params)
// @param `paramInfo` - information about the param inside the tuple
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - number of bytes written to `out`; -1 on error.
int abi_decode_tuple_param( void * out, 
                            size_t outSz, 
                            const ABI_t * types, 
                            size_t numTypes,
                            ABISelector_t tupleInfo,
                            ABISelector_t paramInfo, 
                            const void * in,
                            size_t inSz);

// Encode a payload given a set of types. 
// All parameter data should be tightly packed in `in`. Numbers are expected to be little endian buffers.
// NOTE: This has significant limitations at the moment. Tuples and arrays are NOT supported.
// @param `out`       - output buffer to be written
// @param `outSz`     - size of output buffer to be written
// @param `types`     - all types in the larger ABI definition
// @param `numTypes`  - number of types in the larger ABI definition
// @param `offsets`   - list of size `numTypes` containin offsets for the types' data in `in`
// @param `in`        - Buffer containin the input data
// @param `inSz`      - Size of `in`
// @return            - number of bytes written to `out`; -1 on error.
int abi_encode( void * out, 
                size_t outSz, 
                const ABI_t * types, 
                size_t numTypes, 
                size_t * offsets, 
                const void * in, 
                size_t inSz);

#endif