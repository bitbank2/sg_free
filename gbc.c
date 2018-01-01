/*****************************************************/
/* Code to emulate GameBoy/GBC hardware              */
/*                                                   */
/* Written by Larry Bank                             */
/* Copyright 2002-2017 BitBank Software, Inc.        */
/* Drivers started 4/16/02                           */
/*****************************************************/
// Changes
// 1/1/2006 - improved performance by changing the sprite drawing to only check the active sprites on each scanline with sprites
// 3/27/2006 - Fixed reading HDMA size at FF55 (fixes Rocket Power - Gettin' Air)
// 11/7/2007 - Fixed Pokeymon Crystal by not doing the first HMDA transfer upon a write to FF55
// 11/15/2007 - Fixed Dragon Warrior III problem by changing the compiler optimization settings (see GBIOWrite() )
// 11/22/2007 - Added support for real time clock
// 1/21/2008 - added code to enforce "max 10 sprites per line" rule
// 2/13/2010 - added code to support head-to-head play on a single machine
// 2/20/2010 - finally conquered the link cable logic
// 2/23/2010 - changed GBIOWrite/GBIORead to use 8-bit case values for cleaner ARM code
// 3/1/2010 - fixed DMA status (FF55) to return the correct values - fixed Boarder Patrol, Action Man, Bugs Life
// 3/1/2010 - removed excess memory allocation
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
//#include <gtk/gtk.h>
#include <string.h>
#include "emuio.h"
#include "smartgear.h"
#include "emu.h"
#include "sound.h"
#include "gbc_emu.h"

// for Gamepad / network access
enum _network_packet_types
{
SGNETWORK_NOP=0, // Nop
SGNETWORK_BUTTONS, // opponents button info (for P2 input on client/server setup)
SGNETWORK_FILEINFO, // file size info, creates output file
SGNETWORK_FILEDATA, // file data packets, closed when all data is received
SGNETWORK_MEMDATA, // for client/server 2P games
SGNETWORK_GAMEDATA, // for networked games (e.g. GBC link cable)
SGNETWORK_START, // begin client/server 2P game
SGNETWORK_END // end client/server 2P game
};
void SGSendNetPacket(unsigned char *pBuffer, int iLen, int iPacketType);

#ifdef _WIN32_WCE
typedef void * (*GXBEGINDRAW)(void);
typedef void * (*GXENDDRAW)(void);
extern GXBEGINDRAW pfnGXBeginDraw;
extern GXENDDRAW pfnGXEndDraw;
extern BOOL bGAPI, bNetworked;
#endif

extern int iCheatCount;
extern int iaCheatAddr[], iaCheatValue[], iaCheatCompare[];

void SG_GetTime(int *iDay, int *iHour, int *iMinute, int *iSecond);

GBMACHINE *pGBMachine, gbmachine[2];
// External cartridge RAM size.  Even when not present, allocate 8k to fill 0xa000-0xbfff
int iGBCartRAMSize[8] = {0x2000,0x2000,0x2000,0x8000,0x20000,0x2000,0x2000,0x2000};
static int iPlayer; //, iOldWidth, iSampleCount;
//void * ohandle;
static int iSoundLen; //, iSoundChannels;
extern unsigned char *pSoundBuf;
//BOOL bTrace;
//int iTrace;
static unsigned char *pLocalBitmap;
extern BOOL bHead2Head, b16BitAudio, bSkipFrame, bRegistered;
//extern HDC hdcScreen;
extern char pszROMS[], pszSAVED[], pszCAPTURE[];
extern uint32_t *pTempBitmap;
extern int iScreenX, iScreenY, iDisplayWidth, iDisplayHeight, iVideoSize;
extern unsigned char *pScreen;
extern int iScreenPitch;
extern unsigned char *pBitmap;
int *iOldBGTiles; // tile index data
unsigned char *ucOldBGAttr; // tile attribute data
static unsigned char *pNoise7, *pNoise15; // polynomial noise tables
WAVEQUEUE *pWaveQueue;
uint32_t *ulAttrColor;
int iGBRefCount = 0;
int iWaveQ;
extern int iPitch, iScreenBpp;
int iSendData;
//unsigned char ucScrollArrayX[154]; // horiz scroll position for each scan line
//unsigned char ucScrollArrayY[154];
//unsigned char ucLCDCArray[154]; // array of LCD controller values at each scan line
//unsigned char ucWindowXArray[154];
//unsigned char ucWindowYArray[154];
// Sound variables
//extern char szGameName[];

void DrawTiles(int);
void ConvertTile(int iTile);
void GBGenSamples(unsigned char *pBuf, int iLen);
void GBGenWaveSamples(unsigned char *pBuf, int iLen);
void GBEchoRAMWrite(int, unsigned char);
void GBROMBankWrite(int, unsigned char);
void GBRAMBankWrite(int, unsigned char);
void GBRAMBankWrite2(int, unsigned char);
void GBIOWrite(int, unsigned char);
void GBWorkRAMWrite(int, unsigned char);
void GBVRAMWrite(int, unsigned char);
void GBBogusWrite(int, unsigned char);
void ARMDrawGBCTiles(unsigned char *pSrc, unsigned char *pTile, unsigned char *pEdge, unsigned char ucLCDC, unsigned char *pDest, int iWidth);
void ARM816FAST(unsigned char *pSrc, uint32_t *pDest, unsigned short *PalConvert, int iCount);

unsigned char GBBogusRead(int);
unsigned char GBRAMBankRead(int);
unsigned char GBEchoRAMRead(int);
unsigned char GBRAMBankRead2(int);
unsigned char GBWorkRAMRead(int);
unsigned char GBIORead(int);

int iROMMasks[10] = {1,3,7,15,31,63,127,255,511,1023};
int iRAMMasks[10] = {0,0,0,3,0,0,15,0,1};  // masks for the number of RAM banks in a cartridge
//int iRAMMasks[10] = {0,1,1,3,0,0,15,0,1};  // masks for the number of RAM banks in a cartridge
 // Timer masks for (4096,262144,65536,16384hz)
 // these numbers are based on a basic CPU clock rate of 4.19Mhz (e.g. 4.19Mhz / 4096 = 1024
int iTimerMasks[4] = {1024,16,64,256};

unsigned char ucNoiseDividers[8] = {1,2,4,6,8,10,12,14};

// fixed color values for the monochrome gameboy
unsigned short usColorTable[4] = {0x7fff, 0x4210, 0x2108, 0x0000};
// Sound square wave duty cycle samples have 8 samples at 12.5,25,50 and 75% duty cycles
unsigned char ucSounds[32] = {63,0,0,0,0,0,0,0,       // 12.5%
                              63,63,0,0,0,0,0,0,      // 25%
                              63,63,63,63,0,0,0,0,    // 50%
                              63,63,63,63,63,63,0,0}; // 75%
/* Table of byte flip values */
unsigned char ucGBMirror[256]=
     {0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
      8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
      4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
      12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
      2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
      10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
      6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
      14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
      1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
      9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
      5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
      13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
      3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
      11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
      7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
      15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255};

// Byte expansion tables used to convert tiles with separate bit planes into composite 8bpp images
// each 16-entry table converts a nibble into a 4-byte long.  These are then shifted over to all 8 bit positions
// Each 4-byte entry appears mirrored because on a little-endian machine the bytes are stored backwards
static uint32_t ulExpand[16] = {
0x00000000,0x01000000,0x00010000,0x01010000,0x00000100,0x01000100,0x00010100,0x01010100,
0x00000001,0x01000001,0x00010001,0x01010001,0x00000101,0x01000101,0x00010101,0x01010101};

static uint32_t ulExpand2[16] = {
0x00000000,0x02000000,0x00020000,0x02020000,0x00000200,0x02000200,0x00020200,0x02020200,
0x00000002,0x02000002,0x00020002,0x02020002,0x00000202,0x02000202,0x00020202,0x02020202};

static uint32_t ulExpandMirror[16] = {
0x00000000,0x00000001,0x00000100,0x00000101,0x00010000,0x00010001,0x00010100,0x00010101,
0x01000000,0x01000001,0x01000100,0x01000101,0x01010000,0x01010001,0x01010100,0x01010101};

static uint32_t ulExpandMirror2[16] = {
0x00000000,0x00000002,0x00000200,0x00000202,0x00020000,0x00020002,0x00020200,0x00020202,
0x02000000,0x02000002,0x02000200,0x02000202,0x02020000,0x02020002,0x02020200,0x02020202};

unsigned char ucInitWave[16]={0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF,0x00,0xFF};

static int iLoopCount;
extern BOOL bUserAbort, bFullScreen, bUseDDraw;
//extern int ohandle;
extern BOOL bTrace, bStereo, bAutoLoad;
extern PALETTEENTRY EMUpe[];
void EMUCreateColorTable(PALETTEENTRY *);
void EMUUpdatePalette(int, PALETTEENTRY *);

void CopyBackground(void);

void GBBogusWrite(int iAddr, unsigned char ucByte)
{
   pGBMachine->mem_map[0xc000 + (iAddr & 0xfff)] = ucByte;
}

unsigned char GBBogusRead(int iAddr)
{
   return pGBMachine->mem_map[0xc000 + (iAddr & 0xfff)];
}

//#pragma optimize("g", on)

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBVRAMWrite(int, char)                                     *
 *                                                                          *
 *  PURPOSE    : Color gameboy Video RAM - check for tile changes.          *
 *                                                                          *
 ****************************************************************************/
void GBVRAMWrite(int iAddr, unsigned char ucByte)
{
   pGBMachine->pVRAMBank[iAddr] = ucByte; // update the memory

} /* GBVRAMWrite() */

unsigned char GBVRAMRead(int iAddr)
{
   return pGBMachine->pVRAMBank[iAddr];
} /* GBVRAMRead() */

void GBEchoRAMWrite(int iAddr, unsigned char ucByte)
{
   pGBMachine->mem_map[iAddr-0x2000] = ucByte;

} /* GBEchoRAMWrite() */

unsigned char GBEchoRAMRead(int iAddr)
{
   return pGBMachine->mem_map[iAddr-0x2000];

} /* GBEchoRAMRead() */

unsigned char GBROMRead(int iAddr)
{
   return pGBMachine->mem_map[iAddr];

} /* GBROMRead() */

void GBRAMWrite(int iAddr, unsigned char ucByte)
{
   pGBMachine->mem_map[iAddr] = ucByte;

} /* GBRAMWrite() */

unsigned char GBROMBankRead(int iAddr)
{
   return pGBMachine->pBank[iAddr];

} /* GBROMBankRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBWorkRAMWrite(int, char)                                  *
 *                                                                          *
 *  PURPOSE    : Color gameboy banked work RAM area.                        *
 *                                                                          *
 ****************************************************************************/
void GBWorkRAMWrite(int iAddr, unsigned char ucByte)
{
   pGBMachine->pWorkRAMBank[iAddr & 0xfff] = ucByte;
} /* GBWorkRAMWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBWorkRAMRead(int)                                         *
 *                                                                          *
 *  PURPOSE    : Color gameboy banked work RAM area.                        *
 *                                                                          *
 ****************************************************************************/
unsigned char GBWorkRAMRead(int iAddr)
{
   return pGBMachine->pWorkRAMBank[iAddr & 0xfff];
} /* GBWorkRAMRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBGenWaveSamples(char *, int)                              *
 *                                                                          *
 *  PURPOSE    : Special case to generate the wave table sound samples.     *
 *                                                                          *
 ****************************************************************************/
void GBGenWaveSamples(unsigned char *pBuf, int iLen)
{
register int i, iIndex, iLeftVol, iRightVol;
unsigned char cShift, c;
int iPosDelta, iLenDelta, iPos;
uint32_t ulFraction;
unsigned char ucLeft[2048], *ucRight, ucCurrentLine;
static BOOL bReset;
static uint32_t ulSum;
static BOOL bSoundEnable = FALSE;
unsigned char ucChannels;
unsigned short *psBuf = (unsigned short *)pBuf;

   ucRight = &ucLeft[1024];
   memset(ucLeft, 0, 2048);
   cShift = 0;
//   if (b16BitAudio)
//      iLen >>= 1;
      
   ulFraction = 65536*256 / ((44400*256)/(60 * 155)); // fractional part to advance scanline timing
   if (!(pGBMachine->mem_map[0xff26] & 0x80)) // sound is disabled
      {
      iIndex = 0;
      while (iIndex < iWaveQ) // if samples not rendered
         {
         if (pWaveQueue[iIndex].ucIndex != 0xff)
            {
            c = pWaveQueue[iIndex].ucIndex << 1;
            pGBMachine->ucSamples[c] = (pWaveQueue[iIndex].ucByte >> 4);
            pGBMachine->ucSamples[c+1] = pWaveQueue[iIndex].ucByte & 0xf;
            }
         else
            switch (pWaveQueue[iIndex].ucByte)
               {
               case WAVE_ON:
                  bSoundEnable = TRUE;
                  break;
               case WAVE_OFF:
                  bSoundEnable = FALSE;
                  break;
               case WAVE_RESET:
                  pGBMachine->iWaveLen = pGBMachine->iWaveStartLen;
                  pGBMachine->iWavePos = 0;
                  break;
               }
         iIndex++;
         }
      iWaveQ = 0; // reset queued waveform changes
      pGBMachine->bSound3On = FALSE;
      return;
      }

   ucChannels = pGBMachine->mem_map[0xff25]; // which output goes to which channel

   c = (pGBMachine->mem_map[0xff1c] & 0x60) >> 5;
   switch (c)
      {
      case 0: // mute
         cShift = 8;
         break;
      case 1:
         cShift = 0;
         break;
      case 2:
         cShift = 1;
         break;
      case 3:
         cShift = 2;
         break;
      }
   iPosDelta = 0x17c7000/(2048-pGBMachine->iWaveFreq); // compute wave delta for 44400Hz samples

   if (pGBMachine->mem_map[0xff1e] & 0x40)
      iLenDelta = 1; // waveform output for a fixed length
   else
      {
      if (pGBMachine->iWaveLen == 0)
         pGBMachine->iWaveLen = 1; // make sure it runs
      iLenDelta = 0; // waveform output continuously
      }

// emulate sound channel 3 (waveform table)
   iIndex = 0;
   ucCurrentLine = 0;//pWaveQueue[0].ucScanLine;
//   bReset = FALSE;
   if (pGBMachine->iWaveFreq < 0x7fc)// && (ucChannels & 0x44) && (pGBMachine->mem_map[0xff1a] & 0x80)) // if sound channel 3 is active on either output and enabled
      {
      for (i=0; i<iLen/* && iWaveLen*/; i++) // number of samples to generate
         {
         // update the wave table with the changes for this scan line
         if (iIndex < iWaveQ && ucCurrentLine >= pWaveQueue[iIndex].ucScanLine)
            {
            while (iIndex < iWaveQ && !bReset) // collect new samples and commands until a reset
               {
               if (pWaveQueue[iIndex].ucIndex == 0xff) // sound command
                  {
                  switch (pWaveQueue[iIndex].ucByte)
                     {
                     case WAVE_ON:
                        bSoundEnable = TRUE;
                        break;
                     case WAVE_OFF:
                        bSoundEnable = FALSE;
                        break;
                     case WAVE_RESET:
                        pGBMachine->iWaveLen = pGBMachine->iWaveStartLen;
                        pGBMachine->iWavePos = 0;
                        bReset = TRUE;
                        break;
                     }
                  }
               else
                  {
                  c = pWaveQueue[iIndex].ucIndex << 1;
                  pGBMachine->ucSamples[c] = (pWaveQueue[iIndex].ucByte >> 4);
                  pGBMachine->ucSamples[c+1] = pWaveQueue[iIndex].ucByte & 0xf;
                  }
               iIndex++;
               }
            }
         ulSum += ulFraction;
         if (ulSum >= 0x10000)
            {
            ulSum &= 0xffff;
            ucCurrentLine++;
            }

         if (bSoundEnable)
            {
            iPos = (pGBMachine->iWavePos >> 19) & 0x3f; // get position in the wave table
            if (iPos > 0x1f) // allow next set of samples to get loaded
               {
               bReset = FALSE;
               iPos &= 0x1f;
               pGBMachine->iWavePos &= 0x7ffff;
               }
            c = pGBMachine->ucSamples[iPos] >> cShift;
            if (ucChannels & 0x4)
               ucLeft[i] = c;
            if (ucChannels & 0x40)
               ucRight[i] = c;
            pGBMachine->iWavePos += iPosDelta;
            pGBMachine->iWaveLen -= iLenDelta;
            }

         } // for

   // Interleave the left and right channels at the given volume levels
      iLeftVol = pGBMachine->mem_map[0xff24] & 7;
      iRightVol = (pGBMachine->mem_map[0xff24] >> 4) & 7;
      if (b16BitAudio)
         {
         if (bStereo) // stereo
            {
            unsigned int l, r;
            for (i=0; i<iLen; i++)
               {
                 l = psBuf[0] + ((ucLeft[i] * iLeftVol) << 7);
                 r = psBuf[1] + ((ucRight[i] * iRightVol) << 7);
                 psBuf[0] = l >> 1;
                 psBuf[1] = r >> 1;
//               psBuf[0] += ((ucLeft[i] * iLeftVol) << 7);
//               psBuf[1] += ((ucRight[i] * iRightVol) << 7);
               psBuf += 2;
               }
            }
         else // mono
            {
            for (i=0; i<iLen; i++)
               {
               (*psBuf++) += ((ucLeft[i] * iLeftVol) + (ucRight[i] * iRightVol)) << 6;
               }
            }
         }
      else
         {
         if (bStereo) // stereo
            {
            for (i=0; i<iLen; i++)
               {
               pBuf[0] += (ucLeft[i] * iLeftVol);
               pBuf[1] += (ucRight[i] * iRightVol);
               pBuf += 2;
               }
            }
         else // mono
            {
            for (i=0; i<iLen; i++)
               {
               (*pBuf++) += ((ucLeft[i] * iLeftVol) + (ucRight[i] * iRightVol)) >> 1;
               }
            }
         }
      }

   pGBMachine->bSound3On = ((pGBMachine->mem_map[0xff1a] & 0x80) && pGBMachine->iWaveLen && cShift != 8); // if waveform sound is playing, this will be non-zero
   while (iIndex < iWaveQ) // if samples not rendered
      {
      if (pWaveQueue[iIndex].ucIndex == 0xff)
         {
         switch (pWaveQueue[iIndex].ucByte)
            {
            case WAVE_ON:
               bSoundEnable = TRUE;
               break;
            case WAVE_OFF:
               bSoundEnable = FALSE;
               break;
            case WAVE_RESET:
               pGBMachine->iWaveLen = pGBMachine->iWaveStartLen;
               pGBMachine->iWavePos = 0;
               break;
            }
         }
      else
         {
         c = pWaveQueue[iIndex].ucIndex << 1;
         pGBMachine->ucSamples[c] = (pWaveQueue[iIndex].ucByte >> 4);
         pGBMachine->ucSamples[c+1] = pWaveQueue[iIndex].ucByte & 0xf;
         }
      iIndex++;
      }
   iWaveQ = 0; // reset queued waveform changes

} /* GBGenWaveSamples() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBGenSamples(char *, int)                                  *
 *                                                                          *
 *  PURPOSE    : Generate 1/120 of a second of sound samples.               *
 *                                                                          *
 ****************************************************************************/
