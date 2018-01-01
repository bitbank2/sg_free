//
// TI SN76496 sound chip emulator
//
// Written by Larry Bank
// Copyright (c) 2000-2017 BitBank Software, Inc.
//
// SN76496 structure holds info needed for each chip emulated
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

typedef struct tagSN76_GG
{
unsigned char ucRegs[8];
unsigned char cVolA, cVolB, cVolC, cVolN;
int iPeriodA,iPeriodB,iPeriodC,iPeriodN;
int iCountA,iCountB,iCountC,iCountN;
int iDivisor, iPRNG, iNoise, iNoiseBits, iClock;
unsigned char cAddress; /* Register last accessed (0-7) */
int iFreqDiv[1024];  // precalculated values
} SN76_GG;

// use this structure for save/load states since it's not necessary
// to include another 4K of junk with the basic structure
typedef struct tagSN76496mini
{
unsigned char ucRegs[8];
unsigned char cVolA, cVolB, cVolC, cVolN;
int iPeriodA,iPeriodB,iPeriodC,iPeriodN;
int iCountA,iCountB,iCountC,iCountN;
int iDivisor, iPRNG, iNoise, iNoiseBits, iClock;
unsigned char cAddress; /* Register last accessed (0-7) */
} SN76496mini;


/* Internal registers */
#define SN_A_FREQ   0
#define SN_A_VOL    1
#define SN_B_FREQ   2
#define SN_B_VOL    3
#define SN_C_FREQ   4
#define SN_C_VOL    5
#define SN_N_MODE   6
#define SN_N_VOL    7

void WriteSN76_GG(int iChip, SN76_GG *pPSG, char val);
void SN76_GGGenSamples(int iChip, SN76_GG *pPSG, signed short *pBuf, int iLen, EMU_EVENT_QUEUE *eeq);
void SN76496GenSamplesGG(int iChip, SN76_GG *pPSG, signed short *pBuf, int iLen, unsigned char ucChannels, int iSoundChannels, BOOL b16Bits, EMU_EVENT_QUEUE *eeq);
void InitSN76_GG(int iChip, SN76_GG *pPSG, int iClockFreq, int iSampleFreq);
#ifdef _WIN32_WCE
void ARM_InitSN76496(int iChip, SN76_GG *pPSG, int iClockFreq, int iSampleFreq);
void ARM_WriteSN76496(int iChip, SN76_GG *pPSG, char val);
void ARM_SN76496GenSamples(int iChip, SN76_GG *pPSG, signed short *pBuf, int iLen, EMU_EVENT_QUEUE *eeq);
void ARM_SN76496GenSamplesGG(int iChip, SN76_GG *pPSG, signed short *pBuf, int iLen, unsigned char ucChannels, int iSoundChannels, BOOL b16Bits, EMU_EVENT_QUEUE *eeq);
#else
#define ARM_WriteSN76496 WriteSN76_GG
#define ARM_InitSN76496 InitSN76_GG
#define ARM_SN76496GenSamples SN76_GGGenSamples
#define ARM_SN76496GenSamplesGG SN76496GenSamplesGG
#endif
