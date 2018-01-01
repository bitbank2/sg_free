//
// SmartGear support functions
//
// Copyright (C) 2010-2017 BitBank Software, Inc.
//
// Written by Larry Bank
// Project started 12/25/2010
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


//#include <GLES/gl.h>
//#include <GLES/glext.h>
//#include <jni.h>
#include <time.h>
//#include <android/log.h>
//#include <android/bitmap.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "my_windows.h"
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include "sound.h"
//#include "pil_io.h"
//#include "pil.h"

extern unsigned char *pScreen;
extern int iScreenPitch;

//BOOL EMULoadState(int iConfig, unsigned char *pMap, char *pRegs, int iRegSize, char *pColors, int iColorSize);
int SG_InitGame(unsigned char *pszGame);
void SG_TerminateGame(void);
void SG_ResetGame(void);
void SG_PlayGame(BOOL bAudio, BOOL bVideo, uint32_t ulKeys);
void SG_Rewind(void);
int SG_ReadPrevList(void);
void SGLoadDips(void);
void SGSaveDips(int iGame);
void SG_GetLeafName(char *fname, char *leaf);
void ASMBLT100(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);
void ASMBLT150S(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);
void ASMBLT200HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight);
void ASMBLT200(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iSrcWidth, int iSrcHeight);
void ASMBLT300(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);
void ASMBLT300HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);
void ScreenShot(unsigned char *pBitmap, int width, int height, int pitch);
char szGame[256];
char pszHome[256];
char pszSAVED[256];
char pszScreenShots[256];
char pszHighScores[256];
char pszDIPs[256];
// Cheat list
#define MAX_CHEATS 10
int iCheatCount;
int iaCheatAddr[MAX_CHEATS], iaCheatValue[MAX_CHEATS], iaCheatCompare[MAX_CHEATS];

//static GLuint s_texture = 0;
//static int iDisplayWidth, iDisplayHeight, iTextureWidth, iTextureHeight, iNewWidth, iNewHeight;
//static int iVideoSize, iTexturePitch;
//static unsigned char *pTexture = NULL;
//static int iBorderX, iBorderY, iStretchedWidth, iStretchedHeight;
static pthread_cond_t s_vsync_cond;
static pthread_mutex_t s_vsync_mutex;
int iAutoFire, iAudioRate;
unsigned char HEX2BIN(unsigned char c);
int EMUTestName(char *);
//void glDrawTexiOES(int, int, int, int, int);

enum {VIDEO_75=0,VIDEO_100,VIDEO_150, VIDEO_200, VIDEO_300, VIDEO_FIT2WIN, VIDEO_FIT2WIN_ASPECT};

extern GAME_BLOB blobs[];
extern unsigned char *pBitmap;
extern int iPitch;
extern char szGameFiles[MAX_PREV_GAMES][256];
extern BOOL bHead2Head; // GameBoy/GameGear 2-player head-2-head play
extern BOOL bAutoLoad;
extern BOOL bHideOverscan;
extern GAMEDEF gameList[];
extern char *szGameFourCC[];
extern int iGameType;

#ifdef FUTURE
/* disable these capabilities. */
static GLuint s_disable_caps[] = {
	GL_FOG,
	GL_LIGHTING,
	GL_CULL_FACE,
	GL_ALPHA_TEST,
	GL_BLEND,
	GL_COLOR_LOGIC_OP,
	GL_DITHER,
	GL_STENCIL_TEST,
	GL_DEPTH_TEST,
	GL_COLOR_MATERIAL,
	0
};
#endif // FUTURE

/* wait for the screen to redraw */
void wait_vsync(void)
{
	pthread_mutex_lock(&s_vsync_mutex);
	pthread_cond_wait(&s_vsync_cond, &s_vsync_mutex);
	pthread_mutex_unlock(&s_vsync_mutex);
}

#ifdef FUTURE
static void check_gl_error(const char* op)
{
	GLint error;
	for (error = glGetError(); error; error = glGetError())
	    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "after %s() glError (0x%x)\n", op, error);
} /* check_gl_error() */

void native_gl_resize(JNIEnv *env, jobject obj, jint w, jint h, jboolean bSmooth, jboolean bFillScreen)
{
	int rect[4];
    int iScale, i, j;
	int iWidth = blobs[MAX_PREV_GAMES].iWidth;
	int iHeight = blobs[MAX_PREV_GAMES].iHeight;
	GLuint *start = s_disable_caps;

	iDisplayWidth = w;
	iDisplayHeight = h;

	// On the Xperia Play if the OpenGL texture size is 512x256 or smaller, then it can do 60/61fps
	// If the texture size is changed to 1024/512, the framerate drops to 38-48 fps
	// The CPU time needed to do the high quality stretch doesn't affect the framerate much, but it
	// produces a larger texture size which does drop the effective framerate
	// Using OpenGL to stretch the output to fit the screen doesn't affect the framerate
	// it uses a decent-looking filter which doesn't make it look too blurry

    if (bSmooth)
    {
		if (iWidth*3 <= iDisplayWidth && iHeight*3 <= iDisplayHeight)
		   iVideoSize = VIDEO_300;
		else if (iWidth*2 <= iDisplayWidth && iHeight*2 <= iDisplayHeight)
		   iVideoSize = VIDEO_200;
		else if (((iWidth*3)/2) <= iDisplayWidth && ((iHeight*3)/2) <= iDisplayHeight)
		   iVideoSize = VIDEO_150;
		else if (iWidth <= iDisplayWidth && iHeight <= iDisplayHeight)
		   iVideoSize = VIDEO_100;
		else
		   iVideoSize = VIDEO_75;
		switch (iVideoSize)
		   {
		   case VIDEO_75:
			   iNewWidth = (iWidth * 3)>>2;
			   iNewHeight = (iHeight*3)>>2;
			  break;
		   case VIDEO_100:
			   iNewWidth = iWidth;
			   iNewHeight = iHeight;
			  break;
		   case VIDEO_150:
			   iNewWidth = (iWidth*3)>>1;
			   iNewHeight = (iHeight*3)>>1;
			  break;
		   case VIDEO_200:
			   iNewWidth = iWidth*2;
			   iNewHeight = iHeight*2;
			  break;
		   case VIDEO_300:
			   iNewWidth = iWidth*3;
			   iNewHeight = iHeight*3;
			  break;
		   }
    }
    else // draw it 1:1 and let OpenGL stretch it
    {
    	iVideoSize = VIDEO_100;
    	iNewHeight = iHeight;
    	iNewWidth = iWidth;
    }

    // OpenGL requires that texture dimensions are a power of 2
    iTextureWidth = iTextureHeight = 256;
    if (iNewWidth > 512)
    	iTextureWidth = 1024;
    else if (iNewWidth > 256)
    	iTextureWidth = 512;
    if (iNewHeight > 512)
    	iTextureHeight = 1024;
    else if (iNewHeight > 256)
    	iTextureHeight = 512;
    iTexturePitch = iTextureWidth *2;
	if (pTexture)
		free(pTexture);
	pTexture = malloc(iTexturePitch * iTextureHeight);
//    __android_log_print(ANDROID_LOG_VERBOSE, "native_gl_resize", "screen w,h = %d,%d texture w,h = %d,%d\n", iDisplayWidth, iDisplayHeight, iTextureWidth, iTextureHeight);

	rect[0] = 0;
	rect[1] =  iNewHeight;
	rect[2] =  iNewWidth;
	rect[3] = -iNewHeight;

//	LOGI("native_gl_resize %d %d", w, h);
	glDeleteTextures(1, &s_texture);
	while (*start)
		glDisable(*start++);
	glEnable(GL_TEXTURE_2D);
	glGenTextures(1, &s_texture);
	check_gl_error("glGenTextures");
	glBindTexture(GL_TEXTURE_2D, s_texture);
	check_gl_error("glBindTexture");
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glShadeModel(GL_FLAT);
	check_gl_error("glShadeModel");
	glColor4x(0x10000, 0x10000, 0x10000, 0x10000);
	check_gl_error("glColor4x");
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, rect);
	check_gl_error("glTexParameteriv");
	// stretch the display the last little bit to fit at least one dimension
	i = (iNewWidth * 1024)/iDisplayWidth;
	j = (iNewHeight * 1024)/iDisplayHeight;
	if (bFillScreen) // user wants the entire screen filled even if the ratio is not square
	{
		iStretchedWidth = (iNewWidth * 1024)/i;
		iStretchedHeight = (iNewHeight * 1024)/j;
		iBorderX = iBorderY = 0; // no borders
	}
	else
	{
		iScale = i;
		if (j > i) // width needs more stretching
			iScale = j;
		iStretchedWidth = (iNewWidth * 1024)/iScale;
		iStretchedHeight = (iNewHeight * 1024)/iScale;
		iBorderX = (iDisplayWidth - iStretchedWidth)/2;
		iBorderY = (iDisplayHeight - iStretchedHeight)/2;
	}
	if (iBorderX < 0) iBorderX = 0;
	if (iBorderY < 0) iBorderY = 0;
