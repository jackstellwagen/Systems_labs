/*
 * CS:APP Data Lab
 *
 * <Please put your name and userid here>
 *
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  long Funct(long arg1, long arg2, ...) {
      /* brief description of how your implementation works */
      long var1 = Expr1;
      ...
      long varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. (Long) integer constants 0 through 255 (0xFFL), inclusive. You are
      not allowed to use big constants such as 0xffffffffL.
  3. Function arguments and local variables (no global variables).
  4. Local variables of type int and long
  5. Unary integer operations ! ~
     - Their arguments can have types int or long
     - Note that ! always returns int, even if the argument is long
  6. Binary integer operations & ^ | + << >>
     - Their arguments can have types int or long
  7. Casting from int to long and from long to int
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting other than between int and long.
  7. Use any data type other than int or long.  This implies that you
     cannot use arrays, structs, or unions.
 
  You may assume that your machine:
  1. Uses 2s complement representations for int and long.
  2. Data type int is 32 bits, long is 64.
  3. Performs right shifts arithmetically.
  4. Has unpredictable behavior when shifting if the shift amount
     is less than 0 or greater than 31 (int) or 63 (long)

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 63
   */
  long pow2plus1(long x) {
     /* exploit ability of shifts to compute powers of 2 */
     /* Note that the 'L' indicates a long constant */
     return (1L << x) + 1L;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 63
   */
  long pow2plus4(long x) {
     /* exploit ability of shifts to compute powers of 2 */
     long result = (1L << x);
     result += 4L;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implement floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants. You can use any arithmetic,
logical, or comparison operations on int or unsigned data.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operations (integer, logical,
     or comparison) that you are allowed to use for your implementation
     of the function.  The max operator count is checked by dlc.
     Note that assignment ('=') is not counted; you may use as many of
     these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */

#endif
/* Copyright (C) 1991-2012 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */
/* This header is separate from features.h so that the compiler can
   include it implicitly at the start of every compilation.  It must
   not itself include <features.h> or any other header that includes
   <features.h> because the implicit include comes before any feature
   test macros that may be defined in a source file before it first
   explicitly includes a system header.  GCC knows the name of this
   header in order to preinclude it.  */
/* We do support the IEC 559 math functionality, real and complex.  */
/* wchar_t uses ISO/IEC 10646 (2nd ed., published 2011-03-15) /
   Unicode 6.0.  */
/* We do not support C11 <threads.h>.  */
// 1
/*
 * bitMatch - Create mask indicating which bits in x match those in y
 *            using only ~ and &
 *   Example: bitMatch(0x7L, 0xEL) = 0xFFFFFFFFFFFFFFF6L
 *   Legal ops: ~ &
 *   Max ops: 14
 *   Rating: 1
 */
long bitMatch(long x, long y) {
    return ~(~(x & y) & ~(~x & ~y));
}
// 2
/*
 * allOddBits - return 1 if all odd-numbered bits in word set to 1
 *   where bits are numbered from 0 (least significant) to 63 (most significant)
 *   Examples: allOddBits(0xFFFFFFFDFFFFFFFDL) = 0L,
 *             allOddBits(0xAAAAAAAAAAAAAAAAL) = 1L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 14
 *   Rating: 2
 */
long allOddBits(long x) {
    long odds = (0xAA << 8) + 0xAA;
    odds = (odds << 16) + odds;
    odds = (odds << 32) + odds;
    return !((x & odds) ^ odds);
}
/*
 * leastBitPos - return a mask that marks the position of the
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96L) = 0x20L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
long leastBitPos(long x) {
    return (~x + 1) & x;
}
/*
 * copyLSB - set all bits of result to least significant bit of x
 *   Examples:
 *     copyLSB(5L) = 0xFFFFFFFFFFFFFFFFL,
 *     copyLSB(6L) = 0x0000000000000000L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
long copyLSB(long x) {
    return (x << 63) >> 63;
}
// 3
/*
 * conditional - same as x ? y : z
 *   Example: conditional(2,4L,5L) = 4L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
long conditional(long x, long y, long z) {

    long condition_mask = (((long)!x) << 63) >> 63;

    return (~condition_mask & y) | (condition_mask & z);
}
/*
 * bitMask - Generate a mask consisting of all 1's
 *   between lowbit and highbit
 *   Examples: bitMask(5L,3L) = 0x38L
 *   Assume 0 <= lowbit < 64, and 0 <= highbit < 64
 *   If lowbit > highbit, then mask should be all 0's
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
long bitMask(long highbit, long lowbit) {
    long all1s = ~0L;
    long h = ~((all1s << highbit) << 1);
    long l = all1s << lowbit;
    return l & h;
}
/*
 * isLess - if x < y  then return 1, else return 0
 *   Example: isLess(4L,5L) = 1L.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
long isLess(long x, long y) {
    long difference = (x + (~y + 1));

    long sameSign = ((x & y) | (~x & ~y));

    return ((sameSign & difference) | (x & ~y)) >> 63 & 1;
    // long xSign = ~(x >> 63) + 1 ;
    // long ySign = ~(y >>63) + 1 ;
    // long differenceSign = ~((x + (~y +1)) >> 63) + 1 ;

    // long sameSign = ((xSign & ySign ) | (~xSign & ~ySign)) & 1;

    // return (sameSign & differenceSign) | (xSign & 1 & ~ySign) ;

}
// 4
/*
 * trueThreeFourths - multiplies by 3/4 rounding toward 0,
 *   avoiding errors due to overflow
 *   Examples:
 *    trueThreeFourths(11L) = 8
 *    trueThreeFourths(-9L) = -6
 *    trueThreeFourths(4611686018427387904L) = 3458764513820540928L (no
 * overflow) Legal ops: ! ~ & ^ | + << >> Max ops: 20 Rating: 4
 */
long trueThreeFourths(long x) {
    long remainder = x & 3L;
    long totalRemainder = remainder + remainder + remainder;
    long missingPositive = totalRemainder >> 2;

    long missingNegative = ~((~totalRemainder + 1) >> 2) + 1;

    long sign = x >> 63;
    long n = x >> 2;

    return n + n + n + (~sign & missingPositive) + (sign & missingNegative);
}
/*
 * isPalindrome - Return 1 if bit pattern in x is equal to its mirror image
 *   Example: isPalindrome(0x6F0F0123c480F0F6L) = 1L
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 70
 *   Rating: 4
 */
long isPalindrome(long x) {
    long firstHalfMask = ~(~0L << 32);
    long half = x >> 32 & firstHalfMask;

    long swap1 = (0x55 << 8) | 0x55;
    // swap1 = (swap1 << 16) | swap1;

    long swap2 = (0x33 << 8) | 0x33;
    // swap2 = (swap2 << 16) | swap2;

    long swap3 = (0x0f << 8) | 0x0f;
    // swap3 = (swap3 << 16) | swap3;

    long swap4 = (0x00 << 8) | 0xff;
    // swap4 = (swap4 << 16) | swap4;

    long swap5 = (0xff << 8) | 0xff;

    swap1 = (swap1 << 16) | swap1;
    swap2 = (swap2 << 16) | swap2;
    swap3 = (swap3 << 16) | swap3;
    swap4 = (swap4 << 16) | swap4;

    half = ((swap1 & half) << 1) | ((swap1 & (half >> 1)));
    half = ((swap2 & half) << 2) | ((swap2 & (half >> 2)));
    half = ((swap3 & half) << 4) | ((swap3 & (half >> 4)));
    half = ((swap4 & half) << 8) | ((swap4 & (half >> 8)));
    half = ((swap5 & half) << 16) | ((swap5 & (half >> 16)));

    return !(half ^ (x & firstHalfMask));
}
// float
/*
 * floatNegate - Return bit-level equivalent of expression -f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   Rating: 2
 */
unsigned floatNegate(unsigned uf) {
    // unsigned exp_mask = 0x7fc00000;
    // if ((uf & exp_mask) == exp_mask && !(uf & 0x003fffff))
    //     return uf;
    // return uf ^ (1L <<31);

    unsigned exp_mask = 0x7f800000;
    if ((uf & exp_mask) == exp_mask && (uf & 0x007fffff))
        return uf;
    return uf ^ (1L << 31);
}
/*
 * floatIsEqual - Compute f == g for floating point arguments f and g.
 *   Both the arguments are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   If either argument is NaN, return 0.
 *   +0 and -0 are considered equal.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 25
 *   Rating: 2
 */
int floatIsEqual(unsigned uf, unsigned ug) {
    unsigned exp_mask = 0x7f800000;
    int ufNaN = (uf & exp_mask) == exp_mask && (uf & 0x007fffff);
    int ugNaN = (ug & exp_mask) == exp_mask && (ug & 0x007fffff);
    int remove_sign = ~(1 << 31);

    if (ufNaN || ugNaN)
        return 0;

    if (!(uf & remove_sign) && !(ug & remove_sign))
        return 1;

    return uf == ug;
}
/*
 * floatIsLess - Compute f < g for floating point arguments f and g.
 *   Both the arguments are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   If either argument is NaN, return 0.
 *   +0 and -0 are considered equal.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 3
 */
int floatIsLess(unsigned uf, unsigned ug) {

    unsigned exp_mask = 0x7f800000;
    int ufNaN = (uf & exp_mask) == exp_mask && (uf & 0x007fffff);
    int ugNaN = (ug & exp_mask) == exp_mask && (ug & 0x007fffff);

    int ufSign = (uf >> 31) & 1;
    int ugSign = (ug >> 31) & 1;

    int comparison = ((uf & 0x7fffffff)) < ((ug & 0x7fffffff));

    if (ufNaN | ugNaN)
        return 0;

    if (!(uf & 0x7fffffff) && !(ug & 0x7fffffff))
        return 0;
    if (uf == ug)
        return 0;

    if (!(ufSign | ugSign))
        return comparison;
    else if (ufSign & ugSign)
        return !comparison;

    return ufSign > ugSign;
}
/*
 * floatScale2 - Return bit-level equivalent of expression 2*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatScale2(unsigned uf) {
    unsigned exp_mask = 0x7f800000;
    int ufNaNorInf = (uf & exp_mask) == exp_mask;
    int carry = 0x00400000 & uf;

    int new = ((uf << 1) & 0x007fffff) | (uf & 0xff800000);
    

    if (ufNaNorInf)
        return uf;

    if (!(uf & 0x7fffffff))
        return uf;

    if (!(uf & exp_mask))
        return new + (carry << 1);

    new = uf + 0x00800000;
    if ((new & exp_mask) == exp_mask) return (new & 0xff800000);
    return new;
}
/*
 * floatUnsigned2Float - Return bit-level equivalent of expression (float) u
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned floatUnsigned2Float(unsigned u) {
    unsigned exp, guard, round, sticky, uShifted, fraction;
    

    int i = 31;
    
    if (!u) return u;

    while (!(u >> i) ) {
        i--;
    }

    exp = 127 + i;


    exp = exp << 23;

    uShifted = u << (31 - i);
    
    if (i < 23)
        return exp | (0x007fffff & (u << (23 - i)));

    guard = (uShifted << 23) & 0x80000000;
    round = (uShifted << 24) & 0x80000000;
    sticky = (uShifted << 25);

    fraction = (0x007fffff & (u >> (i - 23)));
    
    
    if ((guard && round) || (round && sticky)){
        fraction++;
        if ((fraction & 0x00800000)) exp = exp + 0x00800000;
        //return exp | ((fraction + 1) & 0x007fffff);
    }

    return exp | (fraction & 0x007fffff);
}
