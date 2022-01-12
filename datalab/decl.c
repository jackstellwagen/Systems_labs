#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#define TMin LONG_MIN
#define TMax LONG_MAX

#include "btest.h"
#include "bits.h"

test_rec test_set[] = {
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
//1
 {"bitMatch", (funct_t) bitMatch, (funct_t) test_bitMatch, 2, "& | ~", 14, 1,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
//2
 {"allOddBits", (funct_t) allOddBits, (funct_t) test_allOddBits, 1,
    "! ~ & ^ | + << >>", 14, 2,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
 {"leastBitPos", (funct_t) leastBitPos, (funct_t) test_leastBitPos, 1, "! ~ & ^ | + << >>", 6, 2,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
 {"copyLSB", (funct_t) copyLSB, (funct_t) test_copyLSB, 1,
    "! ~ & ^ | + << >>", 5, 2,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
//3
 {"conditional", (funct_t) conditional, (funct_t) test_conditional, 3, "! ~ & ^ | << >>", 16, 3,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
 {"bitMask", (funct_t) bitMask, (funct_t) test_bitMask, 2,
    "! ~ & ^ | + << >>", 16, 3,
  {{0, 63},{0,63},{TMin,TMax}}},
 {"isLess", (funct_t) isLess, (funct_t) test_isLess, 2,
    "! ~ & ^ | + << >>", 24, 3,
  {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
//4
 {"trueThreeFourths", (funct_t) trueThreeFourths, (funct_t) test_trueThreeFourths, 1,
    "! ~ & ^ | + << >>", 20, 4,
  {{TMin,TMax},{TMin,TMax},{TMin,TMax}}},
{"isPalindrome", (funct_t) isPalindrome, (funct_t) test_isPalindrome, 1, "! ~ & ^ | + << >>", 70, 4,
 {{TMin, TMax},{TMin,TMax},{TMin,TMax}}},
//float
 {"floatNegate", (funct_t) floatNegate, (funct_t) test_floatNegate, 1,
    "$", 10, 2,
     {{1, 1},{1,1},{1,1}}},
 {"floatIsEqual", (funct_t) floatIsEqual, (funct_t) test_floatIsEqual, 2,
    "$", 25, 2,
     {{1, 1},{1,1},{1,1}}},
 {"floatIsLess", (funct_t) floatIsLess, (funct_t) test_floatIsLess, 2,
    "$", 30, 3,
     {{1, 1},{1,1},{1,1}}},
 {"floatScale2", (funct_t) floatScale2, (funct_t) test_floatScale2, 1,
    "$", 30, 4,
     {{1, 1},{1,1},{1,1}}},
 {"floatUnsigned2Float", (funct_t) floatUnsigned2Float, (funct_t) test_floatUnsigned2Float, 1,
    "$", 30, 4,
     {{1, 1},{1,1},{1,1}}},
  {"", NULL, NULL, 0, "", 0, 0,
   {{0, 0},{0,0},{0,0}}}
};