//    __android_log_print(ANDROID_LOG_VERBOSE, "native_gl_resize", "borderX=%d, borderY=%d, StretchedWidth=%d, StretchedHeight=%d\n", iBorderX, iBorderY, iStretchedWidth, iStretchedHeight);

} /* native_gl_resize() */

void native_start(JNIEnv *env, jclass clazz)
{
	pthread_cond_init(&s_vsync_cond, NULL);
	pthread_mutex_init(&s_vsync_mutex, NULL);
} /* native_start() */

void native_gl_render(JNIEnv *env, jobject obj, jobject bitmap, jboolean bPaused, jboolean bSmooth)
{
	int iNewWidth, iNewHeight;
	int iWidth = blobs[MAX_PREV_GAMES].iWidth;
	int iHeight = blobs[MAX_PREV_GAMES].iHeight;
	unsigned char *pSrcBitmap;
    GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
//    int iSourceBitmap;
//    volatile int iTimeOut;
//    signed int *pFrameNumber;
//    unsigned char *pSrc;
//    int iRetry;

    if (pTexture == NULL || pBitmap == NULL)
    	return; // nothing to do

    pSrcBitmap = (unsigned char *)pBlob->pBitmap;
    // Use the newest video buffer (double buffer scheme)
//    iTimeOut = 10;
//    while (pBlob->iBitmapFrame0 == -1 && pBlob->iBitmapFrame1 == -1 && iTimeOut > 0) // source bitmap not ready
//    {
//    	iTimeOut--;
//    	usleep(500); // wait up to 5ms for producer thread to be ready
//    }
//    if (iTimeOut == 0) // ran out of time, leave
//    	return;
//    if (pBlob->iBitmapFrame0 != -1)
//    {
//    	pSrcBitmap = pBlob->pBitmap0;
//    	iSourceBitmap = 0;
//    }
//    else
//    {
//    	pSrcBitmap = pBlob->pBitmap1;
//    	iSourceBitmap = -1;
//    }

#ifdef BOGUS
    if (pBlob->iVideoFrame1 == -1 && pBlob->iVideoFrame2 == -1)
    {
    	iRetry = 0;
    	while (iRetry < 20 && pBlob->iVideoFrame1 == -1 && pBlob->iVideoFrame2 == -1) // allow time for other thread to draw the frame
    	{
    		iRetry++;
    		usleep(500);
    	}
        if (pBlob->iVideoFrame1 == -1 && pBlob->iVideoFrame2 == -1)
        {
        	__android_log_print(ANDROID_LOG_VERBOSE, "native_gl_render", "no frame ready to display, iFrame=%d", iFrame);
        	return; // no video frame ready, leave
        }
    }
    // Use the newest available frame
    if (pBlob->iVideoFrame1 > pBlob->iVideoFrame2)
    {
    	pSrc = pBlob->pBitmap1;
    	pFrameNumber = &pBlob->iVideoFrame1;
    }
    else
    {
    	pSrc = pBlob->pBitmap2;
    	pFrameNumber = &pBlob->iVideoFrame2;
    }
#endif

	iNewWidth = iWidth;
	iNewHeight = iHeight;

	// Stretch the game pixels to the size of the texture
    if (iVideoSize == VIDEO_100) // 100%
       {
       ASMBLT100(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       }
    else if (iVideoSize == VIDEO_150) // 150%
       {
       ASMBLT150S(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       iNewWidth = (iWidth * 3)/2;
       iNewHeight = (iHeight * 3)/2;
       }
    else if (iVideoSize == VIDEO_200) // 200%
       {
        iNewWidth = iWidth * 2;
        iNewHeight = iHeight * 2;
       if (bSmooth)
    	   ASMBLT200HQ(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       else
    	   ASMBLT200(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       }
    else if (iVideoSize == VIDEO_300) // 300%
       {
        iNewWidth = iWidth * 3;
        iNewHeight = iHeight * 3;
       if (bSmooth)
    	   ASMBLT300HQ(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       else
    	   ASMBLT300(pSrcBitmap, iPitch, pTexture, iTexturePitch, iWidth, iHeight);
       }
    // Tell the emulator code that we're finished using the video frame
    // and it can be used for drawing again
//    if (iSourceBitmap == 0)
//    	pBlob->iBitmapFrame0 = -1;
//    else
//    	pBlob->iBitmapFrame1 = -1;

    if (bPaused)
    {
        AndroidBitmapInfo  info;
        void*              pixels;
        int                ret;
        unsigned char      *pSrc, *pDest;
        int                y;

		ret = AndroidBitmap_getInfo(env, bitmap, &info);
		if (ret < 0)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "native_gl_render", "AndroidBitmap_getInfo() failed, error=%d", ret);
			return;
		}
		if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
			__android_log_print(ANDROID_LOG_VERBOSE, "native_gl_render", "Bitmap format is not RGB_565");
			return;
		}
		ret = AndroidBitmap_lockPixels(env, bitmap, &pixels);
		if (ret < 0)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, "native_gl_render", "AndroidBitmap_lockPixels() failed ! error=%d", ret);
			return;
		}
		for (y=0; y<info.height; y++)
		{
			// copy the "paused" bitmap to the middle of the video frame
			pDest = &pTexture[(iNewWidth-info.width) + (iTexturePitch * (y + (iNewHeight-info.height)/2))];
			pSrc = (unsigned char *)pixels;
			pSrc += (y * info.stride);
			memcpy(pDest, pSrc, info.width * 2);
		}
    } // if paused

    glClear(GL_COLOR_BUFFER_BIT);
	glTexImage2D(GL_TEXTURE_2D,		/* target */
			0,			/* level */
			GL_RGB,			/* internal format */
			iTextureWidth,		/* width */
			iTextureHeight,		/* height */
			0,			/* border */
			GL_RGB,			/* format */
			GL_UNSIGNED_SHORT_5_6_5,/* type */
			pTexture);		/* pixels */
//	check_gl_error("glTexImage2D");
//	glDrawTexiOES(0, 0, 0, iDisplayWidth,iDisplayHeight);
//	glDrawTexiOES((iDisplayWidth-iNewWidth)/2, (iDisplayHeight-iNewHeight)/2, 0, iNewWidth,iNewHeight);
	glDrawTexiOES(iBorderX, iBorderY, 0, iStretchedWidth, iStretchedHeight);
//	check_gl_error("glDrawTexiOES");
	/* tell the other thread to carry on */
	pthread_cond_signal(&s_vsync_cond);
} /* native_gl_render() */