void GBGenSamples(unsigned char *pBuf, int iLen)
{
register int i, iSample, iPos, iPosDelta, iLenDelta, iVolDelta;
int iFreqShift, iNoiseMask, iLeftVol, iRightVol;
register unsigned char c, *pWaves, ucChannels;
unsigned char ucLeft[1024], ucRight[1024]; // temp sound buffers to handle stereo output
unsigned short *psBuf = (unsigned short *)pBuf;

//   if (b16BitAudio)
//      iLen >>= 1;
   ucChannels = pGBMachine->mem_map[0xff25]; // which output goes to which channel
   if (pGBMachine->mem_map[0xff26] & 0x80)
      {
      memset(ucLeft, 0, iLen);  // initialize sound bufs
      memset(ucRight, 0, iLen);
      }
   else // no sound
      {
      pGBMachine->bSound1On = pGBMachine->bSound2On = pGBMachine->bSound3On = pGBMachine->bSound4On = FALSE;
      i = 1;
      if (bStereo)
         i <<= 1;
      if (b16BitAudio)
         i <<= 1;
      memset(pBuf, 0, iLen*i); // make sure buffer is silent
      return; // all sounds are turned off
      }

// emulate sound channel 1 (square wave with envelope and frequency sweep)
   if (pGBMachine->mem_map[0xff14] & 0x40)
      iLenDelta = 1; // waveform output for a fixed length
   else
      {
      if (pGBMachine->iSnd1Len == 0)
         pGBMachine->iSnd1Len = 1; // make sure it runs
      iLenDelta = 0; // waveform output continuously
      }
   if (pGBMachine->mem_map[0xff12] & 8)
      iVolDelta = 1;
   else
      iVolDelta = -1;
   pWaves = &ucSounds[8* (pGBMachine->mem_map[0xff11] >> 6)]; // point to correct duty cycle for sound channel 1
   if ((ucChannels & 0x11) && pGBMachine->iFreq1 < 2048) // if sound channel 1 is active on either output
      {
      iPosDelta = 0x17c7000/(2048-pGBMachine->iFreq1); // compute freq delta for 44400Hz samples
      for (i=0; i<iLen && pGBMachine->iSnd1Len; i++)
         {
         if (pGBMachine->iFreq1 < 0x7f8) // must be a frequency that can be heard
            {
            iPos = (pGBMachine->iSnd1Pos >> 20) & 7;
            iSample = (pWaves[iPos] * pGBMachine->iSnd1Vol) >> 4;
            if (ucChannels & 0x1)
               ucLeft[i] += iSample;
            if (ucChannels & 0x10)
               ucRight[i] += iSample;
            pGBMachine->iSnd1Pos += iPosDelta;
            }
         else
            {
            iPos = 0;
            }
         pGBMachine->iSnd1Len -= iLenDelta;
         pGBMachine->iSnd1EnvPos += pGBMachine->iSnd1Delta;
         if (pGBMachine->iSnd1EnvPos > 0x10000)
            {
            pGBMachine->iSnd1EnvPos &= 0xffff;
            pGBMachine->iSnd1Vol += iVolDelta;
            if (pGBMachine->iSnd1Vol < 0) // when vol hits max or min, it stays there
               pGBMachine->iSnd1Vol = 0;
            if (pGBMachine->iSnd1Vol > 15)
               pGBMachine->iSnd1Vol = 15;
            }
         if (pGBMachine->iSweepDelta) // if frequency sweep is enabled
            {
            pGBMachine->iSweepPos += pGBMachine->iSweepDelta;
            if (pGBMachine->iSweepPos > 0x10000) // time to adjust frequency
               {
               pGBMachine->iSweepPos &= 0xffff;
               c = pGBMachine->mem_map[0xff10] & 7; // frequence shift amount
               iFreqShift = pGBMachine->iFreq1 >> c;
               if (pGBMachine->mem_map[0xff10] & 8) // decrease frequency
                  pGBMachine->iFreq1 -= iFreqShift;
               else
                  pGBMachine->iFreq1 += iFreqShift;
               if (pGBMachine->iFreq1 < 0)
                  pGBMachine->iFreq1 += iFreqShift; // if bottoms out, leave last freq
               if (pGBMachine->iFreq1 > 2047) // if greater than max, stop playing sound
                  continue;
               iPosDelta = 0x17c7000/(2048-pGBMachine->iFreq1); // re-compute freq delta for 44400Hz samples
               }
            }
         }
      }
   pGBMachine->bSound1On = (pGBMachine->iSnd1Len && pGBMachine->iSnd1Vol);

// emulate sound channel 2 (simple square wave with envelope)
   if (pGBMachine->mem_map[0xff19] & 0x40)
      iLenDelta = 1; // waveform output for a fixed length
   else
      {
      if (pGBMachine->iSnd2Len == 0)
         pGBMachine->iSnd2Len = 1; // make sure it runs
      iLenDelta = 0; // waveform output continuously
      }
   if (pGBMachine->mem_map[0xff17] & 8)
      iVolDelta = 1;
   else
      iVolDelta = -1;
   pWaves = &ucSounds[8* (pGBMachine->mem_map[0xff16] >> 6)]; // point to correct duty cycle for sound channel 2
   iPosDelta = 0x17c7000/(2048-pGBMachine->iFreq2); // compute freq delta for 44400Hz samples
   if (ucChannels & 0x22) // if sound channel 2 is active on either output
      {
      for (i=0; i<iLen && pGBMachine->iSnd2Len; i++)
         {
         if (pGBMachine->iFreq2 < 0x7f8) // must be a frequency that can be heard
            {
            iPos = (pGBMachine->iSnd2Pos >> 20) & 7;
            iSample = (pWaves[iPos] * pGBMachine->iSnd2Vol) >> 4;
            if (ucChannels & 0x2)
               ucLeft[i] += iSample;
            if (ucChannels & 0x20)
               ucRight[i] += iSample;
            pGBMachine->iSnd2Pos += iPosDelta;
            }
         pGBMachine->iSnd2Len -= iLenDelta;
         pGBMachine->iSnd2EnvPos += pGBMachine->iSnd2Delta;
         if (pGBMachine->iSnd2EnvPos > 0x10000)
            {
            pGBMachine->iSnd2EnvPos &= 0xffff;
            pGBMachine->iSnd2Vol += iVolDelta;
            if (pGBMachine->iSnd2Vol < 0) // when vol hits max or min, it stays there
               pGBMachine->iSnd2Vol = 0;
            if (pGBMachine->iSnd2Vol > 15)
               pGBMachine->iSnd2Vol = 15;
            }
         }
      }
   pGBMachine->bSound2On = (pGBMachine->iSnd2Len && pGBMachine->iSnd2Vol);

// Emulate sound channel 4 polynomial data (noise)
   if (pGBMachine->mem_map[0xff23] & 0x40)
      iLenDelta = 1; // waveform output for a fixed length
   else
      {
      if (pGBMachine->iNoiseLen == 0)
         pGBMachine->iNoiseLen = 1; // make sure it runs
      iLenDelta = 0; // waveform output continuously
      }
   if (pGBMachine->mem_map[0xff22] & 8) // 7/15 step selection
      {
      pWaves = pNoise7;
      iNoiseMask = 0x7f;
      }
   else
      {
      pWaves = pNoise15;
      iNoiseMask = 0x7fff;
      }
   if (pGBMachine->mem_map[0xff21] & 8) // noise envelope slope
      iVolDelta = 1;
   else
      iVolDelta = -1;
   if (ucChannels & 0x88) // if sound channel 4 is active on either output and enabled
      {
      for (i=0; i<iLen && pGBMachine->iNoiseLen; i++) // number of samples to generate
         {
         iPos = (pGBMachine->iNoisePos >> 19) & iNoiseMask; // get position in the wave table
         iSample = (pWaves[iPos] * pGBMachine->iNoiseVol) >> 4;
         if (ucChannels & 0x8)
            ucLeft[i] += iSample;
         if (ucChannels & 0x80)
            ucRight[i] += iSample;
         pGBMachine->iNoisePos += pGBMachine->iNoiseFreq;
         pGBMachine->iNoiseLen -= iLenDelta;
         pGBMachine->iNoiseEnvPos += pGBMachine->iNoiseDelta;
         if (pGBMachine->iNoiseEnvPos > 0x10000)
            {
            pGBMachine->iNoiseEnvPos &= 0xffff;
            pGBMachine->iNoiseVol += iVolDelta;
            if (pGBMachine->iNoiseVol < 0) // when vol hits max or min, it stays there
               pGBMachine->iNoiseVol = 0;
            if (pGBMachine->iNoiseVol > 15)
               pGBMachine->iNoiseVol = 15;
            }
         }
      }
   pGBMachine->bSound4On = (pGBMachine->iNoiseLen && pGBMachine->iNoiseVol);

// Interleave the left and right channels at the given volume levels
   iLeftVol = pGBMachine->mem_map[0xff24] & 7;
   iRightVol = (pGBMachine->mem_map[0xff24] >> 4) & 7;
   if (b16BitAudio)
      {
      if (bStereo) // stereo
         {
         for (i=0; i<iLen; i++)
            {
            psBuf[0] = (ucLeft[i] * iLeftVol)<<4;
            psBuf[1] = (ucRight[i] * iRightVol)<<4;
            psBuf += 2;
            }
         }
      else // mono
         {
         for (i=0; i<iLen; i++)
            {
            (*psBuf++) = ((ucLeft[i] * iLeftVol) + (ucRight[i] * iRightVol))<<3;
            }
         }
      }
   else
      {
      if (bStereo) // stereo
         {
         for (i=0; i<iLen; i++)
            {
            pBuf[0] = (ucLeft[i] * iLeftVol)>>3;
            pBuf[1] = (ucRight[i] * iRightVol)>>3;
            pBuf += 2;
            }
         }
      else // mono
         {
         for (i=0; i<iLen; i++)
            {
            (*pBuf++) = ((ucLeft[i] * iLeftVol) + (ucRight[i] * iRightVol))>>4;
            }
         }
      }
} /* GBGenSamples() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBROMBankWrite(int, char)                                  *
 *                                                                          *
 *  PURPOSE    : Memory bank controller.                                    *
 *                                                                          *
 ****************************************************************************/
