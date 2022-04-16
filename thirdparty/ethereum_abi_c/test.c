#include "abi.h"
#include "test_vec.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])

//===============================================================
// TESTS
//===============================================================


static inline uint32_t get_u32_be(uint8_t * in, size_t off) {
  return (in[off + 3] | in[off + 2] << 8 | in[off + 1] << 16 | in[off + 0] << 24);
}

static inline void test_ex1(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex1_encoded+4;
  size_t inSz = sizeof(ex1_encoded) - 4;
  printf("Example 1...");
  // function baz(uint32 x, bool y)
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) == sizeof(ex1_param_0));
  assert((out[3] | out[2] << 8 | out[1] << 16 | out[0] << 24) == ex1_param_0);
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) == sizeof(ex1_param_1));
  assert((bool) out[0] == ex1_param_1);
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex2(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex2_encoded+4;
  size_t inSz = sizeof(ex2_encoded) - 4;
  printf("Example 2...");
  // function bar(bytes3[2])
  info.typeIdx = 0;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex2_abi, ARRAY_SIZE(ex2_abi), info, in, inSz) == sizeof(ex2_param_00));
  assert(0 == memcmp(ex2_param_00, out, sizeof(ex2_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex2_abi, ARRAY_SIZE(ex2_abi), info, in, inSz) == sizeof(ex2_param_01));
  assert(0 == memcmp(ex2_param_01, out, sizeof(ex2_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 0;
  printf("passed.\n\r");
}

static inline void test_ex3(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex3_encoded+4;
  size_t inSz = sizeof(ex3_encoded) - 4;
  printf("Example 3...");
  // function sam(bytes, bool, uint[])
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_0)); // 4 bytes in payload
  assert(0 == memcmp(ex3_param_0, out, sizeof(ex3_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_1)); // 1 byte for bool
  assert((bool) out[0] == ex3_param_1);
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_20)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_20, out, sizeof(ex3_param_20)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_21)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_21, out, sizeof(ex3_param_21)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == sizeof(ex3_param_22)); // 3 uint values, each 32 bytes
  assert(0 == memcmp(ex3_param_22, out, sizeof(ex3_param_22)));
  memset(out, 0, outSz);
  info.arrIdx = 0;

  // Validate variable array size
  info.typeIdx = 2;
  assert(abi_get_array_sz(ex3_abi, ARRAY_SIZE(ex3_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex4(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex4_encoded+4;
  size_t inSz = sizeof(ex4_encoded) - 4;
  printf("Example 4...");
  // f(uint,uint32[],bytes10,bytes)
  info.typeIdx = 0;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_0)); // uint = uint256 = 32 bytes
  assert(0 == memcmp(ex4_param_0, out, sizeof(ex4_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_10)); // uint32 (see payload)
  assert(get_u32_be(out, 0) == ex4_param_10);
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_11)); // uint32 (see payload)
  assert(get_u32_be(out, 0) == ex4_param_11);
  memset(out, 0, outSz);
  info.arrIdx =  0;
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_2)); // bytes10 = 10 bytes
  assert(0 == memcmp(ex4_param_2, out, sizeof(ex4_param_2)));
  memset(out, 0, outSz);
  info.typeIdx = 3;
  assert(abi_decode_param(out, outSz, ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == sizeof(ex4_param_3)); // 0x0d = 13 bytes
  assert(0 == memcmp(ex4_param_3, out, sizeof(ex4_param_3)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex4_abi, ARRAY_SIZE(ex4_abi), info, in, inSz) == 2);

  printf("passed.\n\r");
}