// H1 = audioFunc
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_Play_H1(JNIEnv * env, jobject obj, jshortArray AudioBuf)
{
	if (blobs[MAX_PREV_GAMES].iSoundChannels == 2) // need to convert stereo to mono - for now DEBUG
	{
		signed short usTemp[740];
		signed short *pAudio = (signed short *)pSoundBuf;
		int i, sum;
		for (i=0; i<740; i++)
			{
			sum = (int)pAudio[i*2] + (int)pAudio[(i*2)+1];
			usTemp[i] = (signed short)(sum >> 1);
			}
		(*env)->SetShortArrayRegion(env, AudioBuf, 0, iSoundBlock/4, (short *)&usTemp[0]);
	}
	else
	{
		(*env)->SetShortArrayRegion(env, AudioBuf, 0, iSoundBlock/2, (short *)pSoundBuf);
	}
} /* audioFunc() */
#endif // FUTURE

// Decode a GameBoy GameShark code
// code is in the form: 01234567
void GBGameSharkDecode(unsigned char *pszString, uint32_t *pulAddr, uint32_t *pulValue)
{
uint32_t ulAddr, ulValue;

	ulValue = HEX2BIN(pszString[2]) << 4;
	ulValue |= HEX2BIN(pszString[3]);
	ulAddr = HEX2BIN(pszString[6]) << 12;
	ulAddr |= HEX2BIN(pszString[7]) << 8;
	ulAddr |= HEX2BIN(pszString[4]) << 4;
	ulAddr |= HEX2BIN(pszString[5]);
	*pulAddr = ulAddr;
	*pulValue = ulValue;

} /* GBGameSharkDecode() */

// Decode a GameGear Game Genie code (6 or 9 digit)
void GGGameGenieDecode(unsigned char *pszCode, uint32_t *pAddr, uint32_t *pValue, uint32_t *pCheck)
{
uint32_t ulAddr, ulValue, ulCheck;

	ulAddr = (HEX2BIN(pszCode[2]) << 8);
	ulAddr |= (HEX2BIN(pszCode[4]) << 4);
	ulAddr |= HEX2BIN(pszCode[5]);
	ulAddr |= ((HEX2BIN(pszCode[6]) ^ 0xf) << 12);
	ulValue = (HEX2BIN(pszCode[0]) << 4);
	ulValue |= HEX2BIN(pszCode[1]);
	if (strlen((char *)pszCode) >= 11)
	{
		ulCheck = (HEX2BIN(pszCode[8]) << 4);
		ulCheck |= HEX2BIN(pszCode[10]);
		ulCheck = (ulCheck >> 2) | ((ulCheck & 3) << 6); // rotate right 2 bits
		ulCheck ^= 0xba;
		*pCheck = ulCheck;
	}

	*pAddr = ulAddr;
	*pValue = ulValue;

} /* GGGameGenieDecode() */

