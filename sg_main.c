//
// SmartGear
//
// A multi-system game emulator
// Copyright (c) 2010-2017 BitBank Software, Inc.
// Project started 12/21/2010
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

//#include <jni.h>
//#ifdef _WIN32
//#include "android/log.h"
//#else
#include <stdint.h>
#include <unistd.h>
//#include <android/log.h>
//#endif
#include "my_windows.h"
#include <stdio.h>
#include <sys/socket.h>

// Use raw framebuffer on ARMv6 Raspberry Pi
//#if defined(__arm__)
//#define USE_FRAMEBUFFER
#include <fcntl.h>
#ifndef __MACH__
#include <linux/fb.h>
#include <sys/mman.h>
#endif // __MACH__
#include <sys/ioctl.h>
//#endif // __arm__

// Talk to RFCOMM bluetooth controllers directly
//#define BLUETOOTH
#if defined(BLUETOOTH) && !defined( __MACH__ )
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#endif // __MACH__
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <cairo.h>
#include <gtk/gtk.h>
#include <pthread.h>
//#ifndef __MACH__
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
//#include <SDL2/SDL_opengles2.h>
//#include <GLES2/gl2.h>
//#endif // __MACH__
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include "sound.h"
//#include "pil_io.h"
//#include "pil.h"

// Variables used by coin-op code that will live here for now
uint32_t ulTransMask, ulPriorityMask;
unsigned long * lCharAddr;
int iSpriteLimitX, iSpriteLimitY, iScreenPitch, iGame, iCoinOpPitch;
unsigned char cTransparent, cTransparent2;
unsigned char *pCoinOpBitmap, *pScreen, ucISOHeader[32]; 

int sample_rate; // DEBUG - external global variable used by MAME sound ym2203/ay8910
extern void LoadCheats();
char szFileName[256];
BOOL SG_Sleep(uint64_t ns);
void SG_TerminateGame(void);
uint64_t SG_Clock(void);
#ifndef __MACH__
static int fbfd; // framebuffer handle
static int iScreenSize;
//static int iScreenBpp = 16;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
int iScreenPitch, iScreenBpp;
#endif // __MACH__

//#endif // __arm__
static unsigned char *pAltScreen, *pStretchScreen;
//static int iScreenPitch;
unsigned char *pISO; // pointer to CD image
unsigned int iISOSize; // size in bytes of the image
extern unsigned char *SG_LoadISO(char *, unsigned int *);

SDL_AudioSpec m_SWantedSpec;
SDL_AudioSpec m_SSDLspec;
SDL_AudioDeviceID m_id;

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
SGNETWORK_END, // end client/server 2P game
SGNETWORK_PING, // used for benchmark tests
};
#define MAX_BLOCK_SIZE 3072
#define MAX_MEM_AREAS 10
uint32_t lMyIP;
unsigned char *pGameData = NULL; // compressed game delta info

extern unsigned char pszSAVED[]; /* Save game dir */
extern unsigned char pszHome[];
//extern GAMEDEF gameList[]; // list of coin-op games
char *szGameFourCC[10] = {"????","NES_","GG__","GBC_","TG16","GEN_","COIN","CLEC","SNES","MSX_"};
char pszCapture[256], pszCurrentDir[256], pszLastDir[256], pszGame[256];
int iGameType;
extern int iGame; // currently selected coin-op game
int iAudioHead, iAudioTail, iAudioTotal, iAudioSampleSize, iAudioAvailable;
unsigned char *pAudioBuffer;
// bluetooth variables
int iGameSocket;
int iDeviceType;

BOOL bDone = FALSE;
BOOL bPerf = FALSE;
SDL_Joystick *joy0, *joy1;
GtkListStore *store;
GtkTreeIter iter;
GtkApplication *theapp;
GtkToolItem *perfTb;
GtkToolItem *head2headTb;
GtkToolItem *audioTb;
GtkToolItem *framebufferTb;
GtkToolItem *stretchTb;
GtkToolItem *p1controlsTb, *p2controlsTb;
volatile BOOL bRunning = FALSE;
volatile BOOL bQuit = FALSE;
BOOL bAutoLoad = FALSE;
extern BOOL bHead2Head;
BOOL bSkipFrame = FALSE;
BOOL bRegistered = TRUE;
extern BOOL bAudio;
extern BOOL bFramebuffer;
extern BOOL bStretch;
BOOL bUserAbort;
#define DEFAULT_WIN_WIDTH 400
#define DEFAULT_WIN_HEIGHT 400
int iScale = 1; // Default game drawing scale
int iDisplayWidth = 320; // DEBUG
int iDisplayHeight = 480;
GtkWindow *window;
GtkBox *box, *hbox0, *hbox1, *hbox2;
GtkGrid *grid;
#define NUM_DIPS 6
GtkWidget *labelList[NUM_DIPS], *dropList[NUM_DIPS];
GtkWidget *toolbar;
static GtkWidget *darea;
//static cairo_surface_t *theimage = NULL;
int iBorderX, iBorderY;
int iScreenX;
int iScreenY;
int iVideoSize;
int iPitch;
int iFrame;
uint64_t llStartTime;
volatile uint32_t u32KeyBits, u32Joy0Bits, u32Joy1Bits, u32iCadeBits, u32ControllerBits;
extern int iP1Control, iP2Control;
BOOL bStereo = FALSE;
BOOL b16BitAudio = TRUE;
BOOL bSlave = FALSE;
BOOL bMaster = FALSE;
BOOL bUseSound = TRUE;
int iSampleRate = 2; // 44100 - DEBUG
//PIL_PAGE pp; // the source bitmap setup of the current game
//PIL_VIEW pv; // the bitmap of the display
unsigned char *pBitmap;
SERIALQ *InQ, *OutQ; // used for game link; simulate for now
unsigned short *pColorConvert;
#define MAX_PREV_GAMES 14
GAME_BLOB blobs[MAX_PREV_GAMES+1];
char szGameNames[MAX_PREV_GAMES+1][64]; // first in the list is the most recently played
char szGameFiles[MAX_PREV_GAMES+1][256];
int iPrevGame;
char *szSystemNames[] = {"NES","GameGear/SMS","GameBoy","TG16/PC-Engine","Genesis","CoinOp",NULL};
unsigned short usPalConvert[256];
void wait_vsync(void);
extern int iAudioRate;
extern void ASMBLT200HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);

static void treeviewChanged(GtkTreeSelection *sel, gpointer data);

int NES_Init(GAME_BLOB *, char *, int);
void NES_Terminate(GAME_BLOB *);
void NES_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);

int GG_Init(GAME_BLOB *, char *, int);
void GG_Terminate(GAME_BLOB *);
void GG_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);

int GB_Init(GAME_BLOB *, char *, int);
void GB_Terminate(GAME_BLOB *);
void GB_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);

int GEN_Init(GAME_BLOB *, char *, int);
void GEN_Terminate(GAME_BLOB *);
void GEN_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
int EMUTestName(char *);
void * SG_RunFrames(void *);
void SG_WritePrevList(void);
int SG_InitGame(unsigned char *pszROM);
static void sdlCallback(void *userdata, uint8_t *stream, int len);
static int SG_SDLEventFilter(void *userdata, SDL_Event *event);
int iWindowX, iWindowY;
SDL_Window *sdlwindow = NULL;
SDL_Surface *surface; // surface used to hold the game graphics
SDL_Surface *surfaceScreen; // the display
SDL_Renderer *renderer;
SDL_Texture *texture;
//void MSXPlay(HANDLE hInst, HWND hWnd, char *szGame);
//void MSXReset(void);
//BOOL MSXLoadGame(int);

//void SNESPlay(HANDLE hInst, HWND hWnd, char *szGame);
//void SNESReset(void);
//BOOL SNESLoadGame(int);

//int CoinOp_Init(GAME_BLOB *, char *, int);
//void CoinOp_Terminate(GAME_BLOB *);
//void CoinOp_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);

//int PCE_Init(GAME_BLOB *, char *, int);
//void PCE_Terminate(GAME_BLOB *);
//void PCE_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);

// structure holding info for all of the emulators
EMULATOR emulators[7] = {{NULL, NULL, NULL}, // SYS_UNKNOWN
                        {NES_Init, NES_Terminate, NES_Play},
                        {GG_Init, GG_Terminate, GG_Play},
                        {GB_Init, GB_Terminate, GB_Play}};
//                        {PCE_Init, PCE_Terminate, PCE_Play},
//                        {GEN_Init, GEN_Terminate, GEN_Play},
//                        {CoinOp_Init, CoinOp_Terminate, CoinOp_Play}};

unsigned char ucMirror[256]=
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

// Save the image of the game along with the state
void SG_SaveGameImage(int iGame)
{
#ifdef FUTURE
PIL_PAGE pp1, pp2;
PIL_FILE pf;
int rc;
char szOut[256];
//unsigned char *pTempBitmap;
int iWidth = blobs[MAX_PREV_GAMES].iWidth;
int iHeight = blobs[MAX_PREV_GAMES].iHeight;

//__android_log_print(ANDROID_LOG_VERBOSE, "SG_SaveGameImage()", "entering");

   memset(&pp1, 0, sizeof(PIL_PAGE));
   memset(&pp2, 0, sizeof(PIL_PAGE));
   pp1.iWidth = iWidth;
   pp1.iHeight = iHeight;
   pp1.cBitsperpixel = 16;
   pp1.iPitch = iPitch;
   pp1.pData = pBitmap;
   pp1.iDataSize = iPitch * iHeight;
   pp1.cCompression = PIL_COMP_NONE;
   pp2.cCompression = PIL_COMP_PNG;
   rc = PILConvert(&pp1, &pp2, 0, NULL, NULL);
   if (rc == 0)
   {
	   sprintf(szOut, "%s%s%s%02d.png", pszSAVED, blobs[MAX_PREV_GAMES].szGameName, szGameFourCC[iGameType], iGame);
//	   __android_log_print(ANDROID_LOG_VERBOSE, "SG_SaveGameImage()", "width=%d, height=%d, name=%s", iWidth, iHeight, szOut);
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
//	   __android_log_print(ANDROID_LOG_VERBOSE, "SG_SaveGameImage", "Error compressing bitmap = %d", rc);
   }

//   __android_log_print(ANDROID_LOG_VERBOSE, "ScreenShot", "Leaving");
#endif // FUTURE
} /* SG_SaveGameImage() */

void SG_PushSamples(unsigned char *pSamples, int iCount)
{
	if (iAudioAvailable + iCount > iAudioTotal)
	{
//		g_print("too much audio generated\n");
		return; // too much audio, throw it away
	}
//	g_print("entering SG_PushSamples(), pSamples =%d, pAudioBuffer=%d\n",(int)pSamples, (int)pAudioBuffer);
	if (iAudioTail + iCount <= iAudioTotal) // simple case, no wrap around
	{
		memcpy(&pAudioBuffer[iAudioTail*iAudioSampleSize], pSamples, iCount*iAudioSampleSize);
		iAudioTail += iCount;
		if (iAudioTail == iAudioTotal) // need to wrap
			iAudioTail = 0; 
	}
	else // have to wrap around
	{
		int iFirst = iAudioTotal - iAudioTail;
		memcpy(&pAudioBuffer[iAudioTail], pSamples, iFirst*iAudioSampleSize);
		memcpy(pAudioBuffer, &pSamples[iFirst*iAudioSampleSize], (iCount-iFirst)*iAudioSampleSize);
		iAudioTail = iCount - iFirst;
	}
	iAudioAvailable += iCount;
//	g_print("leaving SG_PushSamples(), head=%d, tail=%d\n", iAudioHead, iAudioTail);
} /* SG_PushSamples() */