static inline void test_ex5(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex5_encoded+4;
  size_t inSz = sizeof(ex5_encoded) - 4;
  printf("Example 5...");
  // f(uint[3],uint[])
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_00));
  assert(0 == memcmp(ex5_param_00, out, sizeof(ex5_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_01));
  assert(0 == memcmp(ex5_param_01, out, sizeof(ex5_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_02));
  assert(0 == memcmp(ex5_param_02, out, sizeof(ex5_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_10));
  assert(0 == memcmp(ex5_param_10, out, sizeof(ex5_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == sizeof(ex5_param_11));
  assert(0 == memcmp(ex5_param_11, out, sizeof(ex5_param_11)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == 2);

  printf("passed.\n\r");
}

static inline void test_ex6(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex6_encoded;
  size_t inSz = sizeof(ex6_encoded);
  printf("Example 6...");
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_00));
  assert(0 == memcmp(ex6_param_00, out, sizeof(ex6_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_01));
  assert(0 == memcmp(ex6_param_01, out, sizeof(ex6_param_01)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_10));
  assert(0 == memcmp(ex6_param_10, out, sizeof(ex6_param_10)));
  memset(out, 0, outSz);  
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_11));
  assert(0 == memcmp(ex6_param_11, out, sizeof(ex6_param_11)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == sizeof(ex6_param_20));
  assert(0 == memcmp(ex6_param_20, out, sizeof(ex6_param_20)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex6_abi, ARRAY_SIZE(ex6_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex7(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex7_encoded;
  size_t inSz = sizeof(ex7_encoded);
  printf("Example 7...");
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_00));
  assert(0 == memcmp(ex7_param_00, out, sizeof(ex7_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_01));
  assert(0 == memcmp(ex7_param_01, out, sizeof(ex7_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_02));
  assert(0 == memcmp(ex7_param_02, out, sizeof(ex7_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_10));
  assert(0 == memcmp(ex7_param_10, out, sizeof(ex7_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == sizeof(ex7_param_11));
  assert(0 == memcmp(ex7_param_11, out, sizeof(ex7_param_11)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 1;
  assert(abi_get_array_sz(ex7_abi, ARRAY_SIZE(ex7_abi), info, in, inSz) == 2);
  printf("passed.\n\r");
}

static inline void test_ex8(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex8_encoded;
  size_t inSz = sizeof(ex8_encoded);
  printf("Example 8...");
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_00));
  assert(0 == memcmp(ex8_param_00, out, sizeof(ex8_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_01));
  assert(0 == memcmp(ex8_param_01, out, sizeof(ex8_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_02));
  assert(0 == memcmp(ex8_param_02, out, sizeof(ex8_param_02)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_10));
  assert(0 == memcmp(ex8_param_10, out, sizeof(ex8_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == sizeof(ex8_param_11));
  assert(0 == memcmp(ex8_param_11, out, sizeof(ex8_param_11)));
  memset(out, 0, outSz);

  // Validate array size
  info.typeIdx = 0;
  info.arrIdx = 2;
  assert(abi_get_array_sz(ex8_abi, ARRAY_SIZE(ex8_abi), info, in, inSz) == 3);

  printf("passed.\n\r");
}

static inline void test_ex9(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex9_encoded;
  size_t inSz = sizeof(ex9_encoded);
  printf("Example 9...");

  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_0));
  assert(0 == memcmp(ex9_param_0, out, sizeof(ex9_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_10));
  assert(0 == memcmp(ex9_param_10, out, sizeof(ex9_param_10)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_20));
  assert(0 == memcmp(ex9_param_20, out, sizeof(ex9_param_20)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_21));
  assert(0 == memcmp(ex9_param_21, out, sizeof(ex9_param_21)));
  memset(out, 0, outSz);
  info.typeIdx = 3;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex9_abi, ARRAY_SIZE(ex9_abi), info, in, inSz) == sizeof(ex9_param_3));
  assert(0 == memcmp(ex9_param_3, out, sizeof(ex9_param_3)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex10(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex10_encoded;
  size_t inSz = sizeof(ex10_encoded);
  printf("Example 10...");
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_00));
  assert(0 == memcmp(ex10_param_00, out, sizeof(ex10_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_01));
  assert(0 == memcmp(ex10_param_01, out, sizeof(ex10_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_02));
  assert(0 == memcmp(ex10_param_02, out, sizeof(ex10_param_02)));
  memset(out, 0, outSz);

  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_10));
  assert(0 == memcmp(ex10_param_10, out, sizeof(ex10_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex10_abi, ARRAY_SIZE(ex10_abi), info, in, inSz) == sizeof(ex10_param_11));
  assert(0 == memcmp(ex10_param_11, out, sizeof(ex10_param_11)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex11(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex11_encoded;
  size_t inSz = sizeof(ex11_encoded);
  printf("Example 11...");
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_0));
  assert(0 == memcmp(ex11_param_0, out, sizeof(ex11_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_10));
  assert(0 == memcmp(ex11_param_10, out, sizeof(ex11_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_11));
  assert(0 == memcmp(ex11_param_11, out, sizeof(ex11_param_11)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_12));
  assert(0 == memcmp(ex11_param_12, out, sizeof(ex11_param_12)));
  memset(out, 0, outSz);
  info.arrIdx = 3;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_13));
  assert(0 == memcmp(ex11_param_13, out, sizeof(ex11_param_13)));
  memset(out, 0, outSz);
  info.arrIdx = 4;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_14));
  assert(0 == memcmp(ex11_param_14, out, sizeof(ex11_param_14)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex11_abi, ARRAY_SIZE(ex11_abi), info, in, inSz) == sizeof(ex11_param_2));
  assert(0 == memcmp(ex11_param_2, out, sizeof(ex11_param_2)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex12(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex12_encoded;
  size_t inSz = sizeof(ex12_encoded);
  printf("Example 12...");
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_00));
  assert(0 == memcmp(ex12_param_00, out, sizeof(ex12_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_01));
  assert(0 == memcmp(ex12_param_01, out, sizeof(ex12_param_01)));
  memset(out, 0, outSz);
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_02));
  assert(0 == memcmp(ex12_param_02, out, sizeof(ex12_param_02)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_1));
  assert(0 == memcmp(ex12_param_1, out, sizeof(ex12_param_1)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_2));
  assert(get_u32_be(out, 0) == ex12_param_2);
  memset(out, 0, outSz);
  info.typeIdx = 3;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_30));
  assert(0 == memcmp(ex12_param_30, out, sizeof(ex12_param_30)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex12_abi, ARRAY_SIZE(ex12_abi), info, in, inSz) == sizeof(ex12_param_31));
  assert(0 == memcmp(ex12_param_31, out, sizeof(ex12_param_31)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex13(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex13_encoded;
  size_t inSz = sizeof(ex13_encoded);
  printf("Example 13...");
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_00));
  assert(0 == memcmp(ex13_param_00, out, sizeof(ex13_param_00)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_01));
  assert(0 == memcmp(ex13_param_01, out, sizeof(ex13_param_01)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  info.arrIdx = 0;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_10));
  assert(0 == memcmp(ex13_param_10, out, sizeof(ex13_param_10)));
  memset(out, 0, outSz);
  info.arrIdx = 1;
  assert(abi_decode_param(out, outSz, ex13_abi, ARRAY_SIZE(ex13_abi), info, in, inSz) == sizeof(ex13_param_11));
  assert(0 == memcmp(ex13_param_11, out, sizeof(ex13_param_11)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_ex14(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = ex14_encoded;
  size_t inSz = sizeof(ex14_encoded);
  printf("Example 14...");
  assert(abi_decode_param(out, outSz, ex14_abi, ARRAY_SIZE(ex14_abi), info, in, inSz) == sizeof(ex14_param_0));
  assert(0 == memcmp(ex14_param_0, out, sizeof(ex14_param_0)));
  memset(out, 0, outSz);
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex14_abi, ARRAY_SIZE(ex14_abi), info, in, inSz) == sizeof(ex14_param_1));
  assert(0 == memcmp(ex14_param_1, out, sizeof(ex14_param_1)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, ex14_abi, ARRAY_SIZE(ex14_abi), info, in, inSz) == sizeof(ex14_param_2));
  assert(0 == memcmp(ex14_param_2, out, sizeof(ex14_param_2)));
  memset(out, 0, outSz);
  info.typeIdx = 3;
  assert(abi_decode_param(out, outSz, ex14_abi, ARRAY_SIZE(ex14_abi), info, in, inSz) == sizeof(ex14_param_3));
  assert(0 == memcmp(ex14_param_3, out, sizeof(ex14_param_3)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}

static inline void test_fillOrder(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = fillOrder_encoded+4;
  size_t inSz = sizeof(fillOrder_encoded) - 4;
  printf("(Tuple) FillOrder...");

  assert(true == abi_is_valid_schema(fillOrder_abi, ARRAY_SIZE(fillOrder_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), info, in, inSz) == sizeof(fillOrder_p1));
  assert(0 == memcmp(fillOrder_p1, out, sizeof(fillOrder_p1)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert(abi_decode_param(out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), info, in, inSz) == sizeof(fillOrder_p2_0));
  assert(0 == memcmp(fillOrder_p2_0, out, sizeof(fillOrder_p2_0)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  size_t decSz;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t0));
  assert(0 == memcmp(fillOrder_p0_t0, out, decSz));

  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t1));
  assert(0 == memcmp(fillOrder_p0_t1, out, decSz));

  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t2));
  assert(0 == memcmp(fillOrder_p0_t2, out, decSz));
  
  paramInfo.typeIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t3));
  assert(0 == memcmp(fillOrder_p0_t3, out, decSz));
  
  paramInfo.typeIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t4));
  assert(0 == memcmp(fillOrder_p0_t4, out, decSz));
  
  paramInfo.typeIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t5));
  assert(0 == memcmp(fillOrder_p0_t5, out, decSz));

  paramInfo.typeIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t6));
  assert(0 == memcmp(fillOrder_p0_t6, out, decSz));

  paramInfo.typeIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t7));
  assert(0 == memcmp(fillOrder_p0_t7, out, decSz));

  paramInfo.typeIdx = 8;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t8));
  assert(0 == memcmp(fillOrder_p0_t8, out, decSz));

  paramInfo.typeIdx = 9;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t9));
  assert(0 == memcmp(fillOrder_p0_t9, out, decSz));

  paramInfo.typeIdx = 10;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t10));
  assert(0 == memcmp(fillOrder_p0_t10, out, decSz));

  paramInfo.typeIdx = 11;
  decSz = abi_decode_tuple_param( out, outSz, fillOrder_abi, ARRAY_SIZE(fillOrder_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(fillOrder_p0_t11));
  assert(0 == memcmp(fillOrder_p0_t11, out, decSz));

  printf("passed.\n\r");
}