// Decode a NES Game Genie code (6 or 8 digit)
// Cypher for 6-digit codes
// Code: ABCD EFGH IJKL MNOP QRST UVWX
// Clear: INOP QVWX EJKL MRST AFGH UBCD
// Cypher for 8-digit codes
// Code: ABCD EFGH IJKL MNOP QRST UVWX abcd efgh
// Clear: INOP QVWX EJKL MRST AFGH eBCD afgh Ubcd
void NESGameGenieDecode(unsigned char *pszCode, uint32_t *pAddr, uint32_t *pValue, uint32_t *pCheck)
{
int i, k, iLen, iDash;
unsigned char c, j;
uint32_t ulAddr, ulValue, ulCheck;
unsigned char ucDigits[8];
const unsigned char ucAlphabet[] = {'A','P','Z','L','G','I','T','Y','E','O','X','U','K','S','V','N'};

	ulAddr = ulValue = 0; // suppress warning
	iLen = (int)strlen((char *)pszCode);
    if (iLen < 6 || iLen > 9)
       return; // invalid code length

	if (iLen == 7)
		iDash = 3;
	else if (iLen == 9)
		iDash = 4;
	else iDash = 10; // no dash/space
	k = 0;
	for (i=0; i<iLen; i++)
	{
		if (i == iDash)
			i++; // skip space or dash separator
		c = pszCode[i];
		if (c >= 'a' && c <= 'z')
			c &= 0x5f; // make it upper case
		for (j=0; j<16; j++)
		{
			if (c == ucAlphabet[j])
				break;
		}
		ucDigits[k++] = j;
	}
	// Rearrange the bits based on the cypher
	if (iLen == 6 || iLen == 7) // 6 digit codes
	{
        ulAddr = ((ucDigits[2] & 8) << 12);
        ulAddr |= ((ucDigits[3] & 7) << 12);
        ulAddr |= ((ucDigits[4] & 8) << 8);
        ulAddr |= ((ucDigits[5] & 7) << 8);
        ulAddr |= ((ucDigits[1] & 8) << 4);
        ulAddr |= ((ucDigits[2] & 7) << 4);
        ulAddr |= (ucDigits[3] & 8);
        ulAddr |= (ucDigits[4] & 7);

        ulValue = ((ucDigits[0] & 8) << 4);
        ulValue |= ((ucDigits[1] & 7) << 4);
        ulValue |= (ucDigits[5] & 8);
        ulValue |= (ucDigits[0] & 7);
	}
	else if (iLen == 8 || iLen == 9) // 8 digit codes
	{

        ulAddr = ((ucDigits[2] & 8) << 12);
        ulAddr |= ((ucDigits[3] & 7) << 12);
        ulAddr |= ((ucDigits[4] & 8) << 8);
        ulAddr |= ((ucDigits[5] & 7) << 8);
        ulAddr |= ((ucDigits[1] & 8) << 4);
        ulAddr |= ((ucDigits[2] & 7) << 4);
        ulAddr |= (ucDigits[3] & 8);
        ulAddr |= (ucDigits[4] & 7);

        ulValue = ((ucDigits[0] & 8) << 4);
        ulValue |= ((ucDigits[1] & 7) << 4);
        ulValue |= (ucDigits[7] & 8);
        ulValue |= (ucDigits[0] & 7);

        ulCheck = ((ucDigits[6] & 8) << 4);
        ulCheck |= ((ucDigits[7] & 7) << 4);
        ulCheck |= (ucDigits[5] & 8);
        ulCheck |= (ucDigits[6] & 7);

		*pCheck = ulCheck;
	}
    if (ulAddr < 0x8000)
       ulAddr += 0x8000;
	*pAddr = ulAddr;
	*pValue = ulValue;

} /* NESGameGenieDecode() */

// Decode a Genesis Game Genie code
// Code is in the form: ABCD-EFGH
void GenGameGenieDecode(unsigned char *pszCode, uint32_t *pAddr, uint32_t *pValue)
{
int i;
unsigned char c, j;
uint32_t ulAddr, ulValue;
unsigned char ucBytes[5];

const char cAlphabet[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'J', 'K', 'L', 'M', 'N', 'P', 'R', 'S',
    'T', 'V', 'W', 'X', 'Y', 'Z', '0', '1',
    '2', '3', '4', '5', '6', '7', '8', '9'};

    memset(&ucBytes[0], 0, sizeof(ucBytes)); // suppress compiler warning
// first decode the code letters (ABCD:EFGH) to a 40-bit number (8 digits x 5 bits each)
    for (i=0; i<9; i++)
    {
       if (i == 4) i++; // skip colon in middle of code
       c = pszCode[i];
       if (c >= 'a' && c <= 'z') // make it upper case
          c &= 0x5f;
       for (j=0; j<32; j++)
          if (cAlphabet[j] == c) // found it
             break;
       switch (i) // arrange the bits in the correct output bytes
       {
       case 0:
          ucBytes[0] = j << 3; // upper 5 bits
          break;
       case 1:
          ucBytes[0] |= (j >> 2); // upper 3 bits here
          ucBytes[1] = (j << 6); // lower 2 bits here
          break;
       case 2:
          ucBytes[1] |= (j << 1); // 5 more bits
          break;
       case 3:
          ucBytes[1] |= (j >> 4); // lsb of second byte
          ucBytes[2] = (j << 4); // upper 4 bits of 3rd byte
          break;
       case 5:
          ucBytes[2] |= (j >> 1); // 4 more bits
          ucBytes[3] = (j << 7); // lsb goes to msb
          break;
       case 6:
          ucBytes[3] |= (j << 2); // 5 more bits
          break;
       case 7:
          ucBytes[3] |= (j >> 3); // last 2 bits
          ucBytes[4] = (j << 5); // lower 3 bits in upper 3 bits spot
          break;
       case 8:
          ucBytes[4] |= j; // last 5 bits
          break;
       } // switch
    }
// now we've got the 40-bits (upper/lower halves) which need to be further decoded into the address and value
// Game Genie's convoluted "encryption" transposes the bits like this:
// Code: ijklmnop IJKLMNOP ABCDEFGH defghabc QRSTUVWX
// Clear: ABCDEFGH IJKLMNOP QRSTUVWX abcdefgh ijklmnop
// The decoded addr/value is AAAAAA:VVVV (24-bit address, 16-bit value)
//	__android_log_print(ANDROID_LOG_VERBOSE, "GenGameGenieDecode", "Bytes: %02x, %02x, %02x, %02x, %02x", ucBytes[0],ucBytes[1],ucBytes[2],ucBytes[3],ucBytes[4]);
    ulAddr = ucBytes[2] << 16; // topmost byte of address
    ulAddr |= ucBytes[1] << 8; // next byte
    ulAddr |= ucBytes[4]; // last byte of address
    ulValue = ((ucBytes[3] << 13) & 0xe000); // lower 3 bits from here
    ulValue |= ((ucBytes[3] & 0xf8) << 5);
    ulValue |= ucBytes[0]; // lower 8 bits
    *pAddr = ulAddr;
    *pValue = ulValue;
} /* GenGameGenieDecode() */

unsigned char HEX2BIN(unsigned char c)
{
	if (c <= '9')
		return (c - '0');
	else
		return ((c & 0x5f) - 0x37);

} /* HEX2BIN() */

