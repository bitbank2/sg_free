/*********************************************************************/
/* Code to emulate the Texas Instruments SN76496 sound generator     */
/* Written by Larry Bank                                             */
/* Project started 1/31/2000                                         */
/* Copyright (c) 2000-2017 BitBank Software, Inc.                    */
/*                                                                   */
/* Change log                                                        */
/* 2/19/2008 - added freq conversion table to avoid divides at run-time */
/*********************************************************************/
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

#include <string.h>
#include <stdint.h>
#include "my_windows.h"
#include "smartgear.h"
#include "emu.h"
#include "sn76_gg.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned char cAttenuation[16] = {63,47,35,26,20,15,11,8,6,5,4,3,2,1,1,0};
/****************************************************************************
 *                                                                          *
 *  FUNCTION   : InitSN76496(int, SN76496 *, int, int)                      *
 *                                                                          *
 *  PURPOSE    : Initialize each SN76496 chip based on the chip frequency and*
 *               audio sample rate.                                         *
 *                                                                          *
 ****************************************************************************/
void InitSN76_GG(int iChip, SN76_GG *pPSG, int iClockFreq, int iSampleFreq)
{
double d;
int i;

   memset(&pPSG[iChip], 0, sizeof(SN76_GG));
   d = (double)iClockFreq * 44100.0;
   d = d / ((double)iSampleFreq * 20.6109 * 2.1);
   pPSG[iChip].iDivisor = (int)d; /* Get the increment for creating waveforms */
   pPSG[iChip].iPRNG = 1;
   pPSG[iChip].iClock = iClockFreq;
   // frequencies lower than 4 cannot be produced
   for (i=0x30; i<1024; i++)
      {
      pPSG[iChip].iFreqDiv[i] = pPSG[iChip].iDivisor / i;
      }
} /* InitSN76496() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SN76496GenSamples(int, SN76496 *, char *, int, int)        *
 *                                                                          *
 *  PURPOSE    : Generate n 8-bit sound samples for the specified chip.     *
 *                                                                          *
 ****************************************************************************/
void SN76_GGGenSamples(int iChip, SN76_GG *pPSG, signed short *pBuf, int iLen, EMU_EVENT_QUEUE *eeq)
{
int i, iEvent, iDelta;
uint32_t ulClock = 0;
signed short c;
unsigned char ucChanFlags; // which channels are active
SN76_GG *pChip;

   iDelta = 0;
   pChip = &pPSG[iChip];
   ucChanFlags = 0;
   if (pChip->cVolA)
      ucChanFlags |= 1; // Channel A is active
   if (pChip->cVolB)
      ucChanFlags |= 2; // Channel B is active
   if (pChip->cVolC)
      ucChanFlags |= 4; // Channel C is active
   if (pChip->cVolN)
      ucChanFlags |= 8; // Channel N is active
   if (ucChanFlags == 0)
      {
      if (eeq == NULL || eeq->iCount == 0)
         {
         memset(pBuf, 0, iLen*sizeof(short));
         return; // no active channels, return
         }
      }
      
/* Determine noise period */
      c = pChip->ucRegs[SN_N_MODE] & 3;
      if (c == 3) /* Use tone generator 3 */
         pChip->iPeriodN = pChip->iPeriodC * 2;
      else
         pChip->iPeriodN = pChip->iDivisor >> (9+c);

   iEvent = 0;
   if (eeq)
      {
      ulClock = eeq->iFrameTicks; // 1 frame worth of CPU ticks
      ulClock <<= 8; // shift to get fractional changes
      // calculate the value to convert audio samples into CPU clock ticks
      iDelta = ulClock / iLen;
      }
   for (i=0; i<iLen; i++)
      {
      if (eeq) // if there is an event queue, play back all events preceding this sample
         {
         while ((ulClock>>16) <= (eeq->ulTime[iEvent]>>8) && iEvent < eeq->iCount)
            {
            WriteSN76_GG(iChip, pPSG, eeq->ucEvent[iEvent++]);
            }
         ulClock -= iDelta;
         }
      c = 0;
      if (ucChanFlags & 1) // Channel A is active
         {
         pChip->iCountA += pChip->iPeriodA;
         if (pChip->iCountA & 0x4000)
            c += pChip->cVolA;
         else
            c -= pChip->cVolA;
         }
      if (ucChanFlags & 2) // Channel B is active
         {
         pChip->iCountB += pChip->iPeriodB;
         if (pChip->iCountB & 0x4000)
            c += pChip->cVolB;
         else
            c -= pChip->cVolB;
         }
      if (ucChanFlags & 4) // Channel C is active
         {
         pChip->iCountC += pChip->iPeriodC;
         if (pChip->iCountC & 0x4000)
            c += pChip->cVolC;
         else
            c -= pChip->cVolC;
         }
      if (ucChanFlags & 8) // Noise is active
         {
         pChip->iCountN += pChip->iPeriodN;
         /* Add noise to each channel */
         if (pChip->iCountN & 0x4000) /* if during active period of noise square wave */
            {
            if (pChip->iPRNG & 1)
               pChip->iPRNG ^= pChip->iNoiseBits;
            pChip->iPRNG >>= 1;
            if (pChip->iPRNG & 1) // if output bit active
               pChip->iNoise ^= 1; /* Toggles from 0 to 1 */
            }
         if (pChip->iNoise) /* If noise bit is active now */
            c += pChip->cVolN;
         else
            c -= pChip->cVolN;
         }
      pBuf[i] = (c << 7);
      }
   if (iEvent != eeq->iCount)
      iEvent |= 0;
} /* SN76_GGGenSamples() */