static inline void test_marketSellOrders(uint8_t * out, size_t outSz) {
  ABISelector_t tupleInfo = { .typeIdx = 0 };
  uint8_t * in = marketSellOrders_encoded+4;
  size_t inSz = sizeof(marketSellOrders_encoded) - 4;
  printf("(Tuple) marketSellOrders...");

  assert(true == abi_is_valid_schema(marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi)));

  // Non-tuple params
  tupleInfo.typeIdx = 1;
  assert(abi_decode_param(out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                          tupleInfo, in, inSz) == sizeof(marketSellOrders_p1));
  assert(0 == memcmp(marketSellOrders_p1, out, sizeof(marketSellOrders_p1)));
  memset(out, 0, outSz);
  tupleInfo.typeIdx = 2;
  tupleInfo.arrIdx = 0;

  assert(abi_decode_param(out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                          tupleInfo, in, inSz) == sizeof(marketSellOrders_p2_0));
  assert(0 == memcmp(marketSellOrders_p2_0, out, sizeof(marketSellOrders_p2_0)));
  memset(out, 0, outSz);

  // Tuple types
  tupleInfo.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  size_t decSz;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p0));
  assert(0 == memcmp(marketSellOrders_p0_t0_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p1));
  assert(0 == memcmp(marketSellOrders_p0_t0_p1, out, decSz));
  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p2));
  assert(0 == memcmp(marketSellOrders_p0_t0_p2, out, decSz));
  paramInfo.typeIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p3));
  assert(0 == memcmp(marketSellOrders_p0_t0_p3, out, decSz));
  paramInfo.typeIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p4));
  assert(0 == memcmp(marketSellOrders_p0_t0_p4, out, decSz));
  paramInfo.typeIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p5));
  assert(0 == memcmp(marketSellOrders_p0_t0_p5, out, decSz));
  paramInfo.typeIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p6));
  assert(0 == memcmp(marketSellOrders_p0_t0_p6, out, decSz));
  paramInfo.typeIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p7));
  assert(0 == memcmp(marketSellOrders_p0_t0_p7, out, decSz));
  paramInfo.typeIdx = 8;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p8));
  assert(0 == memcmp(marketSellOrders_p0_t0_p8, out, decSz));
  paramInfo.typeIdx = 9;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p9));
  assert(0 == memcmp(marketSellOrders_p0_t0_p9, out, decSz));
  paramInfo.typeIdx = 10;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p10));
  assert(0 == memcmp(marketSellOrders_p0_t0_p10, out, decSz));
  paramInfo.typeIdx = 11;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t0_p11));
  assert(0 == memcmp(marketSellOrders_p0_t0_p11, out, decSz));

  tupleInfo.arrIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p0));
  assert(0 == memcmp(marketSellOrders_p0_t1_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p1));
  assert(0 == memcmp(marketSellOrders_p0_t1_p1, out, decSz));
  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p2));
  assert(0 == memcmp(marketSellOrders_p0_t1_p2, out, decSz));
  paramInfo.typeIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p3));
  assert(0 == memcmp(marketSellOrders_p0_t1_p3, out, decSz));
  paramInfo.typeIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p4));
  assert(0 == memcmp(marketSellOrders_p0_t1_p4, out, decSz));
  paramInfo.typeIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p5));
  assert(0 == memcmp(marketSellOrders_p0_t1_p5, out, decSz));
  paramInfo.typeIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p6));
  assert(0 == memcmp(marketSellOrders_p0_t1_p6, out, decSz));
  paramInfo.typeIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p7));
  assert(0 == memcmp(marketSellOrders_p0_t1_p7, out, decSz));
  paramInfo.typeIdx = 8;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p8));
  assert(0 == memcmp(marketSellOrders_p0_t1_p8, out, decSz));
  paramInfo.typeIdx = 9;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p9));
  assert(0 == memcmp(marketSellOrders_p0_t1_p9, out, decSz));
  paramInfo.typeIdx = 10;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p10));
  assert(0 == memcmp(marketSellOrders_p0_t1_p10, out, decSz));
  paramInfo.typeIdx = 11;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t1_p11));
  assert(0 == memcmp(marketSellOrders_p0_t1_p11, out, decSz));

  tupleInfo.arrIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p0));
  assert(0 == memcmp(marketSellOrders_p0_t2_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p1));
  assert(0 == memcmp(marketSellOrders_p0_t2_p1, out, decSz));
  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p2));
  assert(0 == memcmp(marketSellOrders_p0_t2_p2, out, decSz));
  paramInfo.typeIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p3));
  assert(0 == memcmp(marketSellOrders_p0_t2_p3, out, decSz));
  paramInfo.typeIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p4));
  assert(0 == memcmp(marketSellOrders_p0_t2_p4, out, decSz));
  paramInfo.typeIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p5));
  assert(0 == memcmp(marketSellOrders_p0_t2_p5, out, decSz));
  paramInfo.typeIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p6));
  assert(0 == memcmp(marketSellOrders_p0_t2_p6, out, decSz));
  paramInfo.typeIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p7));
  assert(0 == memcmp(marketSellOrders_p0_t2_p7, out, decSz));
  paramInfo.typeIdx = 8;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p8));
  assert(0 == memcmp(marketSellOrders_p0_t2_p8, out, decSz));
  paramInfo.typeIdx = 9;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p9));
  assert(0 == memcmp(marketSellOrders_p0_t2_p9, out, decSz));
  paramInfo.typeIdx = 10;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p10));
  assert(0 == memcmp(marketSellOrders_p0_t2_p10, out, decSz));
  paramInfo.typeIdx = 11;
  decSz = abi_decode_tuple_param( out, outSz, marketSellOrders_abi, ARRAY_SIZE(marketSellOrders_abi), 
                                  tupleInfo, paramInfo, in, inSz);
  assert(decSz == sizeof(marketSellOrders_p0_t2_p11));
  assert(0 == memcmp(marketSellOrders_p0_t2_p11, out, decSz));

  printf("passed.\n\r");
}

