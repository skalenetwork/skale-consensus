#include "abi.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//===============================================
// HELPERS
//===============================================
// Get the u32 that is represented in big endian in a 32 byte word (i.e. last 4 bytes).
// Returns a little endian numerical representation of bytes loc+29:loc+32.
static uint32_t get_abi_u32_be(const void * in, size_t loc) {
  const uint8_t * inPtr = in;
  size_t l = loc + 32;
  return inPtr[l-1] | inPtr[l-2] << 8 | inPtr[l-3] << 16 | inPtr[l-4] << 24;
}

static void write_u32_be(void * out, uint32_t n) {
  ((uint8_t*)out)[0] = (uint8_t)((n >> 24) & 0xff);
  ((uint8_t*)out)[1] = (uint8_t)((n >> 16) & 0xff);
  ((uint8_t*)out)[2] = (uint8_t)((n >> 8) & 0xff);
  ((uint8_t*)out)[3] = (uint8_t)((n >> 0) & 0xff);
}

static bool is_fixed_bytes_type(ABI_t t) {
  return t.type >= ABI_BYTES1 && t.type <= ABI_BYTES32;
}

// Dynamic types include `bytes` and `string` - they are always variable length.
static bool is_dynamic_atomic_type(ABI_t t) {
  return (t.type == ABI_BYTES || t.type == ABI_STRING);
}

// Elementary types are atomic types for which there is only one instance (as
// opposed to array types, which contain a fixed or variable number of instances).
static bool is_elementary_atomic_type(ABI_t t) {
  return !is_dynamic_atomic_type(t) && !is_tuple_type(t);
}

static bool is_single_elementary_type(ABI_t t) {
  return is_elementary_atomic_type(t) && !t.isArray;
}

static bool is_single_dynamic_type(ABI_t t) {
  return is_dynamic_atomic_type(t) && !t.isArray;
}

// Array types are simply arrays of elementary types. These can be either 
// fixed size (e.g. uint256[3]) or variable size (e.g. uint256[]). Variable sized
// arrays have `t.isArray = true, t.arraySz = 0` while fixed size have the size
// defined in `t.arraySz`.
static bool is_elementary_type_array(ABI_t t) {
  return is_elementary_atomic_type(t) && t.isArray;
}

static bool is_dynamic_type_array(ABI_t t) {
  return is_dynamic_atomic_type(t) && t.isArray;
}

static inline bool is_fixed_sz_array(ABI_t type) {
  // The ABI spec doesn't really handle multi-dimensional fixed sized arrays,
  // so we will reject any fixed size arrays beyond 1D.
  // The reference implementation (ethereumjs-abi) treats x[3][3] exactly the
  // same as x[3][] and x[3][1], which we do not want to allow.
  // Array must be 1D and have a non-zero arraySz
  return type.isArray && type.arraySz > 0;
}

static inline bool is_variable_sz_array(ABI_t type) {
  // arraySz must be 0 to denote variable sized arrays.
  // Although the ABI spec does describe 2D arrays, we do not currently support
  // them as they introduce quite a bit of complexity and aren't used very often.
  return type.isArray && type.arraySz == 0;
}

static bool is_elementary_type_fixed_sz_array(ABI_t type) {
  return ((is_elementary_type_array(type)) && 
          (is_fixed_sz_array(type)));
}

static bool is_elementary_type_variable_sz_array(ABI_t type) {
  return ((is_elementary_type_array(type)) &&
          (is_variable_sz_array(type)));
}

static bool is_dynamic_type_fixed_sz_array(ABI_t type) {
  return ((is_dynamic_type_array(type)) &&
          (is_fixed_sz_array(type)));
}

static bool is_dynamic_type_variable_sz_array(ABI_t type) {
  return ((is_dynamic_type_array(type)) &&
          (is_variable_sz_array(type)));
}

// Get the index of the first param nested inside of the specified tuple.
// Nested tuple params are appended to the end of the `types` array and are appened
// in the order of the tuples containing them.
static int get_first_tuple_param_idx(const ABI_t * types, size_t numTypes, size_t tupleIdx) {
  if (!types || !is_tuple_type(types[tupleIdx]) || tupleIdx > numTypes)
    return -1;
  size_t toSkip = 0;
  for (size_t i = (tupleIdx + 1); i < numTypes; i++)
    if (is_tuple_type(types[i]))
      toSkip += get_tuple_sz(types[i]);
  return numTypes - get_tuple_sz(types[tupleIdx]) - toSkip;
}

