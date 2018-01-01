/***********************************************************/
/* Code to emulate the Nintendo Entertainment System (NES) */
/*                                                         */
/* Written by Larry Bank                                   */
/* Copyright 2004-2017 BitBank Software, Inc.              */
/* Work started 7/1/04                                     */
/***********************************************************/
// Change log
// 1/1/2006 - changed sprite drawing to only draw active sprites on scanlines with visible sprites
// 12/28/2007 - added mappers #9,#10,#69
// 1/1/2008 - fixed 6502 to work with mapper 7 (all of ROM banked)
// 1/1/2008 - added mapper #11, #71
// 1/1/2008 - added back IORead handler at 0x2002 - without it, many games mess up (e.g. BattleToads, codemaster games)
// 1/1/2008 - clead up mapper 16 (Dragon Ball series)
// 1/1/2008 - fixed save/load code to properly restore VROM offsets, changed save file size
// 1/2/2008 - added mapper #67, fixed Dragon warrior 4 (bad dump), and Spy Hunter (wrong mapper #)
// 1/7/2008 - added mapper 25, 43, 226,255
// 2/28/2010 - fixed DAC sounds by removing wrong event when write occurs
// 3/1/2010 - cleaned up sound by clipping to min/max values
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "emuio.h"
#include "smartgear.h"
#include "emu.h"
#include "sound.h"

BOOL bHideOverscan;
extern int iCheatCount, iaCheatAddr[], iaCheatValue[], iaCheatCompare[];

//#define _USE_ARM_ASM

void ARESETNES6502(unsigned char *mem, REGS6502 *regs);
void AEXECNES6502(unsigned char *memmap, REGS6502 *regs, EMUHANDLERS *emuh, int *cpuClocks, unsigned char *ucIRQs);
void SG_GetLeafName(char *fname, char *leaf);

typedef struct tagNESMachine
{
unsigned char *pVRAM;
unsigned char *pVROM; // for restoring saved game
unsigned char *pBankROM; // for restoring saved game
unsigned char *pRAMBank;
unsigned char *mem_map;
BOOL bIRQCounter;
BOOL bLowerAddr;
BOOL bVideoEnable;
BOOL bSpriteEnable;
BOOL bVBlankIntEnable;
BOOL bHitIntEnable;
BOOL bHideCol1;
BOOL bRAMEnable;
BOOL bSunToggle;
BOOL bMapperLatch;
uint32_t ulPalFlags;
unsigned long ulVROMOffsets[8];
unsigned long ulVRAMOffsets[8];
unsigned short usPalConvert[128];
EMUHANDLERS *pEMUH; /* memory handlers */
int iROMPages;
int iVROMPages;
int iMapper;
int iJoyShift1;
int iJoyShift2;
int iScrollX;
int iScrollY;
unsigned long iVRAMAddr;
unsigned long iVRAMAddrTemp;
int iVDPIncrement;
int iDMACycles;
int iIRQCount;
int iIRQStartCount;
int iIRQTempCount;
int iIRQLatch;
int iSpriteSize;
int iNameTable;
int iPatternTable;
int iSpritePattern;
int iRefreshLatch;
int iRefreshAddr;
int iSndLen1;
int iSndLen2;
int iSndLen3;
int iSndLen4;
int iFreq1;
int iFreq2;
int iFreq3;
int iFreq4;
int iSnd1Pos;
int iSnd2Pos;
int iSnd3Pos;
int iNoisePos;
int iNoisePoly;
int iNoiseMask;
int iDMCMode;
int iDMCStartCount;
int iDMCCount;
unsigned long iDMCAddr;
unsigned long iDMCStartAddr;
int iDMCLen;
int iDMCStartLen;
int iDMCBit;
int iDMCFreq;
int iSweepDelta1;
int iSweepDelta2;
int iSweepPos1;
int iSweepPos2;
int iEnvVol1;
int iEnvVol2;
int iEnvVol4;
int iEnvPos1;
int iEnvPos2;
int iEnvPos4;
int iEnvDelta1;
int iEnvDelta2;
int iEnvDelta4;
unsigned char ucSpriteRAM[256];
unsigned char ucPRGBanks[8];
unsigned char ucCHRBanks[8];
unsigned char ucJoyButtons[2];
unsigned char ucVRAMBuffer;
unsigned char ucSpriteAddr;
unsigned char ucIRQs;
unsigned char ucIRQLatch;
unsigned char ucCartCMD;
unsigned char ucCartShift;
unsigned char ucCartLatch;
unsigned char ucCartOldIndex;
unsigned char ucSoundChannels;
unsigned char ucSndCtrl1;
unsigned char ucSndCtrl2;
unsigned char ucSndCtrl3;
unsigned char ucSndCtrl4;
unsigned char ucLinearCount;
unsigned char ucLinearCounter;
unsigned char ucDMCByte;
unsigned char ucSweep1;
unsigned char ucSweep2;

REGS6502 regs6502;
} NESMACHINE;

void NESSetMapperHandlers(NESMACHINE *pNESTemp);

NESMACHINE nesmachine, *pNESMachine;
//void * ohandle;
int iNESRefCount = 0;
//static int iAudioRate;
extern unsigned char *pSoundBuf;
static BOOL bTrace, bSpriteChanged;
static int iTrace, iSpriteCount;
unsigned char ucSpriteDraw[224], ucSpriteList[64];
extern int iScreenX, iScreenY;
static int iClocks;
static EMU_EVENT_QUEUE *pEEQ; // for queueing DAC sounds
extern BOOL bRegistered;
//extern HBITMAP hbmScreen;
//extern HDC hdcMem;
extern BOOL b16BitAudio, bStereo, bRunning, bAutoLoad;
extern char pszROMS[], pszSAVED[], pszCAPTURE[];
extern uint32_t *pTempBitmap;
extern int iScale;
extern unsigned char *pScreen;
extern int iScreenPitch;
unsigned char *pBitmap;
//static unsigned char *pBackground;
static BOOL bVBlank, bSpriteCollision, b8Sprites;
//extern HPALETTE hpalEMU;
extern int iPitch, iScreenBpp;
static int iTotalClocks;
// Sound Variables
signed short *psAudioBuf;
unsigned char *pNoise7, *pNoise15; // polynomial noise tables
// This represents the number of CPU clocks (at 1.79Mhz) between 8-bit DMC samples
int iDMCTranslate[16] = {0xd60, 0xbe0, 0xaa0, 0xa00, 0x8f0, 0x7f0, 0x710, 0x6b0, 0x5f0, 0x500, 0x470, 0x400, 0x350, 0x2a8, 0x240, 0x1b0};
void ARM816FAST(unsigned char *pSrc, uint32_t *pDest, unsigned short *pColorConvert, int iCount);
void ARMDrawNESTiles(uint32_t *, int, unsigned char *);

//extern unsigned short usPalConvert[]; // temporary data for game to read back
//extern char szGameName[];
void NESIOWrite(int, unsigned char);
void NESWRAMWrite(int, unsigned char);
void NESMapper11(int iAddr, unsigned char ucByte);
void NESMapper3(int, unsigned char);
void NESMapper69(int, unsigned char);
void NESMapper5(int, unsigned char);
void NESMapper9(int, unsigned char);
void NESMapper10(int, unsigned char);
void NESMapper10SetPages(BOOL);
unsigned char NESIORead(int);
unsigned char NESWRAMRead(int);
unsigned char NESRAMRead(int);
unsigned char NULLMapperRead(int);
void NULLMapperWrite(int, unsigned char);
void NESRAMWrite(int, unsigned char);
void EMUSetupHandlers(unsigned char *mem_map, EMUHANDLERS *emuh, MEMHANDLERS *mhHardware);
// Translate sound length value into frames
unsigned char ucSndLenTranslate[32] = {
0x05, 0x7f, 0x0a, 0x01, 0x14, 0x02, 0x28, 0x03, 0x50, 0x04, 0x1e, 0x05, 0x07, 0x06, 0x0d, 0x07,
0x06, 0x08, 0x0c, 0x09, 0x18, 0x0a, 0x30, 0x0b, 0x60, 0x0c, 0x24, 0x0d, 0x08, 0x0e, 0x10, 0x0f};
unsigned char ucSquareWaves[32] = {63,0,0,0,0,0,0,0,       // 12.5%
                              63,63,0,0,0,0,0,0,      // 25%
                              63,63,63,63,0,0,0,0,    // 50%
                              63,63,63,63,63,63,0,0}; // 75%
unsigned char ucTriangles[32] = {0x3c,0x38,0x34,0x30,0x2c,0x28,0x24,0x20,0x1c,0x18,0x14,0x10,0xc,0x8,0x4,0x0,
                                 0x0,0x4,0x8,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,0x30,0x34,0x38,0x3c};
int iNoiseFreq[16] = {0x02, 0x04, 0x08,0x10,0x20,0x30,0x40,0x50,0x65,0x7f,0xbe,0xfe,0x17d,0x1fc,0x3f9,0x7f2};

MEMHANDLERS mhNES[] = {{0x2000, 0x3000, NESIORead, NESIOWrite},
                       {0x6000, 0x2000, NESWRAMRead, NESWRAMWrite},
                       {0x8000, 0x8000, NULL, NESMapper3},
                       {0x5000, 0x1000, NULLMapperRead, NULLMapperWrite}, // place holder for some mappers
                       {0x800, 0x1800, NESRAMRead, NESRAMWrite}, // for mirrored SRAM at 0-7FF
                       {0,0,NULL,NULL}};

unsigned char ucNESPalette[64*3] = {
0x80,0x80,0x80, 0x00,0x3D,0xA6, 0x00,0x12,0xB0, 0x44,0x00,0x96,
0xA1,0x00,0x5E, 0xC7,0x00,0x28, 0xBA,0x06,0x00, 0x8C,0x17,0x00,
0x5C,0x2F,0x00, 0x10,0x45,0x00, 0x05,0x4A,0x00, 0x00,0x47,0x2E,
0x00,0x41,0x66, 0x00,0x00,0x00, 0x05,0x05,0x05, 0x05,0x05,0x05,
0xC7,0xC7,0xC7, 0x00,0x77,0xFF, 0x21,0x55,0xFF, 0x82,0x37,0xFA,
0xEB,0x2F,0xB5, 0xFF,0x29,0x50, 0xFF,0x22,0x00, 0xD6,0x32,0x00,
0xC4,0x62,0x00, 0x35,0x80,0x00, 0x05,0x8F,0x00, 0x00,0x8A,0x55,
0x00,0x99,0xCC, 0x21,0x21,0x21, 0x09,0x09,0x09, 0x09,0x09,0x09,
0xFF,0xFF,0xFF, 0x0F,0xD7,0xFF, 0x69,0xA2,0xFF, 0xD4,0x80,0xFF,
0xFF,0x45,0xF3, 0xFF,0x61,0x8B, 0xFF,0x88,0x33, 0xFF,0x9C,0x12,
0xFA,0xBC,0x20, 0x9F,0xE3,0x0E, 0x2B,0xF0,0x35, 0x0C,0xF0,0xA4,
0x05,0xFB,0xFF, 0x5E,0x5E,0x5E, 0x0D,0x0D,0x0D, 0x0D,0x0D,0x0D,
0xFF,0xFF,0xFF, 0xA6,0xFC,0xFF, 0xB3,0xEC,0xFF, 0xDA,0xAB,0xEB,
0xFF,0xA8,0xF9, 0xFF,0xAB,0xB3, 0xFF,0xD2,0xB0, 0xFF,0xEF,0xA6,
0xFF,0xF7,0x9C, 0xD7,0xE8,0x95, 0xA6,0xED,0xAF, 0xA2,0xF2,0xDA,
0x99,0xFF,0xFC, 0xDD,0xDD,0xDD, 0x11,0x11,0x11, 0x11,0x11,0x11};
 
// Byte expansion tables used to convert tiles with separate bit planes into composite 8bpp images
// each 16-entry table converts a nibble into a 4-byte long.  These are then shifted over to all 8 bit positions
// Each 4-byte entry appears mirrored because on a little-endian machine the bytes are stored backwards
static uint32_t ulExpand0[16] = {
0x00000000,0x01000000,0x00010000,0x01010000,0x00000100,0x01000100,0x00010100,0x01010100,
0x00000001,0x01000001,0x00010001,0x01010001,0x00000101,0x01000101,0x00010101,0x01010101};
static uint32_t ulExpand1[16] = {
0x00000000,0x02000000,0x00020000,0x02020000,0x00000200,0x02000200,0x00020200,0x02020200,
0x00000002,0x02000002,0x00020002,0x02020002,0x00000202,0x02000202,0x00020202,0x02020202};
static uint32_t ulExpandMirror0[16] = {
0x00000000,0x00000001,0x00000100,0x00000101,0x00010000,0x00010001,0x00010100,0x00010101,
0x01000000,0x01000001,0x01000100,0x01000101,0x01010000,0x01010001,0x01010100,0x01010101};
static uint32_t ulExpandMirror1[16] = {
0x00000000,0x00000002,0x00000200,0x00000202,0x00020000,0x00020002,0x00020200,0x00020202,
0x02000000,0x02000002,0x02000200,0x02000202,0x02020000,0x02020002,0x02020200,0x02020202};

static int iScanLine;
extern BOOL bUserAbort, bFullScreen, bUseDDraw;
//extern int ohandle;
extern BOOL bTrace, bUseSound, bAutoLoad, bSmartphone;
extern PALETTEENTRY EMUpe[];
void EMUCreateColorTable(PALETTEENTRY *);
void EMUUpdatePalette(int, PALETTEENTRY *);
void NESSetMirror(NESMACHINE *, int a, int b, int c, int d);
int EMULoadNESRoms(char *pszName, unsigned char *mem, unsigned char **pROM);

unsigned char NULLMapperRead(int iAddr)
{
   return pNESMachine->mem_map[iAddr];

} /* NULLMapperRead() */

void NULLMapperWrite(int iAddr, unsigned char ucByte)
{
   pNESMachine->mem_map[iAddr] = ucByte;
} /* NULLMapperWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUSetupHandlers(char *, EMUHANDLERS *, MEMHANDLERS *, int)*
 *                                                                          *
 *  PURPOSE    : Set up the memory handlers for this CPU.                   *
 *                                                                          *
 ****************************************************************************/
void EMUSetupHandlers(unsigned char *mem_map, EMUHANDLERS *emuh, MEMHANDLERS *mhHardware)
{
unsigned char c;
int i, iNum;

   iNum = 0;
   i = 0;
   while (mhHardware[i].usLen)
      {
      emuh[iNum].pfn_read = mhHardware[i].pfn_read; /* Copy read and write routines */
      emuh[iNum].pfn_write = mhHardware[i].pfn_write;
      c = iNum++;
      if (mhHardware[i].pfn_read)
         c |= 0x40;
      if (mhHardware[i].pfn_write)
         c |= 0x80;
      memset(&mem_map[MEM_FLAGS + mhHardware[i].usStart], c, mhHardware[i].usLen);
      i++;
      }
//   *iNumHandlers = iNum;

} /* EMUSetupHandlers() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESWRAMWrite(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : Emulate the optional RAM at 0x6000-7FFF                    *
 *                                                                          *
 ****************************************************************************/
void NESWRAMWrite(int iAddr, unsigned char ucByte)
{
   if (pNESMachine->bRAMEnable)
      pNESMachine->mem_map[iAddr] = ucByte;
} /* NESWRAMWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESWRAMRead(int)                                           *
 *                                                                          *
 *  PURPOSE    : Emulate the optional RAM at 0x6000-7fff.                   *
 *                                                                          *
 ****************************************************************************/
unsigned char NESWRAMRead(int iAddr)
{
   if (pNESMachine->bRAMEnable)
      return pNESMachine->mem_map[iAddr];
   else
      return 0;
} /* NESWRAMRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESRAMRead(int)                                            *
 *                                                                          *
 *  PURPOSE    : Emulate the mirrored RAM addresses                         *
 *                                                                          *
 ****************************************************************************/
unsigned char NESRAMRead(int iAddr)
{
   return pNESMachine->mem_map[iAddr & 0x7ff]; // 0x0000-0x07ff is mirrored at 0x0800, 0x1000 and 0x1800
} /* NESRAMRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESRAMWrite(int)                                           *
 *                                                                          *
 *  PURPOSE    : Emulate the mirrored RAM addresses                         *
 *                                                                          *
 ****************************************************************************/
void NESRAMWrite(int iAddr, unsigned char ucByte)
{
   pNESMachine->mem_map[iAddr & 0x7ff] = ucByte; // 0x0000-0x07ff is mirrored at 0x0800, 0x1000 and 0x1800
} /* NESRAMWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESIORead(int)                                             *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the NES.                                *
 *                                                                          *
 ****************************************************************************/
unsigned char NESIORead(int iAddr)
{
unsigned char c, *p;

   switch (iAddr)
      {
      case 0x2002: // video status
         c = 0;
         if (bVBlank) // in VBlank
            c |= 0x80;
         if (bSpriteCollision)
            c |= 0x40; // video retrace has hit sprite 0
         if (b8Sprites)
            c |= 0x20; // 8 or more sprites on this line
         bVBlank = FALSE; // DEBUG - vblank should be cleared (apparently)
//         bSpriteCollision = FALSE; // don't clear it or Dig Dug 2 stops working
         pNESMachine->bLowerAddr = FALSE;
         break;

      case 0x2004: // Sprite RAM access
         c = pNESMachine->ucSpriteRAM[pNESMachine->ucSpriteAddr++];
         break;

      case 0x2007: // VRAM access
         c = pNESMachine->ucVRAMBuffer;
         if (pNESMachine->iVRAMAddr >= 0x2000)
            p = (unsigned char *)((pNESMachine->iVRAMAddr & 0x3ff) + pNESMachine->ulVRAMOffsets[(pNESMachine->iVRAMAddr >> 10) & 3]);
         else
            p = (unsigned char *)(pNESMachine->iVRAMAddr + pNESMachine->ulVROMOffsets[pNESMachine->iVRAMAddr >> 10]);
         pNESMachine->ucVRAMBuffer = p[0];
         pNESMachine->iVRAMAddr = (pNESMachine->iVRAMAddr + pNESMachine->iVDPIncrement) & 0x3fff;
         pNESMachine->bLowerAddr = FALSE;
         break;

      case 0x4015: // sound channel / status
         c = 0;
         if (pNESMachine->iSndLen1)
            c |= 1;
         if (pNESMachine->iSndLen2)
            c |= 2;
         if (pNESMachine->iSndLen3)
            c |= 4;
         if (pNESMachine->iSndLen4)
            c |= 8;
         if (pNESMachine->iDMCLen)
            c |= 16;
         break;

      case 0x4016: // joystick 1
         c = 0x40;
         if (pNESMachine->iJoyShift1 == 19) // reading "signature"
            {
            c |= 1;
            pNESMachine->iJoyShift1++;
            }
         else
            {
            if (pNESMachine->ucJoyButtons[0] & (1<<pNESMachine->iJoyShift1++))
               c += 1;
            }
         break;

      case 0x4017: // joystick 2
         c = 0x40;
         if (pNESMachine->iJoyShift2 == 18) // reading "signature"
            {
            c |= 1;
            pNESMachine->iJoyShift2++;
            }
         else
            {
            if (pNESMachine->ucJoyButtons[1] & (1<<pNESMachine->iJoyShift2++))
               c += 1;
            }
         break;

      default:
         c = 0xff;
         break;
      }

   return c;

} /* NESIORead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESIOWrite(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the NES.                                *
 *                                                                          *
 ****************************************************************************/
