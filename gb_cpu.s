.ifdef ARM
@      TITLE("Z80 Emulator")
@++
@
@ Z80 CPU emulator for use in arcade game emulation
@ written by Larry Bank
@ Copyright 1998-2017 BitBank Software, Inc.
@ Project started 5/4/98
@
@ Many Z80 instructions can be combined because they contain a 2 or 3-bit
@ field specifying a register or bit position.  Each of these has been expanded
@ to have their own handler code for maximum possible speed.  Much more tedious
@ but it should be worth the extra effort.
@
@ Change History
@----------------
@ 6/5/98 - sped up emulation by rewriting M_RDB macro to have ROM reads with the shortest code path
@ 6/6/98 - sped up emulation by rewriting instruction execution to assume that code is executing in normal ROM
@ 6/16/98 - sped up emulation by reworking my cpu tick counter to only store back to global when calling read_slow or write_slow
@ 12/21/99 - sped up emulation by checking pending interrupts only when necessary
@ 4/2/99 - sped up emulation by cleaning up the inner loop of execution to account for AGI of EBX
@ 11/26/04 - started conversion from x86 to ARM assembly language
@ 12/09/04 - first mostly working ARM assembly language version
@ 12/17/04 - changed instruction clock cylce count from a lookup table to individual instructions
@ 12/27/04 - converted Z80 into GBZ80
@ 2/19/10 - began removal of global vars - aim is to be completely threadsafe
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

.equ TRACE, 0 
@ interrupt priority bits
.equ INT_VBLANK, 1 
.equ INT_LCDSTAT, 2 
.equ INT_TIMER, 4 
.equ INT_SERIAL, 8 
.equ INT_BUTTONS, 16 
.equ F_CARRY, 16 
.equ F_HALFCARRY, 32 
.equ F_ADDSUB, 64 
.equ F_ZERO, 128 
@
@ Current CPU state structure
@
@ Offsets within the register structure 
.equ sregF, 0 
.equ sregA, 1 
.equ dummy1, 2 
.equ sregC, 4 
.equ sregB, 5 
.equ dummy2, 6 
.equ sregE, 8 
.equ sregD, 9 
.equ dummy3, 10 
.equ sregL, 12 
.equ sregH, 13 
.equ dummy4, 14 
.equ sregSP, 16 
.equ sregPC, 20 
.equ sregI, 24 
.equ sregHALT, 25 
@------- original register structure is 28 bytes
.equ ulRWFlags, 28 
.equ temp, -4 
.equ ulOpList, -72 @ list of 16 offsets to 4K blocks of memory
.equ iCPUSpeed, -76 
.equ iMasterClock, -80 
.equ bSpeedChange, -84 
.equ bTimerOn, -100 
.equ iTimerMask, -96 
.equ iTimerLimit, -92 
.equ ucTimerCounter, -85 
.equ ucLCDSTAT, -86 
.equ ucTimerModulo, -87 
.equ ucIRQFlags, -88 
.equ ucIRQEnable, -103 
.equ mem_map, -108 
.equ emuh, -112 
.equ eiticks, -116 @ for proper EI emulation
.equ pCBXXOps, -120 
.equ p00FFOps, -124 
  .extern  TRACEGB  
  .extern  GBIORead  
  .extern  GBIOWrite  
  .global  ARESETGB  
  .global  AEXECGB  
.macro M_EA_A 
        ldrb r9,[r7],#1            @ get the low byte
        ldrb r0,[r7],#1         @ get the high byte
        orr  r9,r9,r0,LSL #8    @ combine low and high bytes
.endm      
.macro M_EA_REL8 
        ldrsb r9,[r7],#1        @ get sign extended addr & increment PC
.endm      
.macro M_EA_I8 
        ldrb r10,[r7],#1        @ get imm byte & increment PC
.endm      
.macro M_EA_I16 
        ldrb r10,[r7],#1        @ get low byte
        ldrb r0,[r7],#1            @ get high byte
        orr  r10,r10,r0,LSL #8    @ combine
.endm      
@ Instruction macros
.macro M_XOR 
        eor  r8,r8,r10
        mov  r12,#0            @ all flags get blasted
        tst  r8,#0xff
        orreq r12,r12,#F_ZERO        @ only zero flag set
.endm      
.macro M_OR 
        orr  r8,r8,r10
        mov  r12,#0            @ all flags get blasted
        tst  r8,#0xff
        orreq r12,r12,#F_ZERO        @ only zero flag set
.endm      
.macro M_AND 
        and  r8,r8,r10
        mov  r12,#F_HALFCARRY    @ all flags get blasted
        tst  r8,#0xff
        orreq r12,r12,#F_ZERO        @ only zero flag set
.endm      
.macro M_RLA 
        mov  r0,r12,LSR #4            @ keep a copy of carry flag in bit 0
        and  r12,r12,#255-(F_HALFCARRY + F_CARRY + F_ADDSUB + F_HALFCARRY)
        and  r0,r0,#1            @ isolate carry flag
        mov     r8,r8,LSL #1        @ shift left one place
        orr     r8,r8,r0            @ get carry back as new bit 0
        tst  r8, #0x100            @ new carry generated
        orrne  r12,r12,#F_CARRY        @ set carry on result
.endm      
.macro M_RL 
        mov  r0,r12,LSR #4            @ keep a copy of carry flag in bit 0
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        and  r0,r0,#1            @ isolate bit 0
        mov     r10,r10,LSL #1        @ shift left one place
        orr     r10,r10,r0         @ get carry back as bit 0
        tst  r10, #0x100            @ new carry generated
        orrne  r12,r12,#F_CARRY        @ set carry on result
        tst  r10,#0xff
        orreq  r12,r12,#F_ZERO        @ and zero
.endm      
.macro M_SRL 
        and     r0,r10,#1            @ keep a copy of bit 0
        and  r10,r10,#0xff        @ trim to 8-bits
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        orr  r12,r12,r0,LSL #4        @ get new carry
        mov     r10,r10,LSR #1        @ shift right one place
        tst  r10,#0xff
        orreq  r12,r12,#F_ZERO
.endm      
.macro M_SWAP 
        mov  r0,r10,LSR #4        @ swap upper and lower nibbles
        mov  r10,r10,LSL #4        @ move lower nibble to upper
        and  r0,r0,#0xf            @ isolate upper (now lower) nibble
        mov  r12,#0                @ all flags get blasted
        orr  r10,r10,r0            @ finished
        tst  r10,#0xff            @ Zero flag is the only one tested
        orreq r12,r12,#F_ZERO
.endm      
.macro M_SRA 
        and     r0,r10,#1            @ keep a copy of bit 0
        and  r10,r10,#0xff        @ trim to 8-bits
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        orr  r12,r12,r0,LSL #4    @ get new carry
        mov  r10,r10,LSR #1
        tst  r10,#0x40
        orrne r10,r10,#0x80        @ repeat high bit
        tst  r10,#0xff
        orreq  r12,r12,#F_ZERO
.endm      
.macro M_SLA 
        mov  r10,r10,LSL #1        @ shift left 1 bit
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        tst  r10,#0x100            @ see if there was a new carry
        orrne  r12,r12,#F_CARRY        @ set carry if needed
        tst  r10,#0xff
        orreq  r12,r12,#F_ZERO
.endm      
.macro M_RLC 
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        mov  r10,r10,LSL #1
        tst  r10,#0x100            @ check for carry/rotated bit
        orrne r10,r10,#1            @ rotate high bit around
        orrne  r12,r12,#F_CARRY        @ set carry if needed
        tst  r10,#0xff
        orreq r12,r12,#F_ZERO
.endm      
.macro M_RLCA 
        mov  r8,r8,LSL #1
        and  r12,r12,#(255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY))
        tst  r8,#0x100
        orrne  r8,r8,#1            @ rotate high bit around
        orrne  r12,r12,#F_CARRY        @ set carry if needed
.endm      
.macro M_RRA 
        and  r0,r12,#F_CARRY    @ keep a copy of carry flag
        and  r8,r8,#0xff        @ trim to 8-bits
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        tst  r8,#1
        mov  r8,r8,LSR #1   @ rotate right 1 bit
        orrne  r12,r12,#F_CARRY    @ set carry flag from old bit 0
        orr  r8,r8,r0,LSL #3    @ old carry becomes bit 7
.endm      
.macro M_RR 
        and  r0,r12,#F_CARRY    @ keep a copy of carry
        and  r10,r10,#0xff        @ trim to 8-bits
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        tst  r10,#1                @ keep bit 0 for new carry
        mov  r10,r10,LSR #1
        orrne r12,r12,#F_CARRY
        orr  r10,r10,r0,LSL #3    @ get old carry as new bit 7
        tst  r10,#0xff
        orreq r12,r12,#F_ZERO
.endm      
.macro M_RRC 
        and  r0,r10,#1            @ get bit 0 to rotate around
        and  r10,r10,#0xff        @ trim to 8-bits
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        mov  r10,r10,LSR #1    @ rotate and get carry
        orr  r10,r10,r0,LSL #7        @ rotate bit 0 to bit 7
        orr  r12,r12,r0,LSL #4        @ bit 0 goes to carry
        tst  r10,#0xff
        orreq r12,r12,#F_ZERO
.endm      
.macro M_RRCA 
        and  r12,r12,#255-(F_ZERO + F_CARRY + F_ADDSUB + F_HALFCARRY)
        and  r8,r8,#0xff        @ trim to 8-bits
        and  r0,r8,#1            @ keep bit 0
        mov  r8,r8,LSR #1    @ shift down
        orr  r8,r8,r0,LSL #7        @ rotate bit 0 into bit 7
        orr  r12,r12,r0,LSL #4          @ get new carry
.endm      
.macro M_INC 
        add  r10,r10,#1
        and  r12,r12,#F_CARRY  @ only carry is preserved
        tst  r10,#0xff
        orreq r12,r12,#F_ZERO
        tst  r10,#0xf
        orreq r12,r12,#F_HALFCARRY
.endm      
.macro M_DEC 
        sub  r10,r10,#1
        and  r12,r12,#F_CARRY  @ only carry is preserved
        and  r0,r10,#0xf        @ test lower 4 bits
        orr  r12,r12,#F_ADDSUB
        tst  r10,#0xff
        orreq r12,r12,#F_ZERO
        cmp  r0,#0xf            @ half-carry?
        orreq r12,r12,#F_HALFCARRY
.endm      
.macro M_SBC 
        and   r1,r12,#F_CARRY        @ get carry into r1
        and   r12,r12,#255-(F_CARRY + F_HALFCARRY + F_ZERO)
        and   r10,r10,#0xff        @ trim to 8-bits
        and   r8,r8,#0xff
        sub   r0,r8,r10
        sub   r0,r0,r1,LSR #4        @ subtract old carry
        tst   r0,#0xff
        orreq r12,r12,#F_ZERO
        tst   r0,#0x100
        orrne r12,r12,#F_CARRY
        orr   r12,r12,#F_ADDSUB        @ always set the subtract flag
        eor   r1,r0,r8
        eor   r1,r1,r10        @ half carry test
        tst   r1,#0x10
        orrne r12,r12,#F_HALFCARRY
        mov   r8,r0            @ get the result of sbc into reg A
.endm      
.macro M_CP 
        and   r12,r12,#255-(F_CARRY + F_HALFCARRY + F_ZERO)
        and   r10,r10,#0xff
        and   r8,r8,#0xff        @ trim to 8-bits
        sub   r0,r8,r10
        tst   r0,#0xff
        orreq r12,r12,#F_ZERO
        tst   r0,#0x100
        orrne r12,r12,#F_CARRY
        eor   r1,r0,r8
        eor   r1,r1,r10        @ half carry test
        tst   r1,#0x10
        orrne r12,r12,#F_HALFCARRY
        orr   r12,r12,#F_ADDSUB
.endm      
.macro M_SUB 
        and   r12,r12,#255-(F_CARRY + F_HALFCARRY + F_ZERO)
        and   r10,r10,#0xff
        and   r8,r8,#0xff        @ trim to 8-bits
        sub   r0,r8,r10
        tst   r0,#0xff
        orreq r12,r12,#F_ZERO
        tst   r0,#0x100
        orrne r12,r12,#F_CARRY
        orr   r12,r12,#F_ADDSUB        @ always set the subtract flag
        eor   r1,r0,r8
        eor   r1,r1,r10        @ half carry test
        tst   r1,#0x10
        orrne r12,r12,#F_HALFCARRY
        mov   r8,r0            @ get the result of sub into reg A
.endm      
.macro M_ADC 
        and   r1,r12,#F_CARRY        @ get carry bit into r1
        and   r12,r12,#255-(F_CARRY + F_HALFCARRY + F_ADDSUB + F_ZERO)
        and   r10,r10,#0xff
        and   r8,r8,#0xff        @ trim to 8-bits
        add   r0,r8,r10
        add   r0,r0,r1,LSR #4        @ add carry bit
        tst   r0,#0xff
        orreq r12,r12,#F_ZERO
        tst   r0,#0x100
        orrne r12,r12,#F_CARRY
        eor   r1,r0,r8
        eor   r1,r1,r10        @ half carry test
        tst   r1,#0x10
        orrne r12,r12,#F_HALFCARRY
        mov   r8,r0            @ get the result of adc into reg A
.endm      
.macro M_ADD 
        and   r12,r12,#255-(F_CARRY + F_HALFCARRY + F_ADDSUB + F_ZERO)
        and   r8,r8,#0xff        @ trim to 8-bits
        and   r10,r10,#0xff
        add   r0,r8,r10
        tst   r0,#0xff
        orreq r12,r12,#F_ZERO
        tst   r0,#0x100
        orrne r12,r12,#F_CARRY
        eor   r1,r0,r8
        eor   r1,r1,r10        @ half carry test
        tst   r1,#0x10
        orrne r12,r12,#F_HALFCARRY
        mov   r8,r0            @ get the result of adc into reg A
.endm      
.macro M_BIT bit
        and  r12,r12,#F_CARRY     @ preserve only the carry
        orr  r12,r12,#F_HALFCARRY
        tst  r10,#(1 << \bit)
        orreq  r12,r12,#F_ZERO
.endm      
.macro M_RES bit
        bic  r10,r10,#(1 << \bit)     @ clear the specified bit
.endm      
.macro M_SET bit
        orr  r10,r10,#(1 << \bit)     @ set the specified bit