// It is important to know if a tuple has a dynamic param. If it does, that tuple
// will be represented by an offset in the header param words EVEN IF it is a fixed
// size tuple array.
static bool tuple_has_dynamic_type(const ABI_t * types, size_t numTypes, size_t idx) {
  if (!types || !is_tuple_type(types[idx]))
    return false;
  int firstParamIdx = get_first_tuple_param_idx(types, numTypes, idx);
  if (firstParamIdx < 0)
    return false;
  int numParams = get_tuple_sz(types[idx]);
  if (numParams < 0)
    return false;
  for (int i = firstParamIdx; i < firstParamIdx + numParams; i++) {
    if (is_dynamic_atomic_type(types[i]))
      return true;
  }
  return false;
}

static bool tuple_has_variable_sz_elem_arr(const ABI_t * types, size_t numTypes, size_t idx) {
  if (!types || !is_tuple_type(types[idx]))
    return false;
  int firstParamIdx = get_first_tuple_param_idx(types, numTypes, idx);
  if (firstParamIdx < 0)
    return false;
  int numParams = get_tuple_sz(types[idx]);
  if (numParams < 0)
    return false;
  for (int i = firstParamIdx; i < firstParamIdx + numParams; i++) {
    if (is_elementary_type_variable_sz_array(types[i]))
      return true;
  }
  return false;
}

static bool tuple_has_fixed_sz_elem_arr(const ABI_t * types, size_t numTypes, size_t idx) {
  if (!types || !is_tuple_type(types[idx]))
    return false;
  int firstParamIdx = get_first_tuple_param_idx(types, numTypes, idx);
  if (firstParamIdx < 0)
    return false;
  int numParams = get_tuple_sz(types[idx]);
  if (numParams < 0)
    return false;
  for (int i = firstParamIdx; i < firstParamIdx + numParams; i++) {
    if (is_elementary_type_fixed_sz_array(types[i]))
      return true;
  }
  return false;
}

// Get the number of bytes describing an elementary data type
static size_t elem_sz(ABI_t t) {
  if (is_dynamic_atomic_type(t))
    return 0;
  if (is_fixed_bytes_type(t))
    return 1 + (t.type - ABI_BYTES1);
  // Numerical types (excluding 256-bit numbers)
  if (t.type >= ABI_UINT8 && t.type < ABI_UINT256)
    return 32 - (ABI_UINT256 - t.type);
  // Signed integers are encoded as (UINT256_MAX - val), so we need to always
  // return 32 bytes of data regardless of the underlying int type size
  if (t.type >= ABI_INT8 && t.type < ABI_INT256)
    return 32;
  // Other types
  switch (t.type) {
    case ABI_ADDRESS:
      return 20;
    case ABI_BOOL:
      return 1;
    case ABI_UINT256:
    case ABI_INT256:
    case ABI_UINT:
    case ABI_INT:
      return 32;
    default:
      return 0;
  }
}

// Decode a parameter of elementary type. Each elementary type is encoded in a single 32 byte word,
// but may contain less data than 32 bytes (depending on the type -- see `elemSz()`).
static int decode_elem_param( void * out, 
                              size_t outSz, 
                              ABI_t type, 
                              const void * in, 
                              size_t inSz, 
                              size_t off) 
{
  // Ensure there is space for this data in `out` and that this is an elementary type
  if (ABI_WORD_SZ > outSz || is_dynamic_atomic_type(type))
    return -1;
  size_t nBytes = elem_sz(type);
  const uint8_t * inPtr = in;
  if (outSz < nBytes)
    return -1;
  // Most types have data written at the end of the word. Start with this assumption. 
  size_t start = off + (ABI_WORD_SZ - nBytes);
  // Non-numerical (and non-bool) types have data written to the beginning of the word
  if (is_fixed_bytes_type(type))
    start = off;
  if (start + nBytes > inSz)
    return -1;
  memcpy(out, inPtr + start, nBytes);
  return nBytes;
}

// Decode a parameter of dynamic type. Each dynamic type is prefixed with a word that
// specifies the size of the item, followed by `N` words worth of data. If the param
// is not a multiple of 32 bytes, it is right-padded with zeros.
// We only copy the data itself, i.e. we discard the right-padded zeros.
static int decode_dynamic_param(  void * out, 
                                  size_t outSz, 
                                  ABI_t type, 
                                  const void * in, 
                                  size_t inSz, 
                                  size_t off) {
  if (!is_dynamic_atomic_type(type))
    return -1;
  const uint8_t * inPtr = in;
  if (off + ABI_WORD_SZ > inSz)
    return -1;
  size_t elemSz = get_abi_u32_be(in, off);
  off += ABI_WORD_SZ;
  if (outSz < elemSz)
    return -1;
  if (off + elemSz > inSz)
    return -1;
  memcpy(out, inPtr + off, elemSz);
  return elemSz;
}

