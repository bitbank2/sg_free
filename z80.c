/*******************************************************************/
/* Z80 CPU emulator written by Larry Bank                          */
/* Copyright 1998-2017 BitBank Software, Inc.                      */
/*                                                                 */
/* This code was written from scratch using the Z80  data from     */
/* the Zilog databook "Components".                                */
/*                                                                 */
/* Change history:                                                 */
/* 2/8/98 Wrote it - Larry B.                                      */
/* 9/30/04 Fixed interrupt handling to allow interrupt AFTER the   */
/* execution of the instruction FOLLOWING the EI.  This allowed    */
/* The New Zealand Story to work correctly.                        */
/* 11/08/04 disabled EI fix in favor of patching TNZS since it     */
/*    slows things considerably and is only needed on one game.    */
/* 3/9/07 Added Branch_and_exit instructions                       */
/*******************************************************************/
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

#include <stdint.h>
#include "my_windows.h"
#include "smartgear.h"
#include "emu.h"

#define F_CARRY     1
#define F_ADDSUB    2
#define F_OVERFLOW  4
#define F_HALFCARRY 16
#define F_ZERO      64
#define F_SIGN      128

#define MEM_DECRYPT 0x20000
#define FAST_LOOPS   // speed up LDIR and other looping instructions
//#define SPEED_HACKS
//#define BRANCH_AND_EXIT
//#define HISTO
#ifdef HISTO
uint32_t ulHisto[0x500]; // 5 tables, opcodes xx, CBxx, EDxx, DDxx/FDxx, DDCBxx/FDCBxx
#endif // HISTO
/* Some statics */
EMUHANDLERS *mem_handlers_z80;
unsigned char *m_map_z80;
unsigned char *m_map_z80_flags; /* Faster for a RISC processor not to have to add MEM_FLAGS repeatedly */
unsigned long *z80_ulCPUOffs; // offset table for banked memory
int iMyClocks; /* CPU tick counter for speed */
int *iRealClocks; /* Access to external clock variable */
extern BOOL bTrace;
uint32_t ulTraceCount;
#ifdef PORTABLE
int Z80OLDPC;
int Z80OLDD;
#else
extern int Z80OLDPC, Z80OLDD;
#endif
void TRACEZ80(REGSZ80 *regs, int iClocks);

#define SET_V8(a,b,r) regs->ucRegF |= (((a^b^r^(r>>1))&0x80)>>5)
#define SET_V16(a,b,r) regs->ucRegF |= (((a^b^r^(r>>1))&0x8000)>>13)

/* flags for Increment */
unsigned char cZ80INC[257] = {
          F_ZERO + F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 00-0F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
          F_OVERFLOW + F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* 80-8F */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* 90-9F */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* A0-AF */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* B0-BF */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* C0-CF */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* D0-DF */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* E0-EF */
          F_HALFCARRY + F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
          F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* F0-FF */
          F_ZERO + F_HALFCARRY};                                            /* 100==00 */
/* flags for Decrement */
unsigned char cZ80DEC[256] = {
            F_ADDSUB + F_ZERO,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,     /* 00-0F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 10-1F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 20-2F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 30-3F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 40-4F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 50-5F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 60-6F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_OVERFLOW + F_HALFCARRY,          /* 70-7F */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* 80-8F */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* 90-9F */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* A0-AF */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* B0-BF */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* C0-CF */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* D0-DF */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY,        /* E0-EF */
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,
          F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN,F_ADDSUB + F_SIGN + F_HALFCARRY};        /* F0-FF */
/* Sign, Zero and Parity for faster lookups */
unsigned char cZ80SZParity[256] = {0x44,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4, /* 00-0F */
              0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,     /* 10-1F */
              0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,     /* 20-2F */
              4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,     /* 30-3F */
              0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,     /* 40-4F */
              4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,     /* 50-5F */
              4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,     /* 60-6F */
              0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,     /* 70-7F */
              0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,     /* 80-8F */
              0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,     /* 90-9F */
              0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,     /* A0-AF */
              0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,     /* B0-BF */
              0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,     /* C0-CF */
              0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,     /* D0-DF */
              0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84,0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,     /* E0-EF */
              0x84,0x80,0x80,0x84,0x80,0x84,0x84,0x80,0x80,0x84,0x84,0x80,0x84,0x80,0x80,0x84};    /* F0-FF */

/* Instruction t-states by opcode for 1 byte opcodes */
unsigned char cZ80Cycles[256] = {4,10,7,6,4,4,7,4,4,11,7,6,4,4,7,4,      /* 00-0F */
                                 8,10,7,6,4,4,7,4,12,11,7,6,4,4,7,4,     /* 10-1F */
                                 7,10,16,6,4,4,7,4,7,11,16,6,4,4,7,4,    /* 20-2F */
                                 7,10,13,6,11,11,10,4,7,11,13,6,4,4,7,4, /* 30-3F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* 40-4F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* 50-5F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* 60-6F */
                                 7,7,7,7,7,7,4,7,4,4,4,4,4,4,7,4,        /* 70-7F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* 80-8F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* 90-9F */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* A0-AF */
                                 4,4,4,4,4,4,7,4,4,4,4,4,4,4,7,4,        /* B0-BF */
                                 5,10,10,10,10,11,7,11,5,10,10,0,10,17,7,11, /* C0-CF */
                                 5,10,10,11,10,11,7,11,5,4,10,11,10,0,7,11, /* D0-DF */
                                 5,10,10,19,10,11,7,11,5,4,10,4,10,0,7,11,  /* E0-EF */
                                 5,10,10,4,10,11,7,11,5,6,10,4,10,0,7,11};  /* F0-FF */

/* Instruction t-states by opcode for 2 byte opcodes  DDXX & FDXX */
unsigned char cZ80Cycles2[256] = {
                0,0,0,0,0,0,0,0,0,15,0,0,0,0,0,0,        /* 00-0F */
                0,0,0,0,0,0,0,0,0,15,0,0,0,0,0,0,        /* 10-1F */
                0,14,20,10,4,4,7,0,0,15,20,10,4,4,7,0,   /* 20-2F */
                0,0,0,0,23,23,19,0,0,15,0,0,0,0,0,0,     /* 30-3F */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* 40-4F */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* 50-5F */
                4,4,4,4,4,4,19,4,4,4,4,4,4,4,19,4,       /* 60-6F */
                19,19,19,19,19,19,0,19,0,0,0,0,4,4,19,0, /* 70-7F */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* 80-8F */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* 90-9F */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* A0-AF */
                0,0,0,0,4,4,19,0,0,0,0,0,4,4,19,0,       /* B0-BF */
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,         /* C0-CF */
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,         /* D0-DF */
                0,14,0,23,0,15,0,0,0,8,0,0,0,0,0,0,      /* E0-EF */
                0,0,0,0,0,0,0,0,0,10,0,0,0,0,0,0};       /* F0-FF */

/* Instruction t-states by opcode for 3 byte opcodes  DDCBXX & FDCBXX */
unsigned char cZ80Cycles3[256] = {
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* 00-0F */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* 10-1F */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* 20-2F */
                0,0,0,0,0,0,0,0,0,0,0,0,0,0,23,0,      /* 30-3F */
                0,0,0,0,0,0,20,0,0,0,0,0,0,0,20,0,     /* 40-4F */
                0,0,0,0,0,0,20,0,0,0,0,0,0,0,20,0,     /* 50-5F */
                0,0,0,0,0,0,20,0,0,0,0,0,0,0,20,0,     /* 60-6F */
                0,0,0,0,0,0,20,0,0,0,0,0,0,0,20,0,     /* 70-7F */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* 80-8F */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* 90-9F */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* A0-AF */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* B0-BF */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* C0-CF */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* D0-DF */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0,     /* E0-EF */
                0,0,0,0,0,0,23,0,0,0,0,0,0,0,23,0};    /* F0-FF */

/* Sign and zero flags for quicker flag settings */
unsigned char cZ80SZ[256]={
      F_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 00-0F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* 80-8F */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* 90-9F */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* A0-AF */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* B0-BF */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* C0-CF */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* D0-DF */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,          /* E0-EF */
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,
      F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN,F_SIGN};         /* F0-FF */

// This table represents the output of the DAA instruction (A = low byte, and the flags = high byte)
// Index the table with the input value of A as the lower 8 bits, bit 8 = carry, bit 9 = half carry, bit 10 = add_sub
unsigned short usDAATable[0x800] = {
	0x5400,0x1001,0x1002,0x1403,0x1004,0x1405,0x1406,0x1007,0x1008,0x1409,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,0x1016,0x1417,0x1418,0x1019,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,0x1026,0x1427,0x1428,0x1029,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,0x1436,0x1037,0x1038,0x1439,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,0x1046,0x1447,0x1448,0x1049,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,0x1456,0x1057,0x1058,0x1459,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,0x1466,0x1067,0x1068,0x1469,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,0x1076,0x1477,0x1478,0x1079,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,0x9086,0x9487,0x9488,0x9089,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,0x9496,0x9097,0x9098,0x9499,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,0x1566,0x1167,0x1168,0x1569,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,0x1176,0x1577,0x1578,0x1179,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,0x9186,0x9587,0x9588,0x9189,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,0x9596,0x9197,0x9198,0x9599,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,0x95a6,0x91a7,0x91a8,0x95a9,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,0x91b6,0x95b7,0x95b8,0x91b9,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,0x95c6,0x91c7,0x91c8,0x95c9,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,0x91d6,0x95d7,0x95d8,0x91d9,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,0x91e6,0x95e7,0x95e8,0x91e9,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,0x95f6,0x91f7,0x91f8,0x95f9,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,0x1506,0x1107,0x1108,0x1509,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,0x1116,0x1517,0x1518,0x1119,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,0x1126,0x1527,0x1528,0x1129,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,0x1536,0x1137,0x1138,0x1539,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,0x1146,0x1547,0x1548,0x1149,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,0x1556,0x1157,0x1158,0x1559,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1406,0x1007,0x1008,0x1409,0x140a,0x100b,0x140c,0x100d,0x100e,0x140f,0x1010,0x1411,0x1412,0x1013,0x1414,0x1015,
	0x1016,0x1417,0x1418,0x1019,0x101a,0x141b,0x101c,0x141d,0x141e,0x101f,0x1020,0x1421,0x1422,0x1023,0x1424,0x1025,
	0x1026,0x1427,0x1428,0x1029,0x102a,0x142b,0x102c,0x142d,0x142e,0x102f,0x1430,0x1031,0x1032,0x1433,0x1034,0x1435,
	0x1436,0x1037,0x1038,0x1439,0x143a,0x103b,0x143c,0x103d,0x103e,0x143f,0x1040,0x1441,0x1442,0x1043,0x1444,0x1045,
	0x1046,0x1447,0x1448,0x1049,0x104a,0x144b,0x104c,0x144d,0x144e,0x104f,0x1450,0x1051,0x1052,0x1453,0x1054,0x1455,
	0x1456,0x1057,0x1058,0x1459,0x145a,0x105b,0x145c,0x105d,0x105e,0x145f,0x1460,0x1061,0x1062,0x1463,0x1064,0x1465,
	0x1466,0x1067,0x1068,0x1469,0x146a,0x106b,0x146c,0x106d,0x106e,0x146f,0x1070,0x1471,0x1472,0x1073,0x1474,0x1075,
	0x1076,0x1477,0x1478,0x1079,0x107a,0x147b,0x107c,0x147d,0x147e,0x107f,0x9080,0x9481,0x9482,0x9083,0x9484,0x9085,
	0x9086,0x9487,0x9488,0x9089,0x908a,0x948b,0x908c,0x948d,0x948e,0x908f,0x9490,0x9091,0x9092,0x9493,0x9094,0x9495,
	0x9496,0x9097,0x9098,0x9499,0x949a,0x909b,0x949c,0x909d,0x909e,0x949f,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x1566,0x1167,0x1168,0x1569,0x156a,0x116b,0x156c,0x116d,0x116e,0x156f,0x1170,0x1571,0x1572,0x1173,0x1574,0x1175,
	0x1176,0x1577,0x1578,0x1179,0x117a,0x157b,0x117c,0x157d,0x157e,0x117f,0x9180,0x9581,0x9582,0x9183,0x9584,0x9185,
	0x9186,0x9587,0x9588,0x9189,0x918a,0x958b,0x918c,0x958d,0x958e,0x918f,0x9590,0x9191,0x9192,0x9593,0x9194,0x9595,
	0x9596,0x9197,0x9198,0x9599,0x959a,0x919b,0x959c,0x919d,0x919e,0x959f,0x95a0,0x91a1,0x91a2,0x95a3,0x91a4,0x95a5,
	0x95a6,0x91a7,0x91a8,0x95a9,0x95aa,0x91ab,0x95ac,0x91ad,0x91ae,0x95af,0x91b0,0x95b1,0x95b2,0x91b3,0x95b4,0x91b5,
	0x91b6,0x95b7,0x95b8,0x91b9,0x91ba,0x95bb,0x91bc,0x95bd,0x95be,0x91bf,0x95c0,0x91c1,0x91c2,0x95c3,0x91c4,0x95c5,
	0x95c6,0x91c7,0x91c8,0x95c9,0x95ca,0x91cb,0x95cc,0x91cd,0x91ce,0x95cf,0x91d0,0x95d1,0x95d2,0x91d3,0x95d4,0x91d5,
	0x91d6,0x95d7,0x95d8,0x91d9,0x91da,0x95db,0x91dc,0x95dd,0x95de,0x91df,0x91e0,0x95e1,0x95e2,0x91e3,0x95e4,0x91e5,
	0x91e6,0x95e7,0x95e8,0x91e9,0x91ea,0x95eb,0x91ec,0x95ed,0x95ee,0x91ef,0x95f0,0x91f1,0x91f2,0x95f3,0x91f4,0x95f5,
	0x95f6,0x91f7,0x91f8,0x95f9,0x95fa,0x91fb,0x95fc,0x91fd,0x91fe,0x95ff,0x5500,0x1101,0x1102,0x1503,0x1104,0x1505,
	0x1506,0x1107,0x1108,0x1509,0x150a,0x110b,0x150c,0x110d,0x110e,0x150f,0x1110,0x1511,0x1512,0x1113,0x1514,0x1115,
	0x1116,0x1517,0x1518,0x1119,0x111a,0x151b,0x111c,0x151d,0x151e,0x111f,0x1120,0x1521,0x1522,0x1123,0x1524,0x1125,
	0x1126,0x1527,0x1528,0x1129,0x112a,0x152b,0x112c,0x152d,0x152e,0x112f,0x1530,0x1131,0x1132,0x1533,0x1134,0x1535,
	0x1536,0x1137,0x1138,0x1539,0x153a,0x113b,0x153c,0x113d,0x113e,0x153f,0x1140,0x1541,0x1542,0x1143,0x1544,0x1145,
	0x1146,0x1547,0x1548,0x1149,0x114a,0x154b,0x114c,0x154d,0x154e,0x114f,0x1550,0x1151,0x1152,0x1553,0x1154,0x1555,
	0x1556,0x1157,0x1158,0x1559,0x155a,0x115b,0x155c,0x115d,0x115e,0x155f,0x1560,0x1161,0x1162,0x1563,0x1164,0x1565,
	0x5600,0x1201,0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x1210,0x1611,0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x1220,0x1621,0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x1630,0x1231,0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x1240,0x1641,0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x1650,0x1251,0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x1660,0x1261,0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x1270,0x1671,0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x9280,0x9681,0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x9690,0x9291,0x9292,0x9693,0x9294,0x9695,0x9696,0x9297,0x9298,0x9699,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97a0,0x93a1,0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x93b0,0x97b1,0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x97c0,0x93c1,0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x93d0,0x97d1,0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93e0,0x97e1,0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x97f0,0x93f1,0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x5700,0x1301,0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x1310,0x1711,0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x1320,0x1721,0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x1730,0x1331,0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5600,0x1201,0x1202,0x1603,0x1204,0x1605,0x1606,0x1207,0x1208,0x1609,
	0x160a,0x120b,0x160c,0x120d,0x120e,0x160f,0x1210,0x1611,0x1612,0x1213,0x1614,0x1215,0x1216,0x1617,0x1618,0x1219,
	0x121a,0x161b,0x121c,0x161d,0x161e,0x121f,0x1220,0x1621,0x1622,0x1223,0x1624,0x1225,0x1226,0x1627,0x1628,0x1229,
	0x122a,0x162b,0x122c,0x162d,0x162e,0x122f,0x1630,0x1231,0x1232,0x1633,0x1234,0x1635,0x1636,0x1237,0x1238,0x1639,
	0x163a,0x123b,0x163c,0x123d,0x123e,0x163f,0x1240,0x1641,0x1642,0x1243,0x1644,0x1245,0x1246,0x1647,0x1648,0x1249,
	0x124a,0x164b,0x124c,0x164d,0x164e,0x124f,0x1650,0x1251,0x1252,0x1653,0x1254,0x1655,0x1656,0x1257,0x1258,0x1659,
	0x165a,0x125b,0x165c,0x125d,0x125e,0x165f,0x1660,0x1261,0x1262,0x1663,0x1264,0x1665,0x1666,0x1267,0x1268,0x1669,
	0x166a,0x126b,0x166c,0x126d,0x126e,0x166f,0x1270,0x1671,0x1672,0x1273,0x1674,0x1275,0x1276,0x1677,0x1678,0x1279,
	0x127a,0x167b,0x127c,0x167d,0x167e,0x127f,0x9280,0x9681,0x9682,0x9283,0x9684,0x9285,0x9286,0x9687,0x9688,0x9289,
	0x928a,0x968b,0x928c,0x968d,0x968e,0x928f,0x9690,0x9291,0x9292,0x9693,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799,
	0x979a,0x939b,0x979c,0x939d,0x939e,0x979f,0x97a0,0x93a1,0x93a2,0x97a3,0x93a4,0x97a5,0x97a6,0x93a7,0x93a8,0x97a9,
	0x97aa,0x93ab,0x97ac,0x93ad,0x93ae,0x97af,0x93b0,0x97b1,0x97b2,0x93b3,0x97b4,0x93b5,0x93b6,0x97b7,0x97b8,0x93b9,
	0x93ba,0x97bb,0x93bc,0x97bd,0x97be,0x93bf,0x97c0,0x93c1,0x93c2,0x97c3,0x93c4,0x97c5,0x97c6,0x93c7,0x93c8,0x97c9,
	0x97ca,0x93cb,0x97cc,0x93cd,0x93ce,0x97cf,0x93d0,0x97d1,0x97d2,0x93d3,0x97d4,0x93d5,0x93d6,0x97d7,0x97d8,0x93d9,
	0x93da,0x97db,0x93dc,0x97dd,0x97de,0x93df,0x93e0,0x97e1,0x97e2,0x93e3,0x97e4,0x93e5,0x93e6,0x97e7,0x97e8,0x93e9,
	0x93ea,0x97eb,0x93ec,0x97ed,0x97ee,0x93ef,0x97f0,0x93f1,0x93f2,0x97f3,0x93f4,0x97f5,0x97f6,0x93f7,0x93f8,0x97f9,
	0x97fa,0x93fb,0x97fc,0x93fd,0x93fe,0x97ff,0x5700,0x1301,0x1302,0x1703,0x1304,0x1705,0x1706,0x1307,0x1308,0x1709,
	0x170a,0x130b,0x170c,0x130d,0x130e,0x170f,0x1310,0x1711,0x1712,0x1313,0x1714,0x1315,0x1316,0x1717,0x1718,0x1319,
	0x131a,0x171b,0x131c,0x171d,0x171e,0x131f,0x1320,0x1721,0x1722,0x1323,0x1724,0x1325,0x1326,0x1727,0x1728,0x1329,
	0x132a,0x172b,0x132c,0x172d,0x172e,0x132f,0x1730,0x1331,0x1332,0x1733,0x1334,0x1735,0x1736,0x1337,0x1338,0x1739,
	0x173a,0x133b,0x173c,0x133d,0x133e,0x173f,0x1340,0x1741,0x1742,0x1343,0x1744,0x1345,0x1346,0x1747,0x1748,0x1349,
	0x134a,0x174b,0x134c,0x174d,0x174e,0x134f,0x1750,0x1351,0x1352,0x1753,0x1354,0x1755,0x1756,0x1357,0x1358,0x1759,
	0x175a,0x135b,0x175c,0x135d,0x135e,0x175f,0x1760,0x1361,0x1362,0x1763,0x1364,0x1765,0x1766,0x1367,0x1368,0x1769,
	0x176a,0x136b,0x176c,0x136d,0x136e,0x176f,0x1370,0x1771,0x1772,0x1373,0x1774,0x1375,0x1376,0x1777,0x1778,0x1379,
	0x137a,0x177b,0x137c,0x177d,0x177e,0x137f,0x9380,0x9781,0x9782,0x9383,0x9784,0x9385,0x9386,0x9787,0x9788,0x9389,
	0x938a,0x978b,0x938c,0x978d,0x978e,0x938f,0x9790,0x9391,0x9392,0x9793,0x9394,0x9795,0x9796,0x9397,0x9398,0x9799};

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ADD(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Add an 8-bit value to A.                                   *
 *                                                                          *
 ****************************************************************************/
void Z80ADD(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA + ucByte;
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   if (us & 0x100)
      regs->ucRegF |= F_CARRY;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);
   SET_V8(regs->ucRegA, ucByte, us);
   regs->ucRegA = (unsigned char)us;

} /* Z80ADD() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ADC(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Add an 8-bit value to A with carry.                        *
 *                                                                          *
 ****************************************************************************/