.endm      
@
@ convenient macros to load and store all of the registers
@ this will make the code almost pure macros
@
.macro M_LOAD_B 
        ldrb r10,[r3,#sregB]
.endm      
.macro M_LOAD_C 
        ldrb r10,[r3,#sregC]
.endm      
.macro M_LOAD_D 
        ldrb r10,[r3,#sregD]
.endm      
.macro M_LOAD_E 
        ldrb r10,[r3,#sregE]
.endm      
.macro M_LOAD_H 
        mov  r10,r11,LSR #8
.endm      
.macro M_LOAD_L 
        mov r10,r11
.endm      
.macro M_LOAD_A 
        mov  r10,r8
.endm      
.macro M_STORE_B 
        strb r10,[r3,#sregB]
.endm      
.macro M_STORE_C 
        strb r10,[r3,#sregC]
.endm      
 
.macro M_STORE_D 
        strb r10,[r3,#sregD]
.endm      
.macro M_STORE_E 
        strb r10,[r3,#sregE]
.endm      
.macro M_STORE_H 
        and r11,r11,#0xff
        orr r11,r11,r10,LSL #8
.endm      
.macro M_STORE_L 
        and r10,r10,#0xff
        and r11,r11,#0xff00
        orr r11,r11,r10
.endm      
.macro M_ADDHL 
        mov      r0,#0x10000
        sub   r0,r0,#1            @ trim to 16-bits
        and   r10,r10,r0
        and   r11,r11,r0
        and   r12,r12,#F_ZERO  @ only preserve ZERO flag
        add   r0,r11,r10
        tst   r0,#0x10000
        orrne r12,r12,#F_CARRY
        eor   r1,r0,r11
        eor   r1,r1,r10        @ half carry test
        tst   r1,#(F_HALFCARRY << 7)
        orrne r12,r12,#F_HALFCARRY
        mov   r11,r0
.endm      
.macro M_RDB 
        mov   r9,r9,LSL #16        @ trim address to 16-bits
        mov   r9,r9,LSR #16
        mov   r1,r9,LSR #12        @ shift down to get offset from table
        mov   r0,#1
        tst   r5,r0,LSL r1        @ see if this address has a handler routine
        ldreq r0,[r4,r1,LSL #2]    @ get offset from table
        addeq r0,r0,r9
        ldreqb r10,[r0]            @ get the byte if no handler
        blne  read_slow            @ need to call the hardware handler
.endm      
.macro M_RDW 
        mov  r9,r9,LSL #16        @ trim to 16-bits
        mov  r0,#1
        mov  r9,r9,LSR #16
        mov  r1,r9,LSR #12
        tst  r5,r0,LSL r1        @ "special" memory?
        ldreq  r0,[r4,r1,LSL #2]    @ get memory base
        addeq  r0,r0,r9            @ form complete address
        ldreqb r10,[r0]            @ get low byte
        ldreqb r9,[r0,#1]            @ get high byte
        orreq  r10,r10,r9,LSL #8    @ combine into word
        blne   read_slow16
.endm      
.macro M_WRW 
        mov  r9,r9,LSL #16        @ trim to 16-bits
        mov  r0,#0x10000
        mov  r9,r9,LSR #16
        mov  r1,r9,LSR #12
        tst  r5,r0,LSL r1        @ see if there is a write routine
        ldreq  r0,[r4,r1,LSL #2]    @ get memory base
        addeq  r0,r0,r9            @ form complete address
        streqb r10,[r0]            @ store low byte
        moveq  r10,r10,LSR #8
        streqb r10,[r0,#1]        @ store high byte
        blne   write_slow16
.endm      
.macro M_WRB 
        mov   r9,r9,LSL #16        @ trim address to 16-bits
        mov   r9,r9,LSR #16
        mov   r1,r9,LSR #12        @ shift down to get offset from table
        mov   r0,#0x10000        @ bit pattern to test in read/write flags
        tst   r5,r0,LSL r1        @ is there a write routine?
        ldreq r0,[r4,r1,LSL #2]    @ if not, just store it in memory
        addeq r0,r0,r9
        streqb r10,[r0]            @ store the byte if just RAM
        blne  write_slow        @ call hardware handler if needed
.endm      
.macro M_PUSHPC 
        str  r9,[r3,#temp]        @ need to save EA
        ldrh r9,[r3,#sregSP]    @ get current stack pointer
        sub  r9,r9,#2            @ dec by 2
        strh r9,[r3,#sregSP]    @ save it back
        sub  r10,r7,r2            @ get PC in R10
        M_WRW
        ldr  r9,[r3,#temp]
.endm      
.macro M_POPPC 
        ldrh  r9,[r3,#sregSP]  @ address in R9
        add  r0,r9,#2
        strh  r0,[r3,#sregSP]    @ save updated stack
        M_RDW
        mov  r7,r10,LSL #16        @ get new PC
        mov  r7,r7,LSR #16
        mov   r1,r7,LSR #12        @ shift down to get offset from table
        ldr   r2,[r4,r1,LSL #2]    @ get offset from table
        add   r7,r7,r2            @ form the complete address
.endm      
.macro M_POPW 
        ldrh  r9,[r3,#sregSP]
        add  r0,r9,#2
        strh  r0,[r3,#sregSP]    @ save updated stack
        M_RDW
.endm      
.macro M_PUSHW 
        ldrh r9,[r3,#sregSP]
        sub  r9,r9,#2
        strh r9,[r3,#sregSP]
        M_WRW
.endm      
@
@ Read a byte from one of the hardware handlers
@ R1 contains read handler #, R9 = address to read
@
read_slow:   
        stmfd  sp!,{r2,r3,r9,r12,lr}    @ save emuh table and link address
        ldr   r3,[r3,#emuh]    @ get pointer to handlers
        mov   r0,r9         @ address to read from
        mov   lr,pc
        ldr   pc,[r3,r1,LSL #3]    @ call the correct handler
        mov   r10,r0        @ get result in R10
        ldmfd  sp!,{r2,r3,r9,r12,pc}        @ restore emuh table and return
@
@ Read a word from one of the hardware handlers
@ R1 contains read handler #, R9 = address to read
@
read_slow16:   
        stmfd  sp!,{r2,r3,r4,r9,r12,lr}    @ save emuh table and link address
        ldr   r3,[r3,#emuh]    @ get pointer to handlers
        mov   r0,r9         @ address to read from
        ldr   r4,[r3,r1,LSL #3]    @ get the correct handler address
        mov   lr,pc
        mov   pc,r4
        mov   r10,r0        @ get result in R10
        add   r0,r9,#1        @ increment address
        mov   lr,pc
        mov   pc,r4        @ call handler again with new address
        orr   r10,r10,r0,LSL #8    @ combine with high byte        
        ldmfd  sp!,{r2,r3,r4,r9,r12,pc}        @ restore emuh table and return
@
@ Write a byte to one of the hardware handlers
@ R1 = handler #, R9 = address, R10 = data
@
write_slow:   
        stmfd sp!,{r2,r3,r9,r12,lr}
        ldr   r3,[r3,#emuh]        @ get pointer to handlers
        mov   r0,r9
        add      r3,r3,r1,LSL #3    @ need to do this because write handler is offset #4
        mov   r1,r10
        mov   lr,pc
        ldr   pc,[r3,#4]        @ call specific handler
        ldmfd sp!,{r2,r3,r9,r12,pc}    @ restore regs and leave
@
@ Write a byte to one of the hardware handlers
@ R1 = handler #, R9 = address, R10 = data
@
write_slow16:   
        stmfd sp!,{r2,r3,r4,r9,r12,lr}
        ldr   r3,[r3,#emuh]        @ get pointer to handlers
        mov   r0,r9
        add      r3,r3,r1,LSL #3    @ need to do this because write handler is offset #4
        mov   r1,r10
        ldr   r4,[r3,#4]        @ get specific handler address
        mov   lr,pc
        mov   pc,r4            @ call handler for low byte
        mov   r1,r10,LSR #8        @ store high byte
        add   r0,r9,#1            @ increment address
        mov   lr,pc
        mov   pc,r4            @ call handler for high byte
        ldmfd sp!,{r2,r3,r4,r9,r12,pc}    @ restore regs and leave
@ use trace to compare with 'C' version
do_trace:   
          stmfd  sp!,{r2-r12,lr}
          mov       r0,r6            @ get tick counter as param 0
          strb      r8,[r3,#sregA]
          strb      r12,[r3,#sregF]
          strh      r11,[r3,#sregL]    @ store HL
          sub       r10,r7,r2        @ get PC back
          strh      r10,[r3,#sregPC]
          bl        TRACEGB          @ write trace data
          ldmfd     sp!,{r2-r12,pc}
@
@ Call from C as ARESETGB(REGSGB *)
@
ARESETGB:   
          mov        r3,r0            @ point to register structure
          mov       r0,#0
          strh      r0,[r3,#sregI]    @ I=0, HALT = FALSE
          mov       r0,#0x100
          strh      r0,[r3,#sregPC]        @ PC = 100
          add       r0,r0,#0x4d
          strh      r0,[r3,#sregL]        @ HL = 14D
          mov       r0,#0x13
          strh      r0,[r3,#sregC]        @ BC = 13
          mov       r0,#0xd8
          strh      r0,[r3,#sregE]        @ DE = D8
          mvn       r0,#1        @ FFFE
          strh      r0,[r3,#sregSP]        @ SP = FFFE
          mov       r0,#0x1100
          add       r0,r0,#0x20
          strh      r0,[r3,#sregF]        @ AF = 1120
          ldr       r0,=instr_tab        @ 00-FF jump table
          ldr       r1,=instrCB            @ CB00-CBFF jump table
          str       r0,[r3,#p00FFOps]
          str       r1,[r3,#pCBXXOps]
          bx        lr
@
@ Call from C code as AEXECGB(REGSGB *pRegs, int iCycles)@
@
@ Register Usage  
@                 R0 = scratch
@                 R1 = scratch
@                  R2 = memory base for banked memory
@                 R3 = pointer to globals/registers/tables structure
@                  R4 = memory map offset table (if present)
@                  R5 = memory map flags
@                 R6 = CPU clock cycle counter
@                 R7 = current program counter (PC) + memory base
@                 R8 = A register
@                  R9 = Effective address
@                  R10 = data to be read or written at EA
@                  R11 = HL register
@                  R12 = F (flags) register
@
@
AEXECGB:   
        stmfd   sp!, {r4-r12,r14}    @ stack all of the registers
        mov     r3,r0            @ keep register structure pointer in R3
        mov     r6,r1            @ number of clock cycles to execute
        ldrh    r7,[r3,#sregPC]
        ldr     r5,[r3,#ulRWFlags]    @ get the flags bitmap (16+16 bits)
        add        r4,r3,#ulOpList
        mov     r0,r7,LSR #12        @ get memory page # (0-15)
        ldr     r2,[r4,r0,LSL #2]    @ get offset from cpuoffsets table
        add        r7,r7,r2            @ now we have an offset to the current PC
        ldrb    r8,[r3,#sregA]
        ldrh    r11,[r3,#sregL]
        ldrb    r12,[r3,#sregF]
        eor     r0,r0,r0
        str     r0,[r3,#eiticks]    @ reset EI flag
        ldr     r0,[r3,#iCPUSpeed]    @ see if we are set to "FAST"
        cmp     r0,#2
        moveq   r6,r6,LSL #1    @ double ticks if in fast mode
        ldrh    r0,[r3,#iMasterClock]    @ add to master clock
        add     r0,r0,r6        @ iMasterClock += iLocakTicks@
        strh    r0,[r3,#iMasterClock]    @ r0 = new iMasterClock
@
@ Check for timer being enabled
@
        ldrb    r1,[r3,#bTimerOn]
        tst     r1,#0xff
        beq        topZ80int        @ no timer, start normal execution
@
@ Timer is on, see if timer tick will increment in this time slice
@
        stmfd   sp!,{r2-r5}            @ r0 = iMasterClock
        ldr     r2,[r3,#iTimerMask]
        ldr     r1,[r3,#iTimerLimit] @ r1 = iTimerLimit
        ldrb    r10,[r3,#ucTimerCounter]    @ r10 = ucTimerCounter
timer_top:   
        cmp        r0,r1            @ while (iMasterClock > iTimerLimit)
        ble        timer_done
        add     r1,r1,r2        @ iTimerLimit += iTimerMask
        add     r10,r10,#1        @ ucTimerCounter++
        tst     r10,#0xff        @ if (!ucTimerCounter)
        bne     timer_check
        ldrb    r5,[r3,#ucIRQFlags] @ set the timer int pending flag
        orr     r5,r5,#INT_TIMER
        strb    r5,[r3,#ucIRQFlags]
        ldrb    r10,[r3,#ucTimerModulo]    @ ucTimerCounter = ucTimerModulo
timer_check:   
        cmp        r2,#0            @ if (iTimerMask == 0)
        bne     timer_top        @     break@
timer_done:   
        strb    r10,[r3,#ucTimerCounter]        @ store ucTimerCounter back
        cmp     r0,#0x10000        @ don't let them wrap around or it will hose up the timer
        subcs   r1,r1,#0x8000
        subcs   r0,r0,#0x8000
        cmp     r1,#0x10000        @ don't let them wrap around or it will hose up the timer
        subcs   r1,r1,#0x8000
        subcs   r0,r0,#0x8000
        str     r1,[r3,#iTimerLimit]    @ store iTimerLimit
        str     r0,[r3,#iMasterClock]    @ store masterclock
        ldmia   sp!,{r2-r5}
@
@ Top of execution loop
@
topZ80int: ldrb r0,[r3,#sregI] 
          ldrb  r1,[r3,#sregHALT]
          orr   r0,r0,r1
          tst   r0,#0xff
          beq   topZ80            @ if not halted and interrupts disabled, nothing to do
          ldrb  r0,[r3,#ucIRQFlags]
          ldrb  r1,[r3,#ucIRQEnable]
          and   r0,r0,r1        @ see if any interrupts enabled
          tst   r0,#0xff
          bne   intz80        @ handle an interrupt
          
topZ80:   
  .if  TRACE== 1
          orrs      r6,r6,r6    @ any cycles to execute?
          ble       botz80        @ time to go
          bl        do_trace
  .endif    
          ldrb      r9,[r7],#1        @ load opcode and increment
@          ldr       r1,[r3,#p00FFOps]
          ADR       r1,instr_tab
  .if  TRACE == 0
          orrs      r6,r6,r6    @ any cycles to execute?
          ble       botz80        @ time to go
  .endif    
          ldr          pc,[r1,r9,LSL #2]    @ jump to code addr
  .ltorg  @  need literal pool here to reach the "globals" area
@
@ Interrupt handling
@
intz80:   
          ldrb  r1,[r3,#sregHALT]
          tst   r1,#0xff    @ halted?
          addne r7,r7,#1    @ PC++
          movne r1,#0
          strneb r1,[r3,#sregHALT]    @ HALT = FALSE
          M_PUSHPC
          mov   r0,#0
          strb  r0,[r3,#sregI]    @ disable interrupts
          ldrb r0,[r3,#ucIRQFlags]    @ get IRQFlags pending
          ldrb r1,[r3,#ucIRQEnable]    @ get IRQEnable bits
          and  r10,r0,r1        @ test which pending interrupts can go
          tst  r10,#INT_VBLANK    @ highest priority interrupt
          beq  try_int0
          bic  r0,r0,#INT_VBLANK
          mov  r7,#0x40            @ interrupt vector
          strb r0,[r3,#ucIRQFlags]    @ store new pending bits
          ldr  r2,[r4]            @ get bank 0 memory base
          add  r7,r7,r2            @ complete address
          b topZ80
try_int0:   
          tst  r10,#INT_LCDSTAT
          beq  try_int1
          bic  r0,r0,#INT_LCDSTAT
          mov  r7,#0x48            @ interrupt vector
          strb r0,[r3,#ucIRQFlags]    @ store new pending bits
          ldr  r2,[r4]            @ get bank 0 memory base
          add  r7,r7,r2            @ complete address
          b topZ80
try_int1:   
          tst  r10,#INT_TIMER
          beq  try_int2
          bic  r0,r0,#INT_TIMER
          mov  r7,#0x50            @ interrupt vector
          strb r0,[r3,#ucIRQFlags]    @ store new pending bits
          ldr  r2,[r4]            @ get bank 0 memory base
          add  r7,r7,r2            @ complete address
          b topZ80
try_int2:   
          tst  r10,#INT_SERIAL
          beq  try_int3
          bic  r0,r0,#INT_SERIAL
          mov  r7,#0x58            @ interrupt vector
          strb r0,[r3,#ucIRQFlags]    @ store new pending bits
          ldr  r2,[r4]            @ get bank 0 memory base
          add  r7,r7,r2            @ complete address
          b topZ80
try_int3:   
          tst  r10,#INT_BUTTONS
          beq  topZ80
          bic  r0,r0,#INT_BUTTONS
          mov  r7,#0x60            @ interrupt vector
          strb r0,[r3,#ucIRQFlags]    @ store new pending bits
          ldr  r2,[r4]            @ get bank 0 memory base
          add  r7,r7,r2            @ complete address
          b topZ80
@
@ End of the line
@
botz80:   
  .if  TRACE == 0
          sub  r7,r7,#1                @ because it just got incremented
  .endif    
          ldr  r0,[r3,#eiticks]        @ special handling of EI?
          orrs r0,r0,r0                @ need to handle it?
          beq  no_ei
          add  r6,r6,r0                @ adjust tick count
          eor  r0,r0,r0
          str  r0,[r3,#eiticks]        @ reset EI flag
          b    topZ80int            @ check for pending interrupts
no_ei:   
          sub  r7,r7,r2                @ get PC back
          str  r7,[r3,#sregPC]        @ get registers into structure for copying back
          strb r12,[r3,#sregF]
          strb r8,[r3,#sregA]
          strh r11,[r3,#sregL]
          ldmfd   sp!, {r4-r12,pc}        
@ 0xCB Lots of opcodes branch from here
op_cb_ops:   
          ldrb      r9,[r7],#1            @ get opcode and increment PC
@          ldr       r1,[r3,#pCBXXOps]    @ funky way of getting a relative address
          ADR       r1,instrCB
          ldr          pc,[r1,r9,LSL #2]   @ execute the instruction
@
@ Offset table to each instruction handler (base instructions 0-FF)
@
instr_tab:   
  .word  op_nop,  op_ld_bc_nn      @ 00,01
  .word  op_ld_bc_a,  op_inc_bc    @ 02,03
  .word  op_inc_b,  op_dec_b       @ 04,05
  .word  op_ld_b_n,  op_rlca       @ 06,07
  .word  op_ld_nn_sp,  op_add_hl_bc   @ 08,09
  .word  op_ld_a_bc,  op_dec_bc       @ 0A,0B
  .word  op_inc_c,  op_dec_c       @ 0C,0D
  .word  op_ld_c_n,  op_rrca       @ 0E,0F
  .word  op_stop,  op_ld_de_nn     @ 10,11
  .word  op_ld_de_a,  op_inc_de    @ 12,13
  .word  op_inc_d,  op_dec_d       @ 14,15
  .word  op_ld_d_n,  op_rla        @ 16,17
  .word  op_jr,  op_add_hl_de      @ 18,19
  .word  op_ld_a_de,  op_dec_de    @ 1A,1B
  .word  op_inc_e,  op_dec_e       @ 1C,1D
  .word  op_ld_e_n,  op_rra        @ 1E,1F
  .word  op_jr_nz,  op_ld_hl_nn    @ 20,21
  .word  op_ld_hlipp_a,  op_inc_hl   @ 22,23
  .word  op_inc_h,  op_dec_h       @ 24,25
  .word  op_ld_h_n,  op_daa        @ 26,27
  .word  op_jr_z,  op_add_hl_hl    @ 28,29
  .word  op_ld_a_hlipp,  op_dec_hl  @ 2A,2B
  .word  op_inc_l,  op_dec_l       @ 2C,2D
  .word  op_ld_l_n,  op_cpl        @ 2E,2F
  .word  op_jr_nc,  op_ld_sp_nn    @ 30,31
  .word  op_ld_hlimm_a,  op_inc_sp    @ 32,33
  .word  op_inc_hli,  op_dec_hli   @ 34,35
  .word  op_ld_hl_n,  op_scf       @ 36,37
  .word  op_jr_c,  op_add_hl_sp    @ 38,39
  .word  op_ld_a_hlimm,  op_dec_sp    @ 3A,3B
  .word  op_inc_a,  op_dec_a       @ 3C,3D
  .word  op_ld_a_n,  op_ccf        @ 3E,3F
  .word  op_ld_b_b,  op_ld_b_c     @ 40,41
  .word  op_ld_b_d,  op_ld_b_e     @ 42,43
  .word  op_ld_b_h,  op_ld_b_l     @ 44,45
  .word  op_ld_b_hl,  op_ld_b_a    @ 46,47
  .word  op_ld_c_b,  op_ld_c_c     @ 48,49
  .word  op_ld_c_d,  op_ld_c_e     @ 4A,4B
  .word  op_ld_c_h,  op_ld_c_l     @ 4C,4D
  .word  op_ld_c_hl,  op_ld_c_a    @ 4E,4F
  .word  op_ld_d_b,  op_ld_d_c     @ 50,51
  .word  op_ld_d_d,  op_ld_d_e     @ 52,53
  .word  op_ld_d_h,  op_ld_d_l     @ 54,55
  .word  op_ld_d_hl,  op_ld_d_a    @ 56,57
  .word  op_ld_e_b,  op_ld_e_c     @ 58,59
  .word  op_ld_e_d,  op_ld_e_e     @ 5A,5B
  .word  op_ld_e_h,  op_ld_e_l     @ 5C,5D
  .word  op_ld_e_hl,  op_ld_e_a    @ 5E,5F
  .word  op_ld_h_b,  op_ld_h_c     @ 60,61
  .word  op_ld_h_d,  op_ld_h_e     @ 62,63
  .word  op_ld_h_h,  op_ld_h_l     @ 64,65
  .word  op_ld_h_hl,  op_ld_h_a    @ 66,67
  .word  op_ld_l_b,  op_ld_l_c     @ 68,69
  .word  op_ld_l_d,  op_ld_l_e     @ 6A,6B
  .word  op_ld_l_h,  op_ld_l_l     @ 6C,6D
  .word  op_ld_l_hl,  op_ld_l_a    @ 6E,6F
  .word  op_ld_hl_b,  op_ld_hl_c   @ 70,71
  .word  op_ld_hl_d,  op_ld_hl_e   @ 72,73
  .word  op_ld_hl_h,  op_ld_hl_l   @ 74,75
  .word  op_halt,  op_ld_hl_a      @ 76,77
  .word  op_ld_a_b,  op_ld_a_c     @ 78,79
  .word  op_ld_a_d,  op_ld_a_e     @ 7A,7B
  .word  op_ld_a_h,  op_ld_a_l     @ 7C,7D
  .word  op_ld_a_hl,  op_ld_a_a    @ 7E,7F
  .word  op_add_b,  op_add_c   @ 80,81
  .word  op_add_d,  op_add_e   @ 82,83
  .word  op_add_h,  op_add_l   @ 84,85
  .word  op_add_hl,  op_add_a  @ 86,87
  .word  op_adc_b,  op_adc_c   @ 88,89
  .word  op_adc_d,  op_adc_e   @ 8A,8B
  .word  op_adc_h,  op_adc_l   @ 8C,8D
  .word  op_adc_hl,  op_adc_a  @ 8E,8F
  .word  op_sub_b,  op_sub_c   @ 90,91
  .word  op_sub_d,  op_sub_e   @ 92,93
  .word  op_sub_h,  op_sub_l   @ 94,95
  .word  op_sub_hl,  op_sub_a  @ 96,97
  .word  op_sbc_b,  op_sbc_c   @ 98,99
  .word  op_sbc_d,  op_sbc_e   @ 9A,9B
  .word  op_sbc_h,  op_sbc_l   @ 9C,9D
  .word  op_sbc_hl,  op_sbc_a  @ 9E,9F
  .word  op_and_b,  op_and_c   @ A0,A1
  .word  op_and_d,  op_and_e   @ A2,A3
  .word  op_and_h,  op_and_l   @ A4,A5
  .word  op_and_hl,  op_and_a  @ A6,A7
  .word  op_xor_b,  op_xor_c   @ A8,A9
  .word  op_xor_d,  op_xor_e   @ AA,AB
  .word  op_xor_h,  op_xor_l   @ AC,AD
  .word  op_xor_hl,  op_xor_a  @ AE,AF
  .word  op_or_b,  op_or_c     @ B0,B1
  .word  op_or_d,  op_or_e     @ B2,B3
  .word  op_or_h,  op_or_l     @ B4,B5
  .word  op_or_hl,  op_or_a    @ B6,B7
  .word  op_cp_b,  op_cp_c     @ B8,B9
  .word  op_cp_d,  op_cp_e     @ BA,BB
  .word  op_cp_h,  op_cp_l     @ BC,BD
  .word  op_cp_hl,  op_cp_a    @ BE,BF
  .word  op_ret_nz,  op_pop_bc     @ C0,C1
  .word  op_jp_nz,  op_jp_nn       @ C2,C3
  .word  op_call_nz,  op_push_bc   @ C4,C5
  .word  op_add_n,  op_rst_0       @ C6,C7
  .word  op_ret_z,  op_ret         @ C8,C9
  .word  op_jp_z,  op_cb_ops       @ CA,CB
  .word  op_call_z,  op_call       @ CC,CD
  .word  op_adc_n,  op_rst_8       @ CE,CF
  .word  op_ret_nc,  op_pop_de     @ D0,D1
  .word  op_jp_nc,  op_nop         @ D2,D3
  .word  op_call_nc,  op_push_de   @ D4,D5
  .word  op_sub_n,  op_rst_10      @ D6,D7
  .word  op_ret_c,  op_reti        @ D8,D9
  .word  op_jp_c,  op_nop          @ DA,DB
  .word  op_call_c,  op_nop        @ DC,DD
  .word  op_sbc_n,  op_rst_18      @ DE,DF
  .word  op_ld_ffxx_a,  op_pop_hl     @ E0,E1
  .word  op_ld_ffcc_a,  op_ex_sp_hl    @ E2,E3
  .word  op_nop,  op_push_hl       @ E4,E5
  .word  op_and_n,  op_rst_20      @ E6,E7
  .word  op_add_sp_n,  op_jp_hl    @ E8,E9
  .word  op_ld_nn_a,  op_nop       @ EA,EB
  .word  op_nop,  op_nop           @ EC,ED
  .word  op_xor_n,  op_rst_28      @ EE,EF
  .word  op_ld_a_ffxx,  op_pop_af  @ F0,F1
  .word  op_ld_a_ffcc,  op_di      @ F2,F3
  .word  op_nop,  op_push_af       @ F4,F5
  .word  op_or_n,  op_rst_30       @ F6,F7
  .word  op_ld_hl_sp_n,  op_ld_sp_hl    @ F8,F9
  .word  op_ld_a_nn,  op_ei        @ FA,FB
  .word  op_nop,  op_nop           @ FC,FD
  .word  op_cp_n,  op_rst_38       @ FE,FF
@ instruction table for opcodes which begin with 0xCB
instrCB:   
  .word  op_rlc_b,  op_rlc_c @ 00,01
  .word  op_rlc_d,  op_rlc_e @ 02,03
  .word  op_rlc_h,  op_rlc_l @ 04,05
  .word  op_rlc_hl,  op_rlc_a    @ 06,07
  .word  op_rrc_b,  op_rrc_c     @ 08,09
  .word  op_rrc_d,  op_rrc_e     @ 0A,0B
  .word  op_rrc_h,  op_rrc_l     @ 0C,0D
  .word  op_rrc_hl,  op_rrc_a    @ 0E,0F
  .word  op_rl_b,  op_rl_c       @ 10,11
  .word  op_rl_d,  op_rl_e       @ 12,13
  .word  op_rl_h,  op_rl_l       @ 14,15
  .word  op_rl_hl,  op_rl_a      @ 16,17
  .word  op_rr_b,  op_rr_c       @ 18,19
  .word  op_rr_d,  op_rr_e       @ 1A,1B
  .word  op_rr_h,  op_rr_l       @ 1C,1D
  .word  op_rr_hl,  op_rr_a      @ 1E,1F
  .word  op_sla_b,  op_sla_c     @ 20,21
  .word  op_sla_d,  op_sla_e     @ 22,23
  .word  op_sla_h,  op_sla_l     @ 24,25
  .word  op_sla_hl,  op_sla_a    @ 26,27
  .word  op_sra_b,  op_sra_c     @ 28,29
  .word  op_sra_d,  op_sra_e     @ 2A,2B
  .word  op_sra_h,  op_sra_l     @ 2C,2D
  .word  op_sra_hl,  op_sra_a    @ 2E,2F
  .word  op_swap_b,  op_swap_c     @ 30,31
  .word  op_swap_d,  op_swap_e     @ 32,33
  .word  op_swap_h,  op_swap_l     @ 34,35
  .word  op_swap_hl,  op_swap_a    @ 36,37
  .word  op_srl_b,  op_srl_c     @ 38,39
  .word  op_srl_d,  op_srl_e     @ 3A,3B
  .word  op_srl_h,  op_srl_l     @ 3C,3D
  .word  op_srl_hl,  op_srl_a    @ 3E,3F
  .word  op_bit_0_b,  op_bit_0_c   @ 40,41
  .word  op_bit_0_d,  op_bit_0_e   @ 42,43
  .word  op_bit_0_h,  op_bit_0_l   @ 44,45
  .word  op_bit_0_hl,  op_bit_0_a  @ 46,47
  .word  op_bit_1_b,  op_bit_1_c   @ 48,49
  .word  op_bit_1_d,  op_bit_1_e   @ 4A,4B
  .word  op_bit_1_h,  op_bit_1_l   @ 4C,4D
  .word  op_bit_1_hl,  op_bit_1_a  @ 4E,4F
  .word  op_bit_2_b,  op_bit_2_c   @ 50,51
  .word  op_bit_2_d,  op_bit_2_e   @ 52,53
  .word  op_bit_2_h,  op_bit_2_l   @ 54,55
  .word  op_bit_2_hl,  op_bit_2_a  @ 56,57
  .word  op_bit_3_b,  op_bit_3_c   @ 58,59
  .word  op_bit_3_d,  op_bit_3_e   @ 5A,5B
  .word  op_bit_3_h,  op_bit_3_l   @ 5C,5D
  .word  op_bit_3_hl,  op_bit_3_a  @ 5E,5F
  .word  op_bit_4_b,  op_bit_4_c   @ 60,61
  .word  op_bit_4_d,  op_bit_4_e   @ 62,63
  .word  op_bit_4_h,  op_bit_4_l   @ 64,65
  .word  op_bit_4_hl,  op_bit_4_a  @ 66,67
  .word  op_bit_5_b,  op_bit_5_c   @ 68,69
  .word  op_bit_5_d,  op_bit_5_e   @ 6A,6B
  .word  op_bit_5_h,  op_bit_5_l   @ 6C,6D
  .word  op_bit_5_hl,  op_bit_5_a  @ 6E,6F
  .word  op_bit_6_b,  op_bit_6_c   @ 70,71
  .word  op_bit_6_d,  op_bit_6_e   @ 72,73
  .word  op_bit_6_h,  op_bit_6_l   @ 74,75
  .word  op_bit_6_hl,  op_bit_6_a  @ 76,77
  .word  op_bit_7_b,  op_bit_7_c   @ 78,79
  .word  op_bit_7_d,  op_bit_7_e   @ 7A,7B
  .word  op_bit_7_h,  op_bit_7_l   @ 7C,7D
  .word  op_bit_7_hl,  op_bit_7_a  @ 7E,7F
  .word  op_res_0_b,  op_res_0_c   @ 80,81
  .word  op_res_0_d,  op_res_0_e   @ 82,83
  .word  op_res_0_h,  op_res_0_l   @ 84,85
  .word  op_res_0_hl,  op_res_0_a  @ 86,87
  .word  op_res_1_b,  op_res_1_c   @ 88,89
  .word  op_res_1_d,  op_res_1_e   @ 8A,8B
  .word  op_res_1_h,  op_res_1_l   @ 8C,8D
  .word  op_res_1_hl,  op_res_1_a  @ 8E,8F
  .word  op_res_2_b,  op_res_2_c   @ 90,91
  .word  op_res_2_d,  op_res_2_e   @ 92,93
  .word  op_res_2_h,  op_res_2_l   @ 94,95
  .word  op_res_2_hl,  op_res_2_a  @ 96,97
  .word  op_res_3_b,  op_res_3_c   @ 98,99
  .word  op_res_3_d,  op_res_3_e   @ 9A,9B
  .word  op_res_3_h,  op_res_3_l   @ 9C,9D
  .word  op_res_3_hl,  op_res_3_a  @ 9E,9F
  .word  op_res_4_b,  op_res_4_c   @ A0,A1
  .word  op_res_4_d,  op_res_4_e   @ A2,A3
  .word  op_res_4_h,  op_res_4_l   @ A4,A5
  .word  op_res_4_hl,  op_res_4_a  @ A6,A7
  .word  op_res_5_b,  op_res_5_c   @ A8,A9
  .word  op_res_5_d,  op_res_5_e   @ AA,AB
  .word  op_res_5_h,  op_res_5_l   @ AC,AD
  .word  op_res_5_hl,  op_res_5_a  @ AE,AF
  .word  op_res_6_b,  op_res_6_c   @ B0,B1
  .word  op_res_6_d,  op_res_6_e   @ B2,B3
  .word  op_res_6_h,  op_res_6_l   @ B4,B5
  .word  op_res_6_hl,  op_res_6_a  @ B6,B7
  .word  op_res_7_b,  op_res_7_c   @ B8,B9
  .word  op_res_7_d,  op_res_7_e   @ BA,BB
  .word  op_res_7_h,  op_res_7_l   @ BC,BD
  .word  op_res_7_hl,  op_res_7_a  @ BE,BF
  .word  op_set_0_b,  op_set_0_c   @ C0,C1
  .word  op_set_0_d,  op_set_0_e   @ C2,C3
  .word  op_set_0_h,  op_set_0_l   @ C4,C5
  .word  op_set_0_hl,  op_set_0_a  @ C6,C7
  .word  op_set_1_b,  op_set_1_c   @ C8,C9
  .word  op_set_1_d,  op_set_1_e   @ CA,CB
  .word  op_set_1_h,  op_set_1_l   @ CC,CD
  .word  op_set_1_hl,  op_set_1_a  @ CE,CF
  .word  op_set_2_b,  op_set_2_c   @ D0,D1
  .word  op_set_2_d,  op_set_2_e   @ D2,D3
  .word  op_set_2_h,  op_set_2_l   @ D4,D5
  .word  op_set_2_hl,  op_set_2_a  @ D6,D7
  .word  op_set_3_b,  op_set_3_c   @ D8,D9
  .word  op_set_3_d,  op_set_3_e   @ DA,DB
  .word  op_set_3_h,  op_set_3_l   @ DC,DD
  .word  op_set_3_hl,  op_set_3_a  @ DE,DF
  .word  op_set_4_b,  op_set_4_c   @ E0,E1
  .word  op_set_4_d,  op_set_4_e   @ E2,E3
  .word  op_set_4_h,  op_set_4_l   @ E4,E5
  .word  op_set_4_hl,  op_set_4_a  @ E6,E7
  .word  op_set_5_b,  op_set_5_c   @ E8,E9
  .word  op_set_5_d,  op_set_5_e   @ EA,EB
  .word  op_set_5_h,  op_set_5_l   @ EC,ED
  .word  op_set_5_hl,  op_set_5_a  @ EE,EF
  .word  op_set_6_b,  op_set_6_c   @ F0,F1
  .word  op_set_6_d,  op_set_6_e   @ F2,F3
  .word  op_set_6_h,  op_set_6_l   @ F4,F5
  .word  op_set_6_hl,  op_set_6_a  @ F6,F7
  .word  op_set_7_b,  op_set_7_c   @ F8,F9
  .word  op_set_7_d,  op_set_7_e   @ FA,FB
  .word  op_set_7_h,  op_set_7_l   @ FC,FD
  .word  op_set_7_hl,  op_set_7_a  @ FE,FF
@
@ Individual routines for instruction processing
@
OPCODES:   
@ Place holder for illegal instructions
bogus: b topZ80 
@ 0x00 NOP
op_nop: sub r6,r6,#4 
          b   topZ80
@ 0x01 LD BC,nn
op_ld_bc_nn:   
           M_EA_I16
           sub r6,r6,#12
           strh r10,[r3,#sregC]
           b   topZ80
@ 0x02 LD (BC),A
op_ld_bc_a: ldrh r9,[r3,#sregC] 
            mov r10,r8
            sub r6,r6,#8
            M_WRB
            b   topZ80
@ 0x03 INC BC
op_inc_bc: ldrh r0,[r3,#sregC] @ no flags affected
           sub  r6,r6,#8
           add  r0,r0,#1
           strh r0,[r3,#sregC]
           b   topZ80
@ 0x04 INC B
op_inc_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#4
           M_INC
           strb  r10,[r3,#sregB]
           b   topZ80
@ 0x05 DEC B
op_dec_b: ldrb r10,[r3,#sregB] 
           M_DEC
            sub r6,r6,#4
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x06 LD B,n
op_ld_b_n:   
          M_EA_I8
            sub r6,r6,#8
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x07 RLCA
op_rlca:   
           M_RLCA
            sub r6,r6,#4
           b   topZ80
@ 0x08 LD (nn),SP
op_ld_nn_sp:   
            M_EA_A                    @ get absolute address
            sub r6,r6,#20
            ldrh r10,[r3,#sregSP]    @ get stack pointer
            M_WRW                    @ write the word
           b   topZ80
@ 0x09 ADD HL,BC
op_add_hl_bc: ldrh r10,[r3,#sregC] 
            sub r6,r6,#8
           M_ADDHL
           b   topZ80
@ 0x0A LD A,(BC)
op_ld_a_bc: ldrh r9,[r3,#sregC] 
            sub r6,r6,#8
           M_RDB
           mov r8,r10
           b   topZ80
@ 0x0B DEC BC - no flags affected
op_dec_bc: ldrh r0,[r3,#sregC] 
            sub r6,r6,#8
           sub  r0,r0,#1
           strh r0,[r3,#sregC]
           b   topZ80
@ 0x0C INC C
op_inc_c: ldrb r10,[r3,#sregC] 
           M_INC
            sub r6,r6,#4
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x0D DEC C
op_dec_c: ldrb r10,[r3,#sregC] 
           M_DEC
            sub r6,r6,#4
          strb  r10,[r3,#sregC]
           b   topZ80
@ 0x0E LD C,n
op_ld_c_n:   
           M_EA_I8
            sub r6,r6,#8
           strb  r10,[r3,#sregC]
           b   topZ80
@ 0x0F RRCA
op_rrca:   
            sub r6,r6,#4
           M_RRCA
           b   topZ80
@ 0x10 STOP
op_stop:   
         ldrb r1,[r3,#bSpeedChange] @ get speed change boolean
         sub  r6,r6,#4
         tst  r1,#0xff        @ true?
         subeq r7,r7,#1        @ stick on this instruction if not
         moveq r0,#1
         streqb r0,[r3,#sregHALT]    @ in a halted state
         moveq r6,#0        @ stop execution
         beq  topZ80        @ done
@ cpu speed is changing
         mov  r1,#0
         strb r1,[r3,#bSpeedChange]        @ bSpeedChange = FALSE
         ldrb r1,[r3,#iCPUSpeed]    @ speed is changing
         cmp  r1,#1            @ speeding up?
         eor  r1,r1,#3        @ change 1->2 or 2->1
         strb r1,[r3,#iCPUSpeed]        @ update C variable
         moveq r6,r6,LSL #1    @ speeding up, double remaining clock ticks
         movne r6,r6,ASR #1    @ slowing down
         add r7,r7,#1        @ skip second byte (what does it do?)
         b   topZ80
  .ltorg    
@ 0x11 LD DE,nn
op_ld_de_nn:   
          M_EA_I16
         sub  r6,r6,#12
          strh r10,[r3,#sregE]
          b   topZ80
@ 0x12 LD (DE),A
op_ld_de_a: ldrh r9,[r3,#sregE] 
            mov  r10,r8
             sub  r6,r6,#8
            M_WRB
            b   topZ80
@ 0x13 INC DE
op_inc_de: ldrh r10,[r3,#sregE] 
             sub  r6,r6,#8
            add r10,r10,#1
           strh r10,[r3,#sregE]
           b   topZ80
@ 0x14 INC D
op_inc_d: ldrb r10,[r3,#sregD] 
          M_INC
         sub  r6,r6,#4
          strb r10,[r3,#sregD]
          b   topZ80
@ 0x15 DEC D
op_dec_d: ldrb r10,[r3,#sregD] 
          M_DEC
         sub  r6,r6,#4
          strb r10,[r3,#sregD]
          b   topZ80
@ 0x16 LD D,n
op_ld_d_n:   
           M_EA_I8
         sub  r6,r6,#8
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x17 RLA
op_rla:   
         M_RLA
         sub  r6,r6,#4
         b   topZ80
@ 0x18 JR
op_jr:   
       M_EA_REL8
       sub  r6,r6,#12
@ this fix is need for Centipede which jumps from 0x0040 to 0xfffa
       mov  r0,r7        @ see if we cross a 64k boundary
       add  r7,r7,r9
       eor  r0,r0,r7
       tst  r0,#0x10000    @ wrapped around?
       beq  topZ80        @ nope, normal branch
@ fix the base address
       bic  r7,r7,#0x00ff0000    @ clear top 16 bits
       bic  r7,r7,#0xff000000
       mov  r1,r7,LSR #12    @ shift down to get offset from table
       ldr   r2,[r4,r1,LSL #2]    @ get offset from table
       add   r7,r7,r2            @ form the complete address
       b    topZ80
@ 0x19 ADD HL,DE
op_add_hl_de: ldrh r10,[r3,#sregE] 
         sub  r6,r6,#8
         M_ADDHL
         b   topZ80
@ 0x1A LD A,(DE)
op_ld_a_de: ldrh r9,[r3,#sregE] 
         sub  r6,r6,#8
          M_RDB
          mov r8,r10
          b   topZ80
@ 0x1B  DEC DE      - no flags affected
op_dec_de: ldrh r10,[r3,#sregE] 
         sub  r6,r6,#8
          sub r10,r10,#1
          strh r10,[r3,#sregE]
          b   topZ80
@ 0x1C INC E
op_inc_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
         M_INC
         strb r10,[r3,#sregE]
         b   topZ80
@ 0x1D DEC E
op_dec_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
        M_DEC
        strb r10,[r3,#sregE]
        b   topZ80
@ 0x1E LD E,n
op_ld_e_n:   
        M_EA_I8
         sub  r6,r6,#8
        strb r10,[r3,#sregE]
        b   topZ80
@ 0x1F RRA
op_rra:   
         sub  r6,r6,#4
        M_RRA
        b   topZ80
@ 0x20 JR NZ
op_jr_nz:   
         sub  r6,r6,#8
        M_EA_REL8
        tst r12,#F_ZERO
        bne topZ80
        add r7,r7,r9      @ take the branch
        sub r6,r6,#4      @ 4 clocks if taken
        b   topZ80
@ 0x21 LD HL,nn
op_ld_hl_nn:   
       M_EA_I16
         sub  r6,r6,#12
       mov r11,r10
       b   topZ80
@ 0x22 LD (HL++),A
op_ld_hlipp_a:   
        mov  r9,r11    @ get HL in temp addr reg
         sub  r6,r6,#8
         mov r10,r8        @ get A in temp data reg
        M_WRB
        add  r11,r11,#1        @ inc HL
        b    topZ80
@ 0x23 INC HL
op_inc_hl: add r11,r11,#1 
         sub  r6,r6,#8
        b    topZ80
@ 0x24 INC H
op_inc_h: mov r10,r11,LSR #8
        M_INC
        and r10,r10,#0xff
        and r11,r11,#0xff
         sub  r6,r6,#4
        orr r11,r11,r10,LSL #8
        b    topZ80
@ 0x25 DEC H
op_dec_h: mov r10,r11,LSR #8
        M_DEC
        and r10,r10,#0xff
        and r11,r11,#0xff
         sub  r6,r6,#4
        orr r11,r11,r10,LSL #8
        b    topZ80
@ 0x26 LD H,n
op_ld_h_n:   
        M_EA_I8
        and r11,r11,#0xff
         sub  r6,r6,#8
        orr r11,r11,r10,LSL #8
        b    topZ80
@ 0x27 DAA
op_daa:   
         sub  r6,r6,#4
        mov     r9,#0            @ new carry flag
          mov        r0,r8            @ cf
        and        r1,r8,#0xf0        @ msn
        and     r14,r8,#0x0f        @ lsn
        tst        r12,#F_ADDSUB    @ different for subtraction
        beq        daa_add
@ do for subtraction (DAS)
        mov        r10,#0
        cmp        r14,#9
        movhi    r10,#6
        tst     r12,#F_HALFCARRY
        movne   r10,#6
        sub     r0,r0,r10
        mov     r10,#0
        cmp        r0,#0x9f
        movhi   r10,#0x60
        tst     r12,#F_CARRY
        movne   r10,#0x60
        sub     r0,r0,r10
        cmp     r10,#0
        movne    r9,#F_CARRY
        b        das_1            @ common exit
@ do for addition
daa_add:   
        mov        r10,#0
        cmp        r14,#9
        movhi   r10,#6
        tst     r12,#F_HALFCARRY
        movne   r10,#6
        add     r0,r0,r10
        mov     r10,#0
        cmp        r0,#0x9f
        movhi   r10,#0x60
        tst     r12,#F_CARRY
        movne   r10,#0x60
        add     r0,r0,r10
        cmp        r10,#0
        movne    r9,#F_CARRY
das_1:   
        and     r12,r12,#255-(F_HALFCARRY + F_CARRY + F_ZERO)
        and        r8,r0,#0xff        @ new value of A
        orr     r12,r12,r9        @ add back carry if set
        tst     r8,#0xff
        orreq   r12,r12,#F_ZERO
        b    topZ80
@ 0x28 JR Z
op_jr_z:   
        M_EA_REL8
         sub  r6,r6,#8
        tst  r12,#F_ZERO
        beq  topZ80
        add  r7,r7,r9
        sub  r6,r6,#4
        b    topZ80
@ 0x29 ADD HL,HL
op_add_hl_hl: mov r10,r11 
         sub  r6,r6,#8
        M_ADDHL
        b    topZ80
@ 0x2A LD A,(HL++)
op_ld_a_hlipp:   
        mov  r9,r11        @ get temp address
        M_RDB
         sub  r6,r6,#8
         mov r8,r10        @ new data in A
        add  r11,r11,#1        @ HL++
        b    topZ80
@ 0x2B DEC HL
op_dec_hl: sub r11,r11,#1 
         sub  r6,r6,#8
        b    topZ80
@ 0x2C INC L
op_inc_l: mov r10,r11 
        M_INC
        and  r10,r10,#0xff
        and  r11,r11,#0xff00
         sub  r6,r6,#4
        orr  r11,r11,r10
        b    topZ80
@ 0x2D DEC L
op_dec_l: mov r10,r11 
        M_DEC
        and  r10,r10,#0xff
        and  r11,r11,#0xff00
         sub  r6,r6,#4
        orr  r11,r11,r10
        b    topZ80
@ 0x2E LD L,n
op_ld_l_n:   
        M_EA_I8
        and  r10,r10,#0xff
        and  r11,r11,#0xff00
         sub  r6,r6,#8
        orr  r11,r11,r10
        b    topZ80
@ 0x2F CPL
op_cpl: mvn r8,r8 
         sub  r6,r6,#4
        orr  r12,r12,#(F_HALFCARRY + F_ADDSUB)
        b    topZ80
@ 0x30 JR NC
op_jr_nc:   
        M_EA_REL8
         sub  r6,r6,#8
        tst  r12,#F_CARRY
        bne  topZ80
        add  r7,r7,r9
        sub  r6,r6,#4
        b    topZ80
@ 0x31 LD SP,nn
op_ld_sp_nn:   
        M_EA_I16
         sub  r6,r6,#12
        strh r10,[r3,#sregSP]
        b    topZ80
@ 0x32 LD (HL--),A
op_ld_hlimm_a:   
        mov  r9,r11        @ get addr in r9
        mov  r10,r8
         sub  r6,r6,#8
        M_WRB
        sub  r11,r11,#1        @ HL--
        b    topZ80
@ 0x33 INC SP
op_inc_sp: ldrh r10,[r3,#sregSP] 
         sub  r6,r6,#8
        add  r10,r10,#1
        strh r10,[r3,#sregSP]
        b    topZ80
@ 0x34 INC (HL)
op_inc_hli: mov r9,r11 
        M_RDB
         sub  r6,r6,#12
        M_INC
        M_WRB
        b    topZ80
@ 0x35 DEC (HL)
op_dec_hli: mov r9,r11 
        M_RDB
         sub  r6,r6,#12
        M_DEC
        M_WRB
        b   topZ80
@ 0x36 LD (HL),n
op_ld_hl_n:   
        M_EA_I8
        mov  r9,r11
         sub  r6,r6,#12
        M_WRB
        b    topZ80
@ 0x37 SCF
op_scf: orr r12,r12,#F_CARRY 
         sub  r6,r6,#4
        and r12,r12,#(255-(F_HALFCARRY + F_ADDSUB))
        b    topZ80
@ 0x38 JR C
op_jr_c:   
        M_EA_REL8
         sub  r6,r6,#8
        tst r12,#F_CARRY
        beq topZ80
        add r7,r7,r9
        sub r6,r6,#4     @ 4 clocks for jump taken
        b   topZ80
@ 0x39 ADD HL,SP
op_add_hl_sp: ldrh r10,[r3,#sregSP] 
         sub  r6,r6,#8
        M_ADDHL
        b    topZ80
@ 0x3A LD A,(HL--)
op_ld_a_hlimm:   
        mov  r9,r11
        M_RDB
         sub  r6,r6,#8
        mov  r8,r10
        sub  r11,r11,#1        @ HL--
        b    topZ80
@ 0x3B DEC SP
op_dec_sp: ldrh r10,[r3,#sregSP] 
         sub  r6,r6,#8
        sub r10,r10,#1
        strh r10,[r3,#sregSP]
        b   topZ80
@ 0x3C INC A
op_inc_a: mov r10,r8 
         sub  r6,r6,#4
        M_INC
        mov  r8,r10
        b    topZ80
@ 0x3D DEC A
op_dec_a: mov r10,r8 
         sub  r6,r6,#4
        M_DEC
        mov  r8,r10
        b    topZ80
@ 0x3E LD A,n
op_ld_a_n:   
        M_EA_I8
         sub  r6,r6,#8
        mov r8,r10
        b   topZ80
@ 0x3F CCF
op_ccf: eor r12,r12,#F_CARRY @ carry is complimented
        and r12,r12,#255-(F_ADDSUB + F_HALFCARRY)
        sub  r6,r6,#4
        b    topZ80
@ 0x40 LD B,B - nop
op_ld_b_b:   
         sub  r6,r6,#4
        b   topZ80
@ 0x41 LD B,C
op_ld_b_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x42 LD B,D
op_ld_b_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x43 LD B,E
op_ld_b_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x44 LD B,H
op_ld_b_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          strb r10,[r3,#sregB]
          b   topZ80
@ 0x45 LD B,L
op_ld_b_l: strb r11,[r3,#sregB] 
         sub  r6,r6,#4
          b   topZ80
@ 0x46 LD B,(HL)
op_ld_b_hl: mov r9,r11 
           M_RDB
         sub  r6,r6,#8
           strb r10,[r3,#sregB]
           b   topZ80
@ 0x47 LD B,A
op_ld_b_a: strb r8,[r3,#sregB] 
         sub  r6,r6,#4
          b   topZ80
@ 0x48 LD C,B
op_ld_c_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x49 LD C,C
op_ld_c_c:   
         sub  r6,r6,#4
         b   topZ80
@ 0x4A LD C,D
op_ld_c_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x4B LD C,E
op_ld_c_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x4C LD C,H
op_ld_c_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x4D LD C,L
op_ld_c_l: strb r11,[r3,#sregC] 
         sub  r6,r6,#4
           b   topZ80
@ 0x4E LD C,(HL)
op_ld_c_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           strb r10,[r3,#sregC]
           b   topZ80
@ 0x4F LD C,A
op_ld_c_a: strb r8,[r3,#sregC] 
         sub  r6,r6,#4
           b   topZ80
@ 0x50 LD D,B
op_ld_d_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x51 LD D,C
op_ld_d_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x52 LD D,D
op_ld_d_d:   
         sub  r6,r6,#4
          b   topZ80
@ 0x53 LD D,E
op_ld_d_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x54 LD D,H
op_ld_d_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x55 LD D,L
op_ld_d_l: strb r11,[r3,#sregD] 
         sub  r6,r6,#4
           b   topZ80
@ 0x56 LD D,(HL)
op_ld_d_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           strb r10,[r3,#sregD]
           b   topZ80
@ 0x57 LD D,A
op_ld_d_a: strb r8,[r3,#sregD] 
         sub  r6,r6,#4
           b   topZ80
@ 0x58 LD E,B
op_ld_e_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
           strb r10,[r3,#sregE]
           b   topZ80
@ 0x59 LD E,C
op_ld_e_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
           strb r10,[r3,#sregE]
           b   topZ80
@ 0x5A LD E,D
op_ld_e_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
           strb r10,[r3,#sregE]
           b   topZ80
@ 0x5B LD E,E
op_ld_e_e:   
         sub  r6,r6,#4
          b   topZ80
@ 0x5C LD E,H
op_ld_e_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
           strb r10,[r3,#sregE]
           b   topZ80
@ 0x5D LD E,L
op_ld_e_l: strb r11,[r3,#sregE] 
         sub  r6,r6,#4
           b   topZ80
@ 0x5E LD E,(HL)
op_ld_e_hl: mov r9,r11 
         sub  r6,r6,#8
            M_RDB
            strb r10,[r3,#sregE]
            b   topZ80
@ 0x5F LD E,A
op_ld_e_a: strb r8,[r3,#sregE] 
         sub  r6,r6,#4
           b   topZ80
@ 0x60 LD H,B
op_ld_h_b: ldrb r10,[r3,#sregB] 
           and r11,r11,#0xff
         sub  r6,r6,#4
           orr r11,r11,r10,LSL #8
           b   topZ80
@ 0x61 LD H,C
op_ld_h_c: ldrb r10,[r3,#sregC] 
           and r11,r11,#0xff
         sub  r6,r6,#4
           orr r11,r11,r10,LSL #8
           b   topZ80
@ 0x62 LD H,D
op_ld_h_d: ldrb r10,[r3,#sregD] 
           and r11,r11,#0xff
         sub  r6,r6,#4
           orr r11,r11,r10,LSL #8
           b   topZ80
@ 0x63 LD H,E
op_ld_h_e: ldrb r10,[r3,#sregE] 
           and r11,r11,#0xff
         sub  r6,r6,#4
           orr r11,r11,r10,LSL #8
           b   topZ80
@ 0x64 LD H,H
op_ld_h_h:   
         sub  r6,r6,#4
          b   topZ80
@ 0x65 LD H,L
op_ld_h_l: and r11,r11,#0xff 
         sub  r6,r6,#4
           orr r11,r11,r11,LSL #8
           b   topZ80
@ 0x66 LD H,(HL)
op_ld_h_hl: mov r9,r11 
            M_RDB
            and r11,r11,#0xff
         sub  r6,r6,#8
            orr r11,r11,r10,LSL #8
            b   topZ80
@ 0x67 LD H,A
op_ld_h_a: and r11,r11,#0xff 
         sub  r6,r6,#4
           orr r11,r11,r8,LSL #8
           b   topZ80
@ 0x68 LD L,B
op_ld_l_b: ldrb r10,[r3,#sregB] 
           and r11,r11,#0xff00
         sub  r6,r6,#4
           orr r11,r11,r10
           b   topZ80
@ 0x69 LD L,C
op_ld_l_c: ldrb r10,[r3,#sregC] 
           and r11,r11,#0xff00
         sub  r6,r6,#4
           orr r11,r11,r10
           b   topZ80
@ 0x6A LD L,D
op_ld_l_d: ldrb r10,[r3,#sregD] 
           and r11,r11,#0xff00
         sub  r6,r6,#4
           orr r11,r11,r10
           b   topZ80
@ 0x6B LD L,E
op_ld_l_e: ldrb r10,[r3,#sregE] 
           and r11,r11,#0xff00
         sub  r6,r6,#4
           orr r11,r11,r10
           b   topZ80
@ 0x6C LD L,H
op_ld_l_h: and r11,r11,#0xff00 
         sub  r6,r6,#4
           orr r11,r11,r11,LSR #8
           b   topZ80
@ 0x6D LD L,L
op_ld_l_l:   
         sub  r6,r6,#4
        b   topZ80
@ 0x6E LD L,(HL)
op_ld_l_hl: mov r9,r11 
           M_RDB
           and r10,r10,#0xff
           and r11,r11,#0xff00
         sub  r6,r6,#8
           orr r11,r11,r10
           b   topZ80
@ 0x6F LD L,A
op_ld_l_a: and r11,r11,#0xff00 
           and r8,r8,#0xff
         sub  r6,r6,#4
           orr r11,r11,r8
           b   topZ80
@ 0x70 LD (HL),B
op_ld_hl_b:   
           ldrb r10,[r3,#sregB]
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x71 LD (HL),C
op_ld_hl_c:   
           ldrb r10,[r3,#sregC]
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x72 LD (HL),D
op_ld_hl_d:   
           ldrb r10,[r3,#sregD]
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x73 LD (HL),E
op_ld_hl_e:   
           ldrb r10,[r3,#sregE]
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x74 LD (HL),H
op_ld_hl_h:   
           mov r10,r11,LSR #8
           mov r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x75 LD (HL),L
op_ld_hl_l: mov r10,r11 
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x76 HALT
op_halt: eor r6,r6,r6 @ set clock count to 0 to exit immediately
        sub r7,r7,#1        @ stick on this instruction until an interrupt
        mov r0,#1
        strb r0,[r3,#sregHALT]    @ set flag indicating we are stuck in a HALTED state
        b   topZ80
@ 0x77 LD (HL),A
op_ld_hl_a:   
           mov  r10,r8
           mov  r9,r11
         sub  r6,r6,#8
           M_WRB
           b   topZ80
@ 0x78 LD A,B
op_ld_a_b: ldrb r8,[r3,#sregB] 
         sub  r6,r6,#4
           b   topZ80
@ 0x79 LD A,C
op_ld_a_c: ldrb r8,[r3,#sregC] 
         sub  r6,r6,#4
           b   topZ80
@ 0x7A LD A,D
op_ld_a_d: ldrb r8,[r3,#sregD] 
         sub  r6,r6,#4
           b   topZ80
@ 0x7B LD A,E
op_ld_a_e: ldrb r8,[r3,#sregE] 
         sub  r6,r6,#4
           b   topZ80
@ 0x7C LD A,H
op_ld_a_h: mov r8,r11,LSR #8
         sub  r6,r6,#4
           b   topZ80
@ 0x7D LD A,L
op_ld_a_l: mov r8,r11 
         sub  r6,r6,#4
           b   topZ80
@ 0x7E LD A,(HL)
op_ld_a_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           mov  r8,r10
           b   topZ80
@ 0x7F LD A,A
op_ld_a_a:   
         sub  r6,r6,#4
          b   topZ80
@ 0x80 ADD B
op_add_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
           M_ADD
           b   topZ80
@ 0x81 ADD C
op_add_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_ADD
          b   topZ80
@ 0x82 ADD D
op_add_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_ADD
          b   topZ80
@ 0x83 ADD E
op_add_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_ADD
          b   topZ80
@ 0x84 ADD H
op_add_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_ADD
          b   topZ80
@ 0x85 ADD L
op_add_l: mov r10,r11 
         sub  r6,r6,#4
          M_ADD
          b   topZ80
@ 0x86 ADD (HL)
op_add_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_ADD
           b   topZ80
@ 0x87 ADD A
op_add_a: mov r10,r8 
         sub  r6,r6,#4
           M_ADD
           b   topZ80
@ 0x88 ADC B
op_adc_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x89 ADC C
op_adc_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x8A ADC D
op_adc_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x8B ADC E
op_adc_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x8C ADC H
op_adc_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x8D ADC L
op_adc_l: mov r10,r11 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x8E ADC (HL)
op_adc_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_ADC
           b   topZ80
@ 0x8F ADC A
op_adc_a: mov r10,r8 
         sub  r6,r6,#4
          M_ADC
          b   topZ80
@ 0x90 SUB B
op_sub_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x91 SUB C
op_sub_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x92 SUB D
op_sub_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x93 SUB E
op_sub_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x94 SUB H
op_sub_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x95 SUB L
op_sub_l: mov r10,r11 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x96 SUB (HL)
op_sub_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_SUB
           b   topZ80
@ 0x97 SUB A
op_sub_a: mov r10,r8 
         sub  r6,r6,#4
          M_SUB
          b   topZ80
@ 0x98 SBC B
op_sbc_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x99 SBC C
op_sbc_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x9A SBC D
op_sbc_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x9B SBC E
op_sbc_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x9C SBC H
op_sbc_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x9D SBC L
op_sbc_l: mov r10,r11 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0x9E SBC (HL)
op_sbc_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_SBC
           b   topZ80
@ 0x9F SBC A
op_sbc_a: mov r10,r8 
         sub  r6,r6,#4
          M_SBC
          b   topZ80
@ 0xA0 AND B
op_and_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
           M_AND
           b   topZ80
@ 0xA1 AND C
op_and_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA2 AND D
op_and_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA3 AND E
op_and_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA4 AND H
op_and_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA5 AND L
op_and_l: mov r10,r11 
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA6 AND (HL)
op_and_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_AND
           b   topZ80
@ 0xA7 AND A
op_and_a: mov r10,r8 
         sub  r6,r6,#4
          M_AND
          b   topZ80
@ 0xA8 XOR B
op_xor_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xA9 XOR C
op_xor_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xAA XOR D
op_xor_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xAB XOR E
op_xor_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xAC XOR H
op_xor_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xAD XOR L
op_xor_l: mov r10,r11 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xAE XOR (HL)
op_xor_hl: mov r9,r11 
         sub  r6,r6,#8
           M_RDB
           M_XOR
           b   topZ80
@ 0xAF XOR A
op_xor_a: mov r10,r8 
         sub  r6,r6,#4
          M_XOR
          b   topZ80
@ 0xB0 OR B
op_or_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
          M_OR
          b   topZ80
@ 0xB1 OR C
op_or_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB2 OR D
op_or_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB3 OR E
op_or_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB4 OR H
op_or_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB5 OR L
op_or_l: mov r10,r11 
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB6 OR (HL)
op_or_hl: mov r9,r11 
         sub  r6,r6,#8
          M_RDB
          M_OR
          b   topZ80
@ 0xB7 OR A
op_or_a: mov r10,r8 
         sub  r6,r6,#4
         M_OR
         b   topZ80
@ 0xB8 CP B
op_cp_b: ldrb r10,[r3,#sregB] 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xB9 CP C
op_cp_c: ldrb r10,[r3,#sregC] 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xBA CP D
op_cp_d: ldrb r10,[r3,#sregD] 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xBB CP E
op_cp_e: ldrb r10,[r3,#sregE] 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xBC CP H
op_cp_h: mov r10,r11,LSR #8
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xBD CP L
op_cp_l: mov r10,r11 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xBE CP (HL)
op_cp_hl: mov r9,r11 
         sub  r6,r6,#8
          M_RDB
          M_CP
          b   topZ80
@ 0xBF CP A
op_cp_a: mov r10,r8 
         sub  r6,r6,#4
         M_CP
         b   topZ80
@ 0xC0 RET NZ
op_ret_nz: tst r12,#F_ZERO 
         sub  r6,r6,#8
           bne  topZ80
           M_POPPC
           sub  r6,r6,#12
           b   topZ80
@ 0xC1 POP BC
op_pop_bc:   
           M_POPW
         sub  r6,r6,#12
           strh r10,[r3,#sregC]
           b   topZ80
@ 0xC2 JP NZ
op_jp_nz:   
          M_EA_A
         sub  r6,r6,#12
          tst  r12,#F_ZERO
          bne  topZ80
          sub  r6,r6,#4
          mov  r7,r9,LSL #16
          mov  r7,r7,LSR #16        @ trim to 16-bits
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
          b   topZ80
@ 0xC3 JP (nn)
op_jp_nn:   
          M_EA_A
         sub  r6,r6,#16
          mov r7,r9,LSL #16
          mov r7,r7,LSR #16        @ trim to 16-bits
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
          b   topZ80
@ 0xC4 CALL NZ
op_call_nz:   
           M_EA_A
         sub  r6,r6,#12
           tst r12,#F_ZERO
           bne topZ80
           sub r6,r6,#12
           M_PUSHPC
           mov r7,r9,LSL #16
           mov r7,r7,LSR #16        @ trim to 16-bits
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
           b   topZ80
@ 0xC5 PUSH BC
op_push_bc: ldrh r10,[r3,#sregC] 
         sub  r6,r6,#16
           M_PUSHW
           b   topZ80
@ 0xC6 ADD n
op_add_n:   
           M_EA_I8
         sub  r6,r6,#8
           M_ADD
           b   topZ80
@ 0xC7 RST 0H
op_rst_0:   
          M_PUSHPC
         sub  r6,r6,#16
          ldr   r7,[r4]        @ get bank 0
          mov   r2,r7
          b   topZ80
@ 0xC8 RET Z
op_ret_z: tst r12,#F_ZERO 
         sub  r6,r6,#8
          beq topZ80
          sub r6,r6,#12
          M_POPPC
          b   topZ80
@ 0xC9 RET
op_ret:   
         sub  r6,r6,#16
        M_POPPC
        b   topZ80
@ 0xCA JP Z
op_jp_z:   
        M_EA_A
         sub  r6,r6,#12
        tst r12,#F_ZERO
        beq  topZ80
        mov r7,r9,LSL #16
        mov r7,r7,LSR #16        @ trim to 16-bits
        sub r6,r6,#4
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
        b   topZ80
@ 0xCC CALL Z
op_call_z:   
           M_EA_A
         sub  r6,r6,#12
           tst r12,#F_ZERO
           beq  topZ80
           M_PUSHPC
           sub r6,r6,#12
           mov r7,r9,LSL #16
           mov r7,r7,LSR #16        @ trim to 16-bits
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
           b   topZ80
@ 0xCD CALL (nn)
op_call:   
         M_EA_A
         sub  r6,r6,#24
         M_PUSHPC
         mov r7,r9,LSL #16
         mov r7,r7,LSR #16        @ trim to 16-bits
          mov   r1,r7,LSR #12        @ shift down to get offset from table
          ldr   r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
         b   topZ80
@ 0xCE ADC n
op_adc_n:   
           M_EA_I8
         sub  r6,r6,#8
           M_ADC
           b   topZ80
@ 0xCF RST 8H
op_rst_8:   
          M_PUSHPC
         sub  r6,r6,#16
          mov r7,#8        @ PC = 8
           ldr   r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
          b   topZ80
@ 0xD0 RET NC
op_ret_nc: tst r12,#F_CARRY 
         sub  r6,r6,#8
           bne topZ80
           M_POPPC
           sub r6,r6,#12
           b   topZ80
@ 0xD1 POP DE
op_pop_de:   
           M_POPW
         sub  r6,r6,#10
           strh r10,[r3,#sregE]
           b   topZ80
@ 0xD2 JP NC
op_jp_nc:   
          M_EA_A
         sub  r6,r6,#12
          tst  r12,#F_CARRY
          bne  topZ80
          mov r7,r9,LSL #16
          mov r7,r7,LSR #16        @ trim to 16-bits
          sub r6,r6,#4
          mov r1,r7,LSR #12        @ shift down to get offset from table
          ldr r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
          b   topZ80
@ 0xD3 NOP
@ 0xD4 CALL NC
op_call_nc:   
           M_EA_A
         sub  r6,r6,#12
           tst r12,#F_CARRY
           bne topZ80
           M_PUSHPC
           sub r6,r6,#12
           mov r7,r9,LSL #16
           mov r7,r7,LSR #16        @ trim to 16-bits
          mov r1,r7,LSR #12        @ shift down to get offset from table
          ldr r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
           b   topZ80
@ 0xD5 PUSH DE
op_push_de: ldrh r10,[r3,#sregE] 
         sub  r6,r6,#16
           M_PUSHW
           b   topZ80
@ 0xD6 SUB n
op_sub_n:   
           M_EA_I8
         sub  r6,r6,#8
           M_SUB
           b   topZ80
@ 0xD7 RST 10H
op_rst_10:   
           M_PUSHPC
         sub  r6,r6,#16
           mov r7,#0x10
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@ 0xD8 RET C
op_ret_c: tst r12,#F_CARRY 
         sub  r6,r6,#8
          beq  topZ80
          sub  r6,r6,#12
          M_POPPC
          b   topZ80
@ 0xD9 RETI
op_reti:   
          mov r0,#1
          sub  r6,r6,#16
          strb r0,[r3,#sregI]        @ re-enable interrupts
          M_POPPC
          b   topZ80
@ 0xDA JP C
op_jp_c:   
          M_EA_A
         sub  r6,r6,#12
          tst r12,#F_CARRY
          beq  topZ80
          mov r7,r9,LSL #16
          mov r7,r7,LSR #16        @ trim to 16-bits
          sub r6,r6,#4
          mov r1,r7,LSR #12        @ shift down to get offset from table
          ldr r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
          b   topZ80
@ 0xDB NOP
@ 0xDC CALL C,nn
op_call_c:   
           M_EA_A
         sub  r6,r6,#12
           tst r12,#F_CARRY
           beq  topZ80
           M_PUSHPC
           mov r7,r9,LSL #16
           mov r7,r7,LSR #16        @ trim to 16-bits
           sub r6,r6,#12
          mov r1,r7,LSR #12        @ shift down to get offset from table
          ldr r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
           b   topZ80
@ 0xDD NOP
@ 0xDE SBC A,n
op_sbc_n:   
           M_EA_I8
         sub  r6,r6,#8
           M_SBC
           b   topZ80
@ 0xDF RST 18H
op_rst_18:   
           M_PUSHPC
         sub  r6,r6,#16
           mov  r7,#0x18
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@ 0xE0 LD (FFXX),A
op_ld_ffxx_a:   
           ldrb r0,[r7],#1        @ get IO address
           sub r6,r6,#12
           cmp  r0,#0x80        @ RAM?
           blt  do_io_1
           cmp  r0,#0xff        @ interrupt enable?
           streqb r8,[r3,#ucIRQEnable] @ RegA = new interrupt mask
           orr  r0,r0,#0xff00    @ form complete address
           ldr  r1,[r3,#mem_map]    @ and store in RAM
           strb r8,[r1,r0]
           beq  topZ80int        @ check for pending interrupts
           b    topZ80
@ Write to IO
do_io_1: orr r0,r0,#0xff00 @ form complete address
           mov  r1,r8            @ get A as second parameter
           stmfd sp!,{r2,r3,r12}    @ need to preserve these
           bl   GBIOWrite
           ldmia sp!,{r2,r3,r12}    @ restore these
@ DEBUG check for DMA
           b    topZ80
@ 0xE1 POP HL
op_pop_hl:   
           M_POPW
           sub r6,r6,#12
           mov r11,r10
           b   topZ80
@ 0xE2 LD (0xFF00 + C),A
op_ld_ffcc_a:   
           ldrb r0,[r3,#sregC]
           sub r6,r6,#8
           cmp r0,#0x80        @ write to RAM?
           blt do_io_0
           cmp r0,#0xff        @ interrupt enable?
           streqb r8,[r3,#ucIRQEnable] @ RegA = new interrupt mask
           orr  r0,r0,#0xff00    @ form complete address
           ldr  r1,[r3,#mem_map]    @ and store in RAM
           strb r8,[r1,r0]
           beq  topZ80int        @ check interrupts pending
           b    topZ80
@ write to IO
do_io_0: orr r0,r0,#0xff00 @ form complete address
          mov r1,r8            @ get A as second paramter
          stmfd sp!,{r2,r3,r12}    @ need to preserve these
          bl  GBIOWrite
          ldmia sp!,{r2,r3,r12} @ restore these
@ DEBUG check for DMA
          b   topZ80
@ 0xE3 EX (SP),HL
op_ex_sp_hl: ldrh r9,[r3,#sregSP] 
           sub r6,r6,#12
           M_RDW
           mov r0,r10
           mov r10,r11
           mov r11,r0
           M_WRW
           b   topZ80
@ 0xE4 NOP
@ 0xE5 PUSH HL
op_push_hl: mov r10,r11 
           sub r6,r6,#16
           M_PUSHW
           b   topZ80
@ 0xE6 AND n
op_and_n:   
           M_EA_I8
           sub r6,r6,#8
           M_AND
           b   topZ80
@ 0xE7 RST 20H
op_rst_20:   
           M_PUSHPC
           sub r6,r6,#16
           mov r7,#0x20
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@ 0xE8 ADD SP,n
op_add_sp_n:   
           M_EA_REL8
           ldrh r0,[r3,#sregSP]
           sub r6,r6,#16
           add r0,r0,r9        @ add sign extended value
           strh r0,[r3,#sregSP]
           b   topZ80
@ 0xE9 JP (HL)
op_jp_hl:   
           mov r7,r11,LSL #16
           sub r6,r6,#4
           mov r7,r7,LSR #16        @ trim to 16-bits
          mov r1,r7,LSR #12        @ shift down to get offset from table
          ldr r2,[r4,r1,LSL #2]    @ get offset from table
          add   r7,r7,r2            @ form the complete address
           b   topZ80
@ 0xEA LD (nn),A
op_ld_nn_a:   
           M_EA_A
           sub r6,r6,#16
           mov r10,r8
           M_WRB
           b   topZ80
@ 0xEB NOP
@ 0xEC NOP
@ 0xED NOP
@ 0xEE XOR n
op_xor_n:   
           M_EA_I8
           sub r6,r6,#8
           M_XOR
           b   topZ80
@ 0xEF RST 28H
op_rst_28:   
           M_PUSHPC
           sub r6,r6,#16
           mov r7,#0x28
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@ 0xF0 LD A,(FFXX)
op_ld_a_ffxx:   
          ldrb r0,[r7],#1        @ get lower byte of address
          sub r6,r6,#12
          cmp  r0,#0x80            @ RAM or I/O space?
          ldrhs r1,[r3,#mem_map]
          orr  r0,r0,#0xff00    @ form complete address
          ldrhsb r8,[r1,r0]        @ read from RAM
          bhs  topZ80            @ done
@ need to read from I/O port
          stmfd sp!,{r2,r3,r12}        @ we need these preserved
          bl    GBIORead        @ call read handler with addr in R0
          ldmia sp!,{r2,r3,r12}        @ restore these
          mov  r8,r0            @ get result back in A
          b   topZ80
@ 0xF1 POP AF
op_pop_af:   
           M_POPW
           sub r6,r6,#12
           mov r12,r10        @ high and low bytes of ECX are reversed
           mov r8,r10,LSR #8
           b   topZ80
@ 0xF2 LD A,(FF00+C)
op_ld_a_ffcc:   
         ldrb r0,[r3,#sregC]
         sub r6,r6,#8
         cmp  r0,#0x80        @ RAM or IO?
         orr  r0,r0,#0xff00    @ form complete address
         ldrhs r1,[r3,#mem_map]
         ldrhsb r8,[r1,r0]    @ read from RAM
         bhs topZ80        @ done
@ need to read from I/O
         stmfd sp!,{r2,r3,r12}        @ we need these preserved
         bl  GBIORead    @ call with addr in R0
         ldmia sp!,{r2,r3,r12}        @ restore these
         mov r8,r0        @ get result in A
         b   topZ80
@ 0xF3 DI
op_di: eor r0,r0,r0 
       sub r6,r6,#4
       strb r0,[r3,#sregI]
       ldr  r0,[r3,#eiticks]    @ is this immediately after an EI?
       orrs r0,r0,r0
       beq topZ80
       add r6,r6,r0                @ adjust tick counter
       eor r0,r0,r0
       str r0,[r3,#eiticks]        @ reset EI flag
       b   topZ80
@ 0xF4 NOP
@ 0xF5 PUSH AF
op_push_af: and r10,r12,#0xff 
           sub r6,r6,#16
            orr r10,r10,r8,LSL #8
           M_PUSHW
           b   topZ80
@ 0xF6 OR n
op_or_n:   
          M_EA_I8
           sub r6,r6,#8
          M_OR
          b   topZ80
@ 0xF7 RST 30H
op_rst_30:   
           M_PUSHPC
           sub r6,r6,#16
           mov r7,#0x30
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@ 0xF8 LD HL,SP+imm
op_ld_hl_sp_n:   
          M_EA_REL8            @ get sign extended byte
          ldrh r11,[r3,#sregSP]
           sub r6,r6,#12
          add r11,r11,r9    @ HL = SP+imm
          b   topZ80
@ 0xF9 LD SP,HL
op_ld_sp_hl: strh r11,[r3,#sregSP] 
           sub r6,r6,#8
           b   topZ80
@ 0xFA LD A,(nn)
op_ld_a_nn:   
          M_EA_A
           sub r6,r6,#16
           M_RDB
           mov r8,r10
          b   topZ80
@ 0xFB EI
op_ei: mov r0,#1 
        sub r6,r6,#4
        strb r0,[r3,#sregI]
        sub r0,r6,#1        @ save tick count in special flag
        mov r6,#1            @ allow 1 more instruction to execute
        str r0,[r3,#eiticks]    @ special handling of EI
        b   topZ80
@ 0xFC NOP
@ 0xFD NOP
@ 0xFE CP n
op_cp_n:   
          M_EA_I8
           sub r6,r6,#8
          M_CP
          b   topZ80
@ 0xFF RST 38H
op_rst_38:   
           M_PUSHPC
           sub r6,r6,#16
           mov r7,#0x38
           ldr r2,[r4]        @ get bank 0
           add r7,r7,r2            @ form complete address
           b   topZ80
@
@ CBXX opcodes
@
@ 0xCB00 RLC B
op_rlc_b: ldrb r10,[r3,#sregB] 
         M_RLC
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB01 RLC C
op_rlc_c: ldrb r10,[r3,#sregC] 
         M_RLC
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB02 RLC D
op_rlc_d: ldrb r10,[r3,#sregD] 
         M_RLC
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB03 RLC E
op_rlc_e: ldrb r10,[r3,#sregE] 
         M_RLC
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB04 RLC H
op_rlc_h: mov r10,r11,LSR #8
         M_RLC
         and  r11,r11,#0xff
         sub r6,r6,#8
         orr  r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB05 RLC L
op_rlc_l: mov r10,r11 
         M_RLC
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB06 RLC (HL)
op_rlc_hl: mov r9,r11 
         M_RDB
         M_RLC
         sub r6,r6,#16
         M_WRB
         b   topZ80
@ 0xCB07 RLC A
op_rlc_a: mov r10,r8 
         M_RLC
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB08 RRC B
op_rrc_b: ldrb r10,[r3,#sregB] 
          M_RRC
          sub r6,r6,#8
          strb r10,[r3,#sregB]
          b   topZ80
@ 0xCB09 RRC C
op_rrc_c: ldrb r10,[r3,#sregC] 
          M_RRC
          sub r6,r6,#8
          strb r10,[r3,#sregC]
          b   topZ80
@ 0xCB0A RRC D
op_rrc_d: ldrb r10,[r3,#sregD] 
          M_RRC
          sub r6,r6,#8
          strb r10,[r3,#sregD]
          b   topZ80
@ 0xCB0B RRC E
op_rrc_e: ldrb r10,[r3,#sregE] 
          M_RRC
          sub r6,r6,#8
          strb r10,[r3,#sregE]
          b   topZ80
@ 0xCB0C RRC H
op_rrc_h: mov r10,r11,LSR #8
          M_RRC
          and r11,r11,#0xff
          sub r6,r6,#8
          orr r11,r11,r10,LSL #8
          b   topZ80
@ 0xCB0D RRC L
op_rrc_l: mov r10,r11 
          M_RRC
          and r10,r10,#0xff
          and r11,r11,#0xff00
          sub r6,r6,#8
          orr r11,r11,r10
          b   topZ80
@ 0xCB0E RRC (HL)
op_rrc_hl: mov r9,r11 
          M_RDB
          M_RRC
          sub r6,r6,#16
          M_WRB
          b   topZ80
@ 0xCB0F RRC A
op_rrc_a:   
          M_RRCA
          sub r6,r6,#8
          b   topZ80
@ 0xCB10 RL B
op_rl_b: ldrb r10,[r3,#sregB] 
         M_RL
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB11 RL C
op_rl_c: ldrb r10,[r3,#sregC] 
         M_RL
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB12 RL D
op_rl_d: ldrb r10,[r3,#sregD] 
         M_RL
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB13 RL E
op_rl_e: ldrb r10,[r3,#sregE] 
         M_RL
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB14 RL H
op_rl_h: mov r10,r11,LSR #8
         M_RL
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB15 RL L
op_rl_l: mov r10,r11 
         M_RL
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB16 RL (HL)
op_rl_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_RL
         M_WRB
         b   topZ80
@ 0xCB17 RL A
op_rl_a: mov r10,r8 
         M_RL
         sub r6,r6,#8
         mov  r8,r10
         b   topZ80
@ 0xCB18 RR B
op_rr_b: ldrb r10,[r3,#sregB] 
         M_RR
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB19 RR C
op_rr_c: ldrb r10,[r3,#sregC] 
         M_RR
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB1A RR D
op_rr_d: ldrb r10,[r3,#sregD] 
         M_RR
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB1B RR E
op_rr_e: ldrb r10,[r3,#sregE] 
         M_RR
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB1C RR H
op_rr_h: mov r10,r11,LSR #8
         M_RR
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB1D RR L
op_rr_l: mov r10,r11 
         M_RR
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB1E RR (HL)
op_rr_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_RR
         M_WRB
         b   topZ80
@ 0xCB1F RR A
op_rr_a: mov r10,r8 
         M_RR
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB20 SLA B
op_sla_b: ldrb r10,[r3,#sregB] 
         M_SLA
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB21 SLA C
op_sla_c: ldrb r10,[r3,#sregC] 
         M_SLA
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB22 SLA D
op_sla_d: ldrb r10,[r3,#sregD] 
         M_SLA
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB23 SLA E
op_sla_e: ldrb r10,[r3,#sregE] 
         M_SLA
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB24 SLA H
op_sla_h: mov r10,r11,LSR #8
         M_SLA
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB25 SLA L
op_sla_l: mov r10,r11 
         M_SLA
         and  r10,r10,#0xff
         and  r11,r11,#0xff00
         sub r6,r6,#8
         orr  r11,r11,r10
         b   topZ80
@ 0xCB26 SLA (HL)
op_sla_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_SLA
         M_WRB
         b   topZ80
@ 0xCB27 SLA A
op_sla_a: mov r10,r8 
         M_SLA
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB28 SRA B
op_sra_b: ldrb r10,[r3,#sregB] 
         M_SRA
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB29 SRA C
op_sra_c: ldrb r10,[r3,#sregC] 
         M_SRA
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB2A SRA D
op_sra_d: ldrb r10,[r3,#sregD] 
         M_SRA
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB2B SRA E
op_sra_e: ldrb r10,[r3,#sregE] 
         M_SRA
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB2C SRA H
op_sra_h: mov r10,r11,LSR #8
         M_SRA
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB2D SRA L
op_sra_l: mov r10,r11 
         M_SRA
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB2E SRA (HL)
op_sra_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_SRA
         M_WRB
         b   topZ80
@ 0xCB2F SRA A
op_sra_a: mov r10,r8 
         M_SRA
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB30 SWAP B
op_swap_b: ldrb r10,[r3,#sregB] 
         M_SWAP
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB31 SWAP C
op_swap_c: ldrb r10,[r3,#sregC] 
         M_SWAP
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB32 SWAP D
op_swap_d: ldrb r10,[r3,#sregD] 
         M_SWAP
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB33 SWAP E
op_swap_e: ldrb r10,[r3,#sregE] 
         M_SWAP
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB34 SWAP H
op_swap_h: mov r10,r11,LSR #8
         M_SWAP
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB35 SWAP L
op_swap_l: mov r10,r11 
         M_SWAP
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB36 SWAP (HL)
op_swap_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_SWAP
         M_WRB
         b   topZ80
@ 0xCB37 SWAP A
op_swap_a: mov r10,r8 
         M_SWAP
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB38 SRL B
op_srl_b: ldrb r10,[r3,#sregB] 
         M_SRL
         sub r6,r6,#8
         strb r10,[r3,#sregB]
         b   topZ80
@ 0xCB39 SRL C
op_srl_c: ldrb r10,[r3,#sregC] 
         M_SRL
         sub r6,r6,#8
         strb r10,[r3,#sregC]
         b   topZ80
@ 0xCB3A SRL D
op_srl_d: ldrb r10,[r3,#sregD] 
         M_SRL
         sub r6,r6,#8
         strb r10,[r3,#sregD]
         b   topZ80
@ 0xCB3B SRL E
op_srl_e: ldrb r10,[r3,#sregE] 
         M_SRL
         sub r6,r6,#8
         strb r10,[r3,#sregE]
         b   topZ80
@ 0xCB3C SRL H
op_srl_h: mov r10,r11,LSR #8
         M_SRL
         and r11,r11,#0xff
         sub r6,r6,#8
         orr r11,r11,r10,LSL #8
         b   topZ80
@ 0xCB3D SRL L
op_srl_l: mov r10,r11 
         M_SRL
         and r10,r10,#0xff
         and r11,r11,#0xff00
         sub r6,r6,#8
         orr r11,r11,r10
         b   topZ80
@ 0xCB3E SRL (HL)
op_srl_hl: mov r9,r11 
         M_RDB
         sub r6,r6,#16
         M_SRL
         M_WRB
         b   topZ80
@ 0xCB3F SRL A
op_srl_a: mov r10,r8 
         M_SRL
         sub r6,r6,#8
         mov r8,r10
         b   topZ80
@ 0xCB40 BIT 0,B
op_bit_0_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB41 BIT 0,C
op_bit_0_c: ldrb r10,[r3,#sregC] 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB42 BIT 0,D
op_bit_0_d: ldrb r10,[r3,#sregD] 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB43 BIT 0,E
op_bit_0_e: ldrb r10,[r3,#sregE] 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB44 BIT 0,H
op_bit_0_h: mov r10,r11,LSR #8
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB45 BIT 0,L
op_bit_0_l: mov r10,r11 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB46 BIT 0,(HL)
op_bit_0_hl: mov r9,r11 
            sub r6,r6,#12
            M_RDB
            M_BIT 0
            b   topZ80
@ 0xCB47 BIT 0,A
op_bit_0_a: mov r10,r8 
            sub r6,r6,#8
            M_BIT 0
            b   topZ80
@ 0xCB48 BIT 1,B
op_bit_1_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB49 BIT 1,C
op_bit_1_c: ldrb r10,[r3,#sregC] 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB4A BIT 1,D
op_bit_1_d: ldrb r10,[r3,#sregD] 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB4B BIT 1,E
op_bit_1_e: ldrb r10,[r3,#sregE] 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB4C BIT 1,H
op_bit_1_h: mov r10,r11,LSR #8
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB4D BIT 1,L
op_bit_1_l: mov r10,r11 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB4E BIT 1,(HL)
op_bit_1_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_BIT 1
            b   topZ80
@ 0xCB4F BIT 1,A
op_bit_1_a: mov r10,r8 
            sub r6,r6,#8
            M_BIT 1
            b   topZ80
@ 0xCB50 BIT 2,B
op_bit_2_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB51 BIT 2,C
op_bit_2_c: ldrb r10,[r3,#sregC] 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB52 BIT 2,D
op_bit_2_d: ldrb r10,[r3,#sregD] 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB53 BIT 2,E
op_bit_2_e: ldrb r10,[r3,#sregE] 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB54 BIT 2,H
op_bit_2_h: mov r10,r11,LSR #8
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB55 BIT 2,L
op_bit_2_l: mov r10,r11 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB56 BIT 2,(HL)
op_bit_2_hl: mov r9,r11 
            sub r6,r6,#12
            M_RDB
            M_BIT 2
            b   topZ80
@ 0xCB57 BIT 2,A
op_bit_2_a: mov r10,r8 
            sub r6,r6,#8
            M_BIT 2
            b   topZ80
@ 0xCB58 BIT 3,B
op_bit_3_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB59 BIT 3,C
op_bit_3_c: ldrb r10,[r3,#sregC] 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB5A BIT 3,D
op_bit_3_d: ldrb r10,[r3,#sregD] 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB5B BIT 3,E
op_bit_3_e: ldrb r10,[r3,#sregE] 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB5C BIT 3,H
op_bit_3_h: mov r10,r11,LSR #8
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB5D BIT 3,L
op_bit_3_l: mov r10,r11 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB5E BIT 3,(HL)
op_bit_3_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_BIT 3
            b   topZ80
@ 0xCB5F BIT 3,A
op_bit_3_a: mov r10,r8 
            sub r6,r6,#8
            M_BIT 3
            b   topZ80
@ 0xCB60 BIT 4,B
op_bit_4_b: ldrb r10,[r3,#sregB] 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB61 BIT 4,C
op_bit_4_c: ldrb r10,[r3,#sregC] 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB62 BIT 4,D
op_bit_4_d: ldrb r10,[r3,#sregD] 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB63 BIT 4,E
op_bit_4_e: ldrb r10,[r3,#sregE] 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB64 BIT 4,H
op_bit_4_h: mov r10,r11,LSR #8
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB65 BIT 4,L
op_bit_4_l: mov r10,r11 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB66 BIT 4,(HL)
op_bit_4_hl: mov r9,r11 
            sub r6,r6,#12
            M_RDB
            M_BIT 4
            b   topZ80
@ 0xCB67 BIT 4,A
op_bit_4_a: mov r10,r8 
            sub r6,r6,#8
            M_BIT 4
            b   topZ80
@ 0xCB68 BIT 5,B
op_bit_5_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB69 BIT 5,C
op_bit_5_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB6A BIT 5,D
op_bit_5_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB6B BIT 5,E
op_bit_5_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB6C BIT 5,H
op_bit_5_h:   
            M_LOAD_H
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB6D BIT 5,L
op_bit_5_l:   
            M_LOAD_L
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB6E BIT 5,(HL)
op_bit_5_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_BIT 5
            b   topZ80
@ 0xCB6F BIT 5,A
op_bit_5_a:   
            M_LOAD_A
            sub r6,r6,#8
            M_BIT 5
            b   topZ80
@ 0xCB70 BIT 6,B
op_bit_6_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB71 BIT 6,C
op_bit_6_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB72 BIT 6,D
op_bit_6_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB73 BIT 6,E
op_bit_6_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB74 BIT 6,H
op_bit_6_h:   
            M_LOAD_H
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB75 BIT 6,L
op_bit_6_l:   
            M_LOAD_L
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB76 BIT 6,(HL)
op_bit_6_hl: mov r9,r11 
            sub r6,r6,#12
            M_RDB
            M_BIT 6
            b   topZ80
@ 0xCB77 BIT 6,A
op_bit_6_a:   
            M_LOAD_A
            sub r6,r6,#8
            M_BIT 6
            b   topZ80
@ 0xCB78 BIT 7,B
op_bit_7_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB79 BIT 7,C
op_bit_7_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB7A BIT 7,D
op_bit_7_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB7B BIT 7,E
op_bit_7_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB7C BIT 7,H
op_bit_7_h:   
            M_LOAD_H
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB7D BIT 7,L
op_bit_7_l:   
            M_LOAD_L
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB7E BIT 7,(HL)
op_bit_7_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_BIT 7
            b   topZ80
@ 0xCB7F BIT 7,A
op_bit_7_a:   
            M_LOAD_A
            sub r6,r6,#8
            M_BIT 7
            b   topZ80
@ 0xCB80 RES 0,B
op_res_0_b:   
          M_LOAD_B
          sub r6,r6,#8
          M_RES 0
          M_STORE_B
           b   topZ80
@ 0xCB81 RES 0,C
op_res_0_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 0
            M_STORE_C
           b   topZ80
@ 0xCB82 RES 0,D
op_res_0_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 0
            M_STORE_D
           b   topZ80
@ 0xCB83 RES 0,E
op_res_0_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 0
            M_STORE_E
           b   topZ80
@ 0xCB84 RES 0,H
op_res_0_h: bic r11,r11,#0x100 
            sub r6,r6,#8
           b   topZ80
@ 0xCB85 RES 0,L
op_res_0_l: bic r11,r11,#0x1 
            sub r6,r6,#8
           b   topZ80
@ 0xCB86 RES 0,(HL)
op_res_0_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 0
            M_WRB
            b   topZ80
@ 0xCB87 RES 0,A
op_res_0_a: and r8,r8,#0xfe 
            sub r6,r6,#8
           b   topZ80
@ 0xCB88 RES 1,B
op_res_1_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 1
            M_STORE_B
           b   topZ80
@ 0xCB89 RES 1,C
op_res_1_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 1
            M_STORE_C
           b   topZ80
@ 0xCB8A RES 1,D
op_res_1_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 1
            M_STORE_D
           b   topZ80
@ 0xCB8B RES 1,E
op_res_1_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 1
            M_STORE_E
           b   topZ80
@ 0xCB8C RES 1,H
op_res_1_h: bic r11,r11,#0x200 
            sub r6,r6,#8
           b   topZ80
@ 0xCB8D RES 1,L
op_res_1_l: bic r11,r11,#0x2 
            sub r6,r6,#8
           b   topZ80
@ 0xCB8E RES 1,(HL)
op_res_1_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 1
            M_WRB
            b   topZ80
@ 0xCB8F RES 1,A
op_res_1_a: and r8,r8,#0xfd 
            sub r6,r6,#8
           b   topZ80
@ 0xCB90 RES 2,B
op_res_2_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 2
            M_STORE_B
           b   topZ80
@ 0xCB91 RES 2,C
op_res_2_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 2
            M_STORE_C
           b   topZ80
@ 0xCB92 RES 2,D
op_res_2_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 2
            M_STORE_D
           b   topZ80
@ 0xCB93 RES 2,E
op_res_2_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 2
            M_STORE_E
           b   topZ80
@ 0xCB94 RES 2,H
op_res_2_h: bic r11,r11,#0x400 
            sub r6,r6,#8
           b   topZ80
@ 0xCB95 RES 2,L
op_res_2_l: bic r11,r11,#0x4 
            sub r6,r6,#8
           b   topZ80
@ 0xCB96 RES 2,(HL)
op_res_2_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 2
            M_WRB
            b   topZ80
@ 0xCB97 RES 2,A
op_res_2_a: and r8,r8,#0xfb 
            sub r6,r6,#8
           b   topZ80
@ 0xCB98 RES 3,B
op_res_3_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 3
            M_STORE_B
           b   topZ80
@ 0xCB99 RES 3,C
op_res_3_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 3
            M_STORE_C
           b   topZ80
@ 0xCB9A RES 3,D
op_res_3_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 3
            M_STORE_D
           b   topZ80
@ 0xCB9B RES 3,E
op_res_3_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 3
            M_STORE_E
           b   topZ80
@ 0xCB9C RES 3,H
op_res_3_h: bic r11,r11,#0x800 
            sub r6,r6,#8
           b   topZ80
@ 0xCB9D RES 3,L
op_res_3_l: bic r11,r11,#0x8 
            sub r6,r6,#8
           b   topZ80
@ 0xCB9E RES 3,(HL)
op_res_3_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 3
            M_WRB
            b   topZ80
@ 0xCB9F RES 3,A
op_res_3_a: and r8,r8,#0xf7 
            sub r6,r6,#8
           b   topZ80
@ 0xCBA0 RES 4,B
op_res_4_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 4
            M_STORE_B
           b   topZ80
@ 0xCBA1 RES 4,C
op_res_4_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 4
            M_STORE_C
           b   topZ80
@ 0xCBA2 RES 4,D
op_res_4_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 4
            M_STORE_D
           b   topZ80
@ 0xCBA3 RES 4,E
op_res_4_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 4
            M_STORE_E
           b   topZ80
@ 0xCBA4 RES 4,H
op_res_4_h: bic r11,r11,#0x1000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBA5 RES 4,L
op_res_4_l: bic r11,r11,#0x10 
            sub r6,r6,#8
           b   topZ80
@ 0xCBA6 RES 4,(HL)
op_res_4_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 4
            M_WRB
            b   topZ80
@ 0xCBA7 RES 4,A
op_res_4_a: and r8,r8,#0xef 
            sub r6,r6,#8
           b   topZ80
@ 0xCBA8 RES 5,B
op_res_5_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 5
            M_STORE_B
           b   topZ80
@ 0xCBA9 RES 5,C
op_res_5_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 5
            M_STORE_C
           b   topZ80
@ 0xCBAA RES 5,D
op_res_5_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 5
            M_STORE_D
           b   topZ80
@ 0xCBAB RES 5,E
op_res_5_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 5
            M_STORE_E
           b   topZ80
@ 0xCBAC RES 5,H
op_res_5_h: bic r11,r11,#0x2000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBAD RES 5,L
op_res_5_l: bic r11,r11,#0x20 
            sub r6,r6,#8
           b   topZ80
@ 0xCBAE RES 5,(HL)
op_res_5_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 5
            M_WRB
            b   topZ80
@ 0xCBAF RES 5,A
op_res_5_a: and r8,r8,#0xdf 
            sub r6,r6,#8
           b   topZ80
@ 0xCBB0 RES 6,B
op_res_6_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 6
            M_STORE_B
           b   topZ80
@ 0xCBB1 RES 6,C
op_res_6_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 6
            M_STORE_C
           b   topZ80
@ 0xCBB2 RES 6,D
op_res_6_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 6
            M_STORE_D
           b   topZ80
@ 0xCBB3 RES 6,E
op_res_6_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 6
            M_STORE_E
           b   topZ80
@ 0xCBB4 RES 6,H
op_res_6_h: bic r11,r11,#0x4000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBB5 RES 6,L
op_res_6_l: bic r11,r11,#0x40 
            sub r6,r6,#8
           b   topZ80
@ 0xCBB6 RES 6,(HL)
op_res_6_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 6
            M_WRB
            b   topZ80
@ 0xCBB7 RES 6,A
op_res_6_a: and r8,r8,#0xbf 
            sub r6,r6,#8
           b   topZ80
@ 0xCBB8 RES 7,B
op_res_7_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_RES 7
            M_STORE_B
           b   topZ80
@ 0xCBB9 RES 7,C
op_res_7_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_RES 7
            M_STORE_C
           b   topZ80
@ 0xCBBA RES 7,D
op_res_7_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_RES 7
            M_STORE_D
           b   topZ80
@ 0xCBBB RES 7,E
op_res_7_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_RES 7
            M_STORE_E
           b   topZ80
@ 0xCBBC RES 7,H
op_res_7_h: bic r11,r11,#0x8000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBBD RES 7,L
op_res_7_l: bic r11,r11,#0x80 
            sub r6,r6,#8
           b   topZ80
@ 0xCBBE RES 7,(HL)
op_res_7_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_RES 7
            M_WRB
            b   topZ80
@ 0xCBBF RES 7,A
op_res_7_a: and r8,r8,#0x7f 
            sub r6,r6,#8
           b   topZ80
@ 0xCBC0 SET 0,B
op_set_0_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 0
            M_STORE_B
           b   topZ80
@ 0xCBC1 SET 0,C
op_set_0_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 0
            M_STORE_C
           b   topZ80
@ 0xCBC2 SET 0,D
op_set_0_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 0
            M_STORE_D
           b   topZ80
@ 0xCBC3 SET 0,E
op_set_0_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 0
            M_STORE_E
           b   topZ80
@ 0xCBC4 SET 0,H
op_set_0_h: orr r11,r11,#0x100 
            sub r6,r6,#8
           b   topZ80
@ 0xCBC5 SET 0,L
op_set_0_l: orr r11,r11,#1 
            sub r6,r6,#8
           b   topZ80
@ 0xCBC6 SET 0,(HL)
op_set_0_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 0
            M_WRB
            b   topZ80
@ 0xCBC7 SET 0,A
op_set_0_a: orr r8,r8,#1 
            sub r6,r6,#8
           b   topZ80
@ 0xCBC8 SET 1,B
op_set_1_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 1
            M_STORE_B
           b   topZ80
@ 0xCBC9 SET 1,C
op_set_1_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 1
            M_STORE_C
           b   topZ80
@ 0xCBCA SET 1,D
op_set_1_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 1
            M_STORE_D
           b   topZ80
@ 0xCBCB SET 1,E
op_set_1_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 1
            M_STORE_E
           b   topZ80
@ 0xCBCC SET 1,H
op_set_1_h: orr r11,r11,#0x200 
            sub r6,r6,#8
           b   topZ80
@ 0xCBCD SET 1,L
op_set_1_l: orr r11,r11,#2 
            sub r6,r6,#8
           b   topZ80
@ 0xCBCE SET 1,(HL)
op_set_1_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 1
            M_WRB
            b   topZ80
@ 0xCBCF SET 1,A
op_set_1_a: orr r8,r8,#2 
            sub r6,r6,#8
           b   topZ80
@ 0xCBD0 SET 2,B
op_set_2_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 2
            M_STORE_B
           b   topZ80
@ 0xCBD1 SET 2,C
op_set_2_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 2
            M_STORE_C
           b   topZ80
@ 0xCBD2 SET 2,D
op_set_2_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 2
            M_STORE_D
           b   topZ80
@ 0xCBD3 SET 2,E
op_set_2_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 2
            M_STORE_E
           b   topZ80
@ 0xCBD4 SET 2,H
op_set_2_h: orr r11,r11,#0x400 
            sub r6,r6,#8
           b   topZ80
@ 0xCBD5 SET 2,L
op_set_2_l: orr r11,r11,#0x4 
            sub r6,r6,#8
           b   topZ80
@ 0xCBD6 SET 2,(HL)
op_set_2_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 2
            M_WRB
            b   topZ80
@ 0xCBD7 SET 2,A
op_set_2_a: orr r8,r8,#4 
            sub r6,r6,#8
           b   topZ80
@ 0xCBD8 SET 3,B
op_set_3_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 3
            M_STORE_B
           b   topZ80
@ 0xCBD9 SET 3,C
op_set_3_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 3
            M_STORE_C
           b   topZ80
@ 0xCBDA SET 3,D
op_set_3_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 3
            M_STORE_D
           b   topZ80
@ 0xCBDB SET 3,E
op_set_3_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 3
            M_STORE_E
           b   topZ80
@ 0xCBDC SET 3,H
op_set_3_h: orr r11,r11,#0x800 
            sub r6,r6,#8
           b   topZ80
@ 0xCBDD SET 3,L
op_set_3_l: orr r11,r11,#0x8 
            sub r6,r6,#8
           b   topZ80
@ 0xCBDE SET 3,(HL)
op_set_3_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 3
            M_WRB
            b   topZ80
@ 0xCBDF SET 3,A
op_set_3_a: orr r8,r8,#8 
            sub r6,r6,#8
           b   topZ80
@ 0xCBE0 SET 4,B
op_set_4_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 4
            M_STORE_B
           b   topZ80
@ 0xCBE1 SET 4,C
op_set_4_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 4
            M_STORE_C
           b   topZ80
@ 0xCBE2 SET 4,D
op_set_4_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 4
            M_STORE_D
           b   topZ80
@ 0xCBE3 SET 4,E
op_set_4_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 4
            M_STORE_E
           b   topZ80
@ 0xCBE4 SET 4,H
op_set_4_h: orr r11,r11,#0x1000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBE5 SET 4,L
op_set_4_l: orr r11,r11,#0x10 
            sub r6,r6,#8
           b   topZ80
@ 0xCBE6 SET 4,(HL)
op_set_4_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 4
            M_WRB
            b   topZ80
@ 0xCBE7 SET 4,A
op_set_4_a: orr r8,r8,#0x10 
            sub r6,r6,#8
           b   topZ80
@ 0xCBE8 SET 5,B
op_set_5_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 5
            M_STORE_B
           b   topZ80
@ 0xCBE9 SET 5,C
op_set_5_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 5
            M_STORE_C
           b   topZ80
@ 0xCBEA SET 5,D
op_set_5_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 5
            M_STORE_D
           b   topZ80
@ 0xCBEB SET 5,E
op_set_5_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 5
            M_STORE_E
           b   topZ80
@ 0xCBEC SET 5,H
op_set_5_h: orr r11,r11,#0x2000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBED SET 5,L
op_set_5_l: orr r11,r11,#0x20 
            sub r6,r6,#8
           b   topZ80
@ 0xCBEE SET 5,(HL)
op_set_5_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 5
            M_WRB
            b   topZ80
@ 0xCBEF SET 5,A
op_set_5_a: orr r8,r8,#32 
            sub r6,r6,#8
           b   topZ80
@ 0xCBF0 SET 6,B
op_set_6_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 6
            M_STORE_B
           b   topZ80
@ 0xCBF1 SET 6,C
op_set_6_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 6
            M_STORE_C
           b   topZ80
@ 0xCBF2 SET 6,D
op_set_6_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 6
            M_STORE_D
           b   topZ80
@ 0xCBF3 SET 6,E
op_set_6_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 6
            M_STORE_E
           b   topZ80
@ 0xCBF4 SET 6,H
op_set_6_h: orr r11,r11,#0x4000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBF5 SET 6,L
op_set_6_l: orr r11,r11,#0x40 
            sub r6,r6,#8
           b   topZ80
@ 0xCBF6 SET 6,(HL)
op_set_6_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 6
            M_WRB
            b   topZ80
@ 0xCBF7 SET 6,A
op_set_6_a: orr r8,r8,#64 
            sub r6,r6,#8
           b   topZ80
@ 0xCBF8 SET 7,B
op_set_7_b:   
            M_LOAD_B
            sub r6,r6,#8
            M_SET 7
            M_STORE_B
           b   topZ80
@ 0xCBF9 SET 7,C
op_set_7_c:   
            M_LOAD_C
            sub r6,r6,#8
            M_SET 7
            M_STORE_C
           b   topZ80
@ 0xCBFA SET 7,D
op_set_7_d:   
            M_LOAD_D
            sub r6,r6,#8
            M_SET 7
            M_STORE_D
           b   topZ80
@ 0xCBFB SET 7,E
op_set_7_e:   
            M_LOAD_E
            sub r6,r6,#8
            M_SET 7
            M_STORE_E
           b   topZ80
@ 0xCBFC SET 7,H
op_set_7_h: orr r11,r11,#0x8000 
            sub r6,r6,#8
           b   topZ80
@ 0xCBFD SET 7,L
op_set_7_l: orr r11,r11,#0x80 
            sub r6,r6,#8
           b   topZ80
@ 0xCBFE SET 7,(HL)
op_set_7_hl: mov r9,r11 
            sub r6,r6,#16
            M_RDB
            M_SET 7
            M_WRB
            b   topZ80
@ 0xCBFF SET 7,A
op_set_7_a: orr r8,r8,#128 
            sub r6,r6,#8
           b   topZ80
@        AREA MYDATA, DATA, READWRITE  @ name this block of code
  .endif
  .end    
