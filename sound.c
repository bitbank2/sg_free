//
// Sound functions for the SmartGear emulator
//
// Copyright (c) 1998-2017 BitBank Software, Inc.
// Written by Larry Bank
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
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include "sound.h"
#include "unzip.h"

int iSoundBlock, iNumSamples;
signed int iSoundBlocks = 0;
int iSoundTail, iSoundHead, iSoundClock;
int iDACSamples;
unsigned char *pSoundBuf;
int iSampleList[32];

void EMUOpenSound(int iSampleRate, int iBits, int iChans)
{
   iSoundBlock = (iSampleRate/60) * (iBits>>3) * iChans;
   iSoundBlocks = iSoundHead = iSoundTail = 0;
   pSoundBuf = EMUAlloc(iSoundBlock * MAX_AUDIO_BLOCKS);
	
} /* EMUOpenSound() */

void EMUCloseSound(void)
{
   iSoundBlocks = 0; // show no sound available
   EMUFree(pSoundBuf);
   pSoundBuf = NULL;

} /* EMUCloseSound() */

void EMUSetVolume(int iLevel)
{
//DWORD dwVol;

//   dwVol = iLevel * 0x11111111; // low word = left, high word = right. range 0 (silent) to FFFF //(max)
//   waveOutSetVolume(hWaveOut, dwVol);

} /* EMUSetVolume() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFreeSamples(SNDSAMPLE *, int)                           *
 *                                                                          *
 *  PURPOSE    : Free the game's sound samples.                             *
 *                                                                          *
 ****************************************************************************/
void EMUFreeSamples(SNDSAMPLE *pSounds, int iCount)
{
int i;

   for (i=0; i<iCount; i++)
      EMUFree(pSounds[i].pSound);

   EMUFree(pSounds);

} /* EMUFreeSamples() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadSamples(SNDSAMPLE *, char **, int)                  *
 *                                                                          *
 *  PURPOSE    : Load the game's sound samples.                             *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadSamples(SNDSAMPLE *pSounds, char **snames, int iCount, char *zipname)
{
unsigned char *p, *d;
signed long lSum;
int i, rc, j, k, l, iMult, iRate, iLen;
int iAudioRate;
register int iDest;
unzFile zHandle;
unsigned char cTemp[256];

//    iAudioRate = (1<<iSampleRate)*11025;
    iAudioRate = 44100; // DEBUG
    memset(pSounds, 0, sizeof(SNDSAMPLE) * iCount); /* make sure all is clear to start */

    zHandle = unzOpen(zipname);
    if (zHandle == NULL)
       return TRUE; // something went wrong
    for (i=0; i<iCount; i++) /* Load sounds from the ZIP file directly */
       {
       if (snames[i]) // if name is not NULL
          {
          rc = unzLocateFile(zHandle, snames[i], 0);
          if (rc == UNZ_END_OF_LIST_OF_FILE) /* Report the file not found */
             {
   //          bUseSound = FALSE; /* Turn off sound support */
             unzClose(zHandle);
             return TRUE;
             }
          rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
          unzReadCurrentFile(zHandle, cTemp, 16); /* Read the sample header */
          iLen = cTemp[4] + cTemp[5] * 256 + cTemp[6] * 65536; /* Length of the sample data */
          iRate = cTemp[8] + cTemp[9] * 256;
          iMult = iAudioRate / iRate; /* How many samples we have to repeat */
          if (iMult == 0) // the samples are higher than the current audio rate
             {
             iMult = iRate / iAudioRate;
             pSounds[i].iLen = iLen / iMult;
             pSounds[i].pSound = EMUAlloc(iLen/iMult); /* Allocate memory for this sound */
             p = EMUAlloc(iLen);
             unzReadCurrentFile(zHandle, p, iLen); /* Read the original sample data */
             rc = unzCloseCurrentFile(zHandle);
             d = (unsigned char *)pSounds[i].pSound;
             l = 0;
             for (j=0; j<pSounds[i].iLen; j++) /* Convert to higher sample rate */
                {
                lSum = 0;
                for (k=0; k<iMult; k++)
                   {
                   lSum += (signed char)p[l++]; // average the samples together
                   }
                lSum /= iMult;
                d[j] = (unsigned char)lSum;
                }
             }
          else
             {
             pSounds[i].iLen = iLen * iMult;
             pSounds[i].pSound = EMUAlloc(iLen*iMult); /* Allocate memory for this sound */
             p = EMUAlloc(iLen);
             unzReadCurrentFile(zHandle, p, iLen); /* Read the original sample data */
             rc = unzCloseCurrentFile(zHandle);
             d = (unsigned char *)pSounds[i].pSound;
             iDest = 0;
             for (j=0; j<iLen; j++) /* Convert to higher sample rate */
                {
                for (k=0; k<iMult; k++)
                   {
                   d[iDest++] = p[j]; /* Stretch out the samples */
                   }
                }
             }
          EMUFree(p); /* Free the original sample data */
          }
       }
    unzClose(zHandle);
    return FALSE;

} /* EMULoadSamples() */