void Z80ADC(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA + ucByte + (regs->ucRegF & F_CARRY);
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   if (us & 0x100)
      regs->ucRegF |= F_CARRY;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);
   SET_V8(regs->ucRegA, ucByte, us);
   regs->ucRegA = (unsigned char)us;

} /* Z80ADC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SUB(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Sub an 8-bit value to A.                                   *
 *                                                                          *
 ****************************************************************************/
void Z80SUB(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA - ucByte;
   regs->ucRegF &= ~(F_CARRY | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   regs->ucRegF |= F_ADDSUB;
   if (us & 0x100)
      regs->ucRegF |= F_CARRY;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);
   SET_V8(regs->ucRegA, ucByte, us);
   regs->ucRegA = (unsigned char)us;

} /* Z80SUB() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SBC(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Sub an 8-bit value to A with carry.                        *
 *                                                                          *
 ****************************************************************************/
void Z80SBC(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA - ucByte - (regs->ucRegF & F_CARRY);
   regs->ucRegF &= ~(F_CARRY | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   regs->ucRegF |= F_ADDSUB;
   if (us & 0x100)
      regs->ucRegF |= F_CARRY;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);
   SET_V8(regs->ucRegA, ucByte, us);
   regs->ucRegA = (unsigned char)us;

} /* Z80SBC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80AND(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Logical AND an 8-bit value and update the flags.           *
 *                                                                          *
 ****************************************************************************/
void Z80AND(REGSZ80 *regs, unsigned char ucByte)
{

   regs->ucRegA &= ucByte;
   regs->ucRegF = F_HALFCARRY; // all flags get blasted
   regs->ucRegF |= cZ80SZParity[regs->ucRegA];

} /* Z80AND() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80XOR(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Logical XOR an 8-bit value and update the flags.           *
 *                                                                          *
 ****************************************************************************/
void Z80XOR(REGSZ80 *regs, unsigned char ucByte)
{
   regs->ucRegA ^= ucByte;
//   regs->ucRegF &= ~(F_SIGN | F_ZERO | F_OVERFLOW | F_CARRY |F_ADDSUB | F_HALFCARRY);
//   regs->ucRegF |= cZ80SZParity[regs->ucRegA];
   regs->ucRegF = cZ80SZParity[regs->ucRegA];
} /* Z80XOR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80OR(REGSZ80 *, unsigned char)                            *
 *                                                                          *
 *  PURPOSE    : Logical OR an 8-bit value and update the flags.            *
 *                                                                          *
 ****************************************************************************/
void Z80OR(REGSZ80 *regs, unsigned char ucByte)
{
   regs->ucRegA |= ucByte;
//   regs->ucRegF &= ~(F_SIGN | F_ZERO | F_OVERFLOW | F_CARRY |F_ADDSUB | F_HALFCARRY);
//   regs->ucRegF |= cZ80SZParity[regs->ucRegA];
   regs->ucRegF = cZ80SZParity[regs->ucRegA];  // all flags get blasted
} /* Z80OR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80CMP(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Compare and 8-bit value to A.                              *
 *                                                                          *
 ****************************************************************************/
void Z80CMP(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA - ucByte;
   regs->ucRegF &= ~(F_CARRY | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   regs->ucRegF |= F_ADDSUB;
   if (us & 0x100)
      regs->ucRegF |= F_CARRY;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);
   SET_V8(regs->ucRegA, ucByte, us);

} /* Z80CMP() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80CMP2(REGSZ80 *, unsigned char)                          *
 *                                                                          *
 *  PURPOSE    : Compare and 8-bit value to A.  This version is for the     *
 *               block instructions (CPI, CPIR, CPD, CPDR) and does not     *
 *               affect the carry flag.                                     *
 *                                                                          *
 ****************************************************************************/
