//
// Graphics routines for my game emulators
// written by Larry Bank
// Copyright (c) 1999-2017 BitBank Software, Inc.
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

//#include <android/log.h>
#include <string.h>
#include <stdint.h>
#include "my_windows.h"
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"

#ifdef USE_SIMD
#if defined( __arm__ ) || defined( __arm64__ ) || defined( __aarch64__)
#define USE_NEON
#include <arm_neon.h>
#else
//#pragma warning(disable: 4731) // C4731: frame pointer register 'ebp' modified by inline assembly code
#define USE_SSE
#include <emmintrin.h>
#include <tmmintrin.h>
#endif
#endif // USE_SIMD

#ifdef USE_ARM_ASM
void ARMDrawCoinOpTile16(unsigned char *p, unsigned char *d, int xCount, int yCount, int iColor, int iCoinOpPitch, int iSize);
void ARMDrawCoinOpSprite16(unsigned char *p, unsigned char *d, int xCount, int yCount, uint32_t ulTransMask, int iColor, int iCoinOpPitch, int iSize);
#endif

static const int iScrollDeltaX[8] = {1,2,-1,-2,0,0,0,0};
static const int iScrollDeltaY[8] = {0,0,0,0,1,2,-1,-2};

//#define PORTABLE

extern unsigned char *pCoinOpBitmap;
extern uint32_t ulTransMask, ulPriorityMask;
extern int iCoinOpPitch;

//#ifdef PORTABLE
void ASMBLT100(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
int y;
   for (y=0; y<iHeight; y++)
   {
       memcpy(pDest, pSrc, iWidth);
       pSrc += iSrcPitch;
       pDest += iDestPitch;
   }
}

void ASMBLT150S(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
//	__android_log_print(ANDROID_LOG_VERBOSE, "SmartGear", "Called ASMBLT150S");
}
void ASMBLT200(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight)
{
//	__android_log_print(ANDROID_LOG_VERBOSE, "SmartGear", "Called ASMBLT200");

}
void ASMBLT300(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
//	__android_log_print(ANDROID_LOG_VERBOSE, "SmartGear", "Called ASMBLT300");

}

#define PIXEL_A 0
#define PIXEL_B 1
#define PIXEL_C 2
#define PIXEL_D iPitch
#define PIXEL_E (iPitch+1)
#define PIXEL_F (iPitch+2)
#define PIXEL_G (iPitch*2)
#define PIXEL_H ((iPitch*2)+1)
#define PIXEL_I ((iPitch*2)+2)

void ASMBLT300HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
//	A B C --\  1 2 3
//	D E F    > 4 5 6
//	G H I --/  7 8 9
//	 1=E; 2=E; 3=E; 4=E; 5=E; 6=E; 7=E; 8=E; 9=E;
//	 IF D==B AND D!=H AND B!=F => 1=D
//	 IF (D==B AND D!=H AND B!=F AND E!=C) OR (B==F AND B!=D AND F!=H AND E!=A) => 2=B
//	 IF B==F AND B!=D AND F!=H => 3=F
//	 IF (H==D AND H!=F AND D!=B AND E!=A) OR (D==B AND D!=H AND B!=F AND E!=G) => 4=D
//	 5=E
//	 IF (B==F AND B!=D AND F!=H AND E!=I) OR (F==H AND F!=B AND H!=D AND E!=C) => 6=F
//	 IF H==D AND H!=F AND D!=B => 7=D
//	 IF (F==H AND F!=B AND H!=D AND E!=G) OR (H==D AND H!=F AND D!=B AND E!=I) => 8=H
//	 IF F==H AND F!=B AND H!=D => 9=F
//	__android_log_print(ANDROID_LOG_VERBOSE, "SmartGear", "Called ASMBLT300HQ");
int x, y, iPitch, iDPitch;
unsigned short *s, *d;
unsigned short o1, o2, o3, o4, o5, o6, o7, o8, o9;

	iPitch = iSrcPitch / 2; // adjust for working with shorts
	iDPitch = iDestPitch / 2;
	for (y=0; y<iHeight-1; y++)
	{
		d = (unsigned short *)pDest;
		s = (unsigned short *)pSrc;
		for (x=0; x<iWidth-1; x++)
		{
			o1 = o2 = o3 = o4 = o5 = o6 = o7 = o8 = o9 = s[PIXEL_E];
			//   IF (d==b && d!=h AND B!=F => 1=D
			if (s[PIXEL_D] == s[PIXEL_B] && s[PIXEL_D] != s[PIXEL_H] && s[PIXEL_B] != s[PIXEL_F])
				o1 = s[PIXEL_D];
			//	 IF (D==B AND D!=H AND B!=F AND E!=C) OR (B==F AND B!=D AND F!=H AND E!=A) => 2=B
			if ((s[PIXEL_D] == s[PIXEL_B] && s[PIXEL_D] != s[PIXEL_H] && s[PIXEL_B] != s[PIXEL_F] && s[PIXEL_E] != s[PIXEL_C]) ||
					(s[PIXEL_B] == s[PIXEL_F] && s[PIXEL_B] != s[PIXEL_D] && s[PIXEL_F] != s[PIXEL_H] && s[PIXEL_E] != s[PIXEL_A]))
				o2 = s[PIXEL_B];
			//	 IF B==F AND B!=D AND F!=H => 3=F
			if (s[PIXEL_B] == s[PIXEL_F] && s[PIXEL_B] != s[PIXEL_D] && s[PIXEL_F] != s[PIXEL_H])
				o3 = s[PIXEL_F];
			//	 IF (H==D AND H!=F AND D!=B AND E!=A) OR (D==B AND D!=H AND B!=F AND E!=G) => 4=D, 5=E
			if ((s[PIXEL_H] == s[PIXEL_D] && s[PIXEL_H] != s[PIXEL_F] && s[PIXEL_D] != s[PIXEL_B] && s[PIXEL_E] != s[PIXEL_A]) ||
					(s[PIXEL_D] == s[PIXEL_B] && s[PIXEL_D] != s[PIXEL_H] && s[PIXEL_B] != s[PIXEL_F] && s[PIXEL_E] != s[PIXEL_G]))
			{
				o4 = s[PIXEL_D];
//				o5 = s[PIXEL_E];
			}
			//	 IF (B==F AND B!=D AND F!=H AND E!=I) OR (F==H AND F!=B AND H!=D AND E!=C) => 6=F
			if ((s[PIXEL_B] == s[PIXEL_F] && s[PIXEL_B] != s[PIXEL_D] && s[PIXEL_F] != s[PIXEL_H] && s[PIXEL_E] != s[PIXEL_I]) ||
					(s[PIXEL_F] == s[PIXEL_H] && s[PIXEL_F] != s[PIXEL_B] && s[PIXEL_H] != s[PIXEL_D] && s[PIXEL_E] != s[PIXEL_C]))
				o6 = s[PIXEL_F];
			//	 IF H==D AND H!=F AND D!=B => 7=D
			if (s[PIXEL_H] == s[PIXEL_D] && s[PIXEL_H] != s[PIXEL_F] && s[PIXEL_D] != s[PIXEL_B])
				o7 = s[PIXEL_D];
			//	 IF (F==H AND F!=B AND H!=D AND E!=G) OR (H==D AND H!=F AND D!=B AND E!=I) => 8=H
			if ((s[PIXEL_F] == s[PIXEL_H] && s[PIXEL_F] != s[PIXEL_B] && s[PIXEL_H] != s[PIXEL_D] && s[PIXEL_E] != s[PIXEL_G]) ||
					(s[PIXEL_H] == s[PIXEL_D] && s[PIXEL_H] != s[PIXEL_F] && s[PIXEL_D] != s[PIXEL_B] && s[PIXEL_E] != s[PIXEL_I]))
				o8 = s[PIXEL_H];
			//	 IF F==H AND F!=B AND H!=D => 9=F
			if (s[PIXEL_F] == s[PIXEL_H] && s[PIXEL_F] != s[PIXEL_B] && s[PIXEL_H] != s[PIXEL_D])
				o9 = s[PIXEL_F];
			// write output
			d[0] = o1; d[1] = o2; d[2] = o3;
			d[iDPitch] = o4; d[iDPitch+1] = o5; d[iDPitch+2] = o6;
			d[(iDPitch*2)] = o7; d[(iDPitch*2)+1] = o8; d[(iDPitch*2)+2] = o9;
			d += 3;
			s += 1;
		} // for x
		pDest += iDestPitch*3;
		pSrc += iSrcPitch;
	} // for y
}