// Cheat data is stored as:
// cheat_string, cheat_name, enabled (0/1) <cr>
void LoadCheats(void)
{
	int i, iLen;
	void * ihandle;
	int j;
	int iPhase;
	int iAddr, iVal, iCheck;
	char pszTemp[256];
	char pszLeaf[256];
    unsigned char c, *pData;
    pData = EMUAlloc(4096);


    iCheatCount = 0; // assume no cheats available
    SG_GetLeafName(szGame, pszLeaf); // get the rom leaf name
    strcpy(pszTemp, pszHome);
    strcat(pszTemp, "Cheats/");
    strcat(pszTemp, pszLeaf);
    strcat(pszTemp, ".cheat");
//	__android_log_print(ANDROID_LOG_VERBOSE, "LoadCheats", "Entering, cheat file = %s", pszTemp);

    ihandle = EMUOpenRO(pszTemp);
    if (ihandle != (void *)-1) // exists
    {
    	iLen = EMURead(ihandle, pData, 4096);
//    	__android_log_print(ANDROID_LOG_VERBOSE, "LoadCheats", "Read %d bytes", iLen);
    	pData[iLen] = 0;
 //   	__android_log_print(ANDROID_LOG_VERBOSE, "LoadCheats", "%s", pData);
    	EMUClose(ihandle);
    	i = 0;
    	j = 0; // current string length
    	iPhase = 0;
    	while (i < iLen && iCheatCount < MAX_CHEATS) // process all of the cheats
    	{
    		c = pData[i++];
    		if (iPhase == 0)
    		{
    			if (c != ',')
        			pszLeaf[j++] = c;
    			else
    			{
    				pszLeaf[j] = 0; // terminator
    				iPhase++;
    			}
				continue;
    		}
    		if (iPhase == 1 && c == ',')
    		{
    			if (pData[i] == '1') // cheat is enabled
    			{
    				iaCheatCompare[iCheatCount] = -1; // assume no compare value
					iCheck = -1; // assume no check value
					iAddr = -1;
    				// process the cheat string
    				switch (blobs[MAX_PREV_GAMES].iGameType)
    				{
    				case SYS_NES:
						NESGameGenieDecode((unsigned char *)pszLeaf, (uint32_t *)&iAddr, (uint32_t *)&iVal, (uint32_t *)&iCheck);
    					if (iAddr > 0 && iAddr < 0x10000) // valid range
    					{
    						iaCheatAddr[iCheatCount] = iAddr;
    						iaCheatValue[iCheatCount] = iVal;
    						iaCheatCompare[iCheatCount] = iCheck;
    						iCheatCount++;
    					}
    					break;
    				case SYS_GAMEBOY:
    				case SYS_GAMEGEAR:
    					if (strlen(pszLeaf) == 8 && pszLeaf[0] == '0')
    						GBGameSharkDecode((unsigned char *)pszLeaf, (uint32_t *)&iAddr, (uint32_t *)&iVal);
    					else
    						GGGameGenieDecode((unsigned char *)pszLeaf, (uint32_t *)&iAddr, (uint32_t *)&iVal, (uint32_t *)&iCheck);
    					if (iAddr > 0 && iAddr < 0x10000) // valid range
    					{
    						iaCheatAddr[iCheatCount] = iAddr;
    						iaCheatValue[iCheatCount] = iVal;
    						iaCheatCompare[iCheatCount] = iCheck;
    						iCheatCount++;
    					}
    					break;
    				case SYS_GENESIS:
    					GenGameGenieDecode((unsigned char *)pszLeaf, (uint32_t *)&iAddr, (uint32_t *)&iVal);
        				iaCheatAddr[iCheatCount] = iAddr;
        				iaCheatValue[iCheatCount] = iVal;
        				iCheatCount++;
    					break;
    				}
//    				__android_log_print(ANDROID_LOG_VERBOSE, "LoadCheats", "Cheat code: %s, val=%02x, addr=%04x", pszLeaf, iVal, iAddr);
    			}
    			i+=2; // skip 0/1 and CR
    			iPhase = 0;
    			j = 0; // reset for next line
    		}
    	}
    }
	EMUFree(pData);

} /* LoadCheats() */

//
// Copy a local video buffer to the one used for our output display
// Destination can be rotated +/- 90 degrees
//
void SG_VideoCopy(unsigned char *pCoinOpBitmap, int iCoinOpPitch, int iWidth, int iHeight, int iBpp, int iAngle)
{
	int y;
	int iBytesPerPixel = iBpp / 8;
	if (iAngle == 0) // no screen rotation
	{
		unsigned char *s, *d;
		for (y=0; y<iHeight; y++)
		{
			s = &pCoinOpBitmap[y * iCoinOpPitch];
			d = &pScreen[y * iScreenPitch];
			memcpy(d, s, iWidth*iBytesPerPixel);
		}
	}
	else if (iAngle == 90)
	{
		unsigned short *us, *ud;
		int x;
		for (x=0; x<iWidth; x++)
		{
			us = (unsigned short *)&pCoinOpBitmap[(iWidth - 1 - x) * 2];
			ud = (unsigned short *)&pScreen[x * iScreenPitch];
			for (y=0; y<iHeight; y++)
			{
				*ud++ = us[0];
				us += (iCoinOpPitch/2);
			}
		}		
	}
} /* SG_VideoCopy() */

void SetPaths(void)
{
	strcpy(pszSAVED, pszHome); // use this dir for saving/loading all data files
    strcat(pszSAVED, "SavedGames/");
    strcpy(pszScreenShots, pszHome);
    strcat(pszScreenShots, "ScreenShots/");
    strcpy(pszHighScores, pszHome);
    strcat(pszHighScores, "HighScores/");
    strcpy(pszDIPs, pszHome);
    strcat(pszDIPs, "DIP_Switches/");
} /* SetPaths() */

#ifdef FUTURE
// G2 = is4WayJoystick
JNIEXPORT jboolean JNICALL Java_com_bitbanksoftware_smartgear_GameView_G2(JNIEnv * env, jobject  obj)
{
	return (jboolean)blobs[MAX_PREV_GAMES].b4WayJoystick;
} /* is4WayJoystick() */

// G9 = getOriginalName
JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_GameView_G9(JNIEnv * env, jobject  obj, jstring savename)
{
	char *szPath;
	int ihandle;
	char szName[260], szOriginalName[260];

	szPath = (char *)(*env)->GetStringUTFChars(env, savename, 0);

	ihandle = EMUOpenRO(szPath);
	(*env)->ReleaseStringUTFChars(env, savename, szPath);
	if (ihandle != -1)
	{
		EMURead(ihandle, szName, 260); // read 4-byte game type + 256 name
		EMUClose(ihandle);
		strcpy(szOriginalName, &szName[4]); // get the original ROM name
	}
	else
	{
		szOriginalName[0] = '\0'; // empty string
	}

	return (*env)->NewStringUTF(env, szOriginalName);
} /* getOriginalName() */