void Z80CMP2(REGSZ80 *regs, unsigned char ucByte)
{
unsigned short us;

   us = regs->ucRegA - ucByte;
   regs->ucRegF &= ~(F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   regs->ucRegF |= F_ADDSUB;
   regs->ucRegF |= cZ80SZ[us & 0xff];
   regs->ucRegF |= ((regs->ucRegA ^ ucByte ^ us) & F_HALFCARRY);

} /* Z80CMP2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80INC(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Increment an 8-bit value and update the flags.             *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80INC(REGSZ80 *regs, uint32_t ucByte)
{
   ucByte++;
//   regs->ucRegF &= F_CARRY; /* Only carry is preserved */
//   regs->ucRegF |= cZ80INC[ucByte]; /* Set all flags */
   regs->ucRegF = (regs->ucRegF & F_CARRY) | cZ80INC[ucByte];
   return (unsigned char)ucByte;
} /* Z80INC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80DEC(REGSZ80 *, unsigned char)                           *
 *                                                                          *
 *  PURPOSE    : Decrement an 8-bit value and update the flags.             *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80DEC(REGSZ80 *regs, unsigned char ucByte)
{

   ucByte--;
//   regs->ucRegF &= F_CARRY; /* Only carry is preserved */
//   regs->ucRegF |= cZ80DEC[ucByte]; /* Set all flags */
   regs->ucRegF = (regs->ucRegF & F_CARRY) | cZ80DEC[ucByte];
   return ucByte;

} /* Z80DEC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ADDHL(REGSZ80 *, unsigned word, unsigned word)          *
 *                                                                          *
 *  PURPOSE    : Add two 16-bit values and update the flags.                *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80ADDHL(REGSZ80 *regs, unsigned short usWord1, unsigned short usWord2)
{
uint32_t ul = usWord1 + usWord2;

   regs->ucRegF &= (F_SIGN | F_ZERO | F_OVERFLOW);
   if (ul & 0x10000)
      regs->ucRegF |= F_CARRY;
   if ((usWord1 ^ usWord2 ^ ul) & 0x1000)
      regs->ucRegF |= F_HALFCARRY;

   return (unsigned short)ul;

} /* Z80ADDHL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ADD16(REGSZ80 *, unsigned word, unsigned word)          *
 *                                                                          *
 *  PURPOSE    : Add two 16-bit values and update the flags.                *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80ADD16(REGSZ80 *regs, unsigned short usWord1, unsigned short usWord2)
{
uint32_t ul = usWord1 + usWord2;

   regs->ucRegF &= F_SIGN | F_ZERO | F_OVERFLOW; // preserve only these 3
   if (ul & 0x10000)
      regs->ucRegF |= F_CARRY;
   if ((usWord1 ^ usWord2 ^ ul) & 0x1000)
      regs->ucRegF |= F_HALFCARRY;
   return (unsigned short)ul;

} /* Z80ADD16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ADC16(REGSZ80 *, unsigned word, unsigned word)          *
 *                                                                          *
 *  PURPOSE    : Add two 16-bit values and update the flags.                *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80ADC16(REGSZ80 *regs, unsigned short usWord1, unsigned short usWord2)
{
uint32_t ul;

   ul = usWord1 + usWord2 + (regs->ucRegF & F_CARRY);
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   if (ul & 0x10000)
      regs->ucRegF |= F_CARRY;
   if (ul & 0x8000)
      regs->ucRegF |= F_SIGN;
   if (ul == 0 || ul == 0x10000)
      regs->ucRegF |= F_ZERO;
   SET_V16(usWord1, usWord2, ul);
   if ((usWord1 ^ usWord2 ^ ul) & 0x1000)
      regs->ucRegF |= F_HALFCARRY;

   return (unsigned short)ul;

} /* Z80ADC16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SBC16(REGSZ80 *, unsigned word, unsigned word)          *
 *                                                                          *
 *  PURPOSE    : Sub two 16-bit values and update the flags.                *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80SBC16(REGSZ80 *regs, unsigned short usWord1, unsigned short usWord2)
{
uint32_t ul;

   ul = usWord1 - usWord2 - (regs->ucRegF & F_CARRY);
   regs->ucRegF &= ~(F_CARRY | F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY);
   regs->ucRegF |= F_ADDSUB;
   if (ul & 0x10000)
      regs->ucRegF |= F_CARRY;
   if (ul & 0x8000)
      regs->ucRegF |= F_SIGN;
   if (ul == 0 || ul == 0x10000)
      regs->ucRegF |= F_ZERO;
   SET_V16(usWord1, usWord2, ul);
   if ((usWord1 ^ usWord2 ^ ul) & 0x1000)
      regs->ucRegF |= F_HALFCARRY;

   return (unsigned short)ul;

} /* Z80SBC16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RL(REGSZ80 *, unsigned word, unsigned word)             *
 *                                                                          *
 *  PURPOSE    : Rotate left through carry.                                 *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80RL(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = regs->ucRegF & F_CARRY; /* Preserve old carry flag */
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   if (ucByte & 0x80)
      regs->ucRegF |= F_CARRY;
   ucByte = (ucByte <<1) | uc;
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80RL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SLA(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Shift left arithmetic.                                     *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80SLA(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = ucByte & 0x80;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   if (uc)
      regs->ucRegF |= F_CARRY;
   ucByte = ucByte <<1;
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80SLA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SLL(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Shift left logical.                                        *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80SLL(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = ucByte & 0x80;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   if (uc)
      regs->ucRegF |= F_CARRY;
   ucByte = (ucByte << 1) + 1;
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80SLL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SRA(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Shift right arithmetic.                                    *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80SRA(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;
unsigned char ucOld = ucByte;

   uc = ucByte & 1;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   regs->ucRegF |= uc;
   ucByte = (ucByte >>1) | (ucOld & 0x80);
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80SRA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80SRL(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Shift right logical.                                       *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80SRL(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = ucByte & 1;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   regs->ucRegF |= uc;
   ucByte = ucByte >>1;
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80SRL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RR(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Rotate right through carry.                                *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80RR(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = regs->ucRegF & F_CARRY; /* Preserve old carry flag */
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   regs->ucRegF |= (ucByte & F_CARRY);
   ucByte = (ucByte >>1) | (uc << 7);
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80RR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RRA(REGSZ80 *)                                          *
 *                                                                          *
 *  PURPOSE    : Rotate right A through carry.                              *
 *                                                                          *
 ****************************************************************************/
void Z80RRA(REGSZ80 *regs)
{
unsigned char uc;

   uc = regs->ucRegF & F_CARRY; /* Preserve old carry flag */
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY);
   regs->ucRegF |= (regs->ucRegA & F_CARRY);
   regs->ucRegA = (regs->ucRegA >>1) | (uc << 7);

} /* Z80RRA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RLA(REGSZ80 *)                                          *
 *                                                                          *
 *  PURPOSE    : Rotate left A through carry.                               *
 *                                                                          *
 ****************************************************************************/
void Z80RLA(REGSZ80 *regs)
{
unsigned char uc;

   uc = regs->ucRegF & F_CARRY; /* Preserve old carry flag */
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY);
   if (regs->ucRegA & 0x80)
      regs->ucRegF |= F_CARRY;
   regs->ucRegA = regs->ucRegA <<1 | uc;

} /* Z80RLA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RLC(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Rotate left.                                               *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80RLC(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = ucByte & 0x80;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   if (uc)
      regs->ucRegF |= F_CARRY;
   ucByte = (ucByte <<1) | (uc >> 7);
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80RLC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RLCA(REGSZ80 *)                                         *
 *                                                                          *
 *  PURPOSE    : Rotate left A.                                             *
 *                                                                          *
 ****************************************************************************/
void Z80RLCA(REGSZ80 *regs)
{
unsigned char uc;

   uc = regs->ucRegA & 0x80;
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY);
   if (uc)
      regs->ucRegF |= F_CARRY;
   regs->ucRegA = (regs->ucRegA <<1) | (uc >> 7);

} /* Z80RLCA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RRC(REGSZ80 *, unsigned word, unsigned word)            *
 *                                                                          *
 *  PURPOSE    : Rotate right.                                              *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80RRC(REGSZ80 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = ucByte & 1;
   regs->ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_OVERFLOW | F_SIGN | F_HALFCARRY);
   regs->ucRegF |= uc;
   ucByte = (ucByte >>1) | (uc << 7);
   regs->ucRegF |= cZ80SZParity[ucByte];
   return ucByte;

} /* Z80RRC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80RRCA(REGSZ80 *)                                         *
 *                                                                          *
 *  PURPOSE    : Rotate A right.                                            *
 *                                                                          *
 ****************************************************************************/
void Z80RRCA(REGSZ80 *regs)
{
unsigned char uc;

   uc = regs->ucRegA & 1;
   regs->ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY);
   regs->ucRegF |= uc;
   regs->ucRegA = (regs->ucRegA >>1) | (uc << 7);

} /* Z80RRCA() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80WriteByte(short, char)                                  *
 *                                                                          *
 *  PURPOSE    : Write a byte to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
void Z80WriteByte(unsigned short usAddr, unsigned char ucByte)
{
unsigned char c;
   if (usAddr == 0x4f59)
      c = 0;
   c = m_map_z80_flags[usAddr];
   if (c & 0x80) /* If special flag (ROM or hardware) */
      {
      if (c == 0xbf) /* ROM, don't do anything */
         return;
      *iRealClocks = iMyClocks; /* In case external hardware needs accurate timing */
      (mem_handlers_z80[(c&0x3f)].pfn_write)(usAddr, ucByte);
      }
   else /* Normal RAM, just write it and leave */
      {
      if (z80_ulCPUOffs)
         *(unsigned char *)((unsigned long)usAddr + z80_ulCPUOffs[usAddr >> 12]) = ucByte;
      else
         m_map_z80[usAddr+MEM_ROMRAM] = ucByte;
      }

} /* Z80WriteByte() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80WriteWord(short, short)                                 *
 *                                                                          *
 *  PURPOSE    : Write a word to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
void Z80WriteWord(unsigned short usAddr, unsigned short usWord)
{
   Z80WriteByte(usAddr, (unsigned char)(usWord & 0xff));
   Z80WriteByte((unsigned short)(usAddr+1),(unsigned char)(usWord >> 8));
} /* Z80WriteWord() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80IN(regs, short)                                         *
 *                                                                          *
 *  PURPOSE    : Read a byte from a port, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80IN(REGSZ80 *regs, unsigned short usAddr, int iMyClocks)
{
unsigned char c;
   usAddr &= regs->iPortMask; // allows for 8 and 16-bit port addressing
   *iRealClocks = iMyClocks; /* In case external hardware needs accurate timing */ // needed for Konami games
   Z80OLDD = regs->ucRegD; // For Black Tiger protection check

   c = (mem_handlers_z80[0].pfn_read)(usAddr);  /* Handler #0 is reserved for port access */
   regs->ucRegF &= F_CARRY; // only carry flag is preserved
   regs->ucRegF |= cZ80SZParity[c];
   return c;

} /* Z80IN(regs, ) */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80IN2(regs, short)                                        *
 *                                                                          *
 *  PURPOSE    : Read a byte from a port, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80IN2(REGSZ80 *regs, unsigned short usAddr, int iMyClocks)
{
unsigned char c;
   usAddr &= regs->iPortMask; // allows for 8 and 16-bit port addressing
   *iRealClocks = iMyClocks; /* In case external hardware needs accurate timing */ // needed for Konami games
   Z80OLDD = regs->ucRegD; // For Black Tiger protection check

   c = (mem_handlers_z80[0].pfn_read)(usAddr);  /* Handler #0 is reserved for port access */
//   regs->ucRegF &= F_CARRY; // only carry flag is preserved
//   regs->ucRegF |= cZ80SZParity[c];
   return c;

} /* Z80IN(regs, ) */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80OUT(short, char)                                        *
 *                                                                          *
 *  PURPOSE    : Write a byte to a port, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
void Z80OUT(REGSZ80 *regs, unsigned short usAddr, unsigned char ucByte)
{
   *iRealClocks = iMyClocks; /* In case external hardware needs accurate timing */
   usAddr &= regs->iPortMask; // allows for 8 and 16-bit port addressing
   (mem_handlers_z80[0].pfn_write)(usAddr, ucByte);  /* Handler #0 is reserved for port access */
} /* Z80OUT() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ReadByte(short)                                         *
 *                                                                          *
 *  PURPOSE    : Read a byte from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
unsigned char Z80ReadByte(unsigned short usAddr)
{
unsigned char c;

   c = m_map_z80_flags[usAddr];  
   if (c & 0x40) /* If special flag (hardware) */
      {
      *iRealClocks = iMyClocks; // update time for Konami games
      return (mem_handlers_z80[(c&0x3f)].pfn_read)(usAddr);
      }
   else
      {
      if (z80_ulCPUOffs)
         return *(unsigned char *)((unsigned long)usAddr + z80_ulCPUOffs[usAddr >> 12]);
      else
         return m_map_z80[usAddr+MEM_ROMRAM]; /* Just return it */
      }


} /* Z80ReadByte() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ReadWord(short)                                         *
 *                                                                          *
 *  PURPOSE    : Read a word from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80ReadWord(unsigned short usAddr)
{
unsigned short usWord;

   usWord = Z80ReadByte(usAddr);
   usWord += Z80ReadByte((unsigned short)(usAddr+1))*256;
   return usWord;

} /* Z80ReadWord() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80PUSHW(REGSZ80 *)                                        *
 *                                                                          *
 *  PURPOSE    : Push a word to the 'SP' stack.                             *
 *                                                                          *
 ****************************************************************************/
void Z80PUSHW(REGSZ80 *regs, unsigned short usWord)
{

   Z80WriteByte(--regs->usRegSP, (unsigned char)(usWord >> 8));
   Z80WriteByte(--regs->usRegSP, (unsigned char)(usWord & 0xff));

} /* Z80PUSHW() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80POPW(REGSZ80 *)                                         *
 *                                                                          *
 *  PURPOSE    : Pull a word from the 'SP' stack.                           *
 *                                                                          *
 ****************************************************************************/
unsigned short Z80POPW(REGSZ80 *regs)
{
unsigned char hi, lo;

   lo = Z80ReadByte(regs->usRegSP++);
   hi = Z80ReadByte(regs->usRegSP++);
   return (hi * 256 + lo);

} /* Z80POPW() */

#ifdef HISTO
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : ProcessHisto(uint32_t *)                              *
 *                                                                          *
 *  PURPOSE    : Find the top 20 instructions.                              *
 *                                                                          *
 ****************************************************************************/
uint32_t Z80ProcessHisto(uint32_t *pResults)
{
int i, j, iTop, iTotal;
   
   // Blast the beginning of the multi-byte opcodes
   ulHisto[0xdd] = ulHisto[0xcb] = ulHisto[0xed] = ulHisto[0xfd] = 0;
   ulHisto[0x3dd] = ulHisto[0x3fd] = 0; // for DDCBxx
   iTotal = 0; // total instructions executed
   for (j=0; j<20; j++) // find top 20
      {
      iTop = 0;
      for (i=0; i<0x500; i++) // scan entire table
         {
         if (j == 0) // on first pass, find total instructions
            iTotal += ulHisto[i];
         if (ulHisto[i] > ulHisto[iTop])
            iTop = i;
         }
      pResults[j] = iTop;
      ulHisto[iTop]= 0; // eliminate max to find next max
      }
   return iTotal;
   
} /* Z80ProcessHisto() */
#endif // HISTO
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : RESETZ80(REGSZ80 *)                                        *
 *                                                                          *
 *  PURPOSE    : Setup the Z80 after a reset.                               *
 *                                                                          *
 ****************************************************************************/
void RESETZ80(REGSZ80 *regs, int iMask)
{
//   memset(regs, 0, sizeof(REGSZ80)); /* Start with a clean slate at reset */
   
   regs->usRegPC = 0; /* Start running from reset vector */
   regs->ucRegF = 0x40;
//   regs->usRegSP = 0xf000; /* Start stack at a reasonable place */
   regs->usRegIX = 0xffff;
   regs->usRegIY = 0xffff;
   regs->ucRegR = 0; // DEBUG - need random number generator rand();
   regs->iPortMask = iMask; // sets for 8 or 16-bit port addressing
   ulTraceCount = 0;

#ifdef HISTO
   memset(&ulHisto[0], 0, 0x500*sizeof(long));
#endif // HISTO

} /* RESETZ80() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : CheckInterrupts()                                          *
 *                                                                          *
 *  PURPOSE    : Try to handle any pending interrupts.                      *
 *                                                                          *
 ****************************************************************************/
unsigned char CheckInterrupts(char *mem, REGSZ80 *regs, uint32_t *PC, char *ucIRQs, unsigned char cIRQValue)
{
unsigned short usAddr;

      if (*ucIRQs)
         {
         if (regs->ucRegHALT) // if currently halted
            {
            regs->ucRegHALT = 0;
            (*PC)++; // increment PC over halt instruction
            }
         if (*ucIRQs & INT_NMI)// && regs->ucRegNMI != INT_NMI)
            {
            *ucIRQs &= ~INT_NMI; /* acknowledge this NMI */
            regs->ucRegNMI = INT_NMI;
            Z80PUSHW(regs, (unsigned short)*PC); /* Push PC and jump to 66H */
            *PC = 0x66;
//            regs->ucRegIFF2 = regs->ucRegIFF1; /* Preserve maskable interrupt flag */
            regs->ucRegIFF1 = 0; /* Disable maskable interrupts */
            iMyClocks -= 11;
            }
         if ((*ucIRQs & INT_IRQ) && regs->ucRegIFF1) /* If there is an interrupt pending */
            {
            *ucIRQs &= ~INT_IRQ;
//            regs->ucRegIFF1 = 0; /* Disable maskable interrupts */
            regs->ucRegIFF1 = regs->ucRegIFF2 = 0; /* Disable maskable interrupts */
            switch (regs->ucRegIM) /* Different interrupt mode handling */
               {
               case 0: /* Mode 0 has instruction placed on data bus */
                  iMyClocks -= 13;
                  return regs->ucIRQVal; /* This is the new opcode */
               case 1: /* Mode 1 acts like an 8080 */
                  iMyClocks -= 13;
                  return 0xff; /* Do a RST 38H */
               case 2: /* Mode 2 has a vector put on the data bus */
                  iMyClocks -= 19;
                  usAddr = regs->ucRegI * 256 + regs->ucIRQVal;
                  Z80PUSHW(regs, (unsigned short)*PC);
                  *PC = Z80ReadWord(usAddr);  /* Jump to address */
                  break;
               }
            }
         } /* Interrupt pending */
    return 0; /* No opcode to return, just a new PC */

} /* CheckInterrupts() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EXECZ80(char *, REGSZ80 *, EMUHANDLERS *, int *, char *, char, int, ulong *)   *
 *                                                                          *
 *  PURPOSE    : Emulate the Z80 microprocessor for N clock cycles.         *
 *                                                                          *
 ****************************************************************************/
void EXECZ80(unsigned char *mem, REGSZ80 *regs, EMUHANDLERS *emuh, int *iClocks, unsigned char *ucIRQs) //, int iHack)
{
register unsigned short usAddr; /* Temp address */
register unsigned char ucTemp;
register unsigned short usTemp;
//static unsigned int iPCCompare = 0xffff;
int i;
unsigned char ucOpcode, c;
//unsigned int iOldClocks;
uint32_t PC;  /* Current Program Counter address - use a long to get rid of extra "andi 0xffff" */
//uint32_t ulOldPC;
unsigned char *p; //, *mem_decrypt;
int iEITicks;

   p = NULL; // suppress compiler warning
   mem_handlers_z80 = emuh; /* Assign to static for faster execution */
   m_map_z80 = mem; /* ditto */
//   mem_decrypt = mem + MEM_DECRYPT;
   m_map_z80_flags = mem + MEM_FLAGS;
   iRealClocks = iClocks; /* Pointer to external var */
  /* iOldClocks =*/ iMyClocks = *iClocks; /* Global for speed */
   if (regs->ulOffsets[0]) // memory offset table is defined
	   z80_ulCPUOffs = &regs->ulOffsets[0]; // keep global copy
   else
	   z80_ulCPUOffs = NULL;
   iEITicks = 0;

   PC = (uint32_t)regs->usRegPC;

top_emulation_loop:
   if ((ucOpcode = CheckInterrupts((char *)mem, regs, &PC, (char *)ucIRQs, regs->ucIRQVal)) != 0)// If one of the 'special' types of interrupts, use the opcode returned
      goto z80doit; /* Use the new opcode */

   while (iMyClocks > 0) /* Execute for the amount of time alloted */
      {
#ifdef _DEBUG
      if (bTrace)
         {
         regs->usRegPC = (unsigned short)PC;
         TRACEZ80(regs, iMyClocks);
         ulTraceCount++;
         }
//      if (PC == 0x1e) //0x73de && ulTraceCount >= 0x114A0)
//         bTrace = TRUE;
//      if (ulTraceCount >= 0x92BF9)
//         ucOpcode = 0;
//      if (PC == 0x7c0)
//         ucOpcode = 0;
#endif // _DEBUG

//      if (PC == (uint32_t)iHack) /* Busy loop, break out */
//         {
//         if (*ucIRQs == 0) /* Only exit if no pending interrupts */
//            {
//            iMyClocks = 1;  // allow 1 more instruction to execute
//            }
//         }
      if (z80_ulCPUOffs)
         p = (unsigned char *)(PC + z80_ulCPUOffs[PC >> 12]); // get the indirect offset of this 4K block for faster bank switching
      else
         p = &mem[PC];
      ucOpcode = p[0];
#ifdef HISTO
      ulHisto[ucOpcode]++;
#endif // HISTO      
//      ucOpcode = mem_decrypt[PC];
      Z80OLDPC = PC;
//      ulOldPC = PC;
      PC++;
      regs->ucRegR++; // a reasonable approximation of the true behavior
z80doit:
      iMyClocks -= cZ80Cycles[ucOpcode];
      switch (ucOpcode)
         {
         case 0x00: /* NOP */
            break;
         case 0x40: /* LD B,B */
            break;      /* nop */
         case 0x49: /* LD C,C */
            break; /* nop */
         case 0x52: /* LD D,D */
            break; /* nop */
         case 0x5B: /* LD E,E */
            break; /* nop */
#ifdef BRANCH_AND_EXIT
// Careful with these because some games use these opcodes for timed NOPs
// Use 2 bogus opcodes to implement a relative branch and exit
// to save time in time-wasting loops
         case 0x64: // JR_EXIT Z
            if (regs->ucRegF & F_ZERO)
               {
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
               iMyClocks = 0; // we know this is a time wasting loop
               }
            else
               PC++;
            break;
         case 0x6d: // JR_EXIT NZ
            if (!(regs->ucRegF & F_ZERO))
               {
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
               iMyClocks = 0; // we know this is a time wasting loop
               }
            else
               PC++;
            break;
         case 0x7f: // JR_EXIT
            PC = (PC + 1 + (signed char)p[1]) & 0xffff;
            iMyClocks = 0; // we know this is a time wasting loop
            break;
#else
         case 0x64: /* LD H,H */
            break; /* nop */
         case 0x6D: /* LD L,L */
            break; /* nop */
         case 0x7F: /* LD A,A */
            break; /* nop */
#endif            

         case 0x01: /* LD BC,nn */
//            regs->usRegBC = Z80ReadWordFast(PC);
            regs->ucRegC = p[1];
            regs->ucRegB = p[2];
            PC += 2;
            break;
         case 0x11: /* LD DE,nn */
//            regs->usRegDE = Z80ReadWordFast(PC);
            regs->ucRegE = p[1];
            regs->ucRegD = p[2];
            PC += 2;
            break;
         case 0x21: /* LD HL,nn */
//            regs->usRegHL = Z80ReadWordFast(PC);
            regs->ucRegL = p[1];
            regs->ucRegH = p[2];
            PC += 2;
            break;
         case 0x31: /* LD SP,nn */
//            regs->usRegSP = Z80ReadWordFast(PC);
            regs->usRegSP = p[1] + (p[2] << 8);
            PC += 2;
            break;

         case 0x02: /* LD (BC),A */
            Z80WriteByte(regs->usRegBC, regs->ucRegA);
            break;

         case 0x03: /* INC BC */
            regs->usRegBC++;
            break;
         case 0x13: /* INC DE */
            regs->usRegDE++;
            break;
         case 0x23: /* INC HL */
            regs->usRegHL++;
            break;
         case 0x33: /* INC SP */
            regs->usRegSP++;
            break;

         case 0x04: /* INC B */
            regs->ucRegB = Z80INC(regs, regs->ucRegB);
            break;
         case 0x0C: /* INC C */
            regs->ucRegC = Z80INC(regs, regs->ucRegC);
            break;
         case 0x14: /* INC D */
            regs->ucRegD = Z80INC(regs, regs->ucRegD);
            break;
         case 0x1C: /* INC E */
            regs->ucRegE = Z80INC(regs, regs->ucRegE);
            break;
         case 0x24: /* INC H */
            regs->ucRegH = Z80INC(regs, regs->ucRegH);
            break;
         case 0x2C: /* INC L */
            regs->ucRegL = Z80INC(regs, regs->ucRegL);
            break;
         case 0x3C: /* INC A */
            regs->ucRegA = Z80INC(regs, regs->ucRegA);
            break;

         case 0x05: /* DEC B */
            regs->ucRegB = Z80DEC(regs, regs->ucRegB);
            break;
         case 0x0D: /* DEC C */
            regs->ucRegC = Z80DEC(regs, regs->ucRegC);
            break;
         case 0x15: /* DEC D */
            regs->ucRegD = Z80DEC(regs, regs->ucRegD);
            break;
         case 0x1D: /* DEC E */
            regs->ucRegE = Z80DEC(regs, regs->ucRegE);
            break;
         case 0x25: /* DEC H */
            regs->ucRegH = Z80DEC(regs, regs->ucRegH);
            break;
         case 0x2D: /* DEC L */
            regs->ucRegL = Z80DEC(regs, regs->ucRegL);
            break;
         case 0x3D: /* DEC A */
            regs->ucRegA = Z80DEC(regs, regs->ucRegA);
            break;

         case 0x06: /* LD B,n */
            regs->ucRegB = p[1];
            PC++;
            break;
         case 0x0E: /* LD C,n */
            regs->ucRegC = p[1];
            PC++;
            break;
         case 0x16: /* LD D,n */
            regs->ucRegD = p[1];
            PC++;
            break;
         case 0x1E: /* LD E,n */
            regs->ucRegE = p[1];
            PC++;
            break;
         case 0x26: /* LD H,n */
            regs->ucRegH = p[1];
            PC++;
            break;
         case 0x2E: /* LD L,n */
            regs->ucRegL = p[1];
            PC++;
            break;
         case 0x3E: /* LD A,n */
            regs->ucRegA = p[1];
            PC++;
            break;

         case 0x07: /* RLCA */
            Z80RLCA(regs);
            break;

         case 0x08: /* EX AF,AF' */
            usTemp = regs->usRegAF;
            regs->usRegAF = regs->usRegAF1;
            regs->usRegAF1 = usTemp;
            break;

         case 0x09: /* ADD HL,BC */
            regs->usRegHL = Z80ADDHL(regs, regs->usRegHL, regs->usRegBC);
            break;
         case 0x19: /* ADD HL,DE */
            regs->usRegHL = Z80ADDHL(regs, regs->usRegHL, regs->usRegDE);
            break;
         case 0x29: /* ADD HL,HL */
            regs->usRegHL = Z80ADDHL(regs, regs->usRegHL, regs->usRegHL);
            break;
         case 0x39: /* ADD HL,SP */
            regs->usRegHL = Z80ADDHL(regs, regs->usRegHL, regs->usRegSP);
            break;
         case 0x0A: /* LD A,(BC) */
            regs->ucRegA = Z80ReadByte(regs->usRegBC);
            break;

         case 0x0B: /* DEC BC */
            regs->usRegBC--;
            break;
         case 0x1B: /* DEC DE */
            regs->usRegDE--;
            break;
         case 0x2B: /* DEC HL */
            regs->usRegHL--;
            break;
         case 0x3B: /* DEC SP */
            regs->usRegSP--;
            break;

         case 0x0F: /* RRCA */
            Z80RRCA(regs);
            break;
         case 0x10: /* DJNZ (PC+e) */
            regs->ucRegB--;
            if (regs->ucRegB != 0)
               {
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
               iMyClocks -= 5;
//#ifdef FAST_LOOPS - this is always safe to do
               if (p[1] == 0xfe) // time wasting loop
                  {
                  if (regs->ucRegB * 13 > iMyClocks) // not enough cycles to do all
                     {
                     regs->ucRegB -= iMyClocks / 13;
                     iMyClocks = 0;
                     }
                  else
                     {
                     iMyClocks -= regs->ucRegB * 13;
                     regs->ucRegB = 0;
                     PC += 2; // don't repeat it
                     }
                  }
//#endif
               }
            else
               PC++;
            break;
         case 0x12: /* LD (DE),A */
            Z80WriteByte(regs->usRegDE, regs->ucRegA);
            break;
         case 0x17: /* RLA */
            Z80RLA(regs);
            break;
         case 0x18: /* JR e */
            PC = (PC + 1 + (signed char)p[1]) & 0xffff;
//#ifdef SPEED_HACKS
// this is always a safe hack
            if (p[1] == 0xfe) // time wasting loop
               iMyClocks = 0; // we can exit now
//#endif // SPEED_HACKS
            break;
         case 0x1A: /* LD A,(DE) */
            regs->ucRegA = Z80ReadByte(regs->usRegDE);
            break;
         case 0x1F: /* RRA */
            Z80RRA(regs);
            break;
         case 0x20: /* JR NZ,e */
            if (!(regs->ucRegF & F_ZERO))
               {
               iMyClocks -= 5;
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
#ifdef SPEED_HACKS
               if (p[1] >= 0xfa) // small loop, probably wasting time
                  iMyClocks = 0;
#endif // SPEED_HACKS
               }
            else
               PC++;
            break;
         case 0x22: /* LD (nn),HL */
//            usAddr = Z80ReadWordFast(PC);
            usAddr = p[1] + (p[2] << 8);            
            PC += 2;
            Z80WriteWord(usAddr, regs->usRegHL);
            break;
         case 0x27: /* DAA */
            usTemp = regs->ucRegA;
            if (regs->ucRegF & F_CARRY)
               usTemp += 0x100;
            if (regs->ucRegF & F_HALFCARRY)
               usTemp += 0x200;
            if (regs->ucRegF & F_ADDSUB)
               usTemp += 0x400;
            usTemp = usDAATable[usTemp];
            regs->ucRegA = (unsigned char)usTemp;
            regs->ucRegF = (unsigned char)(usTemp >> 8);
            break;
         case 0x28: /* JR Z,e */
            if (regs->ucRegF & F_ZERO)
               {
               iMyClocks -= 5;
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
#ifdef SPEED_HACKS
               if (p[1] >= 0xfa) // small loop, probably wasting time
                  iMyClocks = 0;
#endif // SPEED_HACKS
               }
            else
               PC++;
            break;
         case 0x2A: /* LD HL,(nn) */
//            usAddr = Z80ReadWordFast(PC);
            usAddr = p[1] + (p[2] << 8);
            PC += 2;
            regs->usRegHL = Z80ReadWord(usAddr);
            break;
         case 0x2F: /* CPL */
            regs->ucRegA = 255 - regs->ucRegA;
            regs->ucRegF |= (F_HALFCARRY | F_ADDSUB);
            break;
         case 0x30: /* JR NC,e */
            if (!(regs->ucRegF & F_CARRY))
               {
               iMyClocks -= 5;
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
#ifdef SPEED_HACKS
               if (p[1] >= 0xfa) // small loop, probably wasting time
                  iMyClocks = 0;
#endif // SPEED_HACKS
               }
            else
               PC++;
            break;
         case 0x32: /* LD (nn),A */
//            usAddr = Z80ReadWordFast(PC);
            usAddr = p[1] + (p[2] << 8);
            PC += 2;
            Z80WriteByte(usAddr, regs->ucRegA);
            break;
         case 0x34: /* INC (HL) */
            ucTemp = Z80ReadByte(regs->usRegHL);
            Z80WriteByte(regs->usRegHL, Z80INC(regs, ucTemp));
            break;
         case 0x35: /* DEC (HL) */
            ucTemp = Z80ReadByte(regs->usRegHL);
            Z80WriteByte(regs->usRegHL, Z80DEC(regs, ucTemp));
            break;
         case 0x36: /* LD (HL),n */
            Z80WriteByte(regs->usRegHL, p[1]);
            PC++;
            break;
         case 0x37: /* SCF */
            regs->ucRegF |= F_CARRY;
            regs->ucRegF &= ~(F_HALFCARRY | F_ADDSUB);
            break;
         case 0x38: /* JR C,e */
            if (regs->ucRegF & F_CARRY)
               {
               iMyClocks -= 5;
               PC = (PC + 1 + (signed char)p[1]) & 0xffff;
#ifdef SPEED_HACKS
               if (p[1] >= 0xfa) // small loop, probably wasting time
                  iMyClocks = 0;
#endif // SPEED_HACKS
               }
            else
               PC++;
            break;
         case 0x3A: /* LD A,(nn) */
//            usAddr = Z80ReadWordFast(PC);
            usAddr = p[1] + (p[2] << 8);
            PC += 2;
            regs->ucRegA = Z80ReadByte(usAddr);
            break;
         case 0x3F: /* CCF */
            ucTemp = regs->ucRegF; // carry is placed in half-carry
            regs->ucRegF ^= F_CARRY; // carry is complimented
            regs->ucRegF &= ~(F_ADDSUB + F_HALFCARRY);
            regs->ucRegF |= ((ucTemp << 4) & F_HALFCARRY);
            break;
         case 0x41: /* LD B,C */
            regs->ucRegB = regs->ucRegC;
            break;
         case 0x42: /* LD B,D */
            regs->ucRegB = regs->ucRegD;
            break;
         case 0x43: /* LD B,E */
            regs->ucRegB = regs->ucRegE;
            break;
         case 0x44: /* LD B,H */
            regs->ucRegB = regs->ucRegH;
            break;
         case 0x45: /* LD B,L */
            regs->ucRegB = regs->ucRegL;
            break;
         case 0x47: /* LD B,A */
            regs->ucRegB = regs->ucRegA;
            break;
         case 0x48: /* LD C,B */
            regs->ucRegC = regs->ucRegB;
            break;
         case 0x4A: /* LD C,D */
            regs->ucRegC = regs->ucRegD;
            break;
         case 0x4B: /* LD C,E */
            regs->ucRegC = regs->ucRegE;
            break;
         case 0x4C: /* LD C,H */
            regs->ucRegC = regs->ucRegH;
            break;
         case 0x4D: /* LD C,L */
            regs->ucRegC = regs->ucRegL;
            break;
         case 0x4F: /* LD C,A */
            regs->ucRegC = regs->ucRegA;
            break;
         case 0x50: /* LD D,B */
            regs->ucRegD = regs->ucRegB;
            break;
         case 0x51: /* LD D,C */
            regs->ucRegD = regs->ucRegC;
            break;
         case 0x53: /* LD D,E */
            regs->ucRegD = regs->ucRegE;
            break;
         case 0x54: /* LD D,H */
            regs->ucRegD = regs->ucRegH;
            break;
         case 0x55: /* LD D,L */
            regs->ucRegD = regs->ucRegL;
            break;
         case 0x57: /* LD D,A */
            regs->ucRegD = regs->ucRegA;
            break;
         case 0x58: /* LD E,B */
            regs->ucRegE = regs->ucRegB;
            break;
         case 0x59: /* LD E,C */
            regs->ucRegE = regs->ucRegC;
            break;
         case 0x5A: /* LD E,D */
            regs->ucRegE = regs->ucRegD;
            break;
         case 0x5C: /* LD E,H */
            regs->ucRegE = regs->ucRegH;
            break;
         case 0x5D: /* LD E,L */
            regs->ucRegE = regs->ucRegL;
            break;
         case 0x5F: /* LD E,A */
            regs->ucRegE = regs->ucRegA;
            break;
         case 0x60: /* LD H,B */
            regs->ucRegH = regs->ucRegB;
            break;
         case 0x61: /* LD H,C */
            regs->ucRegH = regs->ucRegC;
            break;
         case 0x62: /* LD H,D */
            regs->ucRegH = regs->ucRegD;
            break;
         case 0x63: /* LD H,E */
            regs->ucRegH = regs->ucRegE;
            break;
         case 0x65: /* LD H,L */
            regs->ucRegH = regs->ucRegL;
            break;
         case 0x67: /* LD H,A */
            regs->ucRegH = regs->ucRegA;
            break;
         case 0x68: /* LD L,B */
            regs->ucRegL = regs->ucRegB;
            break;
         case 0x69: /* LD L,C */
            regs->ucRegL = regs->ucRegC;
            break;
         case 0x6A: /* LD L,D */
            regs->ucRegL = regs->ucRegD;
            break;
         case 0x6B: /* LD L,E */
            regs->ucRegL = regs->ucRegE;
            break;
         case 0x6C: /* LD L,H */
            regs->ucRegL = regs->ucRegH;
            break;
         case 0x6F: /* LD L,A */
            regs->ucRegL = regs->ucRegA;
            break;
         case 0x78: /* LD A,B */
            regs->ucRegA = regs->ucRegB;
            break;
         case 0x79: /* LD A,C */
            regs->ucRegA = regs->ucRegC;
            break;
         case 0x7A: /* LD A,D */
            regs->ucRegA = regs->ucRegD;
            break;
         case 0x7B: /* LD A,E */
            regs->ucRegA = regs->ucRegE;
            break;
         case 0x7C: /* LD A,H */
            regs->ucRegA = regs->ucRegH;
            break;
         case 0x7D: /* LD A,L */
            regs->ucRegA = regs->ucRegL;
            break;

         case 0x46: /* LD B,(HL) */
            regs->ucRegB = Z80ReadByte(regs->usRegHL);
            break;
         case 0x4E: /* LD C,(HL) */
            regs->ucRegC = Z80ReadByte(regs->usRegHL);
            break;
         case 0x56: /* LD D,(HL) */
            regs->ucRegD = Z80ReadByte(regs->usRegHL);
            break;
         case 0x5E: /* LD E,(HL) */
            regs->ucRegE = Z80ReadByte(regs->usRegHL);
            break;
         case 0x66: /* LD H,(HL) */
            regs->ucRegH = Z80ReadByte(regs->usRegHL);
            break;
         case 0x6E: /* LD L,(HL) */
            regs->ucRegL = Z80ReadByte(regs->usRegHL);
            break;
         case 0x7E: /* LD A,(HL) */
            regs->ucRegA = Z80ReadByte(regs->usRegHL);
            break;
         case 0x70: /* LD (HL),B */
            Z80WriteByte(regs->usRegHL, regs->ucRegB);
            break;
         case 0x71: /* LD (HL),C */
            Z80WriteByte(regs->usRegHL, regs->ucRegC);
            break;
         case 0x72: /* LD (HL),D */
            Z80WriteByte(regs->usRegHL, regs->ucRegD);
            break;
         case 0x73: /* LD (HL),E */
            Z80WriteByte(regs->usRegHL, regs->ucRegE);
            break;
         case 0x74: /* LD (HL),H */
            Z80WriteByte(regs->usRegHL, regs->ucRegH);
            break;
         case 0x75: /* LD (HL),L */
            Z80WriteByte(regs->usRegHL, regs->ucRegL);
            break;
         case 0x77: /* LD (HL),A */
            Z80WriteByte(regs->usRegHL, regs->ucRegA);
            break;
         case 0x76: /* HALT */
            iMyClocks = 0;  /* Exit immediately */
            PC--;           /* stick on this instruction until an interrupt */
            regs->ucRegHALT = 1; // set flag indicating we are halted
            break;
         case 0x80: /* ADD A,B */
            Z80ADD(regs, regs->ucRegB);
            break;
         case 0x81: /* ADD A,C */
            Z80ADD(regs, regs->ucRegC);
            break;
         case 0x82: /* ADD A,D */
            Z80ADD(regs, regs->ucRegD);
            break;
         case 0x83: /* ADD A,E */
            Z80ADD(regs, regs->ucRegE);
            break;
         case 0x84: /* ADD A,H */
            Z80ADD(regs, regs->ucRegH);
            break;
         case 0x85: /* ADD A,L */
            Z80ADD(regs, regs->ucRegL);
            break;
         case 0x87: /* ADD A,A */
            Z80ADD(regs, regs->ucRegA);
            break;
         case 0x86: /* ADD A,(HL) */
            Z80ADD(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0x88: /* ADC A,B */
            Z80ADC(regs, regs->ucRegB);
            break;
         case 0x89: /* ADC A,C */
            Z80ADC(regs, regs->ucRegC);
            break;
         case 0x8A: /* ADC A,D */
            Z80ADC(regs, regs->ucRegD);
            break;
         case 0x8B: /* ADC A,E */
            Z80ADC(regs, regs->ucRegE);
            break;
         case 0x8C: /* ADC A,H */
            Z80ADC(regs, regs->ucRegH);
            break;
         case 0x8D: /* ADC A,L */
            Z80ADC(regs, regs->ucRegL);
            break;
         case 0x8F: /* ADC A,A */
            Z80ADC(regs, regs->ucRegA);
            break;
         case 0x8E: /* ADC A,(HL) */
            Z80ADC(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0x90: /* SUB B */
            Z80SUB(regs, regs->ucRegB);
            break;
         case 0x91: /* SUB C */
            Z80SUB(regs, regs->ucRegC);
            break;
         case 0x92: /* SUB D */
            Z80SUB(regs, regs->ucRegD);
            break;
         case 0x93: /* SUB E */
            Z80SUB(regs, regs->ucRegE);
            break;
         case 0x94: /* SUB H */
            Z80SUB(regs, regs->ucRegH);
            break;
         case 0x95: /* SUB L */
            Z80SUB(regs, regs->ucRegL);
            break;
         case 0x97: /* SUB A */
            Z80SUB(regs, regs->ucRegA);
            break;
         case 0x96: /* SUB (HL) */
            Z80SUB(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0x98: /* SBC A,B */
            Z80SBC(regs, regs->ucRegB);
            break;
         case 0x99: /* SBC A,C */
            Z80SBC(regs, regs->ucRegC);
            break;
         case 0x9A: /* SBC A,D */
            Z80SBC(regs, regs->ucRegD);
            break;
         case 0x9B: /* SBC A,E */
            Z80SBC(regs, regs->ucRegE);
            break;
         case 0x9C: /* SBC A,H */
            Z80SBC(regs, regs->ucRegH);
            break;
         case 0x9D: /* SBC A,L */
            Z80SBC(regs, regs->ucRegL);
            break;
         case 0x9F: /* SBC A,A */
            Z80SBC(regs, regs->ucRegA);
            break;
         case 0x9E: /* SBC A,(HL) */
            Z80SBC(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0xA0: /* AND B */
            Z80AND(regs, regs->ucRegB);
            break;
         case 0xA1: /* AND C */
            Z80AND(regs, regs->ucRegC);
            break;
         case 0xA2: /* AND D */
            Z80AND(regs, regs->ucRegD);
            break;
         case 0xA3: /* AND E */
            Z80AND(regs, regs->ucRegE);
            break;
         case 0xA4: /* AND H */
            Z80AND(regs, regs->ucRegH);
            break;
         case 0xA5: /* AND L */
            Z80AND(regs, regs->ucRegL);
            break;
         case 0xA7: /* AND A */
            Z80AND(regs, regs->ucRegA);
            break;
         case 0xA6: /* AND (HL) */
            Z80AND(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0xA8: /* XOR B */
            Z80XOR(regs, regs->ucRegB);
            break;
         case 0xA9: /* XOR C */
            Z80XOR(regs, regs->ucRegC);
            break;
         case 0xAA: /* XOR D */
            Z80XOR(regs, regs->ucRegD);
            break;
         case 0xAB: /* XOR E */
            Z80XOR(regs, regs->ucRegE);
            break;
         case 0xAC: /* XOR H */
            Z80XOR(regs, regs->ucRegH);
            break;
         case 0xAD: /* XOR L */
            Z80XOR(regs, regs->ucRegL);
            break;
         case 0xAF: /* XOR A */
            Z80XOR(regs, regs->ucRegA);
            break;
         case 0xAE: /* XOR (HL) */
            Z80XOR(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0xB0: /* OR B */
            Z80OR(regs, regs->ucRegB);
            break;
         case 0xB1: /* OR C */
            Z80OR(regs, regs->ucRegC);
            break;
         case 0xB2: /* OR D */
            Z80OR(regs, regs->ucRegD);
            break;
         case 0xB3: /* OR E */
            Z80OR(regs, regs->ucRegE);
            break;
         case 0xB4: /* OR H */
            Z80OR(regs, regs->ucRegH);
            break;
         case 0xB5: /* OR L */
            Z80OR(regs, regs->ucRegL);
            break;
         case 0xB7: /* OR A */
            Z80OR(regs, regs->ucRegA);
            break;
         case 0xB6: /* OR (HL) */
            Z80OR(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0xB8: /* CP B */
            Z80CMP(regs, regs->ucRegB);
            break;
         case 0xB9: /* CP C */
            Z80CMP(regs, regs->ucRegC);
            break;
         case 0xBA: /* CP D */
            Z80CMP(regs, regs->ucRegD);
            break;
         case 0xBB: /* CP E */
            Z80CMP(regs, regs->ucRegE);
            break;
         case 0xBC: /* CP H */
            Z80CMP(regs, regs->ucRegH);
            break;
         case 0xBD: /* CP L */
            Z80CMP(regs, regs->ucRegL);
            break;
         case 0xBF: /* CP A */
            Z80CMP(regs, regs->ucRegA);
            break;
         case 0xBE: /* CP (HL) */
            Z80CMP(regs, Z80ReadByte(regs->usRegHL));
            break;
         case 0xC0: /* RET NZ */
            if (!(regs->ucRegF & F_ZERO))
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xC1: /* POP BC */
            regs->usRegBC = Z80POPW(regs);
            break;
         case 0xC2: /* JP NZ,(nn) */
            if (!(regs->ucRegF & F_ZERO))
               {
//               PC = Z80ReadWordFast(PC);
               usAddr = p[1] + (p[2] << 8);
               iMyClocks -= 5;
#ifdef SPEED_HACKS
               if (PC - usAddr <= 3) // time wasting loop
                  iMyClocks = 0;
#endif // SPEED_HACKS
               PC = usAddr;
               }
            else
               PC += 2;
            break;
         case 0xC3: /* JP (nn) */
//            PC = Z80ReadWordFast(PC);
            PC = p[1] + (p[2] << 8);
            break;
         case 0xC4: /* CALL NZ,(nn) */
            if (!(regs->ucRegF & F_ZERO))
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xC5: /* PUSH BC */
            Z80PUSHW(regs, regs->usRegBC);
            break;
         case 0xC6: /* ADD A,n */
            Z80ADD(regs, p[1]);
            PC++;
            break;
         case 0xC7: /* RST 0H */
         case 0xCF: /* RST 8H */
         case 0xD7: /* RST 10H */
         case 0xDF: /* RST 18H */
         case 0xE7: /* RST 20H */
         case 0xEF: /* RST 28H */
         case 0xF7: /* RST 30H */
         case 0xFF: /* RST 38H */
            Z80PUSHW(regs, (unsigned short)PC);
            PC = ucOpcode & 0x38;
            break;
         case 0xC8: /* RET Z */
            if (regs->ucRegF & F_ZERO)
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xC9: /* RET */
            PC = Z80POPW(regs);
            break;
         case 0xCA: /* JP Z,(nn) */
            if (regs->ucRegF & F_ZERO)
               {
               usAddr = p[1] + (p[2] << 8);
               iMyClocks -= 5;
#ifdef SPEED_HACKS
               if (PC - usAddr <= 3) // time wasting loop
                  iMyClocks = 0;
#endif // SPEED_HACKS
               PC = usAddr;
               }
            else
               PC += 2;
            break;
         case 0xCB: /* New set of opcodes branches here */
            regs->ucRegR++;
//            ucOpcode = mem_decrypt[PC];
            ucOpcode = p[1];
#ifdef HISTO
            ulHisto[0x100 + ucOpcode]++;
#endif // HISTO      
            PC++;
            switch(ucOpcode)
               {
               case 0x00: /* RLC B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80RLC(regs, regs->ucRegB);
                  break;
               case 0x01: /* RLC C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80RLC(regs, regs->ucRegC);
                  break;
               case 0x02: /* RLC D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80RLC(regs, regs->ucRegD);
                  break;
               case 0x03: /* RLC E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80RLC(regs, regs->ucRegE);
                  break;
               case 0x04: /* RLC H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80RLC(regs, regs->ucRegH);
                  break;
               case 0x05: /* RLC L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80RLC(regs, regs->ucRegL);
                  break;
               case 0x07: /* RLC A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80RLC(regs, regs->ucRegA);
                  break;
               case 0x06: /* RLC (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80RLC(regs, ucTemp));
                  break;
               case 0x08: /* RRC B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80RRC(regs, regs->ucRegB);
                  break;
               case 0x09: /* RRC C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80RRC(regs, regs->ucRegC);
                  break;
               case 0x0A: /* RRC D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80RRC(regs, regs->ucRegD);
                  break;
               case 0x0B: /* RRC E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80RRC(regs, regs->ucRegE);
                  break;
               case 0x0C: /* RRC H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80RRC(regs, regs->ucRegH);
                  break;
               case 0x0D: /* RRC L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80RRC(regs, regs->ucRegL);
                  break;
               case 0x0F: /* RRC A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80RRC(regs, regs->ucRegA);
                  break;
               case 0x0E: /* RRC (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80RRC(regs, ucTemp));
                  break;
               case 0x10: /* RL B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80RL(regs, regs->ucRegB);
                  break;
               case 0x11: /* RL C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80RL(regs, regs->ucRegC);
                  break;
               case 0x12: /* RL D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80RL(regs, regs->ucRegD);
                  break;
               case 0x13: /* RL E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80RL(regs, regs->ucRegE);
                  break;
               case 0x14: /* RL H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80RL(regs, regs->ucRegH);
                  break;
               case 0x15: /* RL L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80RL(regs, regs->ucRegL);
                  break;
               case 0x17: /* RL A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80RL(regs, regs->ucRegA);
                  break;
               case 0x16: /* RL (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80RL(regs, ucTemp));
                  break;
               case 0x18: /* RR B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80RR(regs, regs->ucRegB);
                  break;
               case 0x19: /* RR C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80RR(regs, regs->ucRegC);
                  break;
               case 0x1A: /* RR D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80RR(regs, regs->ucRegD);
                  break;
               case 0x1B: /* RR E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80RR(regs, regs->ucRegE);
                  break;
               case 0x1C: /* RR H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80RR(regs, regs->ucRegH);
                  break;
               case 0x1D: /* RR L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80RR(regs, regs->ucRegL);
                  break;
               case 0x1F: /* RR A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80RR(regs, regs->ucRegA);
                  break;
               case 0x1E: /* RR (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80RR(regs, ucTemp));
                  break;
               case 0x20: /* SLA B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80SLA(regs, regs->ucRegB);
                  break;
               case 0x21: /* SLA C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80SLA(regs, regs->ucRegC);
                  break;
               case 0x22: /* SLA D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80SLA(regs, regs->ucRegD);
                  break;
               case 0x23: /* SLA E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80SLA(regs, regs->ucRegE);
                  break;
               case 0x24: /* SLA H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80SLA(regs, regs->ucRegH);
                  break;
               case 0x25: /* SLA L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80SLA(regs, regs->ucRegL);
                  break;
               case 0x27: /* SLA A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80SLA(regs, regs->ucRegA);
                  break;
               case 0x26: /* SLA (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL, Z80SLA(regs, ucTemp));
                  break;
               case 0x28: /* SRA B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80SRA(regs, regs->ucRegB);
                  break;
               case 0x29: /* SRA C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80SRA(regs, regs->ucRegC);
                  break;
               case 0x2A: /* SRA D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80SRA(regs, regs->ucRegD);
                  break;
               case 0x2B: /* SRA E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80SRA(regs, regs->ucRegE);
                  break;
               case 0x2C: /* SRA H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80SRA(regs, regs->ucRegH);
                  break;
               case 0x2D: /* SRA L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80SRA(regs, regs->ucRegL);
                  break;
               case 0x2F: /* SRA A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80SRA(regs, regs->ucRegA);
                  break;
               case 0x2E: /* SRA (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80SRA(regs, ucTemp));
                  break;
               case 0x30: /* SLL B */
                  regs->ucRegB = Z80SLL(regs, regs->ucRegB);
                  break;
               case 0x31: /* SLL C */
                  regs->ucRegC = Z80SLL(regs, regs->ucRegC);
                  break;
               case 0x32: /* SLL D */
                  regs->ucRegD = Z80SLL(regs, regs->ucRegD);
                  break;
               case 0x33: /* SLL E */
                  regs->ucRegE = Z80SLL(regs, regs->ucRegE);
                  break;
               case 0x34: /* SLL H */
                  regs->ucRegH = Z80SLL(regs, regs->ucRegH);
                  break;
               case 0x35: /* SLL L */
                  regs->ucRegL = Z80SLL(regs, regs->ucRegL);
                  break;
               case 0x36: /* SLL (HL) */
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL, Z80SLL(regs, ucTemp));
                  break;
               case 0x37: /* SLL A */
                  regs->ucRegA = Z80SLL(regs, regs->ucRegA);
                  break;
               case 0x38: /* SRL B */
                  iMyClocks -= 8;
                  regs->ucRegB = Z80SRL(regs, regs->ucRegB);
                  break;
               case 0x39: /* SRL C */
                  iMyClocks -= 8;
                  regs->ucRegC = Z80SRL(regs, regs->ucRegC);
                  break;
               case 0x3A: /* SRL D */
                  iMyClocks -= 8;
                  regs->ucRegD = Z80SRL(regs, regs->ucRegD);
                  break;
               case 0x3B: /* SRL E */
                  iMyClocks -= 8;
                  regs->ucRegE = Z80SRL(regs, regs->ucRegE);
                  break;
               case 0x3C: /* SRL H */
                  iMyClocks -= 8;
                  regs->ucRegH = Z80SRL(regs, regs->ucRegH);
                  break;
               case 0x3D: /* SRL L */
                  iMyClocks -= 8;
                  regs->ucRegL = Z80SRL(regs, regs->ucRegL);
                  break;
               case 0x3F: /* SRL A */
                  iMyClocks -= 8;
                  regs->ucRegA = Z80SRL(regs, regs->ucRegA);
                  break;
               case 0x3E: /* SRL (HL) */
                  iMyClocks -= 15;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegHL,Z80SRL(regs, ucTemp));
                  break;

               case 0x40: /* BIT 0,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x41: /* BIT 0,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x42: /* BIT 0,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x43: /* BIT 0,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x44: /* BIT 0,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x45: /* BIT 0,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x47: /* BIT 0,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x48: /* BIT 1,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x49: /* BIT 1,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4A: /* BIT 1,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4B: /* BIT 1,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4C: /* BIT 1,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4D: /* BIT 1,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4F: /* BIT 1,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x50: /* BIT 2,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x51: /* BIT 2,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x52: /* BIT 2,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x53: /* BIT 2,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x54: /* BIT 2,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x55: /* BIT 2,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x57: /* BIT 2,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x58: /* BIT 3,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x59: /* BIT 3,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5A: /* BIT 3,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5B: /* BIT 3,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5C: /* BIT 3,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5D: /* BIT 3,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5F: /* BIT 3,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x60: /* BIT 4,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x61: /* BIT 4,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x62: /* BIT 4,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x63: /* BIT 4,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x64: /* BIT 4,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x65: /* BIT 4,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x67: /* BIT 4,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x68: /* BIT 5,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x69: /* BIT 5,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6A: /* BIT 5,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6B: /* BIT 5,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6C: /* BIT 5,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6D: /* BIT 5,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6F: /* BIT 5,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x70: /* BIT 6,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x71: /* BIT 6,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x72: /* BIT 6,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x73: /* BIT 6,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x74: /* BIT 6,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x75: /* BIT 6,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x77: /* BIT 6,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x78: /* BIT 7,B */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegB & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x79: /* BIT 7,C */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegC & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x7A: /* BIT 7,D */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegD & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x7B: /* BIT 7,E */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegE & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x7C: /* BIT 7,H */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegH & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x7D: /* BIT 7,L */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegL & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;
               case 0x7F: /* BIT 7,A */
                  iMyClocks -= 8;
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(regs->ucRegA & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;

               case 0x46: /* BIT 0,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 1))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x4E: /* BIT 1,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 2))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x56: /* BIT 2,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 4))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x5E: /* BIT 3,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 8))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x66: /* BIT 4,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 16))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x6E: /* BIT 5,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 32))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x76: /* BIT 6,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 64))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  break;
               case 0x7E: /* BIT 7,(HL) */
                  iMyClocks -= 12;
                  c = Z80ReadByte(regs->usRegHL);
                  regs->ucRegF &= F_CARRY; // only preserves carry
                  regs->ucRegF |= F_HALFCARRY;
                  if (!(c & 128))
                     regs->ucRegF |= F_ZERO + F_OVERFLOW;
                  else
                     regs->ucRegF |= F_SIGN;
                  break;

               case 0x80: /* RES 0,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~1;
                  break;
               case 0x81: /* RES 0,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~1;
                  break;
               case 0x82: /* RES 0,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~1;
                  break;
               case 0x83: /* RES 0,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~1;
                  break;
               case 0x84: /* RES 0,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~1;
                  break;
               case 0x85: /* RES 0,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~1;
                  break;
               case 0x87: /* RES 0,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~1;
                  break;
               case 0x88: /* RES 1,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~2;
                  break;
               case 0x89: /* RES 1,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~2;
                  break;
               case 0x8A: /* RES 1,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~2;
                  break;
               case 0x8B: /* RES 1,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~2;
                  break;
               case 0x8C: /* RES 1,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~2;
                  break;
               case 0x8D: /* RES 1,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~2;
                  break;
               case 0x8F: /* RES 1,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~2;
                  break;
               case 0x90: /* RES 2,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~4;
                  break;
               case 0x91: /* RES 2,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~4;
                  break;
               case 0x92: /* RES 2,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~4;
                  break;
               case 0x93: /* RES 2,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~4;
                  break;
               case 0x94: /* RES 2,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~4;
                  break;
               case 0x95: /* RES 2,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~4;
                  break;
               case 0x97: /* RES 2,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~4;
                  break;
               case 0x98: /* RES 3,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~8;
                  break;
               case 0x99: /* RES 3,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~8;
                  break;
               case 0x9A: /* RES 3,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~8;
                  break;
               case 0x9B: /* RES 3,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~8;
                  break;
               case 0x9C: /* RES 3,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~8;
                  break;
               case 0x9D: /* RES 3,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~8;
                  break;
               case 0x9F: /* RES 3,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~8;
                  break;
               case 0xA0: /* RES 4,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~16;
                  break;
               case 0xA1: /* RES 4,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~16;
                  break;
               case 0xA2: /* RES 4,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~16;
                  break;
               case 0xA3: /* RES 4,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~16;
                  break;
               case 0xA4: /* RES 4,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~16;
                  break;
               case 0xA5: /* RES 4,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~16;
                  break;
               case 0xA7: /* RES 4,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~16;
                  break;
               case 0xA8: /* RES 5,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~32;
                  break;
               case 0xA9: /* RES 5,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~32;
                  break;
               case 0xAA: /* RES 5,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~32;
                  break;
               case 0xAB: /* RES 5,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~32;
                  break;
               case 0xAC: /* RES 5,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~32;
                  break;
               case 0xAD: /* RES 5,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~32;
                  break;
               case 0xAF: /* RES 5,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~32;
                  break;
               case 0xB0: /* RES 6,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~64;
                  break;
               case 0xB1: /* RES 6,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~64;
                  break;
               case 0xB2: /* RES 6,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~64;
                  break;
               case 0xB3: /* RES 6,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~64;
                  break;
               case 0xB4: /* RES 6,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~64;
                  break;
               case 0xB5: /* RES 6,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~64;
                  break;
               case 0xB7: /* RES 6,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~64;
                  break;
               case 0xB8: /* RES 7,B */
                  iMyClocks -= 8;
                  regs->ucRegB &= ~128;
                  break;
               case 0xB9: /* RES 7,C */
                  iMyClocks -= 8;
                  regs->ucRegC &= ~128;
                  break;
               case 0xBA: /* RES 7,D */
                  iMyClocks -= 8;
                  regs->ucRegD &= ~128;
                  break;
               case 0xBB: /* RES 7,E */
                  iMyClocks -= 8;
                  regs->ucRegE &= ~128;
                  break;
               case 0xBC: /* RES 7,H */
                  iMyClocks -= 8;
                  regs->ucRegH &= ~128;
                  break;
               case 0xBD: /* RES 7,L */
                  iMyClocks -= 8;
                  regs->ucRegL &= ~128;
                  break;
               case 0xBF: /* RES 7,A */
                  iMyClocks -= 8;
                  regs->ucRegA &= ~128;
                  break;

               case 0x86: /* RES 0,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~1;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0x8E: /* RES 1,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~2;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0x96: /* RES 2,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~4;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0x9E: /* RES 3,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~8;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xA6: /* RES 4,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~16;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xAE: /* RES 5,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~32;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xB6: /* RES 6,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~64;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xBE: /* RES 7,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) & ~128;
                  Z80WriteByte(regs->usRegHL, c);
                  break;

               case 0xC0: /* SET 0,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 1;
                  break;
               case 0xC1: /* SET 0,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 1;
                  break;
               case 0xC2: /* SET 0,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 1;
                  break;
               case 0xC3: /* SET 0,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 1;
                  break;
               case 0xC4: /* SET 0,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 1;
                  break;
               case 0xC5: /* SET 0,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 1;
                  break;
               case 0xC7: /* SET 0,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 1;
                  break;
               case 0xC8: /* SET 1,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 2;
                  break;
               case 0xC9: /* SET 1,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 2;
                  break;
               case 0xCA: /* SET 1,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 2;
                  break;
               case 0xCB: /* SET 1,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 2;
                  break;
               case 0xCC: /* SET 1,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 2;
                  break;
               case 0xCD: /* SET 1,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 2;
                  break;
               case 0xCF: /* SET 1,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 2;
                  break;
               case 0xD0: /* SET 2,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 4;
                  break;
               case 0xD1: /* SET 2,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 4;
                  break;
               case 0xD2: /* SET 2,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 4;
                  break;
               case 0xD3: /* SET 2,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 4;
                  break;
               case 0xD4: /* SET 2,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 4;
                  break;
               case 0xD5: /* SET 2,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 4;
                  break;
               case 0xD7: /* SET 2,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 4;
                  break;
               case 0xD8: /* SET 3,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 8;
                  break;
               case 0xD9: /* SET 3,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 8;
                  break;
               case 0xDA: /* SET 3,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 8;
                  break;
               case 0xDB: /* SET 3,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 8;
                  break;
               case 0xDC: /* SET 3,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 8;
                  break;
               case 0xDD: /* SET 3,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 8;
                  break;
               case 0xDF: /* SET 3,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 8;
                  break;
               case 0xE0: /* SET 4,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 16;
                  break;
               case 0xE1: /* SET 4,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 16;
                  break;
               case 0xE2: /* SET 4,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 16;
                  break;
               case 0xE3: /* SET 4,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 16;
                  break;
               case 0xE4: /* SET 4,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 16;
                  break;
               case 0xE5: /* SET 4,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 16;
                  break;
               case 0xE7: /* SET 4,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 16;
                  break;
               case 0xE8: /* SET 5,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 32;
                  break;
               case 0xE9: /* SET 5,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 32;
                  break;
               case 0xEA: /* SET 5,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 32;
                  break;
               case 0xEB: /* SET 5,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 32;
                  break;
               case 0xEC: /* SET 5,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 32;
                  break;
               case 0xED: /* SET 5,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 32;
                  break;
               case 0xEF: /* SET 5,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 32;
                  break;
               case 0xF0: /* SET 6,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 64;
                  break;
               case 0xF1: /* SET 6,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 64;
                  break;
               case 0xF2: /* SET 6,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 64;
                  break;
               case 0xF3: /* SET 6,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 64;
                  break;
               case 0xF4: /* SET 6,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 64;
                  break;
               case 0xF5: /* SET 6,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 64;
                  break;
               case 0xF7: /* SET 6,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 64;
                  break;
               case 0xF8: /* SET 7,B */
                  iMyClocks -= 8;
                  regs->ucRegB |= 128;
                  break;
               case 0xF9: /* SET 7,C */
                  iMyClocks -= 8;
                  regs->ucRegC |= 128;
                  break;
               case 0xFA: /* SET 7,D */
                  iMyClocks -= 8;
                  regs->ucRegD |= 128;
                  break;
               case 0xFB: /* SET 7,E */
                  iMyClocks -= 8;
                  regs->ucRegE |= 128;
                  break;
               case 0xFC: /* SET 7,H */
                  iMyClocks -= 8;
                  regs->ucRegH |= 128;
                  break;
               case 0xFD: /* SET 7,L */
                  iMyClocks -= 8;
                  regs->ucRegL |= 128;
                  break;
               case 0xFF: /* SET 7,A */
                  iMyClocks -= 8;
                  regs->ucRegA |= 128;
                  break;

               case 0xC6: /* SET 0,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 1;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xCE: /* SET 1,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 2;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xD6: /* SET 2,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 4;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xDE: /* SET 3,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 8;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xE6: /* SET 4,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 16;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xEE: /* SET 5,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 32;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xF6: /* SET 6,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 64;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               case 0xFE: /* SET 7,(HL) */
                  iMyClocks -= 15;
                  c = Z80ReadByte(regs->usRegHL) | 128;
                  Z80WriteByte(regs->usRegHL, c);
                  break;
               default: /* ILLEGAL */
                  iMyClocks = 0;
                  break;
               } /* switch on CB */
            break;
         case 0xCC: /* CALL Z,(nn) */
            if (regs->ucRegF & F_ZERO)
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xCD: /* CALL (nn) */
            Z80PUSHW(regs, (unsigned short)(PC+2));
            PC = p[1] + (p[2] << 8);
//            PC = Z80ReadWordFast(PC);
            break;
         case 0xCE: /* ADC A,n */
            Z80ADC(regs, p[1]);
            PC++;
            break;
         case 0xD0: /* RET NC */
            if (!(regs->ucRegF & F_CARRY))
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xD1: /* POP DE */
            regs->usRegDE = Z80POPW(regs);
            break;
         case 0xD2: /* JP NC,(nn) */
            if (!(regs->ucRegF & F_CARRY))
               {
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 5;
               }
            else
               PC += 2;
            break;
         case 0xD3: /* OUT (n),A */
            Z80OUT(regs, p[1], regs->ucRegA);
            PC++;
            break;
         case 0xD4: /* CALL NC,(nn) */
            if (!(regs->ucRegF & F_CARRY))
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xD5: /* PUSH DE */
            Z80PUSHW(regs, regs->usRegDE);
            break;
         case 0xD6: /* SUB n */
            Z80SUB(regs, p[1]);
            PC++;
            break;
         case 0xD8: /* RET C */
            if (regs->ucRegF & F_CARRY)
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xD9: /* EXX */
            usTemp = regs->usRegBC;
            regs->usRegBC = regs->usRegBC1;
            regs->usRegBC1 = usTemp;
            usTemp = regs->usRegDE;
            regs->usRegDE = regs->usRegDE1;
            regs->usRegDE1 = usTemp;
            usTemp = regs->usRegHL;
            regs->usRegHL = regs->usRegHL1;
            regs->usRegHL1 = usTemp;
            break;
         case 0xDA: /* JP C,(nn) */
            if (regs->ucRegF & F_CARRY)
               {
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 5;
               }
            else
               PC += 2;
            break;
         case 0xDB: /* IN A,(n) */
            regs->ucRegA = Z80IN2(regs, (unsigned short)(p[1] + (regs->ucRegA<<8)), iMyClocks);
            PC++;
            break;
         case 0xDC: /* CALL C,(nn) */
            if (regs->ucRegF & F_CARRY)
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xDD: /* A bunch of opcodes branch from here */
            ucOpcode = p[1];
#ifdef HISTO
            ulHisto[0x300 + ucOpcode]++;
#endif // HISTO      
            regs->ucRegR++;
//            ucOpcode = mem_decrypt[PC];
            PC++;
            iMyClocks -= cZ80Cycles2[ucOpcode];
            switch(ucOpcode)
               {
               case 0x09: /* ADD IX,BC */
                  regs->usRegIX = Z80ADD16(regs, regs->usRegIX, regs->usRegBC);
                  break;
               case 0x19: /* ADD IX,DE */
                  regs->usRegIX = Z80ADD16(regs, regs->usRegIX, regs->usRegDE);
                  break;
               case 0x21: /* LD IX,nn */
//                  regs->usRegIX = Z80ReadWordFast(PC);
                  regs->ucRegIXL = p[2];
                  regs->ucRegIXH = p[3];
                  PC += 2;
                  break;
               case 0x22: /* LD (nn),IX */
//                  usAddr = Z80ReadWordFast(PC);
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegIX);
                  break;
               case 0x23: /* INC IX */
                  regs->usRegIX++; /* No flags affected */
                  break;
               case 0x24: /* INC IXH */
                  regs->ucRegIXH = Z80INC(regs, regs->ucRegIXH);
                  break;
               case 0x25: /* DEC IXH */
                  regs->ucRegIXH = Z80DEC(regs, regs->ucRegIXH);
                  break;
               case 0x26: /* LD IXH,n */
                  regs->ucRegIXH = p[2];
                  PC++;
                  break;
               case 0x29: /* ADD IX,IX */
                  regs->usRegIX = Z80ADD16(regs, regs->usRegIX, regs->usRegIX);
                  break;
               case 0x2A: /* LD IX,(nn) */
                  usAddr = p[2] + (p[3] << 8);
//                  usAddr = Z80ReadWordFast(PC);
                  PC += 2;
                  regs->usRegIX = Z80ReadWord(usAddr);
                  break;
               case 0x2B: /* DEC IX */
                  regs->usRegIX--; /* No flags affected */
                  break;
               case 0x2C: /* INC IXL */
                  regs->ucRegIXL = Z80INC(regs, regs->ucRegIXL);
                  break;
               case 0x2D: /* DEC IXL */
                  regs->ucRegIXL = Z80DEC(regs, regs->ucRegIXL);
                  break;
               case 0x2E: /* LD IXL,n */
                  regs->ucRegIXL = p[2];
                  PC++;
                  break;
               case 0x34: /* INC (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
                  ucTemp = Z80ReadByte(usAddr);
                  Z80WriteByte(usAddr, Z80INC(regs, ucTemp));
                  break;
               case 0x35: /* DEC (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
                  ucTemp = Z80ReadByte(usAddr);
                  Z80WriteByte(usAddr, Z80DEC(regs, ucTemp));
                  break;
               case 0x36: /* LD (IX+d),n */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, p[3]);
                  PC++;
                  break;
               case 0x39: /* ADD IX,SP */
                  regs->usRegIX = Z80ADD16(regs, regs->usRegIX, regs->usRegSP);
                  break;
               case 0x40: // LD B,B  - NOP
                  break;
               case 0x41: // LD B,C
                  regs->ucRegB = regs->ucRegC;
                  break;
               case 0x42: // LD B,D
                  regs->ucRegB = regs->ucRegD;
                  break;
               case 0x43: // LD B,E
                  regs->ucRegB = regs->ucRegE;
                  break;
               case 0x44: /* LD B,IXH */
                  regs->ucRegB = regs->ucRegIXH;
                  break;
               case 0x45: /* LD B,IXL */
                  regs->ucRegB = regs->ucRegIXL;
                  break;
               case 0x47: // LD B,A
                  regs->ucRegB = regs->ucRegA;
                  break;
               case 0x48: // LD C,B
                  regs->ucRegC = regs->ucRegB;
                  break;
               case 0x49: // LD C,C
                  break;
               case 0x4A: // LD C,D
                  regs->ucRegC = regs->ucRegD;
                  break;
               case 0x4B: // LD C,E
                  regs->ucRegC = regs->ucRegE;
                  break;
               case 0x4C: /* LD C,IXH */
                  regs->ucRegC = regs->ucRegIXH;
                  break;
               case 0x4D: /* LD C,IXL */
                  regs->ucRegC = regs->ucRegIXL;
                  break;
               case 0x4F: // LD C,A
                  regs->ucRegC = regs->ucRegA;
                  break;
               case 0x50: // LD D,B
                  regs->ucRegD = regs->ucRegB;
                  break;
               case 0x51: // LD D,C
                  regs->ucRegD = regs->ucRegC;
                  break;
               case 0x52: // LD D,D
                  break;
               case 0x53: // LD D,E
                  regs->ucRegD = regs->ucRegE;
                  break;
               case 0x54: /* LD D,IXH */
                  regs->ucRegD = regs->ucRegIXH;
                  break;
               case 0x55: /* LD D,IXL */
                  regs->ucRegD = regs->ucRegIXL;
                  break;
               case 0x57: // LD D,A
                  regs->ucRegD = regs->ucRegA;
                  break;
               case 0x58: // LD E,B
                  regs->ucRegE = regs->ucRegB;
                  break;
               case 0x59: // LD E,C
                  regs->ucRegE = regs->ucRegC;
                  break;
               case 0x5A: // LD E,D
                  regs->ucRegE = regs->ucRegD;
                  break;
               case 0x5B: // LD E,E
                  break;
               case 0x5C: /* LD E,IXH */
                  regs->ucRegE = regs->ucRegIXH;
                  break;
               case 0x5D: /* LD E,IXL */
                  regs->ucRegE = regs->ucRegIXL;
                  break;
               case 0x5F: // LD E,A
                  regs->ucRegE = regs->ucRegA;
                  break;
               case 0x60: /* LD IXH,B */
                  regs->ucRegIXH = regs->ucRegB;
                  break;
               case 0x61: /* LD IXH,C */
                  regs->ucRegIXH = regs->ucRegC;
                  break;
               case 0x62: /* LD IXH,D */
                  regs->ucRegIXH = regs->ucRegD;
                  break;
               case 0x63: /* LD IXH,E */
                  regs->ucRegIXH = regs->ucRegE;
                  break;
               case 0x64: // LD IXH,IXH
                  break;
               case 0x65: /* LD IXH,IXL */
                  regs->ucRegIXH = regs->ucRegIXL;
                  break;
               case 0x67: /* LD IXH,A */
                  regs->ucRegIXH = regs->ucRegA;
                  break;
               case 0x68: /* LD IXL,B */
                  regs->ucRegIXL = regs->ucRegB;
                  break;
               case 0x69: /* LD IXL,C */
                  regs->ucRegIXL = regs->ucRegC;
                  break;
               case 0x6A: /* LD IXL,D */
                  regs->ucRegIXL = regs->ucRegD;
                  break;
               case 0x6B: /* LD IXL,E */
                  regs->ucRegIXL = regs->ucRegE;
                  break;
               case 0x6C: /* LD IXL,IXH */
                  regs->ucRegIXL = regs->ucRegIXH;
                  break;
               case 0x6D: /* LD IXL,IXL */
                  break;
               case 0x6F: /* LD IXL,A */
                  regs->ucRegIXL = regs->ucRegA;
                  break;
               case 0x46: /* LD B,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegB = Z80ReadByte(usAddr);
                  break;
               case 0x4E: /* LD C,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegC = Z80ReadByte(usAddr);
                  break;
               case 0x56: /* LD D,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegD = Z80ReadByte(usAddr);
                  break;
               case 0x5E: /* LD E,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegE = Z80ReadByte(usAddr);
                  break;
               case 0x66: /* LD H,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegH = Z80ReadByte(usAddr);
                  break;
               case 0x6E: /* LD L,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  regs->ucRegL = Z80ReadByte(usAddr);
                  break;
               case 0x7E: /* LD A,(IX+d) */
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
                  regs->ucRegA = Z80ReadByte(usAddr);
                  break;
               case 0x70: /* LD (IX+d),B */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegB);
                  break;
               case 0x71: /* LD (IX+d),C */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegC);
                  break;
               case 0x72: /* LD (IX+d),D */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegD);
                  break;
               case 0x73: /* LD (IX+d),E */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegE);
                  break;
               case 0x74: /* LD (IX+d),H */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegH);
                  break;
               case 0x75: /* LD (IX+d),L */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80WriteByte(usAddr, regs->ucRegL);
                  break;
               case 0x77: /* LD (IX+d),A */
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegA);
                  break;
               case 0x78: // LD A,B
                  regs->ucRegA = regs->ucRegB;
                  break;
               case 0x79: // LD A,C
                  regs->ucRegA = regs->ucRegC;
                  break;
               case 0x7A: // LD A,D
                  regs->ucRegA = regs->ucRegD;
                  break;
               case 0x7B: // LD A,E
                  regs->ucRegA = regs->ucRegE;
                  break;
               case 0x7C: /* LD A,IXH */
                  regs->ucRegA = regs->ucRegIXH;
                  break;
               case 0x7D: /* LD A,IXL */
                  regs->ucRegA = regs->ucRegIXL;
                  break;
               case 0x7F: // LD A,A
                  break;
               case 0x84: /* ADD IXH */
                  Z80ADD(regs, regs->ucRegIXH);
                  break;
               case 0x85: /* ADD IXL */
                  Z80ADD(regs, regs->ucRegIXL);
                  break;
               case 0x8C: /* ADC IXH */
                  Z80ADC(regs, regs->ucRegIXH);
                  break;
               case 0x8D: /* ADC IXL */
                  Z80ADC(regs, regs->ucRegIXL);
                  break;
               case 0x94: /* SUB IXH */
                  Z80SUB(regs, regs->ucRegIXH);
                  break;
               case 0x95: /* SUB IXL */
                  Z80SUB(regs, regs->ucRegIXL);
                  break;
               case 0x9C: /* SBC IXH */
                  Z80SBC(regs, regs->ucRegIXH);
                  break;
               case 0x9D: /* SBC IXL */
                  Z80SBC(regs, regs->ucRegIXL);
                  break;
               case 0xA4: /* AND IXH */
                  Z80AND(regs, regs->ucRegIXH);
                  break;
               case 0xA5: /* AND IXL */
                  Z80AND(regs, regs->ucRegIXL);
                  break;
               case 0xAC: /* XOR IXH */
                  Z80XOR(regs, regs->ucRegIXH);
                  break;
               case 0xAD: /* XOR IXL */
                  Z80XOR(regs, regs->ucRegIXL);
                  break;
               case 0xB4: /* OR IXH */
                  Z80OR(regs, regs->ucRegIXH);
                  break;
               case 0xB5: /* OR IXL */
                  Z80OR(regs, regs->ucRegIXL);
                  break;
               case 0xBC: /* CP IXH */
                  Z80CMP(regs, regs->ucRegIXH);
                  break;
               case 0xBD: /* CP IXL */
                  Z80CMP(regs, regs->ucRegIXL);
                  break;
               case 0x86: /* ADD A,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80ADD(regs, Z80ReadByte(usAddr));
                  break;
               case 0x8E: /* ADC A,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80ADC(regs, Z80ReadByte(usAddr));
                  break;
               case 0x96: /* SUB A,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80SUB(regs, Z80ReadByte(usAddr));
                  break;
               case 0x9E: /* SBC A,(IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80SBC(regs, Z80ReadByte(usAddr));
                  break;
               case 0xA6: /* AND (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80AND(regs, Z80ReadByte(usAddr));
                  break;
               case 0xAE: /* XOR (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80XOR(regs, Z80ReadByte(usAddr));
                  break;
               case 0xB6: /* OR (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80OR(regs, Z80ReadByte(usAddr));
                  break;
               case 0xBE: /* CP (IX+d) */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  Z80CMP(regs, Z80ReadByte(usAddr));
                  break;
               case 0xE1: /* POP IX */
                  regs->usRegIX = Z80POPW(regs);
                  break;
               case 0xE3: /* EX (SP),IX */
                  usTemp = Z80ReadWord(regs->usRegSP);
                  Z80WriteWord(regs->usRegSP, regs->usRegIX);
                  regs->usRegIX = usTemp;
                  break;
               case 0xE5: /* PUSH IX */
                  Z80PUSHW(regs, regs->usRegIX);
                  break;
               case 0xE9: /* JP (IX) */
                  PC = regs->usRegIX;
                  break;
               case 0xF9: /* LD SP,IX */
                  regs->usRegSP = regs->usRegIX;
                  break;
               case 0xCB: /* Another set of opcodes branches from here */
                  regs->ucRegR++;
               /* They all use (IX+d), so load the address */
                  usAddr = regs->usRegIX + (signed char)p[2];
                  PC++;
//                  usAddr = regs->usRegIX + (signed short)(signed char)Z80ReadByteFast(PC++);
                  ucTemp = Z80ReadByte(usAddr);
//                  ucOpcode = Z80ReadByteFast(PC++);
//                  iMyClocks -= cZ80Cycles3[ucOpcode];
//                  ucOpcode = mem_decrypt[PC];
                  PC++;
                  iMyClocks -= cZ80Cycles3[p[3]];
#ifdef HISTO
                  ulHisto[0x400 + p[3]]++;
#endif // HISTO      
//                  switch(ucOpcode)
                  switch (p[3])
                     {
                     case 0x06: /* RLC (IX+d) */
                        Z80WriteByte(usAddr, Z80RLC(regs, ucTemp));
                        break;
                     case 0x0E: /* RRC (IX+d) */
                        Z80WriteByte(usAddr, Z80RRC(regs, ucTemp));
                        break;
                     case 0x16: /* RL (IX+d) */
                        Z80WriteByte(usAddr, Z80RL(regs, ucTemp));
                        break;
                     case 0x1E: /* RR (IX+d) */
                        Z80WriteByte(usAddr, Z80RR(regs, ucTemp));
                        break;
                     case 0x26: /* SLA (IX+d) */
                        Z80WriteByte(usAddr, Z80SLA(regs, ucTemp));
                        break;
                     case 0x2E: /* SRA (IX+d) */
                        Z80WriteByte(usAddr, Z80SRA(regs, ucTemp));
                        break;
                     case 0x36: /* SLL (IX+d) */
                        Z80WriteByte(usAddr, Z80SLL(regs, ucTemp));
                        break;
                     case 0x3E: /* SRL (IX+d) */
                        Z80WriteByte(usAddr, Z80SRL(regs, ucTemp));
                        break;
                     case 0x46: /* BIT 0,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 1))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x4E: /* BIT 1,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 2))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x56: /* BIT 2,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 4))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x5E: /* BIT 3,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 8))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x66: /* BIT 4,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 16))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x6E: /* BIT 5,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 32))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x76: /* BIT 6,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 64))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x7E: /* BIT 7,(IX+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 128))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        else
                           regs->ucRegF |= F_SIGN;
                        break;
                     case 0x86: /* RES 0,(IX+d) */
                        ucTemp &= ~1;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x8E: /* RES 1,(IX+d) */
                        ucTemp &= ~2;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x96: /* RES 2,(IX+d) */
                        ucTemp &= ~4;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x9E: /* RES 3,(IX+d) */
                        ucTemp &= ~8;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xA6: /* RES 4,(IX+d) */
                        ucTemp &= ~16;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xAE: /* RES 5,(IX+d) */
                        ucTemp &= ~32;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xB6: /* RES 6,(IX+d) */
                        ucTemp &= ~64;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xBE: /* RES 7,(IX+d) */
                        ucTemp &= ~128;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xC6: /* SET 0,(IX+d) */
                        ucTemp |= 1;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xCE: /* SET 1,(IX+d) */
                        ucTemp |= 2;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xD6: /* SET 2,(IX+d) */
                        ucTemp |= 4;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xDE: /* SET 3,(IX+d) */
                        ucTemp |= 8;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xE6: /* SET 4,(IX+d) */
                        ucTemp |= 16;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xEE: /* SET 5,(IX+d) */
                        ucTemp |= 32;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xF6: /* SET 6,(IX+d) */
                        ucTemp |= 64;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xFE: /* SET 7,(IX+d) */
                        ucTemp |= 128;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     default: /* ILLEGAL DDCBXX opcodes */
                        iMyClocks = 0;
                        break;
                     } /* switch on DDCB */
                  break;
               default: /* ILLEGAL DDXX opcode */
                  iMyClocks = 0;
                  break;
               } /* switch on DD */
            break;
         case 0xDE: /* SBC A,n */
            Z80SBC(regs, p[1]);
            PC++;
            break;
         case 0xE0: /* RET PO */
            if (!(regs->ucRegF & F_OVERFLOW))
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xE1: /* POP HL */
            regs->usRegHL = Z80POPW(regs);
            break;
         case 0xE2: /* JP PO,(nn) */
            if (!(regs->ucRegF & F_OVERFLOW))
               {
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 5;
               }
            else
               PC += 2;
            break;
         case 0xE3: /* EX (SP),HL */
            usTemp = Z80ReadWord(regs->usRegSP);
            Z80WriteWord(regs->usRegSP, regs->usRegHL);
            regs->usRegHL = usTemp;
            break;
         case 0xE4: /* CALL PO,(nn) */
            if (!(regs->ucRegF & F_OVERFLOW))
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xE5: /* PUSH HL */
            Z80PUSHW(regs, regs->usRegHL);
            break;
         case 0xE6: /* AND n */
            Z80AND(regs, p[1]);
            PC++;
            break;
         case 0xE8: /* RET PE */
            if (regs->ucRegF & F_OVERFLOW)
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xE9: /* JP (HL) */
            PC = regs->usRegHL;
            break;
         case 0xEA: /* JP PE,(nn) */
            if (regs->ucRegF & F_OVERFLOW)
               {
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 5;
               }
            else
               PC += 2;
            break;
         case 0xEB: /* EX DE,HL */
            usTemp = regs->usRegDE;
            regs->usRegDE = regs->usRegHL;
            regs->usRegHL = usTemp;
            break;
         case 0xEC: /* CALL PE,(nn) */
            if (regs->ucRegF & F_OVERFLOW)
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
//               PC = Z80ReadWordFast(PC);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xED: /* lots of opcodes here */
            regs->ucRegR++;
//            ucOpcode = mem_decrypt[PC];
#ifdef HISTO
            ulHisto[0x200 + p[1]]++;
#endif // HISTO      
            PC++;
            switch(p[1])
//            switch (ucOpcode)
               {
               case 0x40: /* IN B,(C) */
                  iMyClocks -= 12;
                  regs->ucRegB = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x48: /* IN C,(C) */
                  iMyClocks -= 12;
                  regs->ucRegC = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x50: /* IN D,(C) */
                  iMyClocks -= 12;
                  regs->ucRegD = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x58: /* IN E,(C) */
                  iMyClocks -= 12;
                  regs->ucRegE = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x60: /* IN H,(C) */
                  iMyClocks -= 12;
                  regs->ucRegH = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x68: /* IN L,(C) */
                  iMyClocks -= 12;
                  regs->ucRegL = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x70: /* IN F,(C) */
                  iMyClocks -= 12;
                  Z80IN(regs, regs->usRegBC, iMyClocks); // only affects flags
                  break;
               case 0x78: /* IN A,(C) */
                  iMyClocks -= 12;
                  regs->ucRegA = Z80IN(regs, regs->usRegBC, iMyClocks);
                  break;
               case 0x41: /* OUT (C),B */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegB);
                  break;
               case 0x49: /* OUT (C),C */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegC);
                  break;
               case 0x51: /* OUT (C),D */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegD);
                  break;
               case 0x59: /* OUT (C),E */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegE);
                  break;
               case 0x61: /* OUT (C),H */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegH);
                  break;
               case 0x69: /* OUT (C),L */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegL);
                  break;
               case 0x79: /* OUT (C),A */
                  iMyClocks -= 12;
                  Z80OUT(regs, regs->usRegBC, regs->ucRegA);
                  break;
               case 0x42: /* SBC HL,BC */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80SBC16(regs, regs->usRegHL, regs->usRegBC);
                  break;
               case 0x52: /* SBC HL,DE */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80SBC16(regs, regs->usRegHL, regs->usRegDE);
                  break;
               case 0x62: /* SBC HL,HL */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80SBC16(regs, regs->usRegHL, regs->usRegHL);
                  break;
               case 0x72: /* SBC HL,SP */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80SBC16(regs, regs->usRegHL, regs->usRegSP);
                  break;

               case 0x43: /* LD (nn),BC */
                  iMyClocks -= 20;