void GBROMBankWrite(int iAddr, unsigned char ucByte)
{
//static unsigned char ucLastWrite;

   switch (pGBMachine->iMBCType) // handle memory bank switching depending on memory controller type
      {
      case 1: // MBC1
         switch (iAddr & 0xe000)
            {
            case 0x0000:
               ucByte &= 0xf;
               pGBMachine->bRAMEnable = (ucByte == 0x0a);
               break;
            case 0x2000:
               ucByte &= 0x1f; // lower 5 bits of ROM bank
               if (ucByte == 0)
                  ucByte = 1; // bank 0 really selects bank 1
               pGBMachine->iROMBank &= 0xffe0;
               pGBMachine->iROMBank |= (ucByte & 0x1f);
               break;
            case 0x4000:
               ucByte &= 3;
               if (pGBMachine->ucMemoryModel) // if 4/32 memory model
                  pGBMachine->iRAMBank = ucByte;
               else
                  {
                  pGBMachine->iROMBank &= 0xff9f;
                  pGBMachine->iROMBank |= (ucByte << 5);
                  }
               break;
            case 0x6000: // select memory model to use
               pGBMachine->ucMemoryModel = ucByte & 1;
               break;
            }
         break;
      case 2: // MBC2
         switch (iAddr & 0xe000)
            {
            case 0x2000:
               pGBMachine->iROMBank = ucByte & 0xf;
               if (pGBMachine->iROMBank == 0)
                  pGBMachine->iROMBank = 1;
               break;
            }
         break;
      case 3: // MBC3
         if (pGBMachine->bRTC) // if realtime clock present
            {
            switch (iAddr & 0xe000)
               {
               case 0x0000: // RAM Enable
                  ucByte &= 0xf;
                  pGBMachine->bRAMEnable = (ucByte == 0x0a);
                  break;
               case 0x2000: // ROM Bank
                  pGBMachine->iROMBank = ucByte & 0x7f;
                  if (pGBMachine->iROMBank == 0)
                     pGBMachine->iROMBank = 1;
                  break;
               case 0x4000: // RAM Bank (only writable when ram is enabled)
                  if (pGBMachine->bRAMEnable)
                  {
                      ucByte &= 0xf;
                     if (ucByte < 4) // don't affect the RAM bank when RTC latch written
                        pGBMachine->iRAMBank = ucByte & 0x3;
//                     else if (ucByte >= 8 && ucByte <= 12)
                     pGBMachine->rtc.ucRegs[7] = ucByte; // keep register select info here
//                     else
//                        pGBMachine->rtc.ucRegs[7] = 0; // invalid value, disable
                  }
                  else
                     return; // no effect
                  break;
               case 0x6000: // Latch RTC data
                  if (pGBMachine->rtc.ucLatch == 0 && ucByte == 1) // latch clock data
                     {
                     pGBMachine->rtc.ucRegs[0] = pGBMachine->rtc.iSecond;
                     pGBMachine->rtc.ucRegs[1] = pGBMachine->rtc.iMinute;
                     pGBMachine->rtc.ucRegs[2] = pGBMachine->rtc.iHour;
                     pGBMachine->rtc.ucRegs[3] = pGBMachine->rtc.iDay;
                     pGBMachine->rtc.ucRegs[4] = (pGBMachine->rtc.iDay >> 8) | (pGBMachine->rtc.ucStop << 6) | (pGBMachine->rtc.ucCarry << 7);
//                     pGBMachine->rtc.ucRegs[5] = 0xff;
//                     pGBMachine->rtc.ucRegs[6] = 0xff;
//                     pGBMachine->rtc.ucRegs[7] = 0xff; // use for register select
                     }
                  if (ucByte == 0 || ucByte == 1)
                     pGBMachine->rtc.ucLatch = ucByte;
                  break;
               }
            } // yes RTC
         else
            {
            switch (iAddr & 0xe000)
               {
               case 0x0000: // RAM enable
                  ucByte &= 0xf;
                  pGBMachine->bRAMEnable = (ucByte == 0x0a);
                  break;
               case 0x2000: // ROM Bank
                  pGBMachine->iROMBank = ucByte & 0x7f;
                  if (pGBMachine->iROMBank == 0)
                     pGBMachine->iROMBank = 1;
                  break;
               case 0x4000: // RAM Bank
                  pGBMachine->iRAMBank = ucByte & 3;
                  break;
               }
            } // no RTC
         break;
      case 5: // MBC5
         switch (iAddr & 0xf000)
            {
            case 0x0000:
            case 0x1000:
               ucByte &= 0xf;
               pGBMachine->bRAMEnable = (ucByte == 0x0a);
               break;
            case 0x2000:
               pGBMachine->iROMBank &= 0x100;
               pGBMachine->iROMBank |= ucByte;
               break;
            case 0x3000:
               pGBMachine->iROMBank &= 0xff;
               pGBMachine->iROMBank |= ((unsigned int)ucByte & 1) << 8;
               break;
            case 0x4000:
            case 0x5000:
               pGBMachine->iRAMBank = ucByte;
               break;

            }
         break;
      case 6: // HUC1
         switch (iAddr & 0xe000)
            {
//            case 0x0000:
//               ucByte &= 0xf;
//               bRAMEnable = (ucByte == 0x0a);
//               break;
            case 0x2000:
               if (pGBMachine->ucMemoryModel)
                  {
                  pGBMachine->iROMBank = ucByte & 0x3f;
                  if (pGBMachine->iROMBank == 0)
                     pGBMachine->iROMBank = 1;
                  }
               else
                  {
   			      pGBMachine->ucHUC1 = (pGBMachine->ucHUC1 & 0x60) + (ucByte & 0x3F);
                  if (pGBMachine->ucHUC1 == 0) // not a valid bank
                     pGBMachine->ucHUC1 = 1;
                  pGBMachine->iROMBank = pGBMachine->ucHUC1;
                  }
               break;
            case 0x4000:
               if (pGBMachine->ucMemoryModel)
                  {
                  pGBMachine->iRAMBank = ucByte & 3;
                  }
               else
                  {
         			pGBMachine->ucHUC1 = ((ucByte << 5) & 0x60) + (pGBMachine->ucHUC1 & 0x3F);
                  if (pGBMachine->ucHUC1 == 0)
                     pGBMachine->ucHUC1 = 1;
                  pGBMachine->iROMBank = pGBMachine->ucHUC1;
                  }
               break;
            case 0x6000: // select memory model to use
               pGBMachine->ucMemoryModel = ucByte & 1;
               pGBMachine->ucHUC1 = 0;
               break;
            }
         break;
      case 7: // HUC3
         switch (iAddr & 0xe000)
            {
            case 0x0000:
               ucByte &= 0xf;
               pGBMachine->bRAMEnable = (ucByte == 0x0a);
               break;
            case 0x2000:
               pGBMachine->iROMBank = ucByte & 0x7f;
               if (pGBMachine->iROMBank == 0)
                  pGBMachine->iROMBank = 1;
               break;
            case 0x4000:
               pGBMachine->iRAMBank = ucByte & 3;
               break;
            }
         break;
      }

   if (pGBMachine->iROMBank != pGBMachine->iOldROM)
      {
      pGBMachine->iROMBank &= pGBMachine->iROMSizeMask; // make sure it is not set beyond actual size
      pGBMachine->iOldROM = pGBMachine->iROMBank;
      pGBMachine->pBank = &pGBMachine->pBankROM[(pGBMachine->iROMBank-1)*0x4000];
      // update the opcode lookup table with the new bank info
      pGBMachine->ulOpList[0x4] = (unsigned long)pGBMachine->pBank;
      pGBMachine->ulOpList[0x5] = (unsigned long)pGBMachine->pBank;
      pGBMachine->ulOpList[0x6] = (unsigned long)pGBMachine->pBank;
      pGBMachine->ulOpList[0x7] = (unsigned long)pGBMachine->pBank;
      }
   pGBMachine->iRAMBank &= pGBMachine->iRAMSizeMask;
   if (pGBMachine->iRAMBank != pGBMachine->iOldRAM)
      {
      pGBMachine->iOldRAM = pGBMachine->iRAMBank;
      pGBMachine->pRAMBank = &pGBMachine->pBankRAM[(pGBMachine->iRAMBank-5)*0x2000];
      pGBMachine->ulOpList[0xa] = (unsigned long)pGBMachine->pRAMBank; // Banked RAM at 0xA000-0xB000
      pGBMachine->ulOpList[0xb] = (unsigned long)pGBMachine->pRAMBank;
      }

} /* GBROMBankWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBRAMBankWrite(int)                                        *
 *                                                                          *
 *  PURPOSE    : Write to the cartridge banked RAM.                         *
 *                                                                          *
 ****************************************************************************/
void GBRAMBankWrite(int iAddr, unsigned char ucByte)
{
   if (pGBMachine->bRAMEnable)
      pGBMachine->pRAMBank[iAddr] = ucByte;
   else
      iAddr = 0;

} /* GBRAMBankWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBRAMBankRead(int)                                         *
 *                                                                          *
 *  PURPOSE    : Read from the cartridge banked RAM.                        *
 *                                                                          *
 ****************************************************************************/
unsigned char GBRAMBankRead(int iAddr)
{
   if (pGBMachine->bRAMEnable)
      return pGBMachine->pRAMBank[iAddr];
   else
      return 0xff;

} /* GBRAMBankRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBRAMBankWrite2(int)                                       *
 *                                                                          *
 *  PURPOSE    : Write to the cartridge banked RAM with RTC.                *
 *                                                                          *
 ****************************************************************************/
void GBRAMBankWrite2(int iAddr, unsigned char ucByte)
{
unsigned char bStopped = pGBMachine->rtc.ucRegs[4] & 0x40;

   if (pGBMachine->bRAMEnable)
      {
      if ((pGBMachine->rtc.ucRegs[7] & 8) == 0)
         pGBMachine->pRAMBank[iAddr] = ucByte;
      else if (bStopped) // writing to RTC register
         {
         switch (pGBMachine->rtc.ucRegs[7]) // must be halted to write to registers
            {
            case 8:
//               pGBMachine->rtc.ucRegs[0] = ucByte;
               pGBMachine->rtc.iSecond = ucByte;
               while (pGBMachine->rtc.iSecond >= 60) pGBMachine->rtc.iSecond -= 60;
               break;
            case 9:
//               pGBMachine->rtc.ucRegs[1] = ucByte;
               pGBMachine->rtc.iMinute = ucByte;
               while (pGBMachine->rtc.iMinute >= 60) pGBMachine->rtc.iMinute -= 60;
               break;
            case 10:
//               pGBMachine->rtc.ucRegs[2] = ucByte;
               pGBMachine->rtc.iHour = ucByte;
               while (pGBMachine->rtc.iHour >= 24) pGBMachine->rtc.iHour -= 24;
               break;
            case 11:
//               pGBMachine->rtc.ucRegs[3] = ucByte;
               pGBMachine->rtc.iDay = (pGBMachine->rtc.iDay & 0x100) | ucByte;
               break;
            case 12:
//               pGBMachine->rtc.ucRegs[4] = ucByte;
               pGBMachine->rtc.iDay = (pGBMachine->rtc.iDay & 0xff) | ((ucByte & 1) << 8);
               pGBMachine->rtc.ucStop = (ucByte >> 6) & 1;
               pGBMachine->rtc.ucCarry = (ucByte >> 7) & 1;
               break;
            }
         }
      }
   else
      iAddr = 0;

} /* GBRAMBankWrite2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBRAMBankRead2(int)                                        *
 *                                                                          *
 *  PURPOSE    : Read from the cartridge banked RAM with RTC.               *
 *                                                                          *
 ****************************************************************************/
unsigned char GBRAMBankRead2(int iAddr)
{
unsigned char c = 0xff;
unsigned char ucBank = pGBMachine->rtc.ucRegs[7];

   if (pGBMachine->bRAMEnable)
      {
      if ((ucBank & 8) == 0)
         c = pGBMachine->pRAMBank[iAddr];
      else // reading from RTC register
         {
         if (ucBank >= 8 && ucBank <= 12)
            c = pGBMachine->rtc.ucRegs[ucBank & 7];
//         else
//            c = 0;
         }
      }
//   else
//      c = 0;

      return c;

} /* GBRAMBankRead2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBIORead(int)                                              *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the Gameboy.                            *
 *                                                                          *
 ****************************************************************************/
unsigned char GBIORead(int iAddr)
{
unsigned char c;

// Not needed anymore because of the memory flag system
   if (iAddr < 0xfe00)
      return GBWorkRAMRead(iAddr);
   if (iAddr < 0xff00) // RAM
      return pGBMachine->mem_map[iAddr];
      
   switch (iAddr & 0xff) // ARM likes 8-bit constants
      {
      case 0x00: // buttons
      // These are sampled sometimes as much as 12 times per scanline!, no need to do this so frequently
         return pGBMachine->ucJoyButtons[pGBMachine->ucJoySelect];

      case 0x01: // serial data in
//         c = pGBMachine->mem_map[iAddr]; // get the current serial data byte
         c = pGBMachine->ucData; // we read our own data until it gets swapped
         break;
      case 0x02:
         c = (pGBMachine->mem_map[iAddr] & 0x83) | 0x7c;
         break;

      case 0x04: // timer divider - incremented 16384 times per second
         c = (unsigned char)((pGBMachine->iDivider - pGBMachine->iMasterClock) >> 8);
         break;

      case 0x05: // timer counter
         c = pGBMachine->ucTimerCounter;
         break;

      case 0x0f:
         c = pGBMachine->ucIRQFlags; // pending interrupt flags
         break;

      case 0x26: // sound status reg
         c = pGBMachine->mem_map[iAddr] & 0x80;
         if (pGBMachine->bSound1On)
            c |= 1;
         if (pGBMachine->bSound2On)
            c |= 2;
         if (pGBMachine->bSound3On)
            c |= 4;
         if (pGBMachine->bSound4On)
            c |= 8;
         break;

      case 0x40:
         c = pGBMachine->ucLCDC;
         break;

      case 0x41: // LCD status (hblank,vblank and so forth)
         c = pGBMachine->ucLCDSTAT | 0x80;
         break;

      case 0x42: // Y scroll
         c = (char)pGBMachine->iScrollY;
         break;

      case 0x43: // X scroll
         c = (char)pGBMachine->iScrollX;
         break;

      case 0x44: // current scan line
         c = (unsigned char)pGBMachine->iScanLine;
         break;

      case 0x45: // LYC - scan line compare
         c = (unsigned char)pGBMachine->iScanLineCompare;
         break;

      case 0x46: // DMA something
         c = 0;
         break;

      case 0x4d:
         if (pGBMachine->iCPUSpeed == 2)
            c = 0x80;
         else
            {
            if (pGBMachine->mem_map[iAddr] & 1)
               c = 1;
            else
               c = 0x7e;
            }
         break;

      case 0x4f: // VRAM Bank
         if (pGBMachine->iVRAMBank)
            c = 1;
         else
            c = 0;
         break;

      case 0x51: // HDMA source high byte
         c = (unsigned char)(pGBMachine->iDMASrc >> 8);
         break;

      case 0x52: // HDMA source low byte
         c = (unsigned char)(pGBMachine->iDMASrc & 0xff);
         break;

      case 0x53: // HDMA dest high byte
         c = (unsigned char)(pGBMachine->iDMADest >> 8);
         break;

      case 0x54: // HDMA dest low byte;
         c = (unsigned char)(pGBMachine->iDMADest & 0xff);
         break;

      case 0x55:
         if (pGBMachine->mem_map[0xff55] & 0x80) // HDMA
            {
            c = ((pGBMachine->iDMALen>>4)-1) & 0x7f;
//            c |= 0x80; // HDMA flag
	   	   if (!pGBMachine->bInDMA) // Bit 7 = DMA completed
               c |= 0x80; //0x7f;
            }
         else
            {
            c = 0xff; // normal DMA, FF = done
            }
         break;

      case 0x56: // IR control
         c = pGBMachine->mem_map[iAddr] | 2; // always show no IR data being received
         break;
                  
      case 0x68:
         c = pGBMachine->bBackIncrement | pGBMachine->ucBackIndex;
         break;

      case 0x69: // background palette
         c = pGBMachine->ucPalData[pGBMachine->ucBackIndex]; // color # 0 - 31 (high + low bytes)
         break;

      case 0x6a:
         c = pGBMachine->bObjectIncrement | pGBMachine->ucObjectIndex;
         break;

      case 0x6b: // object palette
         c = pGBMachine->ucPalData[64 + pGBMachine->ucObjectIndex]; // color # 0 - 31
         break;

      case 0x6c: // undocumented register
         c = pGBMachine->mem_map[iAddr] & 1;
         break;

      case 0x75: // undocumented register
         c = pGBMachine->mem_map[iAddr] & 0x70;
         break;

      case 0x76: // undocumented register
         c = 0;
         break;

      case 0x77: // undocumented register
         c = 0;
         break;

      case 0x70:
         c = pGBMachine->iWorkRAMBank;
         break;
      default:
         c = pGBMachine->mem_map[iAddr];
         break;
      }

   return c;

} /* GBIORead() */

// NB: Dragon Warrior III has problems after an encounter with monsters (screen shakes)
//     if O2 optimization is turned on for this function.  So I set it to custom optimization
//     with no inlining and favor fast code and that allows it to work
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBIOWrite(int, char)                                       *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the Gameboy.                            *
 *                                                                          *
 ****************************************************************************/