void NESIOWrite(int iAddr, unsigned char ucByte)
{
unsigned char *p;
//unsigned char c;
unsigned long ulAddr;
int iOldPattern, iOldSize;

    switch (iAddr)
      {
      case 0x2000: // PPU control register 1
         pNESMachine->iNameTable = ucByte & 3; // name table is at 0x2000, 0x2400, 0x2800, 0x2c00 (0-3)
         pNESMachine->iRefreshLatch &= ~0x0c00;
         pNESMachine->iRefreshLatch |= (ucByte & 3) << 10; // name table address (lower 2 bits)
         if (ucByte & 4)
            pNESMachine->iVDPIncrement = 32;
         else
            pNESMachine->iVDPIncrement = 1;
         if (ucByte & 16)
            pNESMachine->iPatternTable = 0x1000;
         else
            pNESMachine->iPatternTable = 0x0000;
         iOldPattern = pNESMachine->iSpritePattern;
         iOldSize = pNESMachine->iSpriteSize;
         if (ucByte & 8)
            pNESMachine->iSpritePattern = 0x1000;
         else
            pNESMachine->iSpritePattern = 0x0000;
         if (ucByte & 32)
            pNESMachine->iSpriteSize = 16;
         else
            pNESMachine->iSpriteSize = 8;
         if (iOldPattern != pNESMachine->iSpritePattern || iOldSize != pNESMachine->iSpriteSize)
            {
            bSpriteChanged = TRUE; // recalc sprites
            }
         pNESMachine->bHitIntEnable = ucByte & 0x40;
         pNESMachine->bVBlankIntEnable = ucByte & 0x80;
         break;

      case 0x2001: // PPU control register 2
         pNESMachine->bHideCol1 = ~ucByte & 2;
         pNESMachine->bVideoEnable = ucByte & 8;
         pNESMachine->bSpriteEnable = ucByte & 16;
         break;

      case 0x2003: // Sprite memory address
         pNESMachine->ucSpriteAddr = ucByte;
         break;

      case 0x2004: // Sprite RAM access
         pNESMachine->ucSpriteRAM[pNESMachine->ucSpriteAddr++] = ucByte;
         bSpriteChanged = TRUE;
         break;

      case 0x2005: // background scroll
         if (pNESMachine->bLowerAddr)
            {
            pNESMachine->iRefreshLatch &= ~0x03e0;
            pNESMachine->iRefreshLatch |= (ucByte & 0xf8) << 2;

            pNESMachine->iRefreshLatch &= ~0x7000; // keep the "fine scroll" in the upper 3 bits for simplicity
            pNESMachine->iRefreshLatch |= (ucByte & 0x07) << 12;
            pNESMachine->bLowerAddr = FALSE;
            }
         else
            {
            pNESMachine->iScrollX = ucByte; // first write
            pNESMachine->bLowerAddr = TRUE;
            }
         break;

      case 0x2006: // PPU Address
         if (pNESMachine->bLowerAddr)
            {
            pNESMachine->iVRAMAddrTemp &= 0x3f00;
            pNESMachine->iVRAMAddrTemp |= ucByte;
            pNESMachine->iVRAMAddr = pNESMachine->iVRAMAddrTemp;
            pNESMachine->bLowerAddr = FALSE;
            pNESMachine->iRefreshLatch &= 0xff00;
            pNESMachine->iRefreshLatch |= ucByte;
            pNESMachine->iRefreshAddr = pNESMachine->iRefreshLatch;
            pNESMachine->iScrollX &= 7;
            pNESMachine->iScrollX |= (ucByte & 0x1f) << 3;
            }
         else // first write = upper half of address
            {
            pNESMachine->iVRAMAddrTemp &= 0xff;
            pNESMachine->iVRAMAddrTemp |= ((ucByte & 0x3f) << 8);
            if (ucByte != 0x3f)
               {
               pNESMachine->iRefreshLatch &= 0xff;
               pNESMachine->iRefreshLatch |= ((ucByte & 0x3f) << 8);
               }
            pNESMachine->bLowerAddr = TRUE;
            }
         break;

      case 0x2007: // PPU Data
         if (pNESMachine->iVRAMAddr < 0x2000) // see if we need to protect VROM from being written on
            {
//            if (!pNESMachine->iVROMPages)
               pNESMachine->pVRAM[pNESMachine->iVRAMAddr] = ucByte; // only write to VROM area if no VROM available
            }
         else
            {
            if (pNESMachine->iVRAMAddr >= 0x3f00)
               {
               if (!(pNESMachine->iVRAMAddr & 3))
                  {
                  pNESMachine->ulPalFlags |= 1;
                  pNESMachine->pVRAM[pNESMachine->iVRAMAddr & 0x3f1f] = pNESMachine->pVRAM[pNESMachine->iVRAMAddr & 0x3f0f] = ucByte; // palette entries of color 0 are mirrored
                  }
               else
                  {
                  pNESMachine->ulPalFlags |= (1 << (pNESMachine->iVRAMAddr & 0x1f));
                  pNESMachine->pVRAM[pNESMachine->iVRAMAddr] = ucByte;
                  }
               }
            else // "name table space" 0x2000-0x3eff
               {
               p = (unsigned char *)(pNESMachine->ulVRAMOffsets[(pNESMachine->iVRAMAddr >> 10) & 3] + (pNESMachine->iVRAMAddr & 0x3ff));
               p[0] = ucByte;
               }
            }
         pNESMachine->iVRAMAddr = (pNESMachine->iVRAMAddr + pNESMachine->iVDPIncrement) & 0x3fff;
         break;

      case 0x4000: // square wave 1 control
         pNESMachine->ucSndCtrl1 = ucByte;
         pNESMachine->iEnvDelta1 = 0x10000 / ((pNESMachine->ucSndCtrl1 & 0xf)+1);
         pNESMachine->iEnvPos1 = 0;
         break;

      case 0x4001: // sweep control 1
         pNESMachine->ucSweep1 = ucByte;
         if (pNESMachine->ucSweep1 & 0x80) // if sweep is active
            pNESMachine->iSweepDelta1 = 0x10000 / (((pNESMachine->ucSweep1 >> 4) & 7)+1);
         else
            pNESMachine->iSweepDelta1 = 0; // no sweep
         break;

      case 0x4002: // freq 1 low byte
         pNESMachine->iFreq1 &= 0x700;
         pNESMachine->iFreq1 |= ucByte;
         break;

      case 0x4003: // freq/len 1
         pNESMachine->iFreq1 &= 0xff;
         pNESMachine->iFreq1 |= (ucByte & 7) << 8;
         pNESMachine->iSndLen1 = ucSndLenTranslate[ucByte >> 3]; // length in frame times of this sound
         pNESMachine->iEnvVol1 = 0xf; // reset envelope volume to max
         break;

      case 0x4004: // square wave 2 control
         pNESMachine->ucSndCtrl2 = ucByte;
         pNESMachine->iEnvDelta2 = 0x10000 / ((pNESMachine->ucSndCtrl2 & 0xf)+1);
         pNESMachine->iEnvPos2 = 0;
         break;

      case 0x4005: // sweep control 2
         pNESMachine->ucSweep2 = ucByte;
         if (pNESMachine->ucSweep2 & 0x80) // if sweep is active
            pNESMachine->iSweepDelta2 = 0x10000 / (((pNESMachine->ucSweep2 >> 4) & 7)+1);
         else
            pNESMachine->iSweepDelta2 = 0; // no sweep
         break;

      case 0x4006: // freq 2
         pNESMachine->iFreq2 &= 0x700;
         pNESMachine->iFreq2 |= ucByte;
         break;

      case 0x4007: // freq/len 2
         pNESMachine->iFreq2 &= 0xff;
         pNESMachine->iFreq2 |= (ucByte & 7) << 8;
         pNESMachine->iSndLen2 = ucSndLenTranslate[ucByte >> 3]; // length in frame times of this sound
         pNESMachine->iEnvVol2 = 0xf; // reset envelope volume to max
         break;

      case 0x4008: // triangle wave 3 control
         pNESMachine->ucSndCtrl3 = ucByte;
         pNESMachine->ucLinearCount = pNESMachine->ucLinearCounter = ucByte & 0x7f;
         break;

      case 0x400a: // freq 3
         pNESMachine->iFreq3 &= 0x700;
         pNESMachine->iFreq3 |= ucByte;
         break;

      case 0x400b: // freq/halt counter 3
         pNESMachine->iFreq3 &= 0xff;
         pNESMachine->iFreq3 |= (ucByte & 7) << 8;
         pNESMachine->iSndLen3 = ucSndLenTranslate[ucByte >> 3]; // length in frame times of this sound
         break;

      case 0x400c: // noise control
         pNESMachine->ucSndCtrl4 = ucByte;
         pNESMachine->iEnvDelta4 = 0x10000 / ((pNESMachine->ucSndCtrl4 & 0xf)+1);
         pNESMachine->iEnvPos4 = 0;
         break;

      case 0x400e: // freq 4
         pNESMachine->iFreq4 = iNoiseFreq[ucByte & 0xf];
         pNESMachine->iNoisePoly = ucByte & 0x80;
         break;

      case 0x400f: // freq/len 4
//         pNESMachine->iFreq4 &= 0xff;
//         pNESMachine->iFreq4 |= (ucByte & 7) << 8;
         pNESMachine->iSndLen4 = ucSndLenTranslate[ucByte >> 3]; // length in frame times of this sound
         pNESMachine->iEnvVol4 = 0xf; // reset envelope volume to max
         break;

      case 0x4010: // DMC mode / freq
         pNESMachine->iDMCMode = ucByte;
         pNESMachine->iDMCFreq = iDMCTranslate[ucByte & 0xf];
//         pNESMachine->iDMCCount = 0;
         break;

      case 0x4011: // DMC delta counter
         pNESMachine->iDMCCount = pNESMachine->iDMCStartCount = ucByte & 0x7f;
//         pEEQ->ucEvent[pEEQ->iCount] = ucByte & 0x7f; // 7-bit DAC value
//         pEEQ->ulTime[pEEQ->iCount++] = iTotalClocks + iClocks;
         break;

      case 0x4012: // DMC start address
         pNESMachine->iDMCAddr = pNESMachine->iDMCStartAddr = (ucByte << 6) | 0xc000;
         break;

      case 0x4013: // DMC length
         pNESMachine->iDMCLen = pNESMachine->iDMCStartLen = (ucByte << 4);
         pNESMachine->iDMCBit = 0; // starting bit number (LSB shifted out first)
         break;

      case 0x4014: // DMA to sprite memory
		 bSpriteChanged = TRUE;
         ulAddr = 0x100*ucByte;
         p = (unsigned char *)(ulAddr + pNESMachine->regs6502.ulOffsets[ulAddr >> 12]);
         if (pNESMachine->ucSpriteAddr == 0)
            memcpy(pNESMachine->ucSpriteRAM, p, 256); // do DMA copy in one shot
         else
            {
            memcpy(&pNESMachine->ucSpriteRAM[pNESMachine->ucSpriteAddr], p, 256-pNESMachine->ucSpriteAddr); // do DMA copy in two steps
            memcpy(pNESMachine->ucSpriteRAM, &p[256-pNESMachine->ucSpriteAddr], pNESMachine->ucSpriteAddr);
            }
         pNESMachine->iDMACycles = 514; // CPU is halted for 514 cycles
         break;

      case 0x4015: // sound enable bits
         pNESMachine->ucSoundChannels = ucByte;
         break;

      case 0x4016: // joystick 1
         if (!(ucByte & 1)) // joystick reset strobe
            {
            pNESMachine->iJoyShift1 = pNESMachine->iJoyShift2 = 0;
            }
         break;

      case 0x4017:
         break;

      default:
//         c = 0;
         break;
      }
} /* NESIOWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NES_PostLoad(GAME_BLOB *pBlob)                             *
 *                                                                          *
 *  PURPOSE    : Fix the pointers after loading a game state                *
 *                                                                          *
 ****************************************************************************/
void NES_PostLoad(GAME_BLOB *pBlob)
{
int i;
unsigned long iOff;
unsigned long iDelta;
NESMACHINE *pOld, *pNew;

   pOld = (NESMACHINE *)pBlob->OriginalMachine;
   pNew = (NESMACHINE *)pBlob->pMachine;
   // force a decode of the sprites
   bSpriteChanged = TRUE;
   // force a reload of the palette conversion table
   pNew->ulPalFlags = -1;
   // fix the offsets of the local pointers in our structure
   for (i=0; i<16; i++) // restore the main_mem/ROM/RAM Bank selections (could be any of the 3)
   {
      iOff = (i*0x1000) + pNew->regs6502.ulOffsets[i];
      if (iOff >= (unsigned long)pNew->mem_map && iOff < ((unsigned long)pNew->mem_map + 0x10000)) // original memory map
      {
          iDelta = (unsigned long)pNew->mem_map - (unsigned long)pOld->mem_map;
          pNew->regs6502.ulOffsets[i] -= iDelta;
      }
      else if (iOff >= (unsigned long)pNew->pRAMBank && iOff < ((unsigned long)pNew->pRAMBank + 0x10000))
      {
          iDelta = (unsigned long)pNew->pRAMBank - (unsigned long)pOld->pRAMBank;
          pNew->regs6502.ulOffsets[i] -= iDelta;
      }
      else // must be pointing to pBankROM
      {
          iDelta = (unsigned long)pNew->pBankROM - (unsigned long)pOld->pBankROM;
          pNew->regs6502.ulOffsets[i] -= iDelta;
      }
   } // for each memory bank selector
   // Restore the VROM Bank selections (can be pointing to VROM or VRAM)
   for (i=0; i<8; i++)
      {
      // see if it points into VRAM or VROM
      iDelta = pNew->ulVROMOffsets[i] - (unsigned long)pNew->pVRAM;
      if (/*iDelta >=0 &&*/ iDelta < 0x4000) // VRAM
         {
         iDelta = (unsigned long)pNew->pVRAM - (unsigned long)pOld->pVRAM;
         pNew->ulVROMOffsets[i] -= iDelta;
         }
      else // must be VROM
         {
         iDelta = (unsigned long)pNew->pVROM - (unsigned long)pOld->pVROM;
         pNew->ulVROMOffsets[i] -= iDelta;
         }
      }      
   // Restore the VRAM offsets (can point to VROM or VRAM)
   for (i=0; i<8; i++)
      {
      // see if it points into VRAM or VROM
      iDelta = pNew->ulVRAMOffsets[i] - (unsigned long)pNew->pVRAM;
      if (/*iDelta >=0 && */ iDelta < 0x4000) // VRAM
         {
         iDelta = (unsigned long)pNew->pVRAM - (unsigned long)pOld->pVRAM;
         pNew->ulVRAMOffsets[i] -= iDelta;
         }
      else // must be VROM
         {
         iDelta = (unsigned long)pNew->pVROM - (unsigned long)pOld->pVROM;
         pNew->ulVRAMOffsets[i] -= iDelta;
         }
      }      

   // Restore the local pointers
   pNew->pVRAM = pOld->pVRAM;
   pNew->pVROM = pOld->pVROM;
   pNew->mem_map = pOld->mem_map;
   pNew->pRAMBank = pOld->pRAMBank;
   pNew->pBankROM = pOld->pBankROM;
   pNew->pEMUH = pOld->pEMUH;
   NESSetMapperHandlers(pNew); // reset the function pointers for the mapper memory handlers
} /* NES_PostLoad() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESCheckSprite0(void)                                      *
 *                                                                          *
 *  PURPOSE    : Check for sprite collision on non-visible lines or when    *
 *               sprites are disabled.                                      *
 *                                                                          *
 ****************************************************************************/
void NESCheckSprite0(void)
{
unsigned char ucAttr, c1, *pSprite, *pTileBits;
int iAddr, y, iTile, iScanOffset;

   if (!bSpriteCollision)
      {
      pSprite = &pNESMachine->ucSpriteRAM[0];
      y = pSprite[0] - 1;
      if (y > 247)
         y -= 256; // it's a negative value
      if (y != -1 && y <= iScanLine && y + pNESMachine->iSpriteSize > iScanLine) // sprite is visible
         {
         iTile = pSprite[1];
         ucAttr = pSprite[2];
         iScanOffset = (iScanLine - (y & (pNESMachine->iSpriteSize-1))); // offset to correct byte in tile data
         if (ucAttr & 0x80) // V Flip
            iScanOffset = pNESMachine->iSpriteSize - 1 - iScanOffset;
         if (pNESMachine->iSpriteSize == 16) // big sprites (even at 0x0000, odd at 0x1000)
            {
            if (iTile & 1) // odd tiles from 0x1000
               iAddr = 0x1000 + ((iTile & 0xfe)<<4) + ((iScanOffset & 8) << 1) + (iScanOffset&7);
            else // even tiles from 0x0000
               iAddr = (iTile<<4) + ((iScanOffset & 8) << 1) + (iScanOffset&7);
            }
         else
            iAddr = pNESMachine->iSpritePattern + (iTile<<4) + iScanOffset;
         pTileBits = (unsigned char *)(pNESMachine->ulVROMOffsets[iAddr>>10] + iAddr);
         c1 = pTileBits[0]; // plane 0
         c1 |= pTileBits[8]; // plane 1
         if (c1) // non-zero pixels on this line
            {
            bSpriteCollision = TRUE;
            if (pNESMachine->bHitIntEnable)
               pNESMachine->ucIRQs |= INT_NMI; // set interrupt for hit test
            }
         }
      } // if not already collided
} /* NESCheckSprite0() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESProcessSprites(void)                                    *
 *                                                                          *
 *  PURPOSE    : Scan the sprite list to save time later.                   *
 *                                                                          *
 ****************************************************************************/
void NESProcessSprites(void)
{
int i, y, iStart, iEnd;
unsigned char *pSprite;

	iSpriteCount = 0;
	memset(ucSpriteDraw, 0, sizeof(ucSpriteDraw)); // clear scanline flags
    pSprite = pNESMachine->ucSpriteRAM;
	for (i=0; i<64; i++) // scan up to 64 sprites
	{
	y = pSprite[0]-1;
   if (y >= 224)
		y -= 256; // negative
	if (y+pNESMachine->iSpriteSize > 0 && y < 224) // sprite is visible
		{
		ucSpriteList[iSpriteCount++] = i;
		// mark the lines which have this sprite
      iStart = y;
      iEnd = y + pNESMachine->iSpriteSize;
      if (iStart < 0)
         iStart = 0;
      if (iEnd > 224)
         iEnd = 224;
      while (iStart < iEnd)
         ucSpriteDraw[iStart++] = 1; // mark the scanlines with visible sprites
		}
	pSprite += 4;
	}
} /* NESProcessSprites() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESDrawScanline(void)                                      *
 *                                                                          *
 *  PURPOSE    : Render the current scanline.                               *
 *                                                                          *
 ****************************************************************************/