//                  usAddr = Z80ReadWordFast(PC);
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegBC);
                  break;
               case 0x53: /* LD (nn),DE */
                  iMyClocks -= 20;
//                  usAddr = Z80ReadWordFast(PC);
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegDE);
                  break;
               case 0x63: /* LD (nn),HL */
                  iMyClocks -= 20;
//                  usAddr = Z80ReadWordFast(PC);
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegHL);
                  break;
               case 0x73: /* LD (nn),SP */
                  iMyClocks -= 20;
//                  usAddr = Z80ReadWordFast(PC);
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegSP);
                  break;

               case 0x44: /* NEG */
                  iMyClocks -= 8;
                  ucTemp = regs->ucRegA;
                  regs->ucRegA = 0;
                  Z80SUB(regs, ucTemp);
//                  ucTemp = regs->ucRegA;
//                  regs->ucRegF |= F_ADDSUB;
//                  regs->ucRegF &= ~(F_SIGN | F_ZERO | F_CARRY | F_HALFCARRY | F_OVERFLOW);
//                  regs->ucRegA = 0 - regs->ucRegA;
//                  regs->ucRegF |= cZ80SZ[regs->ucRegA];
//                  if (regs->ucRegA == 0x80) /* overflow condition */
//                     regs->ucRegF |= F_OVERFLOW;
//                  regs->ucRegF |= ((ucTemp ^ regs->ucRegA) & F_HALFCARRY);
                  break;
               case 0x45: /* RETN */
                  iMyClocks -= 8;
                  regs->ucRegIFF1 = regs->ucRegIFF2;
                  PC = Z80POPW(regs);
                  regs->ucRegNMI = 0; // Reset NMI state to prevent nesting of NMIs
                  if (*ucIRQs) // if any pending interrupts, we need to check them AFTER execution of the next instruction
                     {
                     if ((ucOpcode = CheckInterrupts((char *)mem, regs, &PC, (char *)ucIRQs, regs->ucIRQVal)) != 0)// If one of the 'special' types of interrupts, use the opcode returned
                        goto z80doit; /* Use the new opcode */
                     }
                  break;
               case 0x46: /* IM 0 */
                  iMyClocks -= 8;
                  regs->ucRegIM = 0;
                  break;
               case 0x47: /* LD I,A */
                  iMyClocks -= 9;
                  regs->ucRegI = regs->ucRegA;
                  break;
               case 0x4A: /* ADC HL,BC */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80ADC16(regs, regs->usRegHL, regs->usRegBC);
                  break;
               case 0x5A: /* ADC HL,DE */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80ADC16(regs, regs->usRegHL, regs->usRegDE);
                  break;
               case 0x6A: /* ADC HL,HL */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80ADC16(regs, regs->usRegHL, regs->usRegHL);
                  break;
               case 0x7A: /* ADC HL,SP */
                  iMyClocks -= 15;
                  regs->usRegHL = Z80ADC16(regs, regs->usRegHL, regs->usRegSP);
                  break;
               case 0x4B: /* LD BC,(nn) */
                  iMyClocks -= 20;
                  usAddr = p[2] + (p[3] << 8);