void SG_PopSamples(unsigned char *pSamples, int iCount)
{
 	if (iAudioAvailable < iCount) // g_print("not enough audio available\n");
	{
		memset(pSamples, 0, iCount * iAudioSampleSize);
		return;
	}
	if (iAudioHead + iCount <= iAudioTotal) // simple case, no wrap around
	{
		memcpy(pSamples, &pAudioBuffer[iAudioHead*iAudioSampleSize], iCount*iAudioSampleSize);
		iAudioHead += iCount;
		if (iAudioHead == iAudioTotal)
			iAudioHead = 0;
	}
	else // must wrap around
	{
		int iFirst = iAudioTotal - iAudioHead;
		memcpy(pSamples, &pAudioBuffer[iAudioHead*iAudioSampleSize], iFirst*iAudioSampleSize);
		memcpy(&pSamples[iFirst*iAudioSampleSize], pAudioBuffer, (iCount-iFirst)*iAudioSampleSize);
		iAudioHead = iCount - iFirst;
	}
	iAudioAvailable -= iCount;
//	g_print("leaving SG_PopSamples(), head=%d, tail=%d\n", iAudioHead, iAudioTail);
} /* SG_PopSamples() */

#ifdef FUTURE
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
struct timespec res;
int iTime;

   if (theimage != NULL)
   {
      clock_gettime(CLOCK_MONOTONIC, &res);
      iTime = 1000*res.tv_sec + res.tv_nsec/1000000;
      cairo_surface_flush(theimage);
//      pv.pBitmap = cairo_image_surface_get_data(theimage);
// DEBUG
//      PILDraw(&pp, &pv, TRUE, NULL);
      clock_gettime(CLOCK_MONOTONIC, &res);
      iTime = (1000*res.tv_sec + res.tv_nsec/1000000) - iTime;
//      g_print("PILDraw time = %dms\n", iTime);
      cairo_surface_mark_dirty(theimage);
      cairo_set_source_surface(cr, theimage, iBorderX, iBorderY);
      clock_gettime(CLOCK_MONOTONIC, &res);
      iTime = 1000*res.tv_sec + res.tv_nsec/1000000;
      cairo_paint(cr);
// Draw any border areas in black
//      cairo_set_source_rgb(cr, 0, 0, 0);
//      cairo_rectangle(cr, 0, 0, iScreenX, iBorderY);
//      cairo_fill(cr);
      return TRUE;
   }
   return FALSE;
} /* on_draw_event() */
#endif // FUTURE

void SG_StopAudio()
{
	if (m_SSDLspec.freq != 0) // close the old setup first
	{
		SDL_CloseAudio();
		SDL_zero(m_SSDLspec);
	}

} /* SG_StopAudio() */

void SG_QuitGame()
{
	bQuit = FALSE;
	bRunning = FALSE;
	SG_TerminateGame(); // clean up resources of this game
	SG_Sleep(20000L); // allow second thread to see that we're done running
//	g_print("About to free surface\n");
//	SDL_FreeSurface(surface);
	g_print("About to destroy SDL window\n");
	if (m_id != 0)
	{
		SDL_CloseAudioDevice(m_id);
	}
	EMUFree(pAltScreen);
	if (bStretch) EMUFree(pStretchScreen);
#ifndef __MACH__
    if (bFramebuffer)
	{
		munmap(pScreen, iScreenSize);
		close(fbfd);
	}
	else
#endif // __MACH__
           if (!bPerf)
	{
		SDL_DestroyTexture(texture);
		SDL_DestroyRenderer(renderer);
	}
	SDL_DestroyWindow(sdlwindow);
	sdlwindow = NULL;
//#endif // !USE_FRAMEBUFFER
	g_print("returned from SDL_DestroyWindow()\n");
	llStartTime = SG_Clock() - llStartTime;
	g_print("total frames = %d, avg FPS = %f\n", iFrame, (float)iFrame / ((float)llStartTime / 1000000000.0));
} /* SG_QuitGame() */

static void perfToggled(GtkWidget *widget, gpointer user_data)
{
        bPerf = !bPerf;
}

#ifdef FUTURE
static void dareaChanged(GtkWidget *widget, GtkAllocation *allocation, void *data)
{
    if (allocation->width != iScreenX || allocation->height != iScreenY) // size changed
    {
	iScreenX = allocation->width;
	iScreenY = allocation->height;
	if (theimage != NULL)
	{
		cairo_surface_destroy(theimage);
		theimage = NULL;
	}
        theimage = cairo_image_surface_create(CAIRO_FORMAT_RGB16_565, iScreenX, iScreenY);
//	memset(&pv, 0, sizeof(pv));
//	pv.iScaleX = pv.iScaleY = 128;
//	pv.iWidth = iScreenX;
//	pv.iHeight = iScreenY;
//	pv.iPitch = pv.iWidth*2;
       g_print ("drawing area size changed: %d x %d\n", allocation->width, allocation->height);

//       updateView(VIEW_FIT_TO_WINDOW, 0, 0, 0);
    }

} /* dareaChanged() */
#endif // FUTURE

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_Rewind(void)                                            *
 *                                                                          *
 *  PURPOSE    : Rewind the currently running game.                         *
 *                                                                          *
 ****************************************************************************/
void SG_Rewind(void)
{
int i, j;
// find the rewind buffer that's at least 4 seconds away
//   __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "current frame = %d, 0=%d, 1=%d, 2=%d, 3=%d", iFrame, blobs[MAX_PREV_GAMES].iRewindFrame[0],blobs[MAX_PREV_GAMES].iRewindFrame[1], blobs[MAX_PREV_GAMES].iRewindFrame[2],blobs[MAX_PREV_GAMES].iRewindFrame[3]);
   for (i=0; i<4; i++)
      {
      j = iFrame - blobs[MAX_PREV_GAMES].iRewindFrame[i];
      if (j >= 480 && j < 720) // at least 8 seconds, but not more than 12
         {
//    	 __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "loading slot %d", i);
         SGLoadGame(NULL, &blobs[MAX_PREV_GAMES], i);
         iFrame = blobs[MAX_PREV_GAMES].iRewindFrame[i]; // set current frame to this one so that it can be rewound further
         return;
         }
      }
 // no valid rewind buffers found
// __android_log_print(ANDROID_LOG_VERBOSE, "SG_Rewind", "no valid rewind slot found");
} /* SG_Rewind() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_Status()                                                *
 *                                                                          *
 *  PURPOSE    : Return status about the currently running game.            *
 *                                                                          *
 ****************************************************************************/
int SG_Status(int *pHigh, int *pCoins)
{
    if (*emulators[iGameType].pfnStatus)
        return (*emulators[iGameType].pfnStatus)(pHigh, pCoins);
    else
        return -1;
} /* SG_Status() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_ResetGame(void)                                         *
 *                                                                          *
 *  PURPOSE    : Reset the currently running game.                          *
 *                                                                          *
 ****************************************************************************/
void SG_ResetGame(unsigned char *pszGame)
{
	(*emulators[iGameType].pfnInit)(&blobs[MAX_PREV_GAMES], NULL, -1);
} /* SG_ResetGame() */

