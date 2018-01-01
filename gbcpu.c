/*******************************************************************/
/* Z80/GB CPU emulator written by Larry Bank                       */
/* Copyright 1998-2017 BitBank Software, Inc.                      */
/*                                                                 */
/* This code was written from scratch using the Z80 data from      */
/* the Zilog databook "Components".                                */
/*                                                                 */
/* Change history:                                                 */
/* 2/8/98 Wrote it - Larry B.                                      */
/* 5/1/02 added gameboy specific stuff - Larry B.                  */
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
#include <string.h>
#include "emuio.h"
#include "smartgear.h"
#include "emu.h"
#include "gbc_emu.h"

#define F_CARRY     16
#define F_HALFCARRY 32
#define F_ADDSUB    64
#define F_ZERO      128

//#define SPEED_HACKS

extern GBMACHINE *pGBMachine;
/* Some statics */
//extern EMUHANDLERS *emuh;
extern BOOL bTrace;
extern int iTrace;
extern int iScanLine;
extern int ohandle;
//static uint32_t *pHisto;
void GBIOWrite(int iAddr, unsigned char ucByte);
unsigned char GBIORead(int iAddr);
void TRACEGB(int);

unsigned char cNibbleSwap[256] =
 {0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,0xa0,0xb0,0xc0,0xd0,0xe0,0xf0,
  0x01,0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81,0x91,0xa1,0xb1,0xc1,0xd1,0xe1,0xf1,                              
  0x02,0x12,0x22,0x32,0x42,0x52,0x62,0x72,0x82,0x92,0xa2,0xb2,0xc2,0xd2,0xe2,0xf2,                              
  0x03,0x13,0x23,0x33,0x43,0x53,0x63,0x73,0x83,0x93,0xa3,0xb3,0xc3,0xd3,0xe3,0xf3,                              
  0x04,0x14,0x24,0x34,0x44,0x54,0x64,0x74,0x84,0x94,0xa4,0xb4,0xc4,0xd4,0xe4,0xf4,                              
  0x05,0x15,0x25,0x35,0x45,0x55,0x65,0x75,0x85,0x95,0xa5,0xb5,0xc5,0xd5,0xe5,0xf5,                              
  0x06,0x16,0x26,0x36,0x46,0x56,0x66,0x76,0x86,0x96,0xa6,0xb6,0xc6,0xd6,0xe6,0xf6,                              
  0x07,0x17,0x27,0x37,0x47,0x57,0x67,0x77,0x87,0x97,0xa7,0xb7,0xc7,0xd7,0xe7,0xf7,                              
  0x08,0x18,0x28,0x38,0x48,0x58,0x68,0x78,0x88,0x98,0xa8,0xb8,0xc8,0xd8,0xe8,0xf8,                              
  0x09,0x19,0x29,0x39,0x49,0x59,0x69,0x79,0x89,0x99,0xa9,0xb9,0xc9,0xd9,0xe9,0xf9,                              
  0x0a,0x1a,0x2a,0x3a,0x4a,0x5a,0x6a,0x7a,0x8a,0x9a,0xaa,0xba,0xca,0xda,0xea,0xfa,                              
  0x0b,0x1b,0x2b,0x3b,0x4b,0x5b,0x6b,0x7b,0x8b,0x9b,0xab,0xbb,0xcb,0xdb,0xeb,0xfb,                              
  0x0c,0x1c,0x2c,0x3c,0x4c,0x5c,0x6c,0x7c,0x8c,0x9c,0xac,0xbc,0xcc,0xdc,0xec,0xfc,                              
  0x0d,0x1d,0x2d,0x3d,0x4d,0x5d,0x6d,0x7d,0x8d,0x9d,0xad,0xbd,0xcd,0xdd,0xed,0xfd,                              
  0x0e,0x1e,0x2e,0x3e,0x4e,0x5e,0x6e,0x7e,0x8e,0x9e,0xae,0xbe,0xce,0xde,0xee,0xfe,                              
  0x0f,0x1f,0x2f,0x3f,0x4f,0x5f,0x6f,0x7f,0x8f,0x9f,0xaf,0xbf,0xcf,0xdf,0xef,0xff};

/* flags for Increment */
static unsigned char cZ80INC[257] = {
          F_ZERO + F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 00-0F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 80-8F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 90-9F */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* A0-AF */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* B0-BF */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* C0-CF */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* D0-DF */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* E0-EF */
          F_HALFCARRY,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* F0-FF */
          F_ZERO + F_HALFCARRY};                              /* 100==00 */
/* flags for Decrement */
static unsigned char cZ80DEC[256] = {
            F_ADDSUB + F_ZERO,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,     /* 00-0F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 10-1F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 20-2F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 30-3F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 40-4F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 50-5F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 60-6F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 70-7F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 80-8F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* 90-9F */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* A0-Af */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* B0-BF */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* C0-CF */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* D0-DF */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY,          /* E0-EF */
          F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB,F_ADDSUB + F_HALFCARRY};         /* F0-FF */

/* Instruction t-states by opcode for 1 byte opcodes */
static unsigned char cZ80Cycles[256] = {4,12,8,8,4,4,8,4, 20,8,8,8,4,4,8,4,      /* 00-0F */
                                 4,12,8,8,4,4,8,4, 12,8,8,8,4,4,8,4,      /* 10-1F */
                                 8,12,8,8,4,4,8,4, 8,8,8,8,4,4,8,4,      /* 20-2F */
                                 8,12,8,8,12,12,12,4, 8,8,8,8,4,4,8,4,   /* 30-3F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* 40-4F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* 50-5F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* 60-6F */
                                 8,8,8,8,8,8,4,8, 4,4,4,4,4,4,8,4,        /* 70-7F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* 80-8F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* 90-9F */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* A0-AF */
                                 4,4,4,4,4,4,8,4, 4,4,4,4,4,4,8,4,        /* B0-BF */
                                 8,12,12,16,12,16,8,16, 8,16,12,0,12,24,8,16, /* C0-CF */
                                 8,12,12,16,12,16,8,16, 8,16,12,16,12,12,8,16, /* D0-DF */
                                 12,12,8,12,12,16,8,16, 16,4,16,12,12,12,8,16,     /* E0-EF */
                                 12,12,8,4,12,16,8,16, 12,8,16,4,12,12,8,16};       /* F0-FF */


/* Instruction t-states for the CBXX opcodes */
unsigned char cZ80CBCycles[256] = {8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 00-0F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 10-1F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 20-2F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 30-3F */
                                   8,8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,      /* 40-4F */
                                   8,8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,      /* 50-5F */
                                   8,8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,      /* 60-6F */
                                   8,8,8,8,8,8,12,8,8,8,8,8,8,8,12,8,      /* 70-7F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 80-8F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* 90-9F */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* A0-AF */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* B0-BF */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* C0-CF */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* D0-DF */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8,      /* E0-EF */
                                   8,8,8,8,8,8,16,8,8,8,8,8,8,8,16,8};     /* F0-FF */

/* Sign and zero flags for quicker flag settings */
unsigned char cZ80Z[512]={
      F_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 00-0F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 80-8F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 90-9F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* A0-AF */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* B0-BF */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* C0-CF */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* D0-DF */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* E0-EF */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* F0-FF */
// When doing an ADD, SUB, or CMP we can catch the carry flag here
      F_ZERO | F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 100-107 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 108-10F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 110-117 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 118-11F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 120-127 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 128-12F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 130-137 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 138-13F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 140-147 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 148-14F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 150-157 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 158-15F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 160-167 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 168-16F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 170-177 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 178-17F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 180-187 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 188-18F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 190-197 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 198-19F */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1A0-1A7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1A8-1AF */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1B0-1B7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1B8-1BF */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1C0-1C7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1C8-1CF */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1D0-1D7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1D8-1DF */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1E0-1E7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1E8-1EF */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY, /* 1F0-1F7 */
      F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY,F_CARRY};/* 1F8-1FF */

#define Z80ADD(ucByte) \
   us = ucRegA + ucByte; \
   ucRegF &= ~(F_CARRY | F_ADDSUB | F_ZERO | F_HALFCARRY); \
   ucRegF |= cZ80Z[us] | (F_HALFCARRY & ((ucRegA ^ ucByte ^ us) << 1)); \
   ucRegA = (unsigned char)us;

#define Z80ADC(ucByte) \
   us = ucRegA + ucByte + ((ucRegF & F_CARRY) >> 4); \
   ucRegF &= ~(F_CARRY | F_ADDSUB | F_ZERO | F_HALFCARRY); \
   ucRegF |= cZ80Z[us] | (F_HALFCARRY & ((ucRegA ^ ucByte ^ us) << 1)); \
   ucRegA = (unsigned char)us;

#define Z80SUB(ucByte) \
   us = ucRegA - ucByte; \
   ucRegF &= ~(F_CARRY | F_ZERO | F_HALFCARRY); \
   ucRegF |= F_ADDSUB | cZ80Z[us & 0x1ff] | (F_HALFCARRY & ((ucRegA ^ ucByte ^ us) << 1)); \
   ucRegA = (unsigned char)us;

#define Z80SBC(ucByte) \
   us = ucRegA - ucByte - ((ucRegF & F_CARRY) >> 4); \
   ucRegF &= ~(F_CARRY | F_ZERO | F_HALFCARRY); \
   ucRegF |= F_ADDSUB | cZ80Z[us & 0x1ff] | (F_HALFCARRY & ((ucRegA ^ ucByte ^ us) << 1)); \
   ucRegA = (unsigned char)us;

#define Z80AND(ucByte) \
   ucRegA &= ucByte; \
   ucRegF = F_HALFCARRY | cZ80Z[ucRegA];

#define Z80XOR(ucByte) \
   ucRegA ^= ucByte; \
   ucRegF = cZ80Z[ucRegA]; \

#define Z80OR(ucByte) \
   ucRegA |= ucByte; \
   ucRegF = cZ80Z[ucRegA];

#define Z80CMP(ucByte) \
   us = ucRegA - ucByte; \
   ucRegF &= ~(F_CARRY | F_ZERO | F_HALFCARRY); \
   ucRegF |= F_ADDSUB | cZ80Z[us & 0x1ff] | (F_HALFCARRY & ((ucRegA ^ ucByte ^ us) << 1)); \

#define Z80INC(ucByte) \
   ucRegF &= F_CARRY; \
   ucByte++; \
   ucRegF |= cZ80INC[ucByte];

#define Z80DEC(ucByte) \
   ucRegF &= F_CARRY; \
   ucByte--; \
   ucRegF |= cZ80DEC[ucByte];

#define Z80ADDHL(usWord) \
   ul = usRegHL + usWord; \
   ucRegF &= F_ZERO; \
   ucRegF |= ((ul & 0x10000) >> 12) | (((usRegHL ^ usWord ^ ul) & 0x1000) >> 7); \
   usRegHL = ul & 0xffff;