//  A    --\ 1 2
//C P B  --/ 3 4
//  D
// 1=P; 2=P; 3=P; 4=P;
// IF C==A AND C!=D AND A!=B => 1=A
// IF A==B AND A!=C AND B!=D => 2=B
// IF B==D AND B!=A AND D!=C => 4=D
// IF D==C AND D!=B AND C!=A => 3=C
void ASMBLT200HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
#ifdef USE_SSE
    __m128i xmmA, xmmB, xmmC, xmmD;
    __m128i xmmCmpCD, xmmCmpAC, xmmCmpAB, xmmCmpBD;
    __m128i xmmDest1, xmmDest2, xmmDest3, xmmDest4;
    __m128i xmmPixelTemp, xmmCmpTemp;
    int x, y;
    unsigned char *s, *d, *s2, *d2;
    
    // Copy top and bottom line as-is
    s = pSrc;
    d = pDest;
    s2 = &pSrc[(iHeight-1) * iSrcPitch];
    d2 = &pDest[(iHeight-1) * 2 * iDestPitch];
    for (x=0; x<iWidth; x+=8)
    {
        xmmA = _mm_loadu_si128((__m128i*)s); // top line
        xmmB = _mm_loadu_si128((__m128i*)s2); // bottom line
        xmmDest1 = _mm_unpacklo_epi16(xmmA, xmmA); // double pixel width
        xmmDest2 = _mm_unpackhi_epi16(xmmA, xmmA);
        xmmDest3 = _mm_unpacklo_epi16(xmmB, xmmB);
        xmmDest4 = _mm_unpackhi_epi16(xmmB, xmmB);
        _mm_storeu_si128((__m128i*)d, xmmDest1);
        _mm_storeu_si128((__m128i*)&d[16], xmmDest2);
        _mm_storeu_si128((__m128i*)&d[iDestPitch], xmmDest1);
        _mm_storeu_si128((__m128i*)&d[iDestPitch+16], xmmDest2);
        _mm_storeu_si128((__m128i*)d2, xmmDest3);
        _mm_storeu_si128((__m128i*)&d2[16], xmmDest4);
        _mm_storeu_si128((__m128i*)&d2[iDestPitch], xmmDest3);
        _mm_storeu_si128((__m128i*)&d2[iDestPitch+16], xmmDest4);
        s += 16; s2 += 16;
        d += 32; d2 += 32;
    }
    for (y=1; y<iHeight-1; y++)
    {
        s = &pSrc[(y-1) * iSrcPitch];
        d = &pDest[y * 2 * iDestPitch + 4];
        for (x=1; x<iWidth-1; x+=8) // process 8 pixels at a time
        {
            xmmDest1 = xmmDest2 = xmmDest3 = xmmDest4 = _mm_loadu_si128((__m128i*)&s[2+iSrcPitch]); // P
            xmmA = _mm_loadu_si128((__m128i*)&s[2]);
            xmmC = _mm_loadu_si128((__m128i*)&s[iSrcPitch]);
            xmmD = _mm_loadu_si128((__m128i*)&s[2+(iSrcPitch*2)]);
            xmmB = _mm_loadu_si128((__m128i*)&s[4+iSrcPitch]);
            xmmCmpCD = _mm_cmpeq_epi16(xmmC, xmmD); // if (C == D) all 4 diagonals and their compliments
            xmmCmpAC = _mm_cmpeq_epi16(xmmC, xmmA); // if (A == C)
            xmmCmpAB = _mm_cmpeq_epi16(xmmA, xmmB); // if (A == B)
            xmmCmpBD = _mm_cmpeq_epi16(xmmB, xmmD); // if (B == D)
            // Prepare pixel 1
            xmmCmpTemp = _mm_andnot_si128(xmmCmpCD, xmmCmpAC); // if (C==A && C!=D)
            xmmCmpTemp = _mm_andnot_si128(xmmCmpAB, xmmCmpTemp); // if (C==A && C!=D && A!=B)
            xmmDest1 = _mm_andnot_si128(xmmCmpTemp, xmmDest1); // remove pixels which will be replaced
            xmmPixelTemp = _mm_and_si128(xmmA, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest1 = _mm_or_si128(xmmDest1, xmmPixelTemp); // 1's have been replaced with A's where needed
            // Prepare pixel 2
            xmmCmpTemp = _mm_andnot_si128(xmmCmpAC, xmmCmpAB); // if (A!=C && A==B)
            xmmCmpTemp = _mm_andnot_si128(xmmCmpBD, xmmCmpTemp); // if (A==B && A!=C && B!=D)
            xmmDest2 = _mm_andnot_si128(xmmCmpTemp, xmmDest2); // remove pixels which will be replaced
            xmmPixelTemp = _mm_and_si128(xmmB, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest2 = _mm_or_si128(xmmDest2, xmmPixelTemp); // 2's have been replaced with B's where needed
            // Prepare pixel 3
            xmmCmpTemp = _mm_andnot_si128(xmmCmpBD, xmmCmpCD); // IF D==C AND D!=B
            xmmCmpTemp = _mm_andnot_si128(xmmCmpAC, xmmCmpTemp); // IF D==C AND D!=B AND C!=A
            xmmDest3 = _mm_andnot_si128(xmmCmpTemp, xmmDest3); // remove pixels which will be replaced
            xmmPixelTemp = _mm_and_si128(xmmC, xmmCmpTemp); // mask off pixels which will replace 3's
            xmmDest3 = _mm_or_si128(xmmDest3, xmmPixelTemp); // 3's have been replaced with C's where needed
            // Prepare pixel 4
            xmmCmpTemp = _mm_andnot_si128(xmmCmpAB, xmmCmpBD); // if (B==D && B!=A)
            xmmCmpTemp = _mm_andnot_si128(xmmCmpCD, xmmCmpTemp); // if (B==D && B!=A && D!=C)
            xmmDest4 = _mm_andnot_si128(xmmCmpTemp, xmmDest4); // remove pixels which will be replaced
            xmmPixelTemp = _mm_and_si128(xmmD, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest4 = _mm_or_si128(xmmDest4, xmmPixelTemp); // 4's have been replaced with D's where needed
            xmmPixelTemp = _mm_unpacklo_epi16(xmmDest1, xmmDest2);
            xmmCmpTemp = _mm_unpackhi_epi16(xmmDest1, xmmDest2);
            _mm_storeu_si128((__m128i*)d, xmmPixelTemp); // write top left 8 destination pixels
            _mm_storeu_si128((__m128i*)&d[16], xmmCmpTemp); // write top right 8 destination pixels
            xmmPixelTemp = _mm_unpacklo_epi16(xmmDest3, xmmDest4);
            xmmCmpTemp = _mm_unpackhi_epi16(xmmDest3, xmmDest4);
            _mm_storeu_si128((__m128i*)&d[iDestPitch], xmmPixelTemp); // write bottom left 8 destination pixels
            _mm_storeu_si128((__m128i*)&d[16+iDestPitch], xmmCmpTemp); // write bottom right 8 destination pixels
            s += 16;
            d += 32;
        } // for x
    } // for y
    // fill in the missing left/right edges
    for (y=1; y<iHeight-1; y++)
    {
          unsigned short usL, usR, *us, *ud, *us2, *ud2;
      us = (unsigned short *)&pSrc[y * iSrcPitch];
      ud = (unsigned short *)&pDest[y * 2 * iDestPitch];
      us2 = (unsigned short *)&pSrc[(y * iSrcPitch) + (iWidth-1)*2];
      ud2 = (unsigned short *)&pDest[(y * 2 * iDestPitch) + (iWidth-1)*4];
      usL = us[0];
      usR = us2[0];
      ud[0] = ud[1] = usL;
      ud[iDestPitch/2] = ud[(iDestPitch/2)+1] = usL;
      ud2[0] = ud2[1] = usR;
      ud2[iDestPitch/2] = ud2[(iDestPitch/2)+1] = usR;
    }
#endif // USE_SSE
#ifdef USE_NEON
    uint16x8_t xmmA, xmmB, xmmC, xmmD;
    uint16x8_t xmmCmpCD, xmmCmpAC, xmmCmpAB, xmmCmpBD;
    uint16x8_t xmmDest1, xmmDest2, xmmDest3, xmmDest4;
    uint16x8_t xmmPixelTemp, xmmCmpTemp;
    uint16x8x2_t xmmOut;
    int x, y;
    unsigned char *s, *d, *s2, *d2;

    // Copy top and bottom line as-is
    s = pSrc;
    d = pDest;
    s2 = &pSrc[(iHeight-1) * iSrcPitch];
    d2 = &pDest[(iHeight-1) * 2 * iDestPitch];
    for (x=0; x<iWidth; x+=8)
    {
        xmmA = vld1q_u16((uint16_t*)s); // top line
        xmmB = vld1q_u16((uint16_t*)s2); // bottom line
        xmmOut.val[0] = xmmOut.val[1] = xmmA;
        vst2q_u16((uint16_t*)d, xmmOut);
        vst2q_u16((uint16_t*)&d[iDestPitch], xmmOut);
        xmmOut.val[0] = xmmOut.val[1] = xmmB;
        vst2q_u16((uint16_t*)d2, xmmOut);
        vst2q_u16((uint16_t*)&d2[iDestPitch], xmmOut);
        s += 16; s2 += 16;
        d += 32; d2 += 32;
    }
    
    for (y=1; y<iHeight-1; y++)
    {
        s = &pSrc[y * iSrcPitch];
        d = &pDest[(y * 2 * iDestPitch) + 4];
        for (x=1; x<iWidth-1; x+=8) // process 8 pixels at a time
        {
            xmmDest1 = xmmDest2 = xmmDest3 = xmmDest4 = vld1q_u16((uint16_t*)&s[2+iSrcPitch]); // P
            xmmA = vld1q_u16((uint16_t*)&s[2]);
            xmmC = vld1q_u16((uint16_t*)&s[iSrcPitch]);
            xmmD = vld1q_u16((uint16_t*)&s[2+(iSrcPitch*2)]);
            xmmB = vld1q_u16((uint16_t*)&s[4+iSrcPitch]);
            xmmCmpCD = vceqq_u16(xmmC, xmmD); // if (C == D) all 4 diagonals and their compliments
            xmmCmpAC = vceqq_u16(xmmC, xmmA); // if (A == C)
            xmmCmpAB = vceqq_u16(xmmA, xmmB); // if (A == B)
            xmmCmpBD = vceqq_u16(xmmB, xmmD); // if (B == D)
            // Prepare pixel 1
            xmmCmpTemp = vbicq_u16(xmmCmpAC, xmmCmpCD); // if (C==A && C!=D)
            xmmCmpTemp = vbicq_u16(xmmCmpTemp, xmmCmpAB); // if (C==A && C!=D && A!=B)
            xmmDest1 = vbicq_u16(xmmDest1, xmmCmpTemp); // remove pixels which will be replaced
            xmmPixelTemp = vandq_u16(xmmA, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest1 = vorrq_u16(xmmDest1, xmmPixelTemp); // 1's have been replaced with A's where needed
            // Prepare pixel 2
            xmmCmpTemp = vbicq_u16(xmmCmpAB, xmmCmpAC); // if (A!=C && A==B)
            xmmCmpTemp = vbicq_u16(xmmCmpTemp, xmmCmpBD); // if (A==B && A!=C && B!=D)
            xmmDest2 = vbicq_u16(xmmDest2, xmmCmpTemp); // remove pixels which will be replaced
            xmmPixelTemp = vandq_u16(xmmB, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest2 = vorrq_u16(xmmDest2, xmmPixelTemp); // 2's have been replaced with B's where needed
            // Prepare pixel 3
            xmmCmpTemp = vbicq_u16(xmmCmpCD, xmmCmpBD); // IF D==C AND D!=B
            xmmCmpTemp = vbicq_u16(xmmCmpTemp, xmmCmpAC); // IF D==C AND D!=B AND C!=A
            xmmDest3 = vbicq_u16(xmmDest3, xmmCmpTemp); // remove pixels which will be replaced
            xmmPixelTemp = vandq_u16(xmmC, xmmCmpTemp); // mask off pixels which will replace 3's
            xmmDest3 = vorrq_u16(xmmDest3, xmmPixelTemp); // 3's have been replaced with C's where needed
            // Prepare pixel 4
            xmmCmpTemp = vbicq_u16(xmmCmpBD, xmmCmpAB); // if (B==D && B!=A)
            xmmCmpTemp = vbicq_u16(xmmCmpTemp, xmmCmpCD); // if (B==D && B!=A && D!=C)
            xmmDest4 = vbicq_u16(xmmDest4, xmmCmpTemp); // remove pixels which will be replaced
            xmmPixelTemp = vandq_u16(xmmD, xmmCmpTemp); // mask off pixels which will replace 1's
            xmmDest4 = vorrq_u16(xmmDest4, xmmPixelTemp); // 4's have been replaced with D's where needed
            xmmOut.val[0] = xmmDest1;
            xmmOut.val[1] = xmmDest2;
            vst2q_u16((uint16_t*)d, xmmOut);
            xmmOut.val[0] = xmmDest3;
            xmmOut.val[1] = xmmDest4;
            vst2q_u16((uint16_t*)&d[iDestPitch], xmmOut);
            s += 16;
            d += 32;
        } // for x
    } // for y
    // fill in the missing left/right edges
    for (y=1; y<iHeight-1; y++)
    {
          unsigned short usL, usR, *us, *ud, *us2, *ud2;
      us = (unsigned short *)&pSrc[y * iSrcPitch];
      ud = (unsigned short *)&pDest[y * 2 * iDestPitch];
      us2 = (unsigned short *)&pSrc[(y * iSrcPitch) + (iWidth-1)*2];
      ud2 = (unsigned short *)&pDest[(y * 2 * iDestPitch) + (iWidth-1)*4];
      usL = us[0];
      usR = us2[0];
      ud[0] = ud[1] = usL;
      ud[iDestPitch/2] = ud[(iDestPitch/2)+1] = usL;
      ud2[0] = ud2[1] = usR;
      ud2[iDestPitch/2] = ud2[(iDestPitch/2)+1] = usR;
    }
#endif // USE_NEON

#if !defined(USE_SSE) && !defined(USE_NEON)      
int x, y;
unsigned short *s,*s2;
uint32_t *d,*d2;
unsigned short A, B, C, D, P;
uint32_t ulPixel, ulPixel2;

// copy the edge pixels as-is
// top+bottom lines first
s = (unsigned short *)pSrc;
s2 = (unsigned short *)&pSrc[(iHeight-1)*iSrcPitch];
d = (uint32_t *)pDest;
d2 = (uint32_t *)&pDest[(iHeight-1)*2*iDestPitch];
for (x=0; x<iWidth; x++)
{
      ulPixel = *s++;
      ulPixel2 = *s2++;
      ulPixel |= (ulPixel << 16); // simply double it
      ulPixel2 |= (ulPixel2 << 16);
      d[0] = ulPixel;
      d[iDestPitch/4] = ulPixel;
      d2[0] = ulPixel2;
      d2[iDestPitch/4] = ulPixel2;
      d++; d2++;
}
for (y=1; y<iHeight-1; y++)
  {
  s = (unsigned short *)&pSrc[y * iSrcPitch];
  d = (uint32_t *)&pDest[(y * 2 * iDestPitch)];
// first pixel is just stretched
  ulPixel = *s++;
  ulPixel |= (ulPixel << 16);
  d[0] = ulPixel;
  d[iDestPitch/4] = ulPixel;
  d++;
  for (x=1; x<iWidth-1; x++)
     {
     A = s[-iSrcPitch/2];
     C = s[-1];
     P = s[0];
     B = s[1];
     D = s[iSrcPitch/2];
     if (C==A && C!=D && A!=B)
        ulPixel = A;
     else
        ulPixel = P;
     if (A==B && A!=C && B!=D)
        ulPixel |= (B << 16);
     else
        ulPixel |= (P << 16);
     d[0] = ulPixel;
     if (D==C && D!=B && C!=A)
        ulPixel = C;
     else
        ulPixel = P;
     if (B==D && B!=A && D!=C)
        ulPixel |= (D << 16);
     else
        ulPixel |= (P << 16);
     d[iDestPitch/4] = ulPixel;
     d++;
     s++;
     } // for x
// last pixel is just stretched
  ulPixel = s[0];
  ulPixel |= (ulPixel << 16);
  d[0] = ulPixel;
  d[iDestPitch/4] = ulPixel;     
  } // for y
#endif // !SSE && !NEON
} /* ASMBLT200HQ() */
//#endif // PORTABLE

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFindScrollAmount()                                      *
 *                                                                          *
 *  PURPOSE    : Find the horizontal or vertical scroll amount between      *
 *               Between 2 16x16 RGB565 tiles. If nothing fits, return 0.   *
 *                                                                          *
 ****************************************************************************/
unsigned char EMUFindScrollAmount(uint16_t *s, uint16_t *d, int iPitch, unsigned  char ucMask)
{
// Try x -2,-1,+1,+2 and y -2,-1,+1,+2 (8 possibilities)
signed int z, x, y, xStart, xEnd, yStart, yEnd;
unsigned char ucScroll;
uint16_t *ps, *pd;

  ucScroll = 0xff; // start loop assuming failure
  for (z=0; z<8 && ucScroll == 0xff; z++) // try up to 8 possible scroll permutations
  {
     if ((ucMask & (1<<z)) == 0) continue; // skip this scroll test

     ucScroll = z; // current pattern
     xStart = iScrollDeltaX[z];
     yStart = iScrollDeltaY[z];
     xEnd = yEnd = 16;
     if (xStart < 0)
        xStart = 0 - xStart;
     else
        xEnd -= xStart;
     if (yStart < 0)
        yStart = 0 - yStart;
     else
        yEnd -= yStart;    
     for (y=yStart; y<yEnd; y++)
     {
        ps = &s[(yStart * iPitch)];
        pd = &d[((yStart + iScrollDeltaY[z]) * iPitch) + iScrollDeltaX[z]];
        for(x=xStart; x<xEnd; x++)
        {
           if (ps[x] != pd[x]) // non-match, stop right here
           {
              x=xEnd; y=yEnd; // stop looking with this x/y offset
              ucScroll = 0xff; // mark it as a failure
           }
        } // for x
        ps += iPitch;
        pd += iPitch;
     } // for y     
  } // for z
  return ucScroll;
} /* EMUFindScroll() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUTestTile()                                              *
 *                                                                          *
 *  PURPOSE    : Test the current tile against the offset previous.         *
 *               Returns true if they don't match, false if they do.        *
 *                                                                          *
 ****************************************************************************/
int EMUTestTile(unsigned char *s, unsigned char *d, int iPitch)
{
int x, y;
uint32_t *ps, *pd;
size_t i;

  // Test if the offset tile matches the original
  // check for unaligned access of destination bitmap due to scroll offset
   i = (size_t)d;
   if (i & 2) // need to access it a word at a time
   {
      uint16_t *s16, *d16;
      for (y=0; y<16; y++)
      {
         s16 = (uint16_t*)&s[y * iPitch];
         d16 = (uint16_t*)&d[y * iPitch];
         for (x=0; x<16; x++)
         {
            if (s16[x] != d16[x]) // match failed, quit
              return 1;
         } // for x
      } // for y
   }
   else // can use dwords
   {
      for (y=0; y<16; y++)
      {
         ps = (uint32_t*)&s[y * iPitch];
         pd = (uint32_t*)&d[y * iPitch];
         for (x=0; x<8; x++)
         {
            if (ps[x] != pd[x]) // match failed, quit
              return 1;
         } // for x
      } // for y
   } // dword access

return 0; // tiles match
} /* EMUTestTile() */

static void EMUScrollBitmap(unsigned char *pBitmap, int iWidth, int iHeight, int iPitch, unsigned char ucScroll)
{
int iDX, iDY;
int x, y;
uint16_t *s, *d;

	iDX = iScrollDeltaX[ucScroll];
	iDY = iScrollDeltaY[ucScroll];

	if (iDX && iDY == 0)
	{
		for (y=0; y<iHeight; y++)
		{
			s = (uint16_t *)&pBitmap[y *iPitch];
			if (iDX < 0)
			{
				iDX = 0-iDX;
				for (x=iWidth-1; x>=iDX; x--) // work backwards
				{
					s[x] = s[x - iDX];
				}
                                for (x=0; x<iDX; x++)
				{
					s[x] = 0;
				}
			}
			else
			{
				for (x=0; x<iWidth-iDX; x++)
				{
					s[x] = s[x+iDX];
				}
				for (; x<iWidth; x++)
				{
					s[x] = 0; // erase new visible edge
				}
			}
		} // for y
	}
	else if (iDX == 0 && iDY) // vertical scrolling
	{
		if (iDY < 0)
		{
                        for (y=iHeight-1+iDY; y>=0; y--)
                        {
                                s = (uint16_t *)&pBitmap[y * iPitch];
                                d = (uint16_t *)&pBitmap[(y-iDY) * iPitch];
                                memcpy(d, s, iPitch);
                        }
		}
		else
		{
			for (y=0; y<iHeight-iDY; y++)
			{
				s = (uint16_t*)&pBitmap[(y+iDY) * iPitch];
				d = (uint16_t*)&pBitmap[(y * iPitch)];
				memcpy(d, s, iPitch);
			}
		}
	}
	else // both vertical and horizontal together
	{
	}
} /* EMUScrollBitmap() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFindChangedRegion()                                     *
 *                                                                          *
 *  PURPOSE    : Find the minimum changed area between 2 bitmaps.           *
 *               The RGB565 bitmap is divided into 16x16 pixel tiles        *
 *               A bitmap of the changed tiles is returned along with H/V   *
 *               scrolling offsets if needed.                               * 
 *                                                                          *
 ****************************************************************************/
int EMUFindChangedRegion(unsigned char *pSrc, unsigned char *pDst, int iWidth,
 int iHeight, int iPitch, int iTileWidth, int iTileHeight, uint32_t *pRegions, int *iScrollX, int *iScrollY)
{
int x, y, xc, yc, dy;
int xCount, yCount;
uint32_t *s, *d, u32RowBits;
int iTotalChanged = 0;
static int iPatOff = 0; // pattern offset toggles each time through
//
// Divide the image into 16x16 tiles; each tile on a row is 1 bit in the changed bit field
// This makes managing the changes easier for display on a normal monitor and on LCDs with
// slow connections (e.g. SPI/I2C)
// The code has an "early exit" for each tile. In essence, the more the bitmaps are alike
// the harder this code will work and the less of the screen will need to be repainted
// In other words, the performance is balanced such that it should maintain a constant output rate
// no matter what the bitmap conditions are
//
   yCount = (iHeight+iTileHeight-1) / iTileHeight; // divide it into NxN tiles
   xCount = (iWidth+iTileWidth-1) / iTileWidth;

  // Loop through all of the tiles
   for (yc=0; yc<yCount; yc++)
   {
      u32RowBits = 0;
      for (xc=0; xc<xCount; xc++)
      {
         //point to the current tile
         s = (uint32_t *)&pSrc[(((yc*iTileHeight)/*+iPatOff*/) * iPitch) + (xc*iTileWidth*2)];
         d = (uint32_t *)&pDst[(((yc*iTileHeight)/*+iPatOff*/) * iPitch) + (xc*iTileWidth*2)];
         // loop through the pixels of this tile
	 if ((yc+1)*iTileHeight > iHeight)
		dy = iHeight - (yc*iTileHeight);
	 else
		dy = iTileHeight;
         for (y =0/* iPatOff */; y<dy; y++)
         {
            for (x = 0; x < iTileWidth/2; x++) // compare pairs of pixels
            { // test pairs of RGB565 pixels
               if (s[x] != d[x]) // any change means we mark this strip as being changed and move on
               {
                  u32RowBits |= (1 << xc);
                  iTotalChanged++;
                  y = iTileHeight; x = iTileWidth; // continue to next tile
               }
            } // for x
            s += (iPitch/4);
            d += (iPitch/4);
         } // for y
      } // for xc
   pRegions[yc] = u32RowBits;
   } // for yc
// Check for scrolling situation
   if ((iScrollX || iScrollY) && iTotalChanged > ((xCount*yCount)/4)) // more than a quarter of bitmap changed
   { // Scroll direction for each tile (3-bit index for x/y offsets)
      unsigned char uc, ucMask, ucPrevScroll;
      int iHisto[8];

      ucMask = 0;
      ucPrevScroll = 0xff;
      if (iScrollX)
      {
          *iScrollX = 0;
          ucMask |= 0xf; // test x scrolling
      }
      if (iScrollY)
      {
          *iScrollY = 0;
          ucMask |= 0xf0; // test y scrolling
      }
      memset(iHisto, 0, sizeof(iHisto)); // reset stats
      for (yc=0; yc<yCount; yc++)
      {
         u32RowBits = pRegions[yc];
         for (xc=0; xc<xCount; xc++)
         {
             if (u32RowBits & (1<<xc)) // tile changed
             {
                s = (uint32_t *)&pSrc[((yc*16) * iPitch) + (xc*32)];
                d = (uint32_t *)&pDst[((yc*16) * iPitch) + (xc*32)];
                uc = 0xff;
                if (ucPrevScroll != 0xff)
                	uc = EMUFindScrollAmount((uint16_t *)s, (uint16_t*)d, iPitch/2, 1<<ucPrevScroll);
                if (uc == 0xff) // previous scroll didn't work
                {
                   uc = EMUFindScrollAmount((uint16_t *)s, (uint16_t*)d, iPitch/2, ucMask & ~(1<<ucPrevScroll));
                }
                if (uc != 0xff)
                {
                   ucPrevScroll = uc;
                   iHisto[uc]++;
                }
             }
         } // for xc
      } // for yc
      // See if the majority of scroll directions are the same
      for (yc=0; yc<8; yc++)
      {
          if (iHisto[yc] > iTotalChanged/2) // majority agree with scroll direction
          {
             int x, y, dx, dy;
             unsigned char *st, *dt;
             uint32_t u32Mask, u32TempRegions[32];
             int iTempChanged;
             dx = iScrollDeltaX[yc];
             dy = iScrollDeltaY[yc];
             // Recalculate the dirty tiles based on the new scroll offset
             iTempChanged = 0; // dirty tile count at scroll offset
             for (y=0; y<yCount; y++)
             {
                u32RowBits = 0;
                for (x=0; x<xCount; x++)
                {
                   st = &pSrc[((y*16) * iPitch) + (x*32)];
                   dt = &pDst[(((y*16)+dy) * iPitch) + (x*32)+(dx*2)];
                   if (EMUTestTile(st, dt, iPitch))
                   {
                      u32RowBits |= (1<<x);
                      iTempChanged++;
                   }
                } // for x
                if (dx > 0) u32Mask = (1<<(xCount-1)); // right edge
		else u32Mask = 1;
                if (!(u32RowBits & u32Mask)) // count the edge that always gets messed up
                {
                   u32RowBits |= u32Mask;
		   iTempChanged++; // count the edge tile which always gets messed up
                }
                u32TempRegions[y] = u32RowBits; // save updated tile flags
             } // for y
             if (iTempChanged < iTotalChanged) // if fewer dirty tiles with scroll offset
             {
		if (iScrollX)
                	*iScrollX = dx; // set the scroll offset 
                if (iScrollY)
			*iScrollY = dy;
                // scroll the destination bitmap to simulate the hardware scroll
                EMUScrollBitmap(pDst, iWidth, iHeight, iPitch, yc);
                memcpy(pRegions, u32TempRegions, yCount * sizeof(uint32_t));
                iTotalChanged = iTempChanged; // new dirty tile count
                yc = 8; // break out of the outer loop
             }
          } // found scroll offset
      }
   } // if majority of frame changed (check for scrolling)
   iPatOff = 1-iPatOff; // toggle pattern even/odd for next time through

   return iTotalChanged;
} /* EMUFindChangedRegion() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite16SD(int, int, uchar, uchar, bool bool)       *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with line scrolling       *
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite16SD(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, uint32_t *pScroll, int iScroll)
{
unsigned char *p;
unsigned short *d;
int xCount, yCount, iDestPitch, iSrcPitch;
unsigned int c;
register int x, y, xOff;

/* Adjust for clipped sprite */
   xCount = yCount = 16; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = (unsigned short *)&pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
   iDestPitch = iCoinOpPitch/2;

   if (bFlipy)
      {
      p += 15*16; // point to bottom of sprite
      iSrcPitch = -16; // next line offset
      }
   else
      iSrcPitch = 16;

   if (bFlipx)
      {
      for (y=0; y<yCount; y++)
         {
         unsigned short *pDest;
         xOff = pScroll[y];
         xOff -= iScroll;
         pDest = &d[-xOff];
         for (x=0; x<xCount; x++)
            {
            c = p[15-x];
            if (!(ulTransMask & (1 << c)))
               {
               pDest[x] = (unsigned short)(iColor + c);
               }
            } // for x
         p += iSrcPitch;
         d += iDestPitch;
         } // for y
      }
   else
      {
      for (y=0; y<yCount; y++)
         {
         unsigned short *pDest;
         xOff = pScroll[y];
         xOff -= iScroll;
         pDest = &d[-xOff];
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1 << c)))
               {
               pDest[x] = (unsigned short)(iColor + c);
               }
            } // for x
         p += iSrcPitch;
         d += iDestPitch;
         } // for y
      }

} /* EMUDrawSprite16SD() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite16D(int, int, uchar, uchar, bool bool)        *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite16D(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, int iSize)
{
unsigned char *d, *p;
int xCount, yCount; //, iDestPitch;
unsigned int c;
unsigned short *usd;
register int x, y;

/* Adjust for clipped sprite */
   xCount = yCount = iSize; /* Assume full size to draw */
   p = &pSpriteData[iSprite * iSize * iSize];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*iSize);
      else
         p -= (sy*iSize); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-iSize) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-iSize) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