static inline void test_tupleElementary(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleElementary_encoded;
  size_t inSz = sizeof(tupleElementary_encoded);
  printf("(Tuple) tupleElementary_encoded...");

  assert(true == abi_is_valid_schema(tupleElementary_abi, ARRAY_SIZE(tupleElementary_abi)));

  // Non-tuple params
  assert( abi_decode_param(out, outSz, tupleElementary_abi, ARRAY_SIZE(tupleElementary_abi), 
          info, in, inSz) == sizeof(tupleElementary_p0));
  assert(0 == memcmp(tupleElementary_p0, out, sizeof(tupleElementary_p0)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert( abi_decode_param(out, outSz, tupleElementary_abi, ARRAY_SIZE(tupleElementary_abi), 
          info, in, inSz) == sizeof(tupleElementary_p2));
  assert(0 == memcmp(tupleElementary_p2, out, sizeof(tupleElementary_p2)));
  memset(out, 0, outSz);

  // Tuple params
  info.typeIdx = 1;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  assert( abi_decode_tuple_param(out, outSz, tupleElementary_abi, ARRAY_SIZE(tupleElementary_abi), 
          info, paramInfo, in, inSz) == sizeof(tupleElementary_p1_t0_p0));
  assert(0 == memcmp(tupleElementary_p1_t0_p0, out, sizeof(tupleElementary_p1_t0_p0)));
  memset(out, 0, outSz);
  paramInfo.typeIdx = 1;
  assert( abi_decode_tuple_param(out, outSz, tupleElementary_abi, ARRAY_SIZE(tupleElementary_abi), 
          info, paramInfo, in, inSz) == sizeof(tupleElementary_p1_t0_p1));
  assert(0 == memcmp(tupleElementary_p1_t0_p1, out, sizeof(tupleElementary_p1_t0_p1)));
  memset(out, 0, outSz);
  printf("passed.\n\r");
}


static inline void test_tupleFixedArray0(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleFixedArray0_encoded;
  size_t inSz = sizeof(tupleFixedArray0_encoded);
  printf("(Tuple) tupleFixedArray0_encoded...");

  assert(true == abi_is_valid_schema(tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi)));

  // Non-tuple params
  assert( abi_decode_param(out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
          info, in, inSz) == sizeof(tupleFixedArray0_p0));
  assert(0 == memcmp(tupleFixedArray0_p0, out, sizeof(tupleFixedArray0_p0)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert( abi_decode_param(out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
          info, in, inSz) == sizeof(tupleFixedArray0_p2_0));
  assert(0 == memcmp(tupleFixedArray0_p2_0, out, sizeof(tupleFixedArray0_p2_0)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 1;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  size_t decSz;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray0_p1_t0_p0));
  assert(0 == memcmp(tupleFixedArray0_p1_t0_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray0_p1_t0_p1));
  assert(0 == memcmp(tupleFixedArray0_p1_t0_p1, out, decSz));
  info.arrIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray0_p1_t1_p0));
  assert(0 == memcmp(tupleFixedArray0_p1_t1_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray0_abi, ARRAY_SIZE(tupleFixedArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray0_p1_t1_p1));
  assert(0 == memcmp(tupleFixedArray0_p1_t1_p1, out, decSz));

  printf("passed.\n\r");
}

static inline void test_tupleFixedArray1(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleFixedArray1_encoded;
  size_t inSz = sizeof(tupleFixedArray1_encoded);
  printf("(Tuple) tupleFixedArray1_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi)));

  // Non-tuple params
  decSz = abi_decode_param( out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleFixedArray1_p0));
  assert(0 == memcmp(tupleFixedArray1_p0, out, sizeof(tupleFixedArray1_p0)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  assert( abi_decode_param(out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
          info, in, inSz) == sizeof(tupleFixedArray1_p2));
  assert(0 == memcmp(tupleFixedArray1_p2, out, sizeof(tupleFixedArray1_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 1;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray1_p1_t0_p0));
  assert(0 == memcmp(tupleFixedArray1_p1_t0_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray1_p1_t0_p1));
  assert(0 == memcmp(tupleFixedArray1_p1_t0_p1, out, decSz));
  info.arrIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray1_p1_t1_p0));
  assert(0 == memcmp(tupleFixedArray1_p1_t1_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleFixedArray1_p1_t1_p1));
  assert(0 == memcmp(tupleFixedArray1_p1_t1_p1, out, decSz));

  // Fixed size array
  assert(tupleFixedArray1_abi[info.typeIdx].arraySz == abi_get_array_sz(tupleFixedArray1_abi, ARRAY_SIZE(tupleFixedArray1_abi), info, in, inSz));

  printf("passed.\n\r");
}

static inline void test_tupleVarArray0(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleVarArray0_encoded;
  size_t inSz = sizeof(tupleVarArray0_encoded);
  printf("(Tuple) tupleVarArray0_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi)));

  // Non-tuple params
  decSz = abi_decode_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p0));
  assert(0 == memcmp(tupleVarArray0_p0, out, sizeof(tupleVarArray0_p0)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p2));
  assert(0 == memcmp(tupleVarArray0_p2, out, sizeof(tupleVarArray0_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 1;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p1_t0_p0));
  assert(0 == memcmp(tupleVarArray0_p1_t0_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p1_t0_p1));
  assert(0 == memcmp(tupleVarArray0_p1_t0_p1, out, decSz));
  info.arrIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p1_t1_p0));
  assert(0 == memcmp(tupleVarArray0_p1_t1_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray0_p1_t1_p1));
  assert(0 == memcmp(tupleVarArray0_p1_t1_p1, out, decSz));

  assert(2 == abi_get_array_sz(tupleVarArray0_abi, ARRAY_SIZE(tupleVarArray0_abi), info, in, inSz));

  printf("passed.\n\r");
}

static inline void test_tupleVarArray1(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleVarArray1_encoded;
  size_t inSz = sizeof(tupleVarArray1_encoded);
  printf("(Tuple) tupleVarArray1_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi)));

  // Non-tuple params
  decSz = abi_decode_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p0));
  assert(0 == memcmp(tupleVarArray1_p0, out, sizeof(tupleVarArray1_p0)));
  memset(out, 0, outSz);
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p2));
  assert(0 == memcmp(tupleVarArray1_p2, out, sizeof(tupleVarArray1_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 1;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p1_t0_p0));
  assert(0 == memcmp(tupleVarArray1_p1_t0_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p1_t0_p1));
  assert(0 == memcmp(tupleVarArray1_p1_t0_p1, out, decSz));
  info.arrIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p1_t1_p0));
  assert(0 == memcmp(tupleVarArray1_p1_t1_p0, out, decSz));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray1_p1_t1_p1));
  assert(0 == memcmp(tupleVarArray1_p1_t1_p1, out, decSz));

  assert(2 == abi_get_array_sz(tupleVarArray1_abi, ARRAY_SIZE(tupleVarArray1_abi), info, in, inSz));

  printf("passed.\n\r");
}

static inline void test_tupleVarArray2(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleVarArray2_encoded;
  size_t inSz = sizeof(tupleVarArray2_encoded);
  printf("(Tuple) tupleVarArray2_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi)));
  assert(2 == abi_get_array_sz(tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), info, in, inSz));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1));
  assert(0 == memcmp(tupleVarArray2_p1, out, sizeof(tupleVarArray2_p1)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p0_0));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p0_1));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p0_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p0_2));
  paramInfo.typeIdx = 1;
  assert(0 == memcmp(tupleVarArray2_p1_t0_p0_2, out, decSz));
  paramInfo.arrIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_0));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_1));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_2));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_2, out, decSz));
  paramInfo.arrIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_3));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_3, out, decSz));
  paramInfo.arrIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_4));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_4, out, decSz));
  paramInfo.arrIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_5));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_5, out, decSz));
  paramInfo.arrIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_6));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_6, out, decSz));
  paramInfo.arrIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p1_7));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p1_7, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t0_p2));
  assert(0 == memcmp(tupleVarArray2_p1_t0_p2, out, decSz));

  info.arrIdx = 1;
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 0;

  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p0_0));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p0_1));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p0_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p0_2));
  paramInfo.typeIdx = 1;
  assert(0 == memcmp(tupleVarArray2_p1_t1_p0_2, out, decSz));
  paramInfo.arrIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_0));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_1));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_2));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_2, out, decSz));
  paramInfo.arrIdx = 3;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_3));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_3, out, decSz));
  paramInfo.arrIdx = 4;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_4));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_4, out, decSz));
  paramInfo.arrIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_5));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_5, out, decSz));
  paramInfo.arrIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_6));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_6, out, decSz));
  paramInfo.arrIdx = 7;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p1_7));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p1_7, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray2_p1_t1_p2));
  assert(0 == memcmp(tupleVarArray2_p1_t1_p2, out, decSz));

  printf("passed.\n\r");
}