// Decode a param given its offset. The rules for decoding depend on the type of param.
// The offset provided (`off`) is the starting place of the param itself. 
static int decode_param(  void * out, 
                          size_t outSz, 
                          ABI_t type, 
                          const void * in, 
                          size_t inSz, 
                          size_t off, 
                          ABISelector_t info) 
{
  if (!out || !in)
    return -1;
  // Elementary types are fairly straight forward
  if (is_elementary_type_variable_sz_array(type)) {
    // Variable sized arrays require a jump to the item
    size_t numElem = get_abi_u32_be(in, off);
    if (info.arrIdx >= numElem)
      return -1;
    // Skip the numElem word and jump to the array index
    off += ABI_WORD_SZ * (1 + info.arrIdx);
    return decode_elem_param(out, outSz, type, in, inSz, off);
  } else if (is_elementary_atomic_type(type)) {
    // Other elementary types can be decoded without modification
    return decode_elem_param(out, outSz, type, in, inSz, off);
  }
  // Dynamic types have prefixes that we need to account for
  if (is_dynamic_type_array(type)) {
    if (is_dynamic_type_fixed_sz_array(type)) {
      off += get_abi_u32_be(in, off + (ABI_WORD_SZ * info.arrIdx));
    } else {
      // Sanity check to avoid overrun
      if (off + ABI_WORD_SZ > inSz)
        return -1;
      // Another sanity check to avoid overrun
      size_t numElem = get_abi_u32_be(in, off);
      if (info.arrIdx >= numElem)
        return -1;
      // Skip past this word
      off += ABI_WORD_SZ;
      // Get the offset for this item and jump to it
      off += get_abi_u32_be(in, off + (ABI_WORD_SZ * info.arrIdx));
    }
  }
  // We should now be at the offset corresponding to the size of the dynamic
  // type element that we want.
  return decode_dynamic_param(out, outSz, type, in, inSz, off);
}

// Get an offset of the parameter in question. The rules depend on the type of param.
static size_t get_param_offset( const ABI_t * types, 
                                size_t numTypes, 
                                ABISelector_t info, 
                                const void * in, 
                                size_t inSz) 
{
  if (!types || !in)
    return -1;
  ABI_t type = types[info.typeIdx];
  size_t off = 0;
  size_t paramOff = 0;

  // Fixed size elementary type arrays pack everything into the header data, i.e.
  // do not come with an offset to the param. If such a param comes before our target
  // param we need to sufficiently skip over it.
  for (size_t i = 0; i < info.typeIdx; i++) {
    // Note the -1, which accounts for the param in this slot. If this was a normal
    // param it would take up 1 word.
    ABI_t _type = types[i];

    bool is_tuple_without_dynamic_type_and_not_var_sz_arr = (
      (is_tuple_type(_type)) &&
      (!tuple_has_dynamic_type(types, numTypes, i)) &&
      (!is_variable_sz_array(_type))
    );

    bool is_tuple_without_var_array_or_dynamic_params = (
      (is_tuple_without_dynamic_type_and_not_var_sz_arr) &&
      (!tuple_has_variable_sz_elem_arr(types, numTypes, i))
    );

    bool is_tuple_with_fixed_elem_array_and_not_var_arr = (
      (is_tuple_without_var_array_or_dynamic_params) &&
      (tuple_has_fixed_sz_elem_arr(types, numTypes, i))
    );

    bool is_non_tuple_fixed_elem_array = (
      (is_elementary_type_fixed_sz_array(_type)) && 
      (!is_tuple_type(_type))
    );
    if (is_non_tuple_fixed_elem_array) {
      // Elementary type fixed sz arrays have all params in the header data
      off += ABI_WORD_SZ * _type.arraySz;
    } else if (is_tuple_with_fixed_elem_array_and_not_var_arr) {
      // Tuples with fixed arrays and no dynamic params or variable arrays are
      // not represented by offsets -- they have all their params packed into the
      // header data.
      int startIdx = get_first_tuple_param_idx(types, numTypes, i);
      if (startIdx < 0)
        return -1;
      size_t numWords = 0;
      for (size_t j = startIdx; j < startIdx + get_tuple_sz(_type); j++) {
        if (is_elementary_type_array(types[j]) && is_fixed_sz_array(types[j])) {
          numWords += types[j].arraySz;
        } else {
          numWords++;
        }
      }
      size_t arrMult = 1;
      if (_type.isArray && _type.arraySz > 0)
        arrMult = _type.arraySz;
      off += arrMult * numWords * ABI_WORD_SZ;
    } else if (is_tuple_without_var_array_or_dynamic_params) {
      // Tuples without dynamic types have all params up front.
      size_t tsz = get_tuple_sz(_type);
      off += ABI_WORD_SZ * (_type.isArray ? _type.arraySz * tsz : tsz);
    } else {
      // Other types are just words in the header data
      off += ABI_WORD_SZ;
    }
  }
  if ((tuple_has_variable_sz_elem_arr(types, numTypes, info.typeIdx)) && 
      (!tuple_has_dynamic_type(types, numTypes, info.typeIdx)) ) {
    paramOff = get_abi_u32_be(in, off);
  } else if (is_dynamic_atomic_type(type) || is_variable_sz_array(type)) {
    // Dynamic types and variable sized arrays of any type are located at 
    // their respective offsets
    paramOff = get_abi_u32_be(in, off);
  } else {
    // All other parameters are located in the header
    paramOff = off;
  }
  // Fixed size elementary type arrays are a special case. The offset we have corresponds
  // to the first param in the array so we just need to skip words to locate the individual
  // item we want.
  if (is_elementary_type_fixed_sz_array(type)) {
    // Sanity check to avoid overrun
    if (type.arraySz <= info.arrIdx)
      return inSz + 1;
    return paramOff + (ABI_WORD_SZ * info.arrIdx);
  }
  // Sanity check to avoid overrun
  if (is_dynamic_type_fixed_sz_array(type) && type.arraySz <= info.arrIdx)
      return inSz + 1;
  // We now have the offset of the data we want
  return paramOff;
}