void GBIOWrite(int iAddr, unsigned char ucByte)
{
int i;
unsigned char c;

// Not needed anymore because of the memory flag system
   if (iAddr < 0xfe00)
      {
      GBWorkRAMWrite(iAddr, ucByte);
      return;
      }

    pGBMachine->mem_map[iAddr] = ucByte;
    if (iAddr < 0xff00)
       return; // fe00 to feff is ram
    switch (iAddr & 0xff) // ARM likes 8-bit constants
      {
      case 0x00: // joystick input select
         pGBMachine->ucJoySelect = (ucByte >> 4) & 3;
         break;

      case 0x01: // serial data to send
         pGBMachine->ucData = ucByte; // put it in the latch
         break;

      case 0x02: // serial transfer status
         if (/*(bNetworked || bHead2Head) && */(ucByte & 0x81) == 0x81) // we are sending data (internal clock)
            { // no matter if someone is connected or not, the operation will complete with an IRQ
            if (ucByte & 2) // high speed mode, need to handle it now
               {
               pGBMachine->ucIRQFlags |= GB_INT_SERIAL; // cause an IRQ to indicate the data has been sent
               pGBMachine->mem_map[0xff02] &= 0x3; // clear the busy bit
               c = pGBMachine->ucData; // Swap the data bytes from the sending end
               pGBMachine->ucData = gbmachine[1-iPlayer].ucData;
               gbmachine[1-iPlayer].ucData = c;
               gbmachine[1-iPlayer].ucIRQFlags |= GB_INT_SERIAL; // receiver gets an IRQ also
               }
            else
               {
               if (pGBMachine->iCPUSpeed == 2) // double speed mode
                  iSendData = 4; // double speed (2KB/s)
               else
                  iSendData = 9; // normal speed (1KB/s)
               }
            }
         if (/*bNetworked && */(ucByte & 0x80)) // we're connected to another machine
            {
#ifdef _WIN32_WCE
            unsigned char ucTemp[2];
            ucTemp[0] = ucByte; // status
            ucTemp[1] = pGBMachine->ucData; // data to send
            SGSendNetPacket(ucTemp, 2, SGNETWORK_GAMEDATA);
#endif
            if ((pGBMachine->ucNetStatus & 0x81) == 0x81) // we already received data
               {
               pGBMachine->mem_map[0xff02] &= 0x3; // clear the busy bit
               pGBMachine->ucNetStatus = 0;
               pGBMachine->ucData = pGBMachine->ucNetData; // get the new data byte
               pGBMachine->ucIRQFlags |= GB_INT_SERIAL; // cause an IRQ to indicate the data has been sent
               }
            }
         break;

      case 0x04: // clock divider
         pGBMachine->iDivider = pGBMachine->iMasterClock;
         break;

      case 0x05: // timer counter
         pGBMachine->ucTimerCounter = ucByte;
         break;

      case 0x06: // timer modulo
         pGBMachine->ucTimerModulo = ucByte;
         break;

      case 0x07: // timer control
         pGBMachine->ucTimerControl = ucByte;
         pGBMachine->bTimerOn = pGBMachine->ucTimerControl & 4;
         pGBMachine->iTimerMask = iTimerMasks[ucByte & 3];
         pGBMachine->iTimerLimit = pGBMachine->iMasterClock + pGBMachine->iTimerMask;
         break;

      case 0x0f:
         pGBMachine->ucIRQFlags = ucByte;
         break;

      case 0x10: // sound 1 sweep info
         i = (ucByte >> 4) & 7; // sweep time
         if (i == 0)
            pGBMachine->iSweepDelta = 0; // no sweep
         else
            pGBMachine->iSweepDelta = 190/i; // sweep delta = 65536 / (1/128 of 44400)
         pGBMachine->iSweepPos = 0;
         break;

      case 0x11: // sound 1 length
         pGBMachine->iSnd1StartLen = pGBMachine->iSnd1Len = (64 - (ucByte & 63))*172; // in 1/256 second increments
         break;

      case 0x12: // sound 1 envelope & vol
         pGBMachine->iSnd1StartVol = pGBMachine->iSnd1Vol = ucByte >> 4; // initial envelope volume 0-F
         ucByte &= 7;
         if (ucByte == 0)
            pGBMachine->iSnd1Delta = 0;
         else
            pGBMachine->iSnd1Delta = 95/ucByte; // envelope delta = 65536 / (1/64 of 44400)
         pGBMachine->iSnd1EnvPos = 0;
         break;

      case 0x13: // sound 1 freq low
         pGBMachine->iFreq1Start &= 0xff00;
         pGBMachine->iFreq1Start |= ucByte;
         pGBMachine->iFreq1 = pGBMachine->iFreq1Start;
         break;

      case 0x14: // sound 1 freq high + control
         pGBMachine->iFreq1Start &= 0xff;
         pGBMachine->iFreq1Start |= (ucByte & 7) << 8;
         pGBMachine->iFreq1 = pGBMachine->iFreq1Start;
         if (ucByte & 0x80) // start playing sound
            {
            pGBMachine->iSnd1Len = pGBMachine->iSnd1StartLen;
            pGBMachine->iSnd1Vol = pGBMachine->iSnd1StartVol;
            }
         break;

      case 0x16: // sound 2 length
         pGBMachine->iSnd2StartLen = pGBMachine->iSnd2Len = (64 - (ucByte & 63))*172; // in 1/256 second increments
         break;

      case 0x17: // sound 2 envelope & vol
         pGBMachine->iSnd2StartVol = pGBMachine->iSnd2Vol = ucByte >> 4; // initial envelope volume 0-F
         ucByte &= 7;
         if (ucByte == 0)
            pGBMachine->iSnd2Delta = 0;
         else
            pGBMachine->iSnd2Delta = 95/ucByte; // envelope delta = 65536 / (1/64 of 44400)
         pGBMachine->iSnd2EnvPos = 0;
         break;

      case 0x18: // sound 2 freq low
         pGBMachine->iFreq2 &= 0xff00;
         pGBMachine->iFreq2 |= ucByte;
         break;

      case 0x19: // sound 2 freq high + control
         pGBMachine->iFreq2 &= 0xff;
         pGBMachine->iFreq2 |= (ucByte & 7) << 8;
         if (ucByte & 0x80) // start playing sound
            {
            pGBMachine->iSnd2Len = pGBMachine->iSnd2StartLen;
            pGBMachine->iSnd2Vol = pGBMachine->iSnd2StartVol;
            }
         break;

      case 0x1a:
         if (iPlayer == 0)
            {
            pWaveQueue[iWaveQ].ucScanLine = iLoopCount;
            pWaveQueue[iWaveQ].ucIndex = 0xff;
            if (ucByte & 0x80) // sound on
               pWaveQueue[iWaveQ++].ucByte = WAVE_ON; // sound on command
            else
               pWaveQueue[iWaveQ++].ucByte = WAVE_OFF; // sound off command
            }
         break;

      case 0x1b: // wave duration
         pGBMachine->iWaveLen = pGBMachine->iWaveStartLen = (256 - ucByte) * 172; // number of destination samples to produce
         break;

//      case 0xff1c:
//         break;

      case 0x1d: // wave freq low
         pGBMachine->iWaveFreq &= 0x700;
         pGBMachine->iWaveFreq |= ucByte;
         break;

      case 0x1e: // wave freq high + control
         pGBMachine->iWaveFreq &= 0xff;
         pGBMachine->iWaveFreq |= (ucByte & 7) << 8;
         if (ucByte & 0x80) // initial on resets wave pointer
            {
//            iWaveLen = iWaveStartLen;
//            iWavePos = 0;
            // insert reset position command
            if (iPlayer == 0)
               {
               pWaveQueue[iWaveQ].ucScanLine = iLoopCount;
               pWaveQueue[iWaveQ].ucIndex = 0xff;
               pWaveQueue[iWaveQ++].ucByte = WAVE_RESET; // reset command
               }
            }
         break;

      case 0x20: // noise length
         pGBMachine->iNoiseStartLen = pGBMachine->iNoiseLen = (64 - (ucByte & 63))*172; // in 1/256 second increments
         break;

      case 0x21: // noise vol control/envelope
         pGBMachine->iNoiseStartVol = pGBMachine->iNoiseVol = ucByte >> 4;
         ucByte &= 7;
         if (ucByte == 0)
            pGBMachine->iNoiseDelta = 0; // no envelope activity
         else
            pGBMachine->iNoiseDelta = 95/ucByte; // envelope delta = 65536 / (1/64 of 44400)
         pGBMachine->iNoiseEnvPos = 0;
         break;

      case 0x22: // noise frequency and type
         pGBMachine->iNoiseFreq = 1 << (ucByte >> 4); // shift clock frequency divider
         pGBMachine->iNoiseFreq = 0x17c7000 / pGBMachine->iNoiseFreq;
         pGBMachine->iNoiseFreq /= ucNoiseDividers[ucByte & 7];
         pGBMachine->iNoiseFreq /= 4;
         if ((ucByte >> 6) == 3)
            pGBMachine->iNoiseLen = 0; // bogus shift clock, stop sound
         break;

      case 0x23:
         if (ucByte & 0x80) // start playing noise
            {
            pGBMachine->iNoiseLen = pGBMachine->iNoiseStartLen;
            pGBMachine->iNoiseVol = pGBMachine->iNoiseStartVol;
            }
         break;

      case 0x24:
         break;

//      case 0x25: // sound channels
//         pWaveQueue[iWaveQ].ucScanLine = iLoopCount;
//         pWaveQueue[iWaveQ].ucIndex = 0xff;
//         pWaveQueue[iWaveQ++].ucByte = WAVE_CHANNELS | (ucByte & 0x44); // we only queue the wave channel bits
//         break;

//      case 0x26: // master enable / status
//         ucByte = 0;
//         break;

      case 0x30: // pre-convert the sound samples for faster access
      case 0x31:
      case 0x32:
      case 0x33:
      case 0x34:
      case 0x35:
      case 0x36:
      case 0x37:
      case 0x38:
      case 0x39:
      case 0x3a:
      case 0x3b:
      case 0x3c:
      case 0x3d:
      case 0x3e:
      case 0x3f:
         if (iPlayer == 0)
            {
            pWaveQueue[iWaveQ].ucIndex = iAddr - 0xff30; // Queue this WAVE table change
            pWaveQueue[iWaveQ].ucScanLine = iLoopCount; //iScanLine;
            pWaveQueue[iWaveQ++].ucByte = ucByte;
            }
//         i = (iAddr - 0xff30)<<1;
//         pGBMachine->ucSamples[i] = (ucByte >> 2) & 0x3c;  // shift up by 2 to balance out to the other sound channels
//         pGBMachine->ucSamples[i+1] = (ucByte << 2) & 0x3c;
         break;

      case 0x40: // LCD Control
         if (ucByte & 0x80 && !(pGBMachine->ucLCDC & 0x80)) // LCD being turned on, reset scan line
            pGBMachine->iScanLine = 0;
         if (pGBMachine->ucLCDC & 0x80 && pGBMachine->iScanLine < 144)
            ucByte |= 0x80; // LCD cannot be turned off outside of VBlank
         if ((pGBMachine->ucLCDC & 4) != (ucByte & 4)) // sprite size changing
            pGBMachine->bSpriteChanged = TRUE;
         pGBMachine->ucLCDC = ucByte;
         break;

      case 0x41: // LCD Status
         pGBMachine->ucLCDSTAT &= 7;
         pGBMachine->ucLCDSTAT |= (ucByte & 0x78);
         break;

      case 0x42: // Y scroll
         pGBMachine->iScrollY = ucByte;
         break;

      case 0x43: // X scroll
         pGBMachine->iScrollX = ucByte;
         break;

//      case 0x44: // current scan line - writing resets to 0
//         pGBMachine->iScanLine = 0;
//         break;

      case 0x45: // scan line compare
         pGBMachine->iScanLineCompare = ucByte;
         break;

      case 0x46: // do a DMA transfer to OAM - does not appear to need precise timing
         pGBMachine->bSpriteChanged = TRUE; // need to rescan sprite list
			switch((ucByte>>5) & 7) // different memory regions need different attention
            {
			   case 0:
			   case 1:
				   memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->pBankROM[ucByte*256],0xA0);
				   break;
			   case 2:
			   case 3:
				   memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->pBank[ucByte*256],0xA0);
				   break;
			   case 4:
				   memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->pVRAM[pGBMachine->iVRAMBank+(ucByte&0x1f)*256],0xA0);
				   break;
			   case 5:
				   memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->pRAMBank[ucByte*256],0xA0);
				   break;
			   case 6:
               if (ucByte & 0x10) // work RAM (0xd000)
                  memcpy(&pGBMachine->mem_map[0xfe00], &pGBMachine->pWorkRAMBank[(ucByte & 0xf)*256], 0xa0);
               else
				      memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->mem_map[(ucByte+0x20)*256],0xA0);
				   break;
			   case 7:
				   memcpy(&pGBMachine->mem_map[0xfe00],&pGBMachine->mem_map[ucByte*256],0xA0);
				   break;
			   }
         break;

      case 0x47: // monochrome background palette
         if (!pGBMachine->bCGB) // only valid on mono gameboy
            {
            for (i=0; i<4; i++)
               {
               pGBMachine->usPalData[i] = usColorTable[(ucByte >> (i*2)) & 3]; // 2-bit shade of gray
               }
            pGBMachine->ulPalChanged |= 0xf; // first 4 colors changed
            }
         break;

      case 0x48: // monochrome sprite palette 0
         if (!pGBMachine->bCGB) // only valid on mono gameboy
            {
            for (i=1; i<4; i++) // transparent color 0 not used
               {
               pGBMachine->usPalData[32+i] = usColorTable[(ucByte >> (i*2)) & 3]; // 2-bit shade of gray
               }
            pGBMachine->ulSpritePalChanged |= 0xf; // first 4 obj colors changed
            }
         break;

      case 0x49: // monochrome sprite palette 1
         if (!pGBMachine->bCGB) // only valid on mono gameboy
            {
            for (i=1; i<4; i++) // transparent color 0 not used
               {
               pGBMachine->usPalData[36+i] = usColorTable[(ucByte >> (i*2)) & 3]; // 2-bit shade of gray
               }
            pGBMachine->ulSpritePalChanged |= 0xf0; // second 4 obj colors changed
            }
         break;

      case 0x4a: // Window Y
         pGBMachine->iWindowY = ucByte;
         break;

      case 0x4b: // Window X
         pGBMachine->iWindowX = ucByte;
         pGBMachine->iWindowX -= 7; // true position on video
         break;

      case 0x4d: // CPU speed
         pGBMachine->bSpeedChange = ucByte & 1;
         break;

      case 0x4f: // VRAM bank
         if (pGBMachine->bInDMA) // can't change bank while DMA is in progress
            return;
         pGBMachine->iVRAMBank = 0x2000 * (ucByte & 1);
         pGBMachine->pVRAMBank = &pGBMachine->pVRAM[-0x8000 + pGBMachine->iVRAMBank];
         pGBMachine->ulOpList[0x8] = (unsigned long)pGBMachine->pVRAMBank;
         pGBMachine->ulOpList[0x9] = (unsigned long)pGBMachine->pVRAMBank;
         break;

      case 0x51: // VRAM DMA source high byte
         pGBMachine->iDMASrc &= 0xf0;
         pGBMachine->iDMASrc |= ((unsigned int)ucByte << 8);
         break;

      case 0x52: // VRAM DMA source low byte
         pGBMachine->iDMASrc &= 0xff00;
         pGBMachine->iDMASrc |= (ucByte & 0xf0);
         break;

      case 0x53: // VRAM DMA dest high byte
         pGBMachine->iDMADest &= 0xf0;
         pGBMachine->iDMADest |= ((unsigned int)ucByte << 8);
         break;

      case 0x54: // VRAM DMA dest low byte;
         pGBMachine->iDMADest &= 0xff00;
         pGBMachine->iDMADest |= (ucByte & 0xf0);
         break;

      case 0x55: // VRAM DMA len/control
//         if (bInDMA && ucByte & 0x80)
//            i = 0;
         if (pGBMachine->bInDMA && !(ucByte & 0x80)) // if already doing HDMA, bit7=0 cancels
            {
            pGBMachine->bInDMA = FALSE;
            return;
            }
         pGBMachine->iDMALen = ((ucByte & 0x7f) + 1) << 4; // transfer length in bytes
         pGBMachine->iDMADest &= 0x1fff; // can only go to VRAM
         pGBMachine->iDMADest |= 0x8000;
         pGBMachine->pDMADest = &pGBMachine->pVRAM[(pGBMachine->iDMADest & 0x1ff0)+pGBMachine->iVRAMBank];
         if (pGBMachine->iDMADest + pGBMachine->iDMALen > 0xa000) // will go past end
            pGBMachine->iDMALen = 0xa000 - pGBMachine->iDMADest;
			switch((pGBMachine->iDMASrc>>13) & 7) // different memory regions need different attention
            {
			   case 0:
			   case 1:
				   pGBMachine->pDMASrc = &pGBMachine->pBankROM[pGBMachine->iDMASrc];
				   break;
			   case 2:
			   case 3:
				   pGBMachine->pDMASrc = &pGBMachine->pBank[pGBMachine->iDMASrc];
				   break;
			   case 5:
				   pGBMachine->pDMASrc = &pGBMachine->pRAMBank[pGBMachine->iDMASrc];
				   break;
			   case 6:
               if (pGBMachine->iDMASrc & 0x1000) // work RAM (0xd000)
                  pGBMachine->pDMASrc = &pGBMachine->pWorkRAMBank[pGBMachine->iDMASrc & 0xfff];
               else
				      pGBMachine->pDMASrc = &pGBMachine->mem_map[pGBMachine->iDMASrc + 0x2000];
				   break;
			   case 7:
				   pGBMachine->pDMASrc = &pGBMachine->mem_map[pGBMachine->iDMASrc];
				   break;
			   }
// NOTE: do the first HDMA transfer immediately.  This fixed problems with Sierra 3D pinball
         if (ucByte & 0x80) // HDMA
            {
            pGBMachine->bInDMA = TRUE;
            if (pGBMachine->iDMALen > 0x10) // for longer HDMA transfers, we can do it later
               return; // this fixes Pokeymon Crystal, Boarder Zone, and others
            // 3D-pinball needs it done immediately
//            pFlags[((int)pDMADest & 0x3ff0) >> 4] = 1; // tile number affected
            memcpy(pGBMachine->pDMADest, pGBMachine->pDMASrc, 0x10);
            pGBMachine->iDMASrc += 0x10;
            pGBMachine->iDMADest += 0x10;
            pGBMachine->iDMALen -= 0x10;
            pGBMachine->pDMADest += 0x10;
            pGBMachine->pDMASrc += 0x10;
            if (pGBMachine->iDMALen == 0)
               pGBMachine->bInDMA = FALSE;
            }
         else // one shot copy (GDMA)
            {
            pGBMachine->bInDMA = FALSE;
            pGBMachine->iDMATime = (pGBMachine->iDMALen*5)>>1;
//            if (pGBMachine->iCPUSpeed == 1)
//               pGBMachine->iDMATime = 440/*880*/ + pGBMachine->iDMALen * 2; // number of clock ticks to halt CPU
//            else
//               pGBMachine->iDMATime = 440 + pGBMachine->iDMALen * 4;
            memcpy(pGBMachine->pDMADest,pGBMachine->pDMASrc,pGBMachine->iDMALen);
//            j = ((int)pDMADest & 0x3ff0) >> 4; // tile numbers affected
//            for (i=0; i<iDMALen>>4; i++)
//               pFlags[j++] = 1; // mark this area as changed
            pGBMachine->iDMASrc += pGBMachine->iDMALen; // update internal registers after operation complete
            pGBMachine->iDMADest += pGBMachine->iDMALen; // needed for Mr. Driller
            }
         break;

      case 0x68: // background palette index
         pGBMachine->bBackIncrement = ucByte & 0x80;
         pGBMachine->ucBackIndex = ucByte & 0x3f;
         break;

      case 0x69: // background palette data register
         pGBMachine->ulPalChanged |= (1 << (pGBMachine->ucBackIndex>>1)); // mark this color as needing a refresh
         pGBMachine->ucPalData[pGBMachine->ucBackIndex] = ucByte;
         if (pGBMachine->bBackIncrement) // auto increment?
            pGBMachine->ucBackIndex = (pGBMachine->ucBackIndex + 1) & 0x3f;
         break;

      case 0x6a: // object palette index
         pGBMachine->bObjectIncrement = ucByte & 0x80;
         pGBMachine->ucObjectIndex = ucByte & 0x3f;
         break;

      case 0x6b: // object palette data register
         pGBMachine->ulSpritePalChanged |= (1 << (pGBMachine->ucObjectIndex>>1)); // mark this color as needing a refresh
         pGBMachine->ucPalData[64+pGBMachine->ucObjectIndex] = ucByte;
         if (pGBMachine->bObjectIncrement) // auto increment?
            pGBMachine->ucObjectIndex = (pGBMachine->ucObjectIndex+1) & 0x3f;
         break;

      case 0x70: // RAM bank select?
         ucByte &= 7;
         if (ucByte == 0) // bank 0 is not allowed
            ucByte = 1;
         pGBMachine->iWorkRAMBank = ucByte;
         pGBMachine->pWorkRAMBank = &pGBMachine->pWorkRAM[pGBMachine->iWorkRAMBank * 0x1000];
         pGBMachine->ulOpList[0xd] = (unsigned long)&pGBMachine->pWorkRAMBank[-0xd000]; // banked Work RAM
         break;

      case 0xff: // interrupt enables
         pGBMachine->ucIRQEnable = ucByte;
         break;

      default:
         break;
      }
} /* GBIOWrite() */