//   iDestPitch = iCoinOpPitch - (xCount*2);

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      usd = (unsigned short *)d;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
      }
   if (bFlipx && !bFlipy)
      {
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p += iSize;
         }
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
      usd = (unsigned short *)d;
#ifdef USE_ARM_ASM
      ARMDrawCoinOpSprite16(p, d, xCount, yCount, ulTransMask, iColor, iCoinOpPitch, iSize);
#else
#ifdef USE_SSE
          if ((xCount & 7) == 0) // must be multiple of 8 pixels
          {
              __m128i xmmIn, xmmColor, xmmZero, xmmMask, xmmOut, xmmShiftL, xmmShiftH;
              __m128i xmmInL, xmmInH;
              // Since SSE doesn't have a non-immediate bit shift, we can simulate it with pshufb
              xmmShiftL = _mm_set_epi8(0,0,0,0,0,0,0,0,0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1);
              xmmShiftH = _mm_set_epi8(0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1,0,0,0,0,0,0,0,0);
              xmmMask = _mm_set1_epi16(ulTransMask);
              xmmZero = _mm_setzero_si128();
              xmmColor = _mm_set1_epi16(iColor);
          for (y=0; y<yCount; y++)
          {
              for (x=0; x<xCount; x+=8)
              {
                  xmmIn = _mm_loadl_epi64((__m128i*)p);
                  xmmOut = _mm_loadu_si128((__m128i*)usd); // need to read output pixels to merge with transparent sprite pixels
                  xmmInL = _mm_shuffle_epi8(xmmShiftL, xmmIn);
                  xmmInH = _mm_shuffle_epi8(xmmShiftH, xmmIn);
                  p += 8;
                  xmmIn = _mm_unpacklo_epi8(xmmIn, xmmZero); // expand to 16-bits
                  xmmInL = _mm_unpacklo_epi8(xmmInL, xmmInH); // get shifted mask to compare with transparency mask
                  xmmIn = _mm_add_epi16(xmmIn, xmmColor); // add the sprite color
                  xmmInL = _mm_and_si128(xmmInL, xmmMask); // see which colors to skip
                  xmmInL = _mm_cmpeq_epi16(xmmInL, xmmZero);
                  xmmOut = _mm_andnot_si128(xmmInL, xmmOut); // preserve transparent pixels in destination
                  xmmIn = _mm_and_si128(xmmInL, xmmIn); // mask off transparent pixels
                  xmmOut = _mm_or_si128(xmmOut, xmmIn); // final output
                  _mm_storeu_si128((__m128i*)usd, xmmOut);
                  usd += 8;
              } // for x
              p += iSize - xCount;
              usd += (iCoinOpPitch/2) - xCount;
          } // for y
          }
          else
#endif // USE_SSE
#ifdef USE_NEON
          if ((xCount & 7) == 0) // must be multiple of 8 pixels
          {
              uint8x8_t xmmIn8;
              uint16x8_t xmmIn, xmmZero, xmmOne, xmmShift, xmmColor, xmmMask, xmmOut;
              xmmMask = vdupq_n_u16(ulTransMask);
              xmmColor = vdupq_n_u16(iColor);
              xmmOne = vdupq_n_u16(1);
              xmmZero = vdupq_n_u16(0);
          for (y=0; y<yCount; y++)
          {
              for (x=0; x<xCount; x+=8)
              {
                  xmmIn8 = vld1_u8(p);
                  xmmOut = vld1q_u16(usd); // need to read output pixels to merge with transparent sprite pixels
                  p += 8;
                  xmmIn = vmovl_u8(xmmIn8);
                  xmmShift = vshlq_u16(xmmOne, vreinterpretq_s16_u16(xmmIn)); // get color masks
                  xmmIn = vaddq_u16(xmmIn, xmmColor); // add the sprite color
                  xmmShift = vandq_u16(xmmShift, xmmMask); // see which colors to skip
                  xmmShift = vceqq_u16(xmmShift, xmmZero);
                  xmmOut = vbicq_u16(xmmOut, xmmShift); // preserve transparent pixels in destination
                  xmmIn = vandq_u16(xmmShift, xmmIn); // mask off transparent pixels
                  xmmOut = vorrq_u16(xmmOut, xmmIn); // final output
                  vst1q_u16(usd, xmmOut);
                  usd += 8;
              } // for x
              p += iSize - xCount;
              usd += (iCoinOpPitch/2) - xCount;
          } // for y
          }
          else
#endif // USE_NEON
	{
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = (unsigned short)(c + iColor);
            } // for x
         usd += iCoinOpPitch/2;
         p += iSize;
         } // for y
	}
#endif
      } // if !flipx && !flipy
   if (!bFlipx && bFlipy)
      {
      usd = (unsigned short *)d;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
      }

} /* EMUDrawSprite16D() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawTile16D(int, int, uchar, uchar, bool bool)          *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               No transparency.                                           *
 *                                                                          *
 ****************************************************************************/
void EMUDrawTile16D(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, int iSize)
{
unsigned char *d, *p;
int xCount, yCount; //, iDestPitch;
unsigned int c;
unsigned short *usd;
register int x, y;

/* Adjust for clipped sprite */
   xCount = yCount = iSize; /* Assume full size to draw */
   p = &pSpriteData[iSprite * iSize * iSize];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*iSize);
      else
         p -= (sy*iSize); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-iSize) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-iSize) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
//   iDestPitch = iCoinOpPitch - (xCount*2);

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      usd = (unsigned short *)d;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
      }
   if (bFlipx && !bFlipy)
      {
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p += iSize;
         }
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
      usd = (unsigned short *)d;
#ifdef USE_ARM_ASM
      ARMDrawCoinOpTile16(p, d, xCount, yCount, iColor, iCoinOpPitch, iSize);
#else
#ifdef USE_SSE
        if ((xCount & 7) == 0)
        {
            __m128i xmmIn, xmmColor, xmmZero;
            xmmZero = _mm_setzero_si128();
            xmmColor = _mm_set1_epi16(iColor);
            for (y=0; y<yCount; y++)
            {
                for (x=0; x<xCount; x+=8)
                {
                    xmmIn = _mm_loadl_epi64((__m128i*)p);
                    p += 8;
                    xmmIn = _mm_unpacklo_epi8(xmmIn, xmmZero); // expand to 16-bits
                    xmmIn = _mm_add_epi16(xmmIn, xmmColor); // add the sprite color
                    _mm_storeu_si128((__m128i*)usd, xmmIn);
                    usd += 8;
                } // for x
                p += iSize - xCount;
                usd += (iCoinOpPitch/2) - xCount;
            } // for y
        }
          else
#endif // USE_SSE
#ifdef USE_NEON
        if ((xCount & 7) == 0)
        {
            uint8x8_t xmmIn;
            uint16x8_t xmmIn16, xmmColor;
            xmmColor = vdupq_n_u16(iColor);
            for (y=0; y<yCount; y++)
            {
                for (x=0; x<xCount; x+=8)
                {
                    xmmIn = vld1_u8(p);
                    p += 8;
                    xmmIn16 = vmovl_u8(xmmIn); // expand to 16-bits
                    xmmIn16 = vaddq_u16(xmmIn16, xmmColor); // add the sprite color
                    vst1q_u16(usd, xmmIn16);
                    usd += 8;
                } // for x
                p += iSize - xCount;
                usd += (iCoinOpPitch/2) - xCount;
            } // for y
        }
          else