void SG_InitSDLAudio(void)
{
	m_SWantedSpec.freq = blobs[MAX_PREV_GAMES].iAudioSampleRate;
	sample_rate = blobs[MAX_PREV_GAMES].iAudioSampleRate; // DEBUG
	if (blobs[MAX_PREV_GAMES].b16BitAudio)
	{
		m_SWantedSpec.format = AUDIO_S16LSB;
	}
	else
	{
		m_SWantedSpec.format = AUDIO_U8;
	}
	m_SWantedSpec.channels = blobs[MAX_PREV_GAMES].iSoundChannels;
	m_SWantedSpec.silence = 0;
// A value of 256 works find on x86 Linux machines, but causes the RPi to
// slow the framerate to 37fps. The value which works on RPi is 512
	m_SWantedSpec.samples = 1024; //512; // must be a power of 2
	m_SWantedSpec.callback = sdlCallback;
	m_SWantedSpec.userdata = NULL;
	m_id = SDL_OpenAudioDevice(NULL, 0, &m_SWantedSpec, &m_SSDLspec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
//        m_id = 0;
//	if (SDL_OpenAudio(&m_SWantedSpec, &m_SSDLspec) < 0)
	if (m_id == 0) // something went wrong
	{
		g_print("SDL_OpenAudio: %s\n", SDL_GetError());
	}
	else
	{
		// push one frame of silence to get started
//		SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
		g_print("Audio started - %d bps, samples=%d, chan=%d, freq=%d, format=%08x\n", blobs[MAX_PREV_GAMES].b16BitAudio ? 16:8, m_SSDLspec.samples, m_SSDLspec.channels, m_SSDLspec.freq, m_SSDLspec.format);
		SDL_PauseAudioDevice(m_id, 0);
//		SDL_PauseAudio(0); // start it playing
	}
} /* SG_InitSDLAudio() */

void SG_SetScale(int scale)
{
	if (scale != iScale)
	{
		int iStretchScale = (bStretch) ? 2:1;
		SDL_SetWindowSize(sdlwindow, blobs[MAX_PREV_GAMES].iWidth*scale*iStretchScale, blobs[MAX_PREV_GAMES].iHeight*scale*iStretchScale);
		iScale = scale;
	}
} /* SG_SetScale() */

static int SG_SDLEventFilter(void *userdata, SDL_Event *event)
{
volatile uint32_t *pBits;

        if (event->type == SDL_JOYBUTTONDOWN || event->type == SDL_JOYBUTTONUP)
        {
         uint32_t ulMask = 0;

         SDL_JoyButtonEvent *jbe = (SDL_JoyButtonEvent *)event;
	if (jbe->which == 1) // second joystick = player 2
		pBits = &u32Joy1Bits;
	else
	    pBits = &u32Joy0Bits;
         if (jbe->button >= 0 && jbe->button < 8) // we only use 8 buttons
         {
            ulMask = RKEY_BUTTA_P1 << (jbe->button);
            if (jbe->state == SDL_PRESSED)
               *pBits |= ulMask;
            else
               *pBits &= ~ulMask;
         }
        }
        else if (event->type == SDL_JOYAXISMOTION)
        {
        SDL_JoyAxisEvent *jae = (SDL_JoyAxisEvent *)event;
	if (jae->which == 1) // second joystick = player 2
		pBits = &u32Joy1Bits;
	else
	    pBits = &u32Joy0Bits;
            if (jae->axis == 0) // || jae->axis == 4) // X
            {
               *pBits &= ~(RKEY_LEFT_P1 | RKEY_RIGHT_P1);
               if (jae->value >= 16384) *pBits |= RKEY_RIGHT_P1;
               else if (jae->value <= -16384) *pBits |= RKEY_LEFT_P1;
            }
            else if (jae->axis == 1)// || jae->axis == 5) // must be Y
            {
               *pBits &= ~(RKEY_DOWN_P1 | RKEY_UP_P1);
               if (jae->value >= 16384) *pBits |= RKEY_DOWN_P1;
               else if (jae->value <= -16384) *pBits |= RKEY_UP_P1;
            }
        }
        else if (event->type == SDL_JOYHATMOTION) // d-pad on gamepads
        {
        SDL_JoyHatEvent *jhe = (SDL_JoyHatEvent *)event;
   		if (jhe->which == 1) // second joystick = player 2
			pBits = &u32Joy1Bits;
		else
	    	pBits = &u32Joy0Bits;
            *pBits &= ~(RKEY_LEFT_P1 | RKEY_RIGHT_P1 | RKEY_UP_P1 | RKEY_DOWN_P1);
            if (jhe->value & SDL_HAT_UP)    *pBits |= RKEY_UP_P1;
            if (jhe->value & SDL_HAT_DOWN)  *pBits |= RKEY_DOWN_P1;
            if (jhe->value & SDL_HAT_LEFT)  *pBits |= RKEY_LEFT_P1;
            if (jhe->value & SDL_HAT_RIGHT) *pBits |= RKEY_RIGHT_P1;
        }
	else if (event->type == SDL_KEYDOWN) // capture the key
	{
		switch (event->key.keysym.sym)
		{
		case SDLK_o:
			u32iCadeBits |= RKEY_START_P1;
			break;
		case SDLK_g:
			u32iCadeBits &= ~RKEY_START_P1;
			break;
		case SDLK_l:
			u32iCadeBits |= RKEY_SELECT_P1;
			break;
		case SDLK_v:
			u32iCadeBits &= ~RKEY_SELECT_P1;
			break;
		case SDLK_w:
			u32iCadeBits |= RKEY_UP_P1;
			break;
		case SDLK_e:
			u32iCadeBits &= ~RKEY_UP_P1;
			break;
		case SDLK_x:
			u32iCadeBits |= RKEY_DOWN_P1;
			break;
		case SDLK_z:
			u32iCadeBits &= ~RKEY_DOWN_P1;
			break;
		case SDLK_a:
			u32iCadeBits |= RKEY_LEFT_P1;
			break;
		case SDLK_q:
			u32iCadeBits &= ~RKEY_LEFT_P1;
			break;
		case SDLK_d:
			u32iCadeBits |= RKEY_RIGHT_P1;
			break;
		case SDLK_c:
			u32iCadeBits &= ~RKEY_RIGHT_P1;
			break;
		case SDLK_u:
			u32iCadeBits |= RKEY_BUTTA_P1;
			break;
		case SDLK_f:
			u32iCadeBits &= ~RKEY_BUTTA_P1;
			break;
		case SDLK_h:
			u32iCadeBits |= RKEY_BUTTB_P1;
			break;
		case SDLK_r:
			u32iCadeBits &= ~RKEY_BUTTB_P1;
			break;
		case SDLK_j:
			u32iCadeBits |= RKEY_BUTTC_P1;
			break;
		case SDLK_n:
			u32iCadeBits &= ~RKEY_BUTTC_P1;
			break;
		case SDLK_y:
			u32iCadeBits |= RKEY_BUTTD_P1;
			break;
		case SDLK_t:
			u32iCadeBits &= ~RKEY_BUTTD_P1;
			break;
		case SDLK_ESCAPE:
			bQuit = TRUE;
		break;
		} // switch on icade codes

		switch (event->key.keysym.sym)
		{
		case SDLK_1: // change video size
		case SDLK_2:
		case SDLK_3:
		case SDLK_4:
		case SDLK_5:
			SG_SetScale(event->key.keysym.sym - SDLK_1 + 1);
			break;
		case SDLK_UP:
			u32KeyBits |= RKEY_UP_P1;
		break;
		case SDLK_DOWN:
			u32KeyBits |= RKEY_DOWN_P1;
		break;
		case SDLK_LEFT:
			u32KeyBits |= RKEY_LEFT_P1;
		break;
		case SDLK_RIGHT:
			u32KeyBits |= RKEY_RIGHT_P1;
		break;
		case SDLK_a:
			u32KeyBits |= RKEY_BUTTA_P1;
		break;
		case SDLK_s:
			u32KeyBits |= RKEY_BUTTB_P1;
		break;
//		case SDLK_d:
//			u32KeyBits |= RKEY_BUTTC_P1;
//		break;
		case SDLK_RETURN:
			u32KeyBits |= RKEY_START_P1;
		break;
		case SDLK_SPACE:
			u32KeyBits |= RKEY_SELECT_P1;
		break;
		case SDLK_ESCAPE:
			bQuit = TRUE;
		break;
		}
		return 0;
	}
	else if (event->type == SDL_KEYUP)
	{
		switch (event->key.keysym.sym)
		{
		case SDLK_UP:
			u32KeyBits &= ~RKEY_UP_P1;
		break;
		case SDLK_DOWN:
			u32KeyBits &= ~RKEY_DOWN_P1;
		break;
		case SDLK_LEFT:
			u32KeyBits &= ~RKEY_LEFT_P1;
		break;
		case SDLK_RIGHT:
			u32KeyBits &= ~RKEY_RIGHT_P1;
		break;
		case SDLK_a:
			u32KeyBits &= ~RKEY_BUTTA_P1;
		break;
		case SDLK_s:
			u32KeyBits &= ~RKEY_BUTTB_P1;
		break;
//		case SDLK_c:
//			u32KeyBits &= ~RKEY_BUTTC_P1;
//		break;
		case SDLK_RETURN:
			u32KeyBits &= ~RKEY_START_P1;
		break;
		case SDLK_SPACE:
			u32KeyBits &= ~RKEY_SELECT_P1;
		break;
		}
		return 0;
	}
	return 1;
} /* SG_SDLEventFilter() */

void SG_InitSDLGraphics(GAME_BLOB *pBlob)
{
int x, y;//, numdisplays;
SDL_Rect r;
int iDestMonitor;

	gtk_window_get_position(window, &x, &y); // get our window corner coordinates
//	numdisplays = SDL_GetNumVideoDisplays();
   	SDL_GetDisplayBounds(0, &r);
	if (x > r.w) // we're on the second monitor
		iDestMonitor = 1;
	else
		iDestMonitor = 0;

   iPitch = pBlob->iPitch = (pBlob->iWidth + 32) * 2;
   pAltScreen = EMUAlloc(iPitch * (pBlob->iHeight + 32));
   if (bStretch)
   {
   		pStretchScreen = EMUAlloc(iPitch * 2 * (pBlob->iHeight*2));
   }
   if (bFramebuffer)
   {
#ifndef __MACH__
// On ARM devices we can use the framebuffer
   	fbfd = open("/dev/fb0", O_RDWR);
	   if (fbfd)
	   {
		int cx, cy;
		cx = pBlob->iWidth; cy = pBlob->iHeight;
		if (bStretch) { cx<<=1; cy<<=1; }
	      // get the fixed screen info
	      ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo);
	      // get the variable screen info
	      ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo);
              iScreenBpp = vinfo.bits_per_pixel;
	      iScreenPitch = (vinfo.xres * vinfo.bits_per_pixel) / 8;
	      iScreenSize = finfo.smem_len;
	      pScreen = (unsigned char *)mmap(0, iScreenSize, PROT_READ | PROT_WRITE,
				MAP_SHARED, fbfd, 0);
		sdlwindow = SDL_CreateWindow("SmartGear", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		cx, cy, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
		SDL_GetWindowPosition(sdlwindow, &iWindowX, &iWindowY);
		g_print("SDL window x = %d, y = %d, bpp=%d\n", iWindowX, iWindowY, vinfo.bits_per_pixel);
	   }
	   else
	   {
	      g_print("Can't open framebuffer\n");
	   }
#endif // __MACH__
   } // framebuffer
   else
   {
	if (!bPerf)
	{
		int iStretchScale = (bStretch) ? 2:1;
//		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
//		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
//		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

		sdlwindow = SDL_CreateWindow("SmartGear", SDL_WINDOWPOS_CENTERED_DISPLAY(iDestMonitor) ,
			 SDL_WINDOWPOS_CENTERED_DISPLAY(iDestMonitor),
		pBlob->iWidth*iScale*iStretchScale, pBlob->iHeight*iScale*iStretchScale, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
//	gl_context = SDL_GL_CreateContext(sdlwindow);
//	g_print("%s", glGetString(GL_VERSION));
//	SDL_GL_DeleteContext(gl_context);
		renderer = SDL_CreateRenderer(sdlwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (renderer == NULL)
		{
			g_print("SDL_CreateRenderer failed");
		}
		else
		{
			SDL_RendererInfo ri;
			SDL_GetRendererInfo(renderer, &ri);
			g_print("Renderer: %s software=%d, accelerated=%d, presentvsync=%d, targettexture=%d\n",
			ri.name, (ri.flags & SDL_RENDERER_SOFTWARE) != 0,
			(ri.flags & SDL_RENDERER_ACCELERATED) != 0,
			(ri.flags & SDL_RENDERER_PRESENTVSYNC) != 0,
			(ri.flags & SDL_RENDERER_TARGETTEXTURE) != 0);
		}
//	surfaceScreen = SDL_GetWindowSurface(sdlwindow);
//	surface = SDL_CreateRGBSurface(0, pBlob->iWidth+32, pBlob->iHeight+32, 16, 0xf800, 0x7e0, 0x1f, 0);
//	if (surface == NULL) // something went wrong
//	{
//		g_print("SDL_CreateRGBSurface failed\n");
//	}
		texture = SDL_CreateTexture(renderer,
//			SDL_PIXELFORMAT_ARGB8888,
                            SDL_PIXELFORMAT_RGB565,
                            SDL_TEXTUREACCESS_STREAMING,
                            1024, 1024);
		if (texture == NULL)
		{
			g_print("SDL_CreateTexture failed");
		}
	} // if !bPerf
//	iPitch = pBlob->iPitch = surface->pitch;
//	pBlob->pBitmap0 = (unsigned char *)surface->pixels;
//	blobs[MAX_PREV_GAMES].pBitmap0 = EMUAlloc((blobs[MAX_PREV_GAMES].iWidth+32)*(blobs[MAX_PREV_GAMES].iHeight+32)*2); // allow for some slop over (PCE)
//	blobs[MAX_PREV_GAMES].pBitmap1 = EMUAlloc((blobs[MAX_PREV_GAMES].iWidth+32)*(blobs[MAX_PREV_GAMES].iHeight+32)*2); // allow for some slop over (PCE)
//	pBlob->pBitmap0 += (32 + 16*iPitch); // slop over
//	pBlob->pBitmap1 += (32 + 16*iPitch); // slop over
//			iPitch = blobs[MAX_PREV_GAMES].iPitch = (blobs[MAX_PREV_GAMES].iWidth+32)*2;
        } // if !bFramebuffer
	pBlob->pBitmap = (void *)1; // to tell game loop we've initialized the texture
} /* SG_InitSDLGraphics() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_InitGame(unsigned char *)                               *
 *                                                                          *
 *  PURPOSE    : Detect game type and try to load a game ROM.               *
 *                                                                          *
 *  RETURNS    : 0 for success, -1 for error                                *
 *                                                                          *
 ****************************************************************************/
int SG_InitGame(unsigned char *pszROM)
{
char pszTemp[256], cTemp[256];
int iError = SG_NO_ERROR;
int iGame;
int i;
void * ihandle;
int iLen;

// Clear any old audio
        memset(pAudioBuffer, 0, 44100*4); // maximum buffer size = 1 second
        iAudioTail = iAudioHead = 0;
        iAudioAvailable = 0;

	strcpy(pszGame, (const char *)pszROM);
	g_print("entering SG_InitGame(), file=%s\n", pszGame);

	iLen = strlen(pszGame);
	if (strcmp(&pszGame[iLen-6],".sgsav") == 0) // it's a save game file, extract the rom name
	{
		char szName[260];
		ihandle = EMUOpenRO(pszGame);
		if (ihandle != (void *)-1)
		{
			EMURead(ihandle, szName, 260); // read 4-byte game type + 256 name
			EMUClose(ihandle);
			strcpy(pszGame, &szName[4]); // get the original ROM name
		}
		else
		{
			iError = SG_GENERAL_ERROR;
			goto sg_initexit;
		}
	}

    iGameType = EMUTestName(pszGame); // see if the file is for NES, GameBoy or GameGear (or something else)
    g_print("iGameType=%d\n", iGameType);
    if (iGameType) // if the game file has a .NES, .GG, GB, or .GBC extension, then try to load it
    {
        InQ = (SERIALQ *)EMUAlloc(4096);
        OutQ = (SERIALQ *)EMUAlloc(4096);
        bHead2Head = gtk_toggle_button_get_active((GtkToggleButton *)head2headTb);
        bAudio = gtk_toggle_button_get_active((GtkToggleButton *)audioTb);
	bFramebuffer = gtk_toggle_button_get_active((GtkToggleButton *)framebufferTb);
	bStretch = gtk_toggle_button_get_active((GtkToggleButton *)stretchTb);
        blobs[MAX_PREV_GAMES].bHead2Head = bHead2Head;
        blobs[MAX_PREV_GAMES].b4WayJoystick = 0; // assume 8-way joysticks
	bUseSound = bAudio;
        if (bUseSound)
           {
           if (iGameType == SYS_GENESIS || iGameType == SYS_COINOP || iGameType == SYS_TG16 || iGameType == SYS_GAMEBOY)
              {
              bStereo = TRUE; // these systems need 16-bit stereo always
              b16BitAudio = TRUE;
	      iAudioSampleSize = 4;
              }
	   else
	      {
	      bStereo = FALSE;
	      b16BitAudio = TRUE;
	      iAudioSampleSize = 2;
              }
	   iAudioRate = 44100; // DEBUG
           blobs[MAX_PREV_GAMES].iSoundChannels = (bStereo) ? 2:1;
           blobs[MAX_PREV_GAMES].iAudioSampleRate = iAudioRate;
           EMUOpenSound(blobs[MAX_PREV_GAMES].iAudioSampleRate, b16BitAudio ? 16:8, blobs[MAX_PREV_GAMES].iSoundChannels);
           blobs[MAX_PREV_GAMES].pAudioBuf = (signed short *)pSoundBuf;
           blobs[MAX_PREV_GAMES].iAudioBlockSize = iSoundBlock;
           blobs[MAX_PREV_GAMES].iSampleCount = iSoundBlock;
           if (blobs[MAX_PREV_GAMES].iSoundChannels == 2)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
           blobs[MAX_PREV_GAMES].b16BitAudio = b16BitAudio;
           if (b16BitAudio)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
           g_print("samplecount=%d, blocksize=%d\n", blobs[MAX_PREV_GAMES].iSampleCount, iSoundBlock);
           if (!bPerf && bAudio)
              {
	          SG_InitSDLAudio();
              }
           else
              {
                  m_id = 0;
              }
           }
        // See if the file passed to us is a savegame file
        iGame = bAutoLoad ? 0 : -1;
        strcpy(pszTemp, pszGame);
        i = strlen(pszGame);
        if (strcmp(&pszGame[i-5],"sgsav") == 0) // it's a savegame file
           {
           // get the rom filename from the savegame file
           ihandle = EMUOpenRO(pszGame);
           EMUSeek(ihandle, 4, 0); // seek to filename
           EMURead(ihandle, cTemp, 256);
           EMUClose(ihandle);
           strcpy(pszTemp, cTemp);
           i = strlen(pszGame);
           iGame = (int)(pszGame[i-7] - '0');
           }

	iFrame = 0;
	llStartTime = SG_Clock();
	u32iCadeBits = u32Joy0Bits = u32Joy1Bits = u32KeyBits = 0;
	// Get the user's preferences for controller type
	iP1Control = gtk_combo_box_get_active((GtkComboBox *)p1controlsTb);
	iP2Control = gtk_combo_box_get_active((GtkComboBox *)p2controlsTb);

	g_print("About to call game init\n");
        iError = (*emulators[iGameType].pfnInit)(&blobs[MAX_PREV_GAMES], (char *)pszTemp, iGame);
	g_print("Returned from game init - cx=%d, cy=%d\n", blobs[MAX_PREV_GAMES].iWidth, blobs[MAX_PREV_GAMES].iHeight);
        if (iError == SG_NO_ERROR)
        {
			// process 1 frame so that TurboGfx-16 games can set the video mode
			(*emulators[iGameType].pfnPlay)(&blobs[MAX_PREV_GAMES], FALSE, FALSE, 0);
        	// allocate 2 bitmaps for double buffering
			blobs[MAX_PREV_GAMES].iBitmapFrame0 = -1;
			blobs[MAX_PREV_GAMES].iBitmapFrame1 = -1;
			blobs[MAX_PREV_GAMES].pBitmap = NULL;
//			blobs[MAX_PREV_GAMES].pBitmap = blobs[MAX_PREV_GAMES].pBitmap0;
			pBitmap = (unsigned char *)blobs[MAX_PREV_GAMES].pBitmap;
//                        memset(&pp, 0, sizeof(pp)); // set up source image as a PIL_PAGE
//			pp.pData = pBitmap;
//			pp.iWidth = blobs[MAX_PREV_GAMES].iWidth;
//			pp.iHeight = blobs[MAX_PREV_GAMES].iHeight;
//			pp.iPitch = iPitch;
//			pp.cBitsperpixel = 16;
//			pp.iDataSize = pp.iPitch * pp.iHeight;
//			pp.cCompression = PIL_COMP_NONE;
//		    g_print("pBitmap=0x%08x, iPitch=%d\n", (int)pBitmap, iPitch);
            // allocate 4 buffers for "rewind" save state data
	        for (i=0; i<4; i++)
	           {
	           blobs[MAX_PREV_GAMES].pRewind[i] = EMUAlloc(MAX_SAVE_SIZE);
	           }
	        blobs[MAX_PREV_GAMES].iRewindIndex = 0;
            bUserAbort = FALSE;
        }

    }
    else // not recognized
    {
    	iError = SG_ERROR_UNSUPPORTED;
    }

    sg_initexit:

	return iError;
} /* SG_InitGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_PlayGame(bool, bool, uint32_t)                     *
 *                                                                          *
 *  PURPOSE    : Execute a single frame of the current game.                *
 *                                                                          *
 ****************************************************************************/
void SG_PlayGame(BOOL bUseAudio, BOOL bVideo, uint32_t ulKeys)
{
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
//int iFrameUsed;
//volatile int iTimeOut;
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_PlayGame(), iGameType=%d", iGameType);

//	wait_vsync();

//	iTimeOut = 20;
//	while (pBlob->iBitmapFrame1 != -1 && pBlob->iBitmapFrame0 != -1 && iTimeOut > 0) // opengl thread is still using the buffers we created
//	{
//		iTimeOut--; // allow up to 10ms for opengl thread to finish its work
//		usleep(500);
//	}
//	if (pBlob->iBitmapFrame1 == -1) // use the buffer that's already been used by the render thread
//	{
//		pBitmap = pBlob->pBitmap = pBlob->pBitmap1;
//		iFrameUsed = 1;
//	}
//	else
//	{
//		pBitmap = pBlob->pBitmap = pBlob->pBitmap0;
//		iFrameUsed = 0;
//	}

//	if (iTimeOut == 0) // OpenGL is dragging, don't draw anything
//		bVideo = FALSE;
	(*emulators[iGameType].pfnPlay)(pBlob, bUseAudio, bVideo, ulKeys); // Execute 1 frame of the game

//	if (iFrameUsed == 1) // we used frame buffer 1
//    	pBlob->iBitmapFrame1 = iFrame;
//    else
 //   	pBlob->iBitmapFrame0 = iFrame;

    if ((iFrame & 0xff) == 0) // save the state every 4 seconds for rewind
       {
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "calling SGSaveGame at frame %d for rewind function", iFrame);
//       pBlob->iRewindFrame[pBlob->iRewindIndex] = iFrame;
//       SGSaveGame(NULL, pBlob, 0);
       }
//    if (!bSkipFrame)
//       {
//       if (bTouchMode && iOpacity != 0) // display touchscreen controls
//          SGDrawTouchControls(&blobs[MAX_PREV_GAMES]);
//       EMUScreenUpdate(blobs[MAX_PREV_GAMES].iWidth, blobs[MAX_PREV_GAMES].iHeight);
//       }
    iFrame++;
    pBlob->iFrame = iFrame; // for Genesis ROM patching
//    if (!bRegistered && iFrame > 2*60*61)     // stop play after two minutes
 //      bUserAbort = TRUE;
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_PlayGame(), frame=%d", iFrame);

} /* SG_PlayGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_GetLeafName(char *, char *)                             *
 *                                                                          *
 *  PURPOSE    : Retrieve the leaf name from a fully qualified path name.   *
 *  			 (removes the path and file extension - if any)             *
 *                                                                          *
 ****************************************************************************/
void SG_GetLeafName(char *fname, char *leaf)
{
int i, iLen;

   iLen = strlen(fname);
   for (i=iLen-1; i>=0; i--)
      {
#ifdef _WIN32
      if (fname[i] == '\\')
#else
      if (fname[i] == '/')
#endif
         break;
      }
   strcpy(leaf, &fname[i+1]);
   // remove the filename extension
   iLen = strlen(leaf);
   for (i=iLen-1; i>=0; i--)
      {
      if (leaf[i] == '.')
         {
         leaf[i] = 0;
         break;
         }
      }
} /* SG_GetLeafName() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_SavePrevPlay(GAME_BLOB)                                 *
 *                                                                          *
 *  PURPOSE    : Save the previously played game info.                      *
 *                                                                          *
 ****************************************************************************/
void SG_SavePrevPlay(GAME_BLOB *pBlob)
{
//PIL_FILE pf;
//PIL_PAGE pp1, pp2;
//PIL_VIEW pv;
char pszTemp[256], pszLeaf[256];
int i, j; //, iThumbCX;

#ifdef FUTURE
//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_SavePrevPlay()");
   iThumbCX = 160;
   // save the thumbnail image of the game
   memset(&pp1, 0, sizeof(pp1));
   pp1.pData = (unsigned char *)pBlob->pBitmap;
//   if (iCurrentSystem == 4) // TG16 uses an offset pointer
//      pp1.pData += 32;
   pp1.cBitsperpixel = 16;
   pp1.iHeight = pBlob->iHeight;
   pp1.iWidth = pBlob->iWidth;
   pp1.cCompression = PIL_COMP_NONE;
   pp1.iPitch = pBlob->iPitch;
   // squeeze it into a 80/160 pixel wide thumbnail image
   pv.iScaleX = (pBlob->iWidth*256)/iThumbCX;
   pv.iScaleY = pv.iScaleX;
   pv.pBitmap = EMUAlloc(iThumbCX*iThumbCX*2);
   pv.cFilter = PIL_VIEWFLAGS_AVERAGE; // do pixel averaging
   pv.iWidth = iThumbCX;
   pv.iPitch = iThumbCX*2;
   pv.iHeight = (pBlob->iHeight*256)/pv.iScaleX;
   pv.iOrientation = 0;
   pv.iWinX = 0;
   pv.iWinY = 0;
   PILDraw(&pp1, &pv, TRUE, NULL);
   // place the scaled down image into a new page structure
   memset(&pp2, 0, sizeof(pp2));
   pp2.cBitsperpixel = 16;
   pp2.cCompression = PIL_COMP_NONE;
   pp2.pData = pv.pBitmap;
   pp2.iWidth = pv.iWidth;
   pp2.iHeight = pv.iHeight;
   pp2.iPitch = pv.iPitch;
   // Convert it to JPEG
   PILModify(&pp2, PIL_MODIFY_COLORS, 24, 0);
   pp1.cCompression = PIL_COMP_LZW;
   PILConvert(&pp2, &pp1, 0, NULL, NULL);
   // save the thumbnail image
   SG_GetLeafName(pszGame, pszLeaf); // get the rom leaf name
   sprintf(pszTemp, "%s%s.tif", pszSAVED, pszLeaf);
   PILCreate((char *)pszTemp, &pf, 0, PIL_FILE_TIFF);
   PILWrite(&pf, &pp1, 0);
   PILClose(&pf);
   PILFree(&pp1);
   PILFree(&pp2);
   EMUFree(pv.pBitmap);
#endif // FUTURE
   // Adjust the previously played games list
   // see if the filename was already there in the top spot
   for (i=0; i<MAX_PREV_GAMES; i++)
      {
      if (strcmp(szGameFiles[i], pszGame) == 0)
         break;
      }
   if (i == 0) // no change, return
      return;
   if (i == MAX_PREV_GAMES) // not found, move everything down and insert new info
      {
      if (szGameFiles[MAX_PREV_GAMES-1][0]) // list is full, delete old image file at bottom
         {
         SG_GetLeafName(szGameFiles[MAX_PREV_GAMES-1], pszLeaf); // get the rom leaf name
         sprintf(pszTemp, "%s%s.tif", pszSAVED, pszLeaf);
         EMUDelete(pszTemp);
         }
      for (i=MAX_PREV_GAMES-2; i>=0; i--)
         {
         strcpy(szGameNames[i+1], szGameNames[i]);
         strcpy(szGameFiles[i+1], szGameFiles[i]);
         }
      }
   else // it was found, but in a different position
      {
      for (j=i; j>0; j--) // cover up old spot
         {
         strcpy(szGameNames[j], szGameNames[j-1]);
         strcpy(szGameFiles[j], szGameFiles[j-1]);
         }
      }
   // put new game in top position
   strcpy(szGameNames[0], (const char *)pBlob->szGameName);
   strcpy(szGameFiles[0], pszGame);
//   SG_WritePrevList(); // save to registry
   // since we just played this game, make it the current on the main window
   iPrevGame = 0;
//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_SavePrevPlay()");
} /* SG_SavePrevPlay() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_TerminateGame(void)                                     *
 *                                                                          *
 *  PURPOSE    : Free the resources of the current game.                    *
 *                                                                          *
 ****************************************************************************/
void SG_TerminateGame(void)
{
int i;

//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_TerminateGame()");

// DEBUG - re-enable this at some future time
//	SGSaveGame(blobs[MAX_PREV_GAMES].szGameName, &blobs[MAX_PREV_GAMES], 0); // always save the last state
    (*emulators[iGameType].pfnTerminate)(&blobs[MAX_PREV_GAMES]);
//    if (blobs[MAX_PREV_GAMES].iGameType != SYS_COINOP)
//    	SG_SavePrevPlay(&blobs[MAX_PREV_GAMES]);
#ifndef _WIN32
    usleep(16666); // give video frame time to use the last buffer
#endif
//    blobs[MAX_PREV_GAMES].pBitmap0 -= (32 + 16*iPitch);
//    blobs[MAX_PREV_GAMES].pBitmap1 -= (32 + 16*iPitch);
//    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap0);
//    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap1);
    blobs[MAX_PREV_GAMES].pBitmap0 = NULL;
    blobs[MAX_PREV_GAMES].pBitmap1 = NULL;
    blobs[MAX_PREV_GAMES].pBitmap = NULL;
    pBitmap = NULL;
    EMUFree(InQ);
    EMUFree(OutQ);
    // free the rewind buffers
    for (i=0; i<4; i++)
       {
       EMUFree(blobs[MAX_PREV_GAMES].pRewind[i]);
       }

    if (bUseSound)
       {
       EMUCloseSound();
       }
//    if (bMaster) // Tell the slave to stop
//       {
//       SGSendNetPacket(NULL, 0, SGNETWORK_END);
//       }
//    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_TerminateGame()");

} /* SG_TerminateGame() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUSaveBRAM(char *, int)                                   *
 *                                                                          *
 *  PURPOSE    : Save the game's backup RAM to disk.                        *
 *                                                                          *
 ****************************************************************************/
void EMUSaveBRAM(GAME_BLOB *pBlob, char *pData, int iLen, char *szSystem)
{
char pszTemp[256];
void * oHandle;

    sprintf(pszTemp, "%s%s.%s.bak", pszSAVED, pBlob->szGameName, szSystem);
    oHandle = EMUCreate(pszTemp);
    if (oHandle == (void *)-1)
    {
//        __android_log_print(ANDROID_LOG_VERBOSE, "EMUSaveBRAM", "Error creating file %s", pszTemp);
    }
    else
    {
    	EMUWrite(oHandle, pData, iLen);
    	EMUClose(oHandle);
    }

} /* EMUSaveBRAM() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadBRAM(char *, int)                                   *
 *                                                                          *
 *  PURPOSE    : Load the game's backup RAM from disk.                      *
 *                                                                          *
 *  RETURNS    : FALSE if success, TRUE if failure.                         *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadBRAM(GAME_BLOB *pBlob, char *pData, int iLen, char *szSystem)
{
char pszTemp[256];
void * iHandle;
int iReadLen;

    sprintf(pszTemp, "%s%s.%s.bak", pszSAVED, pBlob->szGameName, szSystem);
    iHandle = EMUOpenRO(pszTemp);
    if (iHandle == (void *)-1)
       return TRUE;
    iReadLen = EMURead(iHandle, pData, iLen);
    EMUClose(iHandle);
    if (iReadLen != iLen) // file was not what we expected (maybe the save data size changed), abort
    {
    	memset(pData, 0xff, iReadLen); // clear out the data we did actually read
    	return TRUE; // return failure
    }
    return FALSE;

} /* EMULoadBRAM() */

//
// Memory area functions used for synchronizing a master/slave game
//
void SGPrepMemAreas(GAME_BLOB *pBlob)
{
int i;

   i = 0;
   while (pBlob->mem_areas[i].iAreaLength)
      {
      pBlob->mem_areas[i].pCompareArea = EMUAlloc(pBlob->mem_areas[i].iAreaLength);
      i++;
      }
   pGameData = EMUAlloc(MAX_BLOCK_SIZE);

} /* SGPrepMemAreas() */

void SGFreeMemAreas(GAME_BLOB *pBlob)
{
int i;

// Free the temporary areas used for comparison
   i = 0;
   while (pBlob->mem_areas[i].iAreaLength)
      {
      EMUFree(pBlob->mem_areas[i].pCompareArea);
      i++;
      }
   EMUFree(pGameData); // free temp data area

} /* SGFreeMemAreas() */

// Compress the data into skip values (byte+1) up to 127
// and copy values (-byte), then transmit to slave
void SGCompressMemAreas(GAME_BLOB *pBlob)
{
int i, iArea, iAreaLen, iStart, iLen, iCount;
unsigned char *s, *d;

   iLen = 0;
   // Compress each memory area
   iArea = 0;
   while (pBlob->mem_areas[iArea].iAreaLength)
      {
      s = pBlob->mem_areas[iArea].pPrimaryArea;
      d = pBlob->mem_areas[iArea].pCompareArea;
      iAreaLen = pBlob->mem_areas[iArea].iAreaLength;
      i = 0;
      while (i < iAreaLen)
         {
         iCount = 0;
         while (i < iAreaLen && s[i] == d[i]) // count up the area without changes
            {
            i++;
            iCount++;
            }
         if (iCount >= 128) // there area unchanged areas
            {
            pGameData[iLen++] = (unsigned char)(0x80 + (iCount >> 8)); // store skip values >= 128 with 2 bytes
            pGameData[iLen++] = (unsigned char)(iCount & 0xff);
            }
         else // count <= 127 (including 0)
            {
            pGameData[iLen++] = iCount;
            }
         if (i == iAreaLen)
            break; // don't compress a zero length changed area if we're at the end of the area
         iCount = 0; // count changed bytes
         iStart = i; // start of disimilar area
         while (i < iAreaLen && s[i] != d[i]) // count up the area with changes
            {
            i++;
            iCount++;
            }
         if (iCount >= 128) // store change count
            {
            pGameData[iLen++] = (unsigned char)(0x80 + (iCount >> 8)); // store skip values >= 128 with 2 bytes
            pGameData[iLen++] = (unsigned char)(iCount & 0xff);
            }
         else // count <= 127 (including 0)
            {
            pGameData[iLen++] = iCount;
            }
         if (iCount)
            {
            memcpy(&pGameData[iLen], &s[iStart], iCount);
            memcpy(&d[iStart], &s[iStart], iCount); // update the comparison area
            iLen += iCount;
            }
         }
      iArea++;
      }
// DEBUG
//   SGSendNetPacket(pGameData, iLen, SGNETWORK_MEMDATA); // send to slave

} /* SGCompressMemAreas() */

void SGDecompressMemAreas(GAME_BLOB *pBlob, unsigned char *pOut, int iLen)
{
int i, iArea, iCount;
unsigned char *d;

   iArea = 0;
   while (pBlob->mem_areas[iArea].iAreaLength)
      {
      if (pBlob->mem_areas[iArea].iAreaLength) // is it in use?
         {
         i = 0;
         d = pBlob->mem_areas[iArea].pPrimaryArea;
         while (i < pBlob->mem_areas[iArea].iAreaLength)
            {
            iCount = *pOut++;
            if (iCount & 0x80) // count is >= 128
               {
               iCount = ((iCount & 0x7f) << 8); // high byte
               iCount += *pOut++; // low byte
               }
            i += iCount; // skip area unchanged
            if (i < pBlob->mem_areas[iArea].iAreaLength) // if we're not at the end already
               { // get the changed bytes
               iCount = *pOut++;
               if (iCount & 0x80) // count is >= 128
                  {
                  iCount = ((iCount & 0x7f) << 8); // high byte
                  iCount += *pOut++; // low byte
                  }
               memcpy(&d[i], pOut, iCount); // copy the changed bytes
               pOut += iCount;
               i += iCount;
               }
            } // while in the current area
         } // if area is in use
      iArea++;
      } // for each area
} /* SGDecompressMemAreas() */

void SG_GetTime(int *iDay, int *iHour, int *iMinute, int *iSecond)
{
time_t currenttime;
struct tm * timeinfo;
int i, iTotalDays;
static unsigned char ucMonthLen[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

   currenttime = time(NULL);
   timeinfo = localtime(&currenttime);
   // calculate the day of the year
   iTotalDays = 0;
   for (i=timeinfo->tm_mon-1; i>0; i--) // add up everything previous
      {
      iTotalDays += ucMonthLen[i-1];
      }
   *iDay = iTotalDays + timeinfo->tm_mday;
   *iHour = timeinfo->tm_hour;
   *iMinute = timeinfo->tm_min;
   *iSecond = timeinfo->tm_sec;

} /* SG_GetTime() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFreeVideoBuffer(char *)                                 *
 *                                                                          *
 *  PURPOSE    : Free the buffer used to hold video memory.                 *
 *                                                                          *
 ****************************************************************************/
void EMUFreeVideoBuffer(unsigned char *pBuffer)
{
//	EMUFree(pBuffer);
} /* EMUFreeVideoBuffer() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUCreateVideoBuffer(int,int,int,char **)                  *
 *                                                                          *
 *  PURPOSE    : Create the buffer used to hold video memory.               *
 *                                                                          *
 ****************************************************************************/
void EMUCreateVideoBuffer(int iScreenX, int iScreenY, int iScreenBPP, unsigned char **pBuffer)
{

       if (iScreenBPP == 16)
          {
          iPitch = iScreenX*2;
          }
       if (iScreenBPP == 8)
          {
          iPitch = (iScreenX + 3) & 0xfffc; /* Dword align */
          }
       *pBuffer = EMUAlloc(iPitch*iScreenY);

} /* EMUCreateVideoBuffer() */

#ifdef BLUETOOTH
#ifndef __MACH__
static void ConnectBT (GtkWidget *widget, GtkWidget* pWindow)
{
int sock;
inquiry_info *ii = NULL;
char addr[19] = {0};
char name[248] = {0};
int len, i, flags;
int dev_id, max_rsp, num_rsp;

	g_print("Entering ConnectBT\n");

	dev_id = hci_get_route(NULL);
	sock = hci_open_dev(dev_id);
	if (dev_id < 0 || sock < 0)
	{
		g_print("Error opening socket\n");
	}
	g_print("dev_id=%d, sock=%d\n", dev_id, sock);
	len = 8;
	max_rsp = 255;
	flags = IREQ_CACHE_FLUSH;
	ii = (inquiry_info *)malloc(max_rsp * sizeof(inquiry_info));
	g_print("About to call hci_inquiry\n");
	num_rsp = hci_inquiry(dev_id, len, max_rsp, NULL, &ii, flags);
	g_print("Returned from hci_inquiry, num_rsp = %d\n", num_rsp);
	if (num_rsp < 0) g_print("hci_inquiry error\n");

	for (i=0; i<num_rsp; i++)
	{
		ba2str(&(ii+i)->bdaddr, addr);
		memset(name, 0, sizeof(name));
		if (hci_read_remote_name(sock, &(ii+i)->bdaddr, sizeof(name), name, 0) < 0)
			strcpy(name, "[unknown]");
		g_print("%s %s\n", addr, name);
		if (strcmp(name, "Zeemote: SteelSeries FREE") == 0)
		{
			break;
		}
	} // for each device found

	if (num_rsp >= 1) // found something
	{
		struct sockaddr_rc btaddr = {0};
		char zeemote[32] = "28:9A:4B:00:03:99";
		int status, iTotal;
		iGameSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
		btaddr.rc_family = AF_BLUETOOTH;
		btaddr.rc_channel = (uint8_t) 1;
//		str2ba(addr, &btaddr.rc_bdaddr);
		str2ba(zeemote, &btaddr.rc_bdaddr);
		g_print("About to connect to %s\n", name);
		status = connect(iGameSocket, (struct sockaddr *)&btaddr, sizeof(btaddr));
		if (status == 0) // worked
		{
			unsigned char cBuf[32];
			g_print("Socket connected!\n");
			iTotal = 0;
			while (iTotal < 100)
			{
				i = read(iGameSocket, cBuf, 20);
				g_print("Received %d bytes\n", i);
				if (i > 0) iTotal += i;
			}
		}
	}
	free(ii);
	close( sock );

} /* ConnectBT() */
#endif // __MACH__
#endif // BLUETOOTH
static void endApp(void)
{
   bDone = TRUE;
   SG_Sleep(10000LL); // give background thread time to react
   g_application_quit((GApplication *)theapp);

} /* endApp() */

static void SaveStateClick (GtkWidget *widget, GtkWidget* pWindow)
{
	if (bRunning)
	{
	char *p, szName[256];
	GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
		p = getcwd(szName, 256);
		if (p == NULL) {}
		strcat(szName, "/");
		strcat(szName, pBlob->szGameName);
		SGSaveGame(szName, pBlob, 0);
	}
} /* SaveStateClick() */

static void LoadStateClick (GtkWidget *widget, GtkWidget* pWindow)
{
	if (bRunning)
	{
	char *p, szName[256];
	GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
		p = getcwd(szName, 256);
		if (p == NULL) {}
		strcat(szName, "/");
		strcat(szName, pBlob->szGameName);
		SGLoadGame(szName, pBlob, 0);
	}
} /* LoadStateClick() */

static void resetClick (GtkWidget *widget, GtkWidget* pWindow)
{
	// Reset the currently running game
	if (bRunning)
	{
		SG_ResetGame(NULL);
	}
} /* resetClick() */

static void exitClick (GtkWidget *widget, GtkWidget* pWindow)
{
    gtk_widget_destroy((GtkWidget *)window);
    endApp();
} /* exitClick() */

static void SG_InitJoysticks(void)
{
int iNumJoysticks = SDL_NumJoysticks();

   joy0 = joy1 = NULL;
   if (iNumJoysticks > 0)
   {
       joy0 = SDL_JoystickOpen(0);
       if (joy0 != NULL)
       {
          g_print("Joy0 opened\n");
       }
   }
   if (iNumJoysticks > 1)
   {
      joy1 = SDL_JoystickOpen(1);
      if (joy1 != NULL)
      {
         g_print("Joy1 opened\n");
      }
   }
   if (joy0 != NULL || joy1 != NULL)
   {
      SDL_JoystickEventState(SDL_ENABLE); // enable Joystick events to be received
   }

} /* SG_InitJoysticks() */

static void openClick (GtkWidget *widget, GtkWidget* pWindow)
{
GtkWidget *dialog;
GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
gint res;

dialog = gtk_file_chooser_dialog_new ("Open File",
                                      window,
                                      action,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Open",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
//    int iWinX, iWinY;
    int rc;
    GtkFileChooser *chooser;

    chooser = GTK_FILE_CHOOSER (dialog);
    filename = gtk_file_chooser_get_filename (chooser);
    strcpy(szFileName, filename);
    g_free (filename);
    gtk_widget_destroy (dialog);
    rc = SG_InitGame((unsigned char *)szFileName);
    if (rc == SG_NO_ERROR)
    {
	g_print("About to start running\n");
        SG_InitJoysticks();
//	iWinX = blobs[MAX_PREV_GAMES].iWidth*iScale;
//	iWinY = gtk_widget_get_allocated_height(GTK_WIDGET(toolbar));
//	g_print("Toolbar allocated height = %d\n", iWinY);
//	iWinY += blobs[MAX_PREV_GAMES].iHeight*iScale;

//	gtk_widget_set_size_request((GtkWidget *)window, iWinX, iWinY);
//	gtk_widget_hide(GTK_WIDGET(box));
	bRunning = TRUE;
//	LoadCheats(); // try to load cheat codes for this game
    }

  }
  else
  {
     gtk_widget_destroy(dialog);
  }

} /* openClick() */

// Select an ISO file to play
static void isoClick (GtkWidget *widget, GtkWidget* pWindow)
{
GtkWidget *dialog;
GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
gint res;

dialog = gtk_file_chooser_dialog_new ("Open ISO File",
                                      window,
                                      action,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Open",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

res = gtk_dialog_run (GTK_DIALOG (dialog));
if (res == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
	char localFile[256];
//    int iWinX, iWinY;
//    int rc;
    GtkFileChooser *chooser;

    chooser = GTK_FILE_CHOOSER (dialog);
    filename = gtk_file_chooser_get_filename (chooser);
    strcpy(localFile, filename);
    g_free (filename);
    gtk_widget_destroy (dialog);
	if (pISO != NULL)
	{
		EMUFree(pISO);
		pISO = NULL;
	}
	pISO = SG_LoadISO(localFile, &iISOSize);
  }
  else
  {
     gtk_widget_destroy(dialog);
  }

} /* isoClick() */

static gboolean idlecallback(gpointer notused, int toggle)
{
SDL_Event event;
     while (SDL_PollEvent(&event))
     {
          if (event.type == SDL_KEYDOWN)
              g_print("key = %d", event.key.keysym.sym);
	  if (event.type == SDL_WINDOWEVENT)
	  {
		if (event.window.event == SDL_WINDOWEVENT_MOVED)
		{
			int top, left;//, bottom, right;
			iWindowX = (int)event.window.data1;
			iWindowY = (int)event.window.data2;
// DEBUG - only available on SDL 2.0.5 and above
//			SDL_GetWindowBordersSize(sdlwindow, &top, &left, &bottom, &right);
			top = 30; left = 1;
			iWindowX += left;
			iWindowY += top;
//			g_print("window moved, x=%d, y=%d\n", iWindowX, iWindowY);
		}
	  }
     }

    return TRUE;
} /* idlecallback() */

static void activate (GtkApplication *app, gpointer user_data)
{
  int i;
  GtkToolItem *openTb;
//  GtkToolItem *saveTb;
  GtkTreeView *treeview;
  GtkWidget *scrollwindow;
  GtkCellRenderer *gtk_renderer;
  GtkTreeSelection *select;
  GtkToolItem *sep;
  GtkToolItem *exitTb;
  GtkToolItem *resetTb;
  GtkToolItem *isoTb;
  GtkToolItem *LStateTb;
  GtkToolItem *SStateTb;
//  GtkToolItem *prevTb;
//  GdkRGBA color;
  GError *err;

//  color.red = 0xffff;
//  color.blue = 0xffff;
//  color.green = 0xffff;
//  color.alpha = 0xffff;

  window = (GtkWindow *)gtk_application_window_new (app);
 g_signal_connect(window,"destroy",G_CALLBACK(endApp),NULL);
  gtk_window_set_title (GTK_WINDOW (window), "SmartGear");
  gtk_window_set_default_size (GTK_WINDOW (window), DEFAULT_WIN_WIDTH, DEFAULT_WIN_HEIGHT);
//  gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
  gtk_window_set_icon_from_file(window, "smartgear.png", &err);
//  gtk_widget_override_background_color(GTK_WIDGET(window), GTK_STATE_NORMAL, &color);

// Create the dipswitch labels and drop-down controls
  grid = (GtkGrid *)gtk_grid_new();
  for (i=0; i<NUM_DIPS; i++)
  {
     labelList[i] = gtk_label_new("N/A");
     gtk_widget_set_size_request(labelList[i], 150, 24);
//     gtk_label_set_xalign((GtkLabel *)labelList[i], 0.0);
     dropList[i] = gtk_combo_box_text_new();
//     g_signal_connect(G_OBJECT(dropList[i]), "changed", G_CALLBACK(DIPsChanged), NULL);
     gtk_widget_set_size_request(dropList[i], 250, 24);
     gtk_grid_attach(grid, labelList[i], 0, i, 1, 1);
     gtk_grid_attach(grid, dropList[i], 1, i, 1, 1);
  }

  box = (GtkBox *)gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  hbox0 = (GtkBox *)gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  hbox1 = (GtkBox *)gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  hbox2 = (GtkBox *)gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));

  toolbar = gtk_toolbar_new();
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
  openTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), openTb, -1);
  perfTb = (GtkToolItem *)gtk_check_button_new_with_label("performance test");
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), perfTb, -1);
  g_signal_connect(perfTb, "clicked", G_CALLBACK(perfToggled), NULL);
  head2headTb = (GtkToolItem *)gtk_check_button_new_with_label("Head-2-Head (GBC/GG)");
  gtk_toggle_button_set_active((GtkToggleButton *)head2headTb, bHead2Head);
  audioTb = (GtkToolItem *)gtk_check_button_new_with_label("enable audio");
  gtk_toggle_button_set_active((GtkToggleButton *)audioTb, bAudio);
  framebufferTb = (GtkToolItem *)gtk_check_button_new_with_label("use framebuffer");
  gtk_toggle_button_set_active((GtkToggleButton *)framebufferTb, bFramebuffer);
  stretchTb = (GtkToolItem *)gtk_check_button_new_with_label("Smooth Stretch 2X");
  gtk_toggle_button_set_active((GtkToggleButton *)stretchTb, bStretch);
  p1controlsTb = (GtkToolItem *)gtk_combo_box_text_new();
