/*************************************************/
/* MIXER.C                                       */
/*                                               */
/* Code to mix 16-bit signed sound samples       */
/*                                               */
/* Written by Larry Bank                         */
/* Copyright (c) 2005-2017 BitBank Software, Inc.*/
/* Work started 4/14/05                          */
/*************************************************/
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
#include "mixer.h"
#include "emuio.h"

// Table to roughly translate volume levels (0-99) into a left shift amount
// static unsigned char ucVolLog[10] = {0,0,0,1,1,2,2,3,3,3};

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUMixerAlloc(MIXER *, int)                                *
 *                                                                          *
 *  PURPOSE    : Allocate the buffers for every channel.                    *
 *                                                                          *
 ****************************************************************************/
void EMUMixerAlloc(MIXER *pMixer, int iSampleCount)
{
int i, iSize;

   if (pMixer == NULL)
	   return;
   for (i=0; i<pMixer->iChannelCount; i++)
      {
      iSize = iSampleCount*sizeof(signed short)*pMixer->iAudioChannels;
      pMixer->MixerChannels[i].pAudio = EMUAlloc(iSize);
//      pMixer->MixerChannels[i].ucShift = ucVolLog[pMixer->MixerChannels[i].ucVolume/10]; // translate volume into a shift amount
      }

} /* EMUMixerAlloc() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUMixerFree(MIXER *)                                      *
 *                                                                          *
 *  PURPOSE    : Free the buffers for every channel.                        *
 *                                                                          *
 ****************************************************************************/
void EMUMixerFree(MIXER *pMixer)
{
int i;

   if (pMixer == NULL)
	   return;
   for (i=0; i<pMixer->iChannelCount; i++)
      EMUFree(pMixer->MixerChannels[i].pAudio);

} /* EMUMixerFree() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUMix(MIXER *, int, signed short *)                       *
 *                                                                          *
 *  PURPOSE    : Mix the audio channels.                                    *
 *                                                                          *
 ****************************************************************************/
void EMUMix(MIXER *pMixer, int iSampleCount, int16_t *pOutput)
{
int i, j;
int32_t lAcc, lAcc2; // sample accumulators

   if (pMixer == NULL || pOutput == NULL)
	   return;
   if (pMixer->iAudioChannels == 1) // mono
      {
      for (i=0; i<iSampleCount; i++)
         {
         lAcc = 0;
         for (j=0; j<pMixer->iChannelCount; j++)
            {
            lAcc += (pMixer->MixerChannels[j].pAudio[i] * pMixer->MixerChannels[j].ucVolume);
            }
         lAcc >>= 7; // shift down from volume adjustment (volume 0-128)
         if (lAcc < -32768)
            lAcc = -32768;
         if (lAcc > 32767)
            lAcc = 32767;
         *pOutput++ = (int16_t)lAcc; // shift it back down to 100% volume
         }
      }
   else // stereo
      {
      for (i=0; i<iSampleCount; i++)
         {
         lAcc = lAcc2 = 0;
         for (j=0; j<pMixer->iChannelCount; j++)
            {
            if (pMixer->MixerChannels[j].ucChannels & CHANNEL_STEREO) // audio source is stereo
               {
               lAcc += (pMixer->MixerChannels[j].pAudio[i*2] * pMixer->MixerChannels[j].ucVolume);
               lAcc2 += (pMixer->MixerChannels[j].pAudio[i*2+1] * pMixer->MixerChannels[j].ucVolume);
               }
            else // audio source is mono; direct it to the proper channels
               {
               if (pMixer->MixerChannels[j].ucChannels & CHANNEL_LEFT)
                  lAcc += (pMixer->MixerChannels[j].pAudio[i] * pMixer->MixerChannels[j].ucVolume);
               if (pMixer->MixerChannels[j].ucChannels & CHANNEL_RIGHT)
                  lAcc2 += (pMixer->MixerChannels[j].pAudio[i] * pMixer->MixerChannels[j].ucVolume);
               }
            }
         lAcc = (lAcc >> 7); // shift back down from volume adjustment (0-128) and clip
         if (lAcc < -32768)
            lAcc = -32768;
         if (lAcc > 32767)
            lAcc = 32767;
         lAcc2 = (lAcc2 >> 7);
         if (lAcc2 < -32768)
            lAcc2 = -32768;
         if (lAcc2 > 32767)
            lAcc2 = 32767;
         *pOutput++ = (int16_t)lAcc; // shift left back down to 100% volume
         *pOutput++ = (int16_t)lAcc2; // shift right back down to 100% volume
         }
      }
} /* EMUMix() */