#endif // USE_NEON
	{
      for (y=0; y<yCount; y++)
         {
             for (x=0; x<xCount; x++)
                {
                c = p[x];
                usd[x] = (unsigned short)(c + iColor);
                } // for x
             usd += iCoinOpPitch/2;
             p += iSize;
         } // for y
	}
#endif
      }
   if (!bFlipx && bFlipy)
      {
      usd = (unsigned short *)d;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            usd[x] = (unsigned short)(c + iColor);
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
      }

} /* EMUDrawTile16D() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent()                                   *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent(int iAddr, int iPitch, int iChar, int iColor, unsigned char *pBitmap, unsigned char *pCharData)
{
unsigned char *p, *d;
#ifndef BOGUS //PORTABLE
int y;
#endif

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;

   p = &pCharData[iChar * 64];
#ifndef BOGUS //PORTABLE
   iColor++; /* Add one since we usually add the bitmap data to the color */
/* Loop through the 8x8 pixel a byte at a time */
   for (y=0; y<8; y++)
      {
      if (p[0])
         d[0] = iColor;
      if (p[1])
         d[1] = iColor;
      if (p[2])
         d[2] = iColor;
      if (p[3])
         d[3] = iColor;
      if (p[4])
         d[4] = iColor;
      if (p[5])
         d[5] = iColor;
      if (p[6])
         d[6] = iColor;
      if (p[7])
         d[7] = iColor;
      p += 8;
      d += iPitch;
      }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  eax,iColor    /* Add the fixed color offset */
        inc  al         /* Add one since we usually add the bitmap data to the color (1bpp) */
        mov  ecx,iPitch
        mov  dl,cTransparent2
        mov  bl,8      /* y count */
drwc0:  mov  bh,8      /* x count */
drwc1:  cmp  [esi],dl      /* Draw color or nothing ? */
        jnz  drwc2      /* yes, draw it */
/* Transparent pixel, skip it (most frequent path */
        inc  esi
        inc  edi
        dec  bh
        jnz  drwc1
        add  edi,ecx /* Skip to next line */
        dec  bl
        jnz  drwc0
        jmp  drwc3
drwc2:  stosb     /* Draw the pixel */
        inc  esi
        dec  bh
        jnz  drwc1
        add  edi,ecx
        dec  bl
        jnz  drwc0
drwc3:  nop
        }
#endif // PORTABLE
} /* EMUDrawCharTransparent() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent2()                                  *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent2(int iAddr, int iPitch, int iChar, int iColor, unsigned char *pCharData)
{
unsigned char yCount, *p, *d;

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;

   p = &pCharData[iChar * 64];
#ifndef BOGUS //PORTABLE
   for (yCount=0; yCount<8; yCount++)
      {
      register unsigned char c;
      c = p[0];
      if (c)
         d[0] = c + iColor;
      c = p[1];
      if (c)
         d[1] = c + iColor;
      c = p[2];
      if (c)
         d[2] = c + iColor;
      c = p[3];
      if (c)
         d[3] = c + iColor;
      c = p[4];
      if (c)
         d[4] = c + iColor;
      c = p[5];
      if (c)
         d[5] = c + iColor;
      c = p[6];
      if (c)
         d[6] = c + iColor;
      c = p[7];
      if (c)
         d[7] = c + iColor;
      p += 8;
      d += iPitch;
      }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  ebx,iColor    /* Get the fixed color offset */
        mov  yCount,8
        mov  ecx,iPitch
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]     /* Draw color or nothing ? */
        inc  esi
        test al,al
        jnz  drwc2      /* yes, draw it */
/* Transparent pixel, skip it (most frequent path */
        inc  edi
        dec  bh
        jnz  drwc1
        add  edi,ecx /* Skip to next line */
        dec  yCount
        jnz  drwc0
        jmp  drwc3
drwc2:  add  al,bl   /* Add to color */
        stosb        /* Draw the pixel */
        dec  bh
        jnz  drwc1
        add  edi,ecx
        dec  yCount
        jnz  drwc0
drwc3:  nop
        }
#endif // PORTABLE
} /* EMUDrawCharTransparent2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent3()                                  *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent3(int iAddr, unsigned char *pColorPROM, int iPitch, int iChar, int iColor, unsigned char *pCharData, int iScroll)
{
unsigned char *p, *d;
#ifndef BOGUS //PORTABLE
int y;
unsigned int c;
#endif

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;
   d += iScroll; /* Add destination scroll offset */

   p = &pCharData[iChar * 64];
#ifndef BOGUS //PORTABLE
   pColorPROM += iColor;
   for (y=0; y<8; y++)
      {
      c = p[0]; /* if not transparent */
      if (c)
         d[0] = pColorPROM[c];
      c = p[1]; /* if not transparent */
      if (c)
         d[1] = pColorPROM[c];
      c = p[2]; /* if not transparent */
      if (c)
         d[2] = pColorPROM[c];
      c = p[3]; /* if not transparent */
      if (c)
         d[3] = pColorPROM[c];
      c = p[4]; /* if not transparent */
      if (c)
         d[4] = pColorPROM[c];
      c = p[5]; /* if not transparent */
      if (c)
         d[5] = pColorPROM[c];
      c = p[6]; /* if not transparent */
      if (c)
         d[6] = pColorPROM[c];
      c = p[7]; /* if not transparent */
      if (c)
         d[7] = pColorPROM[c];
      d += iPitch;
      p += 8;
      }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  edx,pColorPROM
        add  edx,iColor    /* Add the fixed color offset */
        mov  ecx,iPitch
        xor  eax,eax
        mov  bl,8      /* y count */
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]   /* Draw color or nothing ? */
        inc  esi
        mov  al,[edx+eax]  /* Translate color */
        cmp  al,0xff    /* Transparent? */
        jz   drwc2      /* nope, skip it */
        mov  [edi],al  /* Draw the pixel */
drwc2:  inc  edi
        dec  bh
        jnz  drwc1
        add  edi,ecx
        dec  bl
        jnz  drwc0
        }
#endif
} /* EMUDrawCharTransparent3() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent4()                                  *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent4(int iAddr, unsigned char *pColorPROM, int iPitch, int iChar, int iColor, unsigned char *pCharData, int iScroll)
{
unsigned char *p, *d;

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;
   d += iScroll; /* Add destination scroll offset */

   p = &pCharData[iChar * 64];
#ifndef BOGUS // PORTABLE
   {
   int x, y;
   unsigned char c;
   pColorPROM += iColor;
   for (y=0; y<8; y++)
      {
      for (x=0; x<8; x++)
         {
         c = p[x];
         if (c != cTransparent2)
            d[x] = pColorPROM[c];
         }
      d += iPitch;
      p += 8;
      }
   }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  edx,pColorPROM
        add  edx,iColor    /* Add the fixed color offset */
        mov  cl,cTransparent2
        xor  eax,eax
        mov  bl,8      /* y count */
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]   /* Draw color or nothing ? */
        inc  esi
        cmp  al,cl    /* Transparent? */
        jz   drwc2      /* nope, skip it */
        mov  al,[edx+eax]  /* Translate color */
        mov  [edi],al  /* Draw the pixel */
drwc2:  inc  edi
        dec  bh
        jnz  drwc1
        add  edi,iPitch
        dec  bl
        jnz  drwc0
        }
#endif // PORTABLE
} /* EMUDrawCharTransparent4() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent5()                                  *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *               colors are mapped directly                                 *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent5(int iAddr, int iPitch, int iChar, int iColor, unsigned char *pCharData, int iScroll)
{
unsigned char *p, *d;

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;
   d += iScroll; /* Add destination scroll offset */

   p = &pCharData[iChar * 64];
#ifndef BOGUS // PORTABLE
   {
   int x, y;
   unsigned char c;
   for (y=0; y<8; y++)
      {
      for (x=0; x<8; x++)
         {
         c = p[x];
         if (c != cTransparent2)
            d[x] = c + iColor;
         }
      d += iPitch;
      p += 8;
      }
   }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  edx,iColor    /* Add the fixed color offset */
        mov  cl,cTransparent2
        xor  eax,eax
        mov  bl,8      /* y count */
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]   /* Draw color or nothing ? */
        inc  esi
        cmp  al,cl    /* Transparent? */
        jz   drwc2      /* nope, skip it */
        add  al,dl    /* Translate color */
        mov  [edi],al  /* Draw the pixel */
drwc2:  inc  edi
        dec  bh
        jnz  drwc1
        add  edi,iPitch
        dec  bl
        jnz  drwc0
        }
#endif
} /* EMUDrawCharTransparent5() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharTransparent7()                                  *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *               transparency is determined after color translation         *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharTransparent7(int iAddr, unsigned char *pColorPROM, int iPitch, int iChar, int iColor, unsigned char *pCharData)
{
unsigned char *p, *d;

   d = (unsigned char *)lCharAddr[iAddr];
   if (!d) /* Non-visible stuff */
      return;

   p = &pCharData[iChar * 64];
#ifndef BOGUS // PORTABLE
   {
   int x, y;
   unsigned char c;
   pColorPROM += iColor;
   for (y=0; y<8; y++)
      {
      for (x=0; x<8; x++)
         {
         c = pColorPROM[p[x]];
         if (c != cTransparent2)
            d[x] = c;
         }
      d += iPitch;
      p += 8;
      }
   }
#else
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        mov  edx,pColorPROM
        add  edx,iColor    /* Add the fixed color offset */
        mov  cl,cTransparent2
        xor  eax,eax
        mov  bl,8      /* y count */
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]   /* Draw color or nothing ? */
        inc  esi
        mov  al,[edx+eax]  /* Translate color */
        cmp  al,cl    /* Transparent? */
        jz   drwc2      /* nope, skip it */
        mov  [edi],al  /* Draw the pixel */
drwc2:  inc  edi
        dec  bh
        jnz  drwc1
        add  edi,iPitch
        dec  bl
        jnz  drwc0
        }
#endif
} /* EMUDrawCharTransparent7() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSpriteFast(int, int, int, int, char *, char *, int) *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               No clipping, no flipping, no transparency, no nonsense     *
 *               pure speed!                                                *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSpriteFast(int sx, int sy, int iSprite, int iColor, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2)
{
unsigned char *s, *d;

   d = (unsigned char *)&pScreen[sx+sy*iPitch2]; /* Destination address */
   s = &pSpriteData[iSprite * 256]; /* Source address */

#ifndef BOGUS //PORTABLE
   {
   int x, y;
   uint32_t *uls, *uld, ulColor, ul;
   ulColor = iColor | (iColor << 8) | (iColor << 16) | (iColor << 24);
   uls = (uint32_t *)s;
   uld = (uint32_t *)d;
   for (y=0; y<16; y++) // need to write 1 pixel at a time because of memory boundary issues on RISC CPUs
      {
      if ((unsigned long)d & 3) // destination address falls on an odd dword boundary
         {
         for (x=0; x<16; x+=4)
            {
            ul = (*uls++) + ulColor; // gain a slight speed advantage by reading the source data as DWORDs
            d[0] = (char)ul;
            d[1] = (char)(ul >> 8);
            d[2] = (char)(ul >> 16);
            d[3] = (char)(ul >> 24);
            d += 4;
            }
         d += iPitch2-16;
         }
      else // we can use faster dword access on both read and write
         {
         uld[0] = uls[0] + ulColor;
         uld[1] = uls[1] + ulColor;
         uld[2] = uls[2] + ulColor;
         uld[3] = uls[3] + ulColor;
         uld += iPitch2/4;
         uls += 4;
         }
      }
   }
#else
   _asm {
        push ebp
        mov  esi,s
        mov  ebx,iColor /* Get the character color offset */
        mov  edi,d
        mov  bh,bl      /* distribute the color to all 4 bytes */
        shl  ebx,16
        mov  edx,16     /* Number of lines */
        mov  bl,byte ptr iColor
        mov  ebp,iPitch2
        mov  bh,bl
        sub  ebp,8
dsprf:  mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax
        mov  [edi+4],ecx
        add  edi,8
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx
        add  ecx,ebx
        mov  [edi],eax
        mov  [edi+4],ecx
        add  edi,ebp
        dec  edx
        jnz  dsprf
        pop  ebp
        }
#endif
} /* EMUDrawSpriteFast() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharBlack(int, int, unsigned char *)                *
 *                                                                          *
 *  PURPOSE    : Draw a quick black character.                              *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharBlack(int iAddr, int iPitch2, uint32_t ulBlack) 
{
unsigned char *d;
#ifndef BOGUS // PORTABLE
uint32_t *dl, lPitch;
#endif

    d = (unsigned char *)lCharAddr[iAddr]; /* Screen address of this character */
    if (d)
       {
#ifndef BOGUS // PORTABLE
       dl = (uint32_t *)d;
       lPitch = iPitch2 / 4;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
       dl += lPitch;
       dl[0] = dl[1] = 0;
#else
       _asm {
            mov edi,d
            mov ebx,iPitch2
            mov eax,ulBlack
            mov [edi],eax  // 0
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 1
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 2
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 3
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 4
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 5
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 6
            mov 4[edi],eax
            add edi,ebx
            mov [edi],eax  // 7
            mov 4[edi],eax
            }
#endif
       }

} /* EMUDrawCharBlack() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharFast(unsigned short, unsigned char *, int)      *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharFast(int iAddr, int iPitch, int iChar, int iColor, unsigned long *pCharAddrs, unsigned char *pCharData)
{
unsigned char *p, *d;
#ifndef BOGUS // PORTABLE
uint32_t *s, *dl, ulColor, lPitch;
#endif

   d = (unsigned char *)pCharAddrs[iAddr]; /* Get the screen address directly */
   if (!d)
      return;
   p = &pCharData[iChar << 6];