//  gtk_combo_box_set_title((GtkComboBox *)p1controlsTb, "P1 Controls");
  p2controlsTb = (GtkToolItem *)gtk_combo_box_text_new();
//  gtk_combo_box_set_title((GtkComboBox *)p2controlsTb, "P2 Controls");
  gtk_combo_box_text_append ((GtkComboBoxText *)p1controlsTb, NULL, "Keyboard");
  gtk_combo_box_text_append ((GtkComboBoxText *)p1controlsTb, NULL, "iCade");
  gtk_combo_box_text_append ((GtkComboBoxText *)p1controlsTb, NULL, "Joystick 0");
  gtk_combo_box_text_append ((GtkComboBoxText *)p1controlsTb, NULL, "Joystick 1");
  gtk_combo_box_text_append ((GtkComboBoxText *)p2controlsTb, NULL, "Keyboard");
  gtk_combo_box_text_append ((GtkComboBoxText *)p2controlsTb, NULL, "iCade");
  gtk_combo_box_text_append ((GtkComboBoxText *)p2controlsTb, NULL, "Joystick 0");
  gtk_combo_box_text_append ((GtkComboBoxText *)p2controlsTb, NULL, "Joystick 1");
  gtk_combo_box_set_active ((GtkComboBox *)p1controlsTb, iP1Control);
  gtk_combo_box_set_active ((GtkComboBox *)p2controlsTb, iP2Control);