//                  usAddr = Z80ReadWordFast(PC);
                  PC += 2;
                  regs->usRegBC = Z80ReadWord(usAddr);
                  break;
               case 0x5B: /* LD DE,(nn) */
                  iMyClocks -= 20;
                  usAddr = p[2] + (p[3] << 8);
//                  usAddr = Z80ReadWordFast(PC);
                  PC += 2;
                  regs->usRegDE = Z80ReadWord(usAddr);
                  break;
               case 0x6B: /* LD HL,(nn) */
                  iMyClocks -= 20;
                  usAddr = p[2] + (p[3] << 8);
//                  usAddr = Z80ReadWordFast(PC);
                  PC += 2;
                  regs->usRegHL = Z80ReadWord(usAddr);
                  break;
               case 0x7B: /* LD SP,(nn) */
                  iMyClocks -= 20;
                  usAddr = p[2] + (p[3] << 8);
//                  usAddr = Z80ReadWordFast(PC);
                  PC += 2;
                  regs->usRegSP = Z80ReadWord(usAddr);
                  break;
               case 0x4D: /* RETI */
                  iMyClocks -= 14;
                  regs->ucRegNMI = 0;
                  PC = Z80POPW(regs);
                  break;
               case 0x4F: /* LD R,A */
                  iMyClocks -= 9;
                  regs->ucRegR = (unsigned int)regs->ucRegA;
                  regs->ucRegR2 = regs->ucRegA & 0x80;
                  break;
               case 0x56: /* IM 1 */
                  iMyClocks -= 8;
                  regs->ucRegIM = 1;
                  break;
               case 0x57: /* LD A,I */
                  iMyClocks -= 9;
                  regs->ucRegA = regs->ucRegI;
                  regs->ucRegF &= ~(F_SIGN | F_ZERO | F_OVERFLOW | F_HALFCARRY | F_ADDSUB);
                  regs->ucRegF |= cZ80SZ[regs->ucRegA];
                  if (regs->ucRegIFF2)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0x5E: /* IM 2 */
                  iMyClocks -= 8;
                  regs->ucRegIM = 2;
                  break;
               case 0x5F: /* LD A,R */
                  iMyClocks -= 9;