void EMUStopSample(SNDSAMPLE *pSounds, int iSample) /* De-activate this sample */
{
    int i, j;
    
    for (i=0; i<iNumSamples; i++) /* Find this sample in the list */
        if (iSampleList[i] == iSample) /* Found it */
            break;
    if (i == iNumSamples)  /* Sample not found */
        return;
    /* De-activate if it was a looping sample */
    pSounds[iSample].bActive = FALSE;
    for (j=i; j<iNumSamples-1; j++)
        iSampleList[j] = iSampleList[j+1]; /* Shift everything up one sample */
    
    iNumSamples--; /* One less sample is playing */
    
} /* EMUStopSample() */

void EMUPlaySamples(SNDSAMPLE *pSamples, signed short *pSoundData, int iBlockSize, int iVoiceShift, BOOL bMerge)
{
signed int i, j, k;
signed short s;
SNDSAMPLE *pSamp;
    
   if (iNumSamples == 0) // no active samples, leave
       return;
   for (j=0; j<iBlockSize; j++) /* Loop through one time slice's sound allotment */
      {
      s = 0;
      for (k=0; k<iNumSamples; k++) /* Try to merge all active samples */
         {
         i = iSampleList[k]; /* Index of active sample */
         pSamp = &pSamples[i];
         if (pSamp->bActive && pSamp->pSound) /* If active and loaded */
            {
            s += (signed char)(pSamp->pSound[pSamp->iPos++]); /* Add in this sample */
            if (pSamp->iPos >= pSamp->iLen) /* Need to wrap or stop */
               {
               pSamp->iPos = 0; /* Reset pointer to start */
               if (!pSamp->bLoop)
                  pSamp->bActive = FALSE; /* We are done, turn it off */
               }
            } /* if active */
         } /* for */
      if (bMerge)
      {
         pSoundData[j*2] += (s  << (8-iVoiceShift)); // stereo data, mono sample
         pSoundData[(j*2)+1] += (s  << (8-iVoiceShift));
      }
      else
      {
         pSoundData[j*2] = (s  << (8-iVoiceShift));
          pSoundData[(j*2)+1] = (s  << (8-iVoiceShift));
      }
      } // for j
/* Remove any de-activated samples from the list */
   for (i=0; i<iNumSamples; i++)
      {
      j = iSampleList[i];
      if (!pSamples[j].bActive) /* Needs de-activation */
         {
         EMUStopSample(pSamples, j); /* Remove this sample */
         i--; /* Since the list moved up, re-do this slot */
         }
      }
} /* EMUPlaySamples */

void EMUStartSample(SNDSAMPLE *pSounds, int iSample) /* Activate this sample */
{
   if (pSounds[iSample].bActive) /* If the sound is already playing, reset to start */
      {
      pSounds[iSample].iPos = 0;
      return;
      }
   if (pSounds[iSample].iLen == 0)
      return; // bogus sample?
   iSampleList[iNumSamples++] = iSample; /* Add this sample to the currently play list */
   pSounds[iSample].iPos = 0;
   pSounds[iSample].bActive = TRUE;

} /* EMUStartSample() */

void EMUStopAllSamples(SNDSAMPLE *pSounds)
{
int i;

   for (i=0; i<iNumSamples; i++)
      pSounds[iSampleList[i]].bActive = FALSE;
   iNumSamples = 0;

} /* EMUStopAllSamples() */