// Create the games list
  scrollwindow = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  treeview = (GtkTreeView *)gtk_tree_view_new();
  select = gtk_tree_view_get_selection(treeview);
  gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(select), "changed", G_CALLBACK(treeviewChanged), NULL); 
  iGame = 0; // select first game in the list
  store = gtk_list_store_new(1, G_TYPE_STRING);
  gtk_tree_view_set_model(treeview, (GtkTreeModel *)store);
  gtk_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(treeview, -1, "Coin-Op Games", gtk_renderer, "text", 0, NULL);

// Fill the list with the names of the coin-op games
//  i = 0;
//  while (gameList[i].pfnInit != NULL)
//  {
//     gtk_list_store_insert_with_values(store, &iter, -1, 0, gameList[i].szName, -1);
//     i++;
//  }
// Select game 0
  //gtk_tree_view_row_activated(treeview, 
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), head2headTb, -1);
//  saveTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveTb, -1);
//  prevTb = gtk_tool_button_new(gtk_image_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), prevTb, -1);
//  pageButton = gtk_tool_button_new(NULL, NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pageButton, -1);
  isoTb = gtk_tool_button_new(NULL, "ISO");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), isoTb, -1);
  resetTb = gtk_tool_button_new(NULL, "Reset");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), resetTb, -1);
  LStateTb = gtk_tool_button_new(NULL,"Load");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), LStateTb, -1);
  SStateTb = gtk_tool_button_new(NULL,"save");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), SStateTb, -1);
  sep = gtk_separator_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), sep, -1);