#ifndef BOGUS // PORTABLE
   ulColor = (iColor + 16) & 0xff; /* Put color offset in all 4 bytes */
   ulColor = ulColor | (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
   lPitch = iPitch / 4; /* in terms of DWORDS */
   s = (uint32_t *)p;
   dl = (uint32_t *)d;
   /* Unroll the loop for maximum speed */
//
// NOTE on the MIPS 4121, using s[0] & s[1] is faster than *s++
// because it becomes 2 add rx,4 instead of one add rx,8
//
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
   dl += lPitch;
   s += 2;
   dl[0] = ulColor + s[0];
   dl[1] = ulColor + s[1];
#else
   _asm {
        mov  esi,p
        mov  edi,d
        mov  ebx,iColor /* Get the character color offset */
        add  bl,10h     /* Add offset of 16 for identity palette */
        mov  bh,bl      /* Put in all 4 bytes of the register */
        shl  ebx,8
        mov  bl,bh
        shl  ebx,8
        mov  bl,bh
        mov  edx,iPitch /* Keep offset to next line in a register */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* First scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Second scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Third scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Forth scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Fifth scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Sixth scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  esi,8
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Seventh scanline */
        mov  [edi+4],ecx
        add  edi,edx    /* Point to next line */
        mov  eax,[esi]
        mov  ecx,[esi+4]
        add  eax,ebx    /* Add color offset */
        add  ecx,ebx
        mov  [edi],eax  /* Eighth scanline */
        mov  [edi+4],ecx
        }
#endif // PORTABLE
} /* EMUDrawCharFast() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawCharXY(unsigned short, unsigned char *, int)        *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawCharXY(int sx, int sy, int iPitch, int iChar, int iColor, unsigned char *pBitmap, unsigned char *pCharData)
{
unsigned char *p, *d;
int x, y;

   d = &pBitmap[sx + sy * iPitch];
   p = &pCharData[iChar << 6];
   for (y=0; y<8; y++)
      {
      for (x=0; x<8; x++)
         {
         d[x] = p[x] + iColor;
         }
      d += iPitch;
      p += 8;
      }

} /* EMUDrawCharXY() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawChar(unsigned short, unsigned char *, int)          *
 *                                                                          *
 *  PURPOSE    : Draw the bitmapped character at the given position.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawChar(int iAddr, unsigned char *cColorPROM, int iPitch, int iChar, int iColor, unsigned long *ulCharAddr, unsigned char *pCharData)
{
//int x, y;
unsigned char *p, *d;

//   x = iCharX[iAddr];   /* Translate address offset into x,y coordinate */
//   y = iCharY[iAddr];
//   if (x < 0 || y < 0) /* Non-visible stuff */
//      return;
   d = (unsigned char *)ulCharAddr[iAddr];
   if (d == NULL) /* Non-visible character */
      return;
//   d = &pCoinOpBitmap[y * iPitch + x];
   p = &pCharData[iChar * 64];
#ifndef BOGUS // PORTABLE
   {
   register int x,y;
   cColorPROM += iColor;
   for (y=0; y<8; y++)
      {
      for (x=0; x<8; x+=4)
         {
         d[0] = cColorPROM[p[0]];
         d[1] = cColorPROM[p[1]];
         d[2] = cColorPROM[p[2]];
         d[3] = cColorPROM[p[3]];
         d += 4;
         p += 4;
         }
      d += iPitch-8;
      }
   }
#else /* Do it the RIGHT way */
   iPitch -= 8; /* Use to advance to next line in asm code */
   _asm {
        mov  esi,p
        mov  edi,d
        dec  edi        /* Start back by 1 for better instruction interleave */
        mov  edx,cColorPROM  /* color lookup table */
        add  edx,iColor    /* Add fixed color offset */
        mov  ecx,iPitch
        mov  bl,8      /* y count */
        xor  eax,eax
drwc0:  mov  bh,8      /* x count */
drwc1:  mov  al,[esi]
        inc  edi
        inc  esi
        mov  al,[edx+eax]  /* translate the color */
        dec  bh
        mov  [edi],al
        jnz  drwc1
        add  edi,ecx   /* Skip to next line */
        dec  bl
        jnz  drwc0
        }
#endif /* PORTABLE */

} /* EMUDrawChar() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite(int, int, uchar, uchar, bool bool)           *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/

void EMUDrawSprite(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount; //, iDestPitch;
#ifndef BOGUS //PORTABLE
unsigned int c;
register int x, y;
#else
int iSrcPitch;
#endif

/* Adjust for clipped sprite */
   xCount = yCount = 16; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-16) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      p += 15*16;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[15-x];
            if (c != cTransparent)
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p -= 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - 16;
    _asm {
         mov  esi,p
         add  esi,15*16+15  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         mov  ecx,iDestPitch
         xor  eax,eax
drwsfxy0: mov  bl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfxy0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfxy0: dec  bl
         jnz  drwsfxy1
         add  edi,ecx
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[15-x];
            if (c != cTransparent)
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p += 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15  /* Start from opposite direction right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfx0: mov  ecx,xCount
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfx0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfx0: dec  ecx
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (c != cTransparent)
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p += 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drws0:   mov  ecx,xCount
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwt0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwt0:   dec  ecx
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS //PORTABLE
      pColorPROM += iColor;
      p += 15*16;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (c != cTransparent)
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p -= 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15*16  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfy0: mov  ecx,xCount
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwtfy0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfy0: dec  ecx
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSprite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSpriteTMI(int, int, uchar, uchar, bool bool)        *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency mask determines which colors are trans.   *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSpriteTMI(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pColorPROM, int icx, int icy)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned char c;
register int x, y;
#else
int iSrcPitch, iOffset;
#endif

/* Adjust for clipped sprite */
   xCount = icx;
   yCount = icy; /* Assume full size to draw */
   p = &pSpriteData[iSprite * icx*icy];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*icx);
      else
         p -= (sy*icx); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-icy) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-icx) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      p += (icy-1)*icx;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p -= icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - icx;
   iOffset = (icy-1)*icx + icx-1;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfxy0: mov  ecx,xCount
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         bt   ebx,al
         jc   drwtfxy0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfxy0: dec  ecx
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p += icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx + xCount;
    _asm {
         mov  esi,p
         add  esi,icx  /* Start from opposite direction right */
         dec  esi
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfx0: mov  ecx,xCount
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         bt   ebx,al
         jc   drwtfx0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfx0: dec  ecx
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p += icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drws0:   mov  ecx,xCount
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         bt   ebx,al
         jc   drwt0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwt0:   dec  ecx
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      p += (icy-1)*icx;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               d[x] = pColorPROM[c];
            }
         d += iCoinOpPitch;
         p -= icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx + xCount;
   iOffset = (icy-1)*icx;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfy0: mov  ecx,xCount
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         bt   ebx,al
         jc   drwtfy0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfy0: dec  ecx
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSpriteTMI() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSpriteTM(int, int, uchar, uchar, bool bool)         *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency mask determines which colors are trans.   *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSpriteTM(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, int icx, int icy)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned char c;
register int x, y;
#else
int iSrcPitch, iOffset;
#endif

/* Adjust for clipped sprite */
   xCount = icx;
   yCount = icy; /* Assume full size to draw */
   p = &pSpriteData[iSprite * icx*icy];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*icx);
      else
         p -= (sy*icx); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-icy) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-icx) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      p += (icy-1)*icx;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               d[x] = iColor + c;
            }
         d += iCoinOpPitch;
         p -= icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - icx;
   iOffset = (icy-1)*icx + icx-1;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,iColor   /* get fixed color offset */
         xor  eax,eax
drwsfxy0: mov  ecx,xCount
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         bt   ebx,al
         jc   drwtfxy0    /* transparent, don't draw it */
         add  al,dl
         mov  [edi],al
drwtfxy0: dec  ecx
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               d[x] = iColor + c;
            }
         d += iCoinOpPitch;
         p += icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx + xCount;
    _asm {
         mov  esi,p
         add  esi,icx  /* Start from opposite direction right */
         dec  esi
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,iColor   /* Get fixed color offset */
         xor  eax,eax
drwsfx0: mov  ecx,xCount
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         bt   ebx,al
         jc   drwtfx0    /* transparent, don't draw it */
         add  al,dl
         mov  [edi],al
drwtfx0: dec  ecx
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               d[x] = iColor + c;
            }
         d += iCoinOpPitch;
         p += icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,iColor   /* Get fixed color offset */
         xor  eax,eax
drws0:   mov  ecx,xCount
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         bt   ebx,al
         jc   drwt0    /* transparent, don't draw it */
         add  al,dl
         mov  [edi],al
drwt0:   dec  ecx
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      p += (icy-1)*icx;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               d[x] = iColor + c;
            }
         d += iCoinOpPitch;
         p -= icx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = icx + xCount;
   iOffset = (icy-1)*icx;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,iColor   /* Get fixed color offset */
         xor  eax,eax
drwsfy0: mov  ecx,xCount
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         bt   ebx,al
         jc   drwtfy0    /* transparent, don't draw it */
         add  al,dl
         mov  [edi],al
drwtfy0: dec  ecx
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSpriteTM() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSpriteTMP(int, int, uchar, uchar, bool bool)        *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency mask determines which colors are          *
 *               transparent and which colors have priority.                *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSpriteTMP(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pPriBitmap, int icx, int icy)
{
unsigned char *d, *dp, *p;
int xCount, yCount; //, iDestPitch;
unsigned char c;
register int x, y;

/* Adjust for clipped sprite */
   xCount = icx;
   yCount = icy; /* Assume full size to draw */
   p = &pSpriteData[iSprite * icx*icy];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*icx);
      else
         p -= (sy*icx); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-icy) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-icx) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
   dp = &pPriBitmap[sy*iCoinOpPitch+sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      p += (icy-1)*icx;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               {
               if (!(ulPriorityMask & (1 << dp[x])))
                  d[x] = iColor + c;
               dp[x] = 31; // mark this as high priority so it will not be overdrawn
               }
            }
         d += iCoinOpPitch;
         dp += iCoinOpPitch;
         p -= icx;
         }
      }
   if (bFlipx && !bFlipy)
      {
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[icx-1-x];
            if (!(ulTransMask & (1<<c)))
               {
               if (!(ulPriorityMask & (1 << dp[x])))
                  d[x] = iColor + c;
               dp[x] = 31; // mark this as high priority so it will not be overdrawn
               }
            }
         d += iCoinOpPitch;
         dp += iCoinOpPitch;
         p += icx;
         }
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               {
               if (!(ulPriorityMask & (1 << dp[x])))
                  d[x] = iColor + c;
               dp[x] = 31; // mark this as high priority so it will not be overdrawn
               }
            }
         d += iCoinOpPitch;
         dp += iCoinOpPitch;
         p += icx;
         }
      }
   if (!bFlipx && bFlipy)
      {
      p += (icy-1)*icx;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               {
               if (!(ulPriorityMask & (1 << dp[x])))
                  d[x] = iColor + c;
               dp[x] = 31; // mark this as high priority so it will not be overdrawn
               }
            }
         d += iCoinOpPitch;
         dp += iCoinOpPitch;
         p -= icx;
         }
      }

} /* EMUDrawSpriteTMP() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite16S(int, int, uchar, uchar, bool bool)        *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with line scrolling       *
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite16S(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM, uint32_t *pScroll, int iScroll)
{
unsigned char *p;
unsigned short *d;
int xCount, yCount, iDestPitch, iSrcPitch;
unsigned char c;
register int x, y, xOff;

/* Adjust for clipped sprite */
   xCount = yCount = 16; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

//   if (sx < 0)
//      {
//      xCount += sx; /* Shrink width to draw */
//      if (bFlipx)
//         p += sx;
//      else
//         p -= sx; /* Adjust sprite pointer */
//      sx = 0; /* Start at 0 */
//      }
//   else
//   if (sx > iSpriteLimitX-16) /* Part of it is off the right edge */
//      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = (unsigned short *)&pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
   iDestPitch = iCoinOpPitch/2;

   if (bFlipy)
      {
      p += 15*16; // point to bottom of sprite
      iSrcPitch = -16; // next line offset
      }
   else
      iSrcPitch = 16;

   pColorPROM += iColor;
   if (bFlipx)
      {
      for (y=0; y<yCount; y++)
         {
         xOff = pScroll[y];
         xOff -= iScroll;
         for (x=0; x<xCount; x++)
            {
            c = p[15-x];
            if (!(ulTransMask & (1 << c)))
               {
               d[x-xOff] = pColorPROM[c];
               }
            } // for x
         p += iSrcPitch;
         d += iDestPitch;
         } // for y
      }
   else
      {
      for (y=0; y<yCount; y++)
         {
         xOff = pScroll[y];
         xOff -= iScroll;
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1 << c)))
               {
               d[x-xOff] = pColorPROM[c];
               }
            } // for x
         p += iSrcPitch;
         d += iDestPitch;
         } // for y
      }

} /* EMUDrawSprite16S() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite16(int, int, uchar, uchar, bool bool)         *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite16(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM, int iSize)
{
unsigned char *d, *p;
int xCount, yCount; //, iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned char c;
unsigned short *usd;
register int x, y;
#else
int iSrcPitch, iDelta;
#endif

/* Adjust for clipped sprite */
   xCount = yCount = iSize; /* Assume full size to draw */
   p = &pSpriteData[iSprite * iSize * iSize];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*iSize);
      else
         p -= (sy*iSize); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-iSize) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-iSize) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
//   iDestPitch = iCoinOpPitch - (xCount*2);

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - iSize;
   iDelta = (iSize*iSize)-1;
    _asm {
         mov  esi,p
         add  esi,iDelta  /* Start from bottom right */
         mov  edi,d
         sub  edi,2      /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor  /* Times two to index words */
//         mov  ecx,iDestPitch
         xor  eax,eax
drwsfxy0: mov  cl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfxy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfxy0: dec  cl
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[iSize-1-x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         p += iSize;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = iSize + xCount;
    _asm {
         mov  esi,p
         add  esi,iSize  /* Start from opposite direction right */
         dec  esi
         mov  edi,d
         sub  edi,2      /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor /* times two to index words */
         xor  eax,eax
drwsfx0: mov  cl,byte ptr xCount
drwsfx1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfx0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfx0: dec  cl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         p += iSize;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = iSize - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         sub  edi,2     /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor  /* Times two to index words */
         xor  eax,eax
drws0:   mov  cl,byte ptr xCount
drws1:   mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwt0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwt0:   dec  cl
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS //PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      p += iSize*(iSize-1);
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (!(ulTransMask & (1<<c)))
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         p -= iSize;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = iSize + xCount;
   iDelta = (iSize-1)*iSize;
    _asm {
         mov  esi,p
         add  esi,iDelta  /* Start from bottom */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor  /* Times two to index words */
         xor  eax,eax
drwsfy0: mov  cl,byte ptr xCount
drwsfy1: mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwtfy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfy0: dec  cl
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSprite16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawScaledSprite16(int, int, uchar, uchar, bool bool)   *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawScaledSprite16(int sx, int sy, int iScaleX, int iScaleY, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM)
{
unsigned char c, *p, *pSrc;
unsigned short *d, *pColors;
int x, y, xCount, yCount, iDestPitch;
int iSumX, iSumY, iFracX, iFracY;

/* Adjust for clipped sprite */
   xCount = iScaleX; /* Assume full size to draw */
   yCount = iScaleY;
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > iSpriteLimitX-16) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = (unsigned short *)&pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
   iDestPitch = iCoinOpPitch/2; // short pointer
   iSumX = iSumY = 0;
   iFracX = 0x1000 / iScaleX;
   iFracY = 0x1000 / iScaleY;
   pColors = &pColorPROM[iColor];

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      for (y=0; y<yCount; y++)
         {
         iSumX = 0;
         pSrc = p + 16*(15-(iSumY >> 8)); // point to correct source line
         for (x=0; x<xCount; x++)
            {
            c = pSrc[15-(iSumX >> 8)];
            if (c != cTransparent)
               d[x] = pColors[c];
            iSumX += iFracX;
            }
         iSumY += iFracY;
         d += iDestPitch;
         }
      }
   if (bFlipx && !bFlipy)
      {
      for (y=0; y<yCount; y++)
         {
         iSumX = 0;
         pSrc = p + 16*(iSumY >> 8); // point to correct source line
         for (x=0; x<xCount; x++)
            {
            c = pSrc[15-(iSumX >> 8)];
            if (c != cTransparent)
               d[x] = pColors[c];
            iSumX += iFracX;
            }
         iSumY += iFracY;
         d += iDestPitch;
         }
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
      for (y=0; y<yCount; y++)
         {
         iSumX = 0;
         pSrc = p + 16*(iSumY >> 8); // point to correct source line
         for (x=0; x<xCount; x++)
            {
            c = pSrc[iSumX >> 8];
            if (c != cTransparent)
               d[x] = pColors[c];
            iSumX += iFracX;
            }
         iSumY += iFracY;
         d += iDestPitch;
         }
      }
   if (!bFlipx && bFlipy)
      {
      for (y=0; y<yCount; y++)
         {
         iSumX = 0;
         pSrc = p + 16*(15-(iSumY >> 8)); // point to correct source line
         for (x=0; x<xCount; x++)
            {
            c = pSrc[iSumX >> 8];
            if (c != cTransparent)
               d[x] = pColors[c];
            iSumX += iFracX;
            }
         iSumY += iFracY;
         d += iDestPitch;
         }
      }

} /* EMUDrawScaledSprite16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSmallSprite16(int, int, uchar, uchar, bool bool)    *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSmallSprite16(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned char c;
register int x, y;
unsigned short *usd;
#else
int iSrcPitch;
#endif

/* Adjust for clipped sprite */
   xCount = yCount = 8; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 64];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*8);
      else
         p -= (sy*8); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-8) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-8) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
