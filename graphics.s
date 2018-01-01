#ifdef __arm__
@      TITLE("Gen_Graphics")
@++
@
@ Graphics routines for Genesis emulator
@ written by Larry Bank
@ Copyright 2005-2017 BitBank Software, Inc.
@ Project started 1/14/05
@
@ 1/14/05 - first pass
@ 1/20/05 - optimization
@ 3/3/07 - more optimization (STR pipeline stalls taken into account)
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
  .global  DrawLine
  .global  DrawSimpleLine
  .global  DrawPriorityLine
  .global  ARMDrawLayerB  
  .global  ARMDrawLayerA  
  .global  ARM816FAST  
  .global  ARMDrawSprite  
  .global  ARMDrawNESTiles  
  .global  ARMDrawGGTiles  
  .global  ARMDrawGBCTiles  
  .global  ARMDrawPCETiles  
  .global  ARMDrawCoinOpSprite16
  .global  ARMDrawCoinOpTile16
  .global  G88DrawCharOpaque
@
@ call from C as void void G88DrawCharOpaque(int sx,int sy,int iChar,int iColor,unsigned char *pData, int iPitch, unsigned char *pBitmap)
@ draw an 8x8 fully opaque character
@
G88DrawCharOpaque:
  stmfd sp!,{r4-r12,lr}
  ldr r4,[sp,#40]       @ source data
  ldr r5,[sp,#44]       @ pitch
  ldr r6,[sp,#48]       @ dest data
  add r4,r4,r2,LSL #6   @ point to correct source image (64 * iChar)
  mul r7,r1,r5          @ pDest += y * iPitch
  add r6,r6,r0,LSL #1   @ pDest += x*2
  add r6,r6,r7
  mov r0,#0xff          @ mask to extract pixels
  mov r14,#8            @ number of lines to do
  tst r6,#3                     @ dword aligned address?
  bne g88drawslow
g88drawfast:
  ldmia r4!,{r7,r8}     @ read 8 source pixels
  and r9,r0,r7          @ first pixel
  and r10,r0,r7,LSR #8  @ second pixel
  add r9,r9,r3          @ + color
  add r10,r10,r3
  and r11,r0,r7,LSR #16 @ third pixel
  orr r9,r9,r10,LSL #16 @ combine first 2 pixels
  and r10,r0,r7,LSR #24 @ forth pixel
  add r11,r11,r3        @ + color
  add r10,r10,r3
  orr r10,r11,r10,LSL #16       @ combine second 2 pixels
  and r1,r0,r8          @ fifth pixel
  and r2,r0,r8,LSR #8   @ sixth pixel
  add r1,r1,r3          @ + color
  add r2,r2,r3
  and r12,r0,r8,LSR #16 @ seventh pixel
  orr r11,r1,r2,LSL #16 @ combine pixels 5+6
  and r1,r0,r8,LSR #24  @ eighth pixel
  add r12,r12,r3                @ + color
  add r1,r1,r3
  orr r12,r12,r1,LSL #16        @ combine pixels 7+8
  stmia r6,{r9-r12}             @ store 8 pixels
  add r6,r6,r5          @ pDest += iPitch
  subs r14,r14,#1               @ while (--y)
  bne g88drawfast
g88drawexit:
  ldmfd sp!,{r4-r12,pc}

@ need to draw it 1 pixel at a time
g88drawslow:
  ldmia r4!,{r7,r8}     @ read 8 source pixels
  and r9,r0,r7          @ first pixel
  and r10,r0,r7,LSR #8  @ second pixel
  add r9,r9,r3          @ + color
  add r10,r10,r3
  strh r9,[r6],#2
  strh r10,[r6],#2
  and r11,r0,r7,LSR #16 @ third pixel
  and r10,r0,r7,LSR #24 @ forth pixel
  add r11,r11,r3        @ + color
  add r10,r10,r3
  strh r11,[r6],#2
  strh r10,[r6],#2
  and r1,r0,r8          @ fifth pixel
  and r2,r0,r8,LSR #8   @ sixth pixel
  add r1,r1,r3          @ + color
  add r2,r2,r3
  strh r1,[r6],#2
  strh r2,[r6],#2
  and r12,r0,r8,LSR #16 @ seventh pixel
  and r1,r0,r8,LSR #24  @ eighth pixel
  add r12,r12,r3                @ + color
  add r1,r1,r3
  strh r12,[r6],#2
  strh r1,[r6],#2
  add r6,r6,r5          @ pDest += iPitch
  sub r6,r6,#16
  subs r14,r14,#1               @ while (--y)
  bne g88drawslow
  b g88drawexit

@
@ call from C as void ARMDrawCoinOpTile16(unsigned char *p, unsigned char *d, int xCount, int yCount, int iColor, int iCoinOpPitch, int iSize);
@
ARMDrawCoinOpTile16:
  stmfd sp!,{r4-r10}
  ldr r6,[sp,#28]       @ color offset
  ldr r7,[sp,#32]       @ dest pitch
  ldr r8,[sp,#36]       @ source pitch
drawcotile00:
  mov r10,r2    @ xcount
drawcotile01:
  ldrb r9,[r0],#1       @ source pixel
  subs r10,r10,#1       @ while (xcount--)
  add r9,r9,r6  @ c + color
  strh r9,[r1],#2       @ if !(ulTransMask & 1<<c)
  bne drawcotile01
@ prepare for next line
  add r0,r0,r8          @ p += iSize
  sub r0,r0,r2          @ -xCount
  add r1,r1,r7          @ d += iPitch
  sub r1,r1,r2,LSL #1   @ adjust for bytes written
  subs r3,r3,#1         @ while (ycount--)
  bne drawcotile00
  ldmfd sp!,{r4-r10}
  bx lr

@
@ call from C as void ARMDrawCoinOpSprite16(unsigned char *p, unsigned char *d, int xCount, int yCount, unsigned long ulTransMask, int iColor, int iCoinOpPitch, int iSize);
@
ARMDrawCoinOpSprite16:
  stmfd sp!,{r5-r11}
  ldr r5,[sp,#28]       @ transmask
  ldr r6,[sp,#32]       @ color offset
  ldr r7,[sp,#36]       @ dest pitch
  ldr r8,[sp,#40]       @ source pitch
  mov r11,#1            @ test bit
drawcospr00:
  mov r10,r2    @ xcount
drawcospr01:
  ldrb r9,[r0],#1       @ source pixel
  tst r5,r11,LSL r9     @ transparent color
  add r9,r9,r6  @ c + color
  streqh r9,[r1]        @ if !(ulTransMask & 1<<c)
  add r1,r1,#2
  subs r10,r10,#1       @ while (xcount--)
  bne drawcospr01
@ prepare for next line
  add r0,r0,r8          @ p += iSize
  sub r0,r0,r2          @ -xCount
  add r1,r1,r7          @ d += iPitch
  sub r1,r1,r2,LSL #1   @ adjust for bytes written
  subs r3,r3,#1         @ while (ycount--)
  bne drawcospr00
  ldmfd sp!,{r5-r11}
  bx lr

@
@ Draw a line of 8 pixel (4bpp) files for NEC PC-Engine
@ call from C as unsigned char *ARMDrawPCETiles(unsigned short *pSrc, unsigned short *pEnd, unsigned char *pVRAM, unsigned char *pDest)@
@
ARMDrawPCETiles:   
         stmfd sp!,{r4-r12,lr}
         mov r5,#-1     @ last tile drawn
         mov r14,#0x1
         orr r14,r14,r14,LSL #8   @ create bitmask for 0x1010101
         orr r14,r14,r14,LSL #16
pcefast_top:   
         ldrh r4,[r0],#2   @ get a tile
         cmp  r4,r5        @ same as last one?
         beq  pcefast_output
         mov  r5,r4        @ keep for next time
         and  r9,r4,#0xf000   @ top 4 bits are color palette
         orr  r9,r9,r9,LSR #8
         orr  r9,r9,r9,LSL #16   @ now all 4 bytes have the color
         mov  r10,r9       @ get color in both output words (L+R)
         mov  r8,r4,LSL #21    @ mask off the tile number
         mov  r8,r8,LSR #16
         ldrh r7,[r2,r8]  @ read the tile data (planes 0 and 1)
         add  r8,r8,#0x10 @ point to second set of plane data
         ldrh r8,[r2,r8] @ read planes 2 and 3
@ process bit plane 0
         and r6,r7,#0xff   @ get bit plane 0
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,LSL #18
         and r4,r14,r6,LSR #7 @ mask off the upper nibble pixels we want
         and r6,r14,r6,LSR #3 @ mask off the lower nibble
         orr r9,r9,r4      @ add bit plane 0 (left)
         orr r10,r10,r6      @ ditto (right)
@ process bit plane 1
         and r6,r7,#0xff00   @ get bit plane 1
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,ROR #14
         and r4,r14,r6,ROR #15 @ mask off the upper nibble pixels we want
         and r6,r14,r6,ROR #11 @ mask off the lower nibble
         orr r9,r9,r4,LSL #1      @ add bit plane 1 (left)
         orr r10,r10,r6,LSL #1      @ ditto (right)
@ process bit plane 2
         and r6,r8,#0xff   @ get bit plane 2
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,LSL #18
         and r4,r14,r6,LSR #7 @ mask off the upper nibble pixels we want
         and r6,r14,r6,LSR #3 @ mask off the lower nibble
         orr r9,r9,r4,LSL #2      @ add bit plane 2 (left)
         orr r10,r10,r6,LSL #2      @ ditto (right)
@ process bit plane 3
         and r6,r8,#0xff00   @ get bit plane 3
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,ROR #14
         and r4,r14,r6,ROR #15 @ mask off the upper nibble pixels we want
         and r6,r14,r6,ROR #11 @ mask off the lower nibble
         orr r9,r9,r4,LSL #3      @ add bit plane 3 (left)
         orr r10,r10,r6,LSL #3      @ ditto (right)
pcefast_output:   
         tst r3,#3      @ dword-aligned output?
         bne pceslow_output
         stmia r3!,{r9,r10}
         cmp r0,r1      @ done?
         bne pcefast_top
         mov   r0,r3    @ keep output pointer       
         ldmia sp!,{r4-r12,pc}
         
pceslow_output:   
         strb r9,[r3],#1
         mov r9,r9,LSR #8
         strb r9,[r3],#1
         mov r9,r9,LSR #8
         strb r9,[r3],#1
         mov r9,r9,LSR #8
         strb r9,[r3],#1
         strb r10,[r3],#1
         mov r10,r10,LSR #8
         strb r10,[r3],#1
         mov r10,r10,LSR #8
         strb r10,[r3],#1
         mov r10,r10,LSR #8
         strb r10,[r3],#1
         
         cmp   r0,r1
         bne   pcefast_top
         mov   r0,r3    @ keep output pointer       
         ldmia sp!,{r4-r12,pc}
@
@ Draw a line of 8 pixel (2bpp) tiles for Nintendo GameBoy
@ call from C as void ARMDrawGBCTiles(unsigned char *pTile, unsigned char *pEdge, unsigned char *pSrc, unsigned char ucLCDC, unsigned char *pDest, int iWidth>>3)@
@
ARMDrawGBCTiles:   
         stmfd sp!,{r4-r12,lr}
         ldr r4,[sp,#40]    @ destination
         ldr r12,[sp,#44]   @ get the tile count
         mov r5,#-1     @ last tile drawn + attribute
         mov r14,#0x1
         orr r14,r14,r14,LSL #8   @ create bitmask for 0x1010101
         orr r14,r14,r14,LSL #16
gbcfast_top:   
         cmp   r0,r1
         subge r0,r0,#32   @ wrap around this line of tiles
         add  r7,r0,#0x2000    @ offset too big to fit in 12 bits
         tst  r3,#0x10        @ tile data select
         ldrb r7,[r7]   @ get the attribute
         ldreqsb r8,[r0],#1
         ldrneb r8,[r0],#1    @ get tile number
         addeq r8,r8,#256
         tst  r7,#8        @ second tile bank?
         addne r8,r8,#512
         orr  r7,r7,r8,LSL #8	@ combine R7/R8 to compare tile for next time
         cmp  r5,r7        @ tile same as last time?
         beq  gbcfast_output    @ same, just output it
gbc_newtile:   
         mov  r5,r7        @ save for comparison
         tst  r7,#0x40    @ vertical flip?
         eorne r2,r2,#0xe
         mov  r8,r8,LSL #4    @ shift it over to index the tile image data
         ldrh r8,[r2,r8]    @ get bitplane 0 and 1
         eorne r2,r2,#0xe    @ fix the source tile addr for next time
         tst  r7,#0x20        @ horizontal flip?
         and  r9,r7,#0x80    @ isolate priority bit
         and  r7,r7,#7        @ isolate color palette bits
         orr  r9,r9,r7,LSL #2    @ get them in proper position
         orr  r9,r9,r9,LSL #8    @ fill all 4 bytes with the color
         orr  r9,r9,r9,LSL #16
         mov  r10,r9        @ now r9 and r10 are ready to add pixels
       bne  gbcfast_flipped
@ process bit plane 0
       and r7,r8,#0xff   @ get bit plane 0
       orr r7,r7,r7,LSL #9
       orr r7,r7,r7,LSL #18
       and r11,r14,r7,LSR #7 @ mask off the upper nibble pixels we want
       and r7,r14,r7,LSR #3 @ mask off the lower nibble
       orr r9,r9,r11      @ add bit plane 0 (left)
       orr r10,r10,r7      @ ditto (right)
@ process bit plane 1
       and r7,r8,#0xff00   @ get bit plane 1
       orr r7,r7,r7,LSL #9
       orr r7,r7,r7,ROR #14
       and r8,r14,r7,ROR #15 @ mask off the upper nibble pixels we want
       and r7,r14,r7,ROR #11 @ mask off the lower nibble
       orr r9,r9,r8,LSL #1      @ add bit plane 1 (left)
       orr r10,r10,r7,LSL #1      @ ditto (right)
       b   gbcfast_output
gbcfast_flipped:   
@ process bit plane 0
       and r7,r8,#0xff   @ get bit plane 0
       orr r7,r7,r7,LSL #7
       orr r7,r7,r7,LSL #14
       and r11,r14,r7,ROR #4  @ mask off the upper nibble pixels we want
       and r7,r14,r7,ROR #0  @ mask off the lower nibble
       orr r9,r9,r7      @ add bit plane 0 (left)
       orr r10,r10,r11      @ ditto (right)
@ process bit plane 1
       and r7,r8,#0xff00   @ get bit plane 1
       orr r7,r7,r7,LSL #7
       orr r7,r7,r7,ROR #18
       and r11,r14,r7,ROR #12 @ mask off the upper nibble pixels we want
       and r7,r14,r7,ROR #8  @ mask off the lower nibble
       orr r9,r9,r7,LSL #1      @ add bit plane 1 (left)
       orr r10,r10,r11,LSL #1      @ ditto (right)
gbcfast_output:   
         stmia r4!,{r9,r10} @ draw 8 pixels
         subs r12,r12,#1   @ decrement count
         bne  gbcfast_top         
         
         ldmia sp!,{r4-r12,pc}
@
@ Draw a line of 8 pixel (4bpp) tiles for Sega GameGear
@ call from C as void ARMDrawGGTiles(unsigned short *pTile, unsigned short *pEdge, unsigned long *pSrc, unsigned long *pDest, int iWidth)@
@
ARMDrawGGTiles:   
         stmfd sp!,{r4-r12,lr}
         mov r14,#0xff  @ need to create a mask for the tile #
         add r14,r14,#0x100
         ldr r12,[sp,#40]   @ get the tile count
         mov r5,#-1     @ last tile drawn
         mov r11,#0x1
         orr r11,r11,r11,LSL #8   @ create bitmask for 0x1010101
         orr r11,r11,r11,LSL #16
ggfast_top:   
         cmp   r0,r1
         subge r0,r0,#64   @ wrap around this line of tiles
         ldrh r4,[r0],#2   @ get a tile
         cmp  r4,r5        @ same as last tile
         beq  ggfast_output   @ draw the same pixels
         mov  r5,r4        @ keep for comparison the next time through
         and  r9,r4,#0x1800   @ get the 2 color palette bits
         mov  r9,r9,LSR #7 @ slide color into correct position
         orr  r9,r9,r9,LSL #8
         orr  r9,r9,r9,LSL #16   @ now all 4 bytes have the color
         mov  r10,r9       @ get color in both output words (L+R)
         tst  r4,#0x400    @ vertically flipped?
         and  r8,r4,r14    @ mask off the tile number
         moveq r7,r2       @ original tile data pointer
         eorne r7,r2,#0x1c @ XOR with 7
         ldr  r7,[r7,r8,LSL #5]  @ read the tile data (4 bytes of separated bitplanes)
         tst  r4,#0x200    @ horizontally flipped?
         bne  ggfast_flipped
@ process bit plane 0
         and r6,r7,#0xff   @ get bit plane 0
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,LSL #18
         and r8,r11,r6,LSR #7 @ mask off the upper nibble pixels we want
         and r6,r11,r6,LSR #3 @ mask off the lower nibble
         orr r9,r9,r8      @ add bit plane 0 (left)
         orr r10,r10,r6      @ ditto (right)
@ process bit plane 1
         and r6,r7,#0xff00   @ get bit plane 1
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,ROR #14
         and r8,r11,r6,ROR #15 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #11 @ mask off the lower nibble
         orr r9,r9,r8,LSL #1      @ add bit plane 1 (left)
         orr r10,r10,r6,LSL #1      @ ditto (right)
@ process bit plane 2
         and r6,r7,#0xff0000   @ get bit plane 2
         orr r6,r6,r6,ROR #23
         orr r6,r6,r6,ROR #14
         and r8,r11,r6,ROR #23 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #19 @ mask off the lower nibble
         orr r9,r9,r8,LSL #2      @ add bit plane 2 (left)
         orr r10,r10,r6,LSL #2      @ ditto (right)
@ process bit plane 3
         and r6,r7,#0xff000000   @ get bit plane 3
         orr r6,r6,r6,ROR #23
         orr r6,r6,r6,ROR #14
         and r8,r11,r6,ROR #31 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #27 @ mask off the lower nibble
         orr r9,r9,r8,LSL #3      @ add bit plane 3 (left)
         orr r10,r10,r6,LSL #3      @ ditto (right)
         b   ggfast_output
ggfast_flipped:   
@ process bit plane 0
         and r6,r7,#0xff   @ get bit plane 0
         orr r6,r6,r6,LSL #7
         orr r6,r6,r6,LSL #14
         and r8,r11,r6,ROR #4  @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #0  @ mask off the lower nibble
         orr r9,r9,r6      @ add bit plane 0 (left)
         orr r10,r10,r8      @ ditto (right)
@ process bit plane 1
         and r6,r7,#0xff00   @ get bit plane 1
         orr r6,r6,r6,LSL #7
         orr r6,r6,r6,ROR #18
         and r8,r11,r6,ROR #12 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #8  @ mask off the lower nibble
         orr r9,r9,r6,LSL #1      @ add bit plane 1 (left)
         orr r10,r10,r8,LSL #1      @ ditto (right)
@ process bit plane 2
         and r6,r7,#0xff0000   @ get bit plane 2
         orr r6,r6,r6,LSL #7
         orr r6,r6,r6,ROR #18
         and r8,r11,r6,ROR #20 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #16 @ mask off the lower nibble
         orr r9,r9,r6,LSL #2      @ add bit plane 2 (left)
         orr r10,r10,r8,LSL #2      @ ditto (right)
@ process bit plane 3
         and r6,r7,#0xff000000   @ get bit plane 3
         orr r6,r6,r6,ROR #25
         orr r6,r6,r6,ROR #18
         and r8,r11,r6,ROR #28 @ mask off the upper nibble pixels we want
         and r6,r11,r6,ROR #24 @ mask off the lower nibble
         orr r9,r9,r6,LSL #3      @ add bit plane 3 (left)
         orr r10,r10,r8,LSL #3      @ ditto (right)
ggfast_output:   
         stmia r3!,{r9,r10} @ draw 8 pixels
         subs r12,r12,#1   @ decrement count
         bne  ggfast_top         
         
         ldmia sp!,{r4-r12,pc}
@
@ Draw a line of 8 pixel tiles for NES
@ call from C as void ARMDrawNESTiles(unsigned long *pTiles, int iScanOffset, unsigned long *pDest)@
@
ARMDrawNESTiles:   
         stmfd sp!,{r4-r12,lr}
         add r14,r1,#8  @ save 1 clock per cycle by pointing to tile data
         mov r12,#33    @ number of loops
         mov r11,#-1    @ force first tile to be loaded
         mov r10,#0x1
         orr r10,r10,r10,LSL #8  @ prepare pixel mask
         orr r10,r10,r10,LSL #16
drawnes_top:   
         ldr r3,[r0],#4 @ get 1 tile info
         cmp r3,r11     @ same as last tile?
         beq drawnes_output
         mov r11,r3     @ keep for next comparison
         and r9,r3,#0xc @ get color
         orr r9,r9,r9,LSL #8
         orr r9,r9,r9,LSL #16 @ put in all 4 bytes
         and r3,r3,#0xfffffff0   @ get address of tile data
         ldrb r6,[r3,r1]   @ get LSBs
         ldrb r7,[r3,r14]  @ get MSBs
         orr r6,r6,r6,LSL #9
         orr r6,r6,r6,LSL #18
         and r8,r10,r6,LSR #7 @ mask off the upper nibble pixels we want
         and r6,r10,r6,LSR #3 @ mask off the lower nibble
         orr r4,r9,r8      @ add the color
         orr r5,r9,r6      @ ditto
         orr r7,r7,r7,LSL #9
         orr r7,r7,r7,LSL #18
         and r6,r10,r7,LSR #7 @ mask off the upper nibble pixels we want
         and r8,r10,r7,LSR #3 @ mask off the lower nibble
         orr r4,r4,r6,LSL #1
         orr r5,r5,r8,LSL #1
         
drawnes_output:   
         stmia r2!,{r4,r5} @ store 8 pixels
         subs  r12,r12,#1  @ decrement count
         bne   drawnes_top
         
         ldmia sp!,{r4-r12,pc}
@
@ Draw a 8x8 sprite tile
@ call from C as void ARMDrawSprite(unsigned char *pBits, unsigned char *pDest, unsigned short usTile, int iPitch)@
@
ARMDrawSprite:   
         stmfd sp!,{r4-r12,lr}
         tst r2,#0x1000 @ flipy?
         moveq r14,#4      @ increment = +4
         movne r14,#-4     @ increment = -4
         addne r0,r0,#28   @ point to bottom of tile data
         mov r4,#8      @ ycount
         mov r12,r2,LSR #9
         and r12,r12,#0x70 @ isolate color (and priority bit)
         mov r11,#0xf   @ for quick pixel extraction
         tst r2,#0x8000 @ low priority sprite?
         beq sprite_low @ draw it as low priority
         tst r2,#0x800  @ flipx
         bne sprite_flipx
sprite_top:   
         ldr r5,[r0],r14    @ get 8 pixels
@         add r0,r0,r14   @ advance source pointer
         mov r6,r5,LSR #16 @ see if anything to do in first 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_part2  @ first 4 pixels are transparent
         ands r6,r11,r5,LSR #12   @ first pixel (really 4th)
         orrne r6,r6,r12   @ add color
         strneb r6,[r1]    @ lsb
         ands r6,r11,r5,LSR #8
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#1]
         ands r6,r11,r5,LSR #4
         orrne r6,r6,r12
         strneb r6,[r1,#2]
         ands r6,r11,r5
         orrne r6,r6,r12
         strneb r6,[r1,#3]
sprite_part2:   
         mov r6,r5,LSL #16 @ see if anything to do in last 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_bot @ nope, skip them
         ands r6,r11,r5,LSR #28   @ first pixel (really 4th)
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#4]    @ lsb
         ands r6,r11,r5,LSR #24
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#5]
         ands r6,r11,r5,LSR #20
         orrne r6,r6,r12
         strneb r6,[r1,#6]
         ands r6,r11,r5,LSR #16
         orrne r6,r6,r12
         strneb r6,[r1,#7]
sprite_bot:   
         add r1,r1,r3   @ point to next line
         subs r4,r4,#1  @ y--
         bne  sprite_top
         ldmia sp!,{r4-r12,pc} @ time to go
sprite_flipx:   
         ldr r5,[r0]    @ get 8 pixels
         add r0,r0,r14   @ advance source pointer
         mov r6,r5,LSL #16 @ see if anything to do in first 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_part2flip  @ first 4 pixels are transparent
         ands r6,r11,r5,LSR #16   @ first pixel (really 4th)
         orrne r6,r6,r12   @ add color
         strneb r6,[r1]    @ lsb
         ands r6,r11,r5,LSR #20
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#1]
         ands r6,r11,r5,LSR #24
         orrne r6,r6,r12
         strneb r6,[r1,#2]
         ands r6,r11,r5,LSR #28
         orrne r6,r6,r12
         strneb r6,[r1,#3]
sprite_part2flip:   
         mov r6,r5,LSR #16 @ see if anything to do in last 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_botflip @ nope, skip them
         ands r6,r11,r5   @ first pixel (really 4th)
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#4]    @ lsb
         ands r6,r11,r5,LSR #4
         orrne r6,r6,r12   @ add color
         strneb r6,[r1,#5]
         ands r6,r11,r5,LSR #8
         orrne r6,r6,r12
         strneb r6,[r1,#6]
         ands r6,r11,r5,LSR #12
         orrne r6,r6,r12
         strneb r6,[r1,#7]
sprite_botflip:   
         add r1,r1,r3   @ point to next line
         subs r4,r4,#1  @ y--
         bne  sprite_flipx
         ldmia sp!,{r4-r12,pc}
@ low priority sprite drawing
sprite_low:   
         tst r2,#0x800     @ flipx?
         bne sprite_low_flipx
sprite_low_top:   
         ldr r5,[r0] @ get 8 pixels
         add r0,r0,r14     @ advance source pointer
         mov r6,r5,LSR #16 @ see if anything to do in first 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_low_part2 @ first 4 pixels are transparent
         ands r6,r11,r5,LSR #12  @ first pixel
         beq  sprite_low1  @ skip this pixel
         ldrb r10,[r1]   @ see if destination pixel is higher priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1]    @ nope, we can draw the pixel
sprite_low1:   
         ands r6,r11,r5,LSR #8  @ second pixel
         beq  sprite_low2  @ skip this pixel
         ldrb r10,[r1,#1]  @ see if destination pixel is higher priority
         orr  r6,r6,r12    @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#1] @ nope, we can draw the pixel
sprite_low2:   
         ands r6,r11,r5,LSR #4  @ third pixel
         beq  sprite_low3  @ skip this pixel
         ldrb r10,[r1,#2]  @ see if destination pixel is higher priority
         orr  r6,r6,r12    @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#2] @ nope, we can draw the pixel
sprite_low3:   
         ands r6,r11,r5    @ forth pixel
         beq  sprite_low_part2  @ skip this pixel
         ldrb r10,[r1,#3]  @ see if destination pixel is higher priority
         orr  r6,r6,r12    @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#3] @ nope, we can draw the pixel
sprite_low_part2:   
         mov r6,r5,LSL #16 @ see if anything to do in last 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_low_bot @ nope, skip them
         ands r6,r11,r5,LSR #28   @ first pixel (really 5th)
         beq  sprite_low5
         ldrb r10,[r1,#4]  @ see if destination pixel is higher priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#4]
sprite_low5:   
         ands r6,r11,r5,LSR #24   @ second pixel (really 6th)
         beq  sprite_low6
         ldrb r10,[r1,#5]  @ see if destination pixel is higher priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#5]
sprite_low6:   
         ands r6,r11,r5,LSR #20   @ third pixel (really 7th)
         beq  sprite_low7
         ldrb r10,[r1,#6]  @ see if destination pixel is higher priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#6]
sprite_low7:   
         ands r6,r11,r5,LSR #16   @ forth pixel (really 8th)
         beq  sprite_low_bot
         ldrb r10,[r1,#7]  @ see if destination pixel is higher priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority?
         streqb r6,[r1,#7]
sprite_low_bot:   
         add r1,r1,r3   @ point to next line
         subs r4,r4,#1  @ y--
         bne  sprite_low_top
         ldmia sp!,{r4-r12,pc} @ time to go
sprite_low_flipx:   
         ldr r5,[r0]    @ get 8 pixels
         add r0,r0,r14   @ advance source pointer
         mov r6,r5,LSL #16 @ see if anything to do in first 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_low_part2flip  @ first 4 pixels are transparent
         ands r6,r11,r5,LSR #16   @ first pixel (really 4th)
         beq  sprite_low_flip1
         ldrb r10,[r1]  @ test destination for priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority than our pixel?
         streqb r6,[r1]    @ nope
sprite_low_flip1:   
         ands r6,r11,r5,LSR #20   @ second pixel
         beq  sprite_low_flip2
         ldrb r10,[r1,#1]  @ test destination for priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority than our pixel?
         streqb r6,[r1,#1]    @ nope
sprite_low_flip2:   
         ands r6,r11,r5,LSR #24   @ second pixel
         beq  sprite_low_flip3
         ldrb r10,[r1,#2]  @ test destination for priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority than our pixel?
         streqb r6,[r1,#2]    @ nope
sprite_low_flip3:   
         ands r6,r11,r5,LSR #28   @ second pixel
         beq  sprite_low_part2flip
         ldrb r10,[r1,#3]  @ test destination for priority
         orr  r6,r6,r12   @ add color
         tst  r10,#0x40    @ higher priority than our pixel?
         streqb r6,[r1,#3]    @ nope
sprite_low_part2flip:   
         mov r6,r5,LSR #16 @ see if anything to do in last 4 pixels
         cmp r6,r5,ROR #16
         beq sprite_low_botflip @ nope, skip them
         ands r6,r11,r5   @ first pixel (really 4th)
         beq  sprite_low_flip5
         ldrb r10,[r1,#4]  @ test destination for priority
         orr r6,r6,r12   @ add color
         tst r10,#0x40     @ high priority underneath?
         streqb r6,[r1,#4]    @ lsb
sprite_low_flip5:   
         ands r6,r11,r5,LSR #4
         beq  sprite_low_flip6
         ldrb r10,[r1,#5]  @ test destination for priority
         orr r6,r6,r12   @ add color
         tst r10,#0x40     @ high priority underneath?
         streqb r6,[r1,#5]    @ lsb
sprite_low_flip6:   
         ands r6,r11,r5,LSR #8
         beq  sprite_low_flip7
         ldrb r10,[r1,#6]  @ test destination for priority
         orr r6,r6,r12   @ add color
         tst r10,#0x40     @ high priority underneath?
         streqb r6,[r1,#6]    @ lsb
sprite_low_flip7:   
         ands r6,r11,r5,LSR #12
         beq  sprite_low_botflip
         ldrb r10,[r1,#7]  @ test destination for priority
         orr r6,r6,r12   @ add color
         tst r10,#0x40     @ high priority underneath?
         streqb r6,[r1,#7]    @ lsb
sprite_low_botflip:   
         add r1,r1,r3   @ point to next line
         subs r4,r4,#1  @ y--
         bne  sprite_low_flipx
         ldmia sp!,{r4-r12,pc}
@
@ convert 8-bit to 16-bit pixels with dword aligned source and dest
@ call from C as ARM816FAST(unsigned long *pSrc, unsigned long *pulDest, unsigned short *usPalConvert, int iNewWidth)@
@ count is in 8-pixel blocks
@
ARM816FAST:   
         stmfd sp!,{r4-r11,lr}
         mov r11,#0xff  @ we need this constant for masking off pixels
         mov r11,r11,LSL #1   @ odd shifted constants are not allowed
         tst r0,#3      @ source is dword-aligned?
         bne arm816m_top   @ special case
arm816f_top:   
         ldmia r0!,{r4-r5}   @ get 8 source pixels
         pld [r0,#60]
         and r10,r11,r4,LSL #1
         ldrh r6,[r2,r10]  @ convert pixel 1
         and r10,r11,r4,LSR #7
         ldrh r8,[r2,r10]  @ convert pixel 2
         and r10,r11,r4,LSR #15
         and r4,r11,r4,LSR #23
         orr r6,r6,r8,LSL #16 @ combine pixels 1 and 2
         ldrh r7,[r2,r10]  @ convert pixel 3
         ldrh r8,[r2,r4]  @ convert pixel 4
         and r10,r11,r5,LSL #1
         orr r7,r7,r8,LSL #16 @ combine pixels 3 and 4
         ldrh r8,[r2,r10]  @ convert pixel 5
         and r10,r11,r5,LSR #7
         ldrh r9,[r2,r10]  @ convert pixel 6
         and r10,r11,r5,LSR #15
         orr r8,r8,r9,LSL #16@   combine pixels 5 and 6
         ldrh r9,[r2,r10]  @ convert pixel 7
         and r5,r11,r5,LSR #23
         ldrh r10,[r2,r5]  @ convert pixel 8
         subs r3,r3,#1     @ decrement count
         orr r9,r9,r10,LSL #16   @ combine pixels 7 and 8
         stmia r1!,{r6-r9}    @ store the 8 pixels to dest
         bne  arm816f_top  @ loop through entire line
         ldmia sp!,{r4-r11,pc}
@ case where source is NOT dword aligned
arm816m_top:   
         and  r7,r0,#0xfffffffc  @ dword align
         ldmia r7!,{r4-r6} @ read at least 8 pixels
         add   r0,r0,#8    @ advance source pointer
         and  r8,r0,#3    @ get amount of shift
         mov  r8,r8,LSL #3  @ times 8 bits per byte
         rsb  r10,r8,#32  @ get mask shift amount
         mov  r4,r4,LSR r8   @ shift first byte(s) into position
         mov  r7,r5,LSL r10   @ get bits to shift into first dword
         orr  r4,r4,r7     @ now r4 is complete
         mov  r5,r5,LSR r8   @ get second dword ready to receive partial contents
         mov  r7,r6,LSL r10   @ get bits to shift into second dword
         orr  r5,r5,r7     @ now r5 is complete
         
         and r10,r11,r4,LSL #1
         ldrh r6,[r2,r10]  @ convert pixel 1
         and r10,r11,r4,LSR #7
         ldrh r8,[r2,r10]  @ convert pixel 2
         and r10,r11,r4,LSR #15
         and r4,r11,r4,LSR #23
         orr r6,r6,r8,LSL #16 @ combine pixels 1 and 2
         ldrh r7,[r2,r10]  @ convert pixel 3
         ldrh r8,[r2,r4]  @ convert pixel 4
         and r10,r11,r5,LSL #1
         orr r7,r7,r8,LSL #16 @ combine pixels 3 and 4
         ldrh r8,[r2,r10]  @ convert pixel 5
         and r10,r11,r5,LSR #7
         ldrh r9,[r2,r10]  @ convert pixel 6
         and r10,r11,r5,LSR #15
         orr r8,r8,r9,LSL #16@   combine pixels 5 and 6
         ldrh r9,[r2,r10]  @ convert pixel 7
         and r5,r11,r5,LSR #23
         ldrh r10,[r2,r5]  @ convert pixel 8
         subs r3,r3,#1     @ decrement count
         orr r9,r9,r10,LSL #16   @ combine pixels 7 and 8
         stmia r1!,{r6-r9}    @ store the 8 pixels to dest
         bne  arm816m_top  @ loop through entire line
        
         ldmia sp!,{r4-r11,pc}
@
@ Call as void ARMDrawLayerB(unsigned long *pDest, unsigned long *pEnd, unsigned short *pTiles, unsigned short *pusEnd, int iScrollWidth, unsigned long *pSrc, unsigned long *pSrcFlipped, unsigned long ulBackColor)@
@
ARMDrawLayerB:   
         stmfd sp!,{r4-r12,lr}
         ldr   r6,[sp,#40] @ iScrollWidth * 2
         ldr   r7,[sp,#44] @ pSrc
         ldr   r14,[sp,#48] @ pSrcFlipped
         mov   r4,#0x800   @ tile number mask
         sub   r4,r4,#1
         mov   r8,#0xf    @ pixel mask
         orr   r4,r4,#0xff000000 @ old tile #
         orr   r4,r4,#0xff0000
layerb_top:   
         ldrh  r5,[r2],#2  @ get next tile
         cmp   r2,r3    @ past right edge?
         ldrge r6,[sp,#40]   @ iScrollWidth*2
         ldr   r9,[sp,#52] @ ulBackColor
         subge r2,r2,r6 @ pTiles -= iScrollWidth         
         cmp   r5,r4,LSR #16  @ tile == oldtile?
         beq   layerb_out  @ draw the same pixels
         mov   r4,r4,LSL #16  @ oldtile = tile
         mov   r4,r4,LSR #16
         orr   r4,r4,r5,LSL #16
         orrs  r5,r5,r5 @ anything to draw?
         moveq r11,r9  @ nope, draw background color
         moveq r12,r9
         beq   layerb_out  @ skip to next tile
         and   r10,r5,r4   @ mask off tile number
         tst   r5,#0x1000  @ flipy?
         ldreq r10,[r7,r10,LSL #5]  @ get tile data (8 4-bit pixels)
         ldrne r10,[r14,r10,LSL #5]  @ flipped data
         orrs  r10,r10,r10 @ anything to draw?
         moveq r11,r9  @ nope, draw background color
         moveq r12,r9
         beq   layerb_out
@ handle the 8 pixels
         and   r9,r9,#0xff @ get a single pixel of background color
         tst   r5,#0x800   @ flipx?
         mov   r5,r5,LSR #9   @ get pixel color
         and   r5,r5,#0x70    @ isolate color and priority bit
         orr   r5,r5,r5,LSL #8   @ put it in all 4 pixels
         orr   r11,r5,r5,LSL #16
         mov   r12,r11        @ left and right both have color now
         bne   layerb_flipx
         ands  r6,r8,r10,LSR #12   @ extract 4th pixel
         orrne r11,r11,r6  @ non-transparent pixel
         ands  r6,r8,r10,LSR #8 @ extract 3rd pixel
         orrne r11,r11,r6,LSL #8
         ands  r6,r8,r10,LSR #4 @ extract 2nd pixel
         orrne r11,r11,r6,LSL #16
         ands  r6,r8,r10     @ extract 1st pixel
         orrne r11,r11,r6,LSL #24
         ands  r6,r8,r10,LSR #28   @ extract 8th pixel
         orrne r12,r12,r6
         ands  r6,r8,r10,LSR #24   @ extract 7th pixel
         orrne r12,r12,r6,LSL #8
         ands  r6,r8,r10,LSR #20   @ extract 6th pixel
         orrne r12,r12,r6,LSL #16
         ands  r6,r8,r10,LSR #16   @ extract 5th pixel
         orrne r12,r12,r6,LSL #24
layerb_out:   
         tst   r0,#3    @ dword aligned destination?
         bne   layerb_slow
         stmia r0!,{r11,r12}    @ store pixels 1-8
         cmp   r0,r1       @ done with the line?
         bne   layerb_top
         ldmia sp!,{r4-r12,pc}
layerb_slow:   
         tst   r0,#1        @ real slow?
         bne   layerb_real_slow
         strh  r11,[r0],#2
         mov   r6,r11,LSR #16
         strh  r6,[r0],#2
         strh  r12,[r0],#2
         mov   r6,r12,LSR #16
         strh  r6,[r0],#2
         cmp   r0,r1       @ done with the line?
         bne   layerb_top
         ldmia sp!,{r4-r12,pc}
layerb_real_slow:   
         strb  r11,[r0],#1    @ each pixel needs to be written individually
         mov   r6,r11,LSR #8
         strb  r6,[r0],#1
         mov   r6,r11,LSR #16
         strb  r6,[r0],#1         
         mov   r6,r11,LSR #24
         strb  r6,[r0],#1         
         strb  r12,[r0],#1    @ each pixel needs to be written individually
         mov   r6,r12,LSR #8
         strb  r6,[r0],#1
         mov   r6,r12,LSR #16
         strb  r6,[r0],#1         
         mov   r6,r12,LSR #24
         strb  r6,[r0],#1         
         cmp   r0,r1       @ done with the line?
         bne   layerb_top
         ldmia sp!,{r4-r12,pc}
layerb_flipx:   
         ands  r6,r8,r10,LSR #16   @ extract 4th pixel
         orrne r11,r11,r6  @ non-transparent pixel
         ands  r6,r8,r10,LSR #20 @ extract 3rd pixel
         orrne r11,r11,r6,LSL #8
         ands  r6,r8,r10,LSR #24 @ extract 2nd pixel
         orrne r11,r11,r6,LSL #16
         ands  r6,r8,r10,LSR #28  @ extract 1st pixel
         orrne r11,r11,r6,LSL #24
         ands  r6,r8,r10         @ extract 8th pixel
         orrne r12,r12,r6
         ands  r6,r8,r10,LSR #4   @ extract 7th pixel
         orrne r12,r12,r6,LSL #8
         ands  r6,r8,r10,LSR #8   @ extract 6th pixel
         orrne r12,r12,r6,LSL #16
         ands  r6,r8,r10,LSR #12   @ extract 5th pixel
         orrne r12,r12,r6,LSL #24
         b     layerb_out  @ draw the pixels
@
@ Call as void ARMDrawLayerA(unsigned long *pDest, unsigned long *pEnd, unsigned short *pTiles, unsigned short *pusEnd, int iScrollWidth, unsigned long *pSrc, unsigned long *pSrcFlipped)@
@
ARMDrawLayerA:   
         stmfd sp!,{r4-r12,lr}
         ldr   r9,[sp,#40] @ iScrollWidth * 2
         ldr   r7,[sp,#44] @ pSrc
         ldr   r14,[sp,#48] @ pSrcFlipped
         mov   r4,#0x800   @ tile number mask
         sub   r4,r4,#1
         mov   r8,#0xf    @ pixel mask
         orr   r4,r4,#0xff000000 @ old tile #
         orr   r4,r4,#0xff0000
layera_top:   
         ldrh  r5,[r2],#2  @ get next tile
         cmp   r2,r3    @ past right edge?
         subge r2,r2,r9 @ pTiles -= iScrollWidth         
         orrs  r5,r5,r5 @ anything to draw?
         beq   layera_bot  @ nope, skip to next tile
         and   r10,r5,r4   @ mask off tile number
         tst   r5,#0x1000  @ flipy?
         ldreq r10,[r7,r10,LSL #5]  @ get tile data (8 4-bit pixels)
         ldrne r10,[r14,r10,LSL #5]  @ flipped data
         orrs  r10,r10,r10 @ anything to draw?
         beq   layera_bot  @ nope, skip to next tile
@ handle the 8 pixels transparent
         mvn   r6,r10        @ see if it's all opaque
         and   r6,r10,r10,LSR #2
         ands  r6,r6,r6,LSR #1
         beq   layera_opaque        @ all pixels opaque, can draw it faster
         tst   r5,#0x800   @ flipx?
         mov   r5,r5,LSR #9   @ get pixel color
         and   r5,r5,#0x70    @ isolate color and priority bit
         bne   layera_flipx
         ands  r6,r8,r10,LSR #12   @ extract 4th pixel
         orrne r6,r5,r6  @ non-transparent pixel
         strneb r6,[r0]
         ands  r6,r8,r10,LSR #8 @ extract 3rd pixel
         orrne r6,r5,r6
         strneb r6,[r0,#1]
         ands  r6,r8,r10,LSR #4 @ extract 2nd pixel
         orrne r6,r5,r6
         strneb r6,[r0,#2]
         ands  r6,r8,r10     @ extract 1st pixel
         orrne r6,r5,r6
         strneb r6,[r0,#3]
         ands  r6,r8,r10,LSR #28   @ extract 8th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#4]
         ands  r6,r8,r10,LSR #24   @ extract 7th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#5]
         ands  r6,r8,r10,LSR #20   @ extract 6th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#6]
         ands  r6,r8,r10,LSR #16   @ extract 5th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#7]
         add   r0,r0,#8    @ advance pointer
         cmp   r0,r1       @ done with the line?
         bne   layera_top
         ldmia sp!,{r4-r12,pc}
layera_flipx:   
         ands  r6,r8,r10,LSR #16   @ extract 4th pixel
         orrne r6,r5,r6  @ non-transparent pixel
         strneb r6,[r0]
         ands  r6,r8,r10,LSR #20 @ extract 3rd pixel
         orrne r6,r5,r6
         strneb r6,[r0,#1]
         ands  r6,r8,r10,LSR #24 @ extract 2nd pixel
         orrne r6,r5,r6
         strneb r6,[r0,#2]
         ands  r6,r8,r10,LSR #28  @ extract 1st pixel
         orrne r6,r5,r6
         strneb r6,[r0,#3]
         ands  r6,r8,r10         @ extract 8th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#4]
         ands  r6,r8,r10,LSR #4   @ extract 7th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#5]
         ands  r6,r8,r10,LSR #8   @ extract 6th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#6]
         ands  r6,r8,r10,LSR #12   @ extract 5th pixel
         orrne r6,r5,r6
         strneb r6,[r0,#7]
layera_bot:   
         add   r0,r0,#8    @ advance pointer
         cmp   r0,r1       @ done with the line?
         bne   layera_top
layera_end:   
         ldmia sp!,{r4-r12,pc}
@ all pixels are opaque
layera_opaque:   
         tst   r5,#0x800   @ flipx?
         mov   r5,r5,LSR #9   @ get pixel color
         and   r5,r5,#0x70    @ isolate color and priority bit
         orr   r5,r5,r5,LSL #8      @ fill entire dword with color
         orr   r5,r5,r5,LSL #16
         mov   r11,r5          @ second dword
         bne   layera_opaque_flipx
         and   r6,r8,r10,LSR #12   @ extract 4th pixel
         orr   r5,r5,r6
         and   r6,r8,r10,LSR #8   @ extract 3rd pixel
         orr   r5,r5,r6,LSL #8
         and   r6,r8,r10,LSR #4   @ extract 2nd pixel
         orr   r5,r5,r6,LSL #16
         and   r6,r8,r10          @ extract 1st pixel
         orr   r5,r5,r6,LSL #24
         and   r6,r8,r10,LSR #28   @ extract 8th pixel
         orr   r11,r11,r6
         and   r6,r8,r10,LSR #24   @ extract 7th pixel
         orr   r11,r11,r6,LSL #8
         and   r6,r8,r10,LSR #20   @ extract 6th pixel
         orr   r11,r11,r6,LSL #16
         and   r6,r8,r10,LSR #16   @ extract 5th pixel
         orr   r11,r11,r6,LSL #24
         tst   r0,#3            @ write fast?
         bne   layera_opaque_slow
         stmia r0!,{r5,r11}
         cmp   r0,r1       @ done with the line?
         bne   layera_top
         b     layera_end
layera_opaque_flipx:   
         and   r6,r8,r10,LSR #12   @ extract 4th pixel
         orr   r11,r11,r6,LSL #24 
         and   r6,r8,r10,LSR #8   @ extract 3rd pixel
         orr   r11,r11,r6,LSL #16
         and   r6,r8,r10,LSR #4   @ extract 2nd pixel
         orr   r11,r11,r6,LSL #8
         and   r6,r8,r10          @ extract 1st pixel
         orr   r11,r11,r6,LSL #0
         and   r6,r8,r10,LSR #28   @ extract 8th pixel
         orr   r5,r5,r6,LSL #24
         and   r6,r8,r10,LSR #24   @ extract 7th pixel
         orr   r5,r5,r6,LSL #16
         and   r6,r8,r10,LSR #20   @ extract 6th pixel
         orr   r5,r5,r6,LSL #8
         and   r6,r8,r10,LSR #16   @ extract 5th pixel
         orr   r5,r5,r6,LSL #0
         tst   r0,#3            @ write fast?
         bne   layera_opaque_slow
         stmia r0!,{r5,r11}
         cmp   r0,r1       @ done with the line?
         bne   layera_top
         b     layera_end
layera_opaque_slow:   
         tst   r0,#1        @ very slow?
         bne   layera_opaque_veryslow
         strh  r5,[r0]
         mov   r5,r5,LSR #16
         strh  r5,[r0,#2]
         strh  r11,[r0,#4]
         mov   r11,r11,LSR #16
         strh  r11,[r0,#6]
         b     layera_bot
layera_opaque_veryslow:   
         strb  r5,[r0]
         mov   r5,r5,LSR #8
         strb  r5,[r0,#1]
         mov   r5,r5,LSR #8
         strb  r5,[r0,#2]
         mov   r5,r5,LSR #8
         strb  r5,[r0,#3]
         strb  r11,[r0,#4]
         mov   r11,r11,LSR #8
         strb  r11,[r0,#5]
         mov   r11,r11,LSR #8
         strb  r11,[r0,#6]
         mov   r11,r11,LSR #8
         strb  r11,[r0,#7]
         b     layera_bot
@
@ Call as void DrawPriorityLine(unsigned long ulTile, unsigned char ucColor, int iFlipx, unsigned char *pDest)
@
DrawPriorityLine:   
         stmfd sp!,{r4,r5,lr}
         mov   r14,#0xf    @ bit pattern for extracting each pixel
         tst   r3,#3    @ dword aligned (can we draw it faster)?
         bne   drawpriority_slow
         mvn   r4,r0    @ see if it's all opaque
         and   r4,r4,r4,LSR #2
         ands  r4,r4,r4,LSR #1    @ any transparent pixels?
         bne   drawpriority_slow    @ yes, have to do it the slow way
         orr   r1,r1,r1,LSL #8
         orr   r4,r1,r1,LSL #16        @ get color in all 4 bytes
         mov   r5,r4
@ we have an opaque tile and dword aligned@ do it faster
         orrs  r2,r2,r2    @ horizontal flipped?
         bne   drawpriority_fast_flip
         and   r2,r14,r0,LSR #12
         orr   r4,r4,r2
         and   r2,r14,r0,LSR #8
         orr   r4,r4,r2,LSL #8
         and   r2,r14,r0,LSR #4
         orr   r4,r4,r2,LSL #16
         and   r2,r14,r0,LSR #0
         orr   r4,r4,r2,LSL #24
         and   r2,r14,r0,LSR #28
         orr   r5,r5,r2
         and   r2,r14,r0,LSR #24
         orr   r5,r5,r2,LSL #8
         and   r2,r14,r0,LSR #20
         orr   r5,r5,r2,LSL #16
         and   r2,r14,r0,LSR #16
         orr   r5,r5,r2,LSL #24
         stmia r3!,{r4,r5}
         ldmia sp!,{r4,r5,pc}
drawpriority_fast_flip:   
         and   r2,r14,r0,LSR #16
         orr   r4,r4,r2
         and   r2,r14,r0,LSR #20
         orr   r4,r4,r2,LSL #8
         and   r2,r14,r0,LSR #24
         orr   r4,r4,r2,LSL #16
         and   r2,r14,r0,LSR #28
         orr   r4,r4,r2,LSL #24
         and   r2,r14,r0,LSR #0
         orr   r5,r5,r2
         and   r2,r14,r0,LSR #4
         orr   r5,r5,r2,LSL #8
         and   r2,r14,r0,LSR #8
         orr   r5,r5,r2,LSL #16
         and   r2,r14,r0,LSR #12
         orr   r5,r5,r2,LSL #24
         stmia r3!,{r4,r5}
         ldmia sp!,{r4,r5,pc}
drawpriority_slow:   
         adds r2,r2,#0    @ flipped horizontally?
         bne priority_flipped
         ands r2,r14,r0,LSR #12  @ extract fourth pixel
         orrne r2,r2,r1
         strneb r2,[r3]
         ands r4,r14,r0,LSR #8   @ extract third pixel
         orrne r4,r4,r1          @ add color
         strneb r4,[r3,#1]
         ands r2,r14,r0,LSR #4   @ extract second pixel
         orrne r2,r2,r1          @ add color
         strneb r2,[r3,#2]
         ands r4,r14,r0          @ extract first pixel
         orrne r4,r4,r1          @ add color
         strneb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
         ands r2,r14,r0,LSR #28  @ extract fourth pixel
         orrne r2,r2,r1
         strneb r2,[r3]
         ands r4,r14,r0,LSR #24   @ extract third pixel
         orrne r4,r4,r1
         strneb r4,[r3,#1]
         ands r2,r14,r0,LSR #20   @ extract second pixel
         orrne r2,r2,r1
         strneb r2,[r3,#2]
         ands r4,r14,r0,LSR #16 @ extract first pixel to r2
         orrne r4,r4,r1
         strneb r4,[r3,#3]      
         ldmia sp!,{r4,r5,pc}
@ horizontally flipped
priority_flipped:   
         ands r2,r14,r0,LSR #16 @ extract fourth pixel
         orrne r2,r2,r1
         strneb r2,[r3]
         ands r4,r14,r0,LSR #20   @ extract third pixel
         orrne r4,r4,r1
         strneb r4,[r3,#1]
         ands r2,r14,r0,LSR #24   @ extract second pixel
         orrne r2,r2,r1
         strneb r2,[r3,#2]
         ands r4,r14,r0,LSR #28@ extract first pixel to r4
         orrne r4,r4,r1
         strneb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
         ands r2,r14,r0  @ extract fourth pixel
         orrne r2,r2,r1
         strneb r2,[r3]
         ands r4,r14,r0,LSR #4   @ extract third pixel
         orrne r4,r4,r1
         strneb r4,[r3,#1]
         ands r2,r14,r0,LSR #8   @ extract second pixel
         orrne r2,r2,r1
         strneb r2,[r3,#2]
         ands r4,r14,r0,LSR #12@ extract first pixel to r4
         orrne r4,r4,r1
         strneb r4,[r3,#3]
         ldmia sp!,{r4,r5,pc}
@
@ Call as void DrawLine(unsigned long ulTile, unsigned char ucColor, int iFlipx, unsigned char *pDest)
@
DrawLine:   
         stmfd sp!,{r4,r5,r6,lr}
         orr r1,r1,r1,LSL #8     @ place color palette in all 4 bytes
         orr r1,r1,r1,LSL #16
         mov r6,#0xf    @ bit pattern for extracting each pixel
           adds r2,r2,#0    @ flipped horizontally?
           bne line_flipped
         tst r3,#3      @ word boundary?
         ldreq r14,[r3]   @ get destination word for priority test
         ldrneb r14,[r3]
         ldrneb r2,[r3,#1]
         ldrneb r4,[r3,#2]
         ldrneb r5,[r3,#3]
         orrne r14,r14,r2,LSL #8
         orrne r14,r14,r4,LSL #16
         orrne r14,r14,r5,LSL #24
         mov r5,#0      @ number of pixels in this word
         ands r4,r0,r6  @ extract first pixel to r4
         orrne r5,r5,#1
         tst  r14,#0x40000000   @ see if dest pixel is high priority
         bicne r5,r5,#1       @ don't draw it if high priority
         mov r4,r4,LSL #24  @ shift over to last byte
         ands r2,r6,r0,LSR #4   @ extract second pixel
         orrne r5,r5,#2
         tst  r14,#0x400000   @ see if dest pixel is high priority
         bicne r5,r5,#2       @ don't draw it if high priority
         orr r4,r4,r2,LSL #16    @ combine with other pixels
         ands r2,r6,r0,LSR #8   @ extract third pixel
         orrne r5,r5,#4
         tst  r14,#0x4000   @ see if dest pixel is high priority
         bicne r5,r5,#4       @ don't draw it if high priority
         orr r4,r4,r2,LSL #8    @ combine with other pixels
         ands r2,r6,r0,LSR #12  @ extract fourth pixel
         orrne r5,r5,#8
         tst  r14,#0x40   @ see if dest pixel is high priority
         bicne r5,r5,#8       @ don't draw it if high priority
         orr r4,r4,r2      @ combine with other pixels
         orr r4,r4,r1      @ add color palette to all 4 pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         cmp r5,#15     @ all 4 pixels non-transparent?
         bne   lineslowdraw1   @ need to check each one
         tst r3,#3      @ word boundary?
         bne   lineslowdraw1
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b line_part2  @ draw the second half
lineslowdraw1:   
         tst r5,#8      @ use first pixel?
         strneb r4,[r3]
         tst r5,#4      @ use second pixel?
         mov r4,r4,LSR #8  @ shift it down
         strneb r4,[r3,#1]
         tst r5,#2      @ use third pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#2]
         tst r5,#1      @ use fourth pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
line_part2:   
         tst r3,#3      @ word boundary?
         ldreq r14,[r3]   @ get destination word for priority test
         ldrneb r14,[r3]
         ldrneb r2,[r3,#1]
         ldrneb r4,[r3,#2]
         ldrneb r5,[r3,#3]
         orrne r14,r14,r2,LSL #8
         orrne r14,r14,r4,LSL #16
         orrne r14,r14,r5,LSL #24
         mov r0,r0,LSR #16 @ shift everything down 4 pixels
         mov r5,#0      @ number of pixels in this word
         ands r4,r0,r6 @ extract first pixel to r4
         orrne r5,r5,#1
         tst  r14,#0x40000000   @ see if dest pixel is high priority
         bicne r5,r5,#1       @ don't draw it if high priority
         mov r4,r4,LSL #24  @ shift over to last byte
         ands r2,r6,r0,LSR #4   @ extract second pixel
         orrne r5,r5,#2
         tst  r14,#0x400000   @ see if dest pixel is high priority
         bicne r5,r5,#2       @ don't draw it if high priority
         orr r4,r4,r2,LSL #16    @ combine with other pixels
         ands r2,r6,r0,LSR #8   @ extract third pixel
         orrne r5,r5,#4
         tst  r14,#0x4000     @ see if dest pixel is high priority
         bicne r5,r5,#4       @ don't draw it if high priority
         orr r4,r4,r2,LSL #8   @ combine with other pixels
         ands r2,r6,r0,LSR #12  @ extract fourth pixel
         orrne r5,r5,#8
         tst  r14,#0x40       @ see if dest pixel is high priority
         bicne r5,r5,#8       @ don't draw it if high priority
         orr r4,r4,r2    @ combine with other pixels
         orr r4,r4,r1    @ add color palette to all 4 pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         cmp r5,#15     @ all 4 pixels non-transparent?
         bne   lineslowdraw2   @ need to check each one
         tst r3,#3      @ word boundary?
         bne   lineslowdraw2
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b line_end    @ done
lineslowdraw2:   
         tst r5,#8      @ use first pixel?
         strneb r4,[r3]
         tst r5,#4      @ use second pixel?
         mov r4,r4,LSR #8  @ shift it down
         strneb r4,[r3,#1]
         tst r5,#2      @ use third pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#2]
         tst r5,#1      @ use fourth pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#3]
           b line_end
@ horizontally flipped
line_flipped:   
         tst r3,#3      @ word boundary?
         ldreq r14,[r3]   @ get destination word for priority test
         ldrneb r14,[r3]
         ldrneb r2,[r3,#1]
         ldrneb r4,[r3,#2]
         ldrneb r5,[r3,#3]
         orrne r14,r14,r2,LSL #8
         orrne r14,r14,r4,LSL #16
         orrne r14,r14,r5,LSL #24
         mov r5,#0      @ number of pixels in this word
         ands r4,r6,r0,LSR #28@ extract first pixel to r4
         orrne r5,r5,#1
         mov r4,r4,LSL #24  @ shift over to last byte
         tst  r14,#0x40000000   @ see if dest pixel is high priority
         bicne r5,r5,#1       @ don't draw it if high priority
         ands r2,r6,r0,LSR #24   @ extract second pixel
         orrne r5,r5,#2
         tst  r14,#0x400000   @ see if dest pixel is high priority
         bicne r5,r5,#2       @ don't draw it if high priority
         orr r4,r4,r2,LSL #16    @ combine with other pixels
         ands r2,r6,r0,LSR #20   @ extract third pixel
         orrne r5,r5,#4
         tst  r14,#0x4000   @ see if dest pixel is high priority
         bicne r5,r5,#4       @ don't draw it if high priority
         orr r4,r4,r2,LSL #8    @ combine with other pixels
         ands r2,r6,r0,LSR #16 @ extract fourth pixel
         orrne r5,r5,#8
         tst  r14,#0x40   @ see if dest pixel is high priority
         bicne r5,r5,#8       @ don't draw it if high priority
         orr r4,r4,r2    @ combine with other pixels
         orr r4,r4,r1    @ add color palette to all 4 pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         cmp r5,#15     @ all 4 pixels non-transparent?
         bne   lineslowdraw3   @ need to check each one
         tst r3,#3      @ word boundary?
         bne   lineslowdraw3
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b line_part3  @ draw the second half
lineslowdraw3:   
         tst r5,#8      @ use first pixel?
         strneb r4,[r3]
         tst r5,#4      @ use second pixel?
         mov r4,r4,LSR #8  @ shift it down
         strneb r4,[r3,#1]
         tst r5,#2      @ use third pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#2]
         tst r5,#1      @ use fourth pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
line_part3:   
         tst r3,#3      @ word boundary?
         ldreq r14,[r3]   @ get destination word for priority test
         ldrneb r14,[r3]
         ldrneb r2,[r3,#1]
         ldrneb r4,[r3,#2]
         ldrneb r5,[r3,#3]
         orrne r14,r14,r2,LSL #8
         orrne r14,r14,r4,LSL #16
         orrne r14,r14,r5,LSL #24
         mov r5,#0      @ number of pixels in this word
         ands r4,r6,r0,LSR #12@ extract first pixel to r4
         orrne r5,r5,#1
         tst  r14,#0x40000000   @ see if dest pixel is high priority
         bicne r5,r5,#1       @ don't draw it if high priority
         mov r4,r4,LSL #24  @ shift over to last byte
         ands r2,r6,r0,LSR #8   @ extract second pixel
         orrne r5,r5,#2
         tst  r14,#0x400000   @ see if dest pixel is high priority
         bicne r5,r5,#2       @ don't draw it if high priority
         orr r4,r4,r2,LSL #16   @ combine with other pixels
         ands r2,r6,r0,LSR #4   @ extract third pixel
         orrne r5,r5,#4
         tst  r14,#0x4000   @ see if dest pixel is high priority
         bicne r5,r5,#4       @ don't draw it if high priority
         orr r4,r4,r2,LSL #8    @ combine with other pixels
         ands r2,r6,r0  @ extract fourth pixel
         orrne r5,r5,#8
         tst  r14,#0x40       @ see if dest pixel is high priority
         bicne r5,r5,#8       @ don't draw it if high priority
         orr r4,r4,r2    @ combine with other pixels
         orr r4,r4,r1    @ add color palette to all 4 pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         cmp r5,#15     @ all 4 pixels non-transparent?
         bne   lineslowdraw4   @ need to check each one
         tst r3,#3      @ word boundary?
         bne   lineslowdraw4
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b line_end    @ done
lineslowdraw4:   
         tst r5,#8      @ use first pixel?
         strneb r4,[r3]
         tst r5,#4      @ use second pixel?
         mov r4,r4,LSR #8  @ shift it down
         strneb r4,[r3,#1]
         tst r5,#2      @ use third pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#2]
         tst r5,#1      @ use fourth pixel?
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strneb r4,[r3,#3]
line_end:   
         ldmia sp!,{r4,r5,r6,pc}
@
@ Call as void DrawSimpleLine(unsigned long ulTile, unsigned char ucColor, int iFlipx, unsigned char *pDest, unsigned char ucBackground)
@ all pixels will be drawn@ transparent ones will be draw as the background color
DrawSimpleLine:   
         stmfd sp!,{r4,r6,r7,lr}
         ldr r7,[sp,#16]        @ get background color
         mov r6,#0xf    @ bit pattern for extracting each pixel
         adds r2,r2,#0    @ flipped horizontally?
         bne simple_flipped
         ands r4,r0,r6 @ extract first pixel to r4
         addne r4,r4,r1 @ add color
         addeq r4,r4,r7    @ use background color instead
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #4   @ extract second pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #8   @ extract third pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #12  @ extract fourth pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         tst r3,#3      @ word-aligned address?
         bne   slowdraw1a   @ nope, might as well go the slow route
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b simple_part2  @ draw the second half
slowdraw1a:   
         strb r4,[r3]
         mov r4,r4,LSR #8  @ shift it down
         strb r4,[r3,#1]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#2]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
simple_part2:   
         mov r0,r0,LSR #16 @ shift everything down 4 pixels
         ands r4,r0,r6 @ extract first pixel to r4
         addne r4,r4,r1 @ add color
         addeq r4,r4,r7    @ use background color instead
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #4   @ extract second pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #8   @ extract third pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #12  @ extract fourth pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         tst r3,#3      @ word-aligned address?
         bne   slowdraw2a   @ nope, might as well go the slow route
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b simple_end    @ done
slowdraw2a:   
         strb r4,[r3]
         mov r4,r4,LSR #8  @ shift it down
         strb r4,[r3,#1]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#2]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#3]
         b simple_end
@ horizontally flipped
simple_flipped:   
         ands r4,r6,r0,LSR #28@ extract first pixel to r4
         addne r4,r4,r1 @ add color
         addeq r4,r4,r7    @ use background color instead
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #24   @ extract second pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #20   @ extract third pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #16 @ extract fourth pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         tst r3,#3      @ word-aligned address?
         bne   slowdraw3a   @ nope, might as well go the slow route
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b simple_part3  @ draw the second half
slowdraw3a:   
         strb r4,[r3]
         mov r4,r4,LSR #8  @ shift it down
         strb r4,[r3,#1]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#2]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#3]
         add r3,r3,#4      @ point to next word
simple_part3:   
         ands r4,r6,r0,LSR #12@ extract first pixel to r4
         addne r4,r4,r1 @ add color
         addeq r4,r4,r7    @ use background color instead
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #8   @ extract second pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0,LSR #4   @ extract third pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
         mov r4,r4,LSL #8  @ shift over 1 byte
         ands r2,r6,r0  @ extract fourth pixel
         addne r2,r2,r1    @ add color
         addeq r2,r2,r7    @ use background color instead
         orr r4,r4,r2    @ combine with other pixels
@ now we've got 4 pixels ready to write, see if we can do it in one shot
         tst r3,#3      @ word-aligned address?
         bne   slowdraw4a   @ nope, might as well go the slow route
         str r4,[r3],#4    @ draw the 4 pixels in 1 shot
         b simple_end    @ done
slowdraw4a:   
         strb r4,[r3]
         mov r4,r4,LSR #8  @ shift it down
         strb r4,[r3,#1]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#2]
         mov r4,r4,LSR #8  @ shift down 1 pixel
         strb r4,[r3,#3]
simple_end:   
         ldmia sp!,{r4,r6,r7,pc}
  .end    

#endif