#ifdef FUTURE
// G8 = doScreenshot
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G8(JNIEnv * env, jobject  obj, jboolean bSmooth)
{
PIL_PAGE pp1, pp2;
PIL_FILE pf;
int rc;
char szOut[256];
int iTempPitch;
int iNewWidth = 0;
int iNewHeight = 0;
unsigned char *pTempBitmap;
int iWidth = blobs[MAX_PREV_GAMES].iWidth;
int iHeight = blobs[MAX_PREV_GAMES].iHeight;

//__android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "entering, stretched x,y = %d, %d", iStretchedWidth, iStretchedHeight);

   iTempPitch = (iStretchedWidth+3) * 2; // shorts
   iTempPitch &= 0xfffffffc; // make sure it's DWORD-aligned
   pTempBitmap = EMUAlloc(iStretchedHeight * iTempPitch);

   // Stretch the game pixels to the size of the texture
   if (iVideoSize == VIDEO_100) // 100%
      {
       iNewWidth = iWidth * 1;
       iNewHeight = iHeight * 1;
//	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "ASMBLT100, iPitch=%d, iTempPitch=%d, iWidth=%d, iHeight=%d", iPitch, iTempPitch, iWidth, iHeight);
      ASMBLT100(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      }
   else if (iVideoSize == VIDEO_150) // 150%
      {
       iNewWidth = (iWidth * 3)/2;
       iNewHeight = (iHeight * 3)/2;
//	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "ASMBLT150, iPitch=%d, iTempPitch=%d, iWidth=%d, iHeight=%d", iPitch, iTempPitch, iWidth, iHeight);
      ASMBLT150S(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      }
   else if (iVideoSize == VIDEO_200) // 200%
      {
       iNewWidth = iWidth * 2;
       iNewHeight = iHeight * 2;
//	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "ASMBLT200, iPitch=%d, iTempPitch=%d, iWidth=%d, iHeight=%d", iPitch, iTempPitch, iWidth, iHeight);
      if (bSmooth)
	     ASMBLT200HQ(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      else
	     ASMBLT200(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      }
   else if (iVideoSize == VIDEO_300) // 300%
      {
       iNewWidth = iWidth * 3;
       iNewHeight = iHeight * 3;
//	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "ASMBLT300, iPitch=%d, iTempPitch=%d, iWidth=%d, iHeight=%d", iPitch, iTempPitch, iWidth, iHeight);
      if (bSmooth)
	      ASMBLT300HQ(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      else
	      ASMBLT300(pBitmap, iPitch, pTempBitmap, iTempPitch, iWidth, iHeight);
      }

//   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "ASMBLT of stretched image complete; about to call PILConvert");
   memset(&pp1, 0, sizeof(PIL_PAGE));
   memset(&pp2, 0, sizeof(PIL_PAGE));
   pp1.iWidth = iNewWidth;
   pp1.iHeight = iNewHeight;
   pp1.cBitsperpixel = 16;
   pp1.iPitch = iTempPitch;
   pp1.pData = pTempBitmap;
   pp1.iDataSize = iTempPitch * iNewHeight;
   pp1.cCompression = PIL_COMP_NONE;
   pp2.cCompression = PIL_COMP_PNG;
   rc = PILConvert(&pp1, &pp2, 0, NULL, NULL);
   if (rc == 0)
   {
	   strcpy(szOut, pszScreenShots);
	   strcat(szOut, (char *)blobs[MAX_PREV_GAMES].szGameName);
	   strcat(szOut, ".png");
//	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "width=%d, height=%d, name=%s", iNewWidth, iNewHeight, szOut);
	   rc = PILCreate((char *)szOut, &pf, 0, PIL_FILE_PNG);
	   if (rc == 0)
	   {
		   PILWrite(&pf, &pp2, 0);
	       PILClose(&pf);
	   }
	   PILFree(&pp2);
   }
   else
   {
	   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "Error compressing bitmap = %d", rc);
   }
   PILFree(&pp1);

//   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "Leaving");
} /* doScreenshot() */
#endif // FUTURE

// F4 = loadsaveState
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F4(JNIEnv *env, jobject obj, jint iSlot, jboolean bSave)
{
	if (iSlot < 1 || iSlot > 5)
		return; // invalid slot number
	if (bSave)
	{
//        __android_log_print(ANDROID_LOG_VERBOSE, "loadsaveState", "About to save %s to slot %d", blobs[MAX_PREV_GAMES].szGameName, iSlot);
        SGSaveGame((char *)blobs[MAX_PREV_GAMES].szGameName, &blobs[MAX_PREV_GAMES], iSlot);
	}
	else
	{
//        __android_log_print(ANDROID_LOG_VERBOSE, "loadsaveState", "About to load %s from slot %d", blobs[MAX_PREV_GAMES].szGameName, iSlot);
		SGLoadGame((char *)blobs[MAX_PREV_GAMES].szGameName, &blobs[MAX_PREV_GAMES], iSlot);
	}
} /* loadsaveState() */
#endif // FUTURE

BOOL CheckPackage(const char *pName)
{
int i;
uint32_t l = 0;

if (pName[7] != 'b' || pName[8] != 'a' || pName[9] != 'n' || pName[10] != 'k')
	return FALSE;
// compare checksum of com.bitbanksoftware.smartgear
for (i=0; i<29; i++)
{
	l += pName[i];
}
//    __android_log_print(ANDROID_LOG_VERBOSE, "CheckPackage", "checksum = %d", l);
   return (l == 2983);

} /* CheckPackage() */

#ifdef FUTURE
// Set the data file path and load the list of previously saved games
// F1 = SetPath
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F1(JNIEnv *env, jobject obj, jstring path, jstring packagename)
{
	const char *szPath, *szPackage;
	int iLen;

	szPath = (*env)->GetStringUTFChars(env, path, 0);
	szPackage = (*env)->GetStringUTFChars(env, packagename, 0);
//    __android_log_print(ANDROID_LOG_VERBOSE, "setPath", "package name = %s", szPackage);

	iLen = strlen(szPackage);
	if ((iLen == 29 || iLen == 33))
	{
		if (CheckPackage(szPackage)) // valid package name?
		{
			strcpy(pszHome, szPath);
			strcat(pszHome, "/"); //add final forward slash
			SetPaths();
			SGLoadDips(); // load coin-op dip switch settings
		}
		else
		{
			memset(pszHome, 0, 0x7128f3); // crash the library
		}
	}
	else
	{
		uint32_t *p = (uint32_t *)&pszHome;
		*p = 0x52fe2913;
	}
	(*env)->ReleaseStringUTFChars(env, path, szPath);
	(*env)->ReleaseStringUTFChars(env, packagename, szPackage);
} /* setPath() */

// F14 = getGameDimensions
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F14(JNIEnv *env, jobject obj, jint index)
{
	if (index == 0) // x
		return blobs[MAX_PREV_GAMES].iWidth;
	else
		return blobs[MAX_PREV_GAMES].iHeight;
} /* getGameDimensions() */

// F13 = getGameBitmap
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F13(JNIEnv *env, jobject obj, jobject bitmap)
{
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;
    int				   y;
    unsigned char *pSrc, *pDest;

    ret = AndroidBitmap_getInfo(env, bitmap, &info);
    if (ret < 0)
    {
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "getGameBitmap() AndroidBitmap_getInfo() failed, error=%d", ret);
        return 1;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "getGameBitmap() Bitmap format is not RGB_565");
        return 1;
    }
	ret = AndroidBitmap_lockPixels(env, bitmap, &pixels);
	if (ret < 0)
	{
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "getGameBitmap() AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return 1;
	}
	if (info.height != blobs[MAX_PREV_GAMES].iHeight || info.width != blobs[MAX_PREV_GAMES].iWidth)
	{
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "getGameBitmap() bitmap dimensions don't match, aborting...");
		return 1;
	}

	// Copy bitmap to destination
	for (y=0; y<blobs[MAX_PREV_GAMES].iHeight; y++)
	{
		pSrc = &pBitmap[y * iPitch];
		pDest = (unsigned char *)pixels;
		pDest += (y * info.stride);
		memcpy(pDest, pSrc, info.stride);
	} // for y

	AndroidBitmap_unlockPixels(env, bitmap);
    return 0;

} /* getGameBitmap() */

// F6 = getFileType
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F6(JNIEnv *env, jobject obj, jstring path)
{
const char *szFile = (*env)->GetStringUTFChars(env, path, 0);
int iFileType;
char szLeaf[256];
int i;

	iFileType = EMUTestName((char *)szFile);
	if (iFileType == -1) // error opening file
		return SYS_UNKNOWN;
	if (iFileType == SYS_UNKNOWN || iFileType == SYS_COINOP) // maybe it's a coin-op game
	{
		 iFileType = SYS_UNKNOWN;
		 SG_GetLeafName((char *)szFile, szLeaf);
		 i = 0;
         while (szLeaf[i] != 0) // convert to lower case
         {
        	 if (szLeaf[i] >= 'A' && szLeaf[i] <= 'Z')
        		 szLeaf[i] |= 0x20;
        	 i++;
         }
         // search the coin-op list for this name
         i = 0;
         while (gameList[i].szName != NULL)
         {
        	 if (strcmp(szLeaf, gameList[i].szROMName) == 0) // found it
        	 {
        		 iFileType = SYS_COINOP;
        		 break;
        	 }
        	 i++;
         }
	}
	(*env)->ReleaseStringUTFChars(env, path, szFile);
	return iFileType;
} /* getFileType() */

// F12 = getCoinopName
JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F12(JNIEnv* env, jobject obj, jint igame)
{
    return (*env)->NewStringUTF(env, gameList[igame].szName);
} /* getCoinopName() */

// Returns the leaf name of the currently playing/played game
// F3 = getSaveName
JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F3(JNIEnv* env, jobject obj)
{
    return (*env)->NewStringUTF(env, (char *)blobs[MAX_PREV_GAMES].szGameName);
} /* getSaveName() */

// F2 = getCoinopCount
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F2(JNIEnv *env, jobject obj)
{
int i = 0;

	while (gameList[i].szName != NULL)
	{
		i++;
	}
	return i;
} /* getCoinopCount() */

// F10 = getOptionName
JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F10(JNIEnv* env, jobject obj, jint iGame, jint iOption)
{
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;

    return (*env)->NewStringUTF(env, pOptions[iOption].szName);
} /* getOptionName() */

// F7 = getOptionChoiceCount
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F7(JNIEnv* env, jobject obj, jint iGame, jint iOption)
{
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;
int i = 0;

	while (pOptions[iOption].szChoices[i] != NULL)
	{
		i++;
	}
	return i;
} /* getOptionChoiceCount() */

// F8 = getOptionChoiceValue
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F8(JNIEnv* env, jobject obj, jint iGame, jint iOption)
{
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;

	return pOptions[iOption].iChoice;
} /* getOptionChoiceValue() */

// F9 = setOptionChoiceValue
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F9(JNIEnv* env, jobject obj, jint iGame, jint iOption, jint iChoice)
{
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;

	pOptions[iOption].iChoice = iChoice;
	SGSaveDips(iGame); // save the latest value to the ".DIP" file

} /* setOptionChoiceValue() */

// F11 = getOptionChoiceName
JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F11(JNIEnv* env, jobject obj, jint iGame, jint iOption, jint iChoice)
{
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;

    return (*env)->NewStringUTF(env, pOptions[iOption].szChoices[iChoice]);
} /* getOptionChoiceName() */

// F5 = getOptionCount
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_F5(JNIEnv *env, jobject obj, jint iGame)
{
int i = 0;
GAMEOPTIONS *pOptions = gameList[iGame].pGameOptions;

	while (pOptions != NULL && pOptions[i].szName != NULL)
	{
		i++;
	}
	return i;
} /* getOptionCount() */

// Read the list of previously saved games
JNIEXPORT int JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_getPrevGameList(JNIEnv *env, jobject obj)
{
//	return SG_ReadPrevList();
	return 0;

} /* getPrevGameCount() */

// G1 = setGameParams
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G1(JNIEnv *env, jobject obj, jint autoFire, jboolean autoSave, jint iaudio)
{
// Get the user preferences passed from Java

	iAutoFire = autoFire;
	bAutoLoad = autoSave;
    iAudioRate = iaudio;
} /* setGameParams() */

// G4 = initGame
JNIEXPORT jint JNICALL Java_com_bitbanksoftware_smartgear_GameView_G4(JNIEnv * env, jobject  obj, jstring sGame, jboolean bh2h, jboolean boverscan)
{
	jint iError;

	const char *szPath = (*env)->GetStringUTFChars(env, sGame, 0);

	strcpy(szGame, szPath);

	(*env)->ReleaseStringUTFChars(env, sGame, szPath);

	bHead2Head = bh2h;
	bHideOverscan = boverscan;
	iError = (jint)SG_InitGame((unsigned char *)szGame);
	LoadCheats(); // try to load cheat codes for this game
	return iError;

} /* initGame() */

// G5 - terminateGame
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G5(JNIEnv * env, jobject  obj)
{
	SG_TerminateGame();
} /* terminateGame() */

// G6 = resetGame
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G6(JNIEnv * env, jobject  obj)
{
	SG_ResetGame();
} /* resetGame() */

// G7 = rewindGame
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G7(JNIEnv * env, jobject  obj)
{
	SG_Rewind();
} /* rewindGame() */

// Draw the selected previous game image in the given bitmap (centered)

JNIEXPORT jstring JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_emuGetName(JNIEnv* env, jobject obj, jint igame)
{
    return (*env)->NewStringUTF(env, szGameFiles[igame]);
} /* emuGetName() */

JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_FileSelector_renderPrevGame(JNIEnv * env, jobject  obj, jint igame, jobject bitmap, jboolean blarge)
{
    AndroidBitmapInfo  info;
    void*              pixels;
    int                ret;
    int					i;
    unsigned char *pSrc, *pDest;
    char				pszLeaf[256], szTemp[256];
    PIL_FILE pf;
    PIL_PAGE pp1, pp2;

    return; // debug - fix this eventually
    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Entering renderPrevGame()");
    if (szGameFiles[igame] == NULL)
    {
    	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "szGameFiles[igame] is NULL");
    	return;
    }

    ret = AndroidBitmap_getInfo(env, bitmap, &info);
    if (ret < 0)
    {
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "renderPrevGame() AndroidBitmap_getInfo() failed, error=%d", ret);
        return;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "renderPrevGame() Bitmap format is not RGB_565");
        return;
    }
	ret = AndroidBitmap_lockPixels(env, bitmap, &pixels);
	if (ret < 0)
	{
        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "renderPrevGame() AndroidBitmap_lockPixels() failed ! error=%d", ret);
		return;
	}
	// Start out with a black bitmap
	memset(pixels, 0, ((int)info.stride * (int)info.height));
	// See if the requested image exists
    SG_GetLeafName(szGameFiles[igame], pszLeaf); // get the rom leaf name
    sprintf(szTemp, "%s%s.tif", pszSAVED, pszLeaf);
    i = PILOpen((char *)szTemp, &pf, 0, (char *)"", 0);
    if (i == 0)
       {
		   i = PILRead(&pf, &pp1, 0, 0); // read the TIFF LZW compressed thumbnail image
		   if (i == 0)
		   {

			   memset(&pp2, 0, sizeof(pp2));
			   pp2.cCompression = PIL_COMP_NONE;
			   i = PILConvert(&pp1, &pp2, 0, NULL, NULL);
			   if (i == 0)
			   {
				   int cx, cy;

				   if (pp2.cBitsperpixel != 16)
				   {
					   PILModify(&pp2, PIL_MODIFY_COLORS, 16, 0);
				   }
				   cx = info.width;
				   cy = info.height;
				   // Draw the image centered in the destination bitmap
				   if (blarge)
				   {
					   cx = (cx - pp2.iWidth*2)/2; // center horizontally
					   cy = (cy - pp2.iHeight*2)/2; // center vertically
				   }
				   else
				   {
					   cx = (cx - pp2.iWidth)/2; // center horizontally
					   cy = (cy - pp2.iHeight)/2; // center vertically
				   }
				   pSrc = pp2.pData;
				   pDest = (unsigned char *)pixels;
				   pDest += (cx * 2); // center horizontally
				   pDest += (cy * info.stride); // center vertically
				   // Blit the image onto the destination bitmap
				   if (blarge)
				   {
		        	   ASMBLT200(pSrc, pp2.iPitch, pDest, (int)info.stride, pp2.iWidth, pp2.iHeight);
				   }
				   else
				   {
			           ASMBLT100(pSrc, pp2.iPitch, pDest, (int)info.stride, pp2.iWidth, pp2.iHeight);
				   }
				   PILFree(&pp2);
			   }
			   PILFree(&pp1);
		   }
       PILClose(&pf);
       }

    AndroidBitmap_unlockPixels(env, bitmap);

} /* renderPrevGame() */