//   iDestPitch = iCoinOpPitch - xCount*2;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[(15-y)*16+(15-x)];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - 16;
    _asm {
         mov  esi,p
         add  esi,7*8+7  /* Start from bottom right */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
//         mov  ecx,iDestPitch
         xor  eax,eax
drwsfxy0: mov  cl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfxy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfxy0: dec  cl
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[y*16+(15-x)];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 8 + xCount;
    _asm {
         mov  esi,p
         add  esi,7  /* Start from opposite direction right */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drwsfx0: mov  cl,byte ptr xCount
drwsfx1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfx0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfx0: dec  cl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[y*16+x];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 8 - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drws0:   mov  cl,byte ptr xCount
drws1:   mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwt0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwt0:   dec  cl
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      usd = (unsigned short *)d;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[(15-y)*16+x];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 8 + xCount;
    _asm {
         mov  esi,p
         add  esi,7*8  /* Start from bottom */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drwsfy0: mov  cl,byte ptr xCount
drwsfy1: mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwtfy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfy0: dec  cl
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSmallSprite16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawBigSprite16(int, int, uchar, uchar, bool bool)      *
 *                                                                          *
 *  PURPOSE    : Draw an individual 16-bit sprite with the given attributes.*
 *               The transparency color is in the sprite image data.        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawBigSprite16(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned char c;
unsigned short *usd;
register int x, y;
#else
int iSrcPitch;
#endif

/* Adjust for clipped sprite */
   xCount = yCount = 32; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 1024];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*32);
      else
         p -= (sy*32); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-32) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-32) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx*2];
//   iDestPitch = iCoinOpPitch - xCount*2;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[(15-y)*16+(15-x)];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - 16;
    _asm {
         mov  esi,p
         add  esi,31*32+31  /* Start from bottom right */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
//         mov  ecx,iDestPitch
         xor  eax,eax
drwsfxy0: mov  cl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfxy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfxy0: dec  cl
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[y*16+(15-x)];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 32 + xCount;
    _asm {
         mov  esi,p
         add  esi,31  /* Start from opposite direction right */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drwsfx0: mov  cl,byte ptr xCount
drwsfx1: mov  al,[esi]
         add  edi,2
         dec  esi
         bt   ebx,al
         jc   drwtfx0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfx0: dec  cl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[y*16+x];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 32 - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drws0:   mov  cl,byte ptr xCount
drws1:   mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwt0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwt0:   dec  cl
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[(15-y)*16+x];
            if (c != cTransparent)
               usd[x] = pColorPROM[c];
            }
         usd += iCoinOpPitch/2;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 32 + xCount;
    _asm {
         mov  esi,p
         add  esi,31*32  /* Start from bottom */
         mov  edi,d
         sub  edi,2       /* Adjust for pentium optimization */
         mov  ebx,ulTransMask
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
drwsfy0: mov  ecx,xCount
drwsfy1: mov  al,[esi]
         add  edi,2
         inc  esi
         bt   ebx,al
         jc   drwtfy0    /* transparent, don't draw it */
         mov  eax,[edx+eax*2]
         mov  [edi],al
         mov  [edi+1],ah
         xor  eax,eax
drwtfy0: dec  cl
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawBigSprite16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawTile16(int, int, uchar, uchar, bool bool)           *
 *                                                                          *
 *  PURPOSE    : Draw an individual tile with the given attributes.         *
 *                                                                          *
 ****************************************************************************/
void EMUDrawTile16(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned short *pColorPROM, unsigned char *pTilemap, int iTilePitch, int iSize)
{
unsigned char *d, *p;
//int iDestPitch;
#ifndef BOGUS // PORTABLE
unsigned short *usd;
unsigned int c;
register int x, y;
#else
int iOffset;
#endif

   d = &pTilemap[sy*iTilePitch + sx*2];
   p = &pTileData[iTile*iSize*iSize];
//   iDestPitch = iTilePitch - (iSize*2);

   if (sx <= -iSize || sy <= -iSize || sx >= iSpriteLimitX || sy >= iSpriteLimitY)
	return;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      p += (iSize-1)*iSize;
      for (y=0; y<iSize; y++)
         {
         for (x=0; x<iSize; x++)
            {
            c = p[iSize-1-x];
            usd[x] = pColorPROM[c];
            }
         p -= iSize;
         usd += iTilePitch/2;
         }
#else
   /* Use this to speed up asm code */
   iOffset = (iSize-1)*iSize+(iSize-1);
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
         mov  bh,byte ptr iSize     // ycount
drwsfxy0: mov  bl,byte ptr iSize    // xcount
drwsfxy1: mov  al,[esi]
         dec  esi
         mov  ecx,[edx+eax*2]
         mov  [edi],cl
         mov  [edi+1],ch // 16-bit pixel
         add  edi,2
         dec  bl
         jnz  drwsfxy1
         add  edi,iDestPitch
         dec  bh
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<iSize; y++)
         {
         for (x=0; x<iSize; x++)
            {
            c = p[iSize-1-x];
            usd[x] = pColorPROM[c];
            }
         p += iSize;
         usd += iTilePitch/2;
         }
#else
    _asm {
         mov  esi,p
         add  esi,iSize  /* Start from opposite direction right */
         dec  esi
         mov  edi,d
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
         mov  bh,byte ptr iSize
drwsfx0: mov  bl,byte ptr iSize
drwsfx1: mov  al,[esi]
         dec  esi
         mov  ecx,[edx+eax*2]
         mov  [edi],cl
         mov  [edi+1],ch
         add  edi,2
         dec  bl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSize
         add  esi,iSize
         dec  bh
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      for (y=0; y<iSize; y++)
         {
         for (x=0; x<iSize; x++)
            {
            c = p[x];
            usd[x] = pColorPROM[c];
            }
         p += iSize;
         usd += iTilePitch/2;
         }
#else
    _asm {
         mov  esi,p
         mov  edi,d
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
         mov  bh,byte ptr iSize
drws0:   mov  bl,byte ptr iSize
drws1:   mov  al,[esi]
         inc  esi
         mov  ecx,[edx+eax*2]
         mov  [edi],cl
         mov  [edi+1],ch
         add  edi,2
         dec  bl
         jnz  drws1
         add  edi,iDestPitch
         dec  bh
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS //PORTABLE
      usd = (unsigned short *)d;
      pColorPROM += iColor;
      p += (iSize-1)*iSize;
      for (y=0; y<iSize; y++)
         {
         for (x=0; x<iSize; x++)
            {
            c = p[x];
            usd[x] = pColorPROM[c];
            }
         p -= iSize;
         usd += iTilePitch/2;
         }
#else
      iOffset = (iSize-1)*iSize;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         add  edx,iColor
         xor  eax,eax
         mov  bh,byte ptr iSize
drwsfy0: mov  bl,byte ptr iSize
drwsfy1: mov  al,[esi]
         inc  esi
         mov  ecx,[edx+eax*2]
         mov  [edi],cl
         mov  [edi+1],ch
         add  edi,2
         dec  bl
         jnz  drwsfy1
         sub  esi,iSize
         sub  esi,iSize
         add  edi,iDestPitch
         dec  bh
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawTile16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawGraphicI(int, int, uchar, uchar, bool bool)         *
 *                                                                          *
 *  PURPOSE    : Draw an individual tile with indirect colors (no trans).   *
 *                                                                          *
 ****************************************************************************/
void EMUDrawGraphicI(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned char *pColorPROM, unsigned char *pTilemap, int iTilePitch, int xCount, int yCount)
{
unsigned char *d, *p;
int iOffset; //iDestPitch;
#ifndef BOGUS // PORTABLE
int x, y;
uint32_t ul;
#endif
   d = &pTilemap[sy*iTilePitch + sx];
   p = &pTileData[iTile*xCount*yCount];
//   iDestPitch = iTilePitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      iOffset = (yCount-1)*xCount + (xCount-1);
   /* Use this to speed up asm code */
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      p += (yCount-1)*xCount;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            d[xCount-1-x] = pColorPROM[p[x]];
            }
         d += iTilePitch;
         p -= xCount;
         }
#else
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         mov  edx,pColorPROM
         mov  ecx,iDestPitch
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
         mov  bh,byte ptr yCount
drwsfxy0: mov  bl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         dec  esi
         mov  al,[edx+eax]
         mov  [edi],al
         inc  edi
         dec  bl
         jnz  drwsfxy1
         add  edi,ecx
         dec  bh
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
      iOffset = xCount-1;
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            d[xCount-1-x] = pColorPROM[p[x]];
            }
         d += iTilePitch;
         p += xCount;
         }
#else
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from opposite direction right */
         mov  edi,d
         mov  edx,pColorPROM
         mov  ecx,iDestPitch
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
         mov  bh,byte ptr yCount
drwsfx0: mov  bl,byte ptr xCount
drwsfx1: mov  al,[esi]
         dec  esi
         mov  al,[edx+eax]
         mov  [edi],al
         inc  edi
         dec  bl
         jnz  drwsfx1
         add  edi,ecx
         add  esi,xCount
         add  esi,xCount
         dec  bh
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x+=4)
            {
            ul = *(uint32_t *)p;
            d[0] = pColorPROM[ul & 0xff];
            d[1] = pColorPROM[(ul>>8) & 0xff];
            d[2] = pColorPROM[(ul>>16) & 0xff];
            d[3] = pColorPROM[(ul>>24)];
            d += 4;
            p += 4;
            }
         d += iTilePitch - xCount;
         }
#else
    _asm {
         mov  esi,p
         mov  edi,d
         mov  edx,pColorPROM
         mov  ecx,iDestPitch
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
         mov  bh,byte ptr yCount
drws0:   mov  bl,byte ptr xCount
drws1:   mov  al,[esi]
         inc  esi
         mov  al,[edx+eax]
         mov  [edi],al
         inc  edi
         dec  bl
         jnz  drws1
         add  edi,ecx
         dec  bh
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
      iOffset = (yCount-1)*xCount;
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      p += iOffset;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x+=4)
            {
            ul = *(uint32_t *)p;
            d[0] = pColorPROM[ul & 0xff];
            d[1] = pColorPROM[(ul>>8) & 0xff];
            d[2] = pColorPROM[(ul>>16) & 0xff];
            d[3] = pColorPROM[(ul>>24)];
            d += 4;
            p += 4;
            }
         d += iTilePitch - xCount;
         p -= xCount*2;
         }
#else
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         mov  edx,pColorPROM
         mov  ecx,iDestPitch
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
         mov  bh,byte ptr yCount