/*****************************************************************************
 *                                                                           *
 *  FUNCTION   : WriteSN76496(int, SN76496 *, char)                          *
 *                                                                           *
 *  PURPOSE    : Write a SN76496 register - update internal values as needed.*
 *                                                                           *
 *****************************************************************************/
void WriteSN76_GG(int iChip, SN76_GG *pPSG, char val)
{
SN76_GG *pChip = &pPSG[iChip];
int i,reg;

   if (val & 0x80) /* first byte */
      {
      reg = (val & 0x70)>>4;
      pChip->cAddress = reg; /* Save the register number for next time through */
      switch (reg)
         {
         case SN_A_FREQ:
            pChip->ucRegs[reg] = val & 0xf; /* Store high 4 bits */
            i = ((pChip->ucRegs[reg+1]& 0x3f)<<4) + pChip->ucRegs[reg]; // freq value
            pChip->iPeriodA = pChip->iFreqDiv[i];
            break;
         case SN_B_FREQ:
            pChip->ucRegs[reg] = val & 0xf; /* Store high 4 bits */
            i = ((pChip->ucRegs[reg+1]& 0x3f)<<4) + pChip->ucRegs[reg]; // freq value
            pChip->iPeriodB = pChip->iFreqDiv[i];
            break;
         case SN_C_FREQ:
            pChip->ucRegs[reg] = val & 0xf; /* Store high 4 bits */
            i = ((pChip->ucRegs[reg+1]& 0x3f)<<4) + pChip->ucRegs[reg]; // freq value
            pChip->iPeriodC = pChip->iFreqDiv[i];
            break;
         case SN_A_VOL:
            if (pChip->iPeriodA == 0) // special DAC mode
               pChip->cVolA = (val & 0xf)<<2;
            else
               pChip->cVolA = cAttenuation[val & 0xf];
            break;
         case SN_B_VOL:
            if (pChip->iPeriodB == 0) // special DAC mode
               pChip->cVolB = (val & 0xf)<<2;
            else
               pChip->cVolB = cAttenuation[val & 0xf];
            break;
         case SN_C_VOL:
            if (pChip->iPeriodC == 0) // special DAC mode
               pChip->cVolC = (val & 0xf)<<2;
            else
               pChip->cVolC = cAttenuation[val & 0xf];
            break;
         case SN_N_MODE:
            pChip->ucRegs[reg] = val;
            if (val & 4)
               pChip->iNoiseBits = 0x14002; // white noise
            else
               pChip->iNoiseBits = 0x8000; // synchronous noise
            pChip->iPRNG = 0x0f35; // reset noise shifter
            break;
         case SN_N_VOL:
            pChip->cVolN = cAttenuation[val & 0xf];
            break;
         }
      }
   else /* second byte */
      {
      reg = pChip->cAddress;
      val &= 0x3f; // get useful bits
      i = (val<<4) + pChip->ucRegs[reg]; // freq value
      switch (reg)
         {
         case SN_A_FREQ:
            pChip->ucRegs[reg+1] = val;
            if (i)
               {
               if (i == 1) // special case for voice synthesis
                  {
                  pChip->iPeriodA = 0;
                  pChip->iCountA = 0x4000; // tone on
                  }
               else
                  pChip->iPeriodA = pChip->iFreqDiv[i];
               }
            else
               pChip->iPeriodA = 0;
            break;
         case SN_B_FREQ:
            pChip->ucRegs[reg+1] = val;
            if (i)
               {
               if (i == 1) // special case for voice synthesis
                  {
                  pChip->iPeriodB = 0;
                  pChip->iCountB = 0x4000; // tone on
                  }
               else
                  pChip->iPeriodB = pChip->iFreqDiv[i];
               }
            else
               pChip->iPeriodB = 0;
            break;
         case SN_C_FREQ:
            pChip->ucRegs[reg+1] = val;
            if (i)
               {
               if (i == 1) // special case for voice synthesis
                  {
                  pChip->iPeriodC = 0;
                  pChip->iCountC = 0x4000; // tone on
                  }
               else
                  pChip->iPeriodC = pChip->iFreqDiv[i];
               }
            else
               pChip->iPeriodC = 0;
            break;
         case SN_N_VOL:
            pChip->cVolN = cAttenuation[val & 0xf];
            break;
         case SN_A_VOL:
            pChip->cVolA = cAttenuation[val & 0xf];
            break;
         case SN_B_VOL:
            pChip->cVolB = cAttenuation[val & 0xf];
            break;
         case SN_C_VOL:
            pChip->cVolC = cAttenuation[val & 0xf];
            break;
         default: /* Shouldn't be here - only 3 oscillators have dual-byte transfers */
            reg = 0;
            break;
         }
      }

} /* WriteSN76_GG() */
