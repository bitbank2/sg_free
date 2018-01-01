/*************************************************/
/* MIXER.H                                       */
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

#define CHANNEL_LEFT         1     // mono sound is directed to left channel
#define CHANNEL_RIGHT        2     // mono sound is directed to right channel
#define CHANNEL_BOTH         3     // mono sound is directed to both channels
#define CHANNEL_STEREO       4     // audio source is stereo

typedef struct tagMIXERCHANNEL     // sound channels for mixer
{
uint32_t ucChannels;     // bits defining left/right/both
uint32_t ucVolume;       // volume level of this channel (0-128)
int16_t *pAudio;       // pointer to audio data to be mixed
} MIXERCHANNEL;

typedef struct tagMIXER    // sound mixer 
{
int iChannelCount;      // number of channels of audio sources
int iAudioChannels;     // number of channels of audio output (1 or 2 = mono or stereo)
MIXERCHANNEL MixerChannels[16];  // allow up to 16 simultaneous channels
} MIXER;

void EMUMix(MIXER *pMixer, int iSampleCount, int16_t *pOutput);
void EMUMixerAlloc(MIXER *pMixer, int iSampleCount);
void EMUMixerFree(MIXER *pMixer);