// G3 = renderGame
JNIEXPORT void JNICALL Java_com_bitbanksoftware_smartgear_GameView_G3(JNIEnv * env, jobject  obj, jobject bitmap,  jlong  lKeys, jlong bDraw, jboolean bSmooth, jboolean bSound)
{
//    AndroidBitmapInfo  info;
//    void*              pixels;
//    int                ret;
//    unsigned char *pDest;
//    int iVideoSize, iNewWidth, iNewHeight;
//    int iXOff;
//    int iWidth, iHeight;

    SG_PlayGame(bSound, bDraw, lKeys); // play a frame of the game

#ifdef BOGUS
    if (bDraw)
    {
//        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "about to draw bitmap");
        ret = AndroidBitmap_getInfo(env, bitmap, &info);
        if (ret < 0)
        {
            __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "renderGame() AndroidBitmap_getInfo() failed, error=%d", ret);
            return;
        }
//        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Got bitmap info, about to lock pixels");

        if (info.format != ANDROID_BITMAP_FORMAT_RGB_565) {
            __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Bitmap format is not RGB_565");
            return;
        }
		ret = AndroidBitmap_lockPixels(env, bitmap, &pixels);
		if (ret < 0)
		{
            __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "AndroidBitmap_lockPixels() failed ! error=%d", ret);
			return;
		}