//  gtk_tool_item_set_expand(GTK_TOOL_ITEM(sep), -1);
//  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), FALSE);
  exitTb = gtk_tool_button_new(gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), exitTb, -1);

  gtk_box_pack_start(box, toolbar, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox0, (GtkWidget *)head2headTb, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox0, (GtkWidget *)perfTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, (GtkWidget *)hbox0, FALSE, FALSE, 4);

  gtk_box_pack_start (hbox1, (GtkWidget *)audioTb, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox1, (GtkWidget *)framebufferTb, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox1, (GtkWidget *)stretchTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, (GtkWidget *)hbox1, FALSE, FALSE, 4);

  gtk_box_pack_start (hbox2, (GtkWidget *)p1controlsTb, FALSE, FALSE, 4);
  gtk_box_pack_start (hbox2, (GtkWidget *)p2controlsTb, FALSE, FALSE, 4);
  gtk_box_pack_start (box, (GtkWidget *)hbox2, FALSE, FALSE, 4);

  gtk_widget_set_size_request(GTK_WIDGET(scrollwindow), 100, 150);

  gtk_box_pack_start(box, GTK_WIDGET(scrollwindow), TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(scrollwindow), GTK_WIDGET(treeview));

// add dipswitch control grid
  gtk_box_pack_start(box, (GtkWidget *)grid, TRUE, TRUE, 0);

  darea = gtk_drawing_area_new();
