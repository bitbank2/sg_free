//
// 64-bit ARMv8 optimized code for SmartGear
// Copyright (c) 2014 BitBank Software, Inc.
// Written by Larry Bank
//
// Change log
//
// 11/17/14 - first version
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// AARCH64 calling convention
// X0-X7 arguments and return value
// X8 indirect result (struct) location
// X9-X15 temporary registers
// X16-X17 intro-call-use registers (PLT, linker)
// X18 platform specific use (TLS)
// X19-X28 callee-saved registers
// X29 frame pointer
// X30 link register (LR)
// SP stack pointer and zero (XZR)
//
  .global  ARMDrawScaled32_0

//
// Draw one line of scaled 32bpp image data
// call from C as ARMDrawScaled32_0(pSrc, pDest, iWidth, iscale)
// needs to draw at least 4 pixels
//
//
// Draw one line of scaled 32bpp image data
//                                   x0     x1     x2      x3
// call from C as ARMDrawScaled32_0(pSrc, pDest, iWidth, iscale)
// needs to draw at least 4 pixels
//
 .text
ARMDrawScaled32_0:
   mov x4,#0            // sum starts at 0
   lsr x11,x2,#2		// number of 4-pixel groups to draw
   mov x10,#0xfffffffffffffffc	// mask for clearing lower 2 bits of address
draw32_0_loop:
   and x9,x10,x4,LSR #6   // get the scaled sum (*4 to address longs)
   add x4,x4,x3       // add scale factor
   ldr w5,[x0,x9]	    // get source pixel 1
   and x9,x10,x4,LSR #6
   add x4,x4,x3
   ldr w6,[x0,x9]		// get source pixel 2
   and x9,x10,x4,LSR #6
   add x4,x4,x3
   ldr w7,[x0,x9]		// get source pixel 3
   and x9,x10,x4,LSR #6
   add x4,x4,x3
   ldr w8,[x0,x9]		// get source pixel 4
   subs x11,x11,#1     // decrement count
   orr x12,x5,x6,LSL #32	// combine 2 32-bit registers into 1 64-bit
   orr x13,x7,x8,LSL #32
   stp x12,x13,[x1],#16    // store all 4 pixels to destination
   bne draw32_0_loop
// see if there are any remaining pixels
   ands x2,x2,#3
   beq draw32_0_exit
draw32_0_loop_2:
   and   x9,x10,x4,LSR #6   // get the scaled sum (*4 to address longs)
   add   x4,x4,x3       // add scale factor
   ldr   w5,[x0,x9]     // get the source pixel 1
   subs  x2,x2,#1        // decrement count
   str   w5,[x1],#4      // store to destination
   bne   draw32_0_loop_2
draw32_0_exit:
   ret

  .end    