// Get the starting index of data for the specified tuple. If the tuple is an array
// this will be the starting point of the tuple item we want, but NOT the starting
// point of the tuple's parameter
size_t get_tuple_data_start(const ABI_t * types, size_t numTypes, ABISelector_t tupleInfo, const void * in, size_t inSz)
{
  if (!types || !in)
    return -1;
  size_t dataOff = get_param_offset(types, numTypes, tupleInfo, in, inSz);
  if (dataOff > inSz)
    return 0;
  ABI_t tupleType = types[tupleInfo.typeIdx];
  if (is_variable_sz_array(tupleType)) {
    // The first word here is the size of the tuple array. 
    // Make sure we don't overrun it.
    if (tupleInfo.arrIdx >= get_abi_u32_be(in, dataOff))
      return 0;
    // If this is a variable sz array we need to get the offset of tuple item we want
    // The paramOff points to the tuple metadata so let's roll dataOff forward.
    // Skip the first word here, which is the size of the tuple array
    dataOff += ABI_WORD_SZ;
    // Now find the second offset that jumps us to the start of the tuple item we want.
    if ((tuple_has_dynamic_type(types, numTypes, tupleInfo.typeIdx)) || 
        (tuple_has_variable_sz_elem_arr(types, numTypes, tupleInfo.typeIdx))) {
      dataOff += get_abi_u32_be(in, dataOff + (tupleInfo.arrIdx * ABI_WORD_SZ));
    } else {
      int startIdx = get_first_tuple_param_idx(types, numTypes, tupleInfo.typeIdx);
      if (startIdx < 0)
        return -1;
      size_t tupleSz = get_tuple_sz(tupleType);
      size_t tupleItemDataSz = 0;
      for (size_t i = startIdx; i < (startIdx + tupleSz); i++) {
        if (types[i].isArray && types[i].arraySz > 0)
          tupleItemDataSz += ABI_WORD_SZ * types[i].arraySz;
        else if (!types[i].isArray)
          tupleItemDataSz += ABI_WORD_SZ;
      }
      dataOff += tupleInfo.arrIdx * tupleItemDataSz;
    }
  } else if (tuple_has_dynamic_type(types, numTypes, tupleInfo.typeIdx)) {
    // Any tuple that has a dynamic type is represented by an offset to its data
    dataOff = get_abi_u32_be(in, dataOff);
    if (is_fixed_sz_array(tupleType)) {
      // If this is a fixed sz array we need to jump to the array index
      dataOff += get_abi_u32_be(in, dataOff + (tupleInfo.arrIdx * ABI_WORD_SZ));
    }
  } else if (is_fixed_sz_array(tupleType)) {
    // Fixed size tuple arrays with all elementary types are treated like normal
    // fixed size arrays of individual elementary types, i.e. the data is all serialized
    // in the header params.
    if (tuple_has_variable_sz_elem_arr(types, numTypes, tupleInfo.typeIdx)) {
      dataOff += get_abi_u32_be(in, dataOff + (tupleInfo.arrIdx * ABI_WORD_SZ)); 
    } else {
      dataOff += (tupleInfo.arrIdx * get_tuple_sz(tupleType)) * ABI_WORD_SZ;
    }
  }
  return dataOff;
}