//                  regs->ucRegR = (regs->ucRegR & 0x80) + ((iMyClocks-iOldClocks)>>3 & 0x7f);
                  regs->ucRegA = (regs->ucRegR & 0x7f) | regs->ucRegR2;
                  regs->ucRegF &= F_CARRY; // only preserve carry
                  regs->ucRegF |= cZ80SZ[regs->ucRegA];
                  if (regs->ucRegIFF2)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0x67: /* RRD */
                  iMyClocks -= 18;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  c = regs->ucRegA & 0xf;
                  regs->ucRegA &= 0xf0;
                  regs->ucRegA |= (ucTemp & 0xf);
                  ucTemp >>= 4;
                  ucTemp |= (c << 4);
                  Z80WriteByte(regs->usRegHL, ucTemp);
                  regs->ucRegF &= F_CARRY; // only carry is preserved
                  regs->ucRegF |= cZ80SZParity[regs->ucRegA];
                  break;
               case 0x6F: /* RLD */
                  iMyClocks -= 18;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  c = regs->ucRegA & 0xf;
                  regs->ucRegA &= 0xf0;
                  regs->ucRegA |= (ucTemp >> 4);
                  ucTemp <<= 4;
                  ucTemp |= c;
                  Z80WriteByte(regs->usRegHL, ucTemp);
                  regs->ucRegF &= F_CARRY; // only carry is preserved
                  regs->ucRegF |= cZ80SZParity[regs->ucRegA];
                  break;
               case 0xA0: /* LDI */
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegDE, ucTemp);
                  regs->usRegDE++;
                  regs->usRegHL++;
                  regs->usRegBC--;
                  regs->ucRegF &= ~(F_HALFCARRY | F_ADDSUB | F_OVERFLOW);
                  if (regs->usRegBC)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0xA1: /* CPI */
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80CMP2(regs, ucTemp);
                  regs->usRegHL++;
                  regs->usRegBC--;
                  if (regs->usRegBC != 0)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0xA2: /* INI */
                  iMyClocks -= 16;
                  Z80WriteByte(regs->usRegHL, Z80IN(regs, regs->usRegBC, iMyClocks));
                  regs->usRegHL++;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     regs->ucRegF &= ~F_ZERO;
                  break;

               case 0xA3: /* OUTI */
                  iMyClocks -= 16;
                  Z80OUT(regs, regs->usRegBC, Z80ReadByte(regs->usRegHL));
                  regs->usRegHL++;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     regs->ucRegF &= ~F_ZERO;
                  break;
               case 0xA8: /* LDD */
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegDE, ucTemp);
                  regs->usRegDE--;
                  regs->usRegHL--;
                  regs->usRegBC--;
                  regs->ucRegF &= ~(F_HALFCARRY | F_ADDSUB | F_OVERFLOW);
                  if (regs->usRegBC)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0xA9: /* CPD */
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80CMP2(regs, ucTemp);
                  regs->usRegHL--;
                  regs->usRegBC--;
                  if (regs->usRegBC != 0)
                     regs->ucRegF |= F_OVERFLOW;
                  break;
               case 0xAA: /* IND */
                  iMyClocks -= 16;
                  Z80WriteByte(regs->usRegHL, Z80IN(regs, regs->usRegBC, iMyClocks));
                  regs->usRegHL--;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     regs->ucRegF &= ~F_ZERO;
                  break;
               case 0xAB: /* OUTD */
                  iMyClocks -= 16;
                  Z80OUT(regs, regs->usRegBC, Z80ReadByte(regs->usRegHL));
                  regs->usRegHL--;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     regs->ucRegF &= ~F_ZERO;
                  break;
               case 0xB0: /* LDIR */