#define Z80RL(ucByte) \
   uc1 = (ucRegF & F_CARRY) >> 4; \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= ((ucByte & 0x80) >> 3); \
   ucByte = (ucByte <<1) | uc1; \
   ucRegF |= cZ80Z[ucByte];

#define Z80SLA(ucByte) \
   uc1 = ucByte & 0x80; \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (uc1 >> 3); \
   ucByte = ucByte <<1; \
   ucRegF |= cZ80Z[ucByte];

#define Z80SRA(ucByte) \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= ((ucByte & 1) << 4); \
   ucByte = (ucByte >>1) | (ucByte & 0x80); \
   ucRegF |= cZ80Z[ucByte];

#define Z80SRL(ucByte) \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= ((ucByte & 1) << 4); \
   ucByte = ucByte >>1; \
   ucRegF |= cZ80Z[ucByte];

#define Z80RR(ucByte) \
   uc1 = ((ucRegF & F_CARRY) << 3); \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (ucByte & 1) << 4; \
   ucByte = (ucByte >>1) | uc1; \
   ucRegF |= cZ80Z[ucByte];

#define Z80RRA \
   uc1 = ((ucRegF & F_CARRY)<<3); \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (ucRegA & 1) << 4; \
   ucRegA = (ucRegA >>1) | uc1;

#define Z80RLA \
   uc1 = ((ucRegF & F_CARRY)>>4); \
   ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY | F_ZERO); \
   ucRegF |= ((ucRegA & 0x80) >> 3); \
   ucRegA = ucRegA <<1 | uc1;

#define Z80RLC(ucByte) \
   uc1 = ucByte & 0x80; \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (uc1 >> 3); \
   ucByte = (ucByte <<1) | (uc1 >> 7); \
   ucRegF |= cZ80Z[ucByte];

#define Z80RLCA \
   uc1 = ucRegA & 0x80; \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (uc1 >> 3); \
   ucRegA = (ucRegA <<1) | (uc1 >> 7);

#define Z80RRC(ucByte) \
   uc1 = ucByte & 1; \
   ucRegF &= ~(F_ZERO | F_CARRY | F_ADDSUB | F_HALFCARRY); \
   ucRegF |= (uc1 << 4); \
   ucByte = (ucByte >>1) | (uc1 << 7); \
   ucRegF |= cZ80Z[ucByte];

#define Z80RRCA \
   uc1 = ucRegA & 1; \
   ucRegF &= ~(F_CARRY | F_ADDSUB | F_HALFCARRY | F_ZERO); \
   ucRegF |= (uc1 << 4); \
   ucRegA = (ucRegA >>1) | (uc1 << 7);

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80WriteByte(short, char)                                  *
 *                                                                          *
 *  PURPOSE    : Write a byte to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
#ifndef SLOW
static  void Z80WriteByte(unsigned int iAddr, unsigned char ucByte)
{
register unsigned int c;


//   c = iAddr >> 12;
//   if (iAddr < 0x8000 || iAddr >= 0xff00) // just write it
//      (emuh[c].pfn_write)(iAddr, ucByte);
//   else
//      *(unsigned char *)(iAddr + ulOpList[c]) = ucByte;



   c = iAddr >> 12;
   if (pGBMachine->ulRWFlags & (0x10000<<c)) // there is a write routine
      {
      (pGBMachine->pEMUH[c].pfn_write)(iAddr, ucByte);
      }
   else // normal RAM, just write it
      *(unsigned char *)((unsigned long)iAddr + pGBMachine->ulOpList[c]) = ucByte;
      
} /* Z80WriteByte() */
#else
#define Z80WriteByte(iAddr, ucByte) (emuh[iAddr >> 12].pfn_write)(iAddr, ucByte)
#endif
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80WriteWord(short, short)                                 *
 *                                                                          *
 *  PURPOSE    : Write a word to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
static void Z80WriteWord(unsigned int iAddr, uint32_t usWord)
{
register unsigned int c;

   c = iAddr >> 12;
   if (pGBMachine->ulRWFlags & (0x10000<<c)) // there is a write routine
      {
      (pGBMachine->pEMUH[c].pfn_write)(iAddr, (char)usWord);
      (pGBMachine->pEMUH[c].pfn_write)(iAddr+1, (char)(usWord>>8));
      }
   else // normal RAM
      {
      unsigned long ulAddr = (unsigned long)iAddr + pGBMachine->ulOpList[c];
      *(unsigned char *)(ulAddr) = (char)usWord;
      *(unsigned char *)(ulAddr+1) = (char)(usWord>>8);
      }
//   Z80WriteByte(iAddr, (unsigned char)usWord);
//   iAddr++;
//   Z80WriteByte(iAddr, (unsigned char)(usWord >> 8));

} /* Z80WriteWord() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ReadByte(short)                                         *
 *                                                                          *
 *  PURPOSE    : Read a byte from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
#ifndef SLOW
static  unsigned char Z80ReadByte(unsigned int iAddr)
{
register unsigned int c;

   c = iAddr >> 12;
   if (pGBMachine->ulRWFlags & (1<<c)) // there is a read routine
      {
//   if (emuh[c].pfn_read) // if there is a read routine defined, use it
      return (pGBMachine->pEMUH[c].pfn_read)(iAddr);
      }
   else
      return *(unsigned char *)((unsigned long)iAddr + pGBMachine->ulOpList[c]); // otherwise read the memory directly

} /* Z80ReadByte() */
#else
#define Z80ReadByte(iAddr) (emuh[iAddr >> 12].pfn_read)(iAddr)
#endif
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80ReadWord(short)                                         *
 *                                                                          *
 *  PURPOSE    : Read a word from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
static  uint32_t Z80ReadWord(unsigned int iAddr)
{
register unsigned int c;

   c = iAddr >> 12;
   if (pGBMachine->ulRWFlags & (1<<c)) // there is a read routine
      {
      return (pGBMachine->pEMUH[c].pfn_read)(iAddr) + ((pGBMachine->pEMUH[c].pfn_read)(iAddr+1)<<8);
      }
   else
      {
      unsigned long ulAddr = (unsigned long)iAddr + pGBMachine->ulOpList[c];
      return *(unsigned char *)ulAddr + (*(unsigned char *)(ulAddr+1) << 8);
      }

} /* Z80ReadWord() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80PUSHW(REGSGB *)                                        *
 *                                                                          *
 *  PURPOSE    : Push a word to the 'SP' stack.                             *
 *                                                                          *
 ****************************************************************************/
static void Z80PUSHW(uint32_t usWord)
{
   pGBMachine->regs.usRegSP -= 2;
   Z80WriteWord(pGBMachine->regs.usRegSP, usWord);

} /* Z80PUSHW() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Z80POPW(REGSGB *)                                         *
 *                                                                          *
 *  PURPOSE    : Pull a word from the 'SP' stack.                           *
 *                                                                          *
 ****************************************************************************/
static uint32_t Z80POPW(void)
{
uint32_t ulWord;

   ulWord = Z80ReadWord(pGBMachine->regs.usRegSP);
   pGBMachine->regs.usRegSP+=2;
   return ulWord;
//   return Z80ReadByte(pGBMachine->regs.usRegSP-2) + (Z80ReadByte(pGBMachine->regs.usRegSP-1) << 8);

} /* Z80POPW() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : RESETGB(void)                                              *
 *                                                                          *
 *  PURPOSE    : Setup the Gameboy CPU after a reset.                       *
 *                                                                          *
 ****************************************************************************/
void APIENTRY RESETGB(REGSGB *pRegs)
{
   memset(pRegs, 0, sizeof(REGSGB)); /* Start with a clean slate at reset */
   pRegs->usRegAF = 0x1120; //0x11b0;
   pRegs->usRegBC = 0x0013;
   pRegs->usRegDE = 0x00d8;
   pRegs->usRegHL = 0x014d;
   pRegs->usRegPC = 0x100;
   pRegs->usRegSP = 0xfffe;
   pRegs->ucRegI  = 0; // disable interrupts
   pRegs->ucRegHalt = FALSE;
//   pHisto = (uint32_t *)&pGBMachine->mem_map[0x20000];
   iTrace = 0;

} /* RESETGB() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBCheckInterrupts()                                        *
 *                                                                          *
 *  PURPOSE    : Try to handle any pending interrupts.                      *
 *                                                                          *
 ****************************************************************************/
 int GBCheckInterrupts(int PC)
{
   if (!((pGBMachine->regs.ucRegI || pGBMachine->regs.ucRegHalt) && (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable))) // no interrupts pending?
      return PC; // nothing to do

   if (pGBMachine->regs.ucRegHalt) // if the CPU was Halted, unstick it
      {
      pGBMachine->regs.ucRegHalt = FALSE;
      PC++; // skip over the halt instruction
      }

   Z80PUSHW(PC);
   pGBMachine->regs.ucRegI = 0; // disable interrupts

   if (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable & GB_INT_VBLANK)
      {
      pGBMachine->ucIRQFlags &= ~GB_INT_VBLANK;
      PC = 0x40;
      }
   else
      if (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable & GB_INT_LCDSTAT)
         {
         pGBMachine->ucIRQFlags &= ~GB_INT_LCDSTAT;
         PC = 0x48;
         }
   else
      if (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable & GB_INT_TIMER)
         {
         pGBMachine->ucIRQFlags &= ~GB_INT_TIMER;
         PC = 0x50;
         }
   else
      if (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable & GB_INT_SERIAL)
         {
         pGBMachine->ucIRQFlags &= ~GB_INT_SERIAL;
         PC = 0x58;
         }
   else
      if (pGBMachine->ucIRQFlags & pGBMachine->ucIRQEnable & GB_INT_BUTTONS)
         {
         pGBMachine->ucIRQFlags &= ~GB_INT_BUTTONS;
         PC = 0x60;
         }

   return PC;

} /* GBCheckInterrupts() */

/******************************************************************************
 *                                                                            *
 *  FUNCTION   : EXECGB(int *, char *, char, int)                             *
 *                                                                            *
 *  PURPOSE    : Emulate the Gameboy (Z80) microprocessor for N clock cycles. *
 *                                                                            *
 ******************************************************************************/
