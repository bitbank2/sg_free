@      TITLE("larrys_graphics")
@  ARM asm code for SmartGear
@ Copyright (c) 2000-2017 BitBank Software, Inc.
@ Written by Larry Bank
@
@ Change Log
@ 12/5/2008 - added PLD instructions to speed things up on XScale and Qualcomm MSM7200
@             these are ignored on OMAP and Samsung CPUs
@ 1/24/2009 - changed stretchblt ratio to 65536 for more accuracy
@ 4/23/2009 - added support for Treo 800w.  It has a pitch of 0x1000, but does
@             not have the "venetian blinds" mapping of the Motorola Q9h
@
@ This program is free software: you can redistribute it and/or modify
@ it under the terms of the GNU General Public License as published by
@ the Free Software Foundation, either version 3 of the License, or
@ (at your option) any later version.
@
@ This program is distributed in the hope that it will be useful,
@ but WITHOUT ANY WARRANTY; without even the implied warranty of
@ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
@ GNU General Public License for more details.
@
@ You should have received a copy of the GNU General Public License
@ along with this program.  If not, see <http://www.gnu.org/licenses/>.
@
@    EXPORT  ARMGFXCopy16
  .global  ARMDraw2X  
  .global  SNESDrawSprite  
  .global  ARMDrawTile  
  .global  ARMDrawTileOpaque  
  .global  ASMBLT150S  
  .global  ASMBLT100  
  .global  ASMBLT100_270  
  .global  ASMBLT100_90  
  .global  ASMBLT200HQ  
  .global  ASMBLT200  
  .global  ASMBLT200HQ_90  
  .global  ASMBLT200_90  
  .global  ASMBLT200_90a  
  .global  ASMBLT200_90b  
  .global  ASMBLT200_270  
  .global  ASMBLT300  
  .global  ASMBLT300HQ  
  .global  ASMBLT300_90  
  .global  ASMBLT300_270  
  .global  ASMBLT150S_V6  
  .global  ASMBLT75S  
  .global  ASMBLT50S  
  .global  ASMBLT150S270  
  .global  ARMBLTFTW  
  .global  ARMV6TEST  
  .global  ARMPERFTEST
  .global  ARMDrawCoinOpSprite16
  .global  ARMDrawCoinOpTile16
  .global  G88DrawCharOpaque