/* Let's try to accelerate this */
                  regs->ucRegF &= ~(F_HALFCARRY | F_ADDSUB | F_OVERFLOW);
#ifdef FAST_LOOPS
                  usTemp = regs->usRegBC; // assume full count
                  if (iMyClocks < 21 * regs->usRegBC) // not enough time to do it all
                     usTemp = iMyClocks / 21;
                  for (i=0; i<usTemp; i++)
                     Z80WriteByte((unsigned short)(regs->usRegDE+i),Z80ReadByte((unsigned short)(regs->usRegHL+i)));
                  iMyClocks -= 21 * usTemp;
                  regs->usRegDE += usTemp;
                  regs->usRegHL += usTemp;
                  regs->usRegBC -= usTemp;
#else
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegDE, ucTemp);
                  regs->usRegDE++;
                  regs->usRegHL++;
                  regs->usRegBC--;
#endif
                  if (regs->usRegBC != 0)
                     {
                     regs->ucRegF |= F_OVERFLOW;
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat this instruction until BC == 0 */
                     }
                  break;
               case 0xB1: /* CPIR */
/* Let's try to accelerate this */
                  iMyClocks -= 16;
                  Z80CMP2(regs, Z80ReadByte(regs->usRegHL)); /* Set the flags for exit */
                  regs->usRegHL++;
                  regs->usRegBC--;
                  i = 0;
                  while (i < regs->usRegBC && regs->ucRegA != Z80ReadByte((unsigned short)(regs->usRegHL-1)))
                      {
                      i++;
                      regs->usRegHL++;
                      }
                  iMyClocks -= 21 * i; /* the count we used */
                  regs->usRegBC -= i;
                  Z80CMP2(regs, Z80ReadByte((unsigned short)(regs->usRegHL-1))); /* Set the flags for exit */
                  if (regs->usRegBC != 0)
                     regs->ucRegF |= F_OVERFLOW; // set overflow if count does not run out
                  break;
               case 0xB2: /* INIR */
                  iMyClocks -= 16;
                  Z80WriteByte(regs->usRegHL, Z80IN(regs, regs->usRegBC, iMyClocks));
                  regs->usRegHL++;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     {
                     regs->ucRegF &= ~F_ZERO;
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat this instruction until B==0 */
                     }
                  break;
               case 0xB3: /* OTIR */
                  iMyClocks -= 16;
                  Z80OUT(regs, regs->usRegBC, Z80ReadByte(regs->usRegHL));
                  regs->usRegHL++;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     {
                     regs->ucRegF &= ~F_ZERO;
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat until B==0 */
                     }
                  break;
               case 0xB8: /* LDDR */
                  iMyClocks -= 16;
                  regs->ucRegF &= ~(F_HALFCARRY | F_ADDSUB | F_OVERFLOW);
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80WriteByte(regs->usRegDE, ucTemp);
                  regs->usRegDE--;
                  regs->usRegHL--;
                  regs->usRegBC--;
                  if (regs->usRegBC != 0)
                     {
                     regs->ucRegF |= F_OVERFLOW; // if BC is non-zero, set overflow
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat this instruction until BC == 0 */
                     }
                  break;
               case 0xB9: /* CPDR */
                  iMyClocks -= 16;
                  ucTemp = Z80ReadByte(regs->usRegHL);
                  Z80CMP2(regs, ucTemp);
                  regs->usRegHL--;
                  regs->usRegBC--;
                  if (regs->usRegBC != 0)
                     regs->ucRegF |= F_OVERFLOW; // at end of cycle, overflow gets set
                  if (regs->usRegBC != 0 && ucTemp != regs->ucRegA)
                     {
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat this until BC==0 || A==(HL) */
                     }
                  break;
               case 0xBA: /* INDR */
                  iMyClocks -= 16;
                  Z80WriteByte(regs->usRegHL, Z80IN(regs, regs->usRegBC, iMyClocks));
                  regs->usRegHL--;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     {
                     regs->ucRegF &= ~F_ZERO;
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat this instruction until B==0 */
                     }
                  break;
               case 0xBB: /* OTDR */
                  iMyClocks -= 16;
                  Z80OUT(regs, regs->usRegBC, Z80ReadByte(regs->usRegHL));
                  regs->usRegHL--;
                  regs->ucRegB--;
                  regs->ucRegF |= F_ADDSUB;
                  if (regs->ucRegB == 0)
                     regs->ucRegF |= F_ZERO;
                  else
                     {
                     regs->ucRegF &= ~F_ZERO;
                     iMyClocks -= 5;
                     PC -= 2; /* Repeat until B==0 */
                     }
                  break;
               default: /* ILLEGAL */
                  iMyClocks = 0;
                  break;
               }
            break;
         case 0xEE: /* XOR n */
            PC++;
            Z80XOR(regs, p[1]);
            break;
         case 0xF0: /* RET P */
            if (!(regs->ucRegF & F_SIGN))
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xF1: /* POP AF */
            regs->usRegAF = Z80POPW(regs);
            break;
         case 0xF2: /* JP P,(nn) */
            if (!(regs->ucRegF & F_SIGN))
               {
               usAddr = p[1] + (p[2] << 8);
               iMyClocks -= 5;
#ifdef SPEED_HACKS
               if (PC - usAddr <= 3) // time wasting loop
                  iMyClocks = 0;
#endif // SPEED_HACKS
               PC = usAddr;
               }
            else
               PC += 2;
            break;
         case 0xF3: /* DI */
            regs->ucRegIFF1 = regs->ucRegIFF2 = 0;
            if (iEITicks) // did we just execute an EI instruction?
               {
               iMyClocks += iEITicks; // cancel the EI
               iEITicks = 0;
               }
            break;
         case 0xF4: /* CALL P,(nn) */
            if (!(regs->ucRegF & F_SIGN))
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xF5: /* PUSH AF */
            Z80PUSHW(regs, regs->usRegAF);
            break;
         case 0xF6: /* OR n */
            PC++;
            Z80OR(regs, p[1]);
            break;
         case 0xF8: /* RET M */
            if (regs->ucRegF & F_SIGN)
               {
               PC = Z80POPW(regs);
               iMyClocks -= 6;
               }
            break;
         case 0xF9: /* LD SP,HL */
            regs->usRegSP = regs->usRegHL;
            break;
         case 0xFA: /* JP M,(nn) */
            if (regs->ucRegF & F_SIGN)
               {
               usAddr = p[1] + (p[2] << 8);
               iMyClocks -= 5;
#ifdef SPEED_HACKS
               if (PC - usAddr <= 3) // time wasting loop
                  iMyClocks = 0;
#endif // SPEED_HACKS
               PC = usAddr;
               }
            else
               PC += 2;
            break;
         case 0xFB: /* EI */
            regs->ucRegIFF1 = regs->ucRegIFF2 = 1;
            regs->ucRegNMI = 0; // reset NMI status
            iEITicks = iMyClocks - 1;
            iMyClocks = 1; // allow 1 more instruction to execute
            break;
         case 0xFC: /* CALL M,(nn) */
            if (regs->ucRegF & F_SIGN)
               {
               Z80PUSHW(regs, (unsigned short)(PC+2));
               PC = p[1] + (p[2] << 8);
               iMyClocks -= 7;
               }
            else
               PC += 2;
            break;
         case 0xFD: /* lots of opcodes */
            regs->ucRegR++;