//#pragma optimize("g", off)

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBProcessSprites(void)                                     *
 *                                                                          *
 *  PURPOSE    : Scan the sprites for valid ones; mark each scanline.       *
 *                                                                          *
 ****************************************************************************/
void GBProcessSprites(void)
{
int i, y, iSize, iStart, iEnd;
unsigned char *pSprite;

   iSize = (pGBMachine->ucLCDC & 4) ? 16 : 8; // height of the sprites
   pSprite = &pGBMachine->mem_map[0xfe00];
   pGBMachine->iSpriteCount = 0;
   memset(pGBMachine->ucSpriteDraw, 0, sizeof(pGBMachine->ucSpriteDraw)); // start with no sprites
   for (i=0x9c; i>=0; i-=4) // scan in reverse order
      {
      y = pSprite[i];
      if (y) // if this value is 0, sprite is disabled
         {
         y -= 16;
         if (y+iSize >0 && y < 144) // sprite is visible
            {
            pGBMachine->ucSpriteList[pGBMachine->iSpriteCount++] = i;
            // mark the lines which this sprite touches
            iStart = y;
            iEnd = y + iSize;
            if (iStart < 0)
               iStart = 0;
            if (iEnd > 144)
               iEnd = 144;
            while (iStart < iEnd)
               pGBMachine->ucSpriteDraw[iStart++] = 1; // mark the scanlines
            } // if sprite visible
         } // if sprite active
      } // for each sprites

} /* GBProcessSprites() */

#define MAGIC_EXPAND 0x204081
#define MAGIC_MASK 0x01010101

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBDrawScanline(void)                                       *
 *                                                                          *
 *  PURPOSE    : Render the current scanline.                               *
 *                                                                          *
 ****************************************************************************/