drwsfy0: mov  bl,byte ptr xCount
drwsfy1: mov  al,[esi]
         inc  esi
         mov  al,[edx+eax]
         mov  [edi],al
         inc  edi
         dec  bl
         jnz  drwsfy1
         sub  esi,xCount
         sub  esi,xCount
         add  edi,ecx
         dec  bh
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawGraphicI() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSpriteTransparent(int, int, uchar, uchar, bool bool)           *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The background color has priority over the sprite.         *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSpriteTransparent(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
int x, y;
unsigned char c;
#else
int iSrcPitch;
#endif
/* Adjust for clipped sprite */
   xCount = yCount = 16; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   else
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */

   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   else
   if (sx > (iSpriteLimitX-16)) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */

   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      p += 15*16;
      for (y=0; y<16; y++)
         {
         for (x=0; x<16; x++)
            {
            c = p[x];
            if (c)
               {
               if (d[15-x] == 0x20) // only store if destination is black
                  d[15-x] = pColorPROM[c];
               }
            }
         d += iCoinOpPitch;
         p -= 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - 16;
    _asm {
         mov  esi,p
         add  esi,15*16+15  /* Start from bottom right */
         mov  edi,d
         mov  bh,0x20
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         mov  ecx,iDestPitch
         xor  eax,eax
drwsfxy0: mov  bl,byte ptr xCount
drwsfxy1: mov  al,[esi]
         dec  esi
         test al,al     /* If black, don't even test destination */
         jz   drwtfxy0
         cmp  byte ptr [edi],bh    /* Needs to be black */
         jnz  drwtfxy0   /* has background, don't draw */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfxy0: inc  edi
         dec  bl
         jnz  drwsfxy1
         add  edi,ecx
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      for (y=0; y<16; y++)
         {
         for (x=0; x<16; x++)
            {
            c = p[x];
            if (c)
               {
               if (d[15-x] == 0x20) // only store if destination is black
                  d[15-x] = pColorPROM[c];
               }
            }
         d += iCoinOpPitch;
         p += 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15  /* Start from opposite direction right */
         mov  edi,d
         mov  bh,0x20
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfx0: mov  ecx,xCount
drwsfx1: mov  al,[esi]
         dec  esi
         test al,al     /* If black, don't even test destination */
         jz   drwtfx0
         cmp  byte ptr [edi],bh    /* Needs to be black */
         jnz  drwtfx0
         mov  al,[edx+eax]
         mov  [edi],al
drwtfx0: inc  edi
         dec  ecx
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS //PORTABLE
      pColorPROM += iColor;
      for (y=0; y<16; y++)
         {
         for (x=0; x<16; x++)
            {
            c = p[x];
            if (c)
               {
               if (d[x] == 0x20) // only store if destination is black
                  d[x] = pColorPROM[c];
               }
            }
         d += iCoinOpPitch;
         p += 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         mov  bh,0x20
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drws0:   mov  ecx,xCount
drws1:   mov  al,[esi]
         inc  esi
         test al,al     /* If black, don't even test destination */
         jz   drwt0
         cmp  byte ptr [edi],bh    /* Needs to be black */
         jnz  drwt0     /* Background color, don't draw */
         mov  al,[edx+eax]
         mov  [edi],al
drwt0:   inc  edi
         dec  ecx
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      pColorPROM += iColor;
      p += 15*16;
      for (y=0; y<16; y++)
         {
         for (x=0; x<16; x++)
            {
            c = p[x];
            if (c)
               {
               if (d[x] == 0x20) // only store if destination is black
                  d[x] = pColorPROM[c];
               }
            }
         d += iCoinOpPitch;
         p -= 16;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15*16  /* Start from bottom */
         mov  edi,d
         mov  bh,0x20
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfy0: mov  ecx,xCount
drwsfy1: mov  al,[esi]
         inc  esi
         test al,al     /* If black, don't even test destination */
         jz   drwtfy0
         cmp  byte ptr [edi],bh    /* Needs to be black */
         jnz  drwtfy0      /* Background color, don't draw */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfy0: inc  edi
         dec  ecx
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSpriteTransparent() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite2(int, int, uchar, uchar, bool bool)          *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency color is taken after color translation.   *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite2(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS// PORTABLE
unsigned char c;
register int x, y;
#else
int iSrcPitch;
#endif

/* Adjust for clipped sprite */
   xCount = yCount = 16; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 256];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*16);
      else
         p -= (sy*16); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-16) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-16) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[(15-y)*16+(15-x)]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - 16;
    _asm {
         mov  esi,p
         add  esi,15*16+15  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  bl,byte ptr xCount
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         xor  eax,eax
drwsfxy0: mov  cl,bl
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         mov  al,[edx+eax]
         cmp  al,bh
         jz   drwtfxy0    /* transparent, don't draw it */
         mov  [edi],al
drwtfxy0: dec  cl
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[y*16+(15-x)]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15  /* Start from right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         mov  bl,byte ptr xCount
         xor  eax,eax
drwsfx0: mov  cl,bl
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         mov  al,[edx+eax]
         cmp  al,bh
         jz   drwtfx0    /* transparent, don't draw it */
         mov  [edi],al
drwtfx0: dec  cl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[y*16+x]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 - xCount;
    _asm {
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         mov  bl,byte ptr xCount
         xor  eax,eax
drws0:   mov  cl,bl
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         mov  al,[edx+eax]
         cmp  al,bh
         jz   drwt0    /* transparent, don't draw it */
         mov  [edi],al
drwt0:   dec  cl
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  yCount
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[(15-y)*16+x]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
#else
   /* Use this to speed up asm code */
   iSrcPitch = 16 + xCount;
    _asm {
         mov  esi,p
         add  esi,15*16  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  bh,cTransparent
         mov  edx,pColorPROM
         add  edx,iColor   /* Add fixed color offset */
         mov  bl,byte ptr xCount
         xor  eax,eax
drwsfy0: mov  cl,bl
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         mov  al,[edx+eax]
         cmp  al,bh
         jz   drwtfy0    /* transparent, don't draw it */
         mov  [edi],al
drwtfy0: dec  cl
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  yCount
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawSprite2() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawSprite2_8(int, int, uchar, uchar, bool bool)        *
 *                                                                          *
 *  PURPOSE    : Draw an individual sprite with the given attributes.       *
 *               The transparency color is taken after color translation.   *
 *                                                                          *
 ****************************************************************************/
void EMUDrawSprite2_8(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pColorPROM)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
unsigned char c;
register int x, y;

/* Adjust for clipped sprite */
   xCount = yCount = 8; /* Assume full size to draw */
   p = &pSpriteData[iSprite * 64];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*8);
      else
         p -= (sy*8); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-8) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-8) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pCoinOpBitmap[sy*iCoinOpPitch + sx];
//   iDestPitch = iCoinOpPitch - xCount;

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[(7-y)*8+(7-x)]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
      }
   if (bFlipx && !bFlipy)
      {
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[y*8+(7-x)]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[y*8+x]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
      }
   if (!bFlipx && bFlipy)
      {
      for (y=0; y<yCount; y++)
         for (x=0; x<xCount; x++)
            {
            c = pColorPROM[iColor + p[(7-y)*8+x]];
            if (c != cTransparent)
               d[y*iCoinOpPitch+x] = c;
            }
      }

} /* EMUDrawSprite2_8() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawGraphicTransparent(int, int, uchar, uchar, bool bool)*
 *                                                                          *
 *  PURPOSE    : Draw an individual tile with transparency and flipping.    *
 *               Transparency color is in the sprite data and no color lookup.
 *                                                                          *
 ****************************************************************************/
void EMUDrawGraphicTransparent(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int cx, int cy)
{
unsigned char *d, *p;
int xCount, yCount;
#ifndef BOGUS // PORTABLE
int x, y;
unsigned char c;
#else
int iOffset, iDestPitch, iSrcPitch;
#endif
/* Adjust for clipped sprite */
   xCount = cx;
   yCount = cy; /* Assume full size to draw */
   p = &pSpriteData[iSprite * cx*cy];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*cy);
      else
         p -= (sy*cy); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-cx) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-cy) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pScreen[sy*iPitch2 + sx];

/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS //PORTABLE
   p += (cy-1)*cx;
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x++)
         {
         c = p[cx-1-x];
         if (c != cTransparent)
            d[x] = c + iColor;
         }
      d += iPitch2;
      p -= cx;
      }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - cx;
   iOffset = (cy-1)*cx+(cx-1);
   iDestPitch = iPitch2 - xCount;
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  bh,cTransparent
         mov  ebp,iDestPitch
drwsfxy0: mov  ah,cl
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfxy0    /* transparent, don't draw it */
         add  al,bl
         mov  [edi],al
drwtfxy0: dec  ah
         jnz  drwsfxy1
         add  edi,ebp
         add  esi,edx
         dec  ch
         jnz  drwsfxy0
         pop  ebp
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS // PORTABLE
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x++)
         {
         c = p[cx-1-x];
         if (c != cTransparent)
            d[x] = c + iColor;
         }
      d += iPitch2;
      p += cx;
      }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx + xCount;
   iOffset = cx - 1;
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from opposite direction right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  bh,cTransparent
         mov  ebp,iDestPitch
drwsfx0: mov  ah,cl     /* xCount */
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfx0    /* transparent, don't draw it */
         add  al,bl
         mov  [edi],al
drwtfx0: dec  ah
         jnz  drwsfx1
         add  edi,ebp
         add  esi,edx
         dec  ch        /* yCount */
         jnz  drwsfx0
         pop  ebp
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS //PORTABLE
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x++)
         {
         c = p[x];
         if (c != cTransparent)
            d[x] = c + iColor;
         }
      d += iPitch2;
      p += cx;
      }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx - xCount;
    _asm {
         push ebp
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  bh,cTransparent
         mov  ebp,iDestPitch
drws0:   mov  ah,cl     /* xCount */
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwt0    /* transparent, don't draw it */
         add  al,bl
         mov  [edi],al
drwt0:   dec  ah
         jnz  drws1
         add  edi,ebp
         add  esi,edx
         dec  ch  /* yCount */
         jnz  drws0
         pop  ebp
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
   p += (cy-1)*cx;
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x++)
         {
         c = p[x];
         if (c != cTransparent)
            d[x] = c + iColor;
         }
      d += iPitch2;
      p -= cx;
      }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx + xCount;
   iOffset = (cy-1)*cx;
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  bh,cTransparent
         mov  ebp,iDestPitch
drwsfy0: mov  ah,cl     /* xCount */
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwtfy0    /* transparent, don't draw it */
         add  al,bl
         mov  [edi],al
drwtfy0: dec  ah
         jnz  drwsfy1
         add  edi,ebp
         sub  esi,edx
         dec  ch        /* yCount */
         jnz  drwsfy0
         pop  ebp
         }
#endif
      }

} /* EMUDrawGraphicTransparent() */

/*******************************************************************************
 *                                                                             *
 *  FUNCTION   : EMUDrawGraphicTransparentI(int, int, uchar, uchar, bool bool) *
 *                                                                             *
 *  PURPOSE    : Draw an individual tile with transparency and flipping.       *
 *               Transparency color is in the sprite data and no color lookup. *
 *                                                                             *
 *******************************************************************************/
void EMUDrawGraphicTransparentI(int sx, int sy, int iSprite, int iColor, unsigned char *pColorTable, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int cx, int cy)
{
unsigned char *d, *p;
int xCount, yCount;//, iDestPitch;
#ifndef BOGUS // PORTABLE
int x, y;
unsigned char c;
#else
int iSrcPitch, iOffset;
#endif
/* Adjust for clipped sprite */
   xCount = cx;
   yCount = cy; /* Assume full size to draw */
   p = &pSpriteData[iSprite * cx*cy];
   if (sy < 0)
      {
      yCount += sy; /* Shrink height to draw */
      if (bFlipy)
         p += (sy*cy);
      else
         p -= (sy*cy); /* Adjust sprite pointer also */
      sy = 0; /* Start at 0 */
      }
   if (sx < 0)
      {
      xCount += sx; /* Shrink width to draw */
      if (bFlipx)
         p += sx;
      else
         p -= sx; /* Adjust sprite pointer */
      sx = 0; /* Start at 0 */
      }
   if (sx > iSpriteLimitX-cx) /* Part of it is off the right edge */
      xCount = (iSpriteLimitX-sx); /* Only draw part of it */
   if (sy > iSpriteLimitY-cy) /* Part of it is off the bottom edge */
      yCount = (iSpriteLimitY-sy); /* Only draw part of it */
   if (xCount < 1 || yCount < 1)
      return; /* Nothing to do! */
   d = &pScreen[sy*iPitch2 + sx];

//   iDestPitch = iPitch2 - xCount;
/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
#ifndef BOGUS // PORTABLE
      p += cx*(cy-1);
      pColorTable += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[cx-1-x];
            if (c != cTransparent)
               d[x] = pColorTable[c];
            }
         d += iPitch2;
         p -= cx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = xCount - cx;
   iOffset = (cy-1)*cx+(cx-1);
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edx,pColorTable
         add  edx,iColor
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  bh,cTransparent
         xor  eax,eax
drwsfxy0: mov  bl,cl
drwsfxy1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfxy0    /* transparent, don't draw it */
         mov  al,[edx+eax]  // convert the color
         mov  [edi],al
drwtfxy0: dec  bl
         jnz  drwsfxy1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  ch
         jnz  drwsfxy0
         }
#endif
      }
   if (bFlipx && !bFlipy)
      {
#ifndef BOGUS //PORTABLE
      pColorTable += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[cx-1-x];
            if (c != cTransparent)
               d[x] = pColorTable[c];
            }
         d += iPitch2;
         p += cx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx + xCount;
   iOffset = cx - 1;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from opposite direction right */
         mov  edx,pColorTable
         add  edx,iColor
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  bh,cTransparent
         xor  eax,eax
drwsfx0: mov  bl,cl     /* xCount */
drwsfx1: mov  al,[esi]
         inc  edi
         dec  esi
         cmp  al,bh
         jz   drwtfx0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfx0: dec  bl
         jnz  drwsfx1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  ch        /* yCount */
         jnz  drwsfx0
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
      pColorTable += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (c != cTransparent)
               d[x] = pColorTable[c];
            }
         d += iPitch2;
         p += cx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx - xCount;
    _asm {
         mov  esi,p
         mov  edx,pColorTable
         add  edx,iColor
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  bh,cTransparent
         xor  eax,eax
drws0:   mov  bl,cl     /* xCount */
drws1:   mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwt0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwt0:   dec  bl
         jnz  drws1
         add  edi,iDestPitch
         add  esi,iSrcPitch
         dec  ch  /* yCount */
         jnz  drws0
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
#ifndef BOGUS // PORTABLE
      p += cx*(cy-1);
      pColorTable += iColor;
      for (y=0; y<yCount; y++)
         {
         for (x=0; x<xCount; x++)
            {
            c = p[x];
            if (c != cTransparent)
               d[x] = pColorTable[c];
            }
         d += iPitch2;
         p -= cx;
         }
#else
   /* Use this to speed up asm code */
   iSrcPitch = cx + xCount;
   iOffset = (cy-1)*cx;
    _asm {
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edx,pColorTable
         add  edx,iColor
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  bh,cTransparent
         xor  eax,eax
drwsfy0: mov  bl,cl     /* xCount */
drwsfy1: mov  al,[esi]
         inc  edi
         inc  esi
         cmp  al,bh
         jz   drwtfy0    /* transparent, don't draw it */
         mov  al,[edx+eax]
         mov  [edi],al
drwtfy0: dec  bl
         jnz  drwsfy1
         add  edi,iDestPitch
         sub  esi,iSrcPitch
         dec  ch        /* yCount */
         jnz  drwsfy0
         }
#endif
      }

} /* EMUDrawGraphicTransparentI() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDrawGraphic(int, int, uchar, uchar, bool bool)          *
 *                                                                          *
 *  PURPOSE    : Draw an individual tile (direct colors and no transparency)*
 *               No clipping either.                                        *
 *                                                                          *
 ****************************************************************************/
void EMUDrawGraphic(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int xCount, int yCount)
{
unsigned char *d, *p;
int iOffset, iDestPitch, iSrcPitch;
#ifndef BOGUS  // PORTABLE
int x, y;
uint32_t ul, ulColor;
#endif
   p = &pSpriteData[iSprite * xCount*yCount];
   d = &pScreen[sy*iPitch2 + sx];

   iDestPitch = iPitch2 - xCount;
/* 4 possible flip cases */
   if (bFlipx && bFlipy) /* Both directions flipped */
      {
   /* Use this to speed up asm code */
   iOffset = (yCount-1)*xCount+(xCount-1);
#ifndef BOGUS // PORTABLE
    {
    int x, y;
    p += (yCount-1)*xCount;
    for (y=0; y<yCount; y++)
       {
       for (x=0; x<xCount; x++)
          {
          d[x] = p[xCount-1-x] + iColor;
          }
       d += iPitch2;
       p -= xCount;
       }
    }
#else
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from bottom right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  ebp,iDestPitch
drwsfxy0: mov  ah,cl
drwsfxy1: mov  al,[esi]
         inc  edi
         add  al,bl
         dec  esi
         mov  [edi],al
         dec  ah
         jnz  drwsfxy1
         add  edi,ebp
         dec  ch
         jnz  drwsfxy0
         pop  ebp
         }
#endif // PORTABLE
      }
   if (bFlipx && !bFlipy)
      {
   /* Use this to speed up asm code */
   iSrcPitch = xCount*2;
   iOffset = xCount - 1;
#ifndef BOGUS //PORTABLE
   p += iOffset;
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x++)
         {
         *d++ = *p-- + iColor;
         }
      d += iDestPitch;
      p += iSrcPitch;
      }
#else
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from opposite direction right */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  ebp,iDestPitch
drwsfx0: mov  ah,cl     /* xCount */
drwsfx1: mov  al,[esi]
         inc  edi
         add  al,bl
         dec  esi
         mov  [edi],al
         dec  ah
         jnz  drwsfx1
         add  edi,ebp
         add  esi,edx
         dec  ch        /* yCount */
         jnz  drwsfx0
         pop  ebp
         }