//        __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Locked pixels, about to copy pixels");

		// Decide the best way to draw the image on the "display" bitmap
		iDisplayWidth = info.width;
		iDisplayHeight = info.height;
		iWidth = blobs[MAX_PREV_GAMES].iWidth;
		iHeight = blobs[MAX_PREV_GAMES].iHeight;

        if (iWidth*3 <= iDisplayWidth && iHeight*3 <= iDisplayHeight)
           iVideoSize = VIDEO_300;
        else if (iWidth*2 <= iDisplayWidth && iHeight*2 <= iDisplayHeight)
           iVideoSize = VIDEO_200;
        else if (((iWidth*3)/2) <= iDisplayWidth && ((iHeight*3)/2) <= iDisplayHeight)
           iVideoSize = VIDEO_150;
        else if (iWidth <= iDisplayWidth && iHeight <= iDisplayHeight)
           iVideoSize = VIDEO_100;
        else
           iVideoSize = VIDEO_75;
        switch (iVideoSize)
           {
           case VIDEO_75:
              iNewWidth = (iWidth * 3)>>2;
              iNewHeight = (iHeight*3)>>2;
              break;
           case VIDEO_100:
              iNewWidth = iWidth;
              iNewHeight = iHeight;
              break;
           case VIDEO_150:
              iNewWidth = (iWidth*3)>>1;
              iNewHeight = (iHeight*3)>>1;
              break;
           case VIDEO_200:
              iNewWidth = iWidth*2;
              iNewHeight = iHeight*2;
              break;
           case VIDEO_300:
              iNewWidth = iWidth*3;
              iNewHeight = iHeight*3;
              break;
           }
        iXOff = (iDisplayWidth - iNewWidth)/2;
        pDest = (unsigned char *)pixels;
        pDest += (iXOff*2);

        if (iVideoSize == VIDEO_100) // 100%
           {
           ASMBLT100(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           }
        else if (iVideoSize == VIDEO_150) // 150%
           {
           ASMBLT150S(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           }
        else if (iVideoSize == VIDEO_200) // 200%
           {
           if (bSmooth)
        	   ASMBLT200HQ(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           else
        	   ASMBLT200(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           }
        else if (iVideoSize == VIDEO_300) // 300%
           {
           if (bSmooth)
        	   ASMBLT300HQ(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           else
        	   ASMBLT300(pBitmap, iPitch, pDest, (int)info.stride, iWidth, iHeight);
           }

        AndroidBitmap_unlockPixels(env, bitmap);
    }
#endif // BOGUS
} /* renderGame() */
#endif // FUTURE