void NESDrawScanline(void)
{
unsigned char ucAttr;
uint32_t ulLeft = 0;
uint32_t ulRight = 0;
unsigned char *pDest;
signed int i, x, tx, y, iCount;
int iTable, iScanOffset, iTile, iDeltaX;
unsigned char c1, c2, *pSprite, *pTile, *pTable;
uint32_t ulColor;
uint32_t ulOldColor = 0;
uint32_t *pulDest;
unsigned char ucTemp[512];
unsigned char *pTileBits = NULL;
unsigned long iOldTile, iAddr;
unsigned char ucAttrShift;
BOOL bWrapped;
static unsigned long *pl, ulLineBuf[40]; // holds data for speeding up rendering of tiles
//static unsigned char *pOldTile;

   b8Sprites = FALSE;
   if (bSpriteChanged) // rescan the sprite table
      {
	   NESProcessSprites();
	   bSpriteChanged = FALSE;
      }

   if (!pNESMachine->bVideoEnable)
      {
      memset(ucTemp, 0, 256+16); // start out with a blank line
      goto drawsprites;
      }
           
//   y = ((iScrollY + iScanLine)) & 0x1ff; // current scan line to draw
   pNESMachine->iRefreshAddr &= ~0x400; // get starting H table
   pNESMachine->iRefreshAddr |= (pNESMachine->iRefreshLatch & 0x400);
   y = (pNESMachine->iRefreshAddr >> 12) + ((pNESMachine->iRefreshAddr & 0x3e0) >> 2);
//   iTable = pNESMachine->iNameTable;
   iTable = (pNESMachine->iRefreshAddr & 0xc00) >> 10;
//   if (y >= 240) // vertical wrap around
//      {
//      y -= 240;
//      iTable ^= 2; // vertical second page
//      }
   if (y & 16)
      ucAttrShift = 4; // upper half
   else
      ucAttrShift = 0; // lower half
   pTable = (unsigned char *)(pNESMachine->ulVRAMOffsets[iTable]); // point to current row
   pTile = &pTable[((y>>3)*32)];
   pTable += (0x3c0 + (y>>5)*8); // point to attribute data
   iOldTile = -1;
   bWrapped = FALSE;
   pDest = &ucTemp[8];
   iScanOffset = (y&7); // offset to correct byte in tile data
   tx = pNESMachine->iScrollX>>3;
// This speeds things up because this loop gets executed 443000 times per second
// and the code to calculate the address of the tile data takes a while to calculate
   pl = &ulLineBuf[0]; // buffer holding tile and color info
   if (&pTile[tx] == NULL) // DEBUG - for some reason GCC messes up my tile re-use code
//   if (&pTile[tx] == pOldTile) // drawing the same tiles as the last scan line, speed things up
      {
#ifdef _USE_ARM_ASM
      ARMDrawNESTiles(pl, iScanOffset, pDest);
#else      
      for (x=0; x<33; x++) // draw 33 tiles (256 pixels + 1 for scrolling slop
         {
         pTileBits = (unsigned char *)(*pl++);
         if (iOldTile != (unsigned long)pTileBits) // new tile to draw
            {
            iOldTile = (unsigned long)pTileBits;
            ulColor = (unsigned long)pTileBits & 0xc; // extract the color from the address
            pTileBits = (unsigned char *)((unsigned long)pTileBits & 0xfffffff0); // get the address back
            ulLeft = ulRight = (ulColor | ulColor << 8 | ulColor << 16 | ulColor << 24);
            c1 = pTileBits[iScanOffset]; // plane 0
            c2 = pTileBits[iScanOffset+8]; // plane 1
            if (c1)
               {
               ulLeft |= ulExpand0[(c1 >> 4)];
               ulRight |= ulExpand0[c1 & 0xf];
               }
            if (c2)
               {
               ulLeft |= ulExpand1[(c2 >> 4)];
               ulRight |= ulExpand1[c2 & 0xf];
               }
            } // if tile changed
         *(uint32_t *)pDest = ulLeft;
         *(uint32_t *)&pDest[4] = ulRight;
         pDest += 8;
         } // for x
#endif         
      }
   else // a new line of tiles needs to be drawn
      {
//      pOldTile = &pTile[tx];
      for (x=0; x<262; x+=8)
         {
         if (tx >= 32 && !bWrapped) // we hit the horizontal wrap point
            {
            tx &= 0x1f;
            iTable ^= 1; // horizontal second page
            pNESMachine->iRefreshAddr ^= 0x400;
            pTable = (unsigned char *)(pNESMachine->ulVRAMOffsets[iTable]); // point to current table
            pTile = &pTable[((y>>3)*32)]; // point to current row
            pTable += (0x3c0 + (y>>5)*8); // point to attribute data
            bWrapped = TRUE;
            }
         iTile = pTile[tx];
         ucAttr = pTable[(tx>>2)] >> ucAttrShift;
         if (tx & 2) // right half
            ulColor = ucAttr & 0xc;
         else
            ulColor = ((ucAttr & 3) << 2);
         if (iTile != iOldTile || ulColor != ulOldColor) // if something changed, recalc
            {
            iAddr = pNESMachine->iPatternTable + (iTile<<4);
            pTileBits = (unsigned char *)pNESMachine->ulVROMOffsets[iAddr>>10];
            pTileBits += iAddr;
            if (pNESMachine->bMapperLatch) // if mapper checks for PPU reads
               {
               if (iAddr >= 0xfd0 && iAddr <= 0xfef)
                  {
                  if (pNESMachine->ucCHRBanks[4] != (iAddr>>4))
                     {
                     pNESMachine->ucCHRBanks[4] = (unsigned char)(iAddr>>4);
                     NESMapper10SetPages(FALSE);
                     }
                  }
               if (iAddr >= 0x1fd0 && iAddr <= 0x1fef)
                  {
                  if (pNESMachine->ucCHRBanks[5] != ((iAddr>>4)&0xff))
                     {
                     pNESMachine->ucCHRBanks[5] = (unsigned char)(iAddr>>4);
                     NESMapper10SetPages(TRUE);
                     }
                  }
               }
            // Draw the 8 pixels of the tile
            ulColor |= (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
            ulLeft = ulRight = ulColor;
            c1 = pTileBits[iScanOffset]; // plane 0
            c2 = pTileBits[iScanOffset+8]; // plane 1
            if (c1)
               {
               ulLeft |= ulExpand0[(c1 >> 4)];
               ulRight |= ulExpand0[c1 & 0xf];
               }
            if (c2)
               {
               ulLeft |= ulExpand1[(c2 >> 4)];
               ulRight |= ulExpand1[c2 & 0xf];
               }
            iOldTile = iTile;
            ulOldColor = ulColor; // for next time
            } // if tile changed
         *pl++ = (unsigned long)pTileBits | (ulColor & 0xc); // pointer to tile data + color
         *(uint32_t *)pDest = ulLeft;
         *(uint32_t *)&pDest[4] = ulRight;
         pDest += 8;
         tx++;
         } // for x
      } // if drawing a new set of tiles

drawsprites:
   if (pNESMachine->bSpriteEnable) // draw the sprites
      {
      if (ucSpriteDraw[iScanLine]) // if there are visible sprites on this scanline
         {
         iCount = 0; // max of 8 sprites per line
         iDeltaX = pNESMachine->iScrollX & 7;
         for (i=0; i<iSpriteCount/* && iCount < 8*/; i++) // up to 64 active sprites
            {
            pSprite = &pNESMachine->ucSpriteRAM[ucSpriteList[i]<<2]; // only check active sprites
            y = pSprite[0] -1;
            if (y <= iScanLine && y + pNESMachine->iSpriteSize > iScanLine) // sprite is visible
               {
               iTile = pSprite[1];
               ucAttr = pSprite[2];
               x = pSprite[3] + iDeltaX;
               if (ucAttr & 0x80) // V Flip
                  iScanOffset = pNESMachine->iSpriteSize - 1 - (iScanLine - y);
               else
                  iScanOffset = (iScanLine - y); // offset to correct byte in tile data
               if (pNESMachine->iSpriteSize == 16) // big sprites (even at 0x0000, odd at 0x1000)
                  {
                  if (iTile & 1) // odd tiles from 0x1000
                     iAddr = 0x1000 + ((iTile & 0xfe)<<4) + ((iScanOffset & 8) << 1) + (iScanOffset&7);
                  else // even tiles from 0x0000
                     iAddr = (iTile<<4) + ((iScanOffset & 8) << 1) + (iScanOffset&7);
                  }
               else
                  iAddr = pNESMachine->iSpritePattern + (iTile<<4) + iScanOffset;
               pTileBits = (unsigned char *)(pNESMachine->ulVROMOffsets[iAddr>>10] + iAddr);
               ulColor = 0x50 + ((ucAttr & 3) << 2);
               // Draw the 8 pixels of the tile
               ulLeft = ulRight = (ulColor | (ulColor << 8) | (ulColor << 16) | (ulColor << 24));
               c1 = pTileBits[0]; // plane 0
               c2 = pTileBits[8]; // plane 1
               pDest = &ucTemp[8+x];
               if (ucAttr & 0x40) // H Flip
                  {
                  if (c1)
                     {
                     ulRight |= ulExpandMirror0[(c1 >> 4) & 0xf];
                     ulLeft |= ulExpandMirror0[c1 & 0xf];
                     }
                  if (c2)
                     {
                     ulRight |= ulExpandMirror1[(c2 >> 4) & 0xf];
                     ulLeft |= ulExpandMirror1[c2 & 0xf];
                     }
                  }
               else
                  {
                  if (c1)
                     {
                     ulLeft |= ulExpand0[(c1 >> 4) & 0xf];
                     ulRight |= ulExpand0[c1 & 0xf];
                     }
                  if (c2)
                     {
                     ulLeft |= ulExpand1[(c2 >> 4) & 0xf];
                     ulRight |= ulExpand1[c2 & 0xf];
                     }
                  }
               if (ulLeft & 0x03030303) // first 4 pixels have anything to draw?
                  {
                  if (i == 0 && !bSpriteCollision) // check for sprite collision with non-zero background pixel
                     {
                     bSpriteCollision = TRUE;
                     if (pNESMachine->bHitIntEnable)
                        pNESMachine->ucIRQs |= INT_NMI; // set interrupt for hit test
                     }
                  if (ucAttr & 0x20) // low priority sprite
                     {
                     if (ulLeft & 0x3 && !(pDest[0] & 0x43))
                        pDest[0] = (unsigned char)(ulLeft);
                     else
                        pDest[0] |= 0x40; // mark as a transparent sprite so that other sprites won't overwrite it
                     if (ulLeft & 0x0300 && !(pDest[1] & 0x43))
                        pDest[1] = (unsigned char)(ulLeft >> 8);
                     else
                        pDest[1] |= 0x40;
                     if (ulLeft & 0x030000 && !(pDest[2] & 0x43))
                        pDest[2] = (unsigned char)(ulLeft >> 16);
                     else
                        pDest[2] |= 0x40;
                     if (ulLeft & 0x03000000 && !(pDest[3] & 0x43))
                        pDest[3] = (unsigned char)(ulLeft >> 24);
                     else
                        pDest[3] |= 0x40;
                     }
                  else
                     {
                     if (ulLeft & 0x03 && !(pDest[0] & 0x40))
                        pDest[0] = (unsigned char)(ulLeft);
                     if (ulLeft & 0x0300 && !(pDest[1] & 0x40))
                        pDest[1] = (unsigned char)(ulLeft >> 8);
                     if (ulLeft & 0x030000 && !(pDest[2] & 0x40))
                        pDest[2] = (unsigned char)(ulLeft >> 16);
                     if (ulLeft & 0x03000000 && !(pDest[3] & 0x40))
                        pDest[3] = (unsigned char)(ulLeft >> 24);
                     }
                  }
               if (ulRight & 0x03030303) // last 4 pixels have anything to draw?
                  {
                  if (i == 0 && !bSpriteCollision) // check for sprite collision with non-zero background pixel
                     {
                     bSpriteCollision = TRUE;
                     if (pNESMachine->bHitIntEnable)
                        pNESMachine->ucIRQs |= INT_NMI; // set interrupt for hit test
                     }
                  if (ucAttr & 0x20) // low priority sprite
                     {
                     if (ulRight & 0x03 && !(pDest[4] & 0x43))
                        pDest[4] = (unsigned char)(ulRight);
                     else
                        pDest[4] |= 0x40;
                     if (ulRight & 0x0300 && !(pDest[5] & 0x43))
                        pDest[5] = (unsigned char)(ulRight >> 8);
                     else
                        pDest[5] |= 0x40;
                     if (ulRight & 0x030000 && !(pDest[6] & 0x43))
                        pDest[6] = (unsigned char)(ulRight >> 16);
                     else
                        pDest[6] |= 0x40;
                     if (ulRight & 0x03000000 && !(pDest[7] & 0x43))
                        pDest[7] = (unsigned char)(ulRight >> 24);
                     else
                        pDest[7] |= 0x40;
                     }
                  else
                     {
                     if (ulRight & 0x03 && !(pDest[4] & 0x40))
                        pDest[4] = (unsigned char)(ulRight);
                     if (ulRight & 0x0300 && !(pDest[5] & 0x40))
                        pDest[5] = (unsigned char)(ulRight >> 8);
                     if (ulRight & 0x030000 && !(pDest[6] & 0x40))
                        pDest[6] = (unsigned char)(ulRight >> 16);
                     if (ulRight & 0x03000000 && !(pDest[7] & 0x40))
                        pDest[7] = (unsigned char)(ulRight >> 24);
                     }
                  }
               iCount++;
               } // if sprite visible
            } // for each sprite
         if (iCount >= 8)
            b8Sprites = TRUE;
         }
      } // if sprites enabled
   else // still need to check for sprite 0 collision even when they are disabled (dig dug 2 needs this)
      NESCheckSprite0(); // check for sprite 0 collision (sprites diabled)

// Update the 2 palettes (16 colors each)
   if (pNESMachine->ulPalFlags)
      {
      for (i=0; i<32; i++)
         {
         if (pNESMachine->ulPalFlags & (1<<i))
            {
            pNESMachine->usPalConvert[64+i] = pNESMachine->usPalConvert[i] = pColorConvert[pNESMachine->pVRAM[0x3f00+i]]; // convert to RGB565
            }
         if (!(i & 3))
            pNESMachine->usPalConvert[64+i] = pNESMachine->usPalConvert[i] = pNESMachine->usPalConvert[0]; // background color of every palette points to 0
         }
      pNESMachine->ulPalFlags = 0;
      }

// Convert the 8-bit pixels to 16-bits by translating through the palette
   iCount = 256; //iScreenX; // number of pixels to convert to 16bpp
   pDest = &ucTemp[8+(pNESMachine->iScrollX & 7)];
#ifdef FAST_DRAW //_WIN32_WCE
   if (bSmartphone) // need to draw the whole line to be shrunk later
      {
//      iCount = 176;
      pulDest = (uint32_t *)&pBitmap[iPitch * (iScanLine-8)];
      if (pNESMachine->bHideCol1)
         pDest[0] = pDest[1] = pDest[2] = pDest[3] = pDest[4] = pDest[5] = pDest[6] = pDest[7] = 0;
      }
   else // Pocket PC
      {
      pulDest = (uint32_t *)&pScreen[iScreenPitch*(iScanLine+30)];
      iCount = 240; // Pocket PC is 240 across
      pDest += 8; // skip 8 columns (cut off left and right 8 to make it fit)
      }
#else
   pulDest = (uint32_t *)&pBitmap[iPitch * (iScanLine-8)];
   if (pNESMachine->bHideCol1)
      pDest[0] = pDest[1] = pDest[2] = pDest[3] = pDest[4] = pDest[5] = pDest[6] = pDest[7] = 0;
   if (bHideOverscan)
      pDest[248] = pDest[249] = pDest[250] = pDest[251] = pDest[252] = pDest[253] = pDest[254] = pDest[255] = 0;
#endif // BOGUS
#ifdef _USE_ARM_ASM
   ARM816FAST(pDest, pulDest, pNESMachine->usPalConvert, iCount>>3);
#else
   x = 0;
   if (pNESMachine->iScrollX & 3) // need to do it byte by byte
      {
      for (i=x;i<iCount; i+=8)
         {
         pulDest[0] = pNESMachine->usPalConvert[pDest[0]] | (pNESMachine->usPalConvert[pDest[1]] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pNESMachine->usPalConvert[pDest[2]] | (pNESMachine->usPalConvert[pDest[3]] << 16);
         pulDest[2] = pNESMachine->usPalConvert[pDest[4]] | (pNESMachine->usPalConvert[pDest[5]] << 16);
         pulDest[3] = pNESMachine->usPalConvert[pDest[6]] | (pNESMachine->usPalConvert[pDest[7]] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
   else // can read as longs
      {
      uint32_t ul;
      for (i=x;i<iCount; i+=8)
         {
         ul = *(uint32_t *)&pDest[0];
         pulDest[0] = pNESMachine->usPalConvert[ul & 0xff] | (pNESMachine->usPalConvert[(ul >> 8) & 0xff] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pNESMachine->usPalConvert[(ul >> 16) & 0xff] | (pNESMachine->usPalConvert[(ul >> 24) & 0xff] << 16);
         ul = *(uint32_t *)&pDest[4];
         pulDest[2] = pNESMachine->usPalConvert[ul & 0xff] | (pNESMachine->usPalConvert[(ul >> 8) & 0xff] << 16);
         pulDest[3] = pNESMachine->usPalConvert[(ul >> 16) & 0xff] | (pNESMachine->usPalConvert[(ul >> 24) & 0xff] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
#endif      
} /* NESDrawScanline() */

void TRACE6502(REGS6502 *regs, int iClocks)
{
static uint32_t ulCount = 0;
static unsigned short usOldPC;
unsigned char ucTemp[16];
//unsigned char *p;
if (!bTrace)
   return;
if (usOldPC == regs->usRegPC) // defect in 6502 trace code, reject
   return;
usOldPC = regs->usRegPC;
ulCount++;
if (ulCount>= 0x5B91)
   bTrace = TRUE;
/* Need less bulk, only save critical regs */
memset(ucTemp, 0, 16);
ucTemp[0] = (char)regs->usRegPC;
ucTemp[1] = (char)(regs->usRegPC >> 8);
ucTemp[2] = (char)regs->ucRegA;
ucTemp[3] = (char)regs->ucRegX;
ucTemp[4] = (char)regs->ucRegY;
ucTemp[5] = (char)regs->ucRegS;
ucTemp[6] = (char)regs->ucRegP;
//p = (unsigned char *)(regs->usRegPC + regs.ulOffsets[regs->usRegPC >> 12]);
//p = (unsigned char *)(0x14b + regs->ulOffsets[0]);
//ucTemp[7] = pNESMachine->mem_map[0x100 + regs->ucRegS]; //p[0];
//ucTemp[8] = (char)iScanLine;
//ucTemp[9] = (char)iClocks;
//ucTemp[10] = pNESMachine->mem_map[0x6a17];
//ucTemp[11] = pNESMachine->mem_map[0x11];
//EMUWrite(ohandle, ucTemp, 16);
} /* TRACE6502() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESReset(NESMACHINE *)                                     *
 *                                                                          *
 *  PURPOSE    : Reset the state of the emulator.                           *
 *                                                                          *
 ****************************************************************************/
void NESReset(NESMACHINE *pNESTemp)
{
//int i;

    pNESTemp->iVRAMAddr = 0;
    pNESTemp->bRAMEnable = TRUE; // start out with optional RAM enabled
    pNESTemp->ucSpriteAddr = 0;
    pNESTemp->bLowerAddr = FALSE;
    pNESTemp->iVDPIncrement = 1;
    pNESTemp->iScrollX = pNESTemp->iScrollY = 0;
    pNESTemp->iSpriteSize = 8;
    pNESTemp->bVBlankIntEnable = FALSE;
    pNESTemp->bHitIntEnable = FALSE;
    pNESTemp->bVideoEnable = FALSE;
    pNESTemp->ucSoundChannels = 0; // turn sound off
    pNESTemp->iNameTable = 0;
    pNESTemp->iPatternTable = 0;
    pNESTemp->iSpritePattern = 0;
    pNESTemp->iSweepDelta1 = pNESTemp->iSweepDelta2 = 0;
    pNESTemp->iSweepPos1 = pNESTemp->iSweepPos2 = 0;
    pNESTemp->iDMCLen = pNESTemp->iDMCStartLen = 0;
    pNESTemp->iSndLen1 = pNESTemp->iSndLen2 = pNESTemp->iSndLen3 = pNESTemp->iSndLen4 = 0;
    pNESTemp->iIRQStartCount = pNESTemp->iIRQCount = 0;
    pNESTemp->bIRQCounter = FALSE;
    pNESTemp->ucCartCMD = 0;

    if (pNESTemp->iVROMPages) // if VROM preset, just erase VRAM area
       memset(&pNESTemp->pVRAM[0x2000], 0, 0x2000);
    else
       memset(pNESTemp->pVRAM, 0, 0x4000);

    memset(&pNESTemp->mem_map[0x6000], 0, 0x2000); // optional cartridge RAM
//    memset(&mem_map[0x0000], 0xa0, 0x800); // SRAM starts at 0xa0?

    ARESETNES6502(pNESTemp->mem_map, &pNESTemp->regs6502);
    
    pNESTemp->ucIRQs = 0;

} /* NESReset() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESUpdateButtons(ULONG)                                    *
 *                                                                          *
 *  PURPOSE    : Update the NES button bits.                                *
 *                                                                          *
 ****************************************************************************/
void NESUpdateButtons(uint32_t ulKeys)
{
unsigned char c;

   if (ulKeys == 0) // nothing pressed
      {
      pNESMachine->ucJoyButtons[0] = pNESMachine->ucJoyButtons[1] = 0;
      return;
      }

   c = 0;
   if (ulKeys & RKEY_BUTTA_P1)
      c ^= 0x1;
   if (ulKeys & RKEY_BUTTB_P1)
      c ^= 0x2;
   if (ulKeys & RKEY_SELECT_P1)
      c ^= 0x4;
   if (ulKeys & RKEY_START_P1)
      c ^= 0x8;
   if (ulKeys & RKEY_UP_P1)
      c ^= 0x10;
   if (ulKeys & RKEY_DOWN_P1)
      c ^= 0x20;
   if (ulKeys & RKEY_LEFT_P1)
      c ^= 0x40;
   if (ulKeys & RKEY_RIGHT_P1)
      c ^= 0x80;
   pNESMachine->ucJoyButtons[0] = c;

   c = 0;
   if (ulKeys & RKEY_BUTTA_P2)
      c ^= 0x1;
   if (ulKeys & RKEY_BUTTB_P2)
      c ^= 0x2;
   if (ulKeys & RKEY_SELECT_P2)
      c ^= 0x4;
   if (ulKeys & RKEY_START_P2)
      c ^= 0x8;
   if (ulKeys & RKEY_UP_P2)
      c ^= 0x10;
   if (ulKeys & RKEY_DOWN_P2)
      c ^= 0x20;
   if (ulKeys & RKEY_LEFT_P2)
      c ^= 0x40;
   if (ulKeys & RKEY_RIGHT_P2)
      c ^= 0x80;
   pNESMachine->ucJoyButtons[1] = c;

} /* NESUpdateButtons() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper0(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #0 (none)    .                           *
 *                                                                          *
 ****************************************************************************/
void NESMapper0(int iAddr, unsigned char ucByte)
{
   // do nothing
} /* NESMapper0() */

void NESMapper1Mirror(void)
{
   switch (pNESMachine->ucPRGBanks[0] & 3) // mirroring
      {
      case 0: // one screen at 0x2000
         NESSetMirror(pNESMachine, 0,0,0,0);
         break;
      case 1: // one screen at 0x2400
         NESSetMirror(pNESMachine, 1,1,1,1);
         break;
      case 2: // vertical
         NESSetMirror(pNESMachine, 0,1,0,1); // vertical
         break;
      case 3: // horizontal
         NESSetMirror(pNESMachine, 0,0,1,1); // horizontal
         break;
      }

} /* NESMapper1Mirror() */

void NESMapper1CHRSync(void)
{
int i;

   if (pNESMachine->iVROMPages)
      {
      if (pNESMachine->ucPRGBanks[0] & 0x10) // 4K VROM banks
         {
         for (i=0; i<4; i++)
            {
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucPRGBanks[1] & (pNESMachine->iVROMPages-1))*0x1000]);
            pNESMachine->ulVROMOffsets[i+4] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucPRGBanks[2] & (pNESMachine->iVROMPages-1))*0x1000] - 0x1000);
            }
         }
      else // 8K VROM banks
         {
         for (i=0; i<8; i++)
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucPRGBanks[1] >> 1)*0x2000]);
         }
      }
   else // sync VRAM pages
      {
      if (pNESMachine->ucPRGBanks[0] & 0x10) // 4K VRAM banks
         {
         for (i=0; i<4; i++)
            {
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVRAM[(pNESMachine->ucPRGBanks[1] & 1)*0x1000]);
            pNESMachine->ulVROMOffsets[i+4] = (unsigned long)(&pNESMachine->pVRAM[(pNESMachine->ucPRGBanks[2] & 1)*0x1000] - 0x1000);
            }
         }
      else // 8K VROM banks
         {
         for (i=0; i<8; i++)
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVRAM[0]);
         }
      }
} /* NESMapper1CHRSync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper1PRGSync(void)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #1 update.                               *
 *                                                                          *
 ****************************************************************************/
void NESMapper1PRGSync(void)
{
int i, iPage;
unsigned char ucOffset = pNESMachine->ucPRGBanks[1] & 0x10;

   switch (pNESMachine->ucPRGBanks[0] & 0xc) // switch 16K pages at 8000/C000
      {
      case 0:
      case 4:
         // swap 32K bank at 0x8000 & 0xc000
         iPage = (pNESMachine->ucPRGBanks[3] & 0xe) + ucOffset;
         iPage &= (pNESMachine->iROMPages-1);
         for (i=8; i<16; i++)
            {
            pNESMachine->regs6502.ulOffsets[i] = (unsigned long)&pNESMachine->pBankROM[iPage*0x4000 - 0x8000];
            }
         break;
      case 8:
         iPage = ((pNESMachine->ucPRGBanks[3] & 0xf) + ucOffset) & (pNESMachine->iROMPages-1);
         for (i=0; i<4; i++)
            {
            pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)&pNESMachine->pBankROM[(ucOffset&(pNESMachine->iROMPages-1))*0x4000 - 0x8000];
            pNESMachine->regs6502.ulOffsets[0xc+i] = (unsigned long)&pNESMachine->pBankROM[iPage*0x4000 - 0xc000];
            }
         break;
      case 0xc:
         iPage = ((pNESMachine->ucPRGBanks[3] & 0xf) + ucOffset) & (pNESMachine->iROMPages-1);
         for (i=0; i<4; i++)
            {
            pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)&pNESMachine->pBankROM[iPage*0x4000 - 0x8000];
            pNESMachine->regs6502.ulOffsets[0xc+i] = (unsigned long)&pNESMachine->pBankROM[((0xf+ucOffset)&(pNESMachine->iROMPages-1))*0x4000 - 0xc000];
            }
         break;
      }

} /* NESMapper1PRGSync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper1(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #1 (ROM + VROM).                         *
 *                                                                          *
 ****************************************************************************/