void APIENTRY EXECGB(REGSGB *pUnused, int iCycles)
{
unsigned int usAddr; /* Temp address */
unsigned char uc, uc1;
uint32_t us;
uint32_t ul;
unsigned char *pSegment;
unsigned int iOpcode;
register unsigned int PC;
register int iLocalTicks;
register unsigned char *pPC;
 int usRegHL, iEITicks;
 unsigned char ucRegA, ucRegF; // speed up things
//unsigned short oldPC;

   iLocalTicks = (iCycles * pGBMachine->iCPUSpeed);
   pGBMachine->iMasterClock += iLocalTicks;
   PC = pGBMachine->regs.usRegPC;
   usRegHL = pGBMachine->regs.usRegHL;
   ucRegA = pGBMachine->regs.ucRegA;
   ucRegF = pGBMachine->regs.ucRegF;
   iEITicks = 0; // for proper EI handling

      if (pGBMachine->bTimerOn) // if timer enabled
         {
         while (pGBMachine->iMasterClock > pGBMachine->iTimerLimit) // see if timer tick will increment this time through
            {
            pGBMachine->iTimerLimit += pGBMachine->iTimerMask;
            pGBMachine->ucTimerCounter++;
            if (!pGBMachine->ucTimerCounter) // counter overflow, do interrupt
               {
               pGBMachine->ucTimerCounter = pGBMachine->ucTimerModulo;
               pGBMachine->ucIRQFlags |= GB_INT_TIMER;
//               PC = GBCheckInterrupts(PC);
               }
            if (pGBMachine->iTimerMask == 0) // we would get stuck here if true
               break;
            }
         }
top_of_execution:
   PC = GBCheckInterrupts(PC);
   pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
   pPC = &pSegment[PC];
   while (iLocalTicks > 0) /* Execute for the amount of time alloted */
      {
#ifdef _DEBUG

   if (bTrace)
      {
      pGBMachine->regs.ucRegA = ucRegA;
      pGBMachine->regs.ucRegF = ucRegF;
      pGBMachine->regs.usRegHL = usRegHL;
      pGBMachine->regs.usRegPC = pPC - pSegment;
      TRACEGB(iLocalTicks);
      iTrace++;
//      if (iTrace == 0x7c8b)
//         c[15] = 0;
//      oldPC = PC;
      }
//      PC = pPC - pSegment;
//      if (PC == 0x42a)
//         iOpcode = 0;

#endif
//      pOps = (unsigned char *)(PC + pGBMachine->ulOpList[PC >> 12]); // offset to memory containing current opcode and parameters
      iOpcode = *pPC++;
      iLocalTicks -= cZ80Cycles[iOpcode];
//      pHisto[iOpcode]++;
      switch (iOpcode)
         {
         case 0x00: /* NOP */
         case 0x40: /* LD B,B */
         case 0x49: /* LD C,C */
         case 0x52: /* LD D,D */
         case 0x5B: /* LD E,E */
         case 0x64: /* LD H,H */
         case 0x6D: /* LD L,L */
         case 0x7F: /* LD A,A */
            break;
         // Invalid Opcodes
         case 0xD3: /* NOP */
         case 0xDB: /* NOP */
         case 0xDD: // NOP
         case 0xE4: /* NOP */
         case 0xEB: /* NOP */
         case 0xEC: /* NOP */
         case 0xED: /* NOP */
         case 0xF4: /* NOP */
         case 0xFC: /* NOP */
         case 0xFD: /* NOP */
            pPC += 0;
            break;

         case 0x01: /* LD BC,nn */
            pGBMachine->regs.usRegBC = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            break;
         case 0x11: /* LD DE,nn */
            pGBMachine->regs.usRegDE = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            break;
         case 0x21: /* LD HL,nn */
            usRegHL = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            break;
         case 0x31: /* LD SP,nn */
            pGBMachine->regs.usRegSP = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            break;

         case 0x02: /* LD (BC),A */
            Z80WriteByte(pGBMachine->regs.usRegBC, ucRegA);
            break;

         case 0x03: /* INC BC */
            pGBMachine->regs.usRegBC++;
            pGBMachine->regs.usRegBC &= 0xffff;
            break;
         case 0x13: /* INC DE */
            pGBMachine->regs.usRegDE++;
            pGBMachine->regs.usRegDE &= 0xffff;
            break;
         case 0x23: /* INC HL */
            usRegHL++;
            usRegHL &= 0xffff;
            break;
         case 0x33: /* INC SP */
            pGBMachine->regs.usRegSP++;
            pGBMachine->regs.usRegSP &= 0xffff;
            break;

         case 0x04: /* INC B */
            Z80INC(pGBMachine->regs.ucRegB);
            break;
         case 0x0C: /* INC C */
            Z80INC(pGBMachine->regs.ucRegC);
            break;
         case 0x14: /* INC D */
            Z80INC(pGBMachine->regs.ucRegD);
            break;
         case 0x1C: /* INC E */
            Z80INC(pGBMachine->regs.ucRegE);
            break;
         case 0x24: /* INC H */
         // kludge - clean up later
            pGBMachine->regs.ucRegH = (char)(usRegHL>>8);
            Z80INC(pGBMachine->regs.ucRegH);
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegH << 8);
            break;
         case 0x2C: /* INC L */
         // kludge - clean up later
            pGBMachine->regs.ucRegL = (char)usRegHL;
            Z80INC(pGBMachine->regs.ucRegL);
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegL;
            break;
         case 0x3C: /* INC A */
            Z80INC(ucRegA);
            break;

         case 0x05: /* DEC B */
            Z80DEC(pGBMachine->regs.ucRegB);
            break;
         case 0x0D: /* DEC C */
            Z80DEC(pGBMachine->regs.ucRegC);
            break;
         case 0x15: /* DEC D */
            Z80DEC(pGBMachine->regs.ucRegD);
            break;
         case 0x1D: /* DEC E */
            Z80DEC(pGBMachine->regs.ucRegE);
            break;
         case 0x25: /* DEC H */
           // kludge - clean up later
            pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
            Z80DEC(pGBMachine->regs.ucRegH);
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegH << 8);
            break;
         case 0x2D: /* DEC L */
         // kludge - clean up later
            pGBMachine->regs.ucRegL = (char)usRegHL;
            Z80DEC(pGBMachine->regs.ucRegL);
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegL;
            break;
         case 0x3D: /* DEC A */
            Z80DEC(ucRegA);
            break;

         case 0x06: /* LD B,n */
            pGBMachine->regs.ucRegB = *pPC++;
            break;
         case 0x0E: /* LD C,n */
            pGBMachine->regs.ucRegC = *pPC++;
            break;
         case 0x16: /* LD D,n */
            pGBMachine->regs.ucRegD = *pPC++;
            break;
         case 0x1E: /* LD E,n */
            pGBMachine->regs.ucRegE = *pPC++;
            break;
         case 0x26: /* LD H,n */
            usRegHL &= 0xff;
            usRegHL |= (*pPC++ << 8);
            break;
         case 0x2E: /* LD L,n */
            usRegHL &= 0xff00;
            usRegHL |= *pPC++;
            break;
         case 0x3E: /* LD A,n */
            ucRegA = *pPC++;
            break;

         case 0x07: /* RLCA */
            Z80RLCA
            break;

         case 0x08: /* LD (WORD),SP */
            usAddr = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            Z80WriteWord((unsigned short)usAddr, pGBMachine->regs.usRegSP);
            break;

         case 0x09: /* ADD HL,BC */
            Z80ADDHL(pGBMachine->regs.usRegBC);
            break;
         case 0x19: /* ADD HL,DE */
            Z80ADDHL(pGBMachine->regs.usRegDE);
            break;
         case 0x29: /* ADD HL,HL */
            Z80ADDHL((unsigned short)usRegHL);
            break;
         case 0x39: /* ADD HL,SP */
            Z80ADDHL(pGBMachine->regs.usRegSP);
            break;
         case 0x0A: /* LD A,(BC) */
            ucRegA = Z80ReadByte(pGBMachine->regs.usRegBC);
            break;

         case 0x0B: /* DEC BC */
            pGBMachine->regs.usRegBC--;
            pGBMachine->regs.usRegBC &= 0xffff;
            break;
         case 0x1B: /* DEC DE */
            pGBMachine->regs.usRegDE--;
            pGBMachine->regs.usRegDE &= 0xffff;
            break;
         case 0x2B: /* DEC HL */
            usRegHL--;
            usRegHL &= 0xffff;
            break;
         case 0x3B: /* DEC SP */
            pGBMachine->regs.usRegSP--;
            pGBMachine->regs.usRegSP &= 0xffff;
            break;

         case 0x0F: /* RRCA */
            Z80RRCA
            break;

         case 0x10: /* STOP */
            if (pGBMachine->bSpeedChange)  // toggle CPU speed
               {
               pGBMachine->bSpeedChange = FALSE;
               if (pGBMachine->iCPUSpeed == 1) // speeding up
                  {
                  pGBMachine->iCPUSpeed = 2;
                  iLocalTicks <<= 1; // double the remaining time
                  }
               else
                  {
                  pGBMachine->iCPUSpeed = 1;
                  iLocalTicks >>= 1; // divide the remaining time by 2
                  }
               pPC++; // the other byte does what?
               }
            else
               {
               pPC--;           /* stick on this instruction until an interrupt */
               pGBMachine->regs.ucRegHalt = TRUE; // in a halted state
               iLocalTicks = 0;
               }
            break;

         case 0x12: /* LD (DE),A */
            Z80WriteByte(pGBMachine->regs.usRegDE, ucRegA);
            break;
         case 0x17: /* RLA */
            Z80RLA
            break;
         case 0x18: /* JR e */
#ifdef SPEED_HACKS
            if (pPC[0] >= 0xfd)
               iLocalTicks = 0;
#endif
            pPC = (pPC + 1 + (signed char)pPC[0]);
            if (pPC < pSegment)
               {
               PC = (unsigned long)pPC & 0xffff;
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               }
//            PC &= 0xffff;
            break;
         case 0x1A: /* LD A,(DE) */
            ucRegA = Z80ReadByte(pGBMachine->regs.usRegDE);
            break;
         case 0x1F: /* RRA */
            Z80RRA
            break;
         case 0x20: /* JR NZ,e */
            if (!(ucRegF & F_ZERO))
               {
#ifdef SPEED_HACKS
               if (pPC[0] == 0xfd) // probably a time wasting loop
                  {
                  if (pPC[-2] == 0xbe) // if the looping instruction is CMP (HL), it won't change in this time slice, so just leave
                     {
                     pPC -= 2; // take the branch since we will need to retest it the next time through
                     iLocalTicks = 0;
                     }
                  else
                     {
                     if (pPC[-2] == 0x3d) // another time wasting loop (DEC A) usually after doing an OAM DMA
                        {
                        uc = iLocalTicks / 16; // number of times the loop can execute this time slice
                        if (uc < ucRegA) // we cannot burn through all of the loop this time slice
                           {
                           ucRegA -= uc;
                           iLocalTicks = 0;
                           pPC -= 2;
                           }
                        else // we can be done with this loop
                           {
                           iLocalTicks -= 16*ucRegA;
                           ucRegA = 0;
                           pPC++;
                           }
                        }
                     else
                        {
                        PC |= 0;
                        }
                     }
                  }
               else // normal branch
#endif // SPEED_HACKS
                  {
                  iLocalTicks -= 4;
                  pPC = (pPC + 1 + (signed char)pPC[0]);
//                  PC &= 0xffff;
                  }
               }
            else
               pPC++;
            break;

         case 0x22: /* LD (HL++),A */
            Z80WriteByte(usRegHL, ucRegA);
            usRegHL++;
            usRegHL &= 0xffff;
            break;

         case 0x27: /* DAA */
            {
            unsigned char lsn, nc;
//            unsigned char msn;
            unsigned short cf;
            nc = 0; /* New carry flag */
            cf = ucRegA;
//            msn = ucRegA & 0xf0;
            lsn = ucRegA & 0x0f;
            if (ucRegF & F_ADDSUB) /* Different rules for addition/subtraction */
               { /* really DAS */
               if (lsn > 9 || ucRegF & F_HALFCARRY)
                  cf -= 6;
               if (cf > 0x9f || ucRegF & F_CARRY)
                  {
                  cf -= 0x60;
                  nc = F_CARRY;
                  }
               }
            else /* After addition */
               {
               if (lsn > 9 || ucRegF & F_HALFCARRY)
                  cf += 6;
               if (cf > 0x9f || ucRegF & F_CARRY)
                  {
                  cf += 0x60;
                  nc = F_CARRY;
                  }
               }
            ucRegF &= ~(F_HALFCARRY | F_CARRY | F_ZERO);
            ucRegA = (unsigned char)cf;
            ucRegF += nc;
            ucRegF |= cZ80Z[ucRegA];
            }
            break;

         case 0x28: /* JR Z,e */
            if (ucRegF & F_ZERO)
               {
               iLocalTicks -= 4;
//               PC &= 0xffff;
#ifdef SPEED_HACKS
               if (pPC[0] == 0xfd && pPC[-2] == 0xbe) // CMP (HL) will not change in this time slice
                  iLocalTicks = 0;
#endif
               pPC = (pPC + 1 + (signed char)pPC[0]);
               }
            else
               pPC++;
            break;
         case 0x2A: /* LD A,(HL++) */
            ucRegA = Z80ReadByte(usRegHL);
            usRegHL++;
            usRegHL &= 0xffff;
            break;

         case 0x2F: /* CPL */
            ucRegA = 255 - ucRegA;
            ucRegF |= (F_HALFCARRY | F_ADDSUB);
            break;
         case 0x30: /* JR NC,e */
            if (!(ucRegF & F_CARRY))
               {
               iLocalTicks -= 4;
               pPC = (pPC + 1 + (signed char)pPC[0]);
//               PC &= 0xffff;
               }
            else
               pPC++;
            break;

         case 0x32: /* LD (HL--),A */
            Z80WriteByte(usRegHL, ucRegA);
            usRegHL--;
            usRegHL &= 0xffff;
            break;

         case 0x34: /* INC (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80INC(uc);
            Z80WriteByte(usRegHL, uc);
            break;

         case 0x35: /* DEC (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80DEC(uc);
            Z80WriteByte(usRegHL, uc);
            break;

         case 0x36: /* LD (HL),n */
            Z80WriteByte(usRegHL, *pPC++);
            break;

         case 0x37: /* SCF */
            ucRegF |= F_CARRY;
            ucRegF &= ~(F_HALFCARRY | F_ADDSUB);
            break;

         case 0x38: /* JR C,e */
            if (ucRegF & F_CARRY)
               {
               iLocalTicks -= 4;
               pPC = (pPC + 1 + (signed char)pPC[0]);
//               PC &= 0xffff;
               }
            else
               pPC++;
            break;

         case 0x3A: /* LD A,(HL--) */
            ucRegA = Z80ReadByte(usRegHL);
            usRegHL--;
            usRegHL &= 0xffff;
            break;

         case 0x3F: /* CCF */
            ucRegF ^= F_CARRY;
            ucRegF &= ~(F_ADDSUB | F_HALFCARRY);
            break;
         case 0x41: /* LD B,C */
            pGBMachine->regs.ucRegB = pGBMachine->regs.ucRegC;
            break;
         case 0x42: /* LD B,D */
            pGBMachine->regs.ucRegB = pGBMachine->regs.ucRegD;
            break;
         case 0x43: /* LD B,E */
            pGBMachine->regs.ucRegB = pGBMachine->regs.ucRegE;
            break;
         case 0x44: /* LD B,H */
            pGBMachine->regs.ucRegB = (char)(usRegHL >> 8);
            break;
         case 0x45: /* LD B,L */
            pGBMachine->regs.ucRegB = (char)usRegHL;
            break;
         case 0x47: /* LD B,A */
            pGBMachine->regs.ucRegB = ucRegA;
            break;
         case 0x48: /* LD C,B */
            pGBMachine->regs.ucRegC = pGBMachine->regs.ucRegB;
            break;
         case 0x4A: /* LD C,D */
            pGBMachine->regs.ucRegC = pGBMachine->regs.ucRegD;
            break;
         case 0x4B: /* LD C,E */
            pGBMachine->regs.ucRegC = pGBMachine->regs.ucRegE;
            break;
         case 0x4C: /* LD C,H */
            pGBMachine->regs.ucRegC = (char)(usRegHL >> 8);
            break;
         case 0x4D: /* LD C,L */
            pGBMachine->regs.ucRegC = (char)usRegHL;
            break;
         case 0x4F: /* LD C,A */
            pGBMachine->regs.ucRegC = ucRegA;
            break;
         case 0x50: /* LD D,B */
            pGBMachine->regs.ucRegD = pGBMachine->regs.ucRegB;
            break;
         case 0x51: /* LD D,C */
            pGBMachine->regs.ucRegD = pGBMachine->regs.ucRegC;
            break;
         case 0x53: /* LD D,E */
            pGBMachine->regs.ucRegD = pGBMachine->regs.ucRegE;
            break;
         case 0x54: /* LD D,H */
            pGBMachine->regs.ucRegD = (char)(usRegHL >> 8);
            break;
         case 0x55: /* LD D,L */
            pGBMachine->regs.ucRegD = (char)usRegHL;
            break;
         case 0x57: /* LD D,A */
            pGBMachine->regs.ucRegD = ucRegA;
            break;
         case 0x58: /* LD E,B */
            pGBMachine->regs.ucRegE = pGBMachine->regs.ucRegB;
            break;
         case 0x59: /* LD E,C */
            pGBMachine->regs.ucRegE = pGBMachine->regs.ucRegC;
            break;
         case 0x5A: /* LD E,D */
            pGBMachine->regs.ucRegE = pGBMachine->regs.ucRegD;
            break;
         case 0x5C: /* LD E,H */
            pGBMachine->regs.ucRegE = (char)(usRegHL >> 8);
            break;
         case 0x5D: /* LD E,L */
            pGBMachine->regs.ucRegE = (char)usRegHL;
            break;
         case 0x5F: /* LD E,A */
            pGBMachine->regs.ucRegE = ucRegA;
            break;
         case 0x60: /* LD H,B */
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegB << 8);
            break;
         case 0x61: /* LD H,C */
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegC << 8);
            break;
         case 0x62: /* LD H,D */
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegD << 8);
            break;
         case 0x63: /* LD H,E */
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegE << 8);
            break;
         case 0x65: /* LD H,L */
            usRegHL &= 0xff;
            usRegHL |= (usRegHL << 8);
            break;
         case 0x67: /* LD H,A */
            usRegHL &= 0xff;
            usRegHL |= (ucRegA << 8);
            break;
         case 0x68: /* LD L,B */
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegB;
            break;
         case 0x69: /* LD L,C */
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegC;
            break;
         case 0x6A: /* LD L,D */
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegD;
            break;
         case 0x6B: /* LD L,E */
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegE;
            break;
         case 0x6C: /* LD L,H */
            usRegHL &= 0xff00;
            usRegHL |= (usRegHL >> 8);
            break;
         case 0x6F: /* LD L,A */
            usRegHL &= 0xff00;
            usRegHL |= ucRegA;
            break;
         case 0x78: /* LD A,B */
            ucRegA = pGBMachine->regs.ucRegB;
            break;
         case 0x79: /* LD A,C */
            ucRegA = pGBMachine->regs.ucRegC;
            break;
         case 0x7A: /* LD A,D */
            ucRegA = pGBMachine->regs.ucRegD;
            break;
         case 0x7B: /* LD A,E */
            ucRegA = pGBMachine->regs.ucRegE;
            break;
         case 0x7C: /* LD A,H */
            ucRegA = (char)(usRegHL >> 8);
            break;
         case 0x7D: /* LD A,L */
            ucRegA = (char)usRegHL;
            break;

         case 0x46: /* LD B,(HL) */
            pGBMachine->regs.ucRegB = Z80ReadByte(usRegHL);
            break;
         case 0x4E: /* LD C,(HL) */
            pGBMachine->regs.ucRegC = Z80ReadByte(usRegHL);
            break;
         case 0x56: /* LD D,(HL) */
            pGBMachine->regs.ucRegD = Z80ReadByte(usRegHL);
            break;
         case 0x5E: /* LD E,(HL) */
            pGBMachine->regs.ucRegE = Z80ReadByte(usRegHL);
            break;
         case 0x66: /* LD H,(HL) */
            pGBMachine->regs.ucRegH = Z80ReadByte(usRegHL);
            usRegHL &= 0xff;
            usRegHL |= (pGBMachine->regs.ucRegH << 8);
            break;
         case 0x6E: /* LD L,(HL) */
            pGBMachine->regs.ucRegL = Z80ReadByte(usRegHL);
            usRegHL &= 0xff00;
            usRegHL |= pGBMachine->regs.ucRegL;
            break;
         case 0x7E: /* LD A,(HL) */
            ucRegA = Z80ReadByte(usRegHL);
            break;
         case 0x70: /* LD (HL),B */
            Z80WriteByte(usRegHL, pGBMachine->regs.ucRegB);
            break;
         case 0x71: /* LD (HL),C */
            Z80WriteByte(usRegHL, pGBMachine->regs.ucRegC);
            break;
         case 0x72: /* LD (HL),D */
            Z80WriteByte(usRegHL, pGBMachine->regs.ucRegD);
            break;
         case 0x73: /* LD (HL),E */
            Z80WriteByte(usRegHL, pGBMachine->regs.ucRegE);
            break;
         case 0x74: /* LD (HL),H */
            Z80WriteByte(usRegHL, (char)(usRegHL >> 8));
            break;
         case 0x75: /* LD (HL),L */
            Z80WriteByte(usRegHL, (char)usRegHL);
            break;
         case 0x77: /* LD (HL),A */
            Z80WriteByte(usRegHL, ucRegA);
            break;
         case 0x76: /* HALT */
            pPC--;           /* stick on this instruction until an interrupt */
            pGBMachine->regs.ucRegHalt = TRUE; // in a halted state
            iLocalTicks = 0;
            break;
         case 0x80: /* ADD A,B */
            Z80ADD(pGBMachine->regs.ucRegB);
            break;
         case 0x81: /* ADD A,C */
            Z80ADD(pGBMachine->regs.ucRegC);
            break;
         case 0x82: /* ADD A,D */
            Z80ADD(pGBMachine->regs.ucRegD);
            break;
         case 0x83: /* ADD A,E */
            Z80ADD(pGBMachine->regs.ucRegE);
            break;
         case 0x84: /* ADD A,H */
            uc = (char)(usRegHL >> 8);
            Z80ADD(uc);
            break;
         case 0x85: /* ADD A,L */
            uc = (char)usRegHL;
            Z80ADD(uc);
            break;
         case 0x87: /* ADD A,A */
            Z80ADD(ucRegA);
            break;
         case 0x86: /* ADD A,(HL) */
            uc = Z80ReadByte(usRegHL);
            Z80ADD(uc);
            break;
         case 0x88: /* ADC A,B */
            Z80ADC(pGBMachine->regs.ucRegB);
            break;
         case 0x89: /* ADC A,C */
            Z80ADC(pGBMachine->regs.ucRegC);
            break;
         case 0x8A: /* ADC A,D */
            Z80ADC(pGBMachine->regs.ucRegD);
            break;
         case 0x8B: /* ADC A,E */
            Z80ADC(pGBMachine->regs.ucRegE);
            break;
         case 0x8C: /* ADC A,H */
            uc = (char)(usRegHL >> 8);
            Z80ADC(uc);
            break;
         case 0x8D: /* ADC A,L */
            uc = (char)usRegHL;
            Z80ADC(uc);
            break;
         case 0x8F: /* ADC A,A */
            Z80ADC(ucRegA);
            break;
         case 0x8E: /* ADC A,(HL) */
            uc = Z80ReadByte(usRegHL);
            Z80ADC(uc);
            break;
         case 0x90: /* SUB B */
            Z80SUB(pGBMachine->regs.ucRegB);
            break;
         case 0x91: /* SUB C */
            Z80SUB(pGBMachine->regs.ucRegC);
            break;
         case 0x92: /* SUB D */
            Z80SUB(pGBMachine->regs.ucRegD);
            break;
         case 0x93: /* SUB E */
            Z80SUB(pGBMachine->regs.ucRegE);
            break;
         case 0x94: /* SUB H */
            uc = (char)(usRegHL >> 8);
            Z80SUB(uc);
            break;
         case 0x95: /* SUB L */
            uc = (char)usRegHL;
            Z80SUB(uc);
            break;
         case 0x97: /* SUB A */
            Z80SUB(ucRegA);
            break;
         case 0x96: /* SUB (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80SUB(uc);
            break;
         case 0x98: /* SBC A,B */
            Z80SBC(pGBMachine->regs.ucRegB);
            break;
         case 0x99: /* SBC A,C */
            Z80SBC(pGBMachine->regs.ucRegC);
            break;
         case 0x9A: /* SBC A,D */
            Z80SBC(pGBMachine->regs.ucRegD);
            break;
         case 0x9B: /* SBC A,E */
            Z80SBC(pGBMachine->regs.ucRegE);
            break;
         case 0x9C: /* SBC A,H */
            uc = (char)(usRegHL >> 8);
            Z80SBC(uc);
            break;
         case 0x9D: /* SBC A,L */
            uc = (char)usRegHL;
            Z80SBC(uc);
            break;
         case 0x9F: /* SBC A,A */
            Z80SBC(ucRegA);
            break;
         case 0x9E: /* SBC A,(HL) */
            uc = Z80ReadByte(usRegHL);
            Z80SBC(uc);
            break;
         case 0xA0: /* AND B */
            Z80AND(pGBMachine->regs.ucRegB);
            break;
         case 0xA1: /* AND C */
            Z80AND(pGBMachine->regs.ucRegC);
            break;
         case 0xA2: /* AND D */
            Z80AND(pGBMachine->regs.ucRegD);
            break;
         case 0xA3: /* AND E */
            Z80AND(pGBMachine->regs.ucRegE);
            break;
         case 0xA4: /* AND H */
            uc = (char)(usRegHL >> 8);
            Z80AND(uc);
            break;
         case 0xA5: /* AND L */
            uc = (char)usRegHL;
            Z80AND(uc);
            break;
         case 0xA7: /* AND A */
//            Z80AND(ucRegA);
            ucRegF = F_HALFCARRY | cZ80Z[ucRegA]; // this only affects the flags
            break;
         case 0xA6: /* AND (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80AND(uc);
            break;
         case 0xA8: /* XOR B */
            Z80XOR(pGBMachine->regs.ucRegB);
            break;
         case 0xA9: /* XOR C */
            Z80XOR(pGBMachine->regs.ucRegC);
            break;
         case 0xAA: /* XOR D */
            Z80XOR(pGBMachine->regs.ucRegD);
            break;
         case 0xAB: /* XOR E */
            Z80XOR(pGBMachine->regs.ucRegE);
            break;
         case 0xAC: /* XOR H */
            uc = (char)(usRegHL >> 8);
            Z80XOR(uc);
            break;
         case 0xAD: /* XOR L */
            uc = (char)usRegHL;
            Z80XOR(uc);
            break;
         case 0xAF: /* XOR A */
//            Z80XOR(ucRegA);
            ucRegA = 0;       // XORing A with itself always gives the same result
            ucRegF = F_ZERO;
            break;
         case 0xAE: /* XOR (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80XOR(uc);
            break;
         case 0xB0: /* OR B */
            Z80OR(pGBMachine->regs.ucRegB);
            break;
         case 0xB1: /* OR C */
            Z80OR(pGBMachine->regs.ucRegC);
            break;
         case 0xB2: /* OR D */
            Z80OR(pGBMachine->regs.ucRegD);
            break;
         case 0xB3: /* OR E */
            Z80OR(pGBMachine->regs.ucRegE);
            break;
         case 0xB4: /* OR H */
            uc = (char)(usRegHL >> 8);
            Z80OR(uc);
            break;
         case 0xB5: /* OR L */
            uc = (char)usRegHL;
            Z80OR(uc);
            break;
         case 0xB7: /* OR A */
//            Z80OR(ucRegA);
            ucRegF = cZ80Z[ucRegA]; // this only affects the flags
            break;
         case 0xB6: /* OR (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80OR(uc);
            break;
         case 0xB8: /* CP B */
            Z80CMP(pGBMachine->regs.ucRegB);
            break;
         case 0xB9: /* CP C */
            Z80CMP(pGBMachine->regs.ucRegC);
            break;
         case 0xBA: /* CP D */
            Z80CMP(pGBMachine->regs.ucRegD);
            break;
         case 0xBB: /* CP E */
            Z80CMP(pGBMachine->regs.ucRegE);
            break;
         case 0xBC: /* CP H */
            uc = (char)(usRegHL >> 8);
            Z80CMP(uc);
            break;
         case 0xBD: /* CP L */
            uc = (char)usRegHL;
            Z80CMP(uc);
            break;
         case 0xBF: /* CP A */
            Z80CMP(ucRegA);
            break;
         case 0xBE: /* CP (HL) */
            uc = Z80ReadByte(usRegHL);
            Z80CMP(uc);
            break;
         case 0xC0: /* RET NZ */
            if (!(ucRegF & F_ZERO))
               {
               PC = Z80POPW();
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            break;
         case 0xC1: /* POP BC */
            pGBMachine->regs.usRegBC = Z80POPW();
            break;
         case 0xC2: /* JP NZ,(nn) */
            if (!(ucRegF & F_ZERO))
               {
               PC = pPC[0] + (pPC[1] << 8);
//#ifdef SPEED_HACKS
//               if (pOps[-2] == 0xfe && PC < 8 && PC >= 4) // we are stuck in a cmp/jp loop, exit
//                  iLocalTicks = 0;
//#endif
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 4;
               }
            else
               pPC += 2;
            break;
         case 0xC3: /* JP (nn) */
            PC = pPC[0] + (pPC[1] << 8);
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            break;
         case 0xC4: /* CALL NZ,(nn) */
            if (!(ucRegF & F_ZERO))
               {
               Z80PUSHW((unsigned short)(pPC - pSegment + 2));
               PC = pPC[0] + (pPC[1] << 8);
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            else
               pPC += 2;
            break;
         case 0xC5: /* PUSH BC */
            Z80PUSHW(pGBMachine->regs.usRegBC);
            break;
         case 0xC6: /* ADD A,n */
            Z80ADD(pPC[0]);
            pPC++;
            break;
         case 0xC7: /* RST 0H */
         case 0xCF: /* RST 8H */
         case 0xD7: /* RST 10H */
         case 0xDF: /* RST 18H */
         case 0xE7: /* RST 20H */
         case 0xEF: /* RST 28H */
         case 0xF7: /* RST 30H */
         case 0xFF: /* RST 38H */
            Z80PUSHW((unsigned short)(pPC - pSegment));
            PC = iOpcode & 0x38;
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            break;
         case 0xC8: /* RET Z */
            if (ucRegF & F_ZERO)
               {
               PC = Z80POPW();
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            break;
         case 0xC9: /* RET */
            PC = Z80POPW();
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            break;
         case 0xCA: /* JP Z,(nn) */
            if (ucRegF & F_ZERO)
               {
               PC = pPC[0] + (pPC[1] << 8);
//#ifdef SPEED_HACKS
//               if (pOps[-2] == 0xfe && PC < 8 && PC >= 4) // we are stuck in a cmp/jp loop, exit
//                  iLocalTicks = 0;
//#endif
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 4;
               }
            else
               pPC += 2;
            break;
         case 0xCB: /* New set of opcodes branches here */
            iOpcode = *pPC++;
            iLocalTicks -= cZ80CBCycles[iOpcode];
            switch(iOpcode)
               {
               case 0x00: /* RLC B */
                  Z80RLC(pGBMachine->regs.ucRegB);
                  break;
               case 0x01: /* RLC C */
                  Z80RLC(pGBMachine->regs.ucRegC);
                  break;
               case 0x02: /* RLC D */
                  Z80RLC(pGBMachine->regs.ucRegD);
                  break;
               case 0x03: /* RLC E */
                  Z80RLC(pGBMachine->regs.ucRegE);
                  break;
               case 0x04: /* RLC H */
               // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80RLC(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x05: /* RLC L */
               // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80RLC(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x07: /* RLC A */
                  Z80RLC(ucRegA);
                  break;
               case 0x06: /* RLC (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80RLC(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;
               case 0x08: /* RRC B */
                  Z80RRC(pGBMachine->regs.ucRegB);
                  break;
               case 0x09: /* RRC C */
                  Z80RRC(pGBMachine->regs.ucRegC);
                  break;
               case 0x0A: /* RRC D */
                  Z80RRC(pGBMachine->regs.ucRegD);
                  break;
               case 0x0B: /* RRC E */
                  Z80RRC(pGBMachine->regs.ucRegE);
                  break;
               case 0x0C: /* RRC H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80RRC(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x0D: /* RRC L */
                  // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80RRC(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x0F: /* RRC A */
                  Z80RRC(ucRegA);
                  break;
               case 0x0E: /* RRC (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80RRC(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;
               case 0x10: /* RL B */
                  Z80RL(pGBMachine->regs.ucRegB);
                  break;
               case 0x11: /* RL C */
                  Z80RL(pGBMachine->regs.ucRegC);
                  break;
               case 0x12: /* RL D */
                  Z80RL(pGBMachine->regs.ucRegD);
                  break;
               case 0x13: /* RL E */
                  Z80RL(pGBMachine->regs.ucRegE);
                  break;
               case 0x14: /* RL H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80RL(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x15: /* RL L */
                // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80RL(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x17: /* RL A */
                  Z80RL(ucRegA);
                  break;
               case 0x16: /* RL (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80RL(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;
               case 0x18: /* RR B */
                  Z80RR(pGBMachine->regs.ucRegB);
                  break;
               case 0x19: /* RR C */
                  Z80RR(pGBMachine->regs.ucRegC);
                  break;
               case 0x1A: /* RR D */
                  Z80RR(pGBMachine->regs.ucRegD);
                  break;
               case 0x1B: /* RR E */
                  Z80RR(pGBMachine->regs.ucRegE);
                  break;
               case 0x1C: /* RR H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80RR(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x1D: /* RR L */
                // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80RR(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x1F: /* RR A */
                  Z80RR(ucRegA);
                  break;
               case 0x1E: /* RR (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80RR(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;
               case 0x20: /* SLA B */
                  Z80SLA(pGBMachine->regs.ucRegB);
                  break;
               case 0x21: /* SLA C */
                  Z80SLA(pGBMachine->regs.ucRegC);
                  break;
               case 0x22: /* SLA D */
                  Z80SLA(pGBMachine->regs.ucRegD);
                  break;
               case 0x23: /* SLA E */
                  Z80SLA(pGBMachine->regs.ucRegE);
                  break;
               case 0x24: /* SLA H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80SLA(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x25: /* SLA L */
                // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80SLA(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x27: /* SLA A */
                  Z80SLA(ucRegA);
                  break;
               case 0x26: /* SLA (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80SLA(uc);
                  Z80WriteByte(usRegHL, uc);
                  break;
               case 0x28: /* SRA B */
                  Z80SRA(pGBMachine->regs.ucRegB);
                  break;
               case 0x29: /* SRA C */
                  Z80SRA(pGBMachine->regs.ucRegC);
                  break;
               case 0x2A: /* SRA D */
                  Z80SRA(pGBMachine->regs.ucRegD);
                  break;
               case 0x2B: /* SRA E */
                  Z80SRA(pGBMachine->regs.ucRegE);
                  break;
               case 0x2C: /* SRA H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80SRA(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x2D: /* SRA L */
                  // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80SRA(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x2F: /* SRA A */
                  Z80SRA(ucRegA);
                  break;
               case 0x2E: /* SRA (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80SRA(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;
               case 0x30: // SWAP B
                  pGBMachine->regs.ucRegB = cNibbleSwap[pGBMachine->regs.ucRegB];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegB];
                  break;

               case 0x31: // SWAP C
                  pGBMachine->regs.ucRegC = cNibbleSwap[pGBMachine->regs.ucRegC];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegC];
                  break;

               case 0x32: // SWAP D
                  pGBMachine->regs.ucRegD = cNibbleSwap[pGBMachine->regs.ucRegD];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegD];
                  break;

               case 0x33: // SWAP E
                  pGBMachine->regs.ucRegE = cNibbleSwap[pGBMachine->regs.ucRegE];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegE];
                  break;

               case 0x34: // SWAP H
                  pGBMachine->regs.ucRegH = cNibbleSwap[(unsigned char)(usRegHL >> 8)];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegH];
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;

               case 0x35: // SWAP L
                  pGBMachine->regs.ucRegL = cNibbleSwap[(unsigned char)usRegHL];
                  ucRegF = cZ80Z[pGBMachine->regs.ucRegL];
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;

               case 0x36: // SWAP (HL)
                  uc = cNibbleSwap[Z80ReadByte(usRegHL)];
                  Z80WriteByte(usRegHL, uc);
                  ucRegF = cZ80Z[uc];
                  break;

               case 0x37: // SWAP A
                  ucRegA = cNibbleSwap[ucRegA];
                  ucRegF = cZ80Z[ucRegA];
                  break;

               case 0x38: /* SRL B */
                  Z80SRL(pGBMachine->regs.ucRegB);
                  break;
               case 0x39: /* SRL C */
                  Z80SRL(pGBMachine->regs.ucRegC);
                  break;
               case 0x3A: /* SRL D */
                  Z80SRL(pGBMachine->regs.ucRegD);
                  break;
               case 0x3B: /* SRL E */
                  Z80SRL(pGBMachine->regs.ucRegE);
                  break;
               case 0x3C: /* SRL H */
                  // kludge
                  pGBMachine->regs.ucRegH = (char)(usRegHL >> 8);
                  Z80SRL(pGBMachine->regs.ucRegH);
                  usRegHL &= 0xff;
                  usRegHL |= (pGBMachine->regs.ucRegH << 8);
                  break;
               case 0x3D: /* SRL L */
                  // kludge
                  pGBMachine->regs.ucRegL = (char)usRegHL;
                  Z80SRL(pGBMachine->regs.ucRegL);
                  usRegHL &= 0xff00;
                  usRegHL |= pGBMachine->regs.ucRegL;
                  break;
               case 0x3F: /* SRL A */
                  Z80SRL(ucRegA);
                  break;
               case 0x3E: /* SRL (HL) */
                  uc = Z80ReadByte(usRegHL);
                  Z80SRL(uc);
                  Z80WriteByte(usRegHL,uc);
                  break;

               case 0x40: /* BIT 0,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x41: /* BIT 0,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x42: /* BIT 0,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x43: /* BIT 0,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x44: /* BIT 0,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x100))
                     ucRegF |= F_ZERO;
                  break;
               case 0x45: /* BIT 0,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x47: /* BIT 0,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x48: /* BIT 1,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x49: /* BIT 1,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4A: /* BIT 1,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4B: /* BIT 1,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4C: /* BIT 1,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x200))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4D: /* BIT 1,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4F: /* BIT 1,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 2))
                     ucRegF |= F_ZERO;
                  break;
               case 0x50: /* BIT 2,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x51: /* BIT 2,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x52: /* BIT 2,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x53: /* BIT 2,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x54: /* BIT 2,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x400))
                     ucRegF |= F_ZERO;
                  break;
               case 0x55: /* BIT 2,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x57: /* BIT 2,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x58: /* BIT 3,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x59: /* BIT 3,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5A: /* BIT 3,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5B: /* BIT 3,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5C: /* BIT 3,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x800))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5D: /* BIT 3,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5F: /* BIT 3,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x60: /* BIT 4,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x61: /* BIT 4,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x62: /* BIT 4,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x63: /* BIT 4,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x64: /* BIT 4,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x1000))
                     ucRegF |= F_ZERO;
                  break;
               case 0x65: /* BIT 4,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x67: /* BIT 4,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x68: /* BIT 5,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x69: /* BIT 5,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6A: /* BIT 5,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6B: /* BIT 5,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6C: /* BIT 5,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x2000))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6D: /* BIT 5,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6F: /* BIT 5,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x70: /* BIT 6,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x71: /* BIT 6,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x72: /* BIT 6,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x73: /* BIT 6,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x74: /* BIT 6,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x4000))
                     ucRegF |= F_ZERO;
                  break;
               case 0x75: /* BIT 6,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x77: /* BIT 6,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x78: /* BIT 7,B */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegB & 128))
                     ucRegF |= F_ZERO;
                  break;
               case 0x79: /* BIT 7,C */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegC & 128))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7A: /* BIT 7,D */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegD & 128))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7B: /* BIT 7,E */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(pGBMachine->regs.ucRegE & 128))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7C: /* BIT 7,H */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 0x8000))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7D: /* BIT 7,L */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(usRegHL & 128))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7F: /* BIT 7,A */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(ucRegA & 128))
                     ucRegF |= F_ZERO;
                  break;

               case 0x46: /* BIT 0,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 1))
                     ucRegF |= F_ZERO;
                  break;
               case 0x4E: /* BIT 1,(HL) */
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
#ifdef SPEED_HACKS
                  if (usRegHL == 0xff41 && (ucLCDSTAT & 2)) // the code is waiting for HBlank to do something?
                     {
                     if (pPC[0] == 0x20 && pPC[1] == 0xfc) // it is in a tight loop waiting for HBlank
                        {
                        iLocalTicks = 0; // time to go since LCD status will not change in this time slice
                        }
                     }
                  else
#endif
                     {
                     if (!(Z80ReadByte(usRegHL) & 2))
                        ucRegF |= F_ZERO;
                     }
                  break;
               case 0x56: /* BIT 2,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 4))
                     ucRegF |= F_ZERO;
                  break;
               case 0x5E: /* BIT 3,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 8))
                     ucRegF |= F_ZERO;
                  break;
               case 0x66: /* BIT 4,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 16))
                     ucRegF |= F_ZERO;
                  break;
               case 0x6E: /* BIT 5,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 32))
                     ucRegF |= F_ZERO;
                  break;
               case 0x76: /* BIT 6,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 64))
                     ucRegF |= F_ZERO;
                  break;
               case 0x7E: /* BIT 7,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  ucRegF &= F_CARRY; // only preserves carry
                  ucRegF |= F_HALFCARRY;
                  if (!(Z80ReadByte(usRegHL) & 128))
                     ucRegF |= F_ZERO;
                  break;

               case 0x80: /* RES 0,B */
                  pGBMachine->regs.ucRegB &= ~1;
                  break;
               case 0x81: /* RES 0,C */
                  pGBMachine->regs.ucRegC &= ~1;
                  break;
               case 0x82: /* RES 0,D */
                  pGBMachine->regs.ucRegD &= ~1;
                  break;
               case 0x83: /* RES 0,E */
                  pGBMachine->regs.ucRegE &= ~1;
                  break;
               case 0x84: /* RES 0,H */
                  usRegHL &= ~0x100;
                  break;
               case 0x85: /* RES 0,L */
                  usRegHL &= ~1;
                  break;
               case 0x87: /* RES 0,A */
                  ucRegA &= ~1;
                  break;
               case 0x88: /* RES 1,B */
                  pGBMachine->regs.ucRegB &= ~2;
                  break;
               case 0x89: /* RES 1,C */
                  pGBMachine->regs.ucRegC &= ~2;
                  break;
               case 0x8A: /* RES 1,D */
                  pGBMachine->regs.ucRegD &= ~2;
                  break;
               case 0x8B: /* RES 1,E */
                  pGBMachine->regs.ucRegE &= ~2;
                  break;
               case 0x8C: /* RES 1,H */
                  usRegHL &= ~0x200;
                  break;
               case 0x8D: /* RES 1,L */
                  usRegHL &= ~2;
                  break;
               case 0x8F: /* RES 1,A */
                  ucRegA &= ~2;
                  break;
               case 0x90: /* RES 2,B */
                  pGBMachine->regs.ucRegB &= ~4;
                  break;
               case 0x91: /* RES 2,C */
                  pGBMachine->regs.ucRegC &= ~4;
                  break;
               case 0x92: /* RES 2,D */
                  pGBMachine->regs.ucRegD &= ~4;
                  break;
               case 0x93: /* RES 2,E */
                  pGBMachine->regs.ucRegE &= ~4;
                  break;
               case 0x94: /* RES 2,H */
                  usRegHL &= ~0x400;
                  break;
               case 0x95: /* RES 2,L */
                  usRegHL &= ~4;
                  break;
               case 0x97: /* RES 2,A */
                  ucRegA &= ~4;
                  break;
               case 0x98: /* RES 3,B */
                  pGBMachine->regs.ucRegB &= ~8;
                  break;
               case 0x99: /* RES 3,C */
                  pGBMachine->regs.ucRegC &= ~8;
                  break;
               case 0x9A: /* RES 3,D */
                  pGBMachine->regs.ucRegD &= ~8;
                  break;
               case 0x9B: /* RES 3,E */
                  pGBMachine->regs.ucRegE &= ~8;
                  break;
               case 0x9C: /* RES 3,H */
                  usRegHL &= ~0x800;
                  break;
               case 0x9D: /* RES 3,L */
                  usRegHL &= ~8;
                  break;
               case 0x9F: /* RES 3,A */
                  ucRegA &= ~8;
                  break;
               case 0xA0: /* RES 4,B */
                  pGBMachine->regs.ucRegB &= ~16;
                  break;
               case 0xA1: /* RES 4,C */
                  pGBMachine->regs.ucRegC &= ~16;
                  break;
               case 0xA2: /* RES 4,D */
                  pGBMachine->regs.ucRegD &= ~16;
                  break;
               case 0xA3: /* RES 4,E */
                  pGBMachine->regs.ucRegE &= ~16;
                  break;
               case 0xA4: /* RES 4,H */
                  usRegHL &= ~0x1000;
                  break;
               case 0xA5: /* RES 4,L */
                  usRegHL &= ~16;
                  break;
               case 0xA7: /* RES 4,A */
                  ucRegA &= ~16;
                  break;
               case 0xA8: /* RES 5,B */
                  pGBMachine->regs.ucRegB &= ~32;
                  break;
               case 0xA9: /* RES 5,C */
                  pGBMachine->regs.ucRegC &= ~32;
                  break;
               case 0xAA: /* RES 5,D */
                  pGBMachine->regs.ucRegD &= ~32;
                  break;
               case 0xAB: /* RES 5,E */
                  pGBMachine->regs.ucRegE &= ~32;
                  break;
               case 0xAC: /* RES 5,H */
                  usRegHL &= ~0x2000;
                  break;
               case 0xAD: /* RES 5,L */
                  usRegHL &= ~32;
                  break;
               case 0xAF: /* RES 5,A */
                  ucRegA &= ~32;
                  break;
               case 0xB0: /* RES 6,B */
                  pGBMachine->regs.ucRegB &= ~64;
                  break;
               case 0xB1: /* RES 6,C */
                  pGBMachine->regs.ucRegC &= ~64;
                  break;
               case 0xB2: /* RES 6,D */
                  pGBMachine->regs.ucRegD &= ~64;
                  break;
               case 0xB3: /* RES 6,E */
                  pGBMachine->regs.ucRegE &= ~64;
                  break;
               case 0xB4: /* RES 6,H */
                  usRegHL &= ~0x4000;
                  break;
               case 0xB5: /* RES 6,L */
                  usRegHL &= ~64;
                  break;
               case 0xB7: /* RES 6,A */
                  ucRegA &= ~64;
                  break;
               case 0xB8: /* RES 7,B */
                  pGBMachine->regs.ucRegB &= ~128;
                  break;
               case 0xB9: /* RES 7,C */
                  pGBMachine->regs.ucRegC &= ~128;
                  break;
               case 0xBA: /* RES 7,D */
                  pGBMachine->regs.ucRegD &= ~128;
                  break;
               case 0xBB: /* RES 7,E */
                  pGBMachine->regs.ucRegE &= ~128;
                  break;
               case 0xBC: /* RES 7,H */
                  usRegHL &= ~0x8000;
                  break;
               case 0xBD: /* RES 7,L */
                  usRegHL &= ~128;
                  break;
               case 0xBF: /* RES 7,A */
                  ucRegA &= ~128;
                  break;

               case 0x86: /* RES 0,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~1));
                  break;
               case 0x8E: /* RES 1,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~2));
                  break;
               case 0x96: /* RES 2,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~4));
                  break;
               case 0x9E: /* RES 3,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~8));
                  break;
               case 0xA6: /* RES 4,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~16));
                  break;
               case 0xAE: /* RES 5,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~32));
                  break;
               case 0xB6: /* RES 6,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~64));
                  break;
               case 0xBE: /* RES 7,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) & ~128));
                  break;

               case 0xC0: /* SET 0,B */
                  pGBMachine->regs.ucRegB |= 1;
                  break;
               case 0xC1: /* SET 0,C */
                  pGBMachine->regs.ucRegC |= 1;
                  break;
               case 0xC2: /* SET 0,D */
                  pGBMachine->regs.ucRegD |= 1;
                  break;
               case 0xC3: /* SET 0,E */
                  pGBMachine->regs.ucRegE |= 1;
                  break;
               case 0xC4: /* SET 0,H */
                  usRegHL |= 0x100;
                  break;
               case 0xC5: /* SET 0,L */
                  usRegHL |= 1;
                  break;
               case 0xC7: /* SET 0,A */
                  ucRegA |= 1;
                  break;
               case 0xC8: /* SET 1,B */
                  pGBMachine->regs.ucRegB |= 2;
                  break;
               case 0xC9: /* SET 1,C */
                  pGBMachine->regs.ucRegC |= 2;
                  break;
               case 0xCA: /* SET 1,D */
                  pGBMachine->regs.ucRegD |= 2;
                  break;
               case 0xCB: /* SET 1,E */
                  pGBMachine->regs.ucRegE |= 2;
                  break;
               case 0xCC: /* SET 1,H */
                  usRegHL |= 0x200;
                  break;
               case 0xCD: /* SET 1,L */
                  usRegHL |= 2;
                  break;
               case 0xCF: /* SET 1,A */
                  ucRegA |= 2;
                  break;
               case 0xD0: /* SET 2,B */
                  pGBMachine->regs.ucRegB |= 4;
                  break;
               case 0xD1: /* SET 2,C */
                  pGBMachine->regs.ucRegC |= 4;
                  break;
               case 0xD2: /* SET 2,D */
                  pGBMachine->regs.ucRegD |= 4;
                  break;
               case 0xD3: /* SET 2,E */
                  pGBMachine->regs.ucRegE |= 4;
                  break;
               case 0xD4: /* SET 2,H */
                  usRegHL |= 0x400;
                  break;
               case 0xD5: /* SET 2,L */
                  usRegHL |= 4;
                  break;
               case 0xD7: /* SET 2,A */
                  ucRegA |= 4;
                  break;
               case 0xD8: /* SET 3,B */
                  pGBMachine->regs.ucRegB |= 8;
                  break;
               case 0xD9: /* SET 3,C */
                  pGBMachine->regs.ucRegC |= 8;
                  break;
               case 0xDA: /* SET 3,D */
                  pGBMachine->regs.ucRegD |= 8;
                  break;
               case 0xDB: /* SET 3,E */
                  pGBMachine->regs.ucRegE |= 8;
                  break;
               case 0xDC: /* SET 3,H */
                  usRegHL |= 0x800;
                  break;
               case 0xDD: /* SET 3,L */
                  usRegHL |= 8;
                  break;
               case 0xDF: /* SET 3,A */
                  ucRegA |= 8;
                  break;
               case 0xE0: /* SET 4,B */
                  pGBMachine->regs.ucRegB |= 16;
                  break;
               case 0xE1: /* SET 4,C */
                  pGBMachine->regs.ucRegC |= 16;
                  break;
               case 0xE2: /* SET 4,D */
                  pGBMachine->regs.ucRegD |= 16;
                  break;
               case 0xE3: /* SET 4,E */
                  pGBMachine->regs.ucRegE |= 16;
                  break;
               case 0xE4: /* SET 4,H */
                  usRegHL |= 0x1000;
                  break;
               case 0xE5: /* SET 4,L */
                  usRegHL |= 16;
                  break;
               case 0xE7: /* SET 4,A */
                  ucRegA |= 16;
                  break;
               case 0xE8: /* SET 5,B */
                  pGBMachine->regs.ucRegB |= 32;
                  break;
               case 0xE9: /* SET 5,C */
                  pGBMachine->regs.ucRegC |= 32;
                  break;
               case 0xEA: /* SET 5,D */
                  pGBMachine->regs.ucRegD |= 32;
                  break;
               case 0xEB: /* SET 5,E */
                  pGBMachine->regs.ucRegE |= 32;
                  break;
               case 0xEC: /* SET 5,H */
                  usRegHL |= 0x2000;
                  break;
               case 0xED: /* SET 5,L */
                  usRegHL |= 32;
                  break;
               case 0xEF: /* SET 5,A */
                  ucRegA |= 32;
                  break;
               case 0xF0: /* SET 6,B */
                  pGBMachine->regs.ucRegB |= 64;
                  break;
               case 0xF1: /* SET 6,C */
                  pGBMachine->regs.ucRegC |= 64;
                  break;
               case 0xF2: /* SET 6,D */
                  pGBMachine->regs.ucRegD |= 64;
                  break;
               case 0xF3: /* SET 6,E */
                  pGBMachine->regs.ucRegE |= 64;
                  break;
               case 0xF4: /* SET 6,H */
                  usRegHL |= 0x4000;
                  break;
               case 0xF5: /* SET 6,L */
                  usRegHL |= 64;
                  break;
               case 0xF7: /* SET 6,A */
                  ucRegA |= 64;
                  break;
               case 0xF8: /* SET 7,B */
                  pGBMachine->regs.ucRegB |= 128;
                  break;
               case 0xF9: /* SET 7,C */
                  pGBMachine->regs.ucRegC |= 128;
                  break;
               case 0xFA: /* SET 7,D */
                  pGBMachine->regs.ucRegD |= 128;
                  break;
               case 0xFB: /* SET 7,E */
                  pGBMachine->regs.ucRegE |= 128;
                  break;
               case 0xFC: /* SET 7,H */
                  usRegHL |= 0x8000;
                  break;
               case 0xFD: /* SET 7,L */
                  usRegHL |= 128;
                  break;
               case 0xFF: /* SET 7,A */
                  ucRegA |= 128;
                  break;

               case 0xC6: /* SET 0,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 1));
                  break;
               case 0xCE: /* SET 1,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 2));
                  break;
               case 0xD6: /* SET 2,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 4));
                  break;
               case 0xDE: /* SET 3,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 8));
                  break;
               case 0xE6: /* SET 4,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 16));
                  break;
               case 0xEE: /* SET 5,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 32));
                  break;
               case 0xF6: /* SET 6,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 64));
                  break;
               case 0xFE: /* SET 7,(HL) */