//            ucOpcode = mem_decrypt[PC];
            ucOpcode = p[1];
#ifdef HISTO
            ulHisto[0x300 + ucOpcode]++;
#endif // HISTO      
            PC++;
            iMyClocks -= cZ80Cycles2[ucOpcode];
            switch (ucOpcode)
               {
               case 0x09: /* ADD IY,BC */
                  regs->usRegIY = Z80ADD16(regs, regs->usRegIY, regs->usRegBC);
                  break;
               case 0x19: /* ADD IY,DE */
                  regs->usRegIY = Z80ADD16(regs, regs->usRegIY, regs->usRegDE);
                  break;
               case 0x29: /* ADD IY,IY */
                  regs->usRegIY = Z80ADD16(regs, regs->usRegIY, regs->usRegIY);
                  break;
               case 0x39: /* ADD IY,SP */
                  regs->usRegIY = Z80ADD16(regs, regs->usRegIY, regs->usRegSP);
                  break;
               case 0x21: /* LD IY,nn */
                  regs->usRegIY = p[2] + (p[3] << 8);
                  PC += 2;
                  break;
               case 0x22: /* LD (nn),IY */
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  Z80WriteWord(usAddr, regs->usRegIY);
                  break;
               case 0x23: /* INC IY */
                  regs->usRegIY++; /* No flags affected */
                  break;
               case 0x24: /* INC IYH */
                  regs->ucRegIYH = Z80INC(regs, regs->ucRegIYH);
                  break;
               case 0x25: /* DEC IYH */
                  regs->ucRegIYH = Z80DEC(regs, regs->ucRegIYH);
                  break;
               case 0x26: /* LD IYH,n */
                  regs->ucRegIYH = p[2];
                  PC++;
                  break;
               case 0x2A: /* LD IY,(nn) */
                  usAddr = p[2] + (p[3] << 8);
                  PC += 2;
                  regs->usRegIY = Z80ReadWord(usAddr);
                  break;
               case 0x2B: /* DEC IY */
                  regs->usRegIY--; /* No flags affected */
                  break;
               case 0x2C: /* INC IYL */
                  regs->ucRegIYL = Z80INC(regs, regs->ucRegIYL);
                  break;
               case 0x2D: /* DEC IYL */
                  regs->ucRegIYL = Z80DEC(regs, regs->ucRegIYL);
                  break;
               case 0x2E: /* LD IYL,n */
                  regs->ucRegIYL = p[2];
                  PC++;
                  break;
               case 0x34: /* INC (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  ucTemp = Z80ReadByte(usAddr);
                  Z80WriteByte(usAddr, Z80INC(regs, ucTemp));
                  break;
               case 0x35: /* DEC (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  ucTemp = Z80ReadByte(usAddr);
                  Z80WriteByte(usAddr, Z80DEC(regs, ucTemp));
                  break;
               case 0x36: /* LD (IY+d),n */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  ucTemp = p[3];
                  PC++;
                  Z80WriteByte(usAddr, ucTemp);
                  break;
               case 0x40: // LD B,B  - NOP
                  break;
               case 0x41: // LD B,C
                  regs->ucRegB = regs->ucRegC;
                  break;
               case 0x42: // LD B,D
                  regs->ucRegB = regs->ucRegD;
                  break;
               case 0x43: // LD B,E
                  regs->ucRegB = regs->ucRegE;
                  break;
               case 0x44: /* LD B,IYH */
                  regs->ucRegB = regs->ucRegIYH;
                  break;
               case 0x45: /* LD B,IYL */
                  regs->ucRegB = regs->ucRegIYL;
                  break;
               case 0x47: // LD B,A
                  regs->ucRegB = regs->ucRegA;
                  break;
               case 0x48: // LD C,B
                  regs->ucRegC = regs->ucRegB;
                  break;
               case 0x49: // LD C,C
                  break;
               case 0x4A: // LD C,D
                  regs->ucRegC = regs->ucRegD;
                  break;
               case 0x4B: // LD C,E
                  regs->ucRegC = regs->ucRegE;
                  break;
               case 0x4C: /* LD C,IYH */
                  regs->ucRegC = regs->ucRegIYH;
                  break;
               case 0x4D: /* LD C,IYL */
                  regs->ucRegC = regs->ucRegIYL;
                  break;
               case 0x4F: // LD C,A
                  regs->ucRegC = regs->ucRegA;
                  break;
               case 0x50: // LD D,B
                  regs->ucRegD = regs->ucRegB;
                  break;
               case 0x51: // LD D,C
                  regs->ucRegD = regs->ucRegC;
                  break;
               case 0x52: // LD D,D
                  break;
               case 0x53: // LD D,E
                  regs->ucRegD = regs->ucRegE;
                  break;
               case 0x54: /* LD D,IYH */
                  regs->ucRegD = regs->ucRegIYH;
                  break;
               case 0x55: /* LD D,IYL */
                  regs->ucRegD = regs->ucRegIYL;
                  break;
               case 0x57: // LD D,A
                  regs->ucRegD = regs->ucRegA;
                  break;
               case 0x58: // LD E,B
                  regs->ucRegE = regs->ucRegB;
                  break;
               case 0x59: // LD E,C
                  regs->ucRegE = regs->ucRegC;
                  break;
               case 0x5A: // LD E,D
                  regs->ucRegE = regs->ucRegD;
                  break;
               case 0x5B: // LD E,E
                  break;
               case 0x5C: /* LD E,IYH */
                  regs->ucRegE = regs->ucRegIYH;
                  break;
               case 0x5D: /* LD E,IYL */
                  regs->ucRegE = regs->ucRegIYL;
                  break;
               case 0x5F: // LD E,A
                  regs->ucRegE = regs->ucRegA;
                  break;
               case 0x60: /* LD IYH,B */
                  regs->ucRegIYH = regs->ucRegB;
                  break;
               case 0x61: /* LD IYH,C */
                  regs->ucRegIYH = regs->ucRegC;
                  break;
               case 0x62: /* LD IYH,D */
                  regs->ucRegIYH = regs->ucRegD;
                  break;
               case 0x63: /* LD IYH,E */
                  regs->ucRegIYH = regs->ucRegE;
                  break;
               case 0x64: // LD IYH,IYH
                  break;
               case 0x65: /* LD IYH,IYL */
                  regs->ucRegIYH = regs->ucRegIYL;
                  break;
               case 0x67: /* LD IYH,A */
                  regs->ucRegIYH = regs->ucRegA;
                  break;
               case 0x68: /* LD IYL,B */
                  regs->ucRegIYL = regs->ucRegB;
                  break;
               case 0x69: /* LD IYL,C */
                  regs->ucRegIYL = regs->ucRegC;
                  break;
               case 0x6A: /* LD IYL,D */
                  regs->ucRegIYL = regs->ucRegD;
                  break;
               case 0x6B: /* LD IYL,E */
                  regs->ucRegIYL = regs->ucRegE;
                  break;
               case 0x6C: /* LD IYL,IYH */
                  regs->ucRegIYL = regs->ucRegIYH;
                  break;
               case 0x6D: /* LD IYL,IYL */
                  break;
               case 0x6F: /* LD IYL,A */
                  regs->ucRegIYL = regs->ucRegA;
                  break;
               case 0x46: /* LD B,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegB = Z80ReadByte(usAddr);
                  break;
               case 0x4E: /* LD C,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegC = Z80ReadByte(usAddr);
                  break;
               case 0x56: /* LD D,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegD = Z80ReadByte(usAddr);
                  break;
               case 0x5E: /* LD E,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegE = Z80ReadByte(usAddr);
                  break;
               case 0x66: /* LD H,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegH = Z80ReadByte(usAddr);
                  break;
               case 0x6E: /* LD L,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegL = Z80ReadByte(usAddr);
                  break;
               case 0x7E: /* LD A,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  regs->ucRegA = Z80ReadByte(usAddr);
                  break;
               case 0x70: /* LD (IY+d),B */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegB);
                  break;
               case 0x71: /* LD (IY+d),C */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegC);
                  break;
               case 0x72: /* LD (IY+d),D */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegD);
                  break;
               case 0x73: /* LD (IY+d),E */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegE);
                  break;
               case 0x74: /* LD (IY+d),H */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegH);
                  break;
               case 0x75: /* LD (IY+d),L */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegL);
                  break;
               case 0x77: /* LD (IY+d),A */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80WriteByte(usAddr, regs->ucRegA);
                  break;
               case 0x78: // LD A,B
                  regs->ucRegA = regs->ucRegB;
                  break;
               case 0x79: // LD A,C
                  regs->ucRegA = regs->ucRegC;
                  break;
               case 0x7A: // LD A,D
                  regs->ucRegA = regs->ucRegD;
                  break;
               case 0x7B: // LD A,E
                  regs->ucRegA = regs->ucRegE;
                  break;
               case 0x7C: /* LD A,IYH */
                  regs->ucRegA = regs->ucRegIYH;
                  break;
               case 0x7D: /* LD A,IYL */
                  regs->ucRegA = regs->ucRegIYL;
                  break;
               case 0x7F: // LD A,A
                  break;
               case 0x84: /* ADD IYH */
                  Z80ADD(regs, regs->ucRegIYH);
                  break;
               case 0x85: /* ADD IYL */
                  Z80ADD(regs, regs->ucRegIYL);
                  break;
               case 0x8C: /* ADC IYH */
                  Z80ADC(regs, regs->ucRegIYH);
                  break;
               case 0x8D: /* ADC IYL */
                  Z80ADC(regs, regs->ucRegIYL);
                  break;
               case 0x94: /* SUB IYH */
                  Z80SUB(regs, regs->ucRegIYH);
                  break;
               case 0x95: /* SUB IYL */
                  Z80SUB(regs, regs->ucRegIYL);
                  break;
               case 0x9C: /* SBC IYH */
                  Z80SBC(regs, regs->ucRegIYH);
                  break;
               case 0x9D: /* SBC IYL */
                  Z80SBC(regs, regs->ucRegIYL);
                  break;
               case 0xA4: /* AND IYH */
                  Z80AND(regs, regs->ucRegIYH);
                  break;
               case 0xA5: /* AND IYL */
                  Z80AND(regs, regs->ucRegIYL);
                  break;
               case 0xAC: /* XOR IYH */
                  Z80XOR(regs, regs->ucRegIYH);
                  break;
               case 0xAD: /* XOR IYL */
                  Z80XOR(regs, regs->ucRegIYL);
                  break;
               case 0xB4: /* OR IYH */
                  Z80OR(regs, regs->ucRegIYH);
                  break;
               case 0xB5: /* OR IYL */
                  Z80OR(regs, regs->ucRegIYL);
                  break;
               case 0xBC: /* CP IYH */
                  Z80CMP(regs, regs->ucRegIYH);
                  break;
               case 0xBD: /* CP IYL */
                  Z80CMP(regs, regs->ucRegIYL);
                  break;
               case 0x86: /* ADD A,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80ADD(regs, Z80ReadByte(usAddr));
                  break;
               case 0x8E: /* ADC A,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80ADC(regs, Z80ReadByte(usAddr));
                  break;
               case 0x96: /* SUB A,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80SUB(regs, Z80ReadByte(usAddr));
                  break;
               case 0x9E: /* SBC A,(IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80SBC(regs, Z80ReadByte(usAddr));
                  break;
               case 0xA6: /* AND (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80AND(regs, Z80ReadByte(usAddr));
                  break;
               case 0xAE: /* XOR (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80XOR(regs, Z80ReadByte(usAddr));
                  break;
               case 0xB6: /* OR (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80OR(regs, Z80ReadByte(usAddr));
                  break;
               case 0xBE: /* CP (IY+d) */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  Z80CMP(regs, Z80ReadByte(usAddr));
                  break;
               case 0xE1: /* POP IY */
                  regs->usRegIY = Z80POPW(regs);
                  break;
               case 0xE3: /* EX (SP),IY */
                  usTemp = Z80ReadWord(regs->usRegSP);
                  Z80WriteWord(regs->usRegSP, regs->usRegIY);
                  regs->usRegIY = usTemp;
                  break;
               case 0xE5: /* PUSH IY */
                  Z80PUSHW(regs, regs->usRegIY);
                  break;
               case 0xE9: /* JP (IY) */
                  PC = regs->usRegIY;
                  break;
               case 0xF9: /* LD SP,IY */
                  regs->usRegSP = regs->usRegIY;
                  break;
               case 0xCB: /* Another set of opcodes branches from here */
                  regs->ucRegR++;
               /* They all use (IY+d), so load the address */
                  usAddr = regs->usRegIY + (signed char)p[2];
                  PC++;
                  ucTemp = Z80ReadByte(usAddr);
//                  ucOpcode = mem_decrypt[PC];
                  ucOpcode = p[3];
#ifdef HISTO
                  ulHisto[0x400 + ucOpcode]++;
#endif // HISTO      
                  PC++;
                  iMyClocks -= cZ80Cycles3[ucOpcode];
                  switch (ucOpcode)
                     {
                     case 0x06: /* RLC (IY+d) */
                        Z80WriteByte(usAddr, Z80RLC(regs, ucTemp));
                        break;
                     case 0x0E: /* RRC (IY+d) */
                        Z80WriteByte(usAddr, Z80RRC(regs, ucTemp));
                        break;
                     case 0x16: /* RL (IY+d) */
                        Z80WriteByte(usAddr, Z80RL(regs, ucTemp));
                        break;
                     case 0x1E: /* RR (IY+d) */
                        Z80WriteByte(usAddr, Z80RR(regs, ucTemp));
                        break;
                     case 0x26: /* SLA (IY+d) */
                        Z80WriteByte(usAddr, Z80SLA(regs, ucTemp));
                        break;
                     case 0x2E: /* SRA (IY+d) */
                        Z80WriteByte(usAddr, Z80SRA(regs, ucTemp));
                        break;
                     case 0x36: /* SLL (IY+d) */
                        Z80WriteByte(usAddr, Z80SLL(regs, ucTemp));
                        break;
                     case 0x3E: /* SRL (IY+d) */
                        Z80WriteByte(usAddr, Z80SRL(regs, ucTemp));
                        break;
                     case 0x46: /* BIT 0,(IY+d) */
                        regs->ucRegF &= ~(F_ADDSUB | F_ZERO);
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 1))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x4E: /* BIT 1,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 2))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x56: /* BIT 2,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 4))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x5E: /* BIT 3,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 8))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x66: /* BIT 4,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 16))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x6E: /* BIT 5,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 32))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x76: /* BIT 6,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 64))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        break;
                     case 0x7E: /* BIT 7,(IY+d) */
                        regs->ucRegF &= F_CARRY; // only carry preserved
                        regs->ucRegF |= F_HALFCARRY;
                        if (!(ucTemp & 128))
                           regs->ucRegF |= F_ZERO + F_OVERFLOW;
                        else
                           regs->ucRegF |= F_SIGN;
                        break;
                     case 0x86: /* RES 0,(IY+d) */
                        ucTemp &= ~1;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x8E: /* RES 1,(IY+d) */
                        ucTemp &= ~2;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x96: /* RES 2,(IY+d) */
                        ucTemp &= ~4;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0x9E: /* RES 3,(IY+d) */
                        ucTemp &= ~8;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xA6: /* RES 4,(IY+d) */
                        ucTemp &= ~16;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xAE: /* RES 5,(IY+d) */
                        ucTemp &= ~32;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xB6: /* RES 6,(IY+d) */
                        ucTemp &= ~64;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xBE: /* RES 7,(IY+d) */
                        ucTemp &= ~128;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xC6: /* SET 0,(IY+d) */
                        ucTemp |= 1;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xCE: /* SET 1,(IY+d) */
                        ucTemp |= 2;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xD6: /* SET 2,(IY+d) */
                        ucTemp |= 4;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xDE: /* SET 3,(IY+d) */
                        ucTemp |= 8;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xE6: /* SET 4,(IY+d) */
                        ucTemp |= 16;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xEE: /* SET 5,(IY+d) */
                        ucTemp |= 32;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xF6: /* SET 6,(IY+d) */
                        ucTemp |= 64;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     case 0xFE: /* SET 7,(IY+d) */
                        ucTemp |= 128;
                        Z80WriteByte(usAddr, ucTemp);
                        break;
                     default: /* ILLEGAL FDCBXX opcode*/
                        iMyClocks = 0;
                        break;
                     } /* switch on FDCB */
                  break;
               default: /* ILLEGAL FDXX opcode */
                  iMyClocks = 0;
                  break;
               } /* switch on FD */
            break;
         case 0xFE: /* CP n */
            PC++;
            Z80CMP(regs, p[1]);
            break;
         default: // Illegal
            iMyClocks = 0;
            break;
         }   /* switch */
      } /* while */
   if (iEITicks) // just executed an EI?
      {
      iMyClocks += iEITicks;
      iEITicks = 0;
      goto top_emulation_loop;
      }
   regs->usRegPC = (unsigned short)PC;
   *iClocks = iMyClocks;

} /* EXECZ80() */