void GBDrawScanline(void)
{
uint32_t ulAttr;
unsigned char c, c2;
uint32_t ulLeft = 0;
uint32_t ulRight = 0;
uint32_t ulColor;
unsigned char *pDest, *pSrc;
int i, x, y, iHeight, iTileMask;
int iScanOffset, iTile, iTileOffset, iWidth;
unsigned char *pTileData, *pSprite, *pTile, *pEdge;//, *pTileData;
uint32_t *pulDest;
unsigned char ucTemp[512]; // 8-bit pixel holding area
signed int /*iOldTile,*/ iSpriteSkip;
uint32_t ulOldAttr = 0;
#ifndef BOGUS__arm__
unsigned char *pSrcFlipped;
#endif

   if (pGBMachine->iScanLine == 0)
      pGBMachine->iWindowUsed = 0;
   if (pGBMachine->bSpriteChanged) // rescan the sprite list
      {
      GBProcessSprites();
      pGBMachine->bSpriteChanged = FALSE;
      }
// Draw the background
//   if (ucLCDC & 1) // if background enabled
if (1) // apparently this cannot be disabled or games like "Blue's Clues" stop working
      {
      iWidth = 160;
      if (pGBMachine->iScrollX & 7)
         iWidth = 176; // make sure to get slop over
      if (pGBMachine->ucLCDC & 0x20 && pGBMachine->iWindowX < 160 && pGBMachine->iWindowY <= pGBMachine->iScanLine) // if window enabled we don't draw where the window covers the background tiles
         iWidth = (pGBMachine->iWindowX+15) & 0xf8; // draw partial tile if overlapping
      pDest = &ucTemp[8];
      y = (pGBMachine->iScrollY + pGBMachine->iScanLine) & 0xff; // current scan line to draw
      if (pGBMachine->ucLCDC & 8)
         iTileOffset = 0x1c00;
      else
         iTileOffset = 0x1800;
      pTile = &pGBMachine->pVRAM[iTileOffset+ (y>>3)*32];
      pEdge = pTile + 32; // right edge to scroll around
      pTile += (pGBMachine->iScrollX>>3); // add scroll offset
      pSrc = &pGBMachine->pVRAM[(y&7) << 1];
#ifdef BOGUS__arm__
	   ARMDrawGBCTiles(pTile, pEdge, pSrc, pGBMachine->ucLCDC, pDest, iWidth>>3);
#else 
//      iOldTile = -1;
      pSrcFlipped = &pGBMachine->pVRAM[(7-(y&7)) << 1];
 	  for (x=(iWidth>>3); x>0; x--)
         {
         if (pTile >= pEdge)
            pTile -= 32; // wrap around back to the left side because of horizontal scrolling
         ulAttr = pTile[0x2000];
         if (pGBMachine->ucLCDC & 16) // tile data select
            iTile = *pTile++;
         else
            iTile = (signed char)*pTile++ + 256;
         if (ulAttr & 8) // second bank of tiles
            iTile += 512;
         ulAttr |= (iTile << 8);
         if (/*iTile != iOldTile ||*/ ulAttr != ulOldAttr) // if something changed, recalc
            {
//            iOldTile = iTile;
            ulOldAttr = ulAttr; // for next time
            if (ulAttr & 0x40) // vertically flipped
               {
               c = pSrcFlipped[iTile<<4]; // plane 0
               c2 = pSrcFlipped[(iTile<<4)+1]; // plane 1
               }
            else
               {
               c = pSrc[iTile<<4]; // plane 0
               c2 = pSrc[(iTile<<4)+1]; // plane 1
               }
            // Draw the 8 pixels of the tile
            ulLeft = ulRight = ulAttrColor[ulAttr & 0xff];
//            if (ucAttr & 0x20) // horizontal flip
//               {
//               c = ucGBMirror[c];
//               c2 = ucGBMirror[c2];
//               }
            if (c)
               {
//               ulLeft |= MAGIC_EXPAND * (c>>4);
//               ulRight |= MAGIC_EXPAND * (c & 0xf);
               ulLeft |= ulExpand[c >> 4];
               ulRight |= ulExpand[c & 0xf];
               }
            if (c2)
               {
//		ulLeft |= MAGIC_EXPAND * ((c>>3) & 0x1e);
//		ulRight |= MAGIC_EXPAND * ((c << 1) & 0x1e);
               ulLeft |= ulExpand2[(c2 >> 4)];
               ulRight |= ulExpand2[(c2 & 0xf)];
               }
            } // if tile changed
         if (ulAttr & 0x20)
         {
         *(uint32_t *)pDest = __builtin_bswap32(ulRight);
         *(uint32_t *)&pDest[4] = __builtin_bswap32(ulLeft);
         }
         else
         {
         *(uint32_t *)pDest = ulLeft;
         *(uint32_t *)&pDest[4] = ulRight;
         }
         pDest += 8;
         } // for x
#endif
      }
   else // when background is disabled, window is background color
      {
      memset(&ucTemp[8], 0, 160); // set to color 0
      }
// Draw the "Window"
   if (pGBMachine->ucLCDC & 0x20 && pGBMachine->iWindowX < 160 && pGBMachine->iWindowY <= pGBMachine->iScanLine) // if window enabled & visible on this scan line
      {
      iWidth = 160 - pGBMachine->iWindowX;
      pDest = &ucTemp[8+pGBMachine->iWindowX+(pGBMachine->iScrollX & 7)];
      y = (pGBMachine->iWindowUsed++) & 0xff; // current scan line to draw (always counts down the window from 0)
      if (pGBMachine->ucLCDC & 0x40)
         iTileOffset = 0x1c00;
      else
         iTileOffset = 0x1800;
      pTile = &pGBMachine->pVRAM[iTileOffset + (y>>3)*32];
//      iOldTile = -1;
      for (x=((iWidth+7)>>3); x>0; x--)
         {
         ulAttr = pTile[0x2000];
         if (pGBMachine->ucLCDC & 16) // tile data select
            iTile = pTile[0];
         else
            iTile = (signed char)pTile[0] + 256;
         if (ulAttr & 8)
            iTile += 512;
         ulAttr |= (iTile << 8);
         if (/*iTile != iOldTile ||*/ ulAttr != ulOldAttr) // new data, need to recalc
            {
            ulOldAttr = ulAttr;
            if (ulAttr & 0x40) // vertically flipped
               iScanOffset = (7-(y&7)) << 1;
            else
               iScanOffset = (y & 7) << 1; // offset to correct byte in tile data
            pTileData = &pGBMachine->pVRAM[(iTile << 4) + iScanOffset];
            ulLeft = ulRight = ulAttrColor[ulAttr & 0xff];
            // Draw the 8 pixels of the tile
            c = pTileData[0]; // plane 0
            c2 = pTileData[1]; // plane 1
            if (ulAttr & 0x20) // horizontal flip
               {
               if (c)
                  {
                  ulLeft |= ulExpandMirror[c & 0xf];
                  ulRight |= ulExpandMirror[c >> 4];
                  }
               if (c2)
                  {
                  ulLeft |= ulExpandMirror2[(c2 & 0xf)];
                  ulRight |= ulExpandMirror2[(c2 >> 4)];
                  }
               }
            else
               {
               if (c)
                  {
                  ulLeft |= ulExpand[c >> 4];
                  ulRight |= ulExpand[c & 0xf];
                  }
               if (c2)
                  {
                  ulLeft |= ulExpand2[(c2 >> 4)];
                  ulRight |= ulExpand2[(c2 & 0xf)];
                  }
               }
//            iOldTile = iTile;
//            ucOldAttr = ucAttr; // for comparison next time
            } // if data changed
         pTile++;
//         if (!(iWindowX & 3)) // dword aligned
//            {
//            *(uint32_t *)pDest = ulLeft;
//            *(uint32_t *)&pDest[4] = ulRight;
//            }
//         else
//            {
            pDest[0] = (unsigned char)ulLeft;
            pDest[1] = (unsigned char)(ulLeft >> 8);
            pDest[2] = (unsigned char)(ulLeft >> 16);
            pDest[3] = (unsigned char)(ulLeft >> 24);
            pDest[4] = (unsigned char)ulRight;
            pDest[5] = (unsigned char)(ulRight >> 8);
            pDest[6] = (unsigned char)(ulRight >> 16);
            pDest[7] = (unsigned char)(ulRight >> 24);
//            }
         pDest += 8;
         }
      } // if "Window" enabled

// Draw the sprites which appear on this scan line
   if (pGBMachine->ucLCDC & 2 && pGBMachine->ucSpriteDraw[pGBMachine->iScanLine]) // if sprites enabled and present on this line
      {
      if (pGBMachine->ucLCDC & 4) // 8x16 sprites
         {
         iTileMask = 0xfe; // only even tile numbers for tall sprites
         iHeight = 15;
         }
      else // 8x8 sprites
         {
         iTileMask = 0xff;
         iHeight = 7;
         }
 // Enforce the "Max 10 sprites per line" rule
      iSpriteSkip = 0;
      for (i=0; i<pGBMachine->iSpriteCount; i++)
         {
         y = pGBMachine->mem_map[0xfe00 + pGBMachine->ucSpriteList[i]] - 16;
         if (y <= pGBMachine->iScanLine && (y + iHeight) >= pGBMachine->iScanLine) // this sprite will be drawn
            {
            iSpriteSkip++;
            }
         }
      if (iSpriteSkip > 10)
         iSpriteSkip -= 10; // allow 10 max
      else
         iSpriteSkip = 0; // allow all to be draw   
      for (i=0; i<pGBMachine->iSpriteCount; i++) // only check the active sprites
         {
         pSprite = &pGBMachine->mem_map[0xfe00 + pGBMachine->ucSpriteList[i]];
         y = pSprite[0] - 16;
         x = pSprite[1] - 8 + (pGBMachine->iScrollX & 7);
         if (x < 168 && y <= pGBMachine->iScanLine && (y + iHeight) >= pGBMachine->iScanLine) // need to draw this line
            {
            if (iSpriteSkip) // we need to skip this one to enfore the 10 per line rule
               {
               iSpriteSkip--;
               continue; // don't draw this one
               }
            pDest = &ucTemp[8+x];
            iTile = pSprite[2] & iTileMask;
            ulAttr = pSprite[3];
            if (ulAttr & 8) // selects second bank of tiles
               iTile += 512;
            if (ulAttr & 0x40) // vertically flipped
               iScanOffset = (iHeight-((pGBMachine->iScanLine-y) & iHeight)) << 1; // offset to correct byte in tile data
            else
               iScanOffset = ((pGBMachine->iScanLine-y) & iHeight) << 1;
            pTileData = &pGBMachine->pVRAM[(iTile<<4)+iScanOffset];
            if (pGBMachine->bCGB) // if color gameboy
               ulColor = ((ulAttr & 7) << 2) + 32; // palette entries for objects starts at 32
            else
               ulColor = ((ulAttr & 16) >> 2) + 32; // bit 4 selects palette 0 or 1
            ulLeft = ulRight = ulColor | (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
         // Draw the 8 pixels of the sprite
            c = pTileData[0]; // plane 0
            c2 = pTileData[1]; // plane 1
            if (ulAttr & 0x20) // horizontally flipped
               {
               c = ucGBMirror[c];
               c2 = ucGBMirror[c2];
               }
            if (c)
               {
               ulLeft |= ulExpand[c >> 4];
               ulRight |= ulExpand[c & 0xf];
               }
            if (c2)
               {
               ulLeft |= (ulExpand2[c2 >> 4]);
               ulRight |= (ulExpand2[c2 & 0xf]);
               }
            if (pGBMachine->ucLCDC & 1) // use sprite/bg priorities if there are any high priority tiles on this line
               {
               if (ulLeft & 0x03030303) // if the first 4 bytes have something to draw
                  {
                  if (ulLeft & 0x3)
                     {
                     c2 = pDest[0] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[0] = (char)ulLeft;
                     }
                  if (ulLeft & 0x0300)
                     {
                     c2 = pDest[1] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[1] = (char)(ulLeft >> 8);
                     }
                  if (ulLeft & 0x030000)
                     {
                     c2 = pDest[2] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[2] = (char)(ulLeft >> 16);
                     }
                  if (ulLeft & 0x03000000)
                     {
                     c2 = pDest[3] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[3] = (char)(ulLeft >> 24);
                     }
                  }
               if (ulRight & 0x03030303) // if the second 4 bytes have something to draw
                  {
                  if (ulRight & 0x3)
                     {
                     c2 = pDest[4] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[4] = (char)ulRight;
                     }
                  if (ulRight & 0x0300)
                     {
                     c2 = pDest[5] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[5] = (char)(ulRight >> 8);
                     }
                  if (ulRight & 0x030000)
                     {
                     c2 = pDest[6] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[6] = (char)(ulRight >> 16);
                     }
                  if (ulRight & 0x03000000)
                     {
                     c2 = pDest[7] & 0x83;
                     if (c2 <= 0x80 && !((ulAttr & 0x80) && c2)) // BG is not higher priority
                        pDest[7] = (char)(ulRight >> 24);
                     }
                  }
               } // use sprite/bg priorities
            else // Sprites have priority over everything
               {
               if (ulLeft & 0x03030303) // if the first 4 bytes have something to draw
                  {
                  if (ulLeft & 3)
                     pDest[0] = (char)ulLeft;
                  if (ulLeft & 0x0300)
                     pDest[1] = (char)(ulLeft >> 8);
                  if (ulLeft & 0x030000)
                     pDest[2] = (char)(ulLeft >> 16);
                  if (ulLeft & 0x03000000)
                     pDest[3] = (char)(ulLeft >> 24);
                  }
               if (ulRight & 0x03030303) // if the second 4 bytes have something to draw
                  {
                  if (ulRight & 3)
                     pDest[4] = (char)ulRight;
                  if (ulRight & 0x0300)
                     pDest[5] = (char)(ulRight >> 8);
                  if (ulRight & 0x030000)
                     pDest[6] = (char)(ulRight >> 16);
                  if (ulRight & 0x03000000)
                     pDest[7] = (char)(ulRight >> 24);
                  }
               }
            }
         } // for each of the 40 sprites
      } // sprites are enabled
// Update the 2 palettes
if (pGBMachine->ulPalChanged)
   {
   for (i=0; i<32; i++)
      {
      if (pGBMachine->ulPalChanged & (1 << i))
         {
         unsigned short us;
         us = (pGBMachine->usPalData[i] & 0x1f) << 11; // R
         us |= (pGBMachine->usPalData[i] & 0x3e0) << 1; // G
         us |= (pGBMachine->usPalData[i] & 0x7c00) >> 10; // B
         pGBMachine->usPalConvert[i+128] = pGBMachine->usPalConvert[i] = us; // RGB565 palette for rendering tiles
         }
      }
   pGBMachine->ulPalChanged = 0;
   }
if (pGBMachine->ulSpritePalChanged)
   {
   for (i=0; i<32; i++)
      {
      if (pGBMachine->ulSpritePalChanged & (1 << i))
         {
         unsigned short us;
         us = (pGBMachine->usPalData[i+32] & 0x1f) << 11; // R
         us |= (pGBMachine->usPalData[i+32] & 0x3e0) << 1; // G
         us |= (pGBMachine->usPalData[i+32] & 0x7c00) >> 10; // B
         pGBMachine->usPalConvert[i+128+32] = pGBMachine->usPalConvert[i+32] = us; // RGB565 palette for rendering sprites
         }
      }
   pGBMachine->ulSpritePalChanged = 0;
   }

// Convert the 8-bit pixels to 16-bits by translating through the palette
#ifdef FAST_DRAWING //_WIN32_WCE
   if (iDisplayWidth == 176) // for lores smartphone, draw directly on the display
      pulDest = (uint32_t *)&pScreen[iScreenPitch*(pGBMachine->iScanLine+38) + 16];
   else
#endif //
      pulDest = (uint32_t *)&pLocalBitmap[iPitch * pGBMachine->iScanLine];
   pDest = &ucTemp[8+(pGBMachine->iScrollX & 7)];
#ifdef _WIN32_WCE
   ARM816FAST(pDest, pulDest, pGBMachine->usPalConvert, 160/8);
#else
   if (pGBMachine->iScrollX & 3) // need to do it a byte at a time
      {
      for (i=0;i<160; i+=8)
         {
         pulDest[0] = pGBMachine->usPalConvert[pDest[0]] | (pGBMachine->usPalConvert[pDest[1]] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pGBMachine->usPalConvert[pDest[2]] | (pGBMachine->usPalConvert[pDest[3]] << 16);
         pulDest[2] = pGBMachine->usPalConvert[pDest[4]] | (pGBMachine->usPalConvert[pDest[5]] << 16);
         pulDest[3] = pGBMachine->usPalConvert[pDest[6]] | (pGBMachine->usPalConvert[pDest[7]] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
   else // can read a long at a time
      {
      for (i=0;i<160; i+=8)
         {
         ulLeft = *(uint32_t *)&pDest[0];
         pulDest[0] = pGBMachine->usPalConvert[ulLeft & 0xff] | (pGBMachine->usPalConvert[(ulLeft >> 8) & 0xff] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pGBMachine->usPalConvert[(ulLeft >> 16) & 0xff] | (pGBMachine->usPalConvert[ulLeft >> 24] << 16);
         ulLeft = *(uint32_t *)&pDest[4];
         pulDest[2] = pGBMachine->usPalConvert[ulLeft & 0xff] | (pGBMachine->usPalConvert[(ulLeft >> 8) & 0xff] << 16); // unrolling the loop speeds it up considerably
         pulDest[3] = pGBMachine->usPalConvert[(ulLeft >> 16) & 0xff] | (pGBMachine->usPalConvert[ulLeft >> 24] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
#endif // _WIN32_WCE

} /* GBDrawScanline() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBGenNoise(void)                                           *
 *                                                                          *
 *  PURPOSE    : Generate the polynomial noise of sound channel 4.          *
 *                                                                          *
 ****************************************************************************/
void GBGenNoise(void)
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

} /* GBGenNoise() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBReset(GBMACHINE *)                                       *
 *                                                                          *
 *  PURPOSE    : Reset the state of the emulator.                           *
 *                                                                          *
 ****************************************************************************/
void GBReset(GBMACHINE *pGBTemp)
{
int iDay, iHour, iMinute, iSecond;

    pGBTemp->ucJoyButtons[3] = 0xff; // this value never changes
    pGBTemp->pBank = pGBTemp->pBankROM; // initialize bank pointer to bank #1
	 pGBTemp->iWorkRAMBank = 1; // start pointing to bank 1 (bank 0 is not valid the way we do it)
    pGBTemp->pWorkRAMBank = &pGBTemp->pWorkRAM[0x1000];
    pGBTemp->iROMBank = 1;
    pGBTemp->iRAMBank = 0;
    pGBTemp->pRAMBank = &pGBTemp->pBankRAM[(pGBTemp->iRAMBank-5)*0x2000];
    pGBTemp->bRAMEnable = FALSE;
    pGBTemp->iCPUSpeed = 1; // reset to 4.19Mhz
    pGBTemp->iVRAMBank = 0;
    pGBTemp->pVRAMBank = &pGBTemp->pVRAM[-0x8000];
    pGBTemp->bInDMA = FALSE;
    pGBTemp->ucMemoryModel = 1;
    pGBTemp->ucHUC1 = 0;
    pGBTemp->iOldRAM = -1;
    pGBTemp->iOldROM = -1;
    iSendData = 10;
    pGBTemp->bSpeedChange = FALSE;
	// Make sure the sound is silenced
    pGBTemp->bSound1On = pGBTemp->bSound2On = pGBTemp->bSound3On = pGBTemp->bSound4On = FALSE;
	 memset(pGBTemp->ucSamples, 0, 32); // make sure WAV samples are silent
    pGBTemp->iSnd1StartLen = pGBTemp->iSnd1Len = pGBTemp->iSnd2StartLen = pGBTemp->iSnd2Len = pGBTemp->iWaveStartLen = pGBTemp->iWaveLen = pGBTemp->iNoiseStartLen = pGBTemp->iNoiseLen = 0;
    pGBTemp->iWaveFreq = 0x7ff;
    pGBTemp->iFreq1 = 2049;
    pGBTemp->iFreq2 = 0x7ff;
    pGBTemp->iNoiseVol = 0;

    pGBTemp->ucLCDC = pGBTemp->ucLCDSTAT = 0;
    pGBTemp->iMasterClock = 0;
    pGBTemp->ucTimerCounter = pGBTemp->ucTimerModulo = pGBTemp->iTimerMask = 0;
    pGBTemp->ucTimerControl = 0;

// Setup the opcode offset list for faster access (divide memory into 4K chunks)
    pGBTemp->ulOpList[0x0] = (unsigned long)pGBTemp->pBankROM; // first 16K is non-banked ROM
    pGBTemp->ulOpList[0x1] = (unsigned long)pGBTemp->pBankROM;
    pGBTemp->ulOpList[0x2] = (unsigned long)pGBTemp->pBankROM;
    pGBTemp->ulOpList[0x3] = (unsigned long)pGBTemp->pBankROM;
    pGBTemp->ulOpList[0x4] = (unsigned long)pGBTemp->pBank; // second 16K is banked ROM
    pGBTemp->ulOpList[0x5] = (unsigned long)pGBTemp->pBank;
    pGBTemp->ulOpList[0x6] = (unsigned long)pGBTemp->pBank;
    pGBTemp->ulOpList[0x7] = (unsigned long)pGBTemp->pBank;
    pGBTemp->ulOpList[0x8] = (unsigned long)pGBTemp->pVRAMBank; // VRAM should never be accessed for opcodes
    pGBTemp->ulOpList[0x9] = (unsigned long)pGBTemp->pVRAMBank; // VRAM should never be accessed for opcodes
    pGBTemp->ulOpList[0xa] = (unsigned long)pGBTemp->pRAMBank; // Banked RAM at 0xA000-0xB000
    pGBTemp->ulOpList[0xb] = (unsigned long)pGBTemp->pRAMBank;
    pGBTemp->ulOpList[0xc] = (unsigned long)&pGBTemp->mem_map[0x2000]; // ordinary RAM at 0xC000
    pGBTemp->ulOpList[0xd] = (unsigned long)&pGBTemp->pWorkRAMBank[-0xd000]; // banked Work RAM
    pGBTemp->ulOpList[0xe] = (unsigned long)pGBTemp->mem_map; // echos RAM at 0xC000
    pGBTemp->ulOpList[0xf] = (unsigned long)pGBTemp->mem_map; // this area is tricky, but try this for now

    pGBTemp->iScrollX = pGBTemp->iScrollY = pGBTemp->iWindowX = pGBTemp->iWindowY = 0;

    pGBTemp->ucLCDC = 0x91; // initialize LCD controller to default value
   // DEBUG - b&w gameboy palette init neede
//    GBIOWrite(0xff47, 0xfc); // initialize palette registers
//    GBIOWrite(0xff48, 0xff); // initialize palette registers
//    GBIOWrite(0xff49, 0xff); // initialize palette registers
    pGBTemp->mem_map[0xff26] = 0xf1; // initialize sound to on
    pGBTemp->mem_map[0xff24] = 0x77; // set sound volume to max value (fixes frogger)
    pGBTemp->iScanLine = 0;
    pGBTemp->iScanLineCompare = 0;
    memcpy(&pGBTemp->mem_map[0xff30], ucInitWave, 16); // initial sound wave table values

    memset(pGBTemp->usPalData, 0xcd, 64*2);
//    memset(pFlags, 1, MAX_TILES); // mark all tiles as "dirty"
    memset(pGBTemp->pWorkRAM, 0, 0x8000);
    memset(pGBTemp->pVRAM, 0, 0x4000);
    memset(pGBTemp->pBankRAM, 0, pGBTemp->iRAMSize);
    // set the RTC to the current date and time
    memset(&pGBTemp->rtc, 0, sizeof(RTC)); // clear RTC info
    pGBTemp->rtc.ucStop = 1; // start with the clock halted
    SG_GetTime(&iDay, &iHour, &iMinute, &iSecond);
    pGBTemp->rtc.iDay = iDay;
    pGBTemp->rtc.iHour = iHour;
    pGBTemp->rtc.iMinute = iMinute;
    pGBTemp->rtc.iSecond = iSecond;
    ARESETGB(&pGBTemp->regs);
    
    pGBTemp->ucIRQFlags = 0;
    pGBTemp->ucIRQEnable = 0;

} /* GBReset() */

void GBGeniePatch(unsigned char *pszPatch)
{
unsigned char ucNew, ucOld;
uint32_t ulAddr, ulPatch;
int i;
   // convert the ASCII into a 8-digit hex value
   ulPatch = 0;
   for (i=0; i<8; i++)
      {
      if (i == 7)
         i = 8;
      ulPatch <<= 4;
      if (pszPatch[i] <= '9')
         ulPatch += pszPatch[i] - '0';
      else
         ulPatch += pszPatch[i] - 55;
      }
   ucNew = (unsigned char)(ulPatch >> 24); // first 2 digits are new value
   ulAddr = (ulPatch >> 8) & 0xffff;
   ulAddr = (ulAddr >> 4) | ((ulAddr & 0xf) << 12); // rotate right 4 bits
   ulAddr ^= 0xf000; // XOR with F000
   ucOld = (unsigned char)(ulPatch & 0xff); // value being replaced
   ucOld = (ucOld >> 2) | ((ucOld & 3) << 6); // rotate right 2 bits
   ucOld ^= 0xba;

   if (ulAddr < 0x2000)
      pGBMachine->mem_map[ulAddr] = ucNew; // if in lower bank, just store it
   else
      {
      i = 0;
      while (i <= pGBMachine->iROMSizeMask && pGBMachine->pBankROM[ulAddr] != ucOld)
         {
         ulAddr += 0x4000;
         i++;
         }
      if (i<=pGBMachine->iROMSizeMask) // found it, do the patch
         pGBMachine->pBankROM[ulAddr] = ucNew;
      }
} /* GBGeniePatch() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GBUpdateButtons(ULONG)                                     *
 *                                                                          *
 *  PURPOSE    : Update the gameboy button bits.                            *
 *                                                                          *
 ****************************************************************************/
void GBUpdateButtons(uint32_t ulKeys)
{
unsigned char c;

   if (ulKeys != pGBMachine->ulOldKeys[iPlayer])
      pGBMachine->ucIRQFlags |= GB_INT_BUTTONS;
   pGBMachine->ulOldKeys[iPlayer] = ulKeys;
   
   if (iPlayer == 0) // P1 controls
      {
      c = 0xcf;
      if (ulKeys & RKEY_RIGHT_P1)
         c ^= 1;
      if (ulKeys & RKEY_LEFT_P1)
         c ^= 2;
      if (ulKeys & RKEY_UP_P1)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P1)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P1)
         c ^= 1;
      if (ulKeys & RKEY_BUTTB_P1)
         c ^= 2;
      if (ulKeys & RKEY_SELECT_P1)
         c ^= 4;
      if (ulKeys & RKEY_START_P1)
         c ^= 8;
      pGBMachine->ucJoyButtons[0] = c;

      c = 0xdf;
      if (ulKeys & RKEY_BUTTA_P1)
         c ^= 1;
      if (ulKeys & RKEY_BUTTB_P1)
         c ^= 2;
      if (ulKeys & RKEY_SELECT_P1)
         c ^= 4;
      if (ulKeys & RKEY_START_P1)
         c ^= 8;
      pGBMachine->ucJoyButtons[1] = c;

      c = 0xef;
      if (ulKeys & RKEY_RIGHT_P1)
         c ^= 1;
      if (ulKeys & RKEY_LEFT_P1)
         c ^= 2;
      if (ulKeys & RKEY_UP_P1)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P1)
         c ^= 8;
      pGBMachine->ucJoyButtons[2] = c;
      }
   else // Player 2 controls
      {
      c = 0xcf;
      if (ulKeys & RKEY_RIGHT_P2)
         c ^= 1;
      if (ulKeys & RKEY_LEFT_P2)
         c ^= 2;
      if (ulKeys & RKEY_UP_P2)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P2)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P2)
         c ^= 1;
      if (ulKeys & RKEY_BUTTB_P2)
         c ^= 2;
      if (ulKeys & RKEY_SELECT_P2)
         c ^= 4;
      if (ulKeys & RKEY_START_P2)
         c ^= 8;
      pGBMachine->ucJoyButtons[0] = c;

      c = 0xdf;
      if (ulKeys & RKEY_BUTTA_P2)
         c ^= 1;
      if (ulKeys & RKEY_BUTTB_P2)
         c ^= 2;
      if (ulKeys & RKEY_SELECT_P2)
         c ^= 4;
      if (ulKeys & RKEY_START_P2)
         c ^= 8;
      pGBMachine->ucJoyButtons[1] = c;

      c = 0xef;
      if (ulKeys & RKEY_RIGHT_P2)
         c ^= 1;
      if (ulKeys & RKEY_LEFT_P2)
         c ^= 2;
      if (ulKeys & RKEY_UP_P2)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P2)
         c ^= 8;
      pGBMachine->ucJoyButtons[2] = c;
      }
} /* GBUpdateButtons() */

void RTCTick(void)
{
// Advance the RTC one frame
   if (pGBMachine->rtc.ucStop) return;
   if (++pGBMachine->rtc.iTick >= 60) // one second
      {
      pGBMachine->rtc.iTick = 0;
      if (++pGBMachine->rtc.iSecond >= 60) // one minute
         {
         pGBMachine->rtc.iSecond = 0;
         if (++pGBMachine->rtc.iMinute >= 60) // one hour
            {
            pGBMachine->rtc.iMinute = 0;
            if (++pGBMachine->rtc.iHour >= 24) // one day
               {
               pGBMachine->rtc.iHour = 0;
               if (++pGBMachine->rtc.iDay >= 512) // rolled over
                  {
                  pGBMachine->rtc.ucCarry = 1;
                  pGBMachine->rtc.iDay = 0;
                  }
               }
            }
         }
      }
} /* RTCTick() */

void GBCCreateVideoBuffer(void)
{
    if (bHead2Head)
       {
       if (iDisplayWidth > iDisplayHeight) // make it double wide
          {
          iScreenX = 160*2;
          iScreenY = 144;
          }
       else // make it double high
          {
          iScreenX = 160;
          iScreenY = 144*2; // double height to fit the 2 games
          }
       }
    EMUCreateVideoBuffer(iScreenX, iScreenY, 16, &pBitmap);
    iVideoSize = -1; // force a recalc of the display size
} /* GBCCreateVideoBuffer() */

void GBProcess1Scan(GAME_BLOB *pBlob, BOOL bUseSound, BOOL bVideo)
{
int i;

 // 4.19Mhz or 8.38Mhz (456 total clocks * clocks speed)
         if (pGBMachine->ucLCDC & 0x80)
            {
//            if (pGBMachine->iScanLine == 0)
//               {
//               if (!bSkipFrame && iPlayer == 0)
//                  {
//                  EMUScreenUpdate(iScreenX, iScreenY);
//                  }
//               }
            pGBMachine->ucLCDSTAT &= 0x78;
            if (pGBMachine->iScanLine == pGBMachine->iScanLineCompare)
               {
               pGBMachine->ucLCDSTAT |= 4; // when scan line = compare, set bit 2
               if (pGBMachine->ucLCDSTAT & 0x40)
                  pGBMachine->ucIRQFlags |= GB_INT_LCDSTAT;
               }
            if (pGBMachine->iScanLine >= 144)
               {
               pGBMachine->ucLCDSTAT |= 1; // Vblank status = mode 1
               if (pGBMachine->iScanLine == 144)
                  {
                  AEXECGB(&pGBMachine->regs, 80);
                  pGBMachine->ucIRQFlags |= GB_INT_VBLANK; // set up a pending interrupt for vblank
                  if (pGBMachine->ucLCDSTAT & 0x10) // LCD status says to interrupt on vblank also
                     pGBMachine->ucIRQFlags |= GB_INT_LCDSTAT;
                  AEXECGB(&pGBMachine->regs, 456-80);
                  }
               else
                  if (pGBMachine->iScanLine == 154)
                     {
                     AEXECGB(&pGBMachine->regs, 80);
                     pGBMachine->iScanLine = 0;
                     AEXECGB(&pGBMachine->regs, 456-80);
                     pGBMachine->iScanLine = 154;
                     }
                  else
                     { // check for GDMA which halts the CPU
                     if (pGBMachine->iDMATime < 456)
                        {
                        i = 456 - pGBMachine->iDMATime;
                        pGBMachine->iDMATime = 0;
                        AEXECGB(&pGBMachine->regs, i);
                        }
                     else
                        pGBMachine->iDMATime -= 456; // CPU is halted, don't execute this scanline
                     }
               }
            else // not in vblank
               {
               pGBMachine->ucLCDSTAT |= 2; // mode 2
               if (pGBMachine->ucLCDSTAT & 0x20) // mode 2 interrupt enable
                  pGBMachine->ucIRQFlags |= GB_INT_LCDSTAT;
//               GBDrawScanline();
               AEXECGB(&pGBMachine->regs, 80);
               pGBMachine->ucLCDSTAT |= 3; // mode 3
               AEXECGB(&pGBMachine->regs, 169);
               if (bVideo)
                  GBDrawScanline();
               if (pGBMachine->bInDMA) // if doing HDMA
                  {
                  memcpy(pGBMachine->pDMADest, pGBMachine->pDMASrc, 0x10);
//                  pFlags[((int)pGBMachine->pDMADest & 0x3ff0) >> 4] = 1; // tile number affected
                  pGBMachine->iDMASrc += 0x10;
                  pGBMachine->iDMADest += 0x10;
                  pGBMachine->iDMALen -= 0x10;
                  pGBMachine->pDMADest += 0x10;
                  pGBMachine->pDMASrc += 0x10;
                  if (pGBMachine->iDMALen == 0)
                     pGBMachine->bInDMA = FALSE;
                  }
               pGBMachine->ucLCDSTAT &= 0xfc; // mode 0
               if (pGBMachine->ucLCDSTAT & 0x8) // mode 0 (hblank) interrupt enable
                  pGBMachine->ucIRQFlags |= GB_INT_LCDSTAT;
//               pGBMachine->iScanLine++; // since we are in HBLANK changes affect the next line
               AEXECGB(&pGBMachine->regs, 207);
//               pGBMachine->iScanLine--;
               }
            pGBMachine->iScanLine++;
            if (pGBMachine->iScanLine == 155)
               pGBMachine->iScanLine = 0;
            } // if LCD controller ON
         else
            { // LCD controller OFF
//            AEXECGB(&pGBMachine->regs, 456);
            pGBMachine->ucLCDSTAT |= 2; // mode 2
            AEXECGB(&pGBMachine->regs, 80);
            pGBMachine->ucLCDSTAT |= 3; // mode 3
            AEXECGB(&pGBMachine->regs, 169);
            pGBMachine->ucLCDSTAT &= 0xfc; // mode 0
            AEXECGB(&pGBMachine->regs, 207);
            pGBMachine->ucLCDSTAT &= 0xF8;
            if (pGBMachine->iScanLine == pGBMachine->iScanLineCompare)
               pGBMachine->ucLCDSTAT |= 4; // when scan line = compare, set bit 2
            pGBMachine->iScanLine++;
            if (pGBMachine->iScanLine >= 155)
               pGBMachine->iScanLine = 0;
            }
         if (iSendData < 10) // a data byte is being transmitted
            {
            if (iSendData == 0) // finished sending
               {
               unsigned char c;
               iSendData = 10; // disable
               pGBMachine->ucIRQFlags |= GB_INT_SERIAL; // cause an IRQ to indicate the data has been sent
               pGBMachine->mem_map[0xff02] &= 0x3; // clear the busy bit
               if (bHead2Head && gbmachine[1-iPlayer].mem_map[0xff02] & 0x80) // there is data waiting from receiver
                  {
                  c = pGBMachine->ucData; // Swap the data bytes from the sending end
                  pGBMachine->ucData = gbmachine[1-iPlayer].ucData;
                  gbmachine[1-iPlayer].ucData = c;
                  gbmachine[1-iPlayer].mem_map[0xff02] &= 0x3; // signal data complete
                  gbmachine[1-iPlayer].ucIRQFlags |= GB_INT_SERIAL; // receiver gets an IRQ also
                  }
               else
                  pGBMachine->ucData = 0xff; // nothing connected
               if (/*bNetworked && */pGBMachine->ucNetStatus & 0x80) // we received data from another machine
                  {
                  pGBMachine->ucData = pGBMachine->ucNetData; // get the byte received over the network
                  }
               }
            else
               iSendData--; // 9 scanlines is almost the exact amount of time (1ms) to transmit a character
            }
         if (bUseSound && iPlayer == 0)
            {
            if (iLoopCount == 0 || iLoopCount == 77) // generate sound chunks 120 times per second
               {
               GBGenSamples((unsigned char *)&pBlob->pAudioBuf[iSoundLen], (pBlob->iSampleCount>>1));
               iSoundLen += (pBlob->iAudioBlockSize>>2);
               }
            }
 // Check if data is available from the link cable
         if ((pGBMachine->mem_map[0xff02] & 0x81) == 0x80 && pGBMachine->ucStatus == 0x81) // we are waiting to receive and data is available from a sender
            {
            // the data has already been swapped by the other side during the send
            pGBMachine->ucStatus = 0; // clear the "byte waiting" flag
            pGBMachine->mem_map[0xff02] &= 0x3; // clear the busy bit
            pGBMachine->ucIRQFlags |= GB_INT_SERIAL; // cause an IRQ to indicate the data has been received
            }
} /* GBProcess1Scan() */

// Receive a byte from a virtual Link cable
void GBNetData(unsigned char *pData)
{
   if (pGBMachine) // the game is running
      {
      pGBMachine->ucNetStatus = pData[0]; // save the link cable status
      pGBMachine->ucNetData = pData[1]; // and data byte
      // we have written our data and are waiting to receive
      // and the other side is providing the clock
      if ((pGBMachine->mem_map[0xff02] & 0x80) == 0x80 && (pGBMachine->ucNetStatus & 1))
         { // "receive" it
         pGBMachine->ucData = pData[1];
         pGBMachine->mem_map[0xff02] &= 0x3; // clear the busy bit
         pGBMachine->ucNetStatus = 0; // make sure this is clear
         pGBMachine->ucIRQFlags |= GB_INT_SERIAL; // cause an IRQ to indicate the data has been received
         }
      }
   // if we're using the internal clock, then wait for the 
   // proper time and get the data from ucNetData later
} /* GBNetData() */

void GBProcess1Frame(GAME_BLOB *pBlob, uint32_t ulKeys, BOOL bUseSound, BOOL bVideo)
{
      GBUpdateButtons(ulKeys);
      RTCTick(); // increment real-time clock
      if (pBlob->bHead2Head) // update P2 button info
         {
         iPlayer = 1;
         pGBMachine = &gbmachine[iPlayer];
         GBUpdateButtons(ulKeys);
         RTCTick(); // increment real-time clock
         iPlayer = 0;
         pGBMachine = &gbmachine[iPlayer];
         }
      iSoundLen = 0;
      for (iLoopCount=0; iLoopCount<155; iLoopCount++)
         {
         GBProcess1Scan(pBlob, bUseSound, bVideo);
         if (pBlob->bHead2Head) // run a second GBC emu
            {
//            if (iDisplayWidth > iDisplayHeight) // make it double wide
               pLocalBitmap += 160*2; // use right half
 //           else
 //              pBitmap += (144 * iPitch); // point to second display
            iPlayer = 1;
            pGBMachine = &gbmachine[iPlayer];
            GBProcess1Scan(pBlob, bUseSound, bVideo);
            iPlayer = 0;      
            pGBMachine = &gbmachine[iPlayer];
//            if (iDisplayWidth > iDisplayHeight) // make it double wide
               pLocalBitmap -= 160*2; // back to left half
//            else
//               pBitmap -= (144 * iPitch); // point to first display
            }
         } // for each scanline
      if (bUseSound)
         {
         GBGenWaveSamples((unsigned char *)pBlob->pAudioBuf, pBlob->iSampleCount);
//            EMUDoSound();
//            if (iSoundBlocks < 5) // need to buffer more so we don't fall behind
//               {
//               GBGenSamples(&pSoundBuf[(iSoundHead*iSoundBlock)], iSoundBlock/iSoundChannels);
//               GBGenWaveSamples(&pSoundBuf[iSoundHead*iSoundBlock], iSoundBlock/iSoundChannels);
//               EMUDoSound();
//               }
//            }
         }
      else
         iWaveQ = 0; // clear queued wave samples
} /* GBProcess1Frame() */

void TRACEGB(int iTicks)
{
#ifdef FUTURE
char c[16];

//   if (iTrace == 0x2610)
//      c[0] = 0;
      if (pGBMachine->regs.usRegPC == 0x612c) // && iTicks == 0xa9)
         c[0] = 0;
   if (bTrace)
      {
      c[0] = (char)(pGBMachine->regs.usRegPC & 0xff);
      c[1] = (char)(pGBMachine->regs.usRegPC >> 8);
      c[2] = pGBMachine->regs.ucRegF;
      c[3] = pGBMachine->regs.ucRegA;
      c[4] = pGBMachine->regs.ucRegC;
      c[5] = pGBMachine->regs.ucRegB;
      c[6] = pGBMachine->regs.ucRegE;
      c[7] = pGBMachine->regs.ucRegD;
      c[8] = (char)pGBMachine->regs.usRegHL;
      c[9] = (char)(pGBMachine->regs.usRegHL >> 8);
//      c[8] = regs.ucRegL;
//      c[9] = regs.ucRegH;
      c[10] = (char)pGBMachine->regs.usRegSP;
      c[11] = (char)(pGBMachine->regs.usRegSP >> 8);
      c[12] = pGBMachine->regs.ucRegI;
      c[13] = 0;
      c[14] = 0;
//      c[14] = (char)iTicks & 0xff;
//      c[15] = (char)(iTicks >> 8);
      c[15] = pGBMachine->mem_map[0xcffb];
 //     EMUWrite(ohandle, c, 16);
 //     iTrace++;
      }
#endif // FUTURE
}

void GB_PostLoad(GAME_BLOB *pBlob)
{
GBMACHINE *pOld, *pNew;

   pOld = (GBMACHINE *)pBlob->OriginalMachine;
   pNew = (GBMACHINE *)pBlob->pMachine;
   pNew->pBankROM = pOld->pBankROM;
   pNew->pVRAM = pOld->pVRAM;
   pNew->pBankRAM = pOld->pBankRAM;
   pNew->pWorkRAM = pOld->pWorkRAM;
   pNew->mem_map = pOld->mem_map;
   pNew->usPalConvert = pOld->usPalConvert;
   pNew->usPalData = pOld->usPalData;
   pNew->ucPalData = pOld->ucPalData;
   pNew->p00FFOps = pOld->p00FFOps;
   pNew->pCBXXOps = pOld->pCBXXOps;

   // Force a re-load of the palette conversion table
   pNew->ulPalChanged = 0xffffffff;
   pNew->ulSpritePalChanged = 0xffffffff;

// Restore the banked memory pointers
   pNew->pBank = &pNew->pBankROM[(pNew->iROMBank-1)*0x4000];
   pNew->pRAMBank = &pNew->pBankRAM[(pNew->iRAMBank-5)*0x2000];
   pNew->pWorkRAMBank = &pNew->pWorkRAM[pNew->iWorkRAMBank * 0x1000];
   pNew->pVRAMBank = &pNew->pVRAM[-0x8000 + pNew->iVRAMBank];

   // update the opcode lookup table with the new bank info
   pNew->ulOpList[0x0] = (unsigned long)pNew->pBankROM; // first 16K is non-banked ROM
   pNew->ulOpList[0x1] = (unsigned long)pNew->pBankROM;
   pNew->ulOpList[0x2] = (unsigned long)pNew->pBankROM;
   pNew->ulOpList[0x3] = (unsigned long)pNew->pBankROM;
   pNew->ulOpList[0x4] = (unsigned long)pNew->pBank;
   pNew->ulOpList[0x5] = (unsigned long)pNew->pBank;
   pNew->ulOpList[0x6] = (unsigned long)pNew->pBank;
   pNew->ulOpList[0x7] = (unsigned long)pNew->pBank;
   pNew->ulOpList[0x8] = (unsigned long)pNew->pVRAMBank;
   pNew->ulOpList[0x9] = (unsigned long)pNew->pVRAMBank;
   pNew->ulOpList[0xa] = (unsigned long)pNew->pRAMBank; // Banked RAM at 0xA000-0xB000
   pNew->ulOpList[0xb] = (unsigned long)pNew->pRAMBank;
   pNew->ulOpList[0xc] = (unsigned long)&pNew->mem_map[0x2000]; // ordinary RAM at 0xC000
   pNew->ulOpList[0xd] = (unsigned long)&pNew->pWorkRAMBank[-0xd000]; // banked Work RAM
   pNew->ulOpList[0xe] = (unsigned long)pNew->mem_map; // echos RAM at 0xC000
   pNew->ulOpList[0xf] = (unsigned long)pNew->mem_map; // this area is tricky, but try this for now

   pNew->pEMUH = pOld->pEMUH; // restore handler addresses

// Restore the DMA settings
   pNew->pDMADest = &pNew->pVRAM[(pNew->iDMADest & 0x1ff0)+pNew->iVRAMBank];
	switch(pNew->iDMASrc>>13) // different memory regions need different attention
      {
		case 0:
		case 1:
			pNew->pDMASrc = &pNew->pBankROM[pNew->iDMASrc];
			break;
		case 2:
		case 3:
			pNew->pDMASrc = &pNew->pBank[pNew->iDMASrc];
			break;
		case 5:
			pNew->pDMASrc = &pNew->pRAMBank[pNew->iDMASrc];
			break;
		case 6:
         if (pNew->iDMASrc & 0x1000) // work RAM (0xd000)
            pNew->pDMASrc = &pNew->pWorkRAMBank[pNew->iDMASrc & 0xfff];
         else
				pNew->pDMASrc = &pNew->mem_map[pNew->iDMASrc+0x2000];
			break;
		case 7:
			pNew->pDMASrc = &pNew->mem_map[pNew->iDMASrc];
			break;
		}

} /* GB_PostLoad() */

// Load a game, game state or reset the machine
// If pszROM is NULL and iGameLoad == -1, reset
// If pszROM is NULL and iGameLoad != -1, load a game state
// otherwise, load a new game from file
int GB_Init(GAME_BLOB *pBlob, char *pszROM, int iGameLoad)
{
int i;
int iError = SG_NO_ERROR;
unsigned char *pBankROM;
GBMACHINE *pGBTemp;

   pBankROM = NULL;
//g_print("entering GB_Init(), iGameLoad = %d\n", iGameLoad);
   if (pszROM == NULL)
      {
      if (iGameLoad == -1) // reset machine
         {
         GBReset((GBMACHINE *)&gbmachine[0]);
         if (bHead2Head)
             GBReset((GBMACHINE *)&gbmachine[1]);
         }
      else // load a game state
         {
         if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
             GB_PostLoad(pBlob);
         }
      return SG_NO_ERROR;
      }
   if (bHead2Head)
   {
//           g_print("GBC - Head2Head play enabled; display width = 320");
	   pBlob->iWidth = 160*2; // side-by-side p1/p2
	   pBlob->iHeight = 144;
   }
   else
   {
	   pBlob->iWidth = 160;
	   pBlob->iHeight = 144;
   }
   pBlob->iGameType = SYS_GAMEBOY;

//    g_print("About to try to load the ROM\n");
    if (EMULoadGBRoms(pszROM, &pBankROM))
       return SG_OUTOFMEMORY;
//    g_print("ROM loaded successfully\n");
//    pGBTemp = EMUAlloc(sizeof(GBMACHINE)); // local machine
    pGBTemp = &gbmachine[0];
    if (pGBTemp == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
    gbmachine[0].pBankROM = gbmachine[1].pBankROM = pBankROM;
    pBlob->pMachine = (void *)pGBTemp;
    pBlob->OriginalMachine = (void *)EMUAlloc(sizeof(GBMACHINE)); // another copy used for restoring game states
    gbmachine[0].iRAMSize = gbmachine[1].iRAMSize = iGBCartRAMSize[pBankROM[0x149] & 7];
    pGBTemp->mem_map = (unsigned char *)EMUAlloc(0x2000)-0xe000; /* Allocate 8k for the GB memory map */
    if (bHead2Head)
        gbmachine[1].mem_map = (unsigned char *)EMUAlloc(0x2000)-0xe000; /* Allocate 8k for the GB memory map */
    if ((unsigned long)pGBTemp->mem_map == -0xe000)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
    pGBTemp->pVRAM = EMUAlloc(0x4300);
    if (bHead2Head)
        gbmachine[1].pVRAM = EMUAlloc(0x4300);
    if (pGBTemp->pVRAM == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
    pGBTemp->usPalData = (unsigned short *)&pGBTemp->pVRAM[0x4000];
    pGBTemp->usPalConvert = (unsigned short *)&pGBTemp->pVRAM[0x4100];
    pGBTemp->ucPalData = (unsigned char *)&pGBTemp->usPalData[0];
    if (bHead2Head)
    {
        gbmachine[1].usPalData = (unsigned short *)&gbmachine[1].pVRAM[0x4000];
        gbmachine[1].usPalConvert = (unsigned short *)&gbmachine[1].pVRAM[0x4100];
        gbmachine[1].ucPalData = (unsigned char *)&gbmachine[1].usPalData[0];
    }
    pGBTemp->pBankRAM = EMUAlloc(pGBTemp->iRAMSize+64); // up to 128k of banked RAM in the cartridge
    if (pGBTemp->pBankRAM == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
    memset(pGBTemp->pBankRAM, 0xff, pGBTemp->iRAMSize);
    if (bHead2Head)
        {
    	gbmachine[1].pBankRAM = EMUAlloc(pGBTemp->iRAMSize+64);
        memset(gbmachine[1].pBankRAM, 0xff, pGBTemp->iRAMSize);
        }
    pGBTemp->pWorkRAM = EMUAlloc(0x8000); // work RAM at 0xd000
    if (bHead2Head)
    	gbmachine[1].pWorkRAM = EMUAlloc(0x8000);
    if (pGBTemp->pWorkRAM == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
// Set up the memory area pointers for save/load and networking
    pBlob->mem_areas[0].pPrimaryArea = &pGBTemp->mem_map[0xe000];
    pBlob->mem_areas[0].iAreaLength = 0x2000;
    pBlob->mem_areas[1].pPrimaryArea = pGBTemp->pVRAM;
    pBlob->mem_areas[1].iAreaLength = 0x4300;
    pBlob->mem_areas[2].pPrimaryArea = pGBTemp->pBankRAM;
    pBlob->mem_areas[2].iAreaLength = pGBTemp->iRAMSize;
    pBlob->mem_areas[3].pPrimaryArea = pGBTemp->pWorkRAM;
    pBlob->mem_areas[3].iAreaLength = 0x8000;
    pBlob->mem_areas[4].pPrimaryArea = (unsigned char *)pGBTemp;
    pBlob->mem_areas[4].iAreaLength = sizeof(GBMACHINE);
    pBlob->mem_areas[5].iAreaLength = 0; // end of list
    pGBTemp->pEMUH = EMUAlloc(16*sizeof(EMUHANDLERS));
    if (pGBTemp->pEMUH == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gb_init_leave;
       }
    pGBTemp->pEMUH[0].pfn_read = pGBTemp->pEMUH[1].pfn_read = pGBTemp->pEMUH[2].pfn_read = pGBTemp->pEMUH[3].pfn_read = NULL; // no read routine needed
    pGBTemp->pEMUH[0].pfn_write = pGBTemp->pEMUH[1].pfn_write = pGBTemp->pEMUH[2].pfn_write = pGBTemp->pEMUH[3].pfn_write = GBROMBankWrite;
    pGBTemp->pEMUH[4].pfn_read = pGBTemp->pEMUH[5].pfn_read = pGBTemp->pEMUH[6].pfn_read = pGBTemp->pEMUH[7].pfn_read = NULL; // no read routine needed
    pGBTemp->pEMUH[4].pfn_write = pGBTemp->pEMUH[5].pfn_write = pGBTemp->pEMUH[6].pfn_write = pGBTemp->pEMUH[7].pfn_write = GBROMBankWrite;
    pGBTemp->pEMUH[8].pfn_read = pGBTemp->pEMUH[9].pfn_read = NULL; // no read routine needed for VRAM
    pGBTemp->pEMUH[8].pfn_write = pGBTemp->pEMUH[9].pfn_write = NULL; //GBVRAMWrite; // write routine needed for VRAM to check for tile changes
    pGBTemp->pEMUH[0xa].pfn_read = pGBTemp->pEMUH[0xb].pfn_read = GBRAMBankRead;
    pGBTemp->pEMUH[0xa].pfn_write = pGBTemp->pEMUH[0xb].pfn_write = GBRAMBankWrite;
    pGBTemp->pEMUH[0xc].pfn_read = NULL; // no read routine needed
    pGBTemp->pEMUH[0xc].pfn_write = NULL; // no write routine needed for RAM
    pGBTemp->pEMUH[0xd].pfn_read = NULL; // no read routine needed for Work RAM
    pGBTemp->pEMUH[0xd].pfn_write = NULL; // no write routine needed for Work RAM
    pGBTemp->pEMUH[0xe].pfn_read = NULL; // no read routine needed
    pGBTemp->pEMUH[0xe].pfn_write = NULL; // no write routine neede for Echo RAM
    pGBTemp->pEMUH[0xf].pfn_read = GBIORead;
    pGBTemp->pEMUH[0xf].pfn_write = GBIOWrite;
    gbmachine[1].pEMUH = gbmachine[0].pEMUH;
    if (iGBRefCount == 0) // not allocated yet
       {
       pWaveQueue = EMUAlloc(8192 * sizeof (WAVEQUEUE)); // up to 8k entries per frame
       if (pWaveQueue == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gb_init_leave;
          }
       pNoise7 = EMUAlloc(128);
       if (pNoise7 == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gb_init_leave;
          }
       pNoise15 = EMUAlloc(32768);
       if (pNoise15 == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gb_init_leave;
          }
       // Each handler is for a 4K slice of memory
       // Pre-calculate attribute bits to color info
       ulAttrColor = EMUAlloc(256*sizeof(long));
       if (ulAttrColor == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gb_init_leave;
          }
       for (i=0; i<256; i++)
          {
          uint32_t ulColor;

          ulColor = ((i & 7) << 2) | (i & 0x80);
          ulColor |= (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
          ulAttrColor[i] = ulColor;
          }
       GBGenNoise(); // generate pre-calculated polynomial noise
       }
    // Setup the flag map for quicker memory reads/writes
    pGBTemp->ulRWFlags = 0;
    for (i=0; i<16; i++)
       {
       if (pGBTemp->pEMUH[i].pfn_read)
          pGBTemp->ulRWFlags |= (1<<i);
       if (pGBTemp->pEMUH[i].pfn_write)
          pGBTemp->ulRWFlags |= (1<<(16+i));
       }
    gbmachine[1].ulRWFlags = gbmachine[0].ulRWFlags;

//#ifdef UNICODE
//    mbstowcs(pBlob->szGameName, &pBankROM[0x134], 11);
//#else
//    memcpy(pBlob->szGameName, &pBankROM[0x134],11); // get the game name
//#endif
//    pBlob->szGameName[11] = '\0'; // apparently name was truncated to 11
    SG_GetLeafName((char *)pszROM, (char *)pBlob->szGameName); // derive the game name from the ROM name
    pGBTemp->bCGB = pBankROM[0x143] & 0x80; // color gameboy flag
    gbmachine[1].bCGB = gbmachine[0].bCGB;

    i = pBankROM[0x148]; // ROM size;
    if (i >= 0x52)
      i = i - 0x52 + 7;
    pGBTemp->iROMSizeMask = iROMMasks[i]; // keep ROM bank from being set too high
    pGBTemp->iRAMSizeMask = iRAMMasks[pBankROM[0x149]];
    pGBTemp->bRTC = FALSE; // assume no RTC
    gbmachine[1].iROMSizeMask = gbmachine[0].iROMSizeMask;
    iPlayer = 0;
    pGBMachine = &gbmachine[iPlayer];

    switch (pBankROM[0x147]) // determine the memory controller type from the cartridge type
      {
      case 0:
      case 8:
      case 9:
         pGBTemp->iMBCType = 0; // none
         break;
      case 1:
      case 2:
      case 3:
         pGBTemp->iMBCType = 1;
         break;
      case 5:
      case 6:
         pGBTemp->iMBCType = 2;
         break;
      case 0xb:
      case 0xc:
      case 0xd:
         pGBTemp->iMBCType = 6;
         break;
      case 0x0f:
      case 0x10:
         pGBTemp->bRTC = TRUE; // real-time clock is present
         pGBTemp->iRAMSizeMask = 0xf; // allow RTC register access
         pGBTemp->iMBCType = 3;
         pGBTemp->pEMUH[0xa].pfn_read = pGBTemp->pEMUH[0xb].pfn_read = GBRAMBankRead2; // use special routine to deal with RTC
         pGBTemp->pEMUH[0xa].pfn_write = pGBTemp->pEMUH[0xb].pfn_write = GBRAMBankWrite2;
         break;
      case 0x11:
      case 0x12:
      case 0x13:
         pGBTemp->iMBCType = 3;
         break;
      case 0x19:
      case 0x1a:
      case 0x1b:
      case 0x1c:
      case 0x1d:
      case 0x1e:
         pGBTemp->iMBCType = 5;
         break;
      case 0xff:
         pGBTemp->iMBCType = 6; // HUC1
         break;
      case 0x22:
         pGBTemp->iMBCType = 7; // HUC3
         break;
      default:
         pGBTemp->iMBCType = 0; // assume none
         break;
      }
   gbmachine[1].iMBCType = gbmachine[0].iMBCType;
   gbmachine[1].iRAMSizeMask = gbmachine[0].iRAMSizeMask;
   gbmachine[1].bRTC = gbmachine[0].bRTC;

   GBReset(pGBTemp); // Reset the state of the gameboy emulator
   if (bHead2Head)
	   GBReset(&gbmachine[1]);
   memcpy(pBlob->OriginalMachine, pGBTemp, sizeof(GBMACHINE)); // keep a copy of the original machine pointers to allow for game state loading
   if (iGameLoad >= 0)
      {
      if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
         GB_PostLoad(pBlob);
      }
//   _tcscpy(szGameName, pBlob->szGameName);
   if (!EMULoadBRAM(pBlob, (char *)pGBTemp->pBankRAM, pGBTemp->iRAMSize+64,(char *)TEXT("GBC"))) // try to load battery backed up cartridge RAM
   {  // restore RTC registers too
      memcpy(&pGBTemp->rtc, &pGBTemp->pBankRAM[pGBTemp->iRAMSize], sizeof(pGBTemp->rtc));
//      pGBTemp->rtc.ucStop = 1; // at power up, clock is halted
      pGBTemp->rtc.ucRegs[4] &= 1;
      // restore the correct time to show that time has passed since the last play time
      SG_GetTime(&pGBTemp->rtc.iDay, &pGBTemp->rtc.iHour, &pGBTemp->rtc.iMinute, &pGBTemp->rtc.iSecond);
   }
   
   pGBTemp->iDMATime = 0; // no time HALTed doing DMA yet
   gbmachine[1].iDMATime = 0;

   iWaveQ = 0;
   pBlob->bLoaded = TRUE;

   iGBRefCount++;

gb_init_leave:
   if (iError != SG_NO_ERROR) // try to free everything
      {
      EMUFree(pGBTemp->pBankROM);
      EMUFree(&pGBTemp->mem_map[0xe000]);
      EMUFree(pGBTemp->pVRAM);
      EMUFree(pGBTemp->pBankRAM);
      EMUFree(pGBTemp->pWorkRAM);
      EMUFree(pGBTemp->pEMUH);
      EMUFree(pGBTemp); // free the whole structure
      if (iGBRefCount == 0) // free common areas too
         {
         EMUFree(pWaveQueue);
         EMUFree(pNoise7);
         EMUFree(pNoise15);
         EMUFree(ulAttrColor);
         }
      EMUFree(pBlob->OriginalMachine);
      }
   return iError;
} /* GB_Init() */

void GB_Terminate(GAME_BLOB *pBlob)
{
   if (iGBRefCount == 0) // already released
      return;

   EMUFree(pBlob->OriginalMachine); // free the saved state
   pBlob->bLoaded = FALSE;
//   pGBMachine = (GBMACHINE *)pBlob->pMachine;
   pGBMachine = &gbmachine[0];
//   _tcscpy(szGameName, pBlob->szGameName);
   // save RTC registers too
   memcpy(&pGBMachine->pBankRAM[pGBMachine->iRAMSize], &pGBMachine->rtc, sizeof(pGBMachine->rtc));
   EMUSaveBRAM(pBlob, (char *)pGBMachine->pBankRAM, pGBMachine->iRAMSize+64,(char *)TEXT("GBC")); // try to save cartridge RAM
   // Free everything 
   EMUFree(pGBMachine->pBankROM);
   EMUFree(&pGBMachine->mem_map[0xe000]);
   EMUFree(pGBMachine->pVRAM);
   EMUFree(pGBMachine->pBankRAM);
   EMUFree(pGBMachine->pWorkRAM);
   EMUFree(pGBMachine->pEMUH);
   if (bHead2Head)
   {
	   EMUFree(&gbmachine[1].mem_map[0xe000]); // free player 2 private memory areas
	   EMUFree(gbmachine[1].pVRAM);
	   EMUFree(gbmachine[1].pBankRAM);
	   EMUFree(gbmachine[1].pWorkRAM);
   }
//   EMUFree(pGBMachine); // free the whole structure
   if (iGBRefCount == 1) // the last one to be freed
      {
      EMUFree(pWaveQueue);
      EMUFree(pNoise7);
      EMUFree(pNoise15);
      EMUFree(ulAttrColor);
      }
   iGBRefCount--;
} /* GB_Terminate() */

void GB_Play(GAME_BLOB *pBlob, BOOL bSound, BOOL bVideo, uint32_t ulKeys)
{
int i;
unsigned char *p;

   if (pBlob->bRewound)
   {
       pBlob->bRewound = FALSE;
       GB_PostLoad(pBlob);
   }
   pGBMachine = (GBMACHINE *)pBlob->pMachine;
   // Implement cheats
   for (i=0; i<iCheatCount; i++)
   {
	   p = (unsigned char *)(pGBMachine->ulOpList[iaCheatAddr[i] >> 12] + iaCheatAddr[i]);
	   if (iaCheatCompare[i] != -1)
	   {
		   if (p[0] == (unsigned char)iaCheatCompare[i])
			   p[0] = (unsigned char)iaCheatValue[i];
	   }
	   else
		   p[0] = (unsigned char)iaCheatValue[i];
   }

   pLocalBitmap = (unsigned char *)pBlob->pBitmap;
   iPitch = pBlob->iPitch;
   GBProcess1Frame(pBlob, ulKeys, bSound, bVideo);

} /* GB_Play() */