//===============================================
// API
//===============================================
bool is_tuple_type(ABI_t t) {
  return (t.type <= ABI_TUPLE20 && t.type >= ABI_TUPLE1);
}

int get_tuple_sz(ABI_t t) {
  if (!is_tuple_type(t))
    return -1;
  return (t.type - ABI_TUPLE1) + 1; 
}

bool abi_is_valid_schema(const ABI_t * types, size_t numTypes) {
  if (!types)
    return false;
  for (size_t i = 0; i < numTypes; i++) {
    if ((types[i].type >= ABI_MAX || types[i].type <= ABI_NONE) ||
        ( (!is_single_elementary_type(types[i])) &&
          (!is_single_dynamic_type(types[i])) &&
          (!is_tuple_type(types[i])) &&
          (!is_elementary_type_fixed_sz_array(types[i])) &&
          (!is_elementary_type_variable_sz_array(types[i])) &&
          (!is_dynamic_type_fixed_sz_array(types[i])) &&
          (!is_dynamic_type_variable_sz_array(types[i])) ))
      return false;
  }
  return true;
}

int abi_get_array_sz( const ABI_t * types, 
                      size_t numTypes, 
                      ABISelector_t info, 
                      const void * in,
                      size_t inSz)
{
  if (!types || !in)
    return -1;
  ABI_t type = types[info.typeIdx];
  if (info.typeIdx >= numTypes || !abi_is_valid_schema(types, numTypes))
    return -1;
  // Fixed size arrays have size included
  if (!is_variable_sz_array(type))
    return type.arraySz;
  // Get the offset of this parameter in the function definition
  size_t paramOff = get_param_offset(types, numTypes, info, in, inSz);
  if (paramOff > inSz)
    return -1;
  // The parameter at `paramOff` contains an offset where the array
  // data begins. Since the first word in a variable sized array
  // (which this must be) contains the array size, that's what we need.
  return get_abi_u32_be(in, paramOff);
}

int abi_get_tuple_param_array_sz( const ABI_t * types, 
                                  size_t numTypes, 
                                  ABISelector_t tupleInfo,
                                  ABISelector_t paramInfo, 
                                  const void * in,
                                  size_t inSz)
{
  if (!types || !in)
    return -1;
  int typeIdx = get_first_tuple_param_idx(types, numTypes, tupleInfo.typeIdx);
  if (typeIdx < 0)
    return -1;
  typeIdx += paramInfo.typeIdx;
  ABI_t type = types[typeIdx];
  if (typeIdx >= numTypes || !abi_is_valid_schema(types, numTypes))
    return -1;
  // Fixed size arrays have size included
  if (!is_variable_sz_array(type))
    return type.arraySz;
  // Update types pointer offset to skip non-tuple types. This allows us to treat
  // the tuple as its own sort of "nested" definition.
  int firstTupleParamIdx = get_first_tuple_param_idx(types, numTypes, tupleInfo.typeIdx);
  if (firstTupleParamIdx < 0)
    return -1;
  const ABI_t * tupleTypes = types + firstTupleParamIdx;
  // Get the offset at which the tuple data starts
  size_t dataOff = get_tuple_data_start(types, numTypes, tupleInfo, in, inSz);
  // Jump to the start of our tuple item
  in += dataOff;
  inSz -= dataOff;
  size_t paramOff = get_param_offset(tupleTypes, (numTypes - firstTupleParamIdx), paramInfo, in, inSz);
  return get_abi_u32_be(in, paramOff);
}

int abi_decode_param( void * out, 
                      size_t outSz, 
                      const ABI_t * types, 
                      size_t numTypes, 
                      ABISelector_t info, 
                      const void * in,
                      size_t inSz) 
{
  if (!out || !types || !in)
    return -1;
  // Ensure we have valid types passed
  if ((info.typeIdx >= numTypes) ||
      (!abi_is_valid_schema(types, numTypes)))
    return -1;
  size_t paramOff = get_param_offset(types, numTypes, info, in, inSz);
  if (paramOff > inSz)
    return -1;
  return decode_param(out, outSz, types[info.typeIdx], in, inSz, paramOff, info);
}