static inline void test_tupleVarArray3(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleVarArray3_encoded;
  size_t inSz = sizeof(tupleVarArray3_encoded);
  printf("(Tuple) tupleVarArray3_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi)));
  assert(2 == abi_get_array_sz(tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), info, in, inSz));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p1));
  assert(0 == memcmp(tupleVarArray3_p1, out, sizeof(tupleVarArray3_p1)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  assert(3 == abi_get_tuple_param_array_sz(tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi),
                                            info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t0_p0_0));
  assert(0 == memcmp(tupleVarArray3_p0_t0_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t0_p0_1));
  assert(0 == memcmp(tupleVarArray3_p0_t0_p0_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t0_p0_2));
  assert(0 == memcmp(tupleVarArray3_p0_t0_p0_2, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t0_p1));
  assert(0 == memcmp(tupleVarArray3_p0_t0_p1, out, decSz));
  // Validate that non-array returns array size of 0
  assert(0 == abi_get_tuple_param_array_sz(tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi),
                                            info, paramInfo, in, inSz));

  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  assert(2 == abi_get_tuple_param_array_sz(tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi),
                                            info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t1_p0_0));
  assert(0 == memcmp(tupleVarArray3_p0_t1_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t1_p0_1));
  assert(0 == memcmp(tupleVarArray3_p0_t1_p0_1, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray3_abi, ARRAY_SIZE(tupleVarArray3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray3_p0_t1_p1));
  assert(0 == memcmp(tupleVarArray3_p0_t1_p1, out, decSz));

  printf("passed.\n\r");
}

static inline void test_tupleVarArray4(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleVarArray4_encoded;
  size_t inSz = sizeof(tupleVarArray4_encoded);
  printf("(Tuple) tupleVarArray4_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi)));
  assert(2 == abi_get_array_sz(tupleVarArray2_abi, ARRAY_SIZE(tupleVarArray2_abi), info, in, inSz));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p1));
  assert(0 == memcmp(tupleVarArray4_p1, out, sizeof(tupleVarArray4_p1)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  assert(3 == abi_get_tuple_param_array_sz(tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi),
                                            info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t0_p0_0));
  assert(0 == memcmp(tupleVarArray4_p0_t0_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t0_p0_1));
  assert(0 == memcmp(tupleVarArray4_p0_t0_p0_1, out, decSz));
  paramInfo.arrIdx = 2;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t0_p0_2));
  assert(0 == memcmp(tupleVarArray4_p0_t0_p0_2, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t0_p1));
  assert(0 == memcmp(tupleVarArray4_p0_t0_p1, out, decSz));
  // Validate that non-array returns array size of 0
  assert(0 == abi_get_tuple_param_array_sz(tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi),
                                            info, paramInfo, in, inSz));

  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  assert(2 == abi_get_tuple_param_array_sz(tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi),
                                            info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t1_p0_0));
  assert(0 == memcmp(tupleVarArray4_p0_t1_p0_0, out, decSz));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t1_p0_1));
  assert(0 == memcmp(tupleVarArray4_p0_t1_p0_1, out, decSz));
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleVarArray4_abi, ARRAY_SIZE(tupleVarArray4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleVarArray4_p0_t1_p1));
  assert(0 == memcmp(tupleVarArray4_p0_t1_p1, out, decSz));

  printf("passed.\n\r");
}