void NESMapper1(int iAddr, unsigned char ucByte)
{
unsigned char ucIndex;


   ucIndex = (unsigned char)((iAddr >> 13) & 3);
   if (ucByte & 0x80) // reset bit
      {
      pNESMachine->ucCartShift = 0;
      pNESMachine->ucCartLatch = 0;
      if ((pNESMachine->ucPRGBanks[0] & 0xc) != 0xc)
         {
         pNESMachine->ucPRGBanks[0] |= 0xc;
         NESMapper1PRGSync();
         }
      return; // no bits used on reset
      }
//   if (ucIndex != ucCartOldIndex)
//      {
//      ucCartShift = 0;
//      ucCartLatch = 0;  // if changed registers, start over
//      }
//   ucCartOldIndex = ucIndex;
   pNESMachine->ucCartLatch |= ((ucByte & 1) << (pNESMachine->ucCartShift++)); // shift in low bit

   if (pNESMachine->ucCartShift == 5)
      {
      if (pNESMachine->ucPRGBanks[ucIndex] == pNESMachine->ucCartLatch) // nothing changed
         {
         pNESMachine->ucCartShift = pNESMachine->ucCartLatch = 0;
         return;
         }
      pNESMachine->ucPRGBanks[ucIndex] = pNESMachine->ucCartLatch;
      pNESMachine->ucCartShift = pNESMachine->ucCartLatch = 0;
      switch (ucIndex)
         {
         case 0:
            NESMapper1Mirror();
            NESMapper1CHRSync();
            NESMapper1PRGSync();
            break;
         case 1:
            NESMapper1CHRSync();
            NESMapper1PRGSync();
            break;
         case 2:
            NESMapper1CHRSync();
            break;
         case 3:
            pNESMachine->bRAMEnable = !(pNESMachine->ucPRGBanks[ucIndex] & 0x10);
            NESMapper1PRGSync();
            break;
         } // switch
      } // if 5 bits shifted in
} /* NESMapper1() */


/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper2(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #2 (ROM only).                           *
 *                                                                          *
 ****************************************************************************/
void NESMapper2(int iAddr, unsigned char ucByte)
{
int i;

   ucByte &= (pNESMachine->iROMPages-1);
   if (ucByte != pNESMachine->ucPRGBanks[0])
      {
      pNESMachine->ucPRGBanks[0] = ucByte;
      for (i=8; i<12; i++) // only lower bank can change
         pNESMachine->regs6502.ulOffsets[i] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x4000) - 0x8000];
      }
} /* NESMapper2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper67(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #2 (ROM only).                           *
 *                                                                          *
 ****************************************************************************/
void NESMapper67(int iAddr, unsigned char ucByte)
{
int i, j;

   iAddr &= 0xf800;
   if ((iAddr & 0x800) && iAddr <= 0xb800)
      {
      // set VROM bank
      iAddr = (iAddr - 0x8800)>>1;
      j = iAddr >> 10;
      ucByte &= (pNESMachine->iVROMPages-1);
      pNESMachine->ulVROMOffsets[j] = (unsigned long)(&pNESMachine->pVROM[ucByte*0x800]-iAddr);
      pNESMachine->ulVROMOffsets[j+1] = (unsigned long)(&pNESMachine->pVROM[ucByte*0x800]-iAddr);
      }
   else
      switch (iAddr)
         {
         case 0xc000:
         case 0xc800:
            if (!pNESMachine->bSunToggle)
               {
               pNESMachine->iIRQCount &= 0xff;
               pNESMachine->iIRQCount |= (ucByte << 8);
               }
            else
               {
               pNESMachine->iIRQCount &= 0xff00;
               pNESMachine->iIRQCount |= ucByte;
               }
            pNESMachine->bSunToggle ^= 1;
            break;
         case 0xd800:
            pNESMachine->bSunToggle = 0;
            pNESMachine->bIRQCounter = ucByte & 0x10;
            pNESMachine->iIRQCount = pNESMachine->iIRQCount / 114;
            break;
         case 0xe800: // mirroring
            switch (ucByte & 3)
               {
               case 0: // horizontal
                  NESSetMirror(pNESMachine, 0,0,1,1);
                  break;
               case 1: // vertical
                  NESSetMirror(pNESMachine, 0,1,0,1);
                  break;
               case 2: // all pages at 0x2000
                  NESSetMirror(pNESMachine, 0,0,0,0);
                  break;
               case 3: // all pages at 0x2400
                  NESSetMirror(pNESMachine, 1,1,1,1);
                  break;
               }
            break;
         case 0xf800:
            pNESMachine->ucPRGBanks[0] = ucByte & (pNESMachine->iROMPages-1);
            for (i=8; i<12; i++) // only lower bank can change
               pNESMachine->regs6502.ulOffsets[i] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x4000) - 0x8000];
            break;
         }
} /* NESMapper67() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper71(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #71 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper71(int iAddr, unsigned char ucByte)
{
int i;

   switch (iAddr & 0xf000)
      {
      case 0xf000:
      case 0xe000:
      case 0xd000:
      case 0xc000: // set 16K rom bank at 0x8000
         ucByte &= (pNESMachine->iROMPages-1);
         if (ucByte != pNESMachine->ucPRGBanks[0])
            {
            pNESMachine->ucPRGBanks[0] = ucByte;
            for (i=8; i<12; i++) // only lower bank can change
               pNESMachine->regs6502.ulOffsets[i] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x4000) - 0x8000];
            }
         break;
      case 0x9000: // mirror all VRAM pages
         if (ucByte & 0x10)
            NESSetMirror(pNESMachine, 2,2,2,2);
         else
            NESSetMirror(pNESMachine, 0,0,0,0);
         break;
      }
} /* NESMapper71() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper3(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #3 (VROM only).                          *
 *                                                                          *
 ****************************************************************************/
void NESMapper3(int iAddr, unsigned char ucByte)
{
int i;

   ucByte &= (pNESMachine->iVROMPages-1);
   if (ucByte != pNESMachine->ucCHRBanks[0])
      {
      pNESMachine->ucCHRBanks[0] = ucByte;
      for (i=0; i<8; i++) // switch all 8 pages
         pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[0] * 0x2000]);
      }
} /* NESMapper3() */

void NESMapper10SetPages(BOOL bHigh)
{
int i;
unsigned char ucPage0, ucPage1;

   if (!bHigh) // set low bank
      {
      if (pNESMachine->ucCHRBanks[4] == 0xFD) // FD mapping
         ucPage0 = pNESMachine->ucCHRBanks[0];
      else
         ucPage0 = pNESMachine->ucCHRBanks[1];
      for (i=0; i<4; i++) // switch all 4 pages
         pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(ucPage0 * 0x1000)-0x0000]);
      }
   else // set the high bank
      {
      if (pNESMachine->ucCHRBanks[5] == 0xFD) // FD mapping
         ucPage1 = pNESMachine->ucCHRBanks[2];
      else
         ucPage1 = pNESMachine->ucCHRBanks[3];
      for (i=4; i<8; i++) // switch all 4 pages
         pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(ucPage1 * 0x1000)-0x1000]);
      }
} /* NESMapper10SetPages() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper9(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #9 (ROM+VROM).                           *
 *                                                                          *
 ****************************************************************************/
void NESMapper9(int iAddr, unsigned char ucByte)
{
int i;

   switch (iAddr & 0xf000)
      {
      case 0xa000: // map 8K page to 0x8000
         ucByte &= (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x8000];
         pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x8000];
         break;
      case 0xb000: // select 4K VROM bank at 0x0000 (latch0 == FD)
      case 0xc000:
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[0] = ucByte; // save latched value
         for (i=0; i<4; i++) // switch all 4 pages
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(ucByte * 0x1000)-0x0000]);
         break;
      case 0xd000: // select 4K VROM bank at 0x1000 (latch1 == FD)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[2] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[5] == 0xfd) // set this bank now
            NESMapper10SetPages(TRUE);
         break;
      case 0xe000: // select 4K VROM bank at 0x1000 (latch1 == FE)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[3] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[5] == 0xfe) // set this bank now
            NESMapper10SetPages(TRUE);
         break;
      case 0xf000: // Mirroring select
         if (ucByte & 1) // horizontal
            NESSetMirror(pNESMachine, 0,0,1,1);
         else
            NESSetMirror(pNESMachine, 0,1,0,1);
         break;
      }
} /* NESMapper9() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper10(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #10 (ROM+VROM).                          *
 *                                                                          *
 ****************************************************************************/
void NESMapper10(int iAddr, unsigned char ucByte)
{
   switch (iAddr & 0xf000)
      {
      case 0xa000: // map 16K page to 0x8000
         ucByte &= (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x4000) - 0x8000];
         pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x4000) - 0x8000];
         pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x4000) - 0x8000];
         pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x4000) - 0x8000];
         break;
      case 0xb000: // select 4K VROM bank at 0x0000 (latch0 == FD)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[0] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[4] == 0xfd) // set this bank now
            NESMapper10SetPages(FALSE);
         break;
      case 0xc000: // select 4K VROM bank at 0x0000 (latch0 == FE)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[1] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[4] == 0xfe) // set this bank now
            NESMapper10SetPages(FALSE);
         break;
      case 0xd000: // select 4K VROM bank at 0x1000 (latch1 == FD)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[2] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[5] == 0xfd) // set this bank now
            NESMapper10SetPages(TRUE);
         break;
      case 0xe000: // select 4K VROM bank at 0x1000 (latch1 == FE)
         ucByte &= (pNESMachine->iVROMPages-1);
         pNESMachine->ucCHRBanks[3] = ucByte; // save latched value
         if (pNESMachine->ucCHRBanks[5] == 0xfe) // set this bank now
            NESMapper10SetPages(TRUE);
         break;
      case 0xf000: // Mirroring select
         if (ucByte & 1) // horizontal
            NESSetMirror(pNESMachine, 0,0,1,1);
         else
            NESSetMirror(pNESMachine, 0,1,0,1);
         break;
      }
} /* NESMapper10() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper69(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #69 (ROM+VROM).                          *
 *                                                                          *
 ****************************************************************************/
void NESMapper69(int iAddr, unsigned char ucByte)
{

   switch (iAddr & 0xf000)
      {
      case 0x8000: // command
         pNESMachine->ucCartCMD = ucByte & 0xf;
         break;
      case 0xa000: // Take Action
         switch (pNESMachine->ucCartCMD)
            {
            case 0: // VROM pages
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               pNESMachine->ulVROMOffsets[pNESMachine->ucCartCMD] = (unsigned long)(&pNESMachine->pVROM[(ucByte * 0x400)-(pNESMachine->ucCartCMD*0x400)]);
               break;
            case 8: // ROM/RAM at 0x6000:
               if (ucByte & 0x40)
                  {
                  if (ucByte & 0x80) // select WRAM
                     {
                     pNESMachine->bRAMEnable = TRUE;
                     pNESMachine->regs6502.ulOffsets[0x6] = (unsigned long)pNESMachine->mem_map;
                     pNESMachine->regs6502.ulOffsets[0x7] = (unsigned long)pNESMachine->mem_map;
                     }
                  }
               else
                  {
                  pNESMachine->bRAMEnable = FALSE;
                  ucByte &= (pNESMachine->iROMPages-1);
                  pNESMachine->regs6502.ulOffsets[0x6] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x6000];
                  pNESMachine->regs6502.ulOffsets[0x7] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x6000];
                  }
               break;
            case 9: // ROM at 0x8000:
               ucByte &= (pNESMachine->iROMPages-1);
               pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x8000];
               pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0x8000];
               break;
            case 0xa: // ROM at 0xa000:
               ucByte &= (pNESMachine->iROMPages-1);
               pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0xa000];
               pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0xa000];
               break;
            case 0xb: // ROM at 0xc000:
               ucByte &= (pNESMachine->iROMPages-1);
               pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0xc000];
               pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)&pNESMachine->pBankROM[(ucByte*0x2000) - 0xc000];
               break;
            case 0xc: // mirror
               switch (ucByte & 3)
                  {
                  case 0: // horizontal
                     NESSetMirror(pNESMachine, 0,0,1,1);
                     break;
                  case 1: // vertical
                     NESSetMirror(pNESMachine, 0,1,0,1);
                     break;
                  case 2: // all pages at 0x2000
                     NESSetMirror(pNESMachine, 0,0,0,0);
                     break;
                  case 3: // all pages at 0x2400
                     NESSetMirror(pNESMachine, 1,1,1,1);
                     break;
                  }
               break;
            case 0xd: // IRQ enable
               pNESMachine->bIRQCounter = (ucByte == 0x81);
               break;
            case 0xe: // IRQ clock lower
               pNESMachine->iIRQCount &= 0xff00;
               pNESMachine->iIRQCount |= ucByte;
               pNESMachine->iIRQStartCount = -1; //iIRQCount;
               break;
            case 0xf: // IRQ clock upper
               pNESMachine->iIRQCount &= 0xff;
               pNESMachine->iIRQCount |= (ucByte << 8);
               pNESMachine->iIRQCount /= 114; // bad approximation of clocks per scanline
               pNESMachine->iIRQStartCount = -1; //iIRQCount;
               break;
            }
         break;
      }
} /* NESMapper69() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper4PRGSync(void)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #4 - update the pages.                   *
 *                                                                          *
 ****************************************************************************/
void NESMapper4PRGSync(unsigned char ucCMD)
{
   if (ucCMD & 0x40)
      {
      pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-2) * 0x2000) - 0x8000];
      pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-2) * 0x2000) - 0x8000];

      pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x2000) - 0xc000];
      pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x2000) - 0xc000];
      }
   else
      {
      // "switchable" pages
      pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x2000) - 0x8000];
      pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0] * 0x2000) - 0x8000];
      // "fixed" pages
      pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-2) * 0x2000) - 0xc000];
      pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-2) * 0x2000) - 0xc000];
      }
   pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[1] * 0x2000) - 0xa000];
   pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[1] * 0x2000) - 0xa000];
   pNESMachine->regs6502.ulOffsets[0xe] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-1) * 0x2000) - 0xe000];
   pNESMachine->regs6502.ulOffsets[0xf] = (unsigned long)&pNESMachine->pBankROM[((pNESMachine->iROMPages-1) * 0x2000) - 0xe000];
} /* NESMapper4PRGSync() */

void NESMapper4CHRSync(unsigned char ucCMD)
{
int i, j, iXOR;
   if (pNESMachine->iVROMPages)
      {
      // Sync VROM pages
      iXOR = (ucCMD & 0x80) >> 5; // 4K XOR of VROM address
      for (i=0; i<8; i++) // reset all 8 pages
         {
         j = i ^ iXOR;
         pNESMachine->ulVROMOffsets[j] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[i]*0x400] - j*0x400);
         }
      }
} /* NESMapper4CHRSync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper4(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #4 (ROM + VROM).                         *
 *                                                                          *
 ****************************************************************************/
void NESMapper4(int iAddr, unsigned char ucByte)
{
int iVPage, iPage;
int i;

   switch (iAddr & 0xe001)
      {
      case 0x8000:
         if ((ucByte&0x40) != (pNESMachine->ucCartCMD & 0x40))
            NESMapper4PRGSync(ucByte);
         if ((ucByte&0x80) != (pNESMachine->ucCartCMD & 0x80))
            NESMapper4CHRSync(ucByte);
         pNESMachine->ucCartCMD = ucByte;
         break;
      case 0x8001: // page number of command
         iVPage = ucByte & (pNESMachine->iVROMPages-1);
         iPage = ucByte & (pNESMachine->iROMPages-1);
         switch (pNESMachine->ucCartCMD & 7)
            {
            case 0: // select 2 1K VROM pages at 0x0000
               pNESMachine->ucCHRBanks[0] = iVPage & 0xfe;
               pNESMachine->ucCHRBanks[1] = (iVPage & 0xfe) | 1;
               break;
            case 1: // select 2 1K VROM pages at 0x0800
               pNESMachine->ucCHRBanks[2] = iVPage & 0xfe;
               pNESMachine->ucCHRBanks[3] = (iVPage & 0xfe) | 1;
               break;
            case 2: // select 1K VROM page at 0x1000
            case 3: // select 1K VROM page at 0x1400
            case 4: // select 1K VROM page at 0x1800
            case 5: // select 1K VROM page at 0x1c00
               i = 2+(pNESMachine->ucCartCMD&7);
               pNESMachine->ucCHRBanks[i] = iVPage;
               break;
            case 6: // select first switchable ROM page
               pNESMachine->ucPRGBanks[0] = iPage;
               break;
            case 7: // select second switchable ROM page
               pNESMachine->ucPRGBanks[1] = iPage;
               break;
            }
         if ((pNESMachine->ucCartCMD & 7) < 6)
            NESMapper4CHRSync(pNESMachine->ucCartCMD);
         else
            NESMapper4PRGSync(pNESMachine->ucCartCMD);
         break;
      case 0xa000: // Mirroring select
         if (ucByte & 1) // horizontal
            NESSetMirror(pNESMachine, 0,0,1,1);
         else
            NESSetMirror(pNESMachine, 0,1,0,1);
         break;
      case 0xa001:
//         pNESMachine->bRAMEnable = ucByte & 0x80; // saveram enable (bit 7)
         break;
      case 0xc000: // set both current count and latch
         pNESMachine->iIRQStartCount = pNESMachine->iIRQCount = ucByte;// - 1;
         break;
      case 0xc001: // set latch only
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
         break;
      case 0xe000: // disable counter
         pNESMachine->bIRQCounter = FALSE;
         break;
      case 0xe001: // enable counter
         pNESMachine->bIRQCounter = TRUE;
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
         break;
      }
} /* NESMapper4() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper5Read(int)                                        *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #5 (ROM + VROM).                         *
 *                                                                          *
 ****************************************************************************/
unsigned char NESMapper5Read(int iAddr)
{
unsigned char c = 0;
unsigned short us;

   switch (iAddr)
      {
      case 0x5204: // irq status
         c = pNESMachine->mem_map[0x5204];
         break;
      case 0x5205: // 8x8 multiply result low
         us = pNESMachine->mem_map[0x5205] * pNESMachine->mem_map[0x5206];
         c = (unsigned char)us;
         break;
      case 0x5206: // 8x8 multiply result high
         us = pNESMachine->mem_map[0x5205] * pNESMachine->mem_map[0x5206];
         c = (unsigned char)(us >> 8);
         break;
      default:
         if (iAddr >= 0x5c00) // EXRAM
            return pNESMachine->mem_map[iAddr];
         break;
      }
   return c;

} /* NESMapper5Read() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper5(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #5 (ROM + VROM).                         *
 *                                                                          *
 ****************************************************************************/
void NESMapper5(int iAddr, unsigned char ucByte)
{
int iVPage, iPage;

   iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
   iPage = ucByte & (pNESMachine->iROMPages-1);
   pNESMachine->mem_map[iAddr] = ucByte;

   switch (iAddr)
      {
      case 0x5100: // PRG Bank size 0: 32k 1:16k 2,3:8k
         pNESMachine->ucPRGBanks[iAddr & 3] = ucByte & 3;
         break;
      case 0x5101: // CHR Bank size
         pNESMachine->ucPRGBanks[iAddr & 3] = ucByte & 3; // 0 = 1x8K, 1=2x4K, 2=3x2K, 3=4x1K
         break;

      case 0x5102: // RAM write enable
      case 0x5103:
         pNESMachine->ucPRGBanks[iAddr & 3] = (ucByte & 3); // lower 2 bits must be 10b and 01b
         if (pNESMachine->ucPRGBanks[2] == 2 && pNESMachine->ucPRGBanks[3] == 1)
            pNESMachine->bRAMEnable = TRUE;
         else
            pNESMachine->bRAMEnable = FALSE;
         break;
      case 0x5104: // EXRAM mode setting
         pNESMachine->ucPRGBanks[4] = ucByte & 3;
         break;
      case 0x5105: // name table selection
         pNESMachine->iNameTable = ucByte & 3; // name table is at 0x2000, 0x2400, 0x2800, 0x2c00 (0-3)
         break;

      case 0x5113: // Enable RAM page at 0x6000-0x7fff (bit 2)
         pNESMachine->bRAMEnable = ucByte & 4;
         break;

      case 0x5114: // select 8K ROM bank at 0x8000.  If bit 7 clear, set to RAM bank
         if (!(ucByte & 0x80)) // map FFs
            { // RAM Bank
            pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0x8000]);
            pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0x8000]);
            }
         else
            { // ROM Bank
            pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0x8000]);
            pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0x8000]);
            }
         break;

      case 0x5115: // select 8K ROM bank at 0xa000.  If bit 7 clear, set to RAM bank
         if (!(ucByte & 0x80))
            { // RAM Bank
            pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xa000]);
            pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xa000]);
            }
         else
            { // ROM Bank
            pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xa000]);
            pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xa000]);
            }
         break;

      case 0x5116: // select 8K ROM bank at 0xc000.  If bit 7 clear, set to RAM bank
         if (!(ucByte & 0x80))
            { // RAM Bank
            pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xc000]);
            pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xc000]);
            }
         else
            { // ROM Bank
            pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xc000]);
            pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xc000]);
            }
         break;

      case 0x5117: // select 8K ROM bank at 0xe000.  If bit 7 clear, set to RAM bank
         if (!(ucByte & 0x80))
            { // RAM Bank
            pNESMachine->regs6502.ulOffsets[0xe] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xe000]);
            pNESMachine->regs6502.ulOffsets[0xf] = (unsigned long)(&pNESMachine->pRAMBank[(iPage*0x2000) - 0xe000]);
            }
         else
            { // ROM Bank
            pNESMachine->regs6502.ulOffsets[0xe] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xe000]);
            pNESMachine->regs6502.ulOffsets[0xf] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x2000) - 0xe000]);
            }
         break;

      case 0x5120: // select 1K VROM page at 0x0000
         if (pNESMachine->ucPRGBanks[0] == 0) // only usable when 1k bank size selected
            pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - iVPage*0x400);
         break;

      case 0x5121: // select 1K VROM page at 0x0400 if 1k size, else 2k at 0x0000
         if (pNESMachine->ucPRGBanks[0] == 3)
            pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
         if (pNESMachine->ucPRGBanks[0] == 2)
            {
            pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            }
         break;

      case 0x5122: // select 1K VROM page at 0x0800
         if (pNESMachine->ucPRGBanks[0] == 3) // only usable when 1k bank size selected
            pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x800);
         break;

      case 0x5123: // select 1K VROM page at 0x0c00 if 1k size, else 2k at 0x0800 or 4k at 0x0000
         if (pNESMachine->ucPRGBanks[0] == 3) // 1k bank size
            pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0xc00);
         if (pNESMachine->ucPRGBanks[0] == 2) // 2k bank size
            {
            pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x800);
            pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x800);
            }
         if (pNESMachine->ucPRGBanks[0] == 1) // 4k bank size
            {
            pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            }
         break;

      case 0x5124: // select 1K VROM page at 0x1000
         if (pNESMachine->ucPRGBanks[0] == 3) // only usable when 1k bank size selected
            pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
         break;

      case 0x5125: // select 1K VROM page at 0x1400 if 1k size, else 2k at 0x1000
         if (pNESMachine->ucPRGBanks[0] == 3)
            pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1400);
         if (pNESMachine->ucPRGBanks[0] == 2)
            {
            pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            }
         break;

      case 0x5126: // select 1K VROM page at 0x1800
         if (pNESMachine->ucPRGBanks[0] == 3) // only usable when 1k bank size selected
            pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1800);
         break;

      case 0x5127: // select 1K VROM page at 0x1c00 if 1k size, else 2k at 0x1800 or 4k at 0x1000 or 8k at 0
         if (pNESMachine->ucPRGBanks[0] == 3) // 1k bank size
            pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1c00);
         if (pNESMachine->ucPRGBanks[0] == 2) // 2k bank size
            {
            pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1800);
            pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1800);
            }
         if (pNESMachine->ucPRGBanks[0] == 1) // 4k bank size
            {
            pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
            }
         if (pNESMachine->ucPRGBanks[0] == 0) // 8k bank size
            {
            pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
            }
         break;

      case 0x5128: // select 2 1K VROM pages at 0x0000
         pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
         pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400]);
         break;

      case 0x5129: // select 2 1K VROM pages at 0x0800
         pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x800);
         pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x800);
         break;

      case 0x512a: // select 2 1K VROM pages at 0x1000
         pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
         pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1000);
         break;

      case 0x512b: // select 2 1K VROM pages at 0x1800
         pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1800);
         pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage*0x400] - 0x1800);
         break;

      case 0x5203:
         iVPage = 0;
         break;
      case 0x5204:
         iVPage = 0;
         break;
         
      case 0x5205: // multiply LSB
      case 0x5206: // multiply MSB
         pNESMachine->mem_map[iAddr] = ucByte;
         break;
         
      default:
         if (iAddr >= 0x5c00) // EXRAM
            {
            if (pNESMachine->ucPRGBanks[4] != 3)
               pNESMachine->mem_map[iAddr] = ucByte;
            }
         break;
      }
} /* NESMapper5() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper7(int, char)                                      *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #7 (ROM only).                           *
 *                                                                          *
 ****************************************************************************/
void NESMapper7(int iAddr, unsigned char ucByte)
{
int i;

   pNESMachine->ucPRGBanks[0] = ucByte & (pNESMachine->iROMPages-1);
   for (i=8; i<16; i++)
      pNESMachine->regs6502.ulOffsets[i] = (unsigned long)(&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0]*0x8000) - 0x8000]);
   // Set up mirroring
   if (ucByte & 0x10)
      NESSetMirror(pNESMachine, 1,1,1,1); // one screen mirroring at 0x2400
   else
      NESSetMirror(pNESMachine, 0,0,0,0); // one screen mirroring at 0x2000
} /* NESMapper7() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper11(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #11 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper11(int iAddr, unsigned char ucByte)
{
int i;

   pNESMachine->ucPRGBanks[0] = ucByte & (pNESMachine->iROMPages-1);
   pNESMachine->ucCHRBanks[0] = ucByte >> 4;
   for (i=0; i<8; i++)
      {
      pNESMachine->regs6502.ulOffsets[8+i] = (unsigned long)(&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[0]*0x8000) - 0x8000]);
      pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[0] * 0x2000]);
      }
} /* NESMapper11() */

void NESMapper15Sync(void)
{
int i;
unsigned char uc, ucPage;

   switch (pNESMachine->ucPRGBanks[1])
      {
      case 0:
         for (i=0; i<4; i++)
            {
            ucPage = ((((pNESMachine->ucPRGBanks[0] & 0x7f)<<1)+i)^(pNESMachine->ucPRGBanks[0]>>7)) & (pNESMachine->iROMPages-1);
            pNESMachine->regs6502.ulOffsets[8+i*2] = (unsigned long)(&pNESMachine->pBankROM[ucPage*0x2000] - (0x8000+i*0x2000));
            pNESMachine->regs6502.ulOffsets[9+i*2] = (unsigned long)(&pNESMachine->pBankROM[ucPage*0x2000] - (0x8000+i*0x2000));
            }
         break;
      case 1:
      case 3:
         for (i=0; i<4; i++)
            {
            uc = pNESMachine->ucPRGBanks[0] & 0x7f;
            if (i >= 2 && !(pNESMachine->ucPRGBanks[1] & 2))
               uc = 0x7f;
            pNESMachine->regs6502.ulOffsets[8+i*2] = (unsigned long)(&pNESMachine->pBankROM[(((i&1)+((uc<<1)^(pNESMachine->ucPRGBanks[0]>>7))))*0x2000] - (0x8000 + i*0x2000));
            pNESMachine->regs6502.ulOffsets[9+i*2] = (unsigned long)(&pNESMachine->pBankROM[(((i&1)+((uc<<1)^(pNESMachine->ucPRGBanks[0]>>7))))*0x2000] - (0x8000 + i*0x2000));
            }
         break;
      case 2:
         for (i=0; i<4; i++)
            {
            pNESMachine->regs6502.ulOffsets[8+i*2] = (unsigned long)(&pNESMachine->pBankROM[(((pNESMachine->ucPRGBanks[0] & 0x7f)<<1)+(pNESMachine->ucPRGBanks[0]>>7))*0x2000] - (0x8000+i*0x2000));
            pNESMachine->regs6502.ulOffsets[9+i*2] = (unsigned long)(&pNESMachine->pBankROM[(((pNESMachine->ucPRGBanks[0] & 0x7f)<<1)+(pNESMachine->ucPRGBanks[0]>>7))*0x2000] - (0x8000+i*0x2000));
            }
         break;
      }
} /* NESMapper15Sync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper15(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #15 (ROM only).                          *
 *                                                                          *
 ****************************************************************************/
void NESMapper15(int iAddr, unsigned char ucByte)
{
   pNESMachine->ucPRGBanks[0] = ucByte;
   pNESMachine->ucPRGBanks[1] = iAddr & 3;
   NESMapper15Sync();
} /* NESMapper15() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper16(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #16 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper16(int iAddr, unsigned char ucByte)
{
int i, iVPage, iPage;

   iVPage = ucByte & (pNESMachine->iVROMPages-1);
   iPage = ucByte & (pNESMachine->iROMPages-1);
   switch (iAddr & 0xf)
      {
      case 0x0: // 1K VROM page at 0x0000
         pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400]);
         break;
      case 0x1: // 1K VROM page at 0x0400
         pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x400);
         break;
      case 0x2: // 1K VROM page at 0x0800
         pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x800);
         break;
      case 0x3: // 1K VROM page at 0x0c00
         pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0xc00);
         break;
      case 0x4: // 1K VROM page at 0x1000
         pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1000);
         break;
      case 0x5: // 1K VROM page at 0x1400
         pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1400);
         break;
      case 0x6: // 1K VROM page at 0x1800
         pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1800);
         break;
      case 0x7: // 1K VROM page at 0x1c00
         pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1c00);
         break;
      case 0x8: // ROM at 0x8000
         if (iPage != pNESMachine->ucPRGBanks[0])
            {
            pNESMachine->ucPRGBanks[0] = iPage;
            for (i=8; i<12; i++)
               pNESMachine->regs6502.ulOffsets[i] = (unsigned long)(&pNESMachine->pBankROM[(iPage*0x4000) - 0x8000]);
            }
         break;
      case 0x9: // mirroring
         switch (ucByte & 3)
            {
            case 1: // horizontal
               NESSetMirror(pNESMachine, 0,0,1,1);
               break;
            case 0: // vertical
               NESSetMirror(pNESMachine, 0,1,0,1);
               break;
            case 2: // all pages at 0x2000
               NESSetMirror(pNESMachine, 0,0,0,0);
               break;
            case 3: // all pages at 0x2400
               NESSetMirror(pNESMachine, 1,1,1,1);
               break;
            }
         break;
      case 0xa: // irq control
         pNESMachine->bIRQCounter = ucByte & 1;
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount / 140; //114;
         break;
      case 0xb: // low byte of IRQ counter
         pNESMachine->iIRQStartCount &= 0xff00;
         pNESMachine->iIRQStartCount |= ucByte;
         break;
      case 0xc: // high byte of IRQ counter
         pNESMachine->iIRQStartCount &= 0xff;
         pNESMachine->iIRQStartCount |= (ucByte  << 8);
//         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount / 114;
         break;
      }
} /* NESMapper16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper19Sync(void)                                      *
 *                                                                          *
 *  PURPOSE    : Synchronize the ROM+VROM pages.                            *
 *                                                                          *
 ****************************************************************************/
void NESMapper19Sync(void)
{
int i;

   // Map program ROM
   for (i=0; i<4; i++)
      {
      pNESMachine->regs6502.ulOffsets[8+i*2] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[i] & (pNESMachine->iROMPages-1)) * 0x2000 - ((i+4) * 0x2000)];
      pNESMachine->regs6502.ulOffsets[9+i*2] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[i] & (pNESMachine->iROMPages-1)) * 0x2000 - ((i+4) * 0x2000)];
      }

   // Map VROM
   for (i=0; i<8; i++)
      {
      if ((!(pNESMachine->ucPRGBanks[1] & (i & 4 ? 0x80 : 0x40))) && ((pNESMachine->ucCHRBanks[i] & 0xE0) == 0xE0))
         pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVRAM[(pNESMachine->ucCHRBanks[i] & 7) * 0x400] - (i * 0x400));
		else
         pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[i] & (pNESMachine->iVROMPages-1)) * 0x400] - (i*0x400));
      }

   // Mirror / special
	NESSetMirror(pNESMachine, pNESMachine->ucPRGBanks[4] & 1, pNESMachine->ucPRGBanks[5] & 1, pNESMachine->ucPRGBanks[6] & 1, pNESMachine->ucPRGBanks[7] & 1);
   for (i=0; i<4; i++)
      {
      if ((pNESMachine->ucPRGBanks[i+4] & 0xE0) != 0xE0)
         pNESMachine->ulVRAMOffsets[i] = (unsigned long)&pNESMachine->pVROM[(pNESMachine->ucPRGBanks[4+i] & (pNESMachine->iVROMPages-1))*0x400];
      }

} /* NESMapper19Sync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper19Read(int)                                       *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #19 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
unsigned char NESMapper19Read(int iAddr)
{
   if (iAddr & 0x800)
      return (char)(pNESMachine->iIRQTempCount >> 8);
   else
      return (char)pNESMachine->iIRQTempCount;
} /* NESMapper19Read() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper19(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #19 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper19(int iAddr, unsigned char ucByte)
{

   switch (iAddr & 0xf000)
      {
      case 0x5000: // line counter
         if (iAddr & 0x800)
            {
            pNESMachine->bIRQCounter = ucByte & 0x80;
            pNESMachine->iIRQTempCount &= 0xff;
            pNESMachine->iIRQTempCount |= ((ucByte & 0x7f)<<8);
            pNESMachine->iIRQCount = pNESMachine->iIRQStartCount = ((0x7fff - pNESMachine->iIRQTempCount) / 114);
            }
         else
            {
            pNESMachine->iIRQTempCount &= 0x7f00;
            pNESMachine->iIRQTempCount |= ucByte;
            }
         break;
      case 0x8000:
         if (iAddr & 0x800)
            pNESMachine->ucCHRBanks[1] = ucByte;
         else
            pNESMachine->ucCHRBanks[0] = ucByte;
         NESMapper19Sync();
         break;
      case 0x9000:
         if (iAddr & 0x800)
            pNESMachine->ucCHRBanks[3] = ucByte;
         else
            pNESMachine->ucCHRBanks[2] = ucByte;
         NESMapper19Sync();
         break;
      case 0xa000:
         if (iAddr & 0x800)
            pNESMachine->ucCHRBanks[5] = ucByte;
         else
            pNESMachine->ucCHRBanks[4] = ucByte;
         NESMapper19Sync();
         break;
      case 0xb000:
         if (iAddr & 0x800)
            pNESMachine->ucCHRBanks[7] = ucByte;
         else
            pNESMachine->ucCHRBanks[6] = ucByte;
         NESMapper19Sync();
         break;
      case 0xc000:
         if (iAddr & 0x800)
            pNESMachine->ucPRGBanks[5] = ucByte;
         else
            pNESMachine->ucPRGBanks[4] = ucByte;
         NESMapper19Sync();
         break;
      case 0xd000:
         if (iAddr & 0x800)
            pNESMachine->ucPRGBanks[7] = ucByte;
         else
            pNESMachine->ucPRGBanks[6] = ucByte;
         NESMapper19Sync();
         break;
      case 0xe000:
         if (iAddr & 0x800)
            pNESMachine->ucPRGBanks[1] = ucByte;
         else
            pNESMachine->ucPRGBanks[0] = ucByte;
         NESMapper19Sync();
         break;
      case 0xf000:
         if (!(iAddr & 0x800))
            pNESMachine->ucPRGBanks[2] = ucByte;
         NESMapper19Sync();
         break;
      }

} /* NESMapper19() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper23(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #23 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper23(int iAddr, unsigned char ucByte)
{
int i, iPage;
unsigned char ucTemp;

   iPage = ucByte & (pNESMachine->iROMPages-1);
   switch (iAddr & 0xf000)
      {
      case 0x8000: // map 8K ROM bank at 0x8000
         if (pNESMachine->ucPRGBanks[3] & 2) // controls setting 0x8 vs 0xc bank
            {
            pNESMachine->ucPRGBanks[2] = iPage;
            pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xc000]);
            pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xc000]);
            }
         else
            {
            pNESMachine->ucPRGBanks[0] = iPage;
            pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
            pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
            }
         break;
      case 0x9000: // mirror control
         if (iAddr == 0x9000)
            {
            switch (ucByte & 3)
               {
               case 0: // Vertical
                  NESSetMirror(pNESMachine, 0,1,0,1);
                  break;
               case 1: // Horizontal
                  NESSetMirror(pNESMachine, 0,0,1,1);
                  break;
               case 2: // S0
                  NESSetMirror(pNESMachine, 0,0,0,0);
                  break;
               case 3: // S1
                  NESSetMirror(pNESMachine, 1,1,1,1);
                  break;
               }
            }
         else
            {
            if (pNESMachine->ucPRGBanks[3] != (ucByte & 2)) // swap banks
               {
               pNESMachine->ucPRGBanks[3] = ucByte & 2;
               ucTemp = pNESMachine->ucPRGBanks[0];
               pNESMachine->ucPRGBanks[0] = pNESMachine->ucPRGBanks[2];
               pNESMachine->ucPRGBanks[2] = ucTemp; // swap them
               pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[0]*0x2000 - 0x8000]);
               pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[0]*0x2000 - 0x8000]);
               pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[2]*0x2000 - 0xc000]);
               pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[2]*0x2000 - 0xc000]);
               }
            }
         break;
      case 0xa000: // map 8K ROM bank at 0xa000
         pNESMachine->ucPRGBanks[1] = iPage;
         pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);
         pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);
         break;
      case 0xb000: // VROM at 0x0000 & 0x0400
      case 0xc000: // VROM at 0x0800 & 0x0c00
      case 0xd000: // VROM at 0x1000 & 0x1400
      case 0xe000: // VROM at 0x1800 & 0x1c00
         iAddr |= ((iAddr>>2) & 3) | ((iAddr>>4)&3) | ((iAddr>>6) & 3);
         iAddr &= 0xf003;
         if (iAddr >= 0xb000 && iAddr <= 0xe003)
            {
            i = ((iAddr >> 1) & 1) | ((iAddr-0xb000)>>11);
            pNESMachine->ucCHRBanks[i] &= (0xf0)>>((iAddr&1)<<2);
            pNESMachine->ucCHRBanks[i] |= (ucByte & 0xf)<<((iAddr&1)<<2);
            pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[i]*0x400] - 0x400*i);
            }
         break;
      case 0xf000:
         {
         switch (iAddr)
            {
            case 0xf000:
               pNESMachine->iIRQStartCount &= 0xf0;
               pNESMachine->iIRQStartCount |= ucByte & 0xf;
               break;
            case 0xf002:
               pNESMachine->iIRQStartCount &= 0xf;
               pNESMachine->iIRQStartCount |= (ucByte <<4);
               break;
            case 0xf001:
               pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
               pNESMachine->bIRQCounter = ucByte & 2;
               pNESMachine->ucIRQLatch = ucByte & 1;
               break;
            case 0xf003:
               pNESMachine->bIRQCounter = pNESMachine->ucIRQLatch;
               break;
            }
         }
         break;
      }
} /* NESMapper23() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper25(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #25 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper25(int iAddr, unsigned char ucByte)
{
int i, iPage;

   iPage = ucByte & (pNESMachine->iROMPages-1);
   iAddr = (iAddr & 0xf003) | ((iAddr & 0xc)>>2);
   if ((iAddr & 0xf000) == 0xa000)
      {
      pNESMachine->ucPRGBanks[1] = iPage;
      pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);
      pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);
      }
   else if (iAddr >= 0xb000 && iAddr <= 0xefff) // VROM
      {
      i = (iAddr & 1) | ((iAddr-0xb000)>>11);
      pNESMachine->ucCHRBanks[i] &= (0xf0)>>((iAddr&2)<<1);
      pNESMachine->ucCHRBanks[i] |= (ucByte & 0xf)<<((iAddr&2)<<1);
      pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[i]*0x400] - 0x400*i);
      }
   else if ((iAddr &0xf000) == 0x8000)
      {
      if (pNESMachine->ucPRGBanks[4] & 2)
         {
         pNESMachine->ucPRGBanks[2] = iPage;
         pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xc000]);
         pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xc000]);
         }
      else
         {
         pNESMachine->ucPRGBanks[0] = iPage;
         pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
         pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
         }
      }
   else switch (iAddr)
      {
      case 0x9000:
         switch (ucByte & 3)
            {
            case 0: // Vertical
               NESSetMirror(pNESMachine, 0,1,0,1);
               break;
            case 1: // Horizontal
               NESSetMirror(pNESMachine, 0,0,1,1);
               break;
            case 2: // S0
               NESSetMirror(pNESMachine, 0,0,0,0);
               break;
            case 3: // S1
               NESSetMirror(pNESMachine, 1,1,1,1);
               break;
            }
         break;
      case 0x9001:
         if ((pNESMachine->ucPRGBanks[4] & 2) != (ucByte & 2))
            {
            unsigned char ucSwap = pNESMachine->ucPRGBanks[0];
            pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[2]*0x2000 - 0x8000]);
            pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[2]*0x2000 - 0x8000]);
            pNESMachine->ucPRGBanks[0] =  pNESMachine->ucPRGBanks[2];           
            pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)(&pNESMachine->pBankROM[ucSwap*0x2000 - 0xc000]);
            pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)(&pNESMachine->pBankROM[ucSwap*0x2000 - 0xc000]);
            pNESMachine->ucPRGBanks[2] = ucSwap;
            pNESMachine->ucPRGBanks[4] = ucByte;
            }
         break;
      case 0xf000: 
         pNESMachine->iIRQStartCount &= 0xf0;
         pNESMachine->iIRQStartCount |= ucByte & 0xf;
         break;
      case 0xf002:
         pNESMachine->iIRQStartCount &= 0xf;
         pNESMachine->iIRQStartCount |= (ucByte <<4);
         break;
      case 0xf001:
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
         pNESMachine->bIRQCounter = ucByte & 2;
         pNESMachine->ucIRQLatch = ucByte & 1;
         break;
      case 0xf003:
         pNESMachine->bIRQCounter = pNESMachine->ucIRQLatch;
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount - iScanLine - 22;
         break;
      }
} /* NESMapper25() */

void NESMapper33Sync(void)
{
int iPage;

   iPage = pNESMachine->ucPRGBanks[0] & (pNESMachine->iROMPages-1);
   pNESMachine->regs6502.ulOffsets[8] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
   pNESMachine->regs6502.ulOffsets[9] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0x8000]);
   iPage = pNESMachine->ucPRGBanks[1] & (pNESMachine->iROMPages-1);
   pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);
   pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x2000 - 0xa000]);

   iPage = pNESMachine->ucCHRBanks[0] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0000);
   pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0000);
   iPage = pNESMachine->ucCHRBanks[1] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0800);
   pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0800);

   iPage = pNESMachine->ucCHRBanks[2] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x400] - 0x1000);
   iPage = pNESMachine->ucCHRBanks[3] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x400] - 0x1400);
   iPage = pNESMachine->ucCHRBanks[4] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x400] - 0x1800);
   iPage = pNESMachine->ucCHRBanks[5] & (pNESMachine->iVROMPages-1);
   pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x400] - 0x1c00);

   if (pNESMachine->ucPRGBanks[0] & 0x40)
      NESSetMirror(pNESMachine, 0,0,1,1); // H
   else
      NESSetMirror(pNESMachine, 0,1,0,1); // V
                  

} /* NESMapper33Sync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper33(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #33 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper33(int iAddr, unsigned char ucByte)
{
   switch (iAddr & 0xf000)
      {
      case 0x8000:
      case 0x9000:
         switch (iAddr & 3)
            {
            case 0:
               pNESMachine->ucPRGBanks[0] = ucByte;
               break;
            case 1:
               pNESMachine->ucPRGBanks[1] = ucByte;
               break;
            case 2:
               pNESMachine->ucCHRBanks[0] = ucByte;
               break;
            case 3:
               pNESMachine->ucCHRBanks[1] = ucByte;
               break;
            }
         NESMapper33Sync();
         break;
      case 0xa000:
      case 0xb000:
         switch (iAddr & 3)
            {
            case 0:
               pNESMachine->ucCHRBanks[2] = ucByte;
               break;
            case 1:
               pNESMachine->ucCHRBanks[3] = ucByte;
               break;
            case 2:
               pNESMachine->ucCHRBanks[4] = ucByte;
               break;
            case 3:
               pNESMachine->ucCHRBanks[5] = ucByte;
               break;
            }
         NESMapper33Sync();
         break;
      case 0xc000:
         pNESMachine->iIRQLatch = 256 - ucByte;
         pNESMachine->iIRQStartCount = -1;
         break;
      case 0xc001:
         pNESMachine->iIRQCount = pNESMachine->iIRQLatch;
         break;
      case 0xc002:
         pNESMachine->bIRQCounter = TRUE;
         break;
      case 0xc003:
         pNESMachine->bIRQCounter = FALSE;
         break;
      }
} /* NESMapper33() */

void NESMapper68Sync(void)
{
int i;

         i = pNESMachine->ucCHRBanks[2] & 3;
         if (pNESMachine->ucCHRBanks[2] & 0x10) // VROM usage
            {
            switch (i)
               {
               case 0:
                  pNESMachine->ulVRAMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  break;
               case 1:
                  pNESMachine->ulVRAMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  break;
               case 2:
                  pNESMachine->ulVRAMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[0]|0x80)*0x400]);
                  break;
               case 3:
                  pNESMachine->ulVRAMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  pNESMachine->ulVRAMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[(pNESMachine->ucCHRBanks[1]|0x80)*0x400]);
                  break;
               }
            }
         else
            {
            switch (i)
               {
               case 0: // Vertical
                  NESSetMirror(pNESMachine, 0,1,0,1);
                  break;
               case 1: // Horizontal
                  NESSetMirror(pNESMachine, 0,0,1,1);
                  break;
               case 2: // S0
                  NESSetMirror(pNESMachine, 0,0,0,0);
                  break;
               case 3: // S1
                  NESSetMirror(pNESMachine, 1,1,1,1);
                  break;
               }
            }

} /* NESMapper68Sync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper68(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #68 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper68(int iAddr, unsigned char ucByte)
{
int i, iPage;

   switch (iAddr & 0xf000)
      {
      case 0x8000:
         iPage = ucByte & (pNESMachine->iVROMPages-1);
         pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0000);
         pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0000);
         break;
      case 0x9000:
         iPage = ucByte & (pNESMachine->iVROMPages-1);
         pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0800);
         pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x0800);
         break;
      case 0xa000:
         iPage = ucByte & (pNESMachine->iVROMPages-1);
         pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x1000);
         pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x1000);
         break;
      case 0xb000:
         iPage = ucByte & (pNESMachine->iVROMPages-1);
         pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x1800);
         pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iPage * 0x800] - 0x1800);
         break;
      case 0xc000:
         // VROM low mirroring
         pNESMachine->ucCHRBanks[0] = ucByte;
         NESMapper68Sync();
         break;
      case 0xd000:
         // VROM high mirroring
         pNESMachine->ucCHRBanks[1] = ucByte;
         NESMapper68Sync();
         break;
      case 0xe000: // mirroring
         pNESMachine->ucCHRBanks[2] = ucByte;
         NESMapper68Sync();
         break;
      case 0xf000: // program bank
         iPage = ucByte & (pNESMachine->iROMPages-1);
         for (i=8; i<12; i++)
            pNESMachine->regs6502.ulOffsets[i] = (unsigned long)(&pNESMachine->pBankROM[iPage*0x4000 - 0x8000]);
         break;
      }
} /* NESMapper68() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper70(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #70 (ROM + VROM).                        *
 *                                                                          *
 ****************************************************************************/
void NESMapper70(int iAddr, unsigned char ucByte)
{
int i;

   if (ucByte & 0x80)
      NESSetMirror(pNESMachine, 1,1,1,1);
   else
      NESSetMirror(pNESMachine, 0,0,0,0);
   pNESMachine->ucPRGBanks[0] = (ucByte & 0x70) >> 4;
   pNESMachine->ucCHRBanks[0] = ucByte & 0xf;

   for (i=8; i<0xc; i++)
      pNESMachine->regs6502.ulOffsets[i] = (unsigned long)(&pNESMachine->pBankROM[pNESMachine->ucPRGBanks[0]*0x4000 - 0x8000]);

   for (i=0; i<8; i++)
      pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[pNESMachine->ucCHRBanks[0]*0x2000]);
} /* NESMapper70() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper87(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #87 (VROM mapped only).                  *
 *                                                                          *
 ****************************************************************************/
void NESMapper87(int iAddr, unsigned char ucByte)
{
int i, j;

   j = (ucByte & 2) ? 0x2000 : 0;
   for (i=0; i<8; i++)
      pNESMachine->ulVROMOffsets[i] = (unsigned long)(&pNESMachine->pVROM[j]);
} /* NESMapper87() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper93(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #93 (ROM bank control only).             *
 *                                                                          *
 ****************************************************************************/
void NESMapper93(int iAddr, unsigned char ucByte)
{
int i;

   i = (ucByte >> 4) & 7; // maps up to 8 pages of ROM to 0x8000-0xbfff
   pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(i * 0x4000) - 0x8000];
   pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(i * 0x4000) - 0x8000];
   pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)&pNESMachine->pBankROM[(i * 0x4000) - 0x8000];
   pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)&pNESMachine->pBankROM[(i * 0x4000) - 0x8000];

} /* NESMapper93() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper160(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #160 (ROM + VROM).                       *
 *                                                                          *
 ****************************************************************************/
void NESMapper160(int iAddr, unsigned char ucByte)
{
int iVPage, iPage;

   switch (iAddr)
      {
      case 0x8000: // CPU bank 4
         iPage = ucByte & (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0x8] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0x8000];
         pNESMachine->regs6502.ulOffsets[0x9] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0x8000];
         break;
      case 0x8001: // CPU bank 5
         iPage = ucByte & (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0xa] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xa000];
         pNESMachine->regs6502.ulOffsets[0xb] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xa000];
         break;
      case 0x8002: // CPU bank 6
         iPage = ucByte & (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0xc] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xc000];
         pNESMachine->regs6502.ulOffsets[0xd] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xc000];
         break;
      case 0x8003: // CPU bank 7
         iPage = ucByte & (pNESMachine->iROMPages-1);
         pNESMachine->regs6502.ulOffsets[0xe] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xe000];
         pNESMachine->regs6502.ulOffsets[0xf] = (unsigned long)&pNESMachine->pBankROM[(iPage * 0x2000) - 0xe000];
         break;
      case 0x9000: // PPU bank 0
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[0] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400]);
         break;
      case 0x9001: // PPU bank 1
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[1] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x400);
         break;
      case 0x9002: // PPU bank 2
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[2] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x800);
         break;
      case 0x9003: // PPU bank 3
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[3] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0xc00);
         break;
      case 0x9004: // PPU bank 4
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[4] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1000);
         break;
      case 0x9005: // PPU bank 5
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[5] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1400);
         break;
      case 0x9006: // PPU bank 6
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[6] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1800);
         break;
      case 0x9007: // PPU bank 7
         iVPage = ucByte & (pNESMachine->iVROMPages-1); // make sure the value is within the valid range
         pNESMachine->ulVROMOffsets[7] = (unsigned long)(&pNESMachine->pVROM[iVPage * 0x400] - 0x1c00);
         break;
      case 0xc000: // set both current count and enable
         pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
         pNESMachine->bIRQCounter = (BOOL)pNESMachine->iIRQStartCount;
         break;
      case 0xc001: // set latch only
         pNESMachine->iIRQStartCount = ucByte;
         break;
      case 0xc002: // disable counter
         pNESMachine->bIRQCounter = FALSE;
         break;
      case 0xc003: // set counter directly
         pNESMachine->iIRQCount = ucByte;
         break;
      }
} /* NESMapper160() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper180(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #180 (ROM only).                         *
 *                                                                          *
 ****************************************************************************/
void NESMapper180(int iAddr, unsigned char ucByte)
{
int i;

   ucByte &= 7;
   if (ucByte != pNESMachine->ucPRGBanks[1])
      {
      pNESMachine->ucPRGBanks[1] = ucByte;
      for (i=12; i<16; i++) // only upper bank can change
         pNESMachine->regs6502.ulOffsets[i] = (unsigned long)&pNESMachine->pBankROM[(pNESMachine->ucPRGBanks[1] * 0x4000) - 0xc000];
      }
} /* NESMapper180() */

void NESMapper226Sync(void)
{
uint32_t ul;
int i;

   ul = ((pNESMachine->ucPRGBanks[0]>>1)&0xf) | ((pNESMachine->ucPRGBanks[0]>>3)&0x10) | ((pNESMachine->ucPRGBanks[1]&1)<<5);
   if (pNESMachine->ucPRGBanks[0] & 0x20) // 16k pages
      {
      ul = (ul << 1) | (pNESMachine->ucPRGBanks[0] & 1);
      ul &= (pNESMachine->iROMPages-1);
      for (i=0; i<2; i++)
         {
         pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)(&pNESMachine->pBankROM[ul*0x4000]-0x8000);
         pNESMachine->regs6502.ulOffsets[0xc+i] = (unsigned long)(&pNESMachine->pBankROM[ul*0x4000]-0xc000);
         }      
      }
   else
      { // 32k pages
      ul &= ((pNESMachine->iROMPages>>1)-1);
      for (i=0; i<8; i++)
         pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)(&pNESMachine->pBankROM[ul*0x8000]-0x8000);
      }
} /* NESMapper226Sync() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper226(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #226 (ROM only).                         *
 *                                                                          *
 ****************************************************************************/
void NESMapper226(int iAddr, unsigned char ucByte)
{

   pNESMachine->ucPRGBanks[iAddr & 1] = ucByte;
   NESMapper226Sync();
   if (iAddr & 1)
      {
      if (pNESMachine->ucPRGBanks[1] & 2)
         pNESMachine->bRAMEnable = FALSE;
      else
         pNESMachine->bRAMEnable = TRUE;
      }
   else
      {
      if ((pNESMachine->ucPRGBanks[0]>>6) & 1)
         NESSetMirror(pNESMachine, 0,1,0,1); // vertical
      else
         NESSetMirror(pNESMachine, 0,0,1,1); // horizontal
      }
} /* NESMapper226() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper229(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #229 (ROM + VROM).                       *
 *                                                                          *
 ****************************************************************************/
void NESMapper229(int iAddr, unsigned char ucByte)
{
int i, iPage;

   if ((iAddr>>5) & 1)
      NESSetMirror(pNESMachine, 0,1,0,1); // vertical
   else
      NESSetMirror(pNESMachine, 0,0,1,1); // horizontal
   if (!(iAddr & 0x1e))
      { // set 32k page 0
      for (i=0;i<8;i++)
         pNESMachine->regs6502.ulOffsets[8+i] = (unsigned long)&pNESMachine->pBankROM[-0x8000];
      }
   else
      {
      iPage = (iAddr & 0x1f) & (pNESMachine->iROMPages-1);
      for (i=0; i<4; i++)
         {
         pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)&pNESMachine->pBankROM[(iPage*0x4000)-0x8000];
         pNESMachine->regs6502.ulOffsets[0xc+i] = (unsigned long)&pNESMachine->pBankROM[(iPage*0x4000)-0xc000];
         }
      // Set VROM bank
      iPage = iAddr & (pNESMachine->iVROMPages-1);
      for (i=0; i<8; i++)
         pNESMachine->ulVROMOffsets[i] = (unsigned long)&pNESMachine->pVROM[iPage*0x2000];
      }
} /* NESMapper229() */

unsigned char NESMapper255Read(int iAddr)
{
   return pNESMachine->ucPRGBanks[iAddr&3];
} /* NESMapper255Read() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESMapper255(int, char)                                    *
 *                                                                          *
 *  PURPOSE    : NES memory mapper #255 (ROM + VROM).                       *
 *                                                                          *
 ****************************************************************************/
void NESMapper255(int iAddr, unsigned char ucByte)
{
int i, iPage;
unsigned char ucBank, ucPPage, ucVPage;

   if (iAddr < 0x6000)
      pNESMachine->ucPRGBanks[iAddr&3] = ucByte & 0xf;
   else
      {
      ucBank = (iAddr>>7) & 0x1f;
      ucVPage = iAddr & 0x3f;
      ucPPage = (iAddr>>14)&1;
      if (iAddr & 0x1000) // set 16k banks
         {
         iPage = (ucPPage << 5) | ucBank | ((iAddr & 0x40)>>6);
         iPage &= (pNESMachine->iROMPages-1);
         for (i=0; i<4; i++)
            {
            pNESMachine->regs6502.ulOffsets[0x8+i] = (unsigned long)&pNESMachine->pBankROM[(iPage*0x4000)-0x8000];
            pNESMachine->regs6502.ulOffsets[0xc+i] = (unsigned long)&pNESMachine->pBankROM[(iPage*0x4000)-0xc000];
            }
         }
      else // set 32k banks
         {
         iPage = (ucPPage << 6) & (pNESMachine->iROMPages-1);
         for (i=0; i<8; i++)
            pNESMachine->regs6502.ulOffsets[8+i] = (unsigned long)&pNESMachine->pBankROM[(iPage*0x4000)-0x8000];
         }
      if ((iAddr>>13) & 1)
         NESSetMirror(pNESMachine, 0,1,0,1); // vertical
      else
         NESSetMirror(pNESMachine, 0,0,1,1); // horizontal
      // Set VROM bank
      iPage = (ucPPage<<6) | ucVPage;
      iPage &= (pNESMachine->iVROMPages-1);
      for (i=0; i<8; i++)
         pNESMachine->ulVROMOffsets[i] = (unsigned long)&pNESMachine->pVROM[iPage*0x2000];
      }
} /* NESMapper255() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESGenSound(void)                                          *
 *                                                                          *
 *  PURPOSE    : Emulate the NES sound hardware (4 channels + DCM).         *
 *                                                                          *
 ****************************************************************************/
void NESGenSound(signed short *psBuf, int iLen)
{
int i, iFreqShift, iPos, iSample, iPosDelta, iSndVol;
unsigned char c, *pWaves;

// This constant is calculate as follows:
// 1.79Mhz system clock divided by 16 for sound system = 111875
// full cycle of sound is 8 samples from a lookup table.  The way the sound
// offset is calculated, the frequency * 190.21786 is used to sweep through the
// table.  The NES frequency is 111875 / wavelength value
// so we take 111875 * 190.21786 and get 21280623
//
#define SOUND_CONST 21280623
#define SOUND_CONST2 21280623
   
//   if (b16BitAudio)
//      iLen>>=1; // number of samples (not bytes)
      
// emulate sound channel 1 (square wave with envelope and frequency sweep)
   if (pNESMachine->iSndLen1 && (pNESMachine->ucSoundChannels & 0x01) && pNESMachine->iFreq1) // if sound channel 1 is active
      {
      pWaves = &ucSquareWaves[8* (pNESMachine->ucSndCtrl1 >> 6)]; // point to correct duty cycle for sound channel 1
      iPosDelta = SOUND_CONST/pNESMachine->iFreq1; // compute freq delta for 44100Hz samples
      if (pNESMachine->ucSndCtrl1 & 0x10) // envelope disabled
         {
         iSndVol = (pNESMachine->ucSndCtrl1 & 0xf); // channel 1 volume
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iSnd1Pos >> 20) & 7;
            iSample = (pWaves[iPos] * iSndVol) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iSnd1Pos += iPosDelta;
            }
         }
      else // envelope is enabled
         {
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iSnd1Pos >> 20) & 7;
            iSample = (pWaves[iPos] * pNESMachine->iEnvVol1) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iSnd1Pos += iPosDelta;
            }
         // update envelope
         pNESMachine->iEnvPos1 += pNESMachine->iEnvDelta1;
         if (pNESMachine->iEnvPos1 >= 0x10000)
            {
            pNESMachine->iEnvPos1 &= 0xffff;
            if (pNESMachine->iEnvVol1)
               pNESMachine->iEnvVol1--;
            if (!pNESMachine->iEnvVol1)
               { // if envelope looping bit set
               if (pNESMachine->ucSndCtrl1 & 0x20)
                  pNESMachine->iEnvVol1 = 0xf; // reset to max
               else
                  pNESMachine->iSndLen1 = 0; // stop playing sound
               }
            }
         }

      if (pNESMachine->iSweepDelta1) // if frequency sweep is enabled
         {
         pNESMachine->iSweepPos1 += pNESMachine->iSweepDelta1;
         if (pNESMachine->iSweepPos1 >= 0x10000) // time to adjust frequency
            {
            pNESMachine->iSweepPos1 &= 0xffff;
            c = pNESMachine->ucSweep1 & 7; // frequence shift amount
            iFreqShift = pNESMachine->iFreq1 >> c;
            if (pNESMachine->ucSweep1 & 8) // decrease frequency
               pNESMachine->iFreq1 -= iFreqShift;
            else
               pNESMachine->iFreq1 += iFreqShift;
            if (pNESMachine->iFreq1 > 2047 || pNESMachine->iFreq1 < 8) // if hit extreme, stop
               pNESMachine->iSndLen1 = 0;
            }
         }
      } // if channel 1 on

   if (pNESMachine->iSndLen2 && (pNESMachine->ucSoundChannels & 0x02) && pNESMachine->iFreq2) // if sound channel 2 is active
      {
      pWaves = &ucSquareWaves[8* (pNESMachine->ucSndCtrl2 >> 6)]; // point to correct duty cycle for sound channel 2
      iPosDelta = SOUND_CONST/pNESMachine->iFreq2; // compute freq delta for 44100Hz samples
      if (pNESMachine->ucSndCtrl2 & 0x10) // envelope disabled
         {
         iSndVol = (pNESMachine->ucSndCtrl2 & 0xf); // channel 2 volume
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iSnd2Pos >> 20) & 7;
            iSample = (pWaves[iPos] * iSndVol) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iSnd2Pos += iPosDelta;
            }
         }
      else // envelope is enabled
         {
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iSnd2Pos >> 20) & 7;
            iSample = (pWaves[iPos] * pNESMachine->iEnvVol2) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iSnd2Pos += iPosDelta;
            }
         // update envelope
         pNESMachine->iEnvPos2 += pNESMachine->iEnvDelta2;
         if (pNESMachine->iEnvPos2 >= 0x10000)
            {
            pNESMachine->iEnvPos2 &= 0xffff;
            pNESMachine->iEnvVol2--;
            if (!pNESMachine->iEnvVol2)
               { // if envelope looping bit set
               if (pNESMachine->ucSndCtrl2 & 0x20)
                  pNESMachine->iEnvVol2 = 0xf; // reset to max
               else
                  pNESMachine->iSndLen2 = 0; // stop playing sound
               }
               
            }
         }

      if (pNESMachine->iSweepDelta2) // if frequency sweep is enabled
         {
         pNESMachine->iSweepPos2 += pNESMachine->iSweepDelta2;
         if (pNESMachine->iSweepPos2 >= 0x10000) // time to adjust frequency
            {
            pNESMachine->iSweepPos2 &= 0xffff;
            c = pNESMachine->ucSweep2 & 7; // frequence shift amount
            iFreqShift = pNESMachine->iFreq2 >> c;
            if (pNESMachine->ucSweep2 & 8) // decrease frequency
               pNESMachine->iFreq2 -= iFreqShift;
            else
               pNESMachine->iFreq2 += iFreqShift;
            if (pNESMachine->iFreq2 > 2047 || pNESMachine->iFreq2 < 8) // if hit extreme, stop
               pNESMachine->iSndLen2 = 0;
            }
         }
      } // if channel 2 on

   if (pNESMachine->iSndLen3 && pNESMachine->ucLinearCount && (pNESMachine->ucSoundChannels & 0x04)) // if sound channel 3 is active
      {
      iPosDelta = SOUND_CONST/(pNESMachine->iFreq3+1); // compute freq delta for 44100Hz samples
      for (i=0; i<iLen; i++)
         {
         iPos = (pNESMachine->iSnd3Pos >> 19) & 31;
         psBuf[i] += ucTriangles[iPos];
         pNESMachine->iSnd3Pos += iPosDelta;
         } // for each sound sample
      } // if channel 3 on

   if (pNESMachine->iSndLen4 && (pNESMachine->ucSoundChannels & 0x08) && pNESMachine->iFreq4) // if noise channel 4 is active
      {
      if (pNESMachine->iNoisePoly) // 7/15 step selection
         {
         pWaves = pNoise7;
         pNESMachine->iNoiseMask = 0x7f;
         }
      else
         {
         pWaves = pNoise15;
         pNESMachine->iNoiseMask = 0x7fff;
         }
      iPosDelta = SOUND_CONST2/pNESMachine->iFreq4;
      if (pNESMachine->ucSndCtrl2 & 0x10) // envelope disabled
         {
         iSndVol = (pNESMachine->ucSndCtrl4 & 0xf); // channel 4 volume
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iNoisePos >> 20) & pNESMachine->iNoiseMask; // get position in the wave table
            iSample = (pWaves[iPos] * iSndVol) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iNoisePos += iPosDelta;
            }
         }
      else // envelope enabled
         {
         iSndVol = (pNESMachine->ucSndCtrl4 & 0xf); // channel 4 volume
         for (i=0; i<iLen; i++)
            {
            iPos = (pNESMachine->iNoisePos >> 20) & pNESMachine->iNoiseMask; // get position in the wave table
            iSample = (pWaves[iPos] * pNESMachine->iEnvVol4) >> 4;
            psBuf[i] += iSample;
            pNESMachine->iNoisePos += iPosDelta;
            }
         // update envelope
         pNESMachine->iEnvPos4 += pNESMachine->iEnvDelta4;
         if (pNESMachine->iEnvPos4 >= 0x10000)
            {
            pNESMachine->iEnvPos4 &= 0xffff;
            pNESMachine->iEnvVol4--;
            if (!pNESMachine->iEnvVol4)
               { // if envelope looping bit set
               if (pNESMachine->ucSndCtrl4 & 0x20)
                  pNESMachine->iEnvVol4 = 0xf; // reset to max
               else
                  pNESMachine->iSndLen4 = 0; // stop playing sound
               }
            }
         }
      }

   if (!(pNESMachine->ucSndCtrl1 & 0x20) && pNESMachine->iSndLen1) // if counter enabled
      pNESMachine->iSndLen1--; // decrement the counter once each frame
   if (!(pNESMachine->ucSndCtrl2 & 0x20) && pNESMachine->iSndLen2) // if counter enabled
      pNESMachine->iSndLen2--; // decrement the counter once each frame
   if (!(pNESMachine->ucSndCtrl3 & 0x80) && pNESMachine->iSndLen3) // if counter enabled
      {
      pNESMachine->iSndLen3--; // decrement the counter once each frame
      if (pNESMachine->ucLinearCount)
         pNESMachine->ucLinearCount -= 2; // it is clocked at 240hz, and this routine is clocked at 120hz
      }
   if (!(pNESMachine->ucSndCtrl4 & 0x20) && pNESMachine->iSndLen4) // if counter enabled
      pNESMachine->iSndLen4--; // decrement the counter once each frame

} /* NESGenSound() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESGenSamples(char *, int)                                 *
 *                                                                          *
 *  PURPOSE    : Emulate the NES DMC sound hardware.                        *
 *                                                                          *
 ****************************************************************************/
void NESGenSamples(signed short *psBuf, int iLen)
{
int i, j, k, iOut, iSum, iDelta, iSample, iSampleCount;
unsigned char cData, cNewData, *p;
unsigned int iSlope, iData;

//   if (b16BitAudio)
//      iLen >>= 1;

   p = (unsigned char *)(pNESMachine->iDMCAddr + pNESMachine->regs6502.ulOffsets[pNESMachine->iDMCAddr >> 12]);
   if (pNESMachine->ucSoundChannels & 0x10) // if "samples" channel active
      {
      if (pNESMachine->iDMCLen) // if DMA sample playing, queue up its samples
         {
         iDelta = pNESMachine->iDMCFreq / 4;
         for (i=262*114; i>0 && pNESMachine->iDMCLen; i-=(pNESMachine->iDMCFreq/4)) // loop through all of the cpu clocks of this frame (2 bits at a time)
            {
            pEEQ->ucEvent[pEEQ->iCount] = pNESMachine->iDMCCount;
            pEEQ->ulTime[pEEQ->iCount++] = i;
            // use two bits at a time to generate a reasonable number of samples
            if (pNESMachine->iDMCBit == 0) // fetch a new byte
               {
               pNESMachine->ucDMCByte = *p++;
               pNESMachine->iDMCAddr++;
               if (pNESMachine->iDMCAddr == 0x10000) // 0xffff wraps around to 0x8000
                  pNESMachine->iDMCAddr = 0x8000;
               }
            if (pNESMachine->ucDMCByte & (1<<pNESMachine->iDMCBit)) // 1 bit means increment
               {
               pNESMachine->iDMCCount+=2;
               if (pNESMachine->iDMCCount >= 128)
                  pNESMachine->iDMCCount = 126; // stays at max value
               }
            else // 0 = decrement 
               {
               pNESMachine->iDMCCount-=2;
               if (pNESMachine->iDMCCount < 0)
                  pNESMachine->iDMCCount = 0; // stays at min value
               }
            pNESMachine->iDMCBit = (pNESMachine->iDMCBit+1) & 7;
            // do a pair of bits
            if (pNESMachine->ucDMCByte & (1<<pNESMachine->iDMCBit)) // 1 bit means increment
               {
               pNESMachine->iDMCCount+=2;
               if (pNESMachine->iDMCCount >= 128)
                  pNESMachine->iDMCCount = 126; // stays at max value
               }
            else // 0 = decrement 
               {
               pNESMachine->iDMCCount-=2;
               if (pNESMachine->iDMCCount < 0)
                  pNESMachine->iDMCCount = 0; // stays at min value
               }
            pNESMachine->iDMCBit = (pNESMachine->iDMCBit+1) & 7;
            if (pNESMachine->iDMCBit == 0) // end of this sample byte
               {
               pNESMachine->iDMCLen--; // decrement the total byte length
               if (pNESMachine->iDMCLen == 0) // we are done playing the sample, check next action
                  {
                  if (pNESMachine->iDMCMode & 0x40) // loop sample?
                     {
                     pNESMachine->iDMCLen = pNESMachine->iDMCStartLen;
                     pNESMachine->iDMCAddr = pNESMachine->iDMCStartAddr;
                     pNESMachine->iDMCCount = pNESMachine->iDMCStartCount;
                     p = (unsigned char *)(pNESMachine->iDMCAddr + pNESMachine->regs6502.ulOffsets[pNESMachine->iDMCAddr >> 12]);
                     }
                  else
                     {
                     if (pNESMachine->iDMCMode & 0x80) // gen IRQ at end of sample
                        pNESMachine->ucIRQs |= INT_IRQ;
                     }
                  }
               }
            } // for the entire frame time
         } // if we need to process DMC samples
      if (pEEQ->iCount == 0) // DAC was off, insert last value to prevent click noise
         {
         pEEQ->ucEvent[pEEQ->iCount] = pNESMachine->iDMCCount;
         pEEQ->ulTime[pEEQ->iCount++] = 0;
         }
      cData = pEEQ->ucEvent[pEEQ->iCount-1];
      pEEQ->ucEvent[pEEQ->iCount] = cData;
      pEEQ->ulTime[pEEQ->iCount++] = 0; // make sure the last event has a time of 0
      iSum = pEEQ->iFrameTicks << 8; // number of CPU ticks per frame scaled up for more accuracy
      iDelta = iSum / iLen; // number of scaled CPU ticks per sound sample
      iSampleCount = iSample = 0;
      iOut = 0;
      for (i=0; i<iLen; i++) // create a complete block of sound
         {
         j = iSum >> 8;
         iSampleCount++;
         if (j <= (signed int)pEEQ->ulTime[iSample]) // time to change the sample
            {
         // Do linear approximation of up to 8 samples to "smooth" things out
            if (iSampleCount > 8)
               { // create a straight line of the samples > 8 to not smooth things out too much
               for (k=0; k<iSampleCount-8; k++)
                  {
                  psBuf[iOut++] += cData; // current DAC value
                  }
               iSampleCount = 8;
               }
            cNewData = pEEQ->ucEvent[iSample++]; // >> iVoiceShift;
            iSlope = ((cNewData - cData)*256)/iSampleCount;
            iData = cData*256;
            for (k=0; k<iSampleCount; k++)
               {
               psBuf[iOut++] += (iData>>8); // current DAC value
               iData += iSlope;
               }
            cData = cNewData;
            iSampleCount = 0;
            }
         iSum -= iDelta;
         }
      if (iSampleCount) // unused samples; store them
         {
         for (k=0; k<iSampleCount; k++)
            {
            psBuf[iOut++] += cData;
            }
         }
      }
      pEEQ->ucEvent[0] = pEEQ->ucEvent[pEEQ->iCount-1]; // if no samples, use the last one
} /* NESGenSamples() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESGenNoise(void)                                          *
 *                                                                          *
 *  PURPOSE    : Generate the polynomial noise of sound channel 4.          *
 *                                                                          *
 ****************************************************************************/
void NESGenNoise(void)
{
int i;
unsigned char uc1, uc2;
unsigned short us1, us2;
   uc1 = 0x7f;
   us1 = 0x7fff;
   for (i=0; i<128; i++) // 128 possible combinations of 7-stage counter
      {
      pNoise7[i] = (uc1 & 1) * 63; // create a square wave from bit 0 (output)
      uc2 = ((uc1 & 2) >> 1) ^ (uc1 & 1);
      uc1 = (uc1 >> 1) | (uc2 << 6);
      }
   for (i=0; i<32768; i++) // 32768 possible combinations of 15-stage counter
      {
      pNoise15[i] = (us1 & 1) * 63; // create a square wave from bit 0 (output)
      us2 = ((us1 & 2) >> 1) ^ (us1 & 1);
      us1 = (us1 >> 1) | (us2 << 14);
      }

} /* NESGenNoise() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESGetLeafName(char *, char *)                           *
 *                                                                          *
 *  PURPOSE    : Retrieve the leaf name from a fully qualified path name.   *
 *                                                                          *
 ****************************************************************************/
void NESGetLeafName(char *fname, char *leaf)
{
char pszTemp[128];
int i, j, iLen;

   iLen = (int)strlen((char *)fname);
   pszTemp[127] = '\0'; /* We work backwards, so terminate string first */
   j = 126;
   for (i=iLen-1; i>=0 && j>=0; i--)
      {
      if (fname[i] == '\\')
         break;
      pszTemp[j--] = fname[i];
      }
   strcpy((char *)leaf, (const char *)&pszTemp[j+1]);

} /* NESGetLeafName() */

// Set the mapper handler addresses
void NESSetMapperHandlers(NESMACHINE *pNESTemp)
{
    switch (pNESTemp->iMapper)
       {
       case 64:
       case 0: // no mapper
          pNESTemp->pEMUH[2].pfn_write = NESMapper0;
          break;
       case 65: // same as 1 (caveman games)
       case 1:
          pNESTemp->pEMUH[2].pfn_write = NESMapper1;
          break;
       case 66: // mega man
       case 232: // Codemasters
       case 2:
          pNESTemp->pEMUH[2].pfn_write = NESMapper2;
          break;
       case 3:
          pNESTemp->pEMUH[2].pfn_write = NESMapper3;
          break;
       case 116: // Metro Cross
       case 4:
          pNESTemp->pEMUH[2].pfn_write = NESMapper4;
          break;
       case 5: // MMC5, Castlevania III
          pNESTemp->pEMUH[3].pfn_write = NESMapper5; // 5000-5FFF
          pNESTemp->pEMUH[3].pfn_read = NESMapper5Read; // 5000-5FFF
          break;
       case 7:
          pNESTemp->pEMUH[2].pfn_write = NESMapper7;
          break;
       case 9: // MMC2 
          pNESTemp->pEMUH[2].pfn_write = NESMapper9; // 8000-FFFF
          break;
       case 10: // MMC4 ROM is divided into 16K blocks
          pNESTemp->pEMUH[2].pfn_write = NESMapper10; // 8000-FFFF
          break;
       case 11:
          pNESTemp->pEMUH[2].pfn_write = NESMapper11;
          break;
       case 15:
          pNESTemp->pEMUH[2].pfn_write = NESMapper15;
          break;
       case 16:
          pNESTemp->pEMUH[2].pfn_write = NESMapper16; // 8000-FFFF
          pNESTemp->pEMUH[1].pfn_write = NESMapper16; // 6000-7FFF
          break;
       case 19:
          pNESTemp->pEMUH[2].pfn_write = NESMapper19; // 8000-FFFF
          pNESTemp->pEMUH[1].pfn_write = NESMapper19; // 6000-7FFF
          pNESTemp->pEMUH[3].pfn_write = NESMapper19; // 5000-5FFF
          pNESTemp->pEMUH[3].pfn_read = NESMapper19Read; // 5000-5FFF
          break;
       case 23: // Konami
          pNESTemp->pEMUH[2].pfn_write = NESMapper23; // 8000-FFFF
          break;
//       case 24: // Konami VRC6
//          break;
       case 25:
          pNESTemp->pEMUH[2].pfn_write = NESMapper25; // 8000-FFFF
          break;
       case 33: // Akira
          pNESTemp->pEMUH[2].pfn_write = NESMapper33; // 8000-FFFF
          break;
//       case 43:
//          break;
       case 67:
          pNESTemp->pEMUH[2].pfn_write = NESMapper67; // 8000-FFFF
          break;
       case 68: // After Burner 2
          pNESTemp->pEMUH[2].pfn_write = NESMapper68; // 8000-FFFF
          break;
       case 69:
          pNESTemp->pEMUH[2].pfn_write = NESMapper69;
          break;
       case 70:
          pNESTemp->pEMUH[2].pfn_write = NESMapper70; // 8000-FFFF
          break;
//       case 85: // VRC7 - Konami
//          break;
       case 71:
          pNESTemp->pEMUH[2].pfn_write = NESMapper71;
          break;
       case 87: // only maps 2 8K VROM pages
          pNESTemp->pEMUH[2].pfn_write = NESMapper87;
          break;
       case 93: // used only by Fantasy Zone, maps 16k bank to 0x8000-0xBFFF
          pNESTemp->pEMUH[2].pfn_write = NESMapper93;
          break;
       case 160:
          pNESTemp->pEMUH[2].pfn_write = NESMapper160;
          break;
       case 180: // crazy climber
          pNESTemp->pEMUH[2].pfn_write = NESMapper180;
          break;
//       case 185:
//          pNESTemp->pEMUH[2].pfn_write = NESMapper185;
//          ucPRGBanks[0] = 0;
//          NESMapper185Sync();
//          break;
       case 226:
          pNESTemp->pEMUH[2].pfn_write = NESMapper226;
          break;
       case 229:
          pNESTemp->pEMUH[2].pfn_write = NESMapper229;
          break;
       case 225:
       case 255:
         pNESTemp->pEMUH[2].pfn_write = NESMapper255; // 0x8000-0xffff
         pNESTemp->pEMUH[3].pfn_write = NESMapper255; // 0x5000-0x5fff
         pNESTemp->pEMUH[3].pfn_read = NESMapper255Read;
         break;
       }
} /* NESSetMapperHandlers() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : NESSetMirror(int, int, int, int)                           *
 *                                                                          *
 *  PURPOSE    : Set up the video memory mirroring.                         *
 *                                                                          *
 ****************************************************************************/
void NESSetMirror(NESMACHINE *pNES, int a, int b, int c, int d)
{
   pNES->ulVRAMOffsets[0] = (unsigned long)&pNES->pVRAM[0x2000 + a*0x400];
   pNES->ulVRAMOffsets[1] = (unsigned long)&pNES->pVRAM[0x2000 + b*0x400];
   pNES->ulVRAMOffsets[2] = (unsigned long)&pNES->pVRAM[0x2000 + c*0x400];
   pNES->ulVRAMOffsets[3] = (unsigned long)&pNES->pVRAM[0x2000 + d*0x400];

} /* NESSetMirror() */

int NES_Init(GAME_BLOB *pBlob, char *pszROM, int iGameLoad)
{
int i, iLen;
int iError = SG_NO_ERROR;
NESMACHINE *pNESTemp;
NESMACHINE *pNESOld; // KLUDGE ALERT!

   if (pszROM == NULL)
      {
      if (iGameLoad == -1) // reset machine
         {
         NESReset((NESMACHINE *)pBlob->pMachine);
         }
      else // load a game state
         {
         if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
            NES_PostLoad(pBlob);
         }
      return SG_NO_ERROR;
      }
    pBlob->iWidth = 256;
    pBlob->iHeight = 228;
    pBlob->iGameType = SYS_NES;

    pNESTemp = EMUAlloc(sizeof(NESMACHINE));
    if (pNESTemp == NULL)
       return SG_OUTOFMEMORY;
    pBlob->pMachine = (void *)pNESTemp;

    pBlob->OriginalMachine = (void *)EMUAlloc(sizeof(NESMACHINE)); // another copy used for restoring game states
    pNESTemp->pEMUH = EMUAlloc(10*sizeof(MEMHANDLERS));
    if (pNESTemp->pEMUH == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto nes_init_leave;
       }
    pNESTemp->pVRAM = EMUAlloc(0x4000);
    if (pNESTemp->pVRAM == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto nes_init_leave;
       }
    pNESTemp->mem_map = EMUAlloc(0x20000); /* Allocate 64k for the NES memory map + 64K for flags */
    if (pNESTemp->mem_map == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto nes_init_leave;
       }
    pNESTemp->pRAMBank = EMUAlloc(0x10000); // 64K of extra ram in some carts
    if (pNESTemp->pRAMBank == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto nes_init_leave;
       }
    if (iNESRefCount == 0) // first time through, allocate common stuf
       {
       pEEQ = NULL;
       pNoise7 = NULL;
       pNoise15 = NULL;
       pColorConvert = NULL;
       psAudioBuf = NULL;
       pEEQ = EMUAlloc(sizeof(EMU_EVENT_QUEUE));
       if (pEEQ == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto nes_init_leave;
          }
       pEEQ->iFrameTicks = 1789750 / 60; // number of CPU ticks per frame
       pColorConvert = EMUAlloc(0x1000);
       if (pColorConvert == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto nes_init_leave;
          }
       pNoise7 = EMUAlloc(128);
       if (pNoise7 == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto nes_init_leave;
          }
       pNoise15 = EMUAlloc(32768);
       if (pNoise15 == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto nes_init_leave;
          }
       psAudioBuf = EMUAlloc(0x1000);
       if (psAudioBuf == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto nes_init_leave;
          }
       NESGenNoise();
       // Create the color conversion table (NES palette entries into RGB565)
       for (i=0; i<64; i++)
          {
          unsigned char r, g, b;
          r = ucNESPalette[i*3+0];
          g = ucNESPalette[i*3+1];
          b = ucNESPalette[i*3+2];
          pColorConvert[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); // convert to RGB565
          }
       }
    // Define memory areas for load/save states and networking
    pBlob->mem_areas[0].pPrimaryArea = pNESTemp->pVRAM;
    pBlob->mem_areas[0].iAreaLength = 0x4000;
    pBlob->mem_areas[1].pPrimaryArea = pNESTemp->mem_map;
    pBlob->mem_areas[1].iAreaLength = 0x10000;
    pBlob->mem_areas[2].pPrimaryArea = pNESTemp->pRAMBank;
    pBlob->mem_areas[2].iAreaLength = 0x10000;
    pBlob->mem_areas[3].pPrimaryArea = (unsigned char *)pNESTemp;
    pBlob->mem_areas[3].iAreaLength = sizeof(NESMACHINE);
    pBlob->mem_areas[4].iAreaLength = 0; // end of list
    
    iTrace = 0;

/*--- Load ROM data into memory ---*/
    iLen = EMULoadNESRoms(pszROM, pNESTemp->mem_map, &pNESTemp->pBankROM);
    if (iLen == 0)
       {
       iError = SG_GENERAL_ERROR;
       goto nes_init_leave;
       }
       
    // Determinate the ROM, RAM and VROM pages
    pNESTemp->iROMPages = pNESTemp->pBankROM[4]; // number of 16K program banks
    pNESTemp->iVROMPages = pNESTemp->pBankROM[5]; // number of 8K VROM banks
    pNESTemp->iMapper = (pNESTemp->pBankROM[6] >> 4) + (pNESTemp->pBankROM[7] & 0xf0); // mapper type
    if (pNESTemp->iVROMPages > 1 && pNESTemp->iMapper == 0) // invalid mapper
       {
       if (pNESTemp->iROMPages == 2 && pNESTemp->iVROMPages == 4) // Pipe Dream
          pNESTemp->iMapper = 3;
       }
    if (pNESTemp->iMapper == 68 && pNESTemp->iROMPages != 8)
       pNESTemp->iMapper = 4; // for some reason, a bunch of games are mis-marked
    if (pNESTemp->iMapper == 241) pNESTemp->iMapper = 1; // Gunhed

    for (i=0; i<8; i++)
       pNESTemp->ulVROMOffsets[i] = (unsigned long)pNESTemp->pVRAM; // assume no ROM

    if (pNESTemp->pBankROM[6] & 8) // no mirroring
       {
       NESSetMirror(pNESTemp, 0,1,2,3);
       }
    else // has mirroring
       {
       if (pNESTemp->pBankROM[6] & 1)
          NESSetMirror(pNESTemp, 0, 1, 0, 1); // vertical mirroring
       else
          NESSetMirror(pNESTemp, 0, 0, 1, 1); // horizontal mirroring
       }

    pNESTemp->pVROM = &pNESTemp->pBankROM[0x4000*pNESTemp->iROMPages]; // point to VROM data
    memcpy(pNESTemp->pBankROM, &pNESTemp->pBankROM[16], iLen-16); // cover over the 16-byte header
    if (pNESTemp->iMapper == 1 && pNESTemp->iROMPages == 0x40) // Dragon Warrior 4
       { // need to remap the rom since it's really 512K
       for (i=1; i<64; i+=2) // odd blocks of 4k numbered 65 or greater are offset by 512K
          {
          memcpy(&pNESTemp->pBankROM[(i+64)*0x1000], &pNESTemp->pBankROM[(i+192)*0x1000], 0x1000);
          }
       }
    if (pNESTemp->iMapper == 67 && pNESTemp->iROMPages == 2) // Spy Hunter
       pNESTemp->iMapper = 3;
    if (pNESTemp->iVROMPages)
       {
       for (i=0; i<8; i++)
          pNESTemp->ulVROMOffsets[i] = (unsigned long)&pNESTemp->pVROM[0]; // set up first page of VROM
       }
    // Start out with RAM mapped to everywhere
    for (i=0; i<8; i++)
       pNESTemp->regs6502.ulOffsets[i] = (unsigned long)pNESTemp->mem_map;

    // Map page 0 of ROM and VROM
    // Map in the first 2 pages of ROM at 0x8000-0xffff
    if (pNESTemp->iROMPages == 1) // small program
       {
       for (i=0; i<4; i++)
          {
          pNESTemp->regs6502.ulOffsets[0x8+i] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xc+i] = (unsigned long)&pNESTemp->pBankROM[0 - 0xc000];
          }
       }
    else
       {
       for (i=8; i<16; i++)
          pNESTemp->regs6502.ulOffsets[i] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
       }

//    NESGetLeafName(pszROM, pBlob->szGameName); // derive the game name from the ROM name
//    i = strlen((char *)pBlob->szGameName) - 1;
//    while (i > 0 && pBlob->szGameName[i] != '.')
//       i--;
//    pBlob->szGameName[i] = 0; // remove the filename extension
    SG_GetLeafName((char *)pszROM, (char *)pBlob->szGameName); // derive the game name from the ROM name
//    __android_log_print(ANDROID_LOG_VERBOSE, "NES", "szGameName = %s", pBlob->szGameName);

    EMUSetupHandlers(pNESTemp->mem_map, pNESTemp->pEMUH, mhNES);
    pNESTemp->bMapperLatch = FALSE; // only used on mappers #9, #10
    pNESOld = pNESMachine; // KLUDGE ALERT! this won't work on a multiprocessor system and is risky with a single CPU
    pNESMachine = pNESTemp;
    switch (pNESTemp->iMapper)
       {
       case 64:
       case 0: // no mapper
          pNESTemp->pEMUH[2].pfn_write = NESMapper0;
          break;
       case 65: // same as 1 (caveman games)
       case 1:
          pNESTemp->pEMUH[2].pfn_write = NESMapper1;
          pNESTemp->iVROMPages <<= 1; // count VROM pages as 4K
          pNESTemp->ucPRGBanks[0] = 0x1f;
          pNESTemp->ucPRGBanks[1] = 0;
          pNESTemp->ucPRGBanks[2] = 0;
          pNESTemp->ucPRGBanks[3] = 0;
          NESMapper1PRGSync();
          NESMapper1CHRSync();
          break;
       case 66: // mega man
       case 232: // Codemasters
       case 2:
          pNESTemp->pEMUH[2].pfn_write = NESMapper2;
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 3:
          pNESTemp->ucCHRBanks[0] = 0;
          pNESTemp->pEMUH[2].pfn_write = NESMapper3;
          break;
       case 116: // Metro Cross
       case 4:
          pNESTemp->pEMUH[2].pfn_write = NESMapper4;
          pNESTemp->ucPRGBanks[0] = 0;
          pNESTemp->ucPRGBanks[1] = 1;
          pNESTemp->ucCHRBanks[0] = 0;
          pNESTemp->ucCHRBanks[1] = 2;
          pNESTemp->ucCHRBanks[2] = 4;
          pNESTemp->ucCHRBanks[3] = 5;
          pNESTemp->ucCHRBanks[4] = 6;
          pNESTemp->ucCHRBanks[5] = 7;
          pNESTemp->ucCartCMD = 0;
          pNESTemp->iVROMPages *= 8; // VROM is divided into 1K blocks (instead of 8K blocks)
          pNESTemp->iROMPages *= 2; // ROM is divided into 8K blocks (instead of 16K blocks)
          NESMapper4PRGSync(0); // update the pages
          NESMapper4CHRSync(0);
          pNESTemp->iIRQCount = pNESTemp->iIRQStartCount = 0;
          pNESTemp->bIRQCounter = FALSE;
          break;
       case 5: // MMC5, Castlevania III
          pNESTemp->pEMUH[3].pfn_write = NESMapper5; // 5000-5FFF
          pNESTemp->pEMUH[3].pfn_read = NESMapper5Read; // 5000-5FFF
          pNESTemp->ucPRGBanks[0] = 0;
          pNESTemp->iVROMPages *= 8; // VROM is divided into 1K blocks (instead of 8K blocks)
          pNESTemp->iROMPages *= 2; // ROM is divided into 8K blocks (instead of 16K blocks)
          // Last page is mapped to all rom areas.  Last 8K cannot change
          pNESTemp->regs6502.ulOffsets[0x8] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0x9] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xa] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xb] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xe000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xe000];
          break;
       case 7:
          pNESTemp->pEMUH[2].pfn_write = NESMapper7;
          pNESTemp->iROMPages >>= 1; // uses 32K pages
          NESSetMirror(pNESTemp, 0,0,0,0);
          break;
       case 9: // MMC2 
          pNESTemp->bMapperLatch = TRUE;
          pNESTemp->iROMPages <<= 1; // 8K ROM pages
          pNESTemp->iVROMPages <<= 1; // 4K VROM pages
          pNESTemp->ucCHRBanks[4] = 0xfe; // starting latch0 value
          pNESTemp->ucCHRBanks[5] = 0xfe; // starting latch1 value
          pNESTemp->pEMUH[2].pfn_write = NESMapper9; // 8000-FFFF
          pNESTemp->regs6502.ulOffsets[0x8] = (unsigned long)&pNESTemp->pBankROM[- 0x8000]; // map first 8K to 0x8000
          pNESTemp->regs6502.ulOffsets[0x9] = (unsigned long)&pNESTemp->pBankROM[- 0x8000];
          pNESTemp->regs6502.ulOffsets[0xa] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000]; // last 3 8K pages mapped to 0xa000
          pNESTemp->regs6502.ulOffsets[0xb] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-3)*0x2000 - 0xa000];
          break;
       case 10: // MMC4 ROM is divided into 16K blocks
          pNESTemp->bMapperLatch = TRUE;
          pNESTemp->ucCHRBanks[4] = 0xfe; // starting latch0 value
          pNESTemp->ucCHRBanks[5] = 0xfe; // starting latch1 value
          pNESTemp->pEMUH[2].pfn_write = NESMapper10; // 8000-FFFF
          pNESTemp->iVROMPages <<= 1;
          pNESTemp->regs6502.ulOffsets[0x8] = (unsigned long)&pNESTemp->pBankROM[- 0x8000]; // map first 16K to 0x8000
          pNESTemp->regs6502.ulOffsets[0x9] = (unsigned long)&pNESTemp->pBankROM[- 0x8000];
          pNESTemp->regs6502.ulOffsets[0xa] = (unsigned long)&pNESTemp->pBankROM[- 0x8000];
          pNESTemp->regs6502.ulOffsets[0xb] = (unsigned long)&pNESTemp->pBankROM[- 0x8000];
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000]; // last 16K mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 11:
          pNESTemp->pEMUH[2].pfn_write = NESMapper11;
          pNESTemp->iROMPages >>= 1; // uses 32K pages
          break;
       case 15:
          pNESTemp->pEMUH[2].pfn_write = NESMapper15;
          pNESTemp->ucPRGBanks[0] = pNESTemp->ucPRGBanks[1] = 0;
          pNESTemp->iROMPages <<= 1; // uses 8K pages
          NESMapper15Sync();
          break;
       case 16:
          pNESTemp->ucPRGBanks[0] = 0;
          pNESTemp->pEMUH[2].pfn_write = NESMapper16; // 8000-FFFF
          pNESTemp->pEMUH[1].pfn_write = NESMapper16; // 6000-7FFF
          pNESTemp->iVROMPages *= 8; // uses 1k pages for VROM
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 19:
          pNESTemp->pEMUH[2].pfn_write = NESMapper19; // 8000-FFFF
          pNESTemp->pEMUH[1].pfn_write = NESMapper19; // 6000-7FFF
          pNESTemp->pEMUH[3].pfn_write = NESMapper19; // 5000-5FFF
          pNESTemp->pEMUH[3].pfn_read = NESMapper19Read; // 5000-5FFF
          for (i=0; i<8; i++)
             {
             pNESTemp->ucPRGBanks[i] = 0xff;
             pNESTemp->ucCHRBanks[i] = 0xff;
             }
          pNESTemp->iVROMPages *= 8; // uses 1k pages for VROM
          pNESTemp->iROMPages *= 2; // uses 8K pages for ROM
          // map all pages to the last
          NESMapper19Sync();
          break;
       case 23: // Konami
          pNESTemp->iROMPages *= 2; // 8K ROM pages
          pNESTemp->iVROMPages *= 8; // 1k VROM pages
          pNESTemp->pEMUH[2].pfn_write = NESMapper23; // 8000-FFFF
          // first 16K mapped to 0x8000
          pNESTemp->regs6502.ulOffsets[0x8] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0x9] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xa] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xb] = (unsigned long)&pNESTemp->pBankROM[0 - 0x8000];
          // last 16K mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-2)*0x2000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-2)*0x2000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-2)*0x2000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-2)*0x2000 - 0xc000];
          break;
//       case 24: // Konami VRC6
//          break;
       case 25:
          pNESTemp->pEMUH[2].pfn_write = NESMapper25; // 8000-FFFF
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->iROMPages *= 2; // uses 8K pages for ROM
          pNESTemp->iVROMPages *= 8; // uses 1K pages for VROM
          break;
       case 33: // Akira
          pNESTemp->pEMUH[2].pfn_write = NESMapper33; // 8000-FFFF
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->iROMPages *= 2; // uses 8K pages for ROM
          pNESTemp->iVROMPages *= 8; // uses 1K pages for VROM
          break;
//       case 43:
//          break;
       case 67:
          pNESTemp->bSunToggle = 0;
          pNESTemp->pEMUH[2].pfn_write = NESMapper67; // 8000-FFFF
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->iVROMPages <<= 2; // 2k pages
          break;
       case 68: // After Burner 2
          pNESTemp->pEMUH[2].pfn_write = NESMapper68; // 8000-FFFF
          pNESTemp->iVROMPages *= 4; // uses 2K pages for VROM
          pNESTemp->ucCHRBanks[0] = pNESTemp->ucCHRBanks[1] = pNESTemp->ucCHRBanks[2] = 0;
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0x8] = (unsigned long)&pNESTemp->pBankROM[ - 0x8000];
          pNESTemp->regs6502.ulOffsets[0x9] = (unsigned long)&pNESTemp->pBankROM[ - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xa] = (unsigned long)&pNESTemp->pBankROM[ - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xb] = (unsigned long)&pNESTemp->pBankROM[ - 0x8000];
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 69:
          pNESTemp->iROMPages *= 2; // uses 8K pages for ROM
          pNESTemp->iVROMPages *= 8; // uses 1K pages for VROM
          pNESTemp->bRAMEnable = TRUE; // start with RAM at 0x6000
          pNESTemp->regs6502.ulOffsets[0x6] = (unsigned long)pNESTemp->mem_map;
          pNESTemp->regs6502.ulOffsets[0x7] = (unsigned long)pNESTemp->mem_map;
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xe000]; // last 8K page hard wired here
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x2000 - 0xe000];
          pNESTemp->pEMUH[2].pfn_write = NESMapper69;
          break;
       case 70:
          pNESTemp->ucCHRBanks[0] = 0;
          pNESTemp->ucPRGBanks[0] = 0;
          NESSetMirror(pNESTemp, 1,1,1,1); // uses one screen mirroring
          pNESTemp->pEMUH[2].pfn_write = NESMapper70; // 8000-FFFF
          // Last page starts out mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
//       case 85: // VRC7 - Konami
//          break;
       case 71:
          pNESTemp->pEMUH[2].pfn_write = NESMapper71;
          // Last page is permanently mapped to 0xc000 and only 0x8000 can change
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 87: // only maps 2 8K VROM pages
          pNESTemp->pEMUH[2].pfn_write = NESMapper87;
          break;
       case 93: // used only by Fantasy Zone, maps 16k bank to 0x8000-0xBFFF
          pNESTemp->pEMUH[2].pfn_write = NESMapper93;
          // Last page starts out mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
       case 160:
          pNESTemp->pEMUH[2].pfn_write = NESMapper160;
          // Last page starts out mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->iVROMPages *= 8; // VROM is divided into 1K blocks (instead of 8K blocks)
          pNESTemp->iROMPages *= 2; // ROM is divided into 8K blocks (instead of 16K blocks)
          break;
       case 180: // crazy climber
          pNESTemp->pEMUH[2].pfn_write = NESMapper180;
          NESSetMirror(pNESTemp, 0,0,1,1); // horizontal mirroring
          // Last page starts out mapped to 0xc000
          pNESTemp->regs6502.ulOffsets[0xc] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xd] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xe] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          pNESTemp->regs6502.ulOffsets[0xf] = (unsigned long)&pNESTemp->pBankROM[(pNESTemp->iROMPages-1)*0x4000 - 0xc000];
          break;
//       case 185:
//          pNESTemp->pEMUH[2].pfn_write = NESMapper185;
//          ucPRGBanks[0] = 0;
//          NESMapper185Sync();
//          break;
       case 226:
          pNESTemp->pEMUH[2].pfn_write = NESMapper226;
          pNESTemp->bRAMEnable = TRUE;
          pNESTemp->ucPRGBanks[0] = pNESTemp->ucPRGBanks[1] = 0;
          NESMapper226Sync();
          break;
       case 229:
          pNESTemp->pEMUH[2].pfn_write = NESMapper229;
          break;
       case 225:
       case 255:
         pNESTemp->pEMUH[2].pfn_write = NESMapper255; // 0x8000-0xffff
         pNESTemp->pEMUH[3].pfn_write = NESMapper255; // 0x5000-0x5fff
         pNESTemp->pEMUH[3].pfn_read = NESMapper255Read;
         break;
       default:
          {
          pNESMachine = pNESOld; // KLUDGE ALERT! this won't work on a multiprocessor system and is risky with a single CPU
//          char temp[64];
//          wsprintf(temp, TEXT("Missing Mapper #%d"),pNESTemp->iMapper);
//          MessageBox(HWND_DESKTOP, temp, TEXT("Error!"), MB_OK | MB_ICONSTOP);
          iError = SG_ERROR_UNSUPPORTED;
          goto nes_init_leave; // free the rest and leave
          }
          break;
       }
    pNESMachine = pNESOld; // KLUDGE ALERT! this won't work on a multiprocessor system and is risky with a single CPU

    NESReset(pNESTemp); // Reset the state of the emulator
    memcpy(pBlob->OriginalMachine, pNESTemp, sizeof(NESMACHINE)); // keep a copy of the original machine pointers to allow for game state loading

    if (iGameLoad >= 0)
       {
       if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
          NES_PostLoad(pBlob);
       }               

//   wcscpy(szGameName, pBlob->szGameName);
   EMULoadBRAM(pBlob, (char *)pNESTemp->pRAMBank, 0x10000,(char *)TEXT("NES")); // try to load battery backed up cartridge RAM

   pNESTemp->iDMACycles = 0;
   pBlob->bLoaded = TRUE;

   iNESRefCount++;
nes_init_leave:
   if (iError != SG_NO_ERROR)
      {
      EMUFree(pNESTemp->pEMUH);
      EMUFree(pNESTemp->pVRAM);
      EMUFree(pNESTemp->mem_map);
      EMUFree(pNESTemp->pRAMBank);
      if (iNESRefCount == 0)
         {
         EMUFree(pEEQ);
         EMUFree(pColorConvert);
         EMUFree(pNoise7);
         EMUFree(pNoise15);
         EMUFree(psAudioBuf);
         }
      EMUFree(pNESTemp); // free the whole structure
      }
   return iError;
} /* NES_Init() */

void NES_Terminate(GAME_BLOB *pBlob)
{
   if (iNESRefCount == 0)
      return;
   pBlob->bLoaded = FALSE;
   pNESMachine = (NESMACHINE *)pBlob->pMachine;
   EMUFree(pBlob->OriginalMachine); // free the extra copy of the state machine
//   wcscpy(szGameName, pBlob->szGameName);
   EMUSaveBRAM(pBlob, (char *)pNESMachine->pRAMBank, 0x10000,(char *)TEXT("NES")); // try to save battery backed up cartridge RAM
   EMUFree(pNESMachine->mem_map);
   EMUFree(pNESMachine->pBankROM);
   EMUFree(pNESMachine->pRAMBank);
   EMUFree(pNESMachine->pVRAM);
   EMUFree(pNESMachine->pEMUH);
   EMUFree(pNESMachine); // free the whole structure
   if (iNESRefCount == 1) // last machine, free common resources
      {
      EMUFree(pNoise7);
      EMUFree(pNoise15);
      EMUFree(pColorConvert);
      EMUFree(pEEQ);
      EMUFree(psAudioBuf);
      }
   iNESRefCount--;
} /* NES_Terminate() */

void NES_Play(GAME_BLOB *pBlob, BOOL bSound, BOOL bVideo, uint32_t ulKeys)
{
int i;
unsigned char *p;

      if (pBlob->bRewound)
      {
         pBlob->bRewound = FALSE;
         NES_PostLoad(pBlob);
      }
      pNESMachine = (NESMACHINE *)pBlob->pMachine;
      pBitmap = (unsigned char *)pBlob->pBitmap;
      iPitch = pBlob->iPitch;

      for (i=0; i<iCheatCount; i++)
      {
    	  p = (unsigned char *)(pNESMachine->regs6502.ulOffsets[iaCheatAddr[i] >> 12] + iaCheatAddr[i]);
    	  if (iaCheatCompare[i] != -1)
    	  {
    		  if (p[0] == (unsigned char)iaCheatCompare[i])
    			  p[0] = (unsigned char)iaCheatValue[i];
    	  }
   	   else
   		   p[0] = (unsigned char)iaCheatValue[i];
      }

      NESUpdateButtons(ulKeys);
      iTotalClocks = 262*114;
      bSpriteCollision = FALSE;
      bVBlank = FALSE;
      pEEQ->iCount = 0; // no queued events
      pNESMachine->iRefreshAddr = pNESMachine->iRefreshLatch; // reset video retrace address
      pNESMachine->iRefreshAddr += 0x1000; // advance 1 line (not sure why)
      if (pNESMachine->iRefreshAddr & 0x8000)
         {
         pNESMachine->iRefreshAddr = (pNESMachine->iRefreshAddr + 0x20) & 0xfff;
         if ((pNESMachine->iRefreshAddr & 0x3e0) == 0x3c0) // need to wrap
            {
            pNESMachine->iRefreshAddr &= ~0x3e0;
            pNESMachine->iRefreshAddr ^= 0x800;
            }
         }
      for (iScanLine=0; iScanLine<262; iScanLine++) // total display is 262 scan lines
         {
         unsigned char c;
         c = 0;
         if (bVBlank) // in VBlank
            c |= 0x80;
         if (bSpriteCollision)
            c |= 0x40; // video retrace has hit sprite 0
         if (b8Sprites)
            c |= 0x20; // 8 or more sprites on this line
//         bVBlank = FALSE; // DEBUG - vblank should be cleared (apparently)
//         bSpriteCollision = FALSE; // don't clear it or Dig Dug 2 stops working
//         pNESMachine->bLowerAddr = FALSE;
//         pNESMachine->mem_map[0x2002] = c;
                  
         // Advance to next line in video refresh hardware
         pNESMachine->iRefreshAddr += 0x1000; // fine increment
         if (pNESMachine->iRefreshAddr & 0x8000) // time to do coarse scroll
            {
            if ((pNESMachine->iRefreshAddr & 0x3e0) == 0x3e0) // we are going to wrap to 0
               {
               pNESMachine->iRefreshAddr &= ~0x3e0;
               }
            else
               {
               pNESMachine->iRefreshAddr = (pNESMachine->iRefreshAddr + 0x20) & 0xfff;
               if ((pNESMachine->iRefreshAddr & 0x3e0) == 0x3c0) // need to wrap
                  {
                  pNESMachine->iRefreshAddr &= ~0x3e0;
                  pNESMachine->iRefreshAddr ^= 0x800;
                  }
               }
            }
         if (pNESMachine->bIRQCounter && iScanLine < 239 && (pNESMachine->bVideoEnable || pNESMachine->bSpriteEnable))
            {
            if (pNESMachine->iIRQCount == 0)
               {
               pNESMachine->ucIRQs |= INT_IRQ; // generate a scanline interrupt (e.g. for MMC3/Mapper4)
               pNESMachine->iIRQCount = pNESMachine->iIRQStartCount;
               }
            else
               pNESMachine->iIRQCount--;
            }
         iTotalClocks -= 114;
         if (iScanLine == 240)
            bVBlank = TRUE;
         if (iScanLine == 241 && pNESMachine->bVBlankIntEnable)
            {
            pNESMachine->ucIRQs |= INT_NMI; // generate an interrupt
            }
 // 1.79Mhz = 114 total clocks per scanline x 262 effective scanlines (224 visible)
         if (pNESMachine->iDMACycles < 114)
            {
            iClocks = 114 - pNESMachine->iDMACycles;
            pNESMachine->iDMACycles = 0;
            AEXECNES6502(pNESMachine->mem_map, &pNESMachine->regs6502, pNESMachine->pEMUH, &iClocks, &pNESMachine->ucIRQs);
            }
         else
            pNESMachine->iDMACycles -= 114; // no activity on this scan line
         if (iScanLine < 236)
            {
            if (iScanLine >= 8 && bVideo)
               {
               NESDrawScanline();
               }
            else
               NESCheckSprite0(); // top 8 lines are not visible, but some games test sprite collision
            }
         pNESMachine->iIRQTempCount += 114; // for Namco mapper
         } // for each scanline
      if (bSound)
         {
         signed short s, *ps;
         unsigned char *puc;
         int i, k;
         memset(psAudioBuf, 0, (pBlob->b16BitAudio) ? pBlob->iAudioBlockSize:pBlob->iAudioBlockSize*2);
         NESGenSound(psAudioBuf, pBlob->iSampleCount);
         NESGenSamples(psAudioBuf, pBlob->iSampleCount); // generate DCM/DAC samples once per frame
         // Clip and convert audio buffer
         k = 0;
         if (pBlob->b16BitAudio)
            {
            ps = pBlob->pAudioBuf;
            for (i=0; i<pBlob->iSampleCount; i++)
               {
               s = psAudioBuf[i];
               if (s > 255) s = 255;
               if (s < -255) s = -255;
               ps[k++] = s<<7;
               if (pBlob->iSoundChannels == 2)
                  ps[k++] = s<<7;
               }
            }
         else
            {
            puc = (unsigned char *)pBlob->pAudioBuf;
            for (i=0; i<pBlob->iSampleCount; i++)
               {
               s = psAudioBuf[i]; // + 128;
               if (s > 255) s = 255;
               if (s < 0) s = 0;
               puc[k++] = (unsigned char)s;
               if (pBlob->iSoundChannels == 2)
                  puc[k++] = (unsigned char)s;
               }
            }
         }

} /* NES_Play() */