int abi_decode_tuple_param( void * out, 
                            size_t outSz, 
                            const ABI_t * types, 
                            size_t numTypes,
                            ABISelector_t tupleInfo,
                            ABISelector_t paramInfo, 
                            const void * in,
                            size_t inSz) 
{
  if (!out || !types || !in)
    return -1;
  // Ensure we have valid types passed
  if (tupleInfo.typeIdx >= numTypes || !abi_is_valid_schema(types, numTypes))
    return -1;
  ABI_t tupleType = types[tupleInfo.typeIdx];
  size_t tupleTypeSz = get_tuple_sz(tupleType);
  // Sanity check: ensure this is a tuple type
  if (!is_tuple_type(tupleType))
    return -1;
  // Sanity check: ensure we won't overrun our buffer
  if (paramInfo.typeIdx > tupleTypeSz || paramInfo.typeIdx > numTypes)
    return -1;
  // Update types pointer offset to skip non-tuple types. This allows us to treat
  // the tuple as its own sort of "nested" definition.
  int paramIdx = get_first_tuple_param_idx(types, numTypes, tupleInfo.typeIdx);
  if (paramIdx < 0)
    return -1;
  const ABI_t * tupleTypes = types + paramIdx;

  // Get the offset at which the tuple data starts
  size_t dataOff = get_tuple_data_start(types, numTypes, tupleInfo, in, inSz);
  // Jump to the start of our tuple item
  in += dataOff;
  inSz -= dataOff;
  return abi_decode_param(out, 
                          outSz, 
                          tupleTypes, 
                          tupleTypeSz,
                          paramInfo,
                          in,
                          inSz);
}

int abi_encode( void * out, 
                size_t outSz, 
                const ABI_t * types, 
                size_t numTypes, 
                size_t * offsets, 
                const void * in, 
                size_t inSz)
{
  while (!out || !types || !in);
  if (numTypes == 0 || outSz == 0 || inSz == 0 || (!abi_is_valid_schema(types, numTypes)))
    return -1;
  size_t numWritten = 0;
  size_t dynamicCount = 0;
  for (size_t i = 0; i < numTypes; i++) {
    void * loc = out + (ABI_WORD_SZ * i);

    // TODO: Expand coverage beyond simplisitc types (if demand requires it)
    // Sanity check on the type -- make sure it is allowed
    if (is_tuple_type(types[i]) || types[i].isArray)
      return -1;

    // Get the size of the data
    size_t _sz = 0;
    size_t _off = offsets[i];
    if (i < numTypes - 1) {
      if (offsets[i+1] <= _off) // Ensure offsets increase monotonically
        return -1;
      _sz = offsets[i+1] - _off;
    } else {
      // Last offset can be compared against inSz
      _sz = inSz - _off;
    }

    // Avoid overrunning the buffer
    if ((ABI_WORD_SZ * i) + _sz > outSz)
      return -1;

    // Write the param
    if (is_dynamic_atomic_type(types[i])) {
      // Dynamic types gencodeo at the end of the buffer
      // Write the offset in the buffer in this slot (in big endian)
      size_t dynamicDataOff = (size_t) ABI_WORD_SZ * (numTypes + dynamicCount);
      write_u32_be((loc+ABI_WORD_SZ-4), dynamicDataOff);
      // Write the data size at the edge of teh buffer
      write_u32_be((out+dynamicDataOff+ABI_WORD_SZ-4), _sz);
      // Write the data at the end of the buffer
      memcpy((out+dynamicDataOff+ABI_WORD_SZ), in+_off, _sz);
      // Account for the number of words we just wrote. Ifencode the data exceeds one word,
      // it will wrap into another.
      size_t numWords = 2 + (_sz / ABI_WORD_SZ); // 2 accounts for the size word
      dynamicCount += numWords;
      numWritten += numWords * ABI_WORD_SZ;
    } else {
      // All other params are written to the location
      // Bytes types are written to the front of the word. All others are left padded.
      size_t outOff = 0;
      if (!is_fixed_bytes_type(types[i]))
        outOff += (ABI_WORD_SZ - _sz);
      memcpy((loc+outOff), (in+_off), _sz);
    }
    numWritten += ABI_WORD_SZ;
  }

  return numWritten;
}