/*******************************************************************/
/* 6502 CPU emulator written by Larry Bank                         */
/* Copyright 1998 BitBank Software, Inc.                           */
/*                                                                 */
/* This code was written from scratch using the 6502 data from     */
/* various sources of documentation.                               */
/*                                                                 */
/* Change history:                                                 */
/* 1/18/98 Wrote it - Larry B.                                     */
/* 1/12/06 fixed CLI to allow next instruction to execute.         */
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
#include <string.h>
#include "my_windows.h"
#include "smartgear.h"
#include "emu.h"

#define F_CARRY     1
#define F_ZERO      2
#define F_IRQMASK   4
#define F_DECIMAL   8
#define F_BREAK     16
#define F_UNUSED    32
#define F_OVERFLOW  64
#define F_NEGATIVE  128

#define SPEED_HACKS

/* Some statics */
static EMUHANDLERS *mem_handlers;
static unsigned char *mem_map;
static unsigned long *ulCPUOffs;
static int PC;
static unsigned char *pPC, *pSegment;
unsigned char uc6502Opcode;
static int iClocks;
int *pExternalClock;
extern BOOL bTrace;
void TRACE6502(REGS6502 *regs, int iClocks);
static void M6502PUSHB(unsigned char *, REGS6502 *, unsigned char);
static void M6502PUSHW(unsigned char *, REGS6502 *, int);
static unsigned char M6502PULLB(unsigned char *, REGS6502 *);
static unsigned int M6502PULLW(unsigned char *, REGS6502 *);

static unsigned char uc6502Cycles[256] =
                        {7,6,0,0,0,3,5,0,3,2,2,0,0,4,6,6, /* 00-0F */
                         2,5,0,0,0,4,6,6,2,4,0,0,0,5,7,7, /* 10-1F */
                         6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, /* 20-2F */
                         2,5,0,0,0,4,6,6,2,4,0,0,0,5,7,0, /* 30-3F */
                         6,6,0,0,0,3,5,5,3,2,2,0,3,4,6,0, /* 40-4F */
                         2,5,0,0,0,4,6,6,2,4,0,0,0,5,7,7, /* 50-5F */
                         6,6,0,0,0,3,5,5,4,2,2,0,5,4,6,0, /* 60-6F */
                         2,5,0,0,0,4,6,0,2,4,0,0,0,5,7,0, /* 70-7F */
                         0,6,0,0,3,3,3,0,2,0,2,0,4,4,4,0, /* 80-8F */
                         2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0, /* 90-9F */
                         2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, /* A0-AF */
                         2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0, /* B0-BF */
                         2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* C0-CF */
                         2,5,0,0,0,4,6,0,2,4,0,0,0,5,7,0, /* D0-DF */
                         2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* E0-EF */
                         2,5,0,0,0,4,6,0,2,4,0,0,0,5,7,0};/* F0-FF */

/* Sign and zero flags for quicker flag settings */
static unsigned char uc6502NZ[256]={
      F_ZERO,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,     /* 00-0F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 10-1F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 20-2F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 30-3F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 40-4F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 50-5F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 60-6F */
      0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,          /* 70-7F */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* 80-8F */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* 90-9F */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* A0-AF */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* B0-BF */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* C0-CF */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* D0-DF */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,          /* E0-EF */
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,
      F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE,F_NEGATIVE};         /* F0-FF */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : RESET6502(char *, REGS6502 *)                              *
 *                                                                          *
 *  PURPOSE    : Get the 6502 after a reset.                                *
 *                                                                          *
 ****************************************************************************/
void APIENTRY RESETNES6502(unsigned char *mem, REGS6502 *regs)
{
unsigned char *p;

   mem_map = mem;
   regs->ucRegA = 0;
   regs->ucRegX = 0;
   regs->ucRegY = 0;
   p = (unsigned char *)(0xfffc + regs->ulOffsets[(0xfffc >> 12)]);
   regs->usRegPC = p[0] + p[1] * 256; /* Start at reset vector */
   regs->ucRegP = F_ZERO | F_UNUSED | F_BREAK | F_IRQMASK;
   regs->ucRegS = 0xff;

} /* RESET6502() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502BIT(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a bit test and update flags.                       *
 *                                                                          *
 ****************************************************************************/
static void M6502BIT(REGS6502 *regs, unsigned char ucRegA, unsigned char ucByte)
{
unsigned char uc;

   regs->ucRegP &= ~(F_OVERFLOW | F_ZERO | F_NEGATIVE);
   uc = ucByte & (F_OVERFLOW | F_NEGATIVE); /* Top two bits of argument go right into flags */
   regs->ucRegP |= uc;
   uc = ucRegA & ucByte;
   if (uc == 0)
      regs->ucRegP |= F_ZERO;

} /* M6502BIT() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502INC(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform an increment and update flags.                     *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502INC(REGS6502 *regs, unsigned char ucByte)
{
   ucByte++;
   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
   regs->ucRegP |= uc6502NZ[ucByte];
   return ucByte;

} /* M6502INC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502DEC(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a decrement and update flags.                      *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502DEC(REGS6502 *regs, unsigned char ucByte)
{
   ucByte--;
   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
   regs->ucRegP |= uc6502NZ[ucByte];
   return ucByte;

} /* M6502DEC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ADC(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform a 8-bit addition with carry.                       *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502ADC(REGS6502 *regs, unsigned char ucRegA, unsigned char ucByte)
{
register unsigned short usTemp;
register unsigned char uc;
//unsigned char low, high;

   usTemp = (unsigned short)ucByte + (unsigned short)ucRegA + (regs->ucRegP & F_CARRY);
   regs->ucRegP &= ~(F_ZERO | F_OVERFLOW | F_NEGATIVE | F_CARRY);
   uc = ~(ucRegA ^ ucByte) & (ucRegA ^ usTemp) & 0x80;
   if (uc)
      regs->ucRegP |= F_OVERFLOW;
   if (usTemp & 0x100)
      regs->ucRegP |= F_CARRY;
   ucRegA = usTemp & 0xff;
   regs->ucRegP |= uc6502NZ[ucRegA];
   return ucRegA;

} /* M6502ADC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502SBC(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform a 8-bit subtraction with carry and update flags.   *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502SBC(REGS6502 *regs, unsigned char ucRegA, unsigned char ucByte)
{
register unsigned short usTemp;
register unsigned char uc;
//unsigned char low, high, ucOldFlags;

   usTemp = ucRegA - ucByte - (~regs->ucRegP & F_CARRY);
   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE | F_OVERFLOW | F_CARRY);
   uc = (ucRegA ^ ucByte) & (ucRegA ^ usTemp) & 0x80;
   if (uc)
      regs->ucRegP |= F_OVERFLOW;
   if (!(usTemp & 0x100))
      regs->ucRegP |= F_CARRY;
   ucRegA = usTemp & 0xff;
   regs->ucRegP |= uc6502NZ[ucRegA];
   return ucRegA;

} /* M6502SBC() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502CMP(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform a 8-bit comparison.                                *
 *                                                                          *
 ****************************************************************************/
static void M6502CMP(REGS6502 *regs, unsigned char ucByte1, unsigned char ucByte2)
{
   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE | F_CARRY);
   regs->ucRegP |= uc6502NZ[(ucByte1 - ucByte2) & 0xff];
   if (ucByte1 >= ucByte2)
       regs->ucRegP |= F_CARRY; // backwards from other CPUs

} /* M6502CMP() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502LSR(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a logical shift right and update flags.            *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502LSR(REGS6502 *regs, unsigned char ucByte)
{

   regs->ucRegP &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
   regs->ucRegP |= (ucByte & F_CARRY);
   ucByte >>= 1;
   if (ucByte == 0)
      regs->ucRegP |= F_ZERO;
   return ucByte;

} /* M6502LSR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ASL(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a arithmetic shift left and update flags.          *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502ASL(REGS6502 *regs, unsigned char ucByte)
{
//unsigned short usOld = (unsigned short)ucByte;

   regs->ucRegP &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
   if (ucByte & 0x80)
      regs->ucRegP |= F_CARRY;
   ucByte <<=1;
   regs->ucRegP |= uc6502NZ[ucByte];
   return ucByte;

} /* M6502ASL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ROL(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a rotate left and update flags.                    *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502ROL(REGS6502 *regs, unsigned char ucByte)
{
//unsigned char ucOld = ucByte;
unsigned char uc;

   uc = regs->ucRegP & 1; /* Preserve old carry flag */
   regs->ucRegP &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
   if (ucByte & 0x80)
      regs->ucRegP |= F_CARRY;
   ucByte = (ucByte <<1) | uc;
   regs->ucRegP |= uc6502NZ[ucByte];
   return ucByte;

} /* M6502ROL() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502EOR(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform an exclusive or and update flags.                  *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502EOR(REGS6502 *regs, unsigned char ucByte1, char ucByte2)
{
register unsigned char ucTemp;

   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
   ucTemp = ucByte1 ^ ucByte2;
   regs->ucRegP |= uc6502NZ[ucTemp];
   return ucTemp;

} /* M6502EOR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502OR(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform an inclusive or and update flags.                  *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502OR(REGS6502 *regs, unsigned char ucByte1, char ucByte2)
{
register unsigned char ucTemp;

   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
   ucTemp = ucByte1 | ucByte2;
   regs->ucRegP |= uc6502NZ[ucTemp];
   return ucTemp;

} /* M6502OR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502AND(REGS6502 *, char, char)                           *
 *                                                                          *
 *  PURPOSE    : Perform an AND and update flags.                           *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502AND(REGS6502 *regs, unsigned char ucByte1, char ucByte2)
{
register unsigned char ucTemp;

   regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
   ucTemp = ucByte1 & ucByte2;
   regs->ucRegP |= uc6502NZ[ucTemp];
   return ucTemp;

} /* M6502AND() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ROR(REGS6502 *, char)                                 *
 *                                                                          *
 *  PURPOSE    : Perform a rotate right and update flags.                   *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502ROR(REGS6502 *regs, unsigned char ucByte)
{
unsigned char uc;

   uc = regs->ucRegP & F_CARRY; /* Preserve old carry flag */
   regs->ucRegP &= ~(F_ZERO | F_CARRY | F_NEGATIVE);
   if (ucByte & 0x01)
      regs->ucRegP |= F_CARRY;
   ucByte = (ucByte >> 1) | (uc << 7);
   regs->ucRegP |= uc6502NZ[ucByte];
   return ucByte;

} /* M6502ROR() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502WriteByte(char *, short, char)                        *
 *                                                                          *
 *  PURPOSE    : Write a byte to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
static void M6502WriteByte(unsigned char *mem_map, unsigned int usAddr, unsigned char ucByte)
{
unsigned char c;

   if (mem_map[usAddr+MEM_FLAGS] & 0x80) /* If special flag (ROM or hardware) */
      {
      c = mem_map[usAddr+MEM_FLAGS] & 0x3f;
      if (c == 0x3f) /* ROM */
         return;
//      *pExternalClock = iClocks; // for DAC timing
      (mem_handlers[c].pfn_write)(usAddr, ucByte);
      if (usAddr >= 0x5000)
         {
         // re-establish PC pointer in case a ROM bank was switched
         PC = (unsigned short)(pPC - pSegment);
         pSegment = (unsigned char *)ulCPUOffs[PC >> 12]; // get the indirect offset of this 4K block for faster bank switching
         pPC = &pSegment[PC];
         }
      }
   else
      {
      *(unsigned char *)(usAddr + ulCPUOffs[usAddr >> 12]) = ucByte; // get the indirect offset of this 4K block for faster bank switching
      }

} /* M6502WriteByte() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502FlagsNZ8(M6502REGS *, char)                           *
 *                                                                          *
 *  PURPOSE    : Set appropriate flags for 8 bit value.                     *
 *                                                                          *
 ****************************************************************************/
static void M6502FlagsNZ8(REGS6502 *regs, unsigned char ucByte)
{
    regs->ucRegP &= ~(F_ZERO | F_NEGATIVE);
    regs->ucRegP |= uc6502NZ[ucByte];

} /* M6502FlagsNZ8() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ReadByte(char *, short)                               *
 *                                                                          *
 *  PURPOSE    : Read a byte from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502ReadByte(unsigned char *mem_map, unsigned int usAddr)
{
unsigned char c;

   if (mem_map[usAddr+MEM_FLAGS] & 0x40) /* If special flag (ROM or hardware) */
      {
      c = mem_map[usAddr+MEM_FLAGS] & 0x3f;
      return (mem_handlers[c].pfn_read)(usAddr);
      }
   else
      {
      return *(unsigned char *)(usAddr + ulCPUOffs[usAddr >> 12]); // get the indirect offset of this 4K block for faster bank switching
      }

} /* M6502ReadByte() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502ReadWord(char *, short)                               *
 *                                                                          *
 *  PURPOSE    : Read a word from memory, check for hardware.               *
 *                                                                          *
 ****************************************************************************/
static unsigned short M6502ReadWord(unsigned char *mem_map, unsigned int usAddr)
{
unsigned short usWord;

   usWord = M6502ReadByte(mem_map, usAddr++);
   if (!(usAddr & 0xff)) // account for wrap-around of page boundaries
      usAddr -= 0x100;
   usWord += M6502ReadByte(mem_map, usAddr) * 256;
   return usWord;

} /* M6502ReadWord() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502PUSHB(char *, REGS6502 *, char)                       *
 *                                                                          *
 *  PURPOSE    : Push a byte to the 'S' stack.                              *
 *                                                                          *
 ****************************************************************************/
static void M6502PUSHB(unsigned char *mem_map, REGS6502 *regs, unsigned char ucByte)
{
   mem_map[regs->ucRegS-- + 0x100] = ucByte;

} /* M6502PUSHB() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502PUSHW(char *, REGS6502 *)                             *
 *                                                                          *
 *  PURPOSE    : Push a word to the 'S' stack.                              *
 *                                                                          *
 ****************************************************************************/
static void M6502PUSHW(unsigned char *mem_map, REGS6502 *regs, int usWord)
{
// Doing it this way is necessary because many games depend on the stack pointer wrapping around
   mem_map[regs->ucRegS-- + 0x100] = (unsigned char)(usWord >> 8);
   mem_map[regs->ucRegS-- + 0x100] = (unsigned char)usWord;

} /* M6502PUSHW() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502PULLB(char *, REGS6502 *)                             *
 *                                                                          *
 *  PURPOSE    : Pull a byte from the 'S' stack.                            *
 *                                                                          *
 ****************************************************************************/
static unsigned char M6502PULLB(unsigned char *mem_map, REGS6502 *regs)
{
   return mem_map[0x100 + ++regs->ucRegS];

} /* M6502PULLB() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : M6502PULLW(char *, REGS6502 *)                             *
 *                                                                          *
 *  PURPOSE    : Pull a word from the 'S' stack.                            *
 *                                                                          *
 ****************************************************************************/
static unsigned int M6502PULLW(unsigned char *mem_map, REGS6502 *regs)
{
unsigned int iRet;

// Doing it this way is necessary because many games depend on the stack pointer wrapping around
   iRet = mem_map[0x100 + ++regs->ucRegS];
   iRet += (mem_map[0x100 + ++regs->ucRegS] << 8);
   return iRet;

} /* M6502PULLW() */

static int CheckInterrupts02(unsigned char *m_map02, REGS6502 *regs, int PC, unsigned char *ucIRQs, int *iExtraClocks)
{
   *iExtraClocks = 0;
   if (*ucIRQs & INT_NMI) /* NMI is highest priority */
      {
      M6502PUSHW(m_map02, regs, PC);
      M6502PUSHB(m_map02, regs, (char)(regs->ucRegP & ~F_BREAK));
      regs->ucRegP &= ~F_DECIMAL;
      regs->ucRegP |= F_IRQMASK;
      *iExtraClocks = 7;
      PC = M6502ReadWord(m_map02, 0xfffa);
      *ucIRQs &= ~INT_NMI; /* clear this bit */
      return PC;
      }
   if (*ucIRQs & INT_IRQ && (regs->ucRegP & F_IRQMASK) == 0) /* IRQ is lowest priority */
      {
      M6502PUSHW(m_map02, regs, PC);
      M6502PUSHB(m_map02, regs, (char)(regs->ucRegP & ~F_BREAK));
      regs->ucRegP |= F_IRQMASK; /* Mask interrupts during service routine */
      regs->ucRegP &= ~F_DECIMAL;
      PC = M6502ReadWord(m_map02, 0xfffe);
      *ucIRQs &= ~INT_IRQ; /* clear this bit */
      *iExtraClocks = 7;
      }
   return PC;

} /* CheckInterrupts02() */
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EXEC6502(char *, REGS6502 *, EMUHANDLERS *, int *, char)   *
 *                                                                          *
 *  PURPOSE    : Emulate the M6502 microprocessor for N clock cycles.       *
 *                                                                          *
 ****************************************************************************/
void APIENTRY EXECNES6502(unsigned char *mem, REGS6502 *regs, EMUHANDLERS *emuh, int *iClockCount, unsigned char *ucIRQs)
{
unsigned short usAddr; /* Temp address */
unsigned char ucRegA, ucRegX, ucRegY, ucTemp, ucAddr;
//static unsigned long ulTraceCount = 0;
int iExtraTicks; // local var for speed
int iCLITicks; // extra ticks for allowing 1 instruction to execute after CLI

   mem_handlers = emuh; /* Assign to static for faster execution */
   mem_map = mem; /* ditto */

   PC = regs->usRegPC;
   iCLITicks = 0;
   ucRegA = regs->ucRegA;
   ucRegY = regs->ucRegY;
   ucRegX = regs->ucRegX;
   pExternalClock = iClockCount;
   iClocks = *iClockCount;
   ulCPUOffs = &regs->ulOffsets[0]; // assign to local var
top_execution_loop:
   PC = CheckInterrupts02(mem, regs, PC, ucIRQs, &iExtraTicks);
   iClocks -= iExtraTicks;
   pSegment = (unsigned char *)ulCPUOffs[PC >> 12]; // get the indirect offset of this 4K block for faster bank switching
   pPC = &pSegment[PC];
   while (iClocks > 0) /* Execute for the amount of time alloted */
      {
#ifdef _DEBUG
      if (bTrace)
         {
         PC = (unsigned short)(pPC - pSegment);
         if (PC == 0xc3a5)
            uc6502Opcode = 0;
         regs->usRegPC = PC;
         regs->ucRegA = ucRegA;
         regs->ucRegX = ucRegX;
         regs->ucRegY = ucRegY;
         TRACE6502(regs, iClocks);
         ulTraceCount++;
         }
#endif
//      PC = (unsigned short)(pPC - pSegment);
//      pSegment = (unsigned char *)ulCPUOffs[PC >> 12]; // get the indirect offset of this 4K block for faster bank switching
//      pPC = &pSegment[PC];
      uc6502Opcode = *pPC++;
      iClocks -= (int)uc6502Cycles[uc6502Opcode]; /* Subtract execution time of this instruction */
      switch(uc6502Opcode)
         {
         case 0x00: /* BRK - software interrupt */
            pPC++; // BRK is really a two byte instruction
            M6502PUSHW(mem_map, regs, (unsigned short)(pPC-pSegment));
            M6502PUSHB(mem_map, regs, (unsigned char)(regs->ucRegP | F_BREAK));
            regs->ucRegP = (regs->ucRegP | F_IRQMASK) & ~F_DECIMAL;
            PC = M6502ReadWord(mem_map, 0xfffe);
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0x01: /* ORA - (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr |= (mem_map[ucTemp]<<8); // need to increment in case of wrap around
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502OR(regs, ucRegA, ucTemp);
            break;

         case 0x05: /* ORA - z */
            ucRegA = M6502OR(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0x06: /* ASL - z */
            mem_map[*pPC] = M6502ASL(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0x08: /* PHP - push flags */
            M6502PUSHB(mem_map, regs, regs->ucRegP);
            break;

         case 0x09: /* ORA - immediate */
            ucRegA = M6502OR(regs, ucRegA, *pPC++);
            break;

         case 0x0a: /* ASLA */
            ucRegA = M6502ASL(regs, ucRegA);
            break;

         case 0x0d: /* ORA - absolute */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucRegA = M6502OR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x0e: /* ASL - absolute */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ASL(regs, ucTemp));
            break;

         case 0x10: /* BPL */
            pPC++;
            if (!(regs->ucRegP & F_NEGATIVE))
               {
               iClocks -= 1;
#ifdef SPEED_HACKS
               if (pPC[-1] >= 0xfb) // we are taking a branch to the previous instruction, obviously a semaphore waiting for some event
                  iClocks = 0; // we can exit this timeslice since the semaphore can't change now
#endif
               pPC += (signed char)pPC[-1];
               }
            break;

         case 0x11: /* ORA (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502OR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x15: /* ORA z,x */
            ucTemp = *pPC++ + ucRegX;
            ucRegA = M6502OR(regs, ucRegA, mem_map[ucTemp]);
            break;

         case 0x16: /* ASL z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502ASL(regs, mem_map[ucAddr]);
            break;

         case 0x18: /* CLC */
            regs->ucRegP &= ~F_CARRY;
            break;

         case 0x19: /* ORA abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegA = M6502OR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x1d: /* ORA abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegA = M6502OR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x1e: /* ASL abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ASL(regs, ucTemp));
            break;

         case 0x20: /* JSR abs */
            // PUSH PC + 1
            M6502PUSHW(mem_map, regs, (unsigned short)(pPC-pSegment+1));
            PC = pPC[0] + pPC[1]*256;
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0x21: /* AND (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr |= (mem_map[ucTemp]<<8); // need to increment in case of wrap around
            ucRegA = M6502AND(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x24: /* BIT z */
            M6502BIT(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0x25: /* AND z */
            ucRegA = M6502AND(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0x26: /* ROL z */
            mem_map[*pPC] = M6502ROL(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0x28: /* PLP */
            regs->ucRegP = M6502PULLB(mem_map, regs) | F_UNUSED | F_BREAK;
            PC = (unsigned short)(pPC - pSegment);
            PC = CheckInterrupts02(mem, regs, PC, ucIRQs, &iExtraTicks);
            iClocks -= iExtraTicks;
            pSegment = (unsigned char *)ulCPUOffs[PC>>12];
            pPC = &pSegment[PC];
            break;

         case 0x29: /* AND - immediate */
            ucRegA = M6502AND(regs, ucRegA, *pPC++);
            break;

         case 0x2a: /* ROLA */
            ucRegA = M6502ROL(regs, ucRegA);
            break;

         case 0x2c: /* BIT abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            M6502BIT(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x2d: /* AND abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucRegA = M6502AND(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x2e: /* ROL abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ROL(regs, ucTemp));
            break;

         case 0x30: /* BMI */
            pPC++;
            if (regs->ucRegP & F_NEGATIVE)
               {
               iClocks -= 1;
#ifdef SPEED_HACKS
               if (pPC[-1] == 0xfc) // we are taking a branch to the previous instruction, obviously a semaphore waiting for some event
                  iClocks = 0; // we can exit this timeslice since the semaphore can't change now
#endif
               pPC += (signed char)pPC[-1];
               }
            break;

         case 0x31: /* AND (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502AND(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x35: /* AND z,x */
            ucAddr = *pPC++ + ucRegX;
            ucRegA = M6502AND(regs, ucRegA, mem_map[ucAddr]);
            break;

         case 0x36: /* ROL z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502ROL(regs, mem_map[ucAddr]);
            break;

         case 0x38: /* SEC */
            regs->ucRegP |= F_CARRY;
            break;

         case 0x39: /* AND abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegA = M6502AND(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x3D: /* AND abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegA = M6502AND(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x3E: /* ROL abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ROL(regs, ucTemp));
            break;

         case 0x40: /* RTI */
            regs->ucRegP = M6502PULLB(mem_map, regs) | F_UNUSED | F_BREAK;
            PC = M6502PULLW(mem_map, regs);
            PC = CheckInterrupts02(mem, regs, PC, ucIRQs, &iExtraTicks);
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            iClocks -= iExtraTicks;
            break;

         case 0x41: /* EOR (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr |= (mem_map[ucTemp]<<8); // need to increment in case of wrap around
            ucRegA = M6502EOR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x45: /* EOR z */
            ucRegA = M6502EOR(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0x46: /* LSR z */
            mem_map[*pPC] = M6502LSR(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0x48: /* PHA */
            M6502PUSHB(mem_map, regs, ucRegA);
            break;

         case 0x49: /* EOR - immediate */
            ucRegA = M6502EOR(regs, ucRegA, *pPC++);
            break;

         case 0x4A: /* LSR */
            ucRegA = M6502LSR(regs, ucRegA);
            break;

         case 0x4C: /* JMP abs */
            usAddr = pPC[0] + pPC[1]*256;
#ifdef SPEED_HACKS
            PC = (unsigned short)(pPC - pSegment);
            if (usAddr >= PC-4) // JMP to itself used to waste time waiting for an interrupt
               iClocks = 0; // exit this time slice
#endif
            PC = usAddr;
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0x4D: /* EOR abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502EOR(regs, ucTemp, ucRegA);
            break;

         case 0x4E: /* LSR abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502LSR(regs, ucTemp));
            break;

         case 0x50: /* BVC */
            pPC++;
            if (!(regs->ucRegP & F_OVERFLOW))
               {
               iClocks -= 1;
#ifdef SPEED_HACKS
               if (pPC[-1] >= 0xfc) // condition won't change in this time slice
                  iClocks = 0; // used by some games to wait for sprite collision (BIT abs)
#endif
               pPC += (signed char)pPC[-1];
               }
            break;

         case 0x51: /* EOR (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502EOR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x55: /* EOR z,x */
            ucAddr = *pPC++ + ucRegX;
            ucRegA = M6502EOR(regs, ucRegA, mem_map[ucAddr]);
            break;

         case 0x56: /* LSR z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502LSR(regs, mem_map[ucAddr]);
            break;

         case 0x58: /* CLI */
            regs->ucRegP &= ~F_IRQMASK;
            iCLITicks = iClocks - 1;
            iClocks = 1; // allow 1 more instruction to execute
            break;

         case 0x59: /* EOR abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegA = M6502EOR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x5D: /* EOR abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegA = M6502EOR(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x5E: /* LSR abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502LSR(regs, ucTemp));
            break;

         case 0x60: /* RTS */
            PC = M6502PULLW(mem_map, regs) + 1;
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0x61: /* ADC (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr |= (mem_map[ucTemp]<<8); // need to increment in case of wrap around
            ucRegA = M6502ADC(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x65: /* ADC z */
            ucRegA = M6502ADC(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0x66: /* ROR z */
            mem_map[*pPC] = M6502ROR(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0x68: /* PLA */
            ucRegA = M6502PULLB(mem_map, regs);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0x69: /* ADC - immediate */
            ucRegA = M6502ADC(regs, ucRegA, *pPC++);
            break;

         case 0x6A: /* ROR */
            ucRegA = M6502ROR(regs, ucRegA);
            break;

         case 0x6C: /* JMP (ind) */
            usAddr = pPC[0] + pPC[1]*256;
            PC = M6502ReadWord(mem_map, usAddr);
            pSegment = (unsigned char *)ulCPUOffs[PC >> 12];
            pPC = &pSegment[PC];
            break;

         case 0x6D: /* ADC abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502ADC(regs, ucRegA, ucTemp);
            break;

         case 0x6E: /* ROR abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ROR(regs, ucTemp));
            break;

         case 0x70: /* BVS */
            pPC++;
            if (regs->ucRegP & F_OVERFLOW)
               {
               pPC += (signed char)pPC[-1];
               iClocks -= 1;
               }
            break;

         case 0x71: /* ADC (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502ADC(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x75: /* ADC z,x */
            ucAddr = *pPC++ + ucRegX;
            ucRegA = M6502ADC(regs, ucRegA, mem_map[ucAddr]);
            break;

         case 0x76: /* ROR z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502ROR(regs, mem_map[ucAddr]);
            break;

         case 0x78: /* SEI */
            regs->ucRegP |= F_IRQMASK;
            break;

         case 0x79: /* ADC abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegA = M6502ADC(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x7D: /* ADC abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegA = M6502ADC(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0x7E: /* ROR abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502ROR(regs, ucTemp));
            break;

         case 0x81: /* STA (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr |= (mem_map[ucTemp]<<8); // need to increment in case of wrap around
            M6502WriteByte(mem_map, usAddr, ucRegA);
            break;

         case 0x84: /* STY z */
            mem_map[*pPC++] = ucRegY;
            break;

         case 0x85: /* STA z */
            mem_map[*pPC++] = ucRegA;
            break;

         case 0x86: /* STX z */
            mem_map[*pPC++] = ucRegX;
            break;

         case 0x88: /* DEY */
            ucRegY = M6502DEC(regs, ucRegY);
            break;

         case 0x8A: /* TXA */
            ucRegA = ucRegX;
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0x8C: /* STY abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            M6502WriteByte(mem_map, usAddr, ucRegY);
            break;

         case 0x8D: /* STA abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            M6502WriteByte(mem_map, usAddr, ucRegA);
            break;

         case 0x8E: /* STX abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            M6502WriteByte(mem_map, usAddr, ucRegX);
            break;

         case 0x90: /* BCC */
            pPC++;
            if (!(regs->ucRegP & F_CARRY))
               {
               pPC += (signed char)pPC[-1];
               iClocks -= 1;
               }
            break;

         case 0x91: /* STA (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            M6502WriteByte(mem_map, usAddr, ucRegA);
            break;

         case 0x94: /* STY z,x */
            ucTemp = *pPC++ + ucRegX;
            mem_map[ucTemp] =ucRegY;
            break;

         case 0x95: /* STA z,x */
            ucTemp = *pPC++ + ucRegX;
            mem_map[ucTemp] = ucRegA;
            break;

         case 0x96: /* STX z,y */
            ucTemp = *pPC++ + ucRegY;
            mem_map[ucTemp] = ucRegX;
            break;

         case 0x98: /* TYA */
            ucRegA = ucRegY;
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0x99: /* STA abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            M6502WriteByte(mem_map, usAddr, ucRegA);
            break;

         case 0x9A: /* TXS */
            regs->ucRegS = ucRegX;
            break;

         case 0x9D: /* STA abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            M6502WriteByte(mem_map, usAddr, ucRegA);
            break;

         case 0xA0: /* LDY - immediate */
            ucRegY = *pPC++;
            M6502FlagsNZ8(regs, ucRegY);
            break;

         case 0xA1: /* LDA (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr += 256 * mem_map[ucTemp]; /* In case of wrap-around */
            ucRegA = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xA2: /* LDX - immediate */
            ucRegX = *pPC++;
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xA4: /* LDY z */
            ucRegY = mem_map[*pPC++];
            M6502FlagsNZ8(regs, ucRegY);
            break;

         case 0xA5: /* LDA z */
            ucRegA = mem_map[*pPC++];
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xA6: /* LDX z */
            ucRegX = mem_map[*pPC++];
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xA8: /* TAY */
            ucRegY = ucRegA;
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xA9: /* LDA - immediate */
            ucRegA = *pPC++;
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xAA: /* TAX */
            ucRegX = ucRegA;
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xAC: /* LDY abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucRegY = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegY);
            break;

         case 0xAD: /* LDA abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucRegA = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xAE: /* LDX abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucRegX = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xB0: /* BCS */
            pPC++;
            if (regs->ucRegP & F_CARRY)
               {
               pPC += (signed char)pPC[-1];
               iClocks -= 1;
               }
            break;

         case 0xB1: /* LDA (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xB4: /* LDY z,x */
            ucTemp = *pPC++ + ucRegX;
            ucRegY = mem_map[ucTemp];
            M6502FlagsNZ8(regs, ucRegY);
            break;

         case 0xB5: /* LDA z,x */
            ucTemp = *pPC++ + ucRegX;
            ucRegA = mem_map[ucTemp];
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xB6: /* LDX z,y */
            ucTemp = *pPC++ + ucRegY;
            ucRegX = mem_map[ucTemp];
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xB8: /* CLV */
            regs->ucRegP &= ~F_OVERFLOW;
            break;

         case 0xB9: /* LDA abs, y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegA = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xBA: /* TSX */
            ucRegX = regs->ucRegS;
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xBC: /* LDY abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegY = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegY);
            break;

         case 0xBD: /* LDA abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucRegA = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegA);
            break;

         case 0xBE: /* LDX abs, y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucRegX = M6502ReadByte(mem_map, usAddr);
            M6502FlagsNZ8(regs, ucRegX);
            break;

         case 0xC0: /* CPY - immediate */
            M6502CMP(regs, ucRegY, *pPC++);
            break;

         case 0xC1: /* CMP (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr += 256 * mem_map[ucTemp]; /* In case of wrap-around */
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502CMP(regs, ucRegA, ucTemp);
            break;

         case 0xC4: /* CPY z */
            M6502CMP(regs, ucRegY, mem_map[*pPC++]);
            break;

         case 0xC5: /* CMP z */
            M6502CMP(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0xC6: /* DEC z */
            mem_map[*pPC] = M6502DEC(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0xC8: /* INY */
            ucRegY = M6502INC(regs, ucRegY);
            break;

         case 0xC9: /* CMP - immediate */
            M6502CMP(regs, ucRegA, *pPC++);
            break;

         case 0xCA: /* DEX */
            ucRegX = M6502DEC(regs, ucRegX);
            break;

         case 0xCC: /* CPY abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502CMP(regs, ucRegY, ucTemp);
            break;

         case 0xCD: /* CMP abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502CMP(regs, ucRegA, ucTemp);
            break;

         case 0xCE: /* DEC abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502DEC(regs, ucTemp));
            break;

         case 0xD0: /* BNE */
            pPC++;
            if (!(regs->ucRegP & F_ZERO))
               {
#ifdef SPEED_HACKS
               if (pPC[-1] == 0xfc) // we are taking a branch to the previous instruction, obviously a semaphore waiting for some event
                  iClocks = 0; // we can exit this timeslice since the semaphore can't change now
#endif
               pPC += (signed char)pPC[-1];
               iClocks -= 1;
               }
            break;

         case 0xD1: /* CMP (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            M6502CMP(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0xD5: /* CMP z,x */
            ucAddr = *pPC++ + ucRegX;
            M6502CMP(regs, ucRegA, mem_map[ucAddr]);
            break;

         case 0xD6: /* DEC z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502DEC(regs, mem_map[ucAddr]);
            break;

         case 0xD8: /* CLD */
            regs->ucRegP &= ~F_DECIMAL;
            break;

         case 0xD9: /* CMP abs,y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            M6502CMP(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0xDD: /* CMP abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            M6502CMP(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0xDE: /* DEC abs,x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502DEC(regs, ucTemp));
            break;

         case 0xE0: /* CPX - immediate */
            M6502CMP(regs, ucRegX, *pPC++);
            break;

         case 0xE1: /* SBC (z,x) */
            ucTemp = *pPC++ + ucRegX;
            usAddr = mem_map[ucTemp++];
            usAddr += 256 * mem_map[ucTemp]; /* In case of wrap-around */
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502SBC(regs, ucRegA, ucTemp);
            break;

         case 0xE4: /* CPX z */
            M6502CMP(regs, ucRegX, mem_map[*pPC++]);
            break;

         case 0xE5: /* SBC z */
            ucRegA = M6502SBC(regs, ucRegA, mem_map[*pPC++]);
            break;

         case 0xE6: /* INC z */
            mem_map[*pPC] = M6502INC(regs, mem_map[*pPC]);
            pPC++;
            break;

         case 0xE8: /* INX */
            ucRegX = M6502INC(regs, ucRegX);
            break;

         case 0xE9: /* SBC - immediate */
            ucRegA = M6502SBC(regs, ucRegA, *pPC++);
            break;

         case 0xEA: /* NOP */
            break;

         case 0xEC: /* CPX abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502CMP(regs, ucRegX, ucTemp);
            break;

         case 0xED: /* SBC abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502SBC(regs, ucRegA, ucTemp);
            break;

         case 0xEE: /* INC abs */
            usAddr = pPC[0] + pPC[1]*256;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502INC(regs, ucTemp));
            break;

         case 0xF0: /* BEQ */
            pPC++;
            if (regs->ucRegP & F_ZERO)
               {
#ifdef SPEED_HACKS
               if (pPC[-1] == 0xfc) // we are taking a branch to the previous instruction, obviously a semaphore waiting for some event
                  iClocks = 0; // we can exit this timeslice since the semaphore can't change now
#endif
               pPC += (signed char)pPC[-1];
               iClocks -= 1;
               }
            break;

         case 0xF1: /* SBC (z),y */
            ucTemp = *pPC++;
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucRegA = M6502SBC(regs, ucRegA, M6502ReadByte(mem_map, usAddr));
            break;

         case 0xF3: // ISC // increment memory and subtrace from accumulator
            ucTemp = *pPC++; // get argument
            usAddr = mem_map[ucTemp++]; /* Account for zp wrap-around */
            usAddr += 256 * mem_map[ucTemp] + ucRegY;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucTemp++;
            M6502WriteByte(mem_map, usAddr, ucTemp);
            ucRegA = M6502SBC(regs, ucRegA, ucTemp);            
            break;
            
         case 0xF5: /* SBC z,x */
            ucAddr = *pPC++ + ucRegX;
            ucRegA = M6502SBC(regs, ucRegA, mem_map[ucAddr]);
            break;

         case 0xF6: /* INC z,x */
            ucAddr = *pPC++ + ucRegX;
            mem_map[ucAddr] = M6502INC(regs, mem_map[ucAddr]);
            break;

         case 0xF8: /* SED */
            regs->ucRegP |= F_DECIMAL;
            break;

         case 0xF9: /* SBC abs, y */
            usAddr = pPC[0] + pPC[1]*256 + ucRegY;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502SBC(regs, ucRegA, ucTemp);
            break;

         case 0xFD: /* SBC abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            ucRegA = M6502SBC(regs, ucRegA, ucTemp);
            break;

         case 0xFE: /* INC abs, x */
            usAddr = pPC[0] + pPC[1]*256 + ucRegX;
            pPC += 2;
            ucTemp = M6502ReadByte(mem_map, usAddr);
            M6502WriteByte(mem_map, usAddr, M6502INC(regs, ucTemp));
            break;

         default: /* Illegal instruction */
            iClocks = 0;
            break;
         } /* switch */
      } /* while iClocks */
   if (iCLITicks) // we just executed CLI, allow execution to continue
      {
      iClocks += iCLITicks; // adjust remaining ticks
      iCLITicks = 0; // reset flag
      PC = (unsigned short)(pPC - pSegment);
      goto top_execution_loop; // check interrupts
      }

   regs->usRegPC = (unsigned short)(pPC - pSegment);
   regs->ucRegA = ucRegA;
   regs->ucRegX = ucRegX;
   regs->ucRegY = ucRegY; // put register vars back in the structure

} /* EXEC6502() */