static inline void test_tupleMulti1(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti1_encoded;
  size_t inSz = sizeof(tupleMulti1_encoded);
  printf("(Tuple) tupleMulti1_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi)));

  // Non-tuple params
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti1_p2));
  assert(0 == memcmp(tupleMulti1_p2, out, sizeof(tupleMulti1_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti1_p0_0));
  assert(0 == memcmp(tupleMulti1_p0_0, out, sizeof(tupleMulti1_p0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti1_p0_1));
  assert(0 == memcmp(tupleMulti1_p0_1, out, sizeof(tupleMulti1_p0_1)));
  paramInfo.typeIdx = 0;
  info.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti1_p1_0));
  assert(0 == memcmp(tupleMulti1_p1_0, out, sizeof(tupleMulti1_p1_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti1_abi, ARRAY_SIZE(tupleMulti1_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti1_p1_1));
  assert(0 == memcmp(tupleMulti1_p1_1, out, sizeof(tupleMulti1_p1_1)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti2(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti2_encoded;
  size_t inSz = sizeof(tupleMulti2_encoded);
  printf("(Tuple) tupleMulti2_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi)));

  // Non-tuple params
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p2));
  assert(0 == memcmp(tupleMulti2_p2, out, sizeof(tupleMulti2_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p0_t0_p0));
  assert(0 == memcmp(tupleMulti2_p0_t0_p0, out, sizeof(tupleMulti2_p0_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p0_t0_p1_0));
  assert(0 == memcmp(tupleMulti2_p0_t0_p1_0, out, sizeof(tupleMulti2_p0_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p0_t1_p0));
  assert(0 == memcmp(tupleMulti2_p0_t1_p0, out, sizeof(tupleMulti2_p0_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p0_t1_p1_0));
  assert(0 == memcmp(tupleMulti2_p0_t1_p1_0, out, sizeof(tupleMulti2_p0_t1_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 0;
  info.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p1_0));
  assert(0 == memcmp(tupleMulti2_p1_0, out, sizeof(tupleMulti2_p1_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti2_abi, ARRAY_SIZE(tupleMulti2_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti2_p1_1));
  assert(0 == memcmp(tupleMulti2_p1_1, out, sizeof(tupleMulti2_p1_1)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti3(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti3_encoded;
  size_t inSz = sizeof(tupleMulti3_encoded);
  printf("(Tuple) tupleMulti3_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi)));

  // Non-tuple params
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p2));
  assert(0 == memcmp(tupleMulti3_p2, out, sizeof(tupleMulti3_p2)));
  memset(out, 0, outSz);

  // Tuple types
  info.typeIdx = 0;
  ABISelector_t paramInfo = { .typeIdx = 0 };
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p0_t0_p0));
  assert(0 == memcmp(tupleMulti3_p0_t0_p0, out, sizeof(tupleMulti3_p0_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p0_t0_p1_0));
  assert(0 == memcmp(tupleMulti3_p0_t0_p1_0, out, sizeof(tupleMulti3_p0_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p0_t1_p0));
  assert(0 == memcmp(tupleMulti3_p0_t1_p0, out, sizeof(tupleMulti3_p0_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p0_t1_p1_0));
  assert(0 == memcmp(tupleMulti3_p0_t1_p1_0, out, sizeof(tupleMulti3_p0_t1_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 0;
  info.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p1_0));
  assert(0 == memcmp(tupleMulti3_p1_0, out, sizeof(tupleMulti3_p1_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti3_abi, ARRAY_SIZE(tupleMulti3_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti3_p1_1));
  assert(0 == memcmp(tupleMulti3_p1_1, out, sizeof(tupleMulti3_p1_1)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti4(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti4_encoded;
  size_t inSz = sizeof(tupleMulti4_encoded);
  printf("(Tuple) tupleMulti4_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p1));
  assert(0 == memcmp(tupleMulti4_p1, out, sizeof(tupleMulti4_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p0_0));
  assert(0 == memcmp(tupleMulti4_p0_0, out, sizeof(tupleMulti4_p0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p0_1));
  assert(0 == memcmp(tupleMulti4_p0_1, out, sizeof(tupleMulti4_p0_1)));

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p2_t0_p0));
  assert(0 == memcmp(tupleMulti4_p2_t0_p0, out, sizeof(tupleMulti4_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti4_p2_t0_p1_0, out, sizeof(tupleMulti4_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p2_t1_p0));
  assert(0 == memcmp(tupleMulti4_p2_t1_p0, out, sizeof(tupleMulti4_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti4_abi, ARRAY_SIZE(tupleMulti4_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti4_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti4_p2_t1_p1_0, out, sizeof(tupleMulti4_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti5(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti5_encoded;
  size_t inSz = sizeof(tupleMulti5_encoded);
  printf("(Tuple) tupleMulti5_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p1));
  assert(0 == memcmp(tupleMulti5_p1, out, sizeof(tupleMulti5_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p0_0));
  assert(0 == memcmp(tupleMulti5_p0_0, out, sizeof(tupleMulti5_p0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p0_1));
  assert(0 == memcmp(tupleMulti5_p0_1, out, sizeof(tupleMulti5_p0_1)));

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p2_t0_p0));
  assert(0 == memcmp(tupleMulti5_p2_t0_p0, out, sizeof(tupleMulti5_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti5_p2_t0_p1_0, out, sizeof(tupleMulti5_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p2_t1_p0));
  assert(0 == memcmp(tupleMulti5_p2_t1_p0, out, sizeof(tupleMulti5_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti5_abi, ARRAY_SIZE(tupleMulti5_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti5_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti5_p2_t1_p1_0, out, sizeof(tupleMulti5_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti6(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti6_encoded;
  size_t inSz = sizeof(tupleMulti6_encoded);
  printf("(Tuple) tupleMulti6_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p1));
  assert(0 == memcmp(tupleMulti6_p1, out, sizeof(tupleMulti6_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p0_0));
  assert(0 == memcmp(tupleMulti6_p0_0, out, sizeof(tupleMulti6_p0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p0_1_0));
  assert(0 == memcmp(tupleMulti6_p0_1_0, out, sizeof(tupleMulti6_p0_1_0)));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p0_1_1));
  assert(0 == memcmp(tupleMulti6_p0_1_1, out, sizeof(tupleMulti6_p0_1_1)));
  paramInfo.arrIdx = 0;

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p2_t0_p0));
  assert(0 == memcmp(tupleMulti6_p2_t0_p0, out, sizeof(tupleMulti6_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti6_p2_t0_p1_0, out, sizeof(tupleMulti6_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p2_t1_p0));
  assert(0 == memcmp(tupleMulti6_p2_t1_p0, out, sizeof(tupleMulti6_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti6_abi, ARRAY_SIZE(tupleMulti6_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti6_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti6_p2_t1_p1_0, out, sizeof(tupleMulti6_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti7(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti7_encoded;
  size_t inSz = sizeof(tupleMulti7_encoded);
  printf("(Tuple) tupleMulti7_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p1));
  assert(0 == memcmp(tupleMulti7_p1, out, sizeof(tupleMulti7_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p0_0));
  assert(0 == memcmp(tupleMulti7_p0_0, out, sizeof(tupleMulti7_p0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p0_1_0));
  assert(0 == memcmp(tupleMulti7_p0_1_0, out, sizeof(tupleMulti7_p0_1_0)));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p0_1_1));
  assert(0 == memcmp(tupleMulti7_p0_1_1, out, sizeof(tupleMulti7_p0_1_1)));
  paramInfo.arrIdx = 0;

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p2_t0_p0));
  assert(0 == memcmp(tupleMulti7_p2_t0_p0, out, sizeof(tupleMulti7_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti7_p2_t0_p1_0, out, sizeof(tupleMulti7_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p2_t1_p0));
  assert(0 == memcmp(tupleMulti7_p2_t1_p0, out, sizeof(tupleMulti7_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti7_abi, ARRAY_SIZE(tupleMulti7_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti7_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti7_p2_t1_p1_0, out, sizeof(tupleMulti7_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti8(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti8_encoded;
  size_t inSz = sizeof(tupleMulti8_encoded);
  printf("(Tuple) tupleMulti8_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p1));
  assert(0 == memcmp(tupleMulti8_p1, out, sizeof(tupleMulti8_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p0_t0_0));
  assert(0 == memcmp(tupleMulti8_p0_t0_0, out, sizeof(tupleMulti8_p0_t0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p0_t0_1_0));
  assert(0 == memcmp(tupleMulti8_p0_t0_1_0, out, sizeof(tupleMulti8_p0_t0_1_0)));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p0_t0_1_1));
  assert(0 == memcmp(tupleMulti8_p0_t0_1_1, out, sizeof(tupleMulti8_p0_t0_1_1)));
  paramInfo.arrIdx = 0;

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p2_t0_p0));
  assert(0 == memcmp(tupleMulti8_p2_t0_p0, out, sizeof(tupleMulti8_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti8_p2_t0_p1_0, out, sizeof(tupleMulti8_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p2_t1_p0));
  assert(0 == memcmp(tupleMulti8_p2_t1_p0, out, sizeof(tupleMulti8_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti8_abi, ARRAY_SIZE(tupleMulti8_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti8_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti8_p2_t1_p1_0, out, sizeof(tupleMulti8_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti9(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  uint8_t * in = tupleMulti9_encoded;
  size_t inSz = sizeof(tupleMulti9_encoded);
  printf("(Tuple) tupleMulti9_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi)));

  // Non-tuple params
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                            info, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p1));
  assert(0 == memcmp(tupleMulti9_p1, out, sizeof(tupleMulti9_p1)));
  memset(out, 0, outSz);

  ABISelector_t paramInfo = { .typeIdx = 0 };
  // Tuple types
  info.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p0_t0_0));
  assert(0 == memcmp(tupleMulti9_p0_t0_0, out, sizeof(tupleMulti9_p0_t0_0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p0_t0_1_0));
  assert(0 == memcmp(tupleMulti9_p0_t0_1_0, out, sizeof(tupleMulti9_p0_t0_1_0)));
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p0_t0_1_1));
  assert(0 == memcmp(tupleMulti9_p0_t0_1_1, out, sizeof(tupleMulti9_p0_t0_1_1)));
  paramInfo.arrIdx = 0;

  info.typeIdx = 2;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p2_t0_p0));
  assert(0 == memcmp(tupleMulti9_p2_t0_p0, out, sizeof(tupleMulti9_p2_t0_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p2_t0_p1_0));
  assert(0 == memcmp(tupleMulti9_p2_t0_p1_0, out, sizeof(tupleMulti9_p2_t0_p1_0)));
  paramInfo.typeIdx = 0;
  info.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p2_t1_p0));
  assert(0 == memcmp(tupleMulti9_p2_t1_p0, out, sizeof(tupleMulti9_p2_t1_p0)));
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti9_abi, ARRAY_SIZE(tupleMulti9_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti9_p2_t1_p1_0));
  assert(0 == memcmp(tupleMulti9_p2_t1_p1_0, out, sizeof(tupleMulti9_p2_t1_p1_0)));
  
  printf("passed.\n\r");
}

static inline void test_tupleMulti10(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  ABISelector_t paramInfo = { .typeIdx = 0 };
  uint8_t * in = tupleMulti10_encoded;
  size_t inSz = sizeof(tupleMulti10_encoded);
  printf("(Tuple) tupleMulti10_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti10_abi, ARRAY_SIZE(tupleMulti10_abi)));

  decSz = abi_decode_tuple_param( out, outSz, tupleMulti10_abi, ARRAY_SIZE(tupleMulti10_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti10_p0_t0_0_0));
  assert(0 == memcmp(tupleMulti10_p0_t0_0_0, out, sizeof(tupleMulti10_p0_t0_0_0)));
  memset(out, 0, outSz);

  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti10_abi, ARRAY_SIZE(tupleMulti10_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti10_p0_t0_1));
  assert(0 == memcmp(tupleMulti10_p0_t0_1, out, sizeof(tupleMulti10_p0_t0_1)));
  memset(out, 0, outSz);

  paramInfo.typeIdx = 2;
  assert(6 == abi_get_tuple_param_array_sz(tupleMulti10_abi, ARRAY_SIZE(tupleMulti10_abi), info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti10_abi, ARRAY_SIZE(tupleMulti10_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti10_p0_t0_2_0));
  assert(0 == memcmp(tupleMulti10_p0_t0_2_0, out, sizeof(tupleMulti10_p0_t0_2_0)));
  memset(out, 0, outSz);


  printf("passed.\n\r");
};

static inline void test_tupleMulti11(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  ABISelector_t paramInfo = { .typeIdx = 0 };
  uint8_t * in = tupleMulti11_encoded;
  size_t inSz = sizeof(tupleMulti11_encoded);
  printf("(Tuple) tupleMulti11_encoded...");
  size_t decSz;

  assert(true == abi_is_valid_schema(tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi)));
  assert(7 == abi_get_tuple_param_array_sz(tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p0_0_0));
  assert(0 == memcmp(tupleMulti11_p0_0_0, out, sizeof(tupleMulti11_p0_0_0)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p0_0_1));
  assert(0 == memcmp(tupleMulti11_p0_0_1, out, sizeof(tupleMulti11_p0_0_1)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p0_0_6));
  assert(0 == memcmp(tupleMulti11_p0_0_6, out, sizeof(tupleMulti11_p0_0_6)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p0_1));
  assert(0 == memcmp(tupleMulti11_p0_1, out, sizeof(tupleMulti11_p0_1)));
  memset(out, 0, outSz);
  
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p1));
  assert(0 == memcmp(tupleMulti11_p1, out, sizeof(tupleMulti11_p1)));
  memset(out, 0, outSz);

  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p2));
  assert(0 == memcmp(tupleMulti11_p2, out, sizeof(tupleMulti11_p2)));
  memset(out, 0, outSz);

  info.typeIdx = 3;
  decSz = abi_decode_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p3_0));
  assert(0 == memcmp(tupleMulti11_p3_0, out, sizeof(tupleMulti11_p3_0)));
  memset(out, 0, outSz);
  info.arrIdx = 3;
  decSz = abi_decode_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p3_3));
  assert(0 == memcmp(tupleMulti11_p3_3, out, sizeof(tupleMulti11_p3_3)));
  memset(out, 0, outSz);
  info.arrIdx = 0;

  info.typeIdx = 4;
  decSz = abi_decode_param( out, outSz, tupleMulti11_abi, ARRAY_SIZE(tupleMulti11_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti11_p4));
  assert(0 == memcmp(tupleMulti11_p4, out, sizeof(tupleMulti11_p4)));
  memset(out, 0, outSz);


  printf("passed.\n\r");
};

static inline void test_tupleMulti12(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  ABISelector_t paramInfo = { .typeIdx = 0 };
  uint8_t * in = tupleMulti12_encoded;
  size_t inSz = sizeof(tupleMulti12_encoded);
  printf("(Tuple) tupleMulti12_encoded...");
  size_t decSz;
  assert(true == abi_is_valid_schema(tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi)));
  assert(6 == abi_get_tuple_param_array_sz(tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti12_p0_0));
  assert(0 == memcmp(tupleMulti12_p0_0, out, sizeof(tupleMulti12_p0_0)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti12_p0_5));
  assert(0 == memcmp(tupleMulti12_p0_5, out, sizeof(tupleMulti12_p0_5)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 0;

  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti12_p1));
  assert(0 == memcmp(tupleMulti12_p1, out, sizeof(tupleMulti12_p1)));
  memset(out, 0, outSz);
  
  paramInfo.typeIdx = 2;
  assert(9 == abi_get_tuple_param_array_sz(tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), info, paramInfo, in, inSz));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti12_p2_0));
  assert(0 == memcmp(tupleMulti12_p2_0, out, sizeof(tupleMulti12_p2_0)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 8;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti12_abi, ARRAY_SIZE(tupleMulti12_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti12_p2_8));
  assert(0 == memcmp(tupleMulti12_p2_8, out, sizeof(tupleMulti12_p2_8)));
  memset(out, 0, outSz);

  printf("passed.\n\r");
};

static inline void test_tupleMulti13(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  ABISelector_t paramInfo = { .typeIdx = 0 };
  uint8_t * in = tupleMulti13_encoded;
  size_t inSz = sizeof(tupleMulti13_encoded);
  printf("(Tuple) tupleMulti13_encoded...");
  size_t decSz;
  assert(true == abi_is_valid_schema(tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi)));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti13_p0_0));
  assert(0 == memcmp(tupleMulti13_p0_0, out, sizeof(tupleMulti13_p0_0)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 5;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti13_p0_5));
  assert(0 == memcmp(tupleMulti13_p0_5, out, sizeof(tupleMulti13_p0_5)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 0;

  info.typeIdx = 1;
  paramInfo.typeIdx = 0;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti13_p1_0));
  assert(0 == memcmp(tupleMulti13_p1_0, out, sizeof(tupleMulti13_p1_0)));
  memset(out, 0, outSz);
  
  paramInfo.typeIdx = 1;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti13_p1_1));
  assert(0 == memcmp(tupleMulti13_p1_1, out, sizeof(tupleMulti13_p1_1)));
  memset(out, 0, outSz);

  info.typeIdx = 2;
  decSz = abi_decode_param(out, outSz, tupleMulti13_abi, ARRAY_SIZE(tupleMulti13_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti13_p2));
  assert(0 == memcmp(tupleMulti13_p2, out, sizeof(tupleMulti13_p2)));
  memset(out, 0, outSz);
  

  printf("passed.\n\r");
};

static inline void test_tupleMulti14(uint8_t * out, size_t outSz) {
  ABISelector_t info = { .typeIdx = 0 };
  ABISelector_t paramInfo = { .typeIdx = 0 };
  uint8_t * in = tupleMulti14_encoded;
  size_t inSz = sizeof(tupleMulti14_encoded);
  printf("(Tuple) tupleMulti14_encoded...");
  size_t decSz;
  assert(true == abi_is_valid_schema(tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi)));
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p0_0));
  assert(0 == memcmp(tupleMulti14_p0_0, out, sizeof(tupleMulti14_p0_0)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 6;
  decSz = abi_decode_tuple_param( out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), 
                                  info, paramInfo, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p0_6));
  assert(0 == memcmp(tupleMulti14_p0_6, out, sizeof(tupleMulti14_p0_6)));
  memset(out, 0, outSz);
  paramInfo.arrIdx = 0;
  paramInfo.typeIdx = 0;
  info.typeIdx = 1;
  decSz = abi_decode_param( out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p1));
  assert(0 == memcmp(tupleMulti14_p1, out, sizeof(tupleMulti14_p1)));
  memset(out, 0, outSz);
  
  info.typeIdx = 2;
  decSz = abi_decode_param( out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p2));
  assert(0 == memcmp(tupleMulti14_p2, out, sizeof(tupleMulti14_p2)));
  memset(out, 0, outSz);
  
  info.typeIdx = 3;
  assert(9 == abi_get_array_sz(tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), info, in, inSz));
  decSz = abi_decode_param( out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p3_0));
  assert(0 == memcmp(tupleMulti14_p3_0, out, sizeof(tupleMulti14_p3_0)));
  memset(out, 0, outSz);

  info.arrIdx = 8;
  decSz = abi_decode_param(out, outSz, tupleMulti14_abi, ARRAY_SIZE(tupleMulti14_abi), info, in, inSz);
  assert(decSz == sizeof(tupleMulti14_p3_8));
  assert(0 == memcmp(tupleMulti14_p3_8, out, sizeof(tupleMulti14_p3_8)));
  memset(out, 0, outSz);

  printf("passed.\n\r");
};

static inline void test_enc(uint8_t * out, size_t outSz) {
  printf("Encoding...");
  size_t encSz = 0;
  encSz = abi_encode( out, outSz, enc_ex1_abi, ARRAY_SIZE(enc_ex1_abi), 
                      enc_ex1_offsets, enc_ex1_params, sizeof(enc_ex1_params));
  assert(sizeof(enc_ex1_encoded) == encSz);
  assert(0 == memcmp(out, enc_ex1_encoded, sizeof(enc_ex1_encoded)));
  memset(out, 0, outSz);

  encSz = abi_encode( out, outSz, enc_ex2_abi, ARRAY_SIZE(enc_ex2_abi), 
                      enc_ex2_offsets, enc_ex2_params, sizeof(enc_ex2_params));
  assert(sizeof(enc_ex2_encoded) == encSz);
  assert(0 == memcmp(out, enc_ex2_encoded, sizeof(enc_ex2_encoded)));
  memset(out, 0, outSz);

  encSz = abi_encode( out, outSz, enc_ex3_abi, ARRAY_SIZE(enc_ex3_abi), 
                      enc_ex3_offsets, enc_ex3_params, sizeof(enc_ex3_params));
  assert(sizeof(enc_ex3_encoded) == encSz);
  assert(0 == memcmp(out, enc_ex3_encoded, sizeof(enc_ex3_encoded)));
  memset(out, 0, outSz);

  encSz = abi_encode( out, outSz, enc_ex4_abi, ARRAY_SIZE(enc_ex4_abi), 
                      enc_ex4_offsets, enc_ex4_params, sizeof(enc_ex4_params));
  assert(sizeof(enc_ex4_encoded) == encSz);
  assert(0 == memcmp(out, enc_ex4_encoded, sizeof(enc_ex4_encoded)));
  memset(out, 0, outSz);

  encSz = abi_encode( out, outSz, enc_ex5_abi, ARRAY_SIZE(enc_ex5_abi), 
                      enc_ex5_offsets, enc_ex5_params, sizeof(enc_ex5_params));
  assert(sizeof(enc_ex5_encoded) == encSz);
  assert(0 == memcmp(out, enc_ex5_encoded, sizeof(enc_ex5_encoded)));
  memset(out, 0, outSz);

  printf("passed.\n\r");
}

static inline void test_failures(uint8_t * out, size_t outSz) {
  printf("Testing failures...");
  // Confirm bad schemas are rejected
  ABI_t fail_ex1_abi[1] = {
    { .type = 103820, },
  };
  assert(false == abi_is_valid_schema(fail_ex1_abi, ARRAY_SIZE(fail_ex1_abi)));
  // Test shorter `inSz` values
  uint8_t * in = ex1_encoded+4;
  size_t inSz = sizeof(ex1_encoded) - 4;
  ABISelector_t info = { .typeIdx = 0 };
  // We need inSz to be at least 32 bytes to allow for extracting of the first word
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, ABI_WORD_SZ) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, ABI_WORD_SZ-1) == -1);
  // We need inSz to be at least 64 bytes to allow for extracting of the second word
  info.typeIdx = 1;
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, 2*ABI_WORD_SZ) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, 2*ABI_WORD_SZ-1) == -1);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz) > 0);
  assert(abi_decode_param(out, outSz, ex1_abi, ARRAY_SIZE(ex1_abi), info, in, inSz-1) == -1);
  memset(out, 0, outSz);
  // Make sure we cannot specify an index out of range of a fixed size array
  in = ex5_encoded+4;
  inSz = sizeof(ex5_encoded) - 4;
  info.typeIdx = 0;
  info.arrIdx = 2;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) > 0);
  info.arrIdx = 3;
  assert(abi_decode_param(out, outSz, ex5_abi, ARRAY_SIZE(ex5_abi), info, in, inSz) == -1);
  printf("passed.\n\r");
}

int main() {
  printf("=============================\n\r");
  printf(" RUNNING ABI TESTS...\n\r");
  printf("=============================\n\r");
  uint8_t out[500] = {0};
  test_ex1(out, sizeof(out));
  test_ex2(out, sizeof(out));
  test_ex3(out, sizeof(out));
  test_ex4(out, sizeof(out));
  test_ex5(out, sizeof(out));
  test_ex6(out, sizeof(out));
  test_ex7(out, sizeof(out));
  test_ex8(out, sizeof(out));
  test_ex9(out, sizeof(out));
  test_ex10(out, sizeof(out));
  test_ex11(out, sizeof(out));
  test_ex12(out, sizeof(out));
  test_ex13(out, sizeof(out));
  test_ex14(out, sizeof(out));
  test_fillOrder(out, sizeof(out));
  test_marketSellOrders(out, sizeof(out));
  test_tupleElementary(out, sizeof(out));
  test_tupleFixedArray0(out, sizeof(out));
  test_tupleFixedArray1(out, sizeof(out));
  test_tupleVarArray0(out, sizeof(out));
  test_tupleVarArray1(out, sizeof(out));
  test_tupleVarArray2(out, sizeof(out));
  test_tupleVarArray3(out, sizeof(out));
  test_tupleVarArray4(out, sizeof(out));
  test_tupleMulti1(out, sizeof(out));
  test_tupleMulti2(out, sizeof(out));
  test_tupleMulti3(out, sizeof(out));
  test_tupleMulti4(out, sizeof(out));
  test_tupleMulti5(out, sizeof(out));
  test_tupleMulti6(out, sizeof(out));
  test_tupleMulti7(out, sizeof(out));
  test_tupleMulti8(out, sizeof(out));
  test_tupleMulti9(out, sizeof(out));
  test_tupleMulti10(out, sizeof(out));
  test_tupleMulti11(out, sizeof(out));
  test_tupleMulti12(out, sizeof(out));
  test_tupleMulti13(out, sizeof(out));
  test_tupleMulti14(out, sizeof(out));
  test_enc(out, sizeof(out));
  test_failures(out, sizeof(out));

  printf("=============================\n\r");
  printf(" ALL ABI TESTS PASSING!\n\r");
  printf("=============================\n\r");
  
  return 0;
}