//                  c = Z80ReadByte(usRegHL);
                  Z80WriteByte(usRegHL, (unsigned char)(Z80ReadByte(usRegHL) | 128));
                  break;
               default: /* ILLEGAL */
                  iLocalTicks = 0;
                  break;
               } /* switch on CB */
            break;
         case 0xCC: /* CALL Z,(nn) */
            if (ucRegF & F_ZERO)
               {
               Z80PUSHW((unsigned short)(pPC - pSegment + 2));
               PC = pPC[0] + (pPC[1] << 8);
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            else
               pPC += 2;
            break;
         case 0xCD: /* CALL (nn) */
            Z80PUSHW((unsigned short)(pPC - pSegment + 2));
            PC = pPC[0] + (pPC[1] << 8);
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            break;
         case 0xCE: /* ADC A,n */
            Z80ADC(pPC[0]);
            pPC++;
            break;
         case 0xD0: /* RET NC */
            if (!(ucRegF & F_CARRY))
               {
               PC = Z80POPW();
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            break;
         case 0xD1: /* POP DE */
            pGBMachine->regs.usRegDE = Z80POPW();
            break;
         case 0xD2: /* JP NC,(nn) */
            if (!(ucRegF & F_CARRY))
               {
               PC = pPC[0] + (pPC[1] << 8);
//#ifdef SPEED_HACKS
//               if (pOps[-2] == 0xfe && PC < 8 && PC >= 4) // we are stuck in a cmp/jp loop, exit
//                  iLocalTicks = 0;
//#endif
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 4;
               }
            else
               pPC += 2;
            break;

         case 0xD4: /* CALL NC,(nn) */
            if (!(ucRegF & F_CARRY))
               {
               Z80PUSHW((unsigned short)(pPC - pSegment + 2));
               PC = pPC[0] + (pPC[1] << 8);
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            else
               pPC += 2;
            break;
         case 0xD5: /* PUSH DE */
            Z80PUSHW(pGBMachine->regs.usRegDE);
            break;
         case 0xD6: /* SUB n */
            Z80SUB(pPC[0]);
            pPC++;
            break;
         case 0xD8: /* RET C */
            if (ucRegF & F_CARRY)
               {
               PC = Z80POPW();
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            break;

         case 0xD9: /* RETI */ // DEBUG
            PC = Z80POPW();
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            pGBMachine->regs.ucRegI = 1; // enable interrupts again
            break;

         case 0xDA: /* JP C,(nn) */
            if (ucRegF & F_CARRY)
               {
               PC = pPC[0] + (pPC[1] << 8);
//#ifdef SPEED_HACKS
//               if (pOps[-2] == 0xfe && PC < 8 && PC >= 4) // we are stuck in a cmp/jp loop, exit
//                  iLocalTicks = 0;
//#endif
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 4;
               }
            else
               pPC += 2;
            break;

         case 0xDC: /* CALL C,(nn) */
            if (ucRegF & F_CARRY)
               {
               Z80PUSHW((unsigned short)(pPC - pSegment + 2));
               PC = pPC[0] + (pPC[1] << 8);
               pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
               pPC = &pSegment[PC];
               iLocalTicks -= 12;
               }
            else
               pPC += 2;
            break;

         case 0xDE: /* SBC A,n */
            Z80SBC(pPC[0]);
            pPC++;
            break;

         case 0xE0: /* LD (0xFFXX),A */
            usAddr = 0xff00 + *pPC++;
            if (usAddr >= 0xff80) // just RAM
               {
               pGBMachine->mem_map[usAddr] = ucRegA;
               if (usAddr == 0xffff)   // react immediately if interrupts have been enabled
                  {
                  pGBMachine->ucIRQEnable = ucRegA;
                  PC = GBCheckInterrupts((int)(pPC - pSegment));
                  pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
                  pPC = &pSegment[PC];
                  }
               }
            else // must be I/O
               {
               GBIOWrite(usAddr, ucRegA);
#ifdef SPEED_HACKS
               if (usAddr == 0xff55 && pGBMachine->iDMATime) // we did DMA, time to leave since CPU is halted
                  iLocalTicks = 0;
#endif
               }
//            Z80WriteByte((unsigned short)usAddr,ucRegA);
//            if (usAddr == 0xff46) // if we just did OAM DMA
//               {
//               if (pOps[2] == 0x3E) // if next instruction is LD A,n, skip this code
//                  {
//                  PC += 5; // skip the time delay code
//                  iLocalClock = iClockCompare; // leave immediately
//                  }
//               }
            break;

         case 0xE1: /* POP HL */
            usRegHL = Z80POPW();
            break;

         case 0xE2: /* LD (0xFF00 + C),A */
            usAddr = 0xff00 + pGBMachine->regs.ucRegC;
            if (usAddr >= 0xff80) // just RAM
               {
               pGBMachine->mem_map[usAddr] = ucRegA;
               if (usAddr == 0xffff)   // react immediately if interrupts have been enabled
                  {
                  pGBMachine->ucIRQEnable = ucRegA;
                  PC = GBCheckInterrupts((int)(pPC - pSegment));
                  pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
                  pPC = &pSegment[PC];
                  }
               }
            else // must be I/O
               {
               GBIOWrite(usAddr, ucRegA);
#ifdef SPEED_HACKS
               if (usAddr == 0xff55 && pGBMachine->iDMATime) // we did DMA, time to leave since CPU is halted
                  iLocalTicks = 0;
#endif
               }
            break;

         case 0xE3: /* EX (SP),HL */
            usAddr = Z80ReadWord(pGBMachine->regs.usRegSP);
            Z80WriteWord(pGBMachine->regs.usRegSP, (unsigned short)usRegHL);
            usRegHL = usAddr;
            break;

         case 0xE5: /* PUSH HL */
            Z80PUSHW((unsigned short)usRegHL);
            break;

         case 0xE6: /* AND n */
            Z80AND(pPC[0]);
            pPC++;
            break;

         case 0xE8: /* ADD SP,offset */
            pGBMachine->regs.usRegSP += (signed char)*pPC++;
            break;

         case 0xE9: /* JP (HL) */
            PC = usRegHL;
            pSegment = (unsigned char *)pGBMachine->ulOpList[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0xEA: /* LD (word),A */
            usAddr = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            Z80WriteByte((unsigned short)usAddr, ucRegA);
            break;

         case 0xEE: /* XOR n */
            Z80XOR(pPC[0]);
            pPC++;
            break;
         case 0xF0: /* LD A,(FFXX) */
            usAddr = 0xff00 + *pPC++;
            if (usAddr >= 0xff80) // just RAM
               ucRegA = pGBMachine->mem_map[usAddr];
            else
               {
               ucRegA = GBIORead(usAddr);
#ifdef SPEED_HACKS
               if (usAddr == 0xff41) // LCD controller
                  { // if next instruction is AND A,#x and a JZ or JNZ then
                    // don't waste the time if we are going to loop in this time slice
                  if (/*!pGBMachine->bTimerOn &&*/ pPC[1] == 0xE6 && (pPC[3] == 0x20 || pPC[3] == 0x28) && pPC[4] >= 0xf9)
                     {
                     if (pPC[2] == 0x20) // JR NZ - check the condition
                        {
                        if (ucRegA & pPC[1]) // it's going to branch back, so we can leave
                           iLocalTicks = 0;  // we can leave
                        }
                     else // JR Z - check the condition
                        {
                        if (!(ucRegA & pPC[1])) // it's going to branch back, so we can leave
                           iLocalTicks = 0;  // we can leave
                        }
                     }
                  }
#endif
               }
            break;

         case 0xF1: /* POP AF */
            pGBMachine->regs.usRegAF = Z80POPW();
            ucRegA = pGBMachine->regs.ucRegA;
            ucRegF = pGBMachine->regs.ucRegF & 0xf0; // get rid of lower 4 bits 
            break;

         case 0xF2: /* LD A,(FF00 + C) */
            usAddr = 0xff00 + pGBMachine->regs.ucRegC;
            if (usAddr >= 0xff80)
               ucRegA = pGBMachine->mem_map[usAddr];
            else
               ucRegA = GBIORead(usAddr);
            break;

         case 0xF3: /* DI */
            pGBMachine->regs.ucRegI = 0;
            if (iEITicks) // if there was just a EI, cancel it
               {
               iLocalTicks += iEITicks;
               iEITicks = 0;
               }
            break;

         case 0xF5: /* PUSH AF */
            pGBMachine->regs.ucRegA = ucRegA;
            pGBMachine->regs.ucRegF = ucRegF;
            Z80PUSHW((unsigned short)(pGBMachine->regs.usRegAF | 0xe)); // not sure why, but low nibble of flags is set to E
            break;

         case 0xF6: /* OR n */
            Z80OR(pPC[0]);
            pPC++;
            break;

         case 0xF8: /* LD HL,SP+imm */
            usRegHL = pGBMachine->regs.usRegSP + (signed char)*pPC++;
            break;

         case 0xF9: /* LD SP,HL */
            pGBMachine->regs.usRegSP = usRegHL;
            break;

         case 0xFA: /* LD A,(word) */
            usAddr = pPC[0] + (pPC[1] << 8);
            pPC += 2;
            ucRegA = Z80ReadByte(usAddr);
            break;

         case 0xFB: /* EI */
            pGBMachine->regs.ucRegI = 1;
            iEITicks = iLocalTicks - 1;
            iLocalTicks = 1; // allow next instruction to execute
            break;

         case 0xFE: /* CP n */
//            if ((pOps[2] == 0x30 || pOps[2] == 0x38 || pOps[2] == 0x28) && pOps[3] == 0xfa)
//               PC |= 0;
#ifdef SPEED_HACKS
            if (ucRegA != pPC[0] && pPC[1] == 0x20 && pPC[2] == 0xfa) // it's going to loop here until the value is equal
               {
               iLocalTicks = 0; // we are done
               }
#endif
            Z80CMP(pPC[0]);
            pPC++;
            break;
         default: // illegal
            iLocalTicks = 0; // uh oh
            break;
         }   /* switch */
      } /* while */
   if (iEITicks) // we just had an EI and another instruction, continue execution
      {
      iLocalTicks += iEITicks;
      iEITicks = 0;
      PC = (int)(pPC - pSegment);
      goto top_of_execution;
      }
   pGBMachine->regs.ucRegA = ucRegA;
   pGBMachine->regs.ucRegF = ucRegF;
   pGBMachine->regs.usRegHL = usRegHL;
   pGBMachine->regs.usRegPC = (unsigned short)(pPC - pSegment);
   if (pGBMachine->iMasterClock >= 0x10000 || pGBMachine->iTimerLimit >= 0x10000) // don't let them wrap around or the timer will get hosed
      {
      pGBMachine->iMasterClock -= 0x8000;
      pGBMachine->iTimerLimit -= 0x8000;
      }

} /* EXECGB() */
