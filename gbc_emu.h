//
// GameBoy emulator structures and definitions
// Copyright (c) 2002-2017 BitBank Software, Inc.
//
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

typedef struct tagRTC
{
int iDay, iHour, iMinute, iSecond, iTick;
unsigned char ucLatch, ucStop, ucCarry, ucRegs[8];
} RTC;

typedef struct tagGBMachine
{
BOOL bInDMA;
BOOL bRTC;
BOOL bCGB;
BOOL bSpriteChanged;
BOOL bRAMEnable;
BOOL bBackIncrement;
BOOL bObjectIncrement; // palette auto-inc flag
BOOL bSound1On, bSound2On, bSound3On, bSound4On;
uint32_t ulOldKeys[2];
uint32_t ulPalChanged;
uint32_t ulSpritePalChanged;
signed int iScrollX;
signed int iScrollY;
signed int iWindowX;
signed int iWindowY;
int iROMSizeMask;
int iRAMSizeMask;
int iMBCType;
int iVRAMBank;
int iWorkRAMBank;
int iDMATime;
int iROMBank;
int iRAMBank;
int iRAMSize;
int iDMASrc;
int iDMADest;
int iDMALen;
int iDivider;
int iFreq1Start;
int iFreq1;
int iFreq2;
int iWaveFreq;
int iWavePos;
int iSnd2Pos;
int iSnd2Delta;
int iSnd2EnvPos;
int iSnd2Vol;
int iSnd2Len;
int iWaveLen;
int iWaveStartLen;
int iSweepPos;
int iSweepDelta;
int iSnd1Len;
int iSnd1Pos;
int iSnd1EnvPos;
int iSnd1Vol;
int iSnd1Delta;
int iSnd1StartLen;
int iSnd2StartLen;
int iSnd1StartVol;
int iSnd2StartVol;
int iNoiseStartVol;
int iNoiseStartLen;
int iNoiseFreq;
int iNoiseVol;
int iNoisePos;
int iNoiseDelta;
int iNoiseLen;
int iNoiseEnvPos;
int iOldROM;
int iOldRAM;
int iScanLine;
int iScanLineCompare;
int iWindowUsed;
unsigned char ucSpriteDraw[144]; // flags for each scanline if sprites present
unsigned char ucSpriteList[40]; // list of active sprites
int iSpriteCount; // number of active sprites
unsigned char ucJoyButtons[4]; // current joystick bits
unsigned char ucSamples[32]; // sound samples
unsigned char *pBank; // current ROM bank
unsigned char *pDMASrc;
unsigned char *pDMADest;
unsigned char  *pRAMBank;
unsigned char *pVRAMBank;
unsigned char *pVRAM;
unsigned char *ucPalData;
unsigned short *usPalData;
unsigned short *usPalConvert;
unsigned char *pBankRAM;
unsigned char *pWorkRAM;
unsigned char *pBankROM;
RTC rtc;
// the following are arranged to be in close proximity to the regs struct
// for easy access in the GB_CPU emulator
// DO NOT CHANGE THE ORDER
void *p00FFOps; // 00-FF instruction jump table
void *pCBXXOps; // CB00-CBFF instruction jump table
int iEITicks; // temp var for gb cpu emulator
EMUHANDLERS *pEMUH;  // emulation handlers
unsigned char *mem_map;
unsigned char ucTimerControl;
unsigned char ucIRQEnable;
unsigned char ucJoySelect;
unsigned char ucLCDC;
BOOL bTimerOn;
int iTimerMask;
int iTimerLimit;
unsigned char ucIRQFlags;
unsigned char ucTimerModulo;
unsigned char ucLCDSTAT;
unsigned char ucTimerCounter;
BOOL bSpeedChange;
int iMasterClock;
int iCPUSpeed;
unsigned long ulOpList[16]; // offset to each of the 16 memory areas
unsigned char *pWorkRAMBank;
uint32_t ulTemp;
REGSGB regs;
uint32_t ulRWFlags;
unsigned char ucObjectIndex;
unsigned char ucBackIndex; // palette index
unsigned char ucMemoryModel;
unsigned char ucHUC1;
unsigned char ucStatus; // link cable status
unsigned char ucData; //  link cable data
unsigned char ucNetData; // data coming from across the nextwork
unsigned char ucNetStatus; // status coming from across the nextwork
} GBMACHINE;