//  gtk_widget_override_background_color(darea, GTK_STATE_NORMAL, &color);

//  gtk_box_pack_start(GTK_BOX(vbox), darea, FALSE, FALSE, 1);

  gtk_widget_add_events(darea, GDK_SCROLL_MASK |
				GDK_BUTTON1_MASK |
				 GDK_POINTER_MOTION_MASK |
				 GDK_BUTTON_PRESS_MASK |
				 GDK_BUTTON_RELEASE_MASK |
				GDK_BUTTON_MOTION_MASK );
//  g_signal_connect (darea, "button_release_event", G_CALLBACK (mouseReleaseEvent), NULL);
//  g_signal_connect (darea, "button_press_event", G_CALLBACK (mousePressEvent), NULL);
//  g_signal_connect (darea, "motion_notify_event", G_CALLBACK (mouseMoveEvent), NULL);
//  g_signal_connect (darea, "scroll_event", G_CALLBACK (mouseScrollEvent), NULL);
//  gtk_widget_set_size_request(darea, 400, 300);
//  theimage = cairo_image_surface_create_from_png("/home/pi/Documents/TIFF/Gold.png");

//  gtk_widget_set_double_buffered(darea, FALSE);
//  gtk_container_add(GTK_CONTAINER(window), darea);

  gtk_widget_set_app_paintable(darea, TRUE);
  gtk_widget_show(darea);
//  gtk_container_set_resize_mode(darea, GTK_RESIZE_IMMEDIATE);
  gtk_box_pack_end(box, darea, TRUE, TRUE, 0);

  g_signal_connect (openTb, "clicked", G_CALLBACK (openClick), NULL);
  g_signal_connect (exitTb, "clicked", G_CALLBACK (exitClick), NULL);
//  g_signal_connect (prevTb, "clicked", G_CALLBACK (prevPage), NULL);
  g_signal_connect (resetTb, "clicked", G_CALLBACK (resetClick), NULL);
  g_signal_connect (LStateTb, "clicked", G_CALLBACK (LoadStateClick), NULL);
  g_signal_connect (SStateTb, "clicked", G_CALLBACK (SaveStateClick), NULL);
  g_signal_connect (isoTb, "clicked", G_CALLBACK (isoClick), NULL);

//  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);

//  g_signal_connect(darea, "size-allocate", G_CALLBACK(dareaChanged), NULL);
//    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(windowChanged), NULL);
//  g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(onKeyPress), NULL);
//  g_signal_connect(G_OBJECT(window), "key_release_event", G_CALLBACK(onKeyRelease), NULL);
//g_idle_add (G_CALLBACK(idlecallback), NULL);
g_timeout_add(33, (GSourceFunc)idlecallback, NULL);
  gtk_widget_show_all(GTK_WIDGET(box));
  gtk_window_present(window);
} /* activate() */

int main (int argc, char **argv)
{
  pthread_t tinfo;
  char *p, szTemp[256], szTemp2[256];
    
  if (sizeof(char *) == 4)
	g_print("SmartGear - running on 32-bit system\n");
  else
	g_print("SmartGear - running on 64-bit system\n");

    p = getcwd(szTemp, 256); // find the config file
    if (p == NULL) {}
    strcat(szTemp, "/smartgear.config");
    SG_ParseConfigFile(szTemp, szTemp2); // get the default values and kb/gp configuration
    if (iP1Control == CONTROLLER_GPIO) // GTK+ can't use GPIO (yet)
	iP1Control = CONTROLLER_KEYBOARD;

// Start SDL audio+video
	SDL_zero(m_SWantedSpec);
	SDL_zero(m_SSDLspec);
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1"); // if not set, slow systems won't get button events
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK))
	{
		g_print("Could not initialize SDL - %s\n", SDL_GetError());
	}
	else
	{
		g_print("SDL initialized; num_joysticks=%d\n", SDL_NumJoysticks());
	}
	SDL_SetEventFilter(SG_SDLEventFilter, NULL);
//	SDL_AddEventWatch(SG_SDLEventFilter, NULL); // capture events from the SDL window
	pAudioBuffer = EMUAlloc(44100*4); // maximum buffer size = 1 second
	iAudioTail = iAudioHead = 0;
	iAudioTotal = 44100; // total buffer size
	iAudioAvailable = 0;
// start a second thread to do the actual emulator work/timing
	pthread_create(&tinfo, NULL, SG_RunFrames, NULL);
//  g_timeout_add(16, SG_RunFrames, NULL);
//  initSG();

  theapp = gtk_application_new ("com.tdfsoftware.smartgeargtk", G_APPLICATION_HANDLES_OPEN/*G_APPLICATION_FLAGS_NONE*/);
  g_signal_connect (theapp, "activate", G_CALLBACK (activate), NULL);
  g_application_run (G_APPLICATION (theapp), argc, argv);
//  activate(theapp, NULL);
//  gtk_main();
  g_object_unref (theapp);
  bDone = TRUE;
  return 0;
} // main()