#endif
      }
   if (!bFlipx && !bFlipy) /* Normal direction */
      {
#ifndef BOGUS // PORTABLE
   ulColor = iColor & 0xff;
   ulColor |= (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x+=4)
         {
         ul = ulColor + *(uint32_t *)p;
         d[0] = (char)ul;
         d[1] = (char)(ul>>8);
         d[2] = (char)(ul>>16);
         d[3] = (char)(ul>>24);
         d += 4;
         p += 4;
         }
      d += iDestPitch;
      }
#else
   /* Use this to speed up asm code */
    _asm {
         push ebp
         mov  esi,p
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  ebp,iDestPitch
drws0:   mov  ah,cl     /* xCount */
drws1:   mov  al,[esi]
         inc  edi
         add  al,bl
         inc  esi
         mov  [edi],al
         dec  ah
         jnz  drws1
         add  edi,ebp
         dec  ch  /* yCount */
         jnz  drws0
         pop  ebp
         }
#endif
      }
   if (!bFlipx && bFlipy)
      {
   /* Use this to speed up asm code */
   iSrcPitch = xCount*2;
   iOffset = (yCount-1)*xCount;
#ifndef BOGUS // PORTABLE
   p += iOffset;
   ulColor = iColor & 0xff;
   ulColor |= (ulColor << 8) | (ulColor << 16) | (ulColor << 24);
   for (y=0; y<yCount; y++)
      {
      for (x=0; x<xCount; x+=4)
         {
         ul = ulColor + *(uint32_t *)p;
         d[0] = (char)ul;
         d[1] = (char)(ul>>8);
         d[2] = (char)(ul>>16);
         d[3] = (char)(ul>>24);
         d += 4;
         p += 4;
         }
      d += iDestPitch;
      p -= iSrcPitch;
      }
#else
    _asm {
         push ebp
         mov  esi,p
         add  esi,iOffset  /* Start from bottom */
         mov  edi,d
         dec  edi       /* Adjust for pentium optimization */
         mov  ebx,xCount
         mov  cl,bl
         mov  ebx,yCount
         mov  ch,bl
         mov  ebx,iColor    /* Only low byte used */
         mov  edx,iSrcPitch
         mov  ebp,iDestPitch
drwsfy0: mov  ah,cl     /* xCount */
drwsfy1: mov  al,[esi]
         inc  edi
         add  al,bl
         inc  esi
         mov  [edi],al
         dec  ah
         jnz  drwsfy1
         add  edi,ebp
         sub  esi,edx
         dec  ch        /* yCount */
         jnz  drwsfy0
         pop  ebp
         }
#endif
      }

} /* EMUDrawGraphic() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUCreateSpriteReplicas(char *, int)                       *
 *                                                                          *
 *  PURPOSE    : Create the mirrored and masked copies.                     *
 *               0=normal,1=mask,2=flipped,3=flipped mask                   *
 *                                                                          *
 ****************************************************************************/
unsigned char * EMUCreateSpriteReplicas(unsigned char *pSource, int iSprites)
{
unsigned char c, d, *s, *pReturn;
unsigned char *d1, *d2, *d3, *d4;
int i, x, y;

   pReturn = EMUAlloc(4 * 256 * iSprites); /* Allocate enough for the 4 copies of each sprite */
   for (i=0; i<iSprites; i++) /* Loop through each sprite */
      {
      s = &pSource[256 * i];
      d1 = &pReturn[i*256];
      d2 = d1+(iSprites*256);
      d3 = d2+(iSprites*256);
      d4 = d3+(iSprites*256);
      for (y=0; y<16; y++)
         {
         for (x=0; x<16; x++)
            {
/* First group is unchanged */
            c = s[y*16+x];
            d1[y*16+x] = c;
/* Mask is 0 for color exists, FF for no color */
            if (c)
               d2[y*16+x] = 0;  /* mask = 0 means color here */
            else
               d2[y*16+x] = 0xff; /* Transparent here */
/* Horizontal mirrored */ 
            d = s[y*16+(15-x)]; /* Get flipped pixel */
            d3[y*16+x] = d; /* Flipped */
/* Horizontal mirrored mask */
            if (d)
               d4[y*16+x] = 0;  /* mask = 0 means color here */
            else
               d4[y*16+x] = 0xff; /* Transparent here */
            }
         }
      }
   return pReturn;

} /* EMUCreateSpriteReplicas() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Convert16To32(...)                                         *
 *                                                                          *
 *  PURPOSE    : Convert a RGB565 bitmap to RGB8888.                        *
 *                                                                          *
 ****************************************************************************/
void Convert16To32(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
{
#ifdef USE_SSE
    __m128i xmmInOld, xmmIn, xmmOut0, xmmOut1;
    __m128i xmmMul5, xmmMul6, xmmRMask, xmmGMask, xmmFF00, xmm00FF;
    __m128i xmmR, xmmG, xmmT1, xmmB, xmmT2;
    int x, y;
    unsigned char *s, *d;
   
    xmmMul5 = _mm_set1_epi32(0x01080108);
    xmmMul6 = _mm_set1_epi32(0x20802080);
    xmmRMask = _mm_set1_epi32(0xf800f800);
    xmmGMask = _mm_set1_epi32(0x07e007e0);
    xmmFF00 = _mm_set1_epi32(0xff00ff00); // alpha value
    xmm00FF = _mm_set1_epi32(0x00ff00ff);
    for (y=0; y<iHeight; y++)
    {
        s = &pSrc[y * iSrcPitch];
        d = &pDest[y * iDestPitch];
        xmmInOld = _mm_loadu_si128((__m128i*)s);
        for (x=0; x<iWidth-7; x+=8)
        {
            xmmIn = xmmInOld;
            xmmInOld = _mm_loadu_si128((__m128i*)&s[16]); // load 8 RGB565 pixels
            xmmR = _mm_and_si128(xmmIn, xmmRMask);
            xmmB = _mm_slli_epi16(xmmIn, 11); // position blue bits at top of the word
            xmmT1 = _mm_mulhi_epi16(xmmR, xmmMul5);
            xmmT2 = _mm_mulhi_epi16(xmmB, xmmMul5);
            xmmT1 = _mm_and_si128(xmmT1, xmm00FF);
            xmmT2 = _mm_slli_epi16(xmmT2, 8);
            xmmB = _mm_or_si128(xmmT1, xmmT2); // R+B
            xmmG = _mm_and_si128(xmmIn, xmmGMask);
            xmmG = _mm_mulhi_epi16(xmmG, xmmMul6);
            xmmG = _mm_or_si128(xmmG, xmmFF00); // A+G
            xmmOut0 = _mm_unpacklo_epi8(xmmB, xmmG);
            xmmOut1 = _mm_unpackhi_epi8(xmmB, xmmG);
            _mm_storeu_si128((__m128i*)d, xmmOut0); // write 8 RGBA pixels
            _mm_storeu_si128((__m128i*)&d[16], xmmOut1);
            s += 16;
            d += 32;
        } // for x
    } // for y
#endif // USE_SSE

#ifdef USE_SSE_SLOWER
    __m128i xmmIn, xmmOut0, xmmOut1;
    __m128i xmmRMask, xmmGMask, xmmBMask, xmmFF;
    __m128i xmmR0, xmmG0, xmmG1, xmmB0, xmmB1;
    int x, y;
    unsigned char *s, *d;
    
    xmmRMask = _mm_set1_epi16(0xf800);
    xmmGMask = _mm_set1_epi16(0x7e0);
    xmmBMask = _mm_set1_epi16(0x1f);
    xmmFF = _mm_set1_epi32(0xff000000); // alpha value
    for (y=0; y<iHeight; y++)
    {
        s = &pSrc[y * iSrcPitch];
        d = &pDest[y * iDestPitch];
        for (x=0; x<iWidth; x+=8)
        {
            xmmIn = _mm_loadu_si128((__m128i*)s); // load 8 RGB565 pixels
            xmmB0 = _mm_and_si128(xmmIn, xmmBMask);
            xmmG0 = _mm_and_si128(xmmIn, xmmGMask);
            xmmR0 = _mm_and_si128(xmmIn, xmmRMask);
            xmmB0 = _mm_slli_epi16(xmmB0, 3); // position blue bits at top of first byte
            xmmB0 = _mm_or_si128(xmmB0, _mm_srli_epi16(xmmB0, 5)); // combine B to make an 8-bit value
            xmmG0 = _mm_or_si128(xmmG0, _mm_srli_epi16(xmmG0, 4)); // fill in the lower 2 bits to make 8
            xmmG0 = _mm_srli_epi16(xmmG0, 3); // get rid of unneeded bits
            xmmG0 = _mm_slli_epi16(xmmG0, 8); // position green in top half of 16-bit value
            xmmR0 = _mm_or_si128(xmmR0, _mm_srli_epi16(xmmR0, 5)); // fill in the lower 3 bits to make 8
            xmmR0 = _mm_srli_epi16(xmmR0, 8); // position red in bottom half of 16-bit value
            xmmB1 = _mm_unpackhi_epi16(_mm_setzero_si128(), xmmB0); // expand to uint32_t
            xmmB0 = _mm_unpacklo_epi16(_mm_setzero_si128(), xmmB0);
            xmmG1 = _mm_unpackhi_epi16(xmmG0, _mm_setzero_si128());
            xmmG0 = _mm_unpacklo_epi16(xmmG0, _mm_setzero_si128());
            xmmOut0 = _mm_unpacklo_epi16(xmmR0, _mm_setzero_si128());
            xmmOut1 = _mm_unpackhi_epi16(xmmR0, _mm_setzero_si128()); // put red in top half of 32-bit value
            xmmOut0 = _mm_or_si128(xmmOut0, xmmG0); // add green
            xmmOut1 = _mm_or_si128(xmmOut1, xmmG1);
            xmmOut0 = _mm_or_si128(xmmOut0, xmmB0); // add blue
            xmmOut1 = _mm_or_si128(xmmOut1, xmmB1);
            xmmOut0 = _mm_or_si128(xmmOut0, xmmFF); // set alpha value
            xmmOut1 = _mm_or_si128(xmmOut1, xmmFF);
            _mm_storeu_si128((__m128i*)d, xmmOut0); // write 8 RGBA pixels
            _mm_storeu_si128((__m128i*)&d[16], xmmOut1);
            s += 16;
            d += 32;
        } // for x
    } // for y
#endif
#ifdef USE_NEON
    uint16x8_t xmmIn0, xmmIn1;
    uint8x16_t xmmR, xmmG, xmmB;
    uint8x16x4_t xmmOut;
    int x, y;
    unsigned char *s, *d;

    xmmOut.val[3] = vdupq_n_u8(0xff); // alpha value
    for (y=0; y<iHeight; y++)
    {
        s = &pSrc[y * iSrcPitch];
        d = &pDest[y * iDestPitch];
        for (x=0; x<iWidth-15; x+=16)
        {
            __builtin_prefetch(&s[256]);
            xmmIn0 = vld1q_u16((uint16_t*)s); // load 8 RGB565 pixels
            xmmIn1 = vld1q_u16((uint16_t*)&s[16]);
            xmmB = vcombine_u8(vmovn_u16(xmmIn0), vmovn_u16(xmmIn1)); // isolate blue
            xmmG = vcombine_u8(vshrn_n_u16(xmmIn0, 5), vshrn_n_u16(xmmIn1,5)); // isolate green
            xmmIn0 = vshrq_n_u16(xmmIn0, 11); // bug in GCC's NEON forces this
            xmmIn1 = vshrq_n_u16(xmmIn1, 11); // instead of narrowing shift
            xmmR = vcombine_u8(vmovn_u16(xmmIn0), vmovn_u16(xmmIn1)); // red
            xmmB = vshlq_n_u8(xmmB, 3); // position blue bits at top of first byte
            xmmG = vshlq_n_u8(xmmG, 2);
            xmmR = vshlq_n_u8(xmmR, 3);
            xmmOut.val[0] = vorrq_u8(xmmB, vshrq_n_u8(xmmB, 5)); // finished with Blue
            xmmOut.val[1] = vorrq_u8(xmmG, vshrq_n_u8(xmmG, 6)); // G
            xmmOut.val[2] = vorrq_u8(xmmR, vshrq_n_u8(xmmR, 5)); // R
            vst4q_u8(d, xmmOut);
            s += 32;
            d += 64;
        } // for x
    } // for y
#endif // USE_NEON
#if !defined(USE_SSE) && !defined(USE_NEON)
    int x, y;
    unsigned short us, *s;
    uint32_t u32, *d;
    
    for (y=0; y<iHeight; y++)
    {
        s = (unsigned short *)&pSrc[iSrcPitch * y];
        d = (uint32_t *)&pDest[y * iDestPitch];
        for (x=0; x<iWidth; x++)
        {
            us = *s++;
            u32 = (us & 0x1f) << 19; // blue
            u32 |= (us & 0x1c) << 14;
            u32 |= (us & 0x7e0) << 5; // green
            u32 |= ((us & 0x600) >> 1); 
            u32 |= (us & 0xf800) >> 8; // red
            u32 |= (us & 0xe000) >> 11;
            u32 |= 0xff000000; // Alpha
            *d++ = u32;
        } // for x
    } // for y
#endif
} /* Convert16To32() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDecodeGFX(GFXDECODE *, char *, char *)                  *
 *                                                                          *
 *  PURPOSE    : Generic graphics decoder.                                  *
 *                                                                          *
 ****************************************************************************/
void EMUDecodeGFX(GFXDECODE *gfx, int iCount, char *src, char *dest, BOOL bRotate)
{
register int x, y;
int i, iBit, iStartBit, iPlane, iOffset;
unsigned char *p, *d, *pDest, ucTemp;

   iOffset = iCount * gfx->cWidth * gfx->cHeight; /* Distance to next block of tiles */
/* Convert the tiles (could be chars or sprites) */
   for (i=0; i<iCount; i++)
      {
      d = (unsigned char *)&dest[i * (gfx->cWidth * gfx->cHeight)]; /* pointer to converted character */
      p = (unsigned char *)&src[i * gfx->iDelta];  /* pointer to original character */
      for (x=0; x<gfx->cWidth; x++)
         {
         pDest = d + x;
         for (y=0; y<gfx->cHeight; y++)
            {
            ucTemp = 0;
            iStartBit = gfx->iBitsX[x] + gfx->iBitsY[y];
            for (iPlane=0; iPlane<gfx->cBitCount; iPlane++)
               {
               iBit = iStartBit + gfx->iPlanes[iPlane];
               if (p[iBit >> 3] & (1<<(iBit&7)))  /* Test if bit is set */
                  ucTemp |= 1<<iPlane;
               } /* bit plane */
            pDest[0] = ucTemp;
            pDest += gfx->cWidth;
            if (bRotate) /* Set up the 3 other rotated positions */
               {
               d[iOffset + y*gfx->cWidth+(gfx->cWidth-1-x)] = ucTemp; /* Mirrored X */
               d[iOffset*2 + (gfx->cHeight-1-y)*gfx->cWidth+x] = ucTemp; /* Mirrored Y */
               d[iOffset*3 + (gfx->cHeight-1-y)*gfx->cWidth+(gfx->cWidth-1-x)] = ucTemp; /* Mirrored X+Y */
               }
            } /* for y */
         } /* for x */
      } /* for each tile */

} /* EMUDecodeGFX() */