@
@ call from C as void void G88DrawCharOpaque(int sx,int sy,int iChar,int iColor,unsigned char *pData, int iPitch, unsigned char *pBitmap)
@ draw an 8x8 fully opaque character
@
G88DrawCharOpaque:
  stmfd sp!,{r4-r12,lr}
  ldr r4,[sp,#40]	@ source data
  ldr r5,[sp,#44]	@ pitch
  ldr r6,[sp,#48]	@ dest data
  add r4,r4,r2,LSL #6	@ point to correct source image (64 * iChar)
  mul r7,r1,r5		@ pDest += y * iPitch
  add r6,r6,r0,LSL #1	@ pDest += x*2
  add r6,r6,r7
  mov r0,#0xff		@ mask to extract pixels
  mov r14,#8		@ number of lines to do
  tst r6,#3			@ dword aligned address?
  bne g88drawslow
g88drawfast:
  ldmia r4!,{r7,r8}	@ read 8 source pixels
  and r9,r0,r7		@ first pixel
  and r10,r0,r7,LSR #8	@ second pixel
  add r9,r9,r3		@ + color
  add r10,r10,r3
  and r11,r0,r7,LSR #16	@ third pixel
  orr r9,r9,r10,LSL #16	@ combine first 2 pixels
  and r10,r0,r7,LSR #24	@ forth pixel
  add r11,r11,r3	@ + color
  add r10,r10,r3
  orr r10,r11,r10,LSL #16	@ combine second 2 pixels
  and r1,r0,r8		@ fifth pixel
  and r2,r0,r8,LSR #8	@ sixth pixel
  add r1,r1,r3		@ + color
  add r2,r2,r3
  and r12,r0,r8,LSR #16	@ seventh pixel
  orr r11,r1,r2,LSL #16	@ combine pixels 5+6
  and r1,r0,r8,LSR #24	@ eighth pixel
  add r12,r12,r3		@ + color
  add r1,r1,r3
  orr r12,r12,r1,LSL #16	@ combine pixels 7+8
  stmia r6,{r9-r12}		@ store 8 pixels
  add r6,r6,r5		@ pDest += iPitch
  subs r14,r14,#1		@ while (--y)
  bne g88drawfast
g88drawexit:
  ldmfd sp!,{r4-r12,pc}

@ need to draw it 1 pixel at a time
g88drawslow:
  ldmia r4!,{r7,r8}	@ read 8 source pixels
  and r9,r0,r7		@ first pixel
  and r10,r0,r7,LSR #8	@ second pixel
  add r9,r9,r3		@ + color
  add r10,r10,r3
  strh r9,[r6],#2
  strh r10,[r6],#2
  and r11,r0,r7,LSR #16	@ third pixel
  and r10,r0,r7,LSR #24	@ forth pixel
  add r11,r11,r3	@ + color
  add r10,r10,r3
  strh r11,[r6],#2
  strh r10,[r6],#2
  and r1,r0,r8		@ fifth pixel
  and r2,r0,r8,LSR #8	@ sixth pixel
  add r1,r1,r3		@ + color
  add r2,r2,r3
  strh r1,[r6],#2
  strh r2,[r6],#2
  and r12,r0,r8,LSR #16	@ seventh pixel
  and r1,r0,r8,LSR #24	@ eighth pixel
  add r12,r12,r3		@ + color
  add r1,r1,r3
  strh r12,[r6],#2
  strh r1,[r6],#2
  add r6,r6,r5		@ pDest += iPitch
  sub r6,r6,#16
  subs r14,r14,#1		@ while (--y)
  bne g88drawslow
  b g88drawexit

@
@ call from C as void ARMDrawCoinOpTile16(unsigned char *p, unsigned char *d, int xCount, int yCount, int iColor, int iCoinOpPitch, int iSize);
@
ARMDrawCoinOpTile16:
  stmfd sp!,{r4-r10}
  ldr r6,[sp,#28]	@ color offset
  ldr r7,[sp,#32]	@ dest pitch
  ldr r8,[sp,#36]	@ source pitch
drawcotile00:
  mov r10,r2	@ xcount
drawcotile01:
  ldrb r9,[r0],#1	@ source pixel
  subs r10,r10,#1	@ while (xcount--)
  add r9,r9,r6	@ c + color
  strh r9,[r1],#2	@ if !(ulTransMask & 1<<c)
  bne drawcotile01
@ prepare for next line
  add r0,r0,r8		@ p += iSize
  sub r0,r0,r2		@ -xCount
  add r1,r1,r7		@ d += iPitch
  sub r1,r1,r2,LSL #1	@ adjust for bytes written
  subs r3,r3,#1		@ while (ycount--)
  bne drawcotile00
  ldmfd sp!,{r4-r10}
  bx lr

@
@ call from C as void ARMDrawCoinOpSprite16(unsigned char *p, unsigned char *d, int xCount, int yCount, unsigned long ulTransMask, int iColor, int iCoinOpPitch, int iSize);
@
ARMDrawCoinOpSprite16:
  stmfd sp!,{r5-r11}
  ldr r5,[sp,#28]	@ transmask
  ldr r6,[sp,#32]	@ color offset
  ldr r7,[sp,#36]	@ dest pitch
  ldr r8,[sp,#40]	@ source pitch
  mov r11,#1		@ test bit
drawcospr00:
  mov r10,r2	@ xcount
drawcospr01:
  ldrb r9,[r0],#1	@ source pixel
  tst r5,r11,LSL r9	@ transparent color
  add r9,r9,r6	@ c + color
  streqh r9,[r1]	@ if !(ulTransMask & 1<<c)
  add r1,r1,#2
  subs r10,r10,#1	@ while (xcount--)
  bne drawcospr01
@ prepare for next line
  add r0,r0,r8		@ p += iSize
  sub r0,r0,r2		@ -xCount
  add r1,r1,r7		@ d += iPitch
  sub r1,r1,r2,LSL #1	@ adjust for bytes written
  subs r3,r3,#1		@ while (ycount--)
  bne drawcospr00
  ldmfd sp!,{r5-r11}
  bx lr

@
@ Structure offsets for ScaleStruct
@
.equ srcptr, 0 
.equ srcpitch, 4 
.equ srcwidth, 8 
.equ srcheight, 12 
.equ destptr, 16 
.equ destpitch, 20 
.equ destwidth, 24 
.equ destheight, 28 
.equ scalex, 32 
.equ scaley, 36 
.equ destx, 40 
.equ desty, 44 
.equ leftdestptr, 48 @ for drawing in the "right" direction
ARMV6TEST:   
    mrs r0, cpsr                @ check if we run in non-user mode
    and r0, r0, #15
    cmp r0, #15
    mov r0, #0
    bne user_mode
    mrc p15, 0, r0, c0, c0, 0    @ read main id cp15 register 0
user_mode:   
    tst r0,#0x900        @ if either bit is set, we have ARMv6 instruction set
    movne r0,#1            @ true
    moveq r0,#0
    mov    pc, lr
@
@ Call from C as ARMPERFTEST(s, d, len)
@ tested on Qualcomm M7200 with 1MB buffer and 20 iterations
@ ldmia all 8 registers at once yielded 362ms
@ ldmia 4 registers at a time yielded 349ms
@ adding PLD instruction dropped it to 225ms
@
ARMPERFTEST:   
    stmfd    sp!,{r4-r12,lr}
@    sub  r2,r2,#4
    mov r2,r2,LSR #5
perfloop1:   
    ldr r4,[r0],#4
    subs r2,r2,#1
@    str r4,[r1],#32
    stmia r1,{r4,r5}
    add r1,r1,#32
    bne perfloop1
    b perfexit
    
    mov  r2,r2,lsr #4        @ 16 bytes per iteration
perfloop0:   
    ldmia r0!,{r4,r5,r6,r7}
@    ldmia r0!,{r4-r11}
    pld [r0,#0x40]
    subs  r2,r2,#1
    stmia r1!,{r4,r5,r6,r7}
@    stmia r1!,{r4-r11}
@    ldmia r0!,{r8,r9,r10,r11}
@    stmia r1!,{r8,r9,r10,r11}
    bne   perfloop0
perfexit:   
    ldmia sp!,{r4-r12,pc}
@
@ stretch or shrink 16-bit pixels from a src bitmap to a dest
@ ARMBLTFTW (added 2/7/07) - scale an image larger or smaller (w/o pixel averaging)
@ call from C as ARMBLTFTW(&dftw, 270)@
@
ARMBLTFTW:   
    stmfd    sp!,{r4-r12,lr}
    mov   r2,#255
    add   r2,r2,#15   @ make 270
    cmp   r1,r2       @ display angle = 270?
    beq   bltftw_270
    cmp   r1,#90
    beq   bltftw_90
    cmp   r1,#0
    ldmneia sp!,{r4-r12,pc} @ invalid angle, leave
@ angle 0 case
bltftw_0:   
   mov r2,#0      @ ysum
   ldr r12,[r0,#destheight]
   ldr r14,[r0,#scaley]
   ldr r10,[r0,#destpitch]
bltftw_0_top:   
   mov r4,r2,LSR #16  @ source y
   ldr r5,[r0,#destptr]
   ldr r6,[r0,#srcptr]
   ldr r1,[r0,#destheight]
   ldr r7,[r0,#srcpitch]
   ldr r11,[r0,#destx]
   ldr r8,[r0,#desty]
   mov r9,r1         @ save height to check if we're on a Treo 800w
   sub r1,r1,r12     @ how far down we are (ysize-ycount)
   add r5,r5,r11,LSL #1 @ add destination X offset
   add r1,r1,r8      @ add destination Y offset
   cmp r9,#241       @ if >= 241, it's a treo 800w
   bge blt_notq9h
   cmp r10,#0x1000   @ special case for Q9H
   andeq r11,r1,#63  @ do special calc for dest address
   addeq r5,r5,r11,LSL #12 @ in 64-line groups, pitch is 0x1000
   moveq r11,r1,LSR #6  @ get the group #
   moveq r1,#0x280   @ offset to each 64-line group
   muleq r8,r1,r11
blt_notq9h:   
   mulne r8,r10,r1
   add r5,r5,r8      @ point to start of dest line
   mul r8,r7,r4      @ point to start of source line
   add r6,r6,r8
   ldr r7,[r0,#scalex]
   mov r4,#0      @ xsum
   ldr r11,[r0,#destwidth]
   bic r11,r11,#1 @ make sure it's an even number of destination pixels
bltftw_0_loop0: @ inner loop
@ do 4 dest pixels at a time for maximum
@ utilization of the ARM write buffers
   mov r8,r4,LSR #15  @ scaled source pixel
   bic r8,r8,#1   @ shorts
   ldrh r1,[r6,r8]   @ source pixel
   add r4,r4,r7   @ xsum += xscale
   mov r8,r4,LSR #15  @ scaled source pixel
   bic r8,r8,#1   @ shorts
   ldrh r9,[r6,r8]   @ source pixel
   add r4,r4,r7   @ xsum += xscale
   orr r3,r1,r9,LSL #16 @ merge 2 pixels for better speed
   mov r8,r4,LSR #15  @ scaled source pixel
   bic r8,r8,#1   @ shorts
   ldrh r1,[r6,r8]   @ source pixel
   add r4,r4,r7   @ xsum += xscale
   mov r8,r4,LSR #15  @ scaled source pixel
   bic r8,r8,#1   @ shorts
   ldrh r9,[r6,r8]   @ source pixel
   add r4,r4,r7   @ xsum += xscale
   subs r11,r11,#4   @ decrement destination count
   orr r9,r1,r9,LSL #16 @ merge 2 pixels for better speed
   stmia r5!,{r3,r9}   @ store and advance pointer
   bne bltftw_0_loop0
@ advance y
   add r2,r2,r14  @ ysum += yscale
   subs r12,r12,#1   @ decrement y count
   bne bltftw_0_top
   b bltftw_done
@   
@ Display rotated 90 case
@
bltftw_90:   
   ldr r2,[r0,#srcwidth]
   cmp r2,#160
   beq bltftw_90a    @ other method is faster for small src
   mov r2,#0      @ ysum
   ldr r12,[r0,#destheight]
   ldr r14,[r0,#scaley]
   ldr r10,[r0,#destpitch]
bltftw_90_top:   
   mov r4,r2,LSR #16  @ source y
   ldr r5,[r0,#leftdestptr]
   ldr r6,[r0,#srcptr]
   ldr r1,[r0,#destheight]
   ldr r7,[r0,#srcpitch]
   ldr r9,[r0,#srcheight]
   sub r1,r1,r12     @ how far down we are (ysize-ycount)
   add r5,r5,r1,LSL #1  @ point to start of dest line
   sub r9,r9,#1
   sub r4,r9,r4
   mul r8,r7,r4      @ point to start of source line (start from bottom)
   add r6,r6,r8
   mov r9,r2,LSL #16
   adds r9,r9,r14,LSL #16    @ see if the next line jumps on the src
   mov r4,r6    @ second source
   subcs r4,r4,r7    @ need to move up 1 line
   ldr r7,[r0,#scalex]
   ldr r11,[r0,#destwidth]
   ldr r9,[r6],#4   @ get 2 source pixels
   ldr r8,[r4],#4    @ and from "next" line
   sub r11,r11,#1    @ since we draw on the 0th count
   mov r3,#2        @ pixels available
bltftw_90_loop0: @ inner loop
   adds r7,r7,r7,LSL #16   @ xsum += xscale
   movcs r9,r9,LSR #16    @ get next pixel (lower)
   movcs r8,r8,LSR #16    @ get next pixel (upper)
   subcss r3,r3,#1
   ldreq r9,[r6],#4
   ldreq r8,[r4],#4
   moveq r3,#2
   subs r11,r11,#1   @ decrement destination count
@ create a double pixel to store
   mov  r1,r8,LSL #16    @ "top"
   mov  r14,r9,LSL #16    @ "bottom"
   orr  r1,r1,r14,LSR #16    @ combine
   str  r1,[r5],r10
   bne bltftw_90_loop0
@ advance y
   ldr r14,[r0,#scaley]
   subs r12,r12,#2   @ decrement y count
   add r2,r2,r14,LSL #1  @ ysum += yscale*2 (we drew 2 lines)
   bgt bltftw_90_top
   b bltftw_done
bltftw_90a:   
   ldr r10,[r0,#scalex]        @ src and delta
   mov r12,#0     @ dest y
   ldr r5,[r0,#srcheight]
   ldr r6,[r0,#srcpitch]
   sub r5,r5,#1
   mul r7,r5,r6        @ start at "bottom left" of src
   ldr r6,[r0,#srcptr]
   add r3,r6,r7        @ now r3 points to bottom left of src
bltftw_90_topa:   
   mov r6,r3        @ source ptr
   ldr r2,[r0,#destpitch]
   ldr r5,[r0,#leftdestptr]
   ldr r7,[r0,#srcpitch]
   rsb r7,r7,#0            @ negative pitch
   mul r8,r2,r12       @ point to start of dest line
   add r5,r5,r8        @ R5 points to start of dest line
   bic r5,r5,#3            @ we have to be on a DWORD boundary
   ldr r8,[r0,#scaley]
   ldr r11,[r0,#destheight]
   adds r4,r10,r10,LSL #16    @ see if this line and the next will be the same
   bcs bltftw_90_loop0a        @ no, we have to touch that memory
   ldr r4,[r0,#destwidth]
   sub r4,r4,r12    @ see if we have at least 1 line left
   cmp r4,#1
   ble bltftw_90_loop0a    @ no, last line
@ we can do it a little faster by drawing both
@ current and next line at the same time
   add  r2,r5,r2    @ point R2 to the next line
bltftw_90_loop1a:   
   ldrh r9,[r6]     @ first source pixel
   adds r8,r8,r8,LSL #16   @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]    @ second source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   orr  r4,r9,r14,LSL #16   @ combine 2 pixels
   ldrh r9,[r6]     @ third source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]     @ fourth source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   pld  [r6,r7]        @ preload next line
   subs r11,r11,#4   @ decrement destination count
   orr  r9,r9,r14,LSL #16   @ combine 2 pixels
   stmia r5!,{r4,r9}
   stmia r2!,{r4,r9}    @ and the line below
   bgt bltftw_90_loop1a
   add r12,r12,#1    @ we did an extra line
   adds r10,r10,r10,LSL #16    @ update y sum for the extra line
   b   bltftw_90n     @ next line
@ have to draw a single line
bltftw_90_loop0a: @ inner loop
   ldrh r9,[r6]     @ first source pixel
   adds r8,r8,r8,LSL #16   @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]    @ second source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   orr  r4,r9,r14,LSL #16   @ combine 2 pixels
   ldrh r9,[r6]     @ third source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]     @ fourth source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   pld  [r6,r7]        @ preload next line
   subs r11,r11,#4   @ decrement destination count
   orr  r9,r9,r14,LSL #16   @ combine 2 pixels
   stmia r5!,{r4,r9}
   bgt bltftw_90_loop0a
@ Advance fractional src x and whole dest y
bltftw_90n: @ next line
   adds r10,r10,r10,LSL #16  @ src xsum += xscale
   addcs r3,r3,#2        @ advance src 1 pixel "right"
   ldr r4,[r0,#destwidth]
   add r12,r12,#1   @ inc dest y
   cmp r12,r4       @ done with whole image?
   bne bltftw_90_topa
   b bltftw_done
@   
@ Display rotated 270 case
@
bltftw_270:   
@   ldr r2,[r0,#srcwidth]
@   cmp r2,#160
@   beq bltftw_270a    @ other method is faster for small src
   mov r2,#0      @ ysum
   ldr r12,[r0,#destheight]
   ldr r14,[r0,#scaley]
   ldr r10,[r0,#destpitch]
   rsb r10,r10,#0    @ negate dest pitch
bltftw_270_top:   
   mov r4,r2,LSR #16  @ source y
   ldr r5,[r0,#leftdestptr]
   ldr r6,[r0,#srcptr]
   ldr r1,[r0,#destheight]
   ldr r7,[r0,#srcpitch]
   ldr r9,[r0,#destwidth]
   sub r1,r1,r12     @ how far down we are (ysize-ycount)
   add r5,r5,r1,LSL #1  @ point to start of dest line
   sub r9,r9,#1        @ start at bottom of dest
   rsb r1,r10,#0    @ get positive dest pitch
   mul r8,r9,r1
   add r5,r5,r8        @ now r5 points to correct dest pixel
   mul r8,r7,r4      @ point to start of source line (start from top)
   add r6,r6,r8        @ now r6 points to correct src pixel
   mov r9,r2,LSL #16
   adds r9,r9,r14,LSL #16    @ see if the next line jumps on the src
   mov r4,r6    @ second source
   addcs r4,r4,r7    @ need to down up 1 line
   ldr r7,[r0,#scalex]
   ldr r11,[r0,#destwidth]
   ldr r9,[r6],#4   @ get 2 source pixels
   ldr r8,[r4],#4    @ and from "next" line
   sub r11,r11,#1    @ since we draw on the 0th count
   mov r3,#2        @ pixels available
bltftw_270_loop0: @ inner loop
   adds r7,r7,r7,LSL #16   @ xsum += xscale
   movcs r9,r9,LSR #16    @ get next pixel (upper)
   movcs r8,r8,LSR #16    @ get next pixel (lower)
   subcss r3,r3,#1
   ldreq r9,[r6],#4
   ldreq r8,[r4],#4
   moveq r3,#2
   subs r11,r11,#1   @ decrement destination count
@ create a double pixel to store
   mov  r1,r8,LSL #16    @ "top"
   mov  r14,r9,LSL #16    @ "bottom"
   orr  r1,r1,r14,LSR #16    @ combine
   str  r1,[r5],r10        @ r10 is negative, so we work our way up the screen
   bne bltftw_270_loop0
@ advance y
   ldr r14,[r0,#scaley]
   subs r12,r12,#2   @ decrement y count
   add r2,r2,r14,LSL #1  @ ysum += yscale*2 (we drew 2 lines)
   bgt bltftw_270_top
   b bltftw_done
bltftw_270a:   
   ldr r10,[r0,#scalex]        @ src and delta
   mov r12,#0     @ dest y
   ldr r5,[r0,#srcheight]
   ldr r6,[r0,#srcpitch]
   sub r5,r5,#1
   mul r7,r5,r6        @ start at "bottom left" of src
   ldr r6,[r0,#srcptr]
   add r3,r6,r7        @ now r3 points to bottom left of src
bltftw_270_topa:   
   mov r6,r3        @ source ptr
   ldr r2,[r0,#destpitch]
   ldr r5,[r0,#leftdestptr]
   ldr r7,[r0,#srcpitch]
   rsb r7,r7,#0            @ negative pitch
   mul r8,r2,r12       @ point to start of dest line
   add r5,r5,r8        @ R5 points to start of dest line
   bic r5,r5,#3            @ we have to be on a DWORD boundary
   ldr r8,[r0,#scaley]
   ldr r11,[r0,#destheight]
   adds r4,r10,r10,LSL #16    @ see if this line and the next will be the same
   bcs bltftw_270_loop0a        @ no, we have to touch that memory
   ldr r4,[r0,#destwidth]
   sub r4,r4,r12    @ see if we have at least 1 line left
   cmp r4,#1
   ble bltftw_270_loop0a    @ no, last line
@ we can do it a little faster by drawing both
@ current and next line at the same time
   add  r2,r5,r2    @ point R2 to the next line
bltftw_270_loop1a:   
   ldrh r9,[r6]     @ first source pixel
   adds r8,r8,r8,LSL #16   @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]    @ second source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   orr  r4,r9,r14,LSL #16   @ combine 2 pixels
   ldrh r9,[r6]     @ third source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]     @ fourth source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   pld  [r6,r7]        @ preload next line
   subs r11,r11,#4   @ decrement destination count
   orr  r9,r9,r14,LSL #16   @ combine 2 pixels
   stmia r5!,{r4,r9}
   stmia r2!,{r4,r9}    @ and the line below
   bgt bltftw_270_loop1a
   add r12,r12,#1    @ we did an extra line
   adds r10,r10,r10,LSL #16    @ update y sum for the extra line
   b   bltftw_270n     @ next line
@ have to draw a single line
bltftw_270_loop0a: @ inner loop
   ldrh r9,[r6]     @ first source pixel
   adds r8,r8,r8,LSL #16   @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]    @ second source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   orr  r4,r9,r14,LSL #16   @ combine 2 pixels
   ldrh r9,[r6]     @ third source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   ldrh r14,[r6]     @ fourth source pixel
   adds r8,r8,r8,LSL #16  @ src y sum
   addcs r6,r6,r7    @ "inc" src y
   pld  [r6,r7]        @ preload next line
   subs r11,r11,#4   @ decrement destination count
   orr  r9,r9,r14,LSL #16   @ combine 2 pixels
   stmia r5!,{r4,r9}
   bgt bltftw_270_loop0a
@ Advance fractional src x and whole dest y
bltftw_270n: @ next line
   adds r10,r10,r10,LSL #16  @ src xsum += xscale
   addcs r3,r3,#2        @ advance src 1 pixel "right"
   ldr r4,[r0,#destwidth]
   add r12,r12,#1   @ inc dest y
   cmp r12,r4       @ done with whole image?
   bne bltftw_270_topa
bltftw_done:   
   ldmia sp!,{r4-r12,pc}
@
@ Draw an image at 75% scale with pixel averaging
@ call from C as ASMBLT75S(s, sPitch, d, dPitch, Width, Height)@
@ 8x8 blocks of pixels turn into 6x6 blocks
@
ASMBLT75S:   
   stmfd sp!,{r4-r12,lr}
   mov  r14,#0xf700        @ prepare averaging mask
   orr  r14,r14,#0xde
   orr  r14,r14,r14,LSL #16
   mov  r14,r14,LSR #1    @ prepare for mask and add
   ldr  r9,[sp,#40]    @ get the source width
   ldr  r11,[sp,#44]    @ get height
   mov  r10,r9        @ keep width in r10
   add  r12,r9,r9,LSL #1    @ width*3
   mov  r12,r12,LSR #1        @ (width*3/4) * sizeof (short)
   
@ first line
blt75_top0:   
   ldmia r0!,{r5-r8}    @ read 8 pixels (which become 6)
   subs r10,r10,#8        @ line count
   and r6,r14,r6,LSR #1    @ second pair gets averaged
   add r6,r6,r6,LSR #16    @ average the 2 pixels together
   mov r6,r6,LSL #16    @ blast upper 16 bits
   mov r6,r6,LSR #16
   orr r6,r6,r7,LSL #16    @ get 5th pixel unchanged
   mov r7,r7,LSR #16    @ get 6th pixel in 5th position
   and r8,r14,r8,LSR #1  @ average the 7th and 8th pixels
   add r8,r8,r8,LSR #16
   mov r8,r8,LSL #16    @ put it in upper pixel position
   orr r7,r7,r8            @ now r7 has the new pixels 5 and 6
   stmia r2!,{r5-r7}    @ store the 6 new pixels
   bne  blt75_top0
   add  r0,r0,r1        @ point to next source line
   sub  r0,r0,r9,LSL #1    @ move back amount we read through
   add  r2,r2,r3        @ point to next dest line
   sub  r2,r2,r12        @ move back amount we wrote
@ second line
   mov  r10,r9            @ width
blt75_top1:   
   ldmia r0!,{r5-r8}    @ read 8 pixels (which become 6)
   subs r10,r10,#8        @ line count
   and r6,r14,r6,LSR #1    @ second pair gets averaged
   add r6,r6,r6,LSR #16    @ average the 2 pixels together
   mov r6,r6,LSL #16    @ blast upper 16 bits
   mov r6,r6,LSR #16
   orr r6,r6,r7,LSL #16    @ get 5th pixel unchanged
   mov r7,r7,LSR #16    @ get 6th pixel in 5th position
   and r8,r14,r8,LSR #1  @ average the 7th and 8th pixels
   add r8,r8,r8,LSR #16
   mov r8,r8,LSL #16    @ put it in upper pixel position
   orr r7,r7,r8            @ now r7 has the new pixels 5 and 6
   stmia r2!,{r5-r7}    @ store the 6 new pixels
   bne  blt75_top1
   add  r0,r0,r1        @ point to next source line
   sub  r0,r0,r9,LSL #1    @ move back amount we read through
   add  r2,r2,r3        @ point to next dest line
   sub  r2,r2,r12        @ move back amount we wrote
@ third line
   mov  r10,r9            @ width
blt75_top2:   
   subs r10,r10,#8        @ decrement x count
   ldr  r6,[r0,r1]        @ first pair below
   ldr  r5,[r0],#4        @ first pair
   and  r6,r14,r6,LSR #1    @ average the 2 together
   and  r5,r14,r5,LSR #1    @ average these as well
   add  r4,r5,r6        @ average top pair to bottom pair
   
   ldr  r6,[r0,r1]        @ second pair below
   ldr  r5,[r0],#4        @ second pair
   and  r6,r14,r6,LSR #1    @ average this pair together
   add  r6,r6,r6,LSR #16
   and  r5,r14,r5,LSR #1    @ and this pair
   add  r5,r5,r5,LSR #16
   and  r5,r14,r5,LSR #1    @ average again
   and  r6,r14,r6,LSR #1
   add  r5,r5,r6            @ now 4 pixels averaged
   mov  r5,r5,LSL #16    @ clear top 16 bits
   mov  r5,r5,LSR #16
      
   ldr  r7,[r0,r1]        @ third pair below
   ldr  r6,[r0],#4        @ third pair
   and  r7,r14,r7,LSR #1    @ average top to bottom
   and  r6,r14,r6,LSR #1
   add  r6,r6,r7        @ top pair averaged to bottom pair
   orr  r5,r5,r6,LSL #16    @ combine this pair
   
   ldr  r8,[r0,r1]        @ fourth pair below
   ldr  r7,[r0],#4        @ fourth pair
   and  r8,r14,r8,LSR #1    @ average left/right of each pair
   and  r7,r14,r7,LSR #1
   add  r7,r7,r7,LSR #16
   add  r8,r8,r8,LSR #16
   and  r7,r14,r7,LSR #1    @ average top to bottom
   and  r8,r14,r8,LSR #1
   add  r7,r7,r8
   mov  r6,r6,LSR #16    @ get fifth pixel in position
   orr  r6,r6,r7,LSL #16    @ combine last pair
   stmia r2!,{r4-r6}    @ store 3 pairs to dest
   bne  blt75_top2
   sub  r0,r0,r9,LSL #1  @ move back to start of this line
   add  r0,r0,r1,LSL #1        @ move down 2 source lines
   add  r2,r2,r3        @ move down to next dest line
   sub  r2,r2,r12        @ move back amount we wrote
   mov  r10,r9            @ keep width in r10
   subs r11,r11,#4        @ decrement height
   bne  blt75_top0
   
   ldmfd sp!,{r4-r12,pc} @ restore regs and return
@
@ Draw an image at 270 degrees rotated
@ call from C as ASMBLT100_270(s, SrcPitch, d, DstPitch, cx, cy)@
@
ASMBLT100_270:   
    stmfd    sp!,{r4-r12,r14}
    ldr r12,[sp,#40]    @ get width
    ldr r14,[sp,#44]    @ get height
    add r0,r0,r12,LSL #1    @ point to last 2 columns
    sub r0,r0,#4
asm100_270_top:   
   mov  r8,r14            @ vertical count
   mov  r9,r0            @ starting pointer
asm100_270a:   
   ldr  r4,[r9]   @ get 2 pixels
   ldr  r5,[r9,r1]    @ get 2 pixels below
   add  r9,r9,r1,LSL #1    @ skip down 2 lines
   mov  r6,r4,LSR #16
   mov  r7,r5,LSR #16
   orr  r6,r6,r7,LSL #16
   mov  r4,r4,LSL #16
   mov  r5,r5,LSL #16
   orr  r5,r5,r4,LSR #16
   str  r6,[r2]
   str  r5,[r2,r3]
   add  r2,r2,#4
   subs r8,r8,#2
   bne  asm100_270a
   sub  r2,r2,r14,LSL #1    @ back to start of dest line
   add  r2,r2,r3,LSL #1        @ skip 2 lines
   sub  r0,r0,#4            @ back 2 pixels in source
   subs r12,r12,#2            @ horizontal count
   bne  asm100_270_top
   ldmfd sp!,{r4-r12,pc} @ restore regs
@
@ Draw an image at 90 degrees rotated
@ call from C as ASMBLT100_90(s, SrcPitch, d, DstPitch, cx, cy)@
@
ASMBLT100_90:   
    stmfd    sp!,{r4-r12,r14}
    ldr r12,[sp,#40]    @ get width
    ldr r14,[sp,#44]    @ get height
    mul r4,r1,r14    @ point to bottom of source
    sub r4,r4,r1,LSL #1    @ -2 lines
    add r0,r0,r4        @ now r0 points to bottom left
asm100_90_top:   
   mov  r8,r14            @ vertical count
   mov  r9,r0            @ starting pointer
asm100_90a:   
   ldr  r4,[r9]   @ get 2 pixels
   ldr  r5,[r9,r1]    @ get 2 pixels below
   sub  r9,r9,r1,LSL #1    @ skip up 2 lines
   mov  r6,r4,LSL #16
   mov  r7,r5,LSL #16
   orr  r6,r6,r7,LSR #16
   mov  r4,r4,LSR #16
   mov  r5,r5,LSR #16
   orr  r5,r5,r4,LSL #16
   str  r6,[r2]
   str  r5,[r2,r3]
   add  r2,r2,#4
   subs r8,r8,#2
   bne  asm100_90a
   sub  r2,r2,r14,LSL #1    @ back to start of dest line
   add  r2,r2,r3,LSL #1        @ skip 2 lines
   add  r0,r0,#4            @ advance 2 pixels in source
   subs r12,r12,#2            @ horizontal count
   bne  asm100_90_top
   ldmfd sp!,{r4-r12,pc} @ restore regs
@
@ Draw one line of tile data (opaque)
@ call from C as ARMDrawTileOpaque(unsigned long ulFlipColor, unsigned long ulLeft, unsigned long ulRight, unsigned char *pDest)
@
ARMDrawTileOpaque:   
    tst     r0,#0x4000        @ flipx?
    and     r0,r0,#0xff        @ isolate color value
    orr     r0,r0,r0,LSL #8
    orr     r0,r0,r0,LSL #16   @ put color in all 4 bytes
   orr     r1,r1,r0     @ add color to pixels
   orr     r2,r2,r0
    bne     tile_opaque_flipx
   tst     r3,#3        @ dword-aligned destination?
   stmeqia r3!,{r1,r2}  @ store all 8 pixels
   moveq   pc,lr        @ leave
@ store one pixel at a time
    tst     r3,#1        @ if it's an even address, we can use that
    bne     tile_opaque_bad @ worst case
    strh    r1,[r3]      @ first pair of pixels
    mov     r1,r1,LSR #16
    strh    r1,[r3,#2]   @ second pair
    strh    r2,[r3,#4]   @ third pair
    mov     r2,r2,LSR #16
    strh    r2,[r3,#6]   @ third pair
    mov     pc,lr
@ worst case
tile_opaque_bad:   
   strb    r1,[r3]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#1]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#2]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#3]
   strb    r2,[r3,#4]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#5]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#6]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#7]
   mov     pc,lr
@ also a worst case (horizontally flipped)
tile_opaque_flipx:   
   strb    r2,[r3,#3]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#2]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#1]
   mov     r2,r2,LSR #8
   strb    r2,[r3,#0]
   strb    r1,[r3,#7]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#6]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#5]
   mov     r1,r1,LSR #8
   strb    r1,[r3,#4]
   mov     pc,lr
   
@
@ Draw one line of tile data
@ call from C as ARMDrawTile(unsigned long ulFlipColor, unsigned long ulLeft, unsigned long ulRight, unsigned char *pDest)
@
ARMDrawTile:   
    tst     r0,#0x4000        @ flipx?
    and     r0,r0,#0xff        @ isolate color value
    bne     tile_flipx
    orrs    r1,r1,r1        @ first 4 pixels not transparent?
    beq     tile_d0      @ skip these 4
    tst     r1,#0xff        @ anything to draw?
    orrne   r12,r0,r1        @ prepare first pixel
    strneb  r12,[r3]
    tst     r1,#0xff00   @ second pixel?
    orrne   r12,r0,r1,LSR #8
    strneb  r12,[r3,#1]
    tst     r1,#0xff0000 @ third pixel?
    orrne   r12,r0,r1,LSR #16
    strneb  r12,[r3,#2]
    tst     r1,#0xff000000  @ forth pixel?
    orrne   r1,r0,r1,LSR #24
    strneb  r1,[r3,#3]
tile_d0:   
    orrs    r2,r2,r2        @ second 4 pixels not transparent?
    moveq   pc,lr        @ time to go
    tst     r2,#0xff        @ anything to draw?
    orrne   r12,r0,r2        @ prepare first pixel
    strneb  r12,[r3,#4]
    tst     r2,#0xff00   @ second pixel?
    orrne   r12,r0,r2,LSR #8
    strneb  r12,[r3,#5]
    tst     r2,#0xff0000 @ third pixel?
    orrne   r12,r0,r2,LSR #16
    strneb  r12,[r3,#6]
    tst     r2,#0xff000000  @ forth pixel?
    orrne   r12,r0,r2,LSR #24
    strneb  r12,[r3,#7]
    mov     pc,lr
tile_flipx:   
    orrs    r2,r2,r2        @ first 4 pixels not transparent?
    beq     tile_fd0
    tst     r2,#0xff        @ anything to draw?
    orrne   r12,r0,r2        @ prepare first pixel
    strneb  r12,[r3,#3]
    tst     r2,#0xff00
    orrne   r12,r0,r2,LSR #8
    strneb  r12,[r3,#2]
    tst     r2,#0xff0000
    orrne   r12,r0,r2,LSR #16
    strneb  r12,[r3,#1]
    tst     r2,#0xff000000
    orrne   r12,r0,r2,LSR #24
    strneb  r12,[r3,#0]
tile_fd0:   
    orrs    r1,r1,r1        @ first 4 pixels not transparent?
    moveq   pc,lr        @ we can return here
    tst     r1,#0xff        @ anything to draw?
    orrne   r12,r0,r1        @ prepare first pixel
    strneb  r12,[r3,#7]
    tst     r1,#0xff00
    orrne   r12,r0,r1,LSR #8
    strneb  r12,[r3,#6]
    tst     r1,#0xff0000
    orrne   r12,r0,r1,LSR #16
    strneb  r12,[r3,#5]
    tst     r1,#0xff000000
    orrne   r12,r0,r1,LSR #24
    strneb  r12,[r3,#4]
    mov     pc,lr
@
@ Draw one line of sprite data
@ call from C as SNESDrawSprite(psrc, pdest, color, flipx)
@
SNESDrawSprite:   
    orrs    r3,r3,r3        @ flipx?
    bne     sprite_flipx
    mov     r3,r1            @ get dest addr in r3
    ldmia   r0,{r0,r1}        @ get 8 source pixels
    orrs    r0,r0,r0        @ first 4 pixels not transparent?
    beq     sprite_d0
    orr     r0,r0,r2        @ add in the color
    tst     r0,#0xf
    strneb  r0,[r3,#0] 
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#1]
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#2]
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#3]
sprite_d0:   
    orrs    r1,r1,r1        @ second 4 pixels not transparent?
    beq     sprite_d1
    orr     r1,r1,r2        @ add in the color
    tst     r1,#0xf
    strneb  r1,[r3,#4]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#5]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#6]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#7]
sprite_d1:   
    mov     pc,lr
sprite_flipx:   
    mov     r3,r1            @ get dest addr in r3
    ldmia   r0,{r0,r1}        @ get 8 source pixels
    orrs    r1,r1,r1        @ second 4 pixels not transparent?
    beq     sprite_fd0
    orr     r1,r1,r2        @ add in the color
    tst     r1,#0xf
    strneb  r1,[r3,#3]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#2]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#1]
    mov     r1,r1,LSR #8
    tst     r1,#0xf
    strneb  r1,[r3,#0]
sprite_fd0:   
    orrs    r0,r0,r0
    beq     sprite_fd1
    orr     r0,r0,r2        @ add in the color
    tst     r0,#0xf
    strneb  r0,[r3,#7] 
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#6]
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#5]
    mov     r0,r0,LSR #8
    tst     r0,#0xf
    strneb  r0,[r3,#4]
sprite_fd1:   
    mov     pc,lr
@
@ Stretch a 16bpp bitmap 2x
@ call from C as ARMDraw2X(unsigned short *s, unsigned short *d, int iSrcPitch, int iDstPitch, int iWidth, int iHeight)@
@
ARMDraw2X:   
   stmfd  sp!,{r4-r12,lr}
   ldr  r9,[sp,#40]    @ get the source width
   ldr  r11,[sp,#44]    @ get height
draw2x_top:   
   mov  r10,r9,LSR #1    @ keep width in r10
@ draw the same line twice to prevent
@ cache misses on the writes
draw2x_loop0:   
   ldr  r4,[r0],#4        @ get 2 source pixels
   subs r10,r10,#1        @ h-count
   mov  r5,r4,LSL #16    @ get left pixel
   mov  r6,r4,LSR #16    @ get right pixel
   orr  r5,r5,r5,LSR #16    @ double it
   orr  r6,r6,r6,LSL #16    @ double it
   stmia r1!,{r5,r6}        @ store to dest
   bne  draw2x_loop0
   sub  r0,r0,r9,LSL #1    @ go back to start of source
   sub  r1,r1,r9,LSL #2    @ go back to start of dest
   add  r1,r1,r3        @ next dest line
@ second pass
   mov  r10,r9,LSR #1    @ keep width in r10
draw2x_loop1:   
   ldr  r4,[r0],#4        @ get 2 source pixels
   subs r10,r10,#1        @ h-count
   mov  r5,r4,LSL #16    @ get left pixel
   mov  r6,r4,LSR #16    @ get right pixel
   orr  r5,r5,r5,LSR #16    @ double it
   orr  r6,r6,r6,LSL #16    @ double it
   stmia r1!,{r5,r6}        @ store to dest
   bne  draw2x_loop1
   sub  r0,r0,r9,LSL #1    @ go back to start of source
   add  r0,r0,r2        @ next source line
   sub  r1,r1,r9,LSL #2    @ go back to start of dest
   add  r1,r1,r3        @ next dest line
   subs r11,r11,#1    @ count down through Y
   bne  draw2x_top
   ldmfd  sp!,{r4-r12,pc}
@
@ Copy 16bpp image data quickly
@ call from C as ARMGFXCopy16(pSrc, pDest, iWidth)
@
@ARMGFXCopy16 proc
@   stmfd  sp!,{r4-r6,lr}
@gfxcopy_loop
@   ldmia r0!,{r3-r6}    @ get 4 words to start (8 pixels)
@   pld  [r0,#32]        @ preload from source (makes it 12% faster on OMAP 2420, but no effect on OMAP 850 nor XSCALE)
@   subs r2,r2,#8    @ 8 pixels
@   stmia r1!,{r3-r6}
@   bgt gfxcopy_loop
@   ldmfd sp!,{r4-r6,pc}
@   endp
@
@ Fast routine for shrinking 50%
@ call from C as ASMBLT50S((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT50S:   
        stmfd   sp!, {r4-r12,r14}
        mov        r14,#0xef            @ need this constant (f7de) for pixel averaging
        orr        r14,r14,#0x7b00
        orr        r14,r14,r14,LSL #16    @ 7bef7bef
        ldr        r11,[sp,#44]        @ src height
        mov     r11,r11,LSR #1        @ dest height = src height / 2
asmblt50s_top:   
        ldr        r12,[sp,#40]        @ src width
        mov     r5,r2                @ dest pointer
        mov     r9,r0                @ source
        add     r10,r9,r1            @ source + 1 line
asmblt50s_0:   
        ldr     r7,[r9],#4            @ grab 2 src pixels
        ldr     r8,[r10],#4            @ grab 2 from the next line
        and     r7,r14,r7,LSR #1    @ prepare top pixels
        and     r8,r14,r8,LSR #1
        add     r7,r7,r8            @ average top/bottom lines
        and     r7,r14,r7,LSR #1    @ prepare to average left/right
        add     r7,r7,r7,LSL #16    @ average into top
        mov     r6,r7,LSR #16        @ store first pixel
        ldr     r7,[r9],#4            @ grab 2 src pixels
        ldr     r8,[r10],#4            @ grab 2 from the next line
        and     r7,r14,r7,LSR #1    @ prepare top pixels
        and     r8,r14,r8,LSR #1
        add     r7,r7,r8            @ average top/bottom lines
        and     r7,r14,r7,LSR #1    @ prepare to average left/right
        add     r7,r7,r7,LSR #16    @ average into bottom
        orr     r6,r6,r7,LSL #16    @ combine into a pixel pair
        str     r6,[r5],#4
        subs    r12,r12,#4            @ horizontal count
        bne     asmblt50s_0
        add     r2,r2,r3            @ next dest line
        add     r0,r0,r1,LSL #1        @ skip 2 source lines
        subs    r11,r11,#1            @ line count
        bne     asmblt50s_top
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for blitting 100%
@ call from C as ASMBLT100((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT100:   
        stmfd   sp!, {r4-r12,r14}
        ldr  r14,[sp,#44]        @ src height
        ldr  r12,[sp,#40]        @ src width
blt100top:   
        orr  r12,r12,r12,LSL #16    @ keep a copy up top
blt100a:   
        ldmia r0!,{r4-r7}    @ get 8 pixels
        stmia r2!,{r4-r7}
        sub   r12,r12,#0x80000    @ 8 pixels << 16
        cmp   r12,#0x10000
        bge   blt100a
        sub   r2,r2,r12,LSL #1    @ back to start of dest
        sub   r0,r0,r12,LSL #1    @ back to start of src
        add   r0,r0,r1        @ next src line
        add   r2,r2,r3        @ next dest line
        subs  r14,r14,#1    @ ycount
        bne   blt100top
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200%
@ call from C as ASMBLT200((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT200:   
        stmfd   sp!, {r4-r12,r14}
        ldr  r11,[sp,#44]        @ src height
        ldr  r12,[sp,#40]        @ src width
blt200top:   
        mov  r4,r2                @ temp dest pointer
        mov  r10,r12,LSR #2        @ number of 4 pixel groups to do
blt200a:   
        ldmia r0!,{r8,r9}    @ get 4 pixels to double
        mov   r6,r8,LSL #16
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r7,r8,LSR #16
        orr   r7,r7,r7,LSL #16    @ double second pixel
        mov   r8,r9,LSL #16
        orr   r8,r8,r8,LSR #16    @ double third pixel
        mov   r9,r9,LSR #16
        orr   r9,r9,r9,LSL #16    @ double forth pixel
        stmia r4!,{r6-r9}        @ store 8 pixels
        subs  r10,r10,#1
        bne   blt200a
        sub   r0,r0,r12,LSL #1    @ go back to start of this line
        add   r2,r2,r3        @ next dest line
        mov   r4,r2
@ second pass through the same line doesn't need to be preloaded
        mov  r10,r12,LSR #2    @ number of 4 pixel groups to do
blt200b:   
        ldmia r0!,{r8,r9}    @ get 4 pixels to double
        mov   r6,r8,LSL #16
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r7,r8,LSR #16
        orr   r7,r7,r7,LSL #16    @ double second pixel
        mov   r8,r9,LSL #16
        orr   r8,r8,r8,LSR #16    @ double third pixel
        mov   r9,r9,LSR #16
        orr   r9,r9,r9,LSL #16    @ double forth pixel
        stmia r4!,{r6-r9}        @ store 8 pixels
        subs  r10,r10,#1
        bne   blt200b
        sub   r0,r0,r12,LSL #1    @ pull back to start
        add   r0,r0,r1        @ point to next line
        add   r2,r2,r3        @ next dest line
        subs  r11,r11,#1        @ y count
        bne   blt200top
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% with high quality
@ call from C as ASMBLT200HQ((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT200HQ:   
        stmfd   sp!, {r4-r12,r14}
        ldr  r11,[sp,#44]        @ src height
        ldr  r12,[sp,#40]        @ src width
blt200hq_top:   
        mov  r4,r2                @ temp dest pointer
        mov  r10,r12,LSR #2        @ number of 4 pixel groups to do
        ldr  r6,[sp,#44]        @ height
        cmp  r11,#1
        cmpne  r11,r6            @ first and last lines just copied
        bne   blt200hq            @ use HQ algorithm
blt200hq_a:   
        ldmia r0!,{r8,r9}    @ get 4 pixels to double
        mov   r6,r8,LSL #16
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r7,r8,LSR #16
        orr   r7,r7,r7,LSL #16    @ double second pixel
        mov   r8,r9,LSL #16
        orr   r8,r8,r8,LSR #16    @ double third pixel
        mov   r9,r9,LSR #16
        orr   r9,r9,r9,LSL #16    @ double forth pixel
        stmia r4!,{r6-r9}        @ store 8 pixels
        subs  r10,r10,#1
        bne   blt200hq_a
        sub   r0,r0,r12,LSL #1    @ go back to start of this line
        add   r2,r2,r3        @ next dest line
        mov   r4,r2
@ second pass through the same line doesn't need to be preloaded
        mov  r10,r12,LSR #2    @ number of 4 pixel groups to do
blt200hq_b:   
        ldmia r0!,{r8,r9}    @ get 4 pixels to double
        mov   r6,r8,LSL #16
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r7,r8,LSR #16
        orr   r7,r7,r7,LSL #16    @ double second pixel
        mov   r8,r9,LSL #16
        orr   r8,r8,r8,LSR #16    @ double third pixel
        mov   r9,r9,LSR #16
        orr   r9,r9,r9,LSL #16    @ double forth pixel
        stmia r4!,{r6-r9}        @ store 8 pixels
        subs  r10,r10,#1
        bne   blt200hq_b
blt200hq_bot:   
        sub   r0,r0,r12,LSL #1    @ pull back to start
        add   r0,r0,r1        @ point to next line
        add   r2,r2,r3        @ next dest line
        subs  r11,r11,#1        @ y count
        bne   blt200hq_top
        ldmfd   sp!, {r4-r12,pc}    @ return
@ do HQ algorithm
blt200hq:   
        rsb  r5,r1,#0            @ negative source pitch
        mov  r14,r12            @ src width
        ldrh r6,[r0],#2            @ first and last pixels are just doubled
        sub  r14,r14,#2
        orr  r6,r6,r6,LSL #16
        str  r6,[r2]            @ store doubled pixel
        str  r6,[r2,r3]            @ and line below
        add  r2,r2,#4
blt200hq_a0:   
        ldrh r6,[r0,r5]            @ A
        ldrh r7,[r0,#-2]        @ C
        ldrh r8,[r0,r1]            @ D
        ldrh r9,[r0],#2            @ P
        ldrh r10,[r0]            @ B
blt200hq_a1:   
        cmp  r6,r7                @ IF C==A AND C!=D AND A!=B => 1=A
        movne r4,r9                @ else 1=P
        bne  blt200hq_a2
        cmp  r7,r8
        cmpne r6,r10
        moveq r4,r9                @ else 1=P        
        movne r4,r6
blt200hq_a2:   
        cmp  r6,r10             @ IF A==B AND A!=C AND B!=D => 2=B
        orrne r4,r4,r9,LSL #16    @ else 2=P
        bne  blt200hq_a3
        cmp  r6,r7
        cmpne r10,r8
        orrne r4,r4,r10,LSL #16
        orreq r4,r4,r9,LSL #16    @ else 2=P
blt200hq_a3:   
        str  r4,[r2]            @ store 1+2
        cmp  r8,r7              @ IF D==C AND D!=B AND C!=A => 3=C
        movne r4,r9                @ else 3=P
        bne  blt200hq_a4
        cmp  r8,r10
        cmpne r7,r6
        movne r4,r7
        moveq r4,r9            @ else 3=P
blt200hq_a4:   
        cmp  r10,r8                @ IF B==D AND B!=A AND D!=C => 4=D
        orrne r4,r4,r9,LSL #16    @ else 4=P
        bne  blt200hq_a5
        cmp  r6,r10
        cmpne r7,r8
        orrne r4,r4,r8,LSL #16
        orreq r4,r4,r9,LSL #16    @ else 4=P
blt200hq_a5:   
        str  r4,[r2,r3]        @ store second pair on line below
        add  r2,r2,#4
        subs  r14,r14,#1            @ X count
@ re-use some pixels and load new ones
        movne  r7,r9            @ C=P
        movne  r9,r10            @ P=B
        ldrneh r6,[r0,r5]        @ A
        ldrneh r10,[r0,#2]        @ B
        ldrneh r8,[r0,r1]        @ D
        addne  r0,r0,#2
        bne  blt200hq_a1
        ldrh r6,[r0],#2            @ last pixel
        orr  r6,r6,r6,LSL #16
        str  r6,[r2,r3]        @ and line below
        str  r6,[r2],#4
        sub  r2,r2,r12,LSL #2    @ back to beginning of src line
        add  r2,r2,r3        @ down 1 line (other line incremented above)
        b    blt200hq_bot
@
@ Fast routine for stretching 300% with high quality
@ call from C as ASMBLT300HQ((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iWidth, int iHeight)@
@
ASMBLT300HQ:   
        stmfd   sp!, {r4-r12,r14}
        ldr  r14,[sp,#44]            @ number of lines to do
blt300hq_top:   
        ldr  r10,[sp,#44]    @ height
        ldr  r12,[sp,#40]    @ width
        cmp  r14,#1
        cmpne r14,r10        @ first or last line?
        bne  blt300hq_b0
@ first and last lines are just stretched
        mov  r12,r12,LSR #2    @ work in groups of 4 pixels
blt300hq_a0:   
        ldmia r0!,{r10,r11}    @ get 4 pixels to stretch
        mov   r6,r10,LSL #16
        mov   r7,r6,LSR #16        @ first pixel x3
        orr   r6,r6,r6,LSR #16    @ triple first pixel
        mov   r10,r10,LSR #16
        orr   r7,r7,r10,LSL #16    @ top half of r7 = start of second pixel
        mov   r8,r10
        orr   r8,r8,r8,LSL #16        @ triple second pixel (3x)
        mov   r9,r11,LSL #16    @ start of third pixel
        mov   r10,r9,LSR #16    @ bottom half of 3rd pixel
        orr   r9,r9,r9,LSR #16    @ 3rd pixel done
        mov   r11,r11,LSR #16    @ 4th pixel
        orr   r10,r10,r11,LSL #16    @ start 4th pixel
        orr   r11,r11,r11,LSL #16   @ finish 4th pixel
        stmia r2,{r6-r11}        @ store 12 pixels
        add   r2,r2,r3
        stmia r2,{r6-r11}        @ 3 times
        add   r2,r2,r3
        stmia r2!,{r6-r11}
        sub   r2,r2,r3,LSL #1
        subs  r12,r12,#1
        bne   blt300hq_a0
blt300hq_bot:   
        ldr   r12,[sp,#40]        @ get width again
        add   r0,r0,r1            @ next source line
        sub   r0,r0,r12,LSL #1    @ adjust for incrementing
        add   r4,r12,r12,LSL #1    @ width*3
        sub   r2,r2,r4,LSL #1    @ back to start of dest line
        add   r2,r2,r3,LSL #1    @ next dest line
        add   r2,r2,r3            @ times 3
        subs  r14,r14,#1
        bne   blt300hq_top
        ldmfd   sp!, {r4-r12,pc}    @ return
@ use "HQ" algorithm
blt300hq_b0:   
        ldrh  r4,[r0],#2        @ first and last pixel are just tripled
        sub   r12,r12,#2        @ remove first and last
        orr   r14,r14,r12,LSL #16    @ put x count on top of y count
        orr   r4,r4,r4,LSL #16
        str   r4,[r2]
        strh  r4,[r2,#4]        @ store the triplet
        add   r2,r2,r3
        str   r4,[r2]
        strh  r4,[r2,#4]
        add   r2,r2,r3
        str   r4,[r2],#4
        strh  r4,[r2],#2
        sub   r2,r2,r3,LSL #1
        sub   r0,r0,r1        @ point to line above
        ldrh  r4,[r0,#-2]    @ A
        ldrh  r5,[r0,#0]    @ B
        ldrh  r6,[r0,#2]    @ C
        add   r0,r0,r1
        ldrh  r7,[r0,#-2]    @ D
        ldrh  r8,[r0,#0]    @ E (current pixel)
        ldrh  r9,[r0,#2]    @ F
        add   r0,r0,r1
        ldrh  r10,[r0,#-2]    @ G
        ldrh  r11,[r0,#0]    @ H (current pixel)
        ldrh  r12,[r0,#2]    @ I
        sub   r0,r0,r1
        add   r0,r0,#2
blt300hq_b1:   
        cmp   r7,r5         @ IF D==B AND D!=H AND B!=F => 1=D
        strneh r8,[r2]        @ 1=E
        bne   blt300hq_b2
        cmp   r7,r11
        cmpne r5,r9
        strneh r7,[r2]        @ 1=D
        streqh r8,[r2]        @ else 1=E
blt300hq_b2:   
        cmp   r7,r5         @ IF (D==B AND D!=H AND B!=F AND E!=C) OR (B==F AND B!=D AND F!=H AND E!=A) 2=B
        bne   blt300hq_b3
        cmp   r7,r11
        cmpne r5,r9
        cmpne r8,r6
        strneh r5,[r2,#2]
        bne   blt300hq_b4
blt300hq_b3:   
        cmp   r5,r9
        strneh r8,[r2,#2]    @ else 2=E
        bne   blt300hq_b4
        cmp   r5,r7
        cmpne r9,r11
        cmpne r8,r4
        strneh r5,[r2,#2]    @ 2=B
        streqh r8,[r2,#2]    @ else 2=E
blt300hq_b4:   
        cmp   r5,r9         @ IF B==F AND B!=D AND F!=H => 3=F
        strneh r8,[r2,#4]    @ else 3=E
        bne   blt300hq_b5
        cmp   r5,r7
        cmpne r9,r11
        strneh r9,[r2,#4]    @ 3=F
        streqh r8,[r2,#4]    @ else 3=E
blt300hq_b5:   
        add   r2,r2,r3        @ point to next dest line
        cmp      r7,r11        @ IF (H==D AND H!=F AND D!=B AND E!=A) OR (D==B AND D!=H AND B!=F AND E!=G) 4=D
        bne   blt300hq_b6
        cmp   r9,r11
        cmpne r7,r5
        cmpne r4,r8
        strneh r7,[r2]
        bne   blt300hq_b7
blt300hq_b6:   
        cmp   r5,r7
        strneh r8,[r2]        @ else 4=E
        bne   blt300hq_b7
        cmp   r7,r11
        cmpne r5,r9
        cmpne r8,r10
        strneh r7,[r2]        @ 4=D
        streqh r8,[r2]        @ else 4=E
blt300hq_b7:   
        strh  r8,[r2,#2]    @ 5=E
        cmp   r5,r9         @ IF (B==F AND B!=D AND F!=H AND E!=I) OR (F==H AND F!=B AND H!=D AND E!=C) 6=F
        bne   blt300hq_b8
        cmp   r5,r7
        cmpne r9,r11
        cmpne r8,r12
        strneh r9,[r2,#4]    @ 6=F
        bne   blt300hq_b9
blt300hq_b8:   
        cmp   r9,r11
        strneh r8,[r2,#4]    @ else 6=E
        bne   blt300hq_b9
        cmp   r9,r5
        cmpne r11,r7
        cmpne r8,r6
        strneh r9,[r2,#4]    @ 6=F
        streqh r8,[r2,#4]    @ else 6=E
blt300hq_b9:   
        add   r2,r2,r3
        cmp   r11,r7        @ IF H==D AND H!=F AND D!=B => 7=D
        strneh r8,[r2]        @ else 7=E
        bne   blt300hq_b10
        cmp   r11,r9
        cmpne r7,r5
        strneh r7,[r2]        @ 7=D
        streqh r8,[r2]        @ else 7=E
blt300hq_b10:   
        cmp   r9,r11        @ IF (F==H AND F!=B AND H!=D AND E!=G) OR (H==D AND H!=F AND D!=B AND E!=I) 8=H
        bne   blt300hq_b11
        cmp   r9,r5
        cmpne r11,r7
        cmpne r8,r10
        strneh r11,[r2,#2]    @ 8=H
        bne   blt300hq_b12
blt300hq_b11:   
        cmp   r11,r7
        strneh r8,[r2,#2]    @ else 8=E
        bne   blt300hq_b12
        cmp   r11,r9
        cmpne r7,r5
        cmpne r8,r12
        strneh r11,[r2,#2]    @ 8=H
        streqh r8,[r2,#2]    @ else 8=E
blt300hq_b12:   
        cmp   r9,r11        @ IF F==H AND F!=B AND H!=D => 9=F
        strneh r8,[r2,#4]    @ else 9=E
        bne   blt300hq_b13
        cmp   r9,r5
        cmpne r11,r7
        strneh r9,[r2,#4]    @ 9=F
        streqh r8,[r2,#4]    @ else 9=E
blt300hq_b13:   
        sub   r2,r2,r3,LSL #1    @ back to current line
        add   r2,r2,#6        @ advance 3 pixels
        sub   r14,r14,#0x10000
        cmp   r14,#0x10000
@ shift everything over 1 pixel
        subge r0,r0,r1    @ point to line above
        movge r4,r5        @ A=B
        movge r5,r6     @ B=C
        ldrgeh r6,[r0,#2]    @ new C
        addge r0,r0,r1
        movge r7,r8        @ D=E
        movge r8,r9     @ E=F
        ldrgeh r9,[r0,#2]    @ new F
        addge r0,r0,r1
        movge r10,r11    @ G=H
        movge r11,r12   @ H=I
        ldrgeh r12,[r0,#2]    @ new I
        addge r0,r0,#2    @ advance source ptr
        subge r0,r0,r1    @ point to current line
        bge   blt300hq_b1
        ldrh  r4,[r0],#2        @ first and last pixel are just tripled
        ldr   r12,[sp,#40]        @ get width again
        strh  r4,[r2]
        strh  r4,[r2,#2]
        strh  r4,[r2,#4]
        add   r2,r2,r3
        strh  r4,[r2]
        strh  r4,[r2,#2]
        strh  r4,[r2,#4]
        add   r2,r2,r3
        strh  r4,[r2],#2
        strh  r4,[r2],#2
        strh  r4,[r2],#2
        sub   r2,r2,r3,LSL #1
        b     blt300hq_bot
@
@ Fast routine for stretching 300%
@ call from C as ASMBLT300((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iWidth, int iHeight)
@
ASMBLT300:   
        stmfd   sp!, {r4-r12,r14}
        mov  r5,r2        @ point to dest line
        ldr  r14,[sp,#44]         @ number of lines to do
blt300top:   
        mov  r4,r5                @ temp dest pointer
        ldr  r12,[sp,#40]
        mov  r12,r12,LSR #2       @ number of 4 pixel groups to do
blt300a:   
        ldmia r0!,{r10,r11}    @ get 4 pixels to double
        mov   r6,r10,LSL #16
        mov   r7,r6,LSR #16        @ first pixel x3
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r10,r10,LSR #16
        orr   r7,r7,r10,LSL #16    @ top half of r7 = start of second pixel
        mov   r8,r10
        orr   r8,r8,r8,LSL #16        @ double second pixel (3x)
        mov   r9,r11,LSL #16    @ start of third pixel
        mov   r10,r9,LSR #16    @ bottom half of 3rd pixel
        orr   r9,r9,r9,LSR #16    @ 3rd pixel done
        mov   r11,r11,LSR #16    @ 4th pixel
        orr   r10,r10,r11,LSL #16    @ start 4th pixel
        orr   r11,r11,r11,LSL #16   @ finish 4th pixel
        stmia r4!,{r6-r11}        @ store 12 pixels
        subs  r12,r12,#1
        bne   blt300a
        ldr   r12,[sp,#40]		@ width
        sub   r0,r0,r12,LSL #1  @ go back to start of this line
        add   r5,r5,r3        @ next dest line
        mov   r4,r5
@ second pass through the same line doesn't need to be preloaded
        mov  r12,r12,LSR #2       @ number of 4 pixel groups to do
blt300b:   
        ldmia r0!,{r10,r11}    @ get 4 pixels to double
        mov   r6,r10,LSL #16
        mov   r7,r6,LSR #16        @ first pixel x3
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r10,r10,LSR #16
        orr   r7,r7,r10,LSL #16    @ top half of r7 = start of second pixel
        mov   r8,r10
        orr   r8,r8,r8,LSL #16        @ double second pixel (3x)
        mov   r9,r11,LSL #16    @ start of third pixel
        mov   r10,r9,LSR #16    @ bottom half of 3rd pixel
        orr   r9,r9,r9,LSR #16    @ 3rd pixel done
        mov   r11,r11,LSR #16    @ 4th pixel
        orr   r10,r10,r11,LSL #16    @ start 4th pixel
        orr   r11,r11,r11,LSL #16   @ finish 4th pixel
        stmia r4!,{r6-r11}        @ store 12 pixels
        subs  r12,r12,#1
        bne   blt300b
        ldr   r12,[sp,#40]
        sub   r0,r0,r12,LSL #1      @ go back to start of this line
        add   r5,r5,r3        @ next dest line
        mov   r4,r5
@ third pass through the same line doesn't need to be preloaded
        mov  r12,r12,LSR #2       @ number of 4 pixel groups to do
blt300c:   
        ldmia r0!,{r10,r11}    @ get 4 pixels to double
        mov   r6,r10,LSL #16
        mov   r7,r6,LSR #16        @ first pixel x3
        orr   r6,r6,r6,LSR #16    @ double first pixel
        mov   r10,r10,LSR #16
        orr   r7,r7,r10,LSL #16    @ top half of r7 = start of second pixel
        mov   r8,r10
        orr   r8,r8,r8,LSL #16        @ double second pixel (3x)
        mov   r9,r11,LSL #16    @ start of third pixel
        mov   r10,r9,LSR #16    @ bottom half of 3rd pixel
        orr   r9,r9,r9,LSR #16    @ 3rd pixel done
        mov   r11,r11,LSR #16    @ 4th pixel
        orr   r10,r10,r11,LSL #16    @ start 4th pixel
        orr   r11,r11,r11,LSL #16   @ finish 4th pixel
        stmia r4!,{r6-r11}        @ store 12 pixels
        subs  r12,r12,#1
        bne   blt300c
        add   r5,r5,r3        @ next dest line
        subs  r14,r14,#1        @ y count
        bne   blt300top
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 150% (with smoothing)
@ call from C as ASMBLT150S((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iWidth, int iHeight)@
@
ASMBLT150S:   
        stmfd   sp!, {r4-r12,r14}
        mov r14,#0xef    @ // need this constant (f7de) for pixel averaging
        orr r14,r14,#0x7b00
        orr r14,r14,r14,LSL #16    @ 7bef7bef
        mov r6,#0            @ y counter starts at 0
        mov r4,r0            @ source pointer in r4
        mov r0,#0xff
        orr r0,r0,#0xff00    @ keep a constant of FFFF for masking words
blt150s_top:   
        add r5,r6,r6,LSL #1    @ y*3/2
        mov r5,r5,LSR #1
@        cmp r1,#0x1000     @ check for Treo 800w (pitch=0x1000, but no venetian blinds)
@        bgt blt150_notq9h
@        cmp r3,#0x1000    @ special case for Moto Q9H
@        andeq r7,r5,#63   @ get the scanline within the group of 64
@        moveq r5,r5,LSR #6   @ get the group #
@        moveq r9,#0x280   @ offset per group
@        muleq r10,r9,r5
@        addeq r10,r10,r7,LSL #12   @ each scanline is 0x1000 bytes apart
blt150_notq9h:   
        ldr r8,[sp,#40]        @ get width
        mul   r10,r5,r3        @ pVideo[((y*3)/2) * iScreenPitch]
@        mulne r10,r5,r3        @ pVideo[((y*3)/2) * iScreenPitch]
        add r5,r10,r2        @ destination pointer ready
        sub r8,r8,#4
        orr r6,r6,r8,LSL #14    @ div by 4 shift left 16
blt150s_xloop0:   
        ldmia r4!,{r8,r9}    @ grab 4 source pixels (ul1,ul3) // ldrd illegal on Samsung ARM CPUs
        pld  [r4,#24]        @ preload pixels ahead
        pld  [r4,r1]        @ this seems to only have an effect on xscale processors
        and r7,r14,r8,LSR #1    @ prepare pixels to be averaged
        add r7,r7,r7,LSR #16    @ ul = (us1 >> 1) + (us2 >> 1)
        and r10,r8,r0        @ get original pixel 0
        orr r7,r10,r7,LSL #16    @ pair = (ul << 16) | ul1
        mov r10,r9,LSL #16    @ lower half of ul3
        orr r8,r10,r8,LSR #16    @ isolate ul2 and form second pair
        and r10,r14,r9,LSR #1        @ prepare for averaging
        add r10,r10,r10,LSR #16    @ average the 2
        and r10,r10,r0        @ & 0xffff
        mov r9,r9,LSR #16    @ prepare upper half of second pixel pair
        orr r9,r10,r9,LSL #16    @ combine 2 pixels
        stmia r5!,{r7,r8,r9}
        
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s_xloop0
        ldr  r8,[sp,#40]    @ get width
        mov  r9,#3
        add  r6,r6,#0x10000    @ make it 0 again
        mul  r10,r9,r8        @ dest amount to pull back
        sub r5,r5,r10
        add r5,r5,r3             @ point to next line
        sub r4,r4,r8,LSL #1        @ rescan the first row
        sub r8,r8,#4
        orr r6,r6,r8,LSL #14    @ do it again (new count << 16)
blt150s_xloop1:   
        ldr r8,[r4,r1]        @ read from the line below
        ldr r7,[r4],#4        @ and the current
        and r8,r14,r8,LSR #1    @ all pixels get averaged
        and r7,r14,r7,LSR #1
        add r9,r7,r8        @ average 2 pixels at once (ulA)
        and r10,r14,r9,LSR #1    @ averge these 2 pixels together
        add r11,r10,r10,LSR #16 @ ulB = (ulA avg ulA>>16)
        and r7,r9,r0        @ get lower portion
        orr r7,r7,r11,LSL #16    @ ulA | (ulB<<16)
        ldr r8,[r4,r1]        @ ul1a = first pair
        ldr r12,[r4],#4        @ ul3 = second pair
        and r8,r14,r8,LSR #1    @ all get averaged
        and r12,r14,r12,LSR #1
        add r10,r12,r8        @ average a pair of pixels in one shot (ulC)
        and r11,r14,r10,LSR #1    @ need to average the 2 halves of ulC
        add r12,r11,r11,LSR #16    @ ulD = ulC avg (ulC >> 16)
        and r12,r12,r0        @ ulD &= 0xffff
        mov r8,r9,LSR #16    @ ulA >> 16
        orr r8,r8,r10,LSL #16    @ pixels = (ulA >> 16) | (ulC << 16)
        mov r10,r10,LSR #16    @ use the upper half of ulC
        orr r9,r12,r10,LSL #16    @ prepare last pair
        stmia r5!,{r7,r8,r9}
        
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s_xloop1
        add  r6,r6,#0x10000
        ldr  r8,[sp,#40]    @ get width
        mov  r9,#3
        mul  r10,r9,r8        @ move back this many pixels (dest)
        sub r5,r5,r10        @ point to start of line
        add r5,r5,r3        @ point to next line
        sub  r4,r4,r8,LSL #1    @ start of source line
        add  r4,r4,r1        @ next line
        sub r8,r8,#4
        orr r6,r6,r8,LSL #14    @ do it again (new count << 16) - final loop
blt150s_xloop2:   
        ldmia r4!,{r8,r9}    @ grab 4 source pixels (ul1,ul3) // ldrd illegal on Samsung ARM CPUs
        pld  [r4,#24]        @ preload pixels ahead
        pld  [r4,r1]        @ this seems to only have an effect on xscale processors
        and r7,r14,r8,LSR #1    @ prepare pixels to be averaged
        add r7,r7,r7,LSR #16    @ ul = (us1 >> 1) + (us2 >> 1)
        and r10,r8,r0        @ get original pixel 0
        orr r7,r10,r7,LSL #16    @ pair = (ul << 16) | ul1
        mov r10,r9,LSL #16    @ lower half of ul3
        orr r8,r10,r8,LSR #16    @ isolate ul2 and form second pair
        and r10,r14,r9,LSR #1        @ prepare for averaging
        add r10,r10,r10,LSR #16    @ average the 2
        and r10,r10,r0        @ & 0xffff
        mov r9,r9,LSR #16    @ prepare upper half of second pixel pair
        orr r9,r10,r9,LSL #16    @ combine 2 pixels
        stmia r5!,{r7,r8,r9}
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s_xloop2
        ldr r8,[sp,#40]        @ width
        ldr r9,[sp,#44]        @ height
        add  r6,r6,#0x10000
        add r6,r6,#2        @ y += 2
        sub  r4,r4,r8,LSL #1    @ start of source line
        add  r4,r4,r1        @ next line
        cmp    r6,r9
        bne blt150s_top     @ loop through entire image
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 150% (with smoothing)
@ call from C as ASMBLT150S((unsigned long *)pBitmap, int iDestY, *pDest, int iDestPitch)@
@ optimized for the ARMv6 instruction set
@
ASMBLT150S_V6:   
        stmfd   sp!, {r4-r12,r14}
        mov r14,#0xef    @ // need this constant (f7de) for pixel averaging
        orr r14,r14,#0x7b00
        orr r14,r14,r14,LSL #16    @ 7bef7bef
        mov r6,#0            @ y counter (144 lines)
        mov r4,r0            @ source pointer in r4
        mov r0,#0xff
        orr r0,r0,#0xff00    @ keep a constant of FFFF for masking words
blt150s6_top:   
        add r5,r6,r6,LSL #1    @ y*3/2
        mov r5,r5,LSR #1
        add r5,r5,r1      @ add dest Y offset
        cmp r1,#20        @ check for Treo 800w (pitch=0x1000, but no venetian blinds)
        bgt blt1506_notq9h
        cmp r3,#0x1000    @ special case for Moto Q9H
        andeq r7,r5,#63   @ get the scanline within the group of 64
        moveq r5,r5,LSR #6   @ get the group #
        moveq r9,#0x280   @ offset per group
        muleq r10,r9,r5
        addeq r10,r10,r7,LSL #12   @ each scanline is 0x1000 bytes apart
blt1506_notq9h:   
        mulne r10,r5,r3        @ pVideo[((y*3)/2) * iScreenPitch]
        add r5,r10,r2        @ destination pointer ready
        orr r6,r6,#0x270000    @ work on 4 pixels at a time (4 become 6) (160/4)<<16 - 1
blt150s6_xloop0:   
        ldmia r4!,{r8,r9}    @ grab 4 source pixels (ul1,ul3) // ldrd illegal on Samsung ARM CPUs
        pld  [r4,#24]        @ preload pixels ahead
        pld  [r4,#320]        @ this seems to only have an effect on xscale processors
        and r7,r14,r8,LSR #1    @ prepare pixels to be averaged
        add r7,r7,r7,LSR #16    @ ul = (us1 >> 1) + (us2 >> 1)
        and r10,r8,r0        @ get original pixel 0
        orr r7,r10,r7,LSL #16    @ pair = (ul << 16) | ul1
        mov r10,r9,LSL #16    @ lower half of ul3
        orr r8,r10,r8,LSR #16    @ isolate ul2 and form second pair
        and r10,r14,r9,LSR #1        @ prepare for averaging
        add r10,r10,r10,LSR #16    @ average the 2
        and r10,r10,r0        @ & 0xffff
        mov r9,r9,LSR #16    @ prepare upper half of second pixel pair
        orr r9,r10,r9,LSL #16    @ combine 2 pixels
        stmia r5!,{r7,r8,r9}
        
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s6_xloop0
        add  r6,r6,#0x10000
        sub r5,r5,#480
        add r5,r5,r3      @ point to next line
        sub r4,r4,#320        @ rescan the first row
        orr r6,r6,#0x270000        @ do it again
blt150s6_xloop1:   
        ldr r8,[r4,#320]    @ read from the line below
        ldr r7,[r4],#4        @ and the current
        and r8,r14,r8,LSR #1    @ all pixels get averaged
        and r7,r14,r7,LSR #1
        add r9,r7,r8        @ average 2 pixels at once (ulA)
        and r10,r14,r9,LSR #1    @ averge these 2 pixels together
        add r11,r10,r10,LSR #16 @ ulB = (ulA avg ulA>>16)
        and r7,r9,r0        @ get lower portion
        orr r7,r7,r11,LSL #16    @ ulA | (ulB<<16)
        ldr r8,[r4,#320]        @ ul1a = first pair
        ldr r12,[r4],#4        @ ul3 = second pair
        and r8,r14,r8,LSR #1    @ all get averaged
        and r12,r14,r12,LSR #1
        add r10,r12,r8        @ average a pair of pixels in one shot (ulC)
        and r11,r14,r10,LSR #1    @ need to average the 2 halves of ulC
        add r12,r11,r11,LSR #16    @ ulD = ulC avg (ulC >> 16)
        and r12,r12,r0        @ ulD &= 0xffff
        mov r8,r9,LSR #16    @ ulA >> 16
        orr r8,r8,r10,LSL #16    @ pixels = (ulA >> 16) | (ulC << 16)
        mov r10,r10,LSR #16    @ use the upper half of ulC
        orr r9,r12,r10,LSL #16    @ prepare last pair
        stmia r5!,{r7,r8,r9}
        
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s6_xloop1
        add  r6,r6,#0x10000
        sub r5,r5,#480    @ point to next line
        add r5,r5,r3
        orr r6,r6,#0x270000        @ final loop (same as loop 0)
blt150s6_xloop2:   
        ldmia r4!,{r8,r9}    @ grab 4 source pixels (ul1,ul3) // ldrd illegal on Samsung ARM CPUs
        pld  [r4,#24]        @ preload pixels ahead
        pld  [r4,#320]        @ this seems to only have an effect on xscale processors
        and r7,r14,r8,LSR #1    @ prepare pixels to be averaged
        add r7,r7,r7,LSR #16    @ ul = (us1 >> 1) + (us2 >> 1)
        and r10,r8,r0        @ get original pixel 0
        orr r7,r10,r7,LSL #16    @ pair = (ul << 16) | ul1
        mov r10,r9,LSL #16    @ lower half of ul3
        orr r8,r10,r8,LSR #16    @ isolate ul2 and form second pair
        and r10,r14,r9,LSR #1        @ prepare for averaging
        add r10,r10,r10,LSR #16    @ average the 2
        and r10,r10,r0        @ & 0xffff
        mov r9,r9,LSR #16    @ prepare upper half of second pixel pair
        orr r9,r10,r9,LSL #16    @ combine 2 pixels
        stmia r5!,{r7,r8,r9}
        subs r6,r6,#0x10000    @ loop count
        bpl blt150s6_xloop2
        add  r6,r6,#0x10000
        add r6,r6,#2        @ y += 2
        cmp    r6,#144
        bne blt150s6_top     @ loop through entire image
        ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% rotated 90 degrees
@ call from C as ASMBLT200_90((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcHeight)@
@
ASMBLT200_90:   
      stmfd   sp!, {r4-r12,r14}
      mov  r14,r1,LSR #1    @ src pitch = "rows" to do * 2
      ldr  r12,[sp,#40]    @ get the source height
      sub  r4,r12,#1        @ point to last line of source
      mla  r5,r4,r1,r0    @ now R5 points to start of last line
      rsb  r1,r1,#0        @ -srcpitch
      mov  r6,r2        @ temp dest pointer
blt200_90top:   
      mov  r11,r12,LSR #2        @ dy/4
@ first pass
blt200_90a:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r8,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r9,[r5]
      ldrh  r10,[r5,r1]
      add   r5,r5,r1,LSL #1
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r8,LSL #16    @ double it
      orr   r9,r9,r9,LSL #16    @ double it
      orr   r10,r10,r10,LSL #16    @ double it
      stmia r6!,{r7-r10}        @ store 4 pixels
      subs  r11,r11,#1            @ "horizontal" count
      bne   blt200_90a
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      sub   r4,r12,#1            @ src height -1
      rsb   r1,r1,#0            @ +srcpitch
      mla   r5,r4,r1,r0            @ start at bottom again
      rsb   r1,r1,#0            @ -srcpitch
      mov   r11,r12,LSR #2        @ "columns" to do = dy/4
@ second pass
blt200_90b:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r8,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r9,[r5]
      ldrh  r10,[r5,r1]
      add   r5,r5,r1,LSL #1
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r8,LSL #16    @ double it
      orr   r9,r9,r9,LSL #16    @ double it
      orr   r10,r10,r10,LSL #16    @ double it
      stmia r6!,{r7-r10}        @ store 4 pixels
      subs  r11,r11,#1            @ "horizontal" count
      bne   blt200_90b
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      add   r0,r0,#2            @ next source column
      sub   r4,r12,#1            @ src height -1
      rsb   r1,r1,#0            @ +srcpitch
      mla   r5,r4,r1,r0            @ start at bottom again
      rsb   r1,r1,#0            @ -srcpitch
      subs  r14,r14,#1            @ column count-1
      bne   blt200_90top
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% rotated 90 degrees
@ call from C as ASMBLT200_90a((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT200_90a:   
      stmfd   sp!, {r4-r12,r14}
      ldr  r12,[sp,#44]    @ get the source height
      mov  r5,r0        @ now R5 points to start of first
      add  r2,r2,r12,LSL #2    @ point to "right corner of dest"
      sub  r2,r2,#8
blt200_90atop:   
      ldr  r14,[sp,#40]    @ get the source width
      mov  r6,r2        @ temp dest pointer
      mov  r14,r14,LSR #1        @ dx = (x/2)
      ldr   r4,[r5,r1]            @ grab 2 source pixels
      ldr   r11,[r5],#4        @ grab 2 on next line
blt200_90aa:   
      mov   r7,r4,LSL #16    @ first pixel
      orr   r7,r7,r7,LSR #16    @ double it
      mov   r8,r4,LSR #16
      orr   r8,r8,r8,LSL #16
      mov   r9,r11,LSL #16
      orr   r9,r9,r9,LSR #16
      mov   r10,r11,LSR #16
      orr   r10,r10,r10,LSL #16
      stmia r6,{r7,r9}        @ the max performance is mostly due to
      add   r6,r6,r3        @ using the write buffers to their max capacity
      stmia r6,{r7,r9}        @ by writing 4 sets of 2 dwords, we've maxed them out
      add   r6,r6,r3
      stmia r6,{r8,r10}
      ldr   r4,[r5,r1]            @ grab 2 source pixels
      ldr   r11,[r5],#4        @ grab 2 on next line
      pld   [r5,#0x60]
      add   r6,r6,r3
      stmia r6,{r8,r10}
      add   r6,r6,r3
      subs  r14,r14,#1            @ "horizontal" count
      bne   blt200_90aa
      add   r0,r0,r1,LSL #1        @ skip to next src line
      mov   r5,r0
      sub   r2,r2,#8            @ move to next line
      subs  r12,r12,#2            @ src height -2
      bne   blt200_90atop
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% rotated 90 degrees with HQ
@ call from C as ASMBLT200HQ_90((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT200HQ_90:   
      stmfd   sp!, {r4-r12,r14}
      ldr  r12,[sp,#44]    @ get the source height
      mov  r5,r0        @ now R5 points to start of first
      add  r2,r2,r12,LSL #2    @ point to "right corner of dest"
      sub  r2,r2,#8
blt200HQ_90top:   
      ldr  r14,[sp,#40]    @ get the source width
      mov  r6,r2        @ temp dest pointer
      mov  r14,r14,LSR #1        @ dx = (x/2)
      ldr   r4,[r5,r1]            @ grab 2 source pixels
      ldr   r11,[r5],#4        @ grab 2 on next line
blt200HQ_90a:   
      mov   r7,r4,LSL #16    @ first pixel
      orr   r7,r7,r7,LSR #16    @ double it
      mov   r8,r4,LSR #16
      orr   r8,r8,r8,LSL #16
      mov   r9,r11,LSL #16
      orr   r9,r9,r9,LSR #16
      mov   r10,r11,LSR #16
      orr   r10,r10,r10,LSL #16
      stmia r6,{r7,r9}        @ the max performance is mostly due to
      add   r6,r6,r3        @ using the write buffers to their max capacity
      stmia r6,{r7,r9}        @ by writing 4 sets of 2 dwords, we've maxed them out
      add   r6,r6,r3
      stmia r6,{r8,r10}
      ldr   r4,[r5,r1]            @ grab 2 source pixels
      ldr   r11,[r5],#4        @ grab 2 on next line
      pld   [r5,#0x60]
      add   r6,r6,r3
      stmia r6,{r8,r10}
      add   r6,r6,r3
      subs  r14,r14,#1            @ "horizontal" count
      bne   blt200HQ_90a
      add   r0,r0,r1,LSL #1        @ skip to next src line
      mov   r5,r0
      sub   r2,r2,#8            @ move to next line
      subs  r12,r12,#2            @ src height -2
      bne   blt200HQ_90top
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% rotated 90 degrees
@ call from C as ASMBLT200_90a((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)@
@
ASMBLT200_90b:   
      stmfd   sp!, {r4-r12,r14}
      ldr  r12,[sp,#44]    @ get the source height
      add  r2,r2,r12,LSL #2    @ point to "right corner of dest"
      sub  r2,r2,#4
blt200_90btop:   
      ldr  r14,[sp,#40]    @ get the source width
      mov  r5,r0        @ now R5 points to start of line
      mov  r6,r2        @ temp dest pointer
      mov  r14,r14,LSR #1        @ dx = iWidth/4
      ldmia r5!,{r4}        @ grab 4 source pixels
blt200_90ba:   
      mov   r9,r4,LSL #16    @ first pixel
      orr   r9,r9,r9,LSR #16    @ double it
      mov   r10,r4,LSR #16    @ second pixel
      orr   r10,r10,r10,LSL #16    @ double it
@      mov   r7,r11,LSL #16        @ third pixel
@      orr   r7,r7,r7,LSR #16    @ double it
@      mov   r8,r11,LSR #16        @ forth pixel
@      orr   r8,r8,r8,LSL #16    @ double it
      ldmia r5!,{r4}        @ grab 4 source pixels
      str   r9,[r6],r3
      str   r9,[r6],r3
      str   r10,[r6],r3
      pld   [r5,#0x40]
      str   r10,[r6],r3
@      str   r7,[r6],r3
@      str   r7,[r6],r3
@      str   r8,[r6],r3
@      str   r8,[r6],r3
      subs  r14,r14,#1            @ "horizontal" count
      bne   blt200_90ba
      add   r0,r0,r1            @ next source line
      sub   r2,r2,#4            @ move to next line
      subs  r12,r12,#1            @ src height -1
      bne   blt200_90btop
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 200% rotated 270 degrees
@ call from C as ASMBLT200_270((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iSrcHeight)@
@
ASMBLT200_270:   
      stmfd   sp!, {r4-r12,r14}
      ldr  r12,[sp,#40]    @ get the source width
      mov  r5,r0        @ now R5 points to start of first
      mul  r6,r3,r12    @ point to bottom left of dest
      mov  r6,r6,LSL #1
      sub  r6,r6,r3,LSL #2    @ -2 lines
      add  r2,r2,r6
      ldr  r12,[sp,#44]    @ get source height
      mov  r14,r2        @ keep dest pointer
blt200_270top:   
      ldr  r11,[sp,#40]    @ get the source width
      mov  r6,r2        @ temp dest pointer
      mov  r11,r11,LSR #1        @ dx = (x/2)
blt200_270a:   
      ldr   r10,[r5,r1]            @ grab 2 source pixels (line below)
      ldr   r8,[r5],#4        @ grab 2 on first line
      mov   r7,r8,LSL #16    @ first pixel
      orr   r7,r7,r7,LSR #16    @ double it
      mov   r8,r8,LSR #16
      orr   r8,r8,r8,LSL #16
      mov   r9,r10,LSL #16
      orr   r9,r9,r9,LSR #16
      mov   r10,r10,LSR #16
      orr   r10,r10,r10,LSL #16
      stmia r6,{r8,r10}
      add   r6,r6,r3
      stmia r6,{r8,r10}
      add   r6,r6,r3
      stmia r6,{r7,r9}
      add   r6,r6,r3
      stmia r6,{r7,r9}
      add   r6,r6,r3
      sub   r6,r6,r3,LSL #3        @ move up "1" line
      subs  r11,r11,#1            @ "horizontal" count
      bne   blt200_270a
      add   r0,r0,r1,LSL #1        @ skip to next 2 src lines
      mov   r5,r0
      add   r14,r14,#8            @ move to next line
      mov   r2,r14                @ new dest pointer
      subs  r12,r12,#2            @ src height -2
      bne   blt200_270top
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 300% rotated 90 degrees
@ call from C as ASMBLT300_90((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch)@
@
ASMBLT300_90:   
      stmfd   sp!, {r4-r12,r14}
      mov  r14,r1,LSR #1   @ src pitch = "rows" to do *2
      mov  r4,#143        @ point to last line of source
      mla  r5,r4,r1,r0    @ now R5 points to start of last line
      rsb  r1,r1,#0        @ -pitch
      mov  r6,r2        @ temp dest pointer
blt300_90top:   
      mov  r4,#36        @ 144/4
@ first pass
blt300_90a:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_90a
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      mov   r4,#143
      rsb   r1,r1,#0            @ +320
      mla   r5,r4,r1,r0            @ start at bottom again
      rsb   r1,r1,#0            @ -320
      mov   r4,#36                @ "columns" to do
@ second pass
blt300_90b:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_90b
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      mov   r4,#143
      rsb   r1,r1,#0            @ +320
      mla   r5,r4,r1,r0            @ start at bottom again
      rsb   r1,r1,#0            @ -320
      mov   r4,#36                @ "columns" to do
@ third pass
blt300_90c:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_90c
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      mov   r4,#143
      add   r0,r0,#2            @ next source column
      rsb   r1,r1,#0            @ +320
      mla   r5,r4,r1,r0            @ start at bottom again
      rsb   r1,r1,#0            @ -320
      subs  r14,r14,#1            @ column count-1
      bne   blt300_90top
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 300% rotated 270 degrees
@ call from C as ASMBLT300_270((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch)@
@
ASMBLT300_270:   
      stmfd   sp!, {r4-r12,r14}
      mov  r14,r1,LSR #1   @ src pitch = "rows" to do *2
      add  r0,r0,r1        @ point to last column of source
      sub  r0,r0,#2
      mov  r6,r2        @ temp dest pointer
blt300_270top:   
      mov  r5,r0        @ src ptr
      mov  r4,#36        @ 144/4
@ first pass
blt300_270a:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving down)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_270a
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      mov   r5,r0                @ start over
      mov   r4,#36                @ "columns" to do
@ second pass
blt300_270b:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_270b
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      mov   r5,r0                @ start again
      mov   r4,#36                @ "columns" to do
@ third pass
blt300_270c:   
      ldrh  r7,[r5]        @ grab 4 source pixels (moving up)
      ldrh  r9,[r5,r1]    @ second pixel
      add   r5,r5,r1,LSL #1
      ldrh  r10,[r5]
      ldrh  r12,[r5,r1]
      add   r5,r5,r1,LSL #1
      mov   r8,r7                @ part 3 of pixel 0
      orr   r7,r7,r7,LSL #16    @ double it
      orr   r8,r8,r9,LSL #16    @ part 1 of pixel 1
      orr   r9,r9,r9,LSL #16    @ double it
      mov   r11,r10                @ part 3 of pixel 2
      orr   r10,r10,r10,LSL #16        @ double it
      orr   r11,r11,r12,LSL #16    @ part 1 of pixel 3
      orr   r12,r12,r12,LSL #16        @ double it
      stmia r6!,{r7-r12}        @ store 4*3 pixels
      subs  r4,r4,#1            @ "horizontal" count
      bne   blt300_270c
      add   r2,r2,r3            @ move to next line
      mov   r6,r2
      sub   r0,r0,#2            @ next source column
      subs  r14,r14,#1            @ column count-1
      bne   blt300_270top
      
      ldmfd   sp!, {r4-r12,pc}    @ return
@
@ Fast routine for stretching 150% rotated 270 degrees (with smoothing)
@ call from C as ASMBLT150S270((unsigned long *)pBitmap, int iSrcPitch, *pDest, int iDestPitch, int iWidth, int iHeight)@
@
ASMBLT150S270:   
        stmfd   sp!, {r4-r12,r14}
        orrs r3,r3,r3        @ positive pitch = 270
        ldr    r4,[sp,#40]    @ get x size (Y on display)
        addpl  r6,r4,r4,LSL #1    @ x3
        movpl  r6,r6,LSR #1    @ y*1.5
        mulpl  r4,r6,r3        @ start at "bottom" of screen
        addpl  r2,r2,r4
        addmi  r6,r4,r4,LSL #1    @ x3
        addmi  r2,r2,r6
        rsb r3,r3,#0   @ make screen pitch negative
        mov r14,#0xde    @ // need this constant (f7de) for pixel averaging
        orr r14,r14,#0xf700
        orr r14,r14,r14,LSL #16    @ f7def7de
        mov r12,#0            @ y counter
blt150s270_top:   
        mul r4,r12,r1        @ pBitmap[y*iPitch]
        add r4,r4,r0        @ source pointer read
        ldr r6,[sp,#40]        @ get width
        add r5,r12,r12,LSL #1    @ y*3/2
@        mov r5,r5,LSR #1
@        mul r10,r5,r3        @ pVideo[((y*3)/2) * iScreenPitch]
@        add r5,r10,r2        @ destination pointer ready
        orrs r3,r3,r3       @ 270 or 90 degrees?
        addmi r5,r2,r5      @ + (y*3)/2 * 2  270
        subpl r5,r2,r5      @ - (y*3)/2 * 2   90
        add r11,r5,r3,LSL #1  @ destination + 2 scanlines
        mov r6,r6,LSR #2    @ work on 4 pixels at a time (4 become 6)
blt150s270_xloop0:   
        pld [r4,r1]
        pld [r4,#32]
        ldmia r4!,{r7,r8}    @ grab 4 source pixels (ul1,ul3)
        and r9,r7,r14        @ prepare pixels to be averaged
        mov r10,r9,LSR #17    @ ul2 = ul1 >> 16
        add r9,r10,r9,LSR #1    @ ul = (us1 >> 1) + (us2 >> 1)
        strh r7,[r5]         @ pixel 0
@        mov r11,r7,LSL #16    @ & 0xffff
@        mov r11,r11,LSR #16    @ get original pixel 0
@        orr r11,r11,r9,LSL #16    @ pair = (ul << 16) | ul1
@        str r11,[r5],#4        @ first pair
      strh r9,[r5,r3]   @ pixel 1
@        mov r10,r8,LSL #16    @ lower half of ul3
@        orr r7,r10,r7,LSR #16    @ isolate ul2 and form second pair
@        str r7,[r5],#4        @ second pair
      mov r10,r7,LSR #16
      strh r10,[r11]    @ pixel 2
      strh r8,[r11,r3]  @ pixel 3
      
      add r5,r5,r3,LSL #2  @ advance dest 4 pixels
      
        and r9,r8,r14        @ prepare for averaging
        mov r10,r9,LSR #17    @ ulA
        add r10,r10,r9,LSR #1    @ average the 2
      strh r10,[r5]     @ pixel 4    
@        mov r10,r10,LSL #16    @ & 0xffff
@        mov r10,r10,LSR #16
        mov r8,r8,LSR #16    @ prepare upper half of second pixel pair
@        orr r10,r10,r8,LSL #16    @ combine 2 pixels
@        str r10,[r5],#4        @ third pair
      strh r8,[r5,r3]   @ pixel 5
      add r5,r5,r3,LSL #1  @ advance dest 2 more pixels
      add r11,r11,r3,LSL #2   @ advance offset pointer 6 pixels
      add r11,r11,r3,LSL #1
        subs r6,r6,#1        @ loop count
        bne blt150s270_xloop0
        ldr r6,[sp,#40]        @ get width
        sub r4,r4,r6,LSL #1        @ rescan the first row
        mov r10,#3
        mul r8,r6,r10        @ x*3
        mov r8,r8,LSR #1    @ x * 1.5
        mov r6,r6,LSR #2        @ do it again
        mul r10,r8,r3
        orrs r3,r3,r3        @ 90 or 270?
        submi r10,r10,#2     @ pitch * X * 1.5 + 2
        addpl r10,r10,#2
        sub r5,r5,r10
        sub r11,r11,r10
blt150s270_xloop1:   
        ldr r8,[r4,r1]        @ read from the line below
        ldr r7,[r4],#4        @ and the current
        and r8,r8,r14        @ all pixels get averaged
        and r7,r7,r14
        mov r9,r7,LSR #1    @ prepare for averaging
        add r9,r9,r8,LSR #1    @ average 2 pixels at once (ulA)
        and r10,r9,r14        @ averge these 2 pixels together
        mov r7,r10,LSR #1  @ ulB = (ulA avg ulA>>16)
        add r7,r7,r10,LSR #17    @ ulB ready
        strh r9,[r5]   @ pixel 0
        strh r7,[r5,r3]  @ pixel 1
@        mov r7,r9,LSL #16    @ get lower portion
@        mov r7,r7,LSR #16
@        orr r7,r7,r11,LSL #16    @ ulA | (ulB<<16)
@        str r7,[r5],#4        @ first pair
        ldr r8,[r4,r1]        @ ul1a = first pair
        ldr r7,[r4],#4        @ ul3 = second pair
        and r8,r8,r14        @ all get averaged
        and r7,r7,r14
        mov r10,r7,LSR #1    @ prepare to avg
        add r10,r10,r8,LSR #1    @ average a pair of pixels in one shot (ulC)
        and r8,r10,r14        @ need to average the 2 halves of ulC
        mov r7,r8,LSR #17    @ average the 2 halves
        add r7,r7,r8,LSR #1    @ ulD = ulC avg (ulC >> 16)
@        mov r7,r7,LSL #16    @ ulD &= 0xffff
@        mov r7,r7,LSR #16
        mov r8,r9,LSR #16    @ ulA >> 16
        strh r8,[r11]     @ pixel 2
        strh r10,[r11,r3] @ pixel 3
        add r5,r5,r3,LSL #2  @ advance dest 4 pixels
@        orr r8,r8,r10,LSL #16    @ pixels = (ulA >> 16) | (ulC << 16)
@        str r8,[r5],#4        @ second pair
        mov r10,r10,LSR #16    @ use the upper half of ulC
        strh r10,[r5]
        strh r7,[r5,r3]
@        orr r7,r7,r10,LSL #16    @ prepare last pair
@        str r7,[r5],#4        @ third pair
      add r5,r5,r3,LSL #1  @ advance dest 2 pixels
      add r11,r11,r3,LSL #2   @ advance other pointer 6 pixels
      add r11,r11,r3,LSL #1
      
        subs r6,r6,#1        @ loop count
        bne blt150s270_xloop1
        ldr r6,[sp,#40]        @ width
        sub r4,r4,r6,LSL #1    @ back to start of src row
        add r4,r4,r1        @ psrc += ipitch = next row
        mov r10,#3
        mul r8,r6,r10        @ X * 3
        mov r8,r8,LSR #1    @ X * 1.5
        mov r6,r6,LSR #2    @ final loop (same as loop 0)
        mul r10,r8,r3
        orrs r3,r3,r3        @ 90 or 270?
        submi r10,r10,#2     @ pitch*X*1.5 + 2
        addpl r10,r10,#2
        sub r5,r5,r10
        sub r11,r11,r10
blt150s270_xloop2:   
        ldmia r4!,{r7,r8}    @ grab 4 source pixels (ul1,ul3)
        and r9,r7,r14        @ prepare pixels to be averaged
        mov r10,r9,LSR #17    @ ul2 = ul1 >> 16
        add r9,r10,r9,LSR #1    @ ul = (us1 >> 1) + (us2 >> 1)
        strh r7,[r5]      @ pixel 0
        strh r9,[r5,r3]   @ pixel 1
@        mov r11,r7,LSL #16    @ & 0xffff
@        mov r11,r11,LSR #16    @ get original pixel 0
@        orr r11,r11,r9,LSL #16    @ pair = (ul << 16) | ul1
@        str r11,[r5],#4        @ first pair
      mov r7,r7,LSR #16
      strh r7,[r11]  @ pixel 2
      strh r8,[r11,r3]  @ pixel 3
@        mov r11,r8,LSL #16    @ lower half of ul3
@        orr r7,r11,r7,LSR #16    @ isolate ul2 and form second pair
@        str r7,[r5],#4        @ second pair
        add r5,r5,r3,LSL #2  @ advance dest 4 pixels
        and r9,r8,r14        @ prepare for averaging
        mov r10,r9,LSR #17    @ ulA
        add r10,r10,r9,LSR #1    @ average the 2
        strh r10,[r5]    @ pixel 4
@        mov r10,r10,LSL #16    @ & 0xffff
@        mov r10,r10,LSR #16
        mov r8,r8,LSR #16    @ prepare upper half of second pixel pair
        strh r8,[r5,r3]   @ pixel 5
@        orr r10,r10,r8,LSL #16    @ combine 2 pixels
@        str r10,[r5],#4        @ third pair
   
      add r5,r5,r3,LSL #1  @ advance dest another 2 pixels
      add r11,r11,r3,LSL #2   @ 6 pixels for other pointer
      add r11,r11,r3,LSL #1
      
        subs r6,r6,#1        @ loop count
        bne blt150s270_xloop2
        ldr r10,[sp,#44]        @ height
        add r12,r12,#2          @ y += 2
        cmp    r12,r10                @ reached the end?
        bne blt150s270_top     @ loop through entire image
        ldmfd   sp!, {r4-r12,pc}    @ return
  .end    