uint64_t SG_Clock()
{
	uint64_t ns;
	struct timespec time;

//#ifdef	__MACH__
//	clock_serv_t cclock;
//	mach_timespec_t mts;
//	host_get_clock_services(mach_host_self(), CALENDAR_CLOCK, &cclock);
//	clock_get_time(cclock, &mts);
//	mach_port_deallocate(mach_task_self(), cclock);
//	time.tv_sec = mts.tv_sec;
//	time.tv_nsec = mts.tv_nsec;
//#else
	clock_gettime(CLOCK_MONOTONIC, &time);
//#endif
	ns = time.tv_nsec + (time.tv_sec * 1000000000LL);
	return ns;
} /* SG_Clock() */

BOOL SG_Sleep(uint64_t ns)
{
struct timespec ts;

	if (ns <= 100LL || ns > 999999999LL) return FALSE;
	ts.tv_sec = 0;
	ts.tv_nsec = ns;
	nanosleep(&ts, NULL);
	return TRUE;

} /* SG_Sleep() */

void SG_RefreshDisplay(void)
{
SDL_Rect srcrect;
SDL_Rect dstrect;
int err;

	if (renderer == NULL || texture == NULL)
		return;
	if (bStretch)
	{
		srcrect.x = 0;
		srcrect.y = 0;
		srcrect.w = blobs[MAX_PREV_GAMES].iWidth * 2;
		srcrect.h = blobs[MAX_PREV_GAMES].iHeight * 2;
	}
	else
	{
		srcrect.x = 16;
		srcrect.y = 16;
		srcrect.w = blobs[MAX_PREV_GAMES].iWidth;
		srcrect.h = blobs[MAX_PREV_GAMES].iHeight;
	}
	dstrect.x = 0;
	dstrect.y = 0;
	dstrect.w = iScale * srcrect.w;
	dstrect.h = iScale * srcrect.h;
	err = SDL_RenderCopy(renderer, texture, &srcrect, &dstrect);
	if (err)
	{
		g_print("SDL_RenderCopy failed with error %s\n", SDL_GetError()); 
	}
	SDL_RenderPresent(renderer);
//	if (SDL_BlitScaled(surface, &srcrect, surfaceScreen, &dstrect) < 0)
//	{
//		g_print("SDL_BlitSurface returned %s\n", SDL_GetError());
//	}
//	else
//	{
//		SDL_UpdateWindowSurface(sdlwindow);
//	}
	
} /* SG_RefreshDisplay() */

void * SG_RunFrames(void *unused)
{
int64_t llTargetTime = 0;
int64_t llFrameDelta;
int64_t llTime; //, llOldTime;
//SDL_Event event;

	llFrameDelta = 1000000000 / 60; // time slice in nanoseconds
//	llOldTime = 0;

	while (!bDone)
	{
		if (bRunning)
		{
			uint64_t ns;
			if (blobs[MAX_PREV_GAMES].pBitmap == NULL)
			{
				SG_InitSDLGraphics(&blobs[MAX_PREV_GAMES]);
			}

			if (bQuit) // user just quit a game; clean it up
			{
				SG_QuitGame();
				llTargetTime = 0;
				continue;
			}
			if (llTargetTime == 0) llTargetTime = SG_Clock() + llFrameDelta;
			if (iAudioAvailable + blobs[MAX_PREV_GAMES].iSampleCount <= iAudioTotal)
			{
			// lock the texture
			GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
			if (bFramebuffer)
			{
				pBlob->pBitmap = pAltScreen + 32 + (iPitch * 16);
				pBlob->iPitch = iPitch;
			}
			else
			{
			unsigned char *pixels;
			SDL_Rect txtRect;
			int iTexturePitch;
				if (bPerf || bStretch)
				{
					pBlob->pBitmap = pAltScreen + 32 + 16*iPitch; // for performance testing
				}
				else
				{
					txtRect.x = txtRect.y = 0;
					txtRect.w = pBlob->iWidth + 32;
					txtRect.h = pBlob->iHeight + 32;
					if (SDL_LockTexture(texture, &txtRect, (void **)&pixels, &iTexturePitch))
					{
						g_print("SDL_LockTexture failed");
					}
					iPitch = pBlob->iPitch = iTexturePitch;
//				g_print("pixels=%08x, pitch=%d", pixels, iPitch);
					pBlob->pBitmap = pixels;
				}
				pBlob->pBitmap += (32 + 16*iPitch); // slop over
			} // if !bFramebuffer
			// combine the controller bits from all sources
			if (iP1Control == CONTROLLER_ICADE)
				u32ControllerBits = u32iCadeBits;
			else if (iP1Control == CONTROLLER_KEYBOARD)
				u32ControllerBits = u32KeyBits;
			else if (iP1Control == CONTROLLER_JOY0)
				u32ControllerBits = u32Joy0Bits;
			else u32ControllerBits = u32Joy1Bits;		
			if (iP2Control == CONTROLLER_ICADE)
				u32ControllerBits |= (u32iCadeBits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_KEYBOARD)
				u32ControllerBits |= (u32KeyBits << PLAYER_SHIFT);
			else if (iP2Control == CONTROLLER_JOY0)
				u32ControllerBits |= (u32Joy0Bits << PLAYER_SHIFT);
			else u32ControllerBits |= (u32Joy1Bits << PLAYER_SHIFT);
			if ((u32ControllerBits & (RKEY_SELECT_P1 | RKEY_START_P1)) ==
				(RKEY_SELECT_P1 | RKEY_START_P1))
			{
				bQuit = TRUE; // start+select on joystick = quit
			}
			SG_PlayGame(bAudio, TRUE, u32ControllerBits);
			if (bStretch)
			{
				if (bFramebuffer)
				{
#ifndef __MACH__
					ASMBLT200HQ((unsigned char *)pBlob->pBitmap, pBlob->iPitch, pStretchScreen, pBlob->iPitch*2, pBlob->iWidth, pBlob->iHeight);
                                if (iScreenBpp == 32)
                                { // need to convert to 32-bits
                                volatile unsigned char *s;
                                uint16_t *pSrc;
                                uint32_t ul, *pDest;
                                int x, y;
                                        s = pStretchScreen;
                                        for (y=0; y<pBlob->iHeight*2; y++)
                                        {
                                                pDest = (uint32_t*)&pScreen[(iScreenPitch*(y+iWindowY))+(iWindowX*4)];
                                                pSrc = (uint16_t*)&s[(iPitch*2 * y)];
                                                for (x=0; x<pBlob->iWidth*2; x++)
                                                {
                                                        ul = (pSrc[0] & 0x1f) << 3; // blue
                                                        ul |= (ul >> 5);
                                                        ul |= (((pSrc[0] & 0x7e0) << 5) | ((pSrc[0] & 0x600) >> 1)); // green
                                                        ul |= (((pSrc[0] & 0xf800) << 8) | ((pSrc[0] & 0xe000) << 3)); // red
                                                        *pDest++ = ul;
                                                        pSrc++;
                                                }
                                        }
                                }
                                else if (iScreenBpp == 16)
				{
                                unsigned short *s, *d;
                                int y;
                                        for (y=0; y<pBlob->iHeight*2; y++)
                                        {
                                                s = (unsigned short *)&pStretchScreen[iPitch*y*2];
                                                d = (unsigned short *)&pScreen[((iWindowY+y) * iScreenPitch) + iWindowX * 2];
                                                memcpy(d, s, pBlob->iWidth*4);
                                        } // for y

				}
#endif // __MACH__
				}
				else
				{
					unsigned char *pixels;
					SDL_Rect txtRect;
					int iTexturePitch;
					txtRect.x = txtRect.y = 0;
					txtRect.w = pBlob->iWidth*2;
					txtRect.h = pBlob->iHeight*2;
					if (SDL_LockTexture(texture, &txtRect, (void **)&pixels, &iTexturePitch))
					{
						g_print("SDL_LockTexture failed");
					}
					ASMBLT200HQ(pAltScreen+64+(32*iPitch), iPitch, pixels, iTexturePitch, pBlob->iWidth, pBlob->iHeight);
				}
			}
			if (bFramebuffer && !bStretch) 
			{
#ifndef __MACH__
				if (iScreenBpp == 32)
				{ // need to convert to 32-bits
					Convert16To32((unsigned char *)pBlob->pBitmap, pBlob->iPitch, &pScreen[(iWindowY*iScreenPitch)+(iWindowX*4)], iScreenPitch, pBlob->iWidth, pBlob->iHeight);
				}
				else if (iScreenBpp == 16)
				{
				unsigned short *s, *d;
				int y;
					for (y=0; y<pBlob->iHeight; y++)
					{
						s = (unsigned short *)&pBlob->pBitmap[iPitch*y];
						d = (unsigned short *)&pScreen[((iWindowY+y) * iScreenPitch) + iWindowX * 2];
						memcpy(d, s, pBlob->iWidth*2);
					} // for y
				}
#endif // __MACH__
			}
			else
			{
				if (!bPerf)
				{
					SDL_UnlockTexture(texture);
				}
			} // if !bFramebuffer
// push this block of audio
				if (!bPerf)
				{
				   if (m_id != 0)
				   {
					SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
					if (iAudioAvailable < blobs[MAX_PREV_GAMES].iSampleCount*2)
					{ // need more audio
//						g_print("needed more audio\n");
						SG_PlayGame(bAudio, FALSE, u32ControllerBits);
						SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
					}			
				   }

//			SDL_QueueAudio(1, blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iAudioBlockSize);
//			gtk_widget_queue_draw(darea);
				if (!bFramebuffer) SG_RefreshDisplay();
				llTime = SG_Clock();
				ns = llTargetTime - llTime;
				if (ns > 4000LL)
					SG_Sleep(ns-4000LL);	// don't trust sleep for the last 4us
				else if ((int64_t)ns < 0) // we need to catch up (aka drop frames)
				{
					while ((int64_t)ns < 0)
					{
						SG_PlayGame(bAudio, FALSE, u32ControllerBits);
						if (bAudio)
						{
							SG_PushSamples((unsigned char *)blobs[MAX_PREV_GAMES].pAudioBuf, blobs[MAX_PREV_GAMES].iSampleCount);
						}
						ns += llFrameDelta;
						llTargetTime += llFrameDelta;
					}
				}
				llTargetTime += llFrameDelta;
				}
			}
			else
			{// we're getting ahead of the framerate, sleep a little
				SG_Sleep(1000LL);
			}
		}
		else // not running
		{
			llTime = SG_Clock();
			SG_Sleep(llFrameDelta * 10LL);
//			llTargetTime = llTime + llFrameDelta;
		}
	} // while !bDone
//	SG_StopAudio();
	return NULL;
} /* game_thread() */

//
// Callback for when the treeview selection changes (coin-op games list)
//
static void treeviewChanged(GtkTreeSelection *sel, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
//  gchar *game;
  GtkTreePath *path;
  int *i;

   if (gtk_tree_selection_get_selected(sel, &model, &iter))
   {
      path = gtk_tree_model_get_path(model, &iter);
      i = gtk_tree_path_get_indices(path);
      iGame = *i; // currently selected game
//      updateDIPSwitches(); // show the correct dipswitch value
//      gtk_tree_model_get(model, &iter, 0, &game, -1);
//      g_print("%d selected", *i);
//      g_free(game);
   }
} /* treeviewChanged() */

static void sdlCallback(void *userdata, uint8_t *stream, int len)
{
    // len is the number of bytes (not samples) to copy to the buffer
	if (bRunning)
	{
		SG_PopSamples(stream, len/iAudioSampleSize);
	}
	else
	{
		memset(stream, 0, len);
	}
} /* sdlCallback() */
