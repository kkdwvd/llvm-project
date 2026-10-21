//===-- udivmodti4.c - Implement __udivmodti4 for BPF ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements __udivmodti4 for BPF: the generic routine, with its
// loops bounded for the kernel's verifier.
//
// A BPF program runs under the verifier, which walks every path of a loop
// and admits a loop it cannot bound only when it can count the iterations or
// an iteration can leave it through a may_goto instruction, which the kernel
// takes after a budget of iterations. The corrections of a quotient digit
// run at most twice, which the generic routine's while loops do not say and
// the verifier cannot see, so they are loops of two here; the shift-subtract
// division runs as many times as the divisor's leading zeros exceed the
// dividend's, so each of its iterations asks may_goto first.
//
//===----------------------------------------------------------------------===//

#include "../int_lib.h"

#ifdef CRT_HAS_128BIT

// The kernel's can_loop: true until the verifier's budget is spent.
#define BPF_CAN_LOOP                                                           \
  ({                                                                           \
    __label__ l_break, l_continue;                                             \
    int ret = 1;                                                               \
    __asm__ volatile goto("may_goto %l[l_break]" : : : : l_break);             \
    goto l_continue;                                                           \
  l_break:                                                                     \
    ret = 0;                                                                   \
  l_continue:;                                                                 \
    ret;                                                                       \
  })

static inline du_int udiv128by64to64(du_int u1, du_int u0, du_int v,
                                     du_int *r) {
  const unsigned n_udword_bits = sizeof(du_int) * CHAR_BIT;
  const du_int b = (1ULL << (n_udword_bits / 2)); // Number base (32 bits)
  du_int un1, un0;                                // Norm. dividend LSD's
  du_int vn1, vn0;                                // Norm. divisor digits
  du_int q1, q0;                                  // Quotient digits
  du_int un64, un21, un10;                        // Dividend digit pairs
  du_int rhat;                                    // A remainder
  si_int s;                                       // Shift amount for normalization
  s = __builtin_clzll(v);
  if (s > 0) {
    v = v << s;
    un64 = (u1 << s) | (u0 >> (n_udword_bits - s));
    un10 = u0 << s; // Shift dividend left
  } else {
    un64 = u1;
    un10 = u0;
  }

  vn1 = v >> (n_udword_bits / 2);
  vn0 = v & 0xFFFFFFFF;

  un1 = un10 >> (n_udword_bits / 2);
  un0 = un10 & 0xFFFFFFFF;

  q1 = un64 / vn1;
  rhat = un64 - q1 * vn1;

  // Knuth's correction of a quotient digit runs at most twice (The Art of
  // Computer Programming, 4.3.1, algorithm D, step D3): a bound the verifier
  // can walk, where it cannot see that the digit stays positive.
  for (int i = 0; i < 2; ++i) {
    if (!(q1 >= b || q1 * vn0 > b * rhat + un1))
      break;
    q1 = q1 - 1;
    rhat = rhat + vn1;
    if (rhat >= b)
      break;
  }

  un21 = un64 * b + un1 - q1 * v;

  q0 = un21 / vn1;
  rhat = un21 - q0 * vn1;

  for (int i = 0; i < 2; ++i) {
    if (!(q0 >= b || q0 * vn0 > b * rhat + un0))
      break;
    q0 = q0 - 1;
    rhat = rhat + vn1;
    if (rhat >= b)
      break;
  }

  *r = (un21 * b + un0 - q0 * v) >> s;
  return q1 * b + q0;
}

COMPILER_RT_ABI tu_int __udivmodti4(tu_int a, tu_int b, tu_int *rem) {
  const unsigned n_utword_bits = sizeof(tu_int) * CHAR_BIT;
  utwords dividend;
  dividend.all = a;
  utwords divisor;
  divisor.all = b;
  utwords quotient;
  utwords remainder;
  if (divisor.all > dividend.all) {
    if (rem)
      *rem = dividend.all;
    return 0;
  }
  // When the divisor fits in 64 bits, we can use an optimized path.
  if (divisor.s.high == 0) {
    remainder.s.high = 0;
    if (dividend.s.high < divisor.s.low) {
      // The result fits in 64 bits.
      quotient.s.low = udiv128by64to64(dividend.s.high, dividend.s.low,
                                       divisor.s.low, &remainder.s.low);
      quotient.s.high = 0;
    } else {
      // First, divide with the high part to get the remainder in dividend.s.high.
      // After that dividend.s.high < divisor.s.low.
      quotient.s.high = dividend.s.high / divisor.s.low;
      dividend.s.high = dividend.s.high % divisor.s.low;
      quotient.s.low = udiv128by64to64(dividend.s.high, dividend.s.low,
                                       divisor.s.low, &remainder.s.low);
    }
    if (rem)
      *rem = remainder.all;
    return quotient.all;
  }
  // 0 <= shift <= 63.
  si_int shift =
      __builtin_clzll(divisor.s.high) - __builtin_clzll(dividend.s.high);
  divisor.all <<= shift;
  quotient.s.high = 0;
  quotient.s.low = 0;
  for (; shift >= 0 && BPF_CAN_LOOP; --shift) {
    quotient.s.low <<= 1;
    // Branch free version of.
    // if (dividend.all >= divisor.all)
    // {
    //    dividend.all -= divisor.all;
    //    carry = 1;
    // }
    const ti_int s =
        (ti_int)(divisor.all - dividend.all - 1) >> (n_utword_bits - 1);
    quotient.s.low |= s & 1;
    dividend.all -= divisor.all & s;
    divisor.all >>= 1;
  }
  if (rem)
    *rem = dividend.all;
  return quotient.all;
}

#endif // CRT_HAS_128BIT
