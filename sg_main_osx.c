//
// SmartGear for OSX
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include "sound.h"
//#include "pil_io.h"
//#include "pil.h"

int sample_rate; // DEBUG - for mame sound code ym2203
extern void LoadCheats();
char szFileName[256];
unsigned char *pISO; // pointer to PCE ISO disk image
GAME_BLOB *currentBlob;
int iISOSize;

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
#define MAX_PREV_GAMES 14
uint32_t lMyIP;
unsigned char *pGameData = NULL; // compressed game delta info

extern unsigned char pszSAVED[]; /* Save game dir */
extern unsigned char pszHome[];
char *szGameFourCC[10] = {"????","NES_","GG__","GBC_","TG16","GEN_","COIN","CLEC","SNES","MSX_"};
char pszCapture[256], pszCurrentDir[256], pszLastDir[256], pszGame[256];
int iGameType;
BOOL bDone = FALSE;
BOOL bRunning = FALSE;
BOOL bAutoLoad = FALSE;
extern BOOL bHead2Head;
BOOL bSkipFrame = FALSE;
BOOL bRegistered = TRUE;
BOOL bUserAbort;
int iDisplayWidth = 320; // DEBUG
int iDisplayHeight = 480;
//GtkWindow *window;
//static GtkWidget *darea;
//static cairo_surface_t *theimage = NULL;
int iBorderX, iBorderY;
int iScreenX;
int iScreenY;
int iVideoSize;
int iPitch;
int iFrame;
int iGameX, iGameY;
uint32_t u32KeyBits = 0;
BOOL bStereo = TRUE;
BOOL b16BitAudio = TRUE;
BOOL bSlave = FALSE;
BOOL bMaster = FALSE;
BOOL bUseSound = TRUE;
int iSampleRate = 2; // 44400 - DEBUG
//PIL_PAGE pp; // the source bitmap setup of the current game
//PIL_VIEW pv; // the bitmap of the display
extern unsigned char *pBitmap;
SERIALQ *InQ, *OutQ; // used for game link; simulate for now
extern unsigned short *pColorConvert;

GAME_BLOB blobs[MAX_PREV_GAMES+1];
char szGameNames[MAX_PREV_GAMES+1][64]; // first in the list is the most recently played
char szGameFiles[MAX_PREV_GAMES+1][256];
int iPrevGame;
char *szSystemNames[] = {"NES","GameGear/SMS","GameBoy","TG16/PC-Engine","Genesis","CoinOp",NULL};
unsigned short usPalConvert[256];
void wait_vsync(void);
extern int iAudioRate;

int NES_Init(GAME_BLOB *, char *, int);
void NES_Terminate(GAME_BLOB *);
void NES_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void NES_PostLoad(GAME_BLOB *);

int GG_Init(GAME_BLOB *, char *, int);
void GG_Terminate(GAME_BLOB *);
void GG_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GG_PostLoad(GAME_BLOB *);

int GB_Init(GAME_BLOB *, char *, int);
void GB_Terminate(GAME_BLOB *);
void GB_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GB_PostLoad(GAME_BLOB *);

int GEN_Init(GAME_BLOB *, char *, int);
void GEN_Terminate(GAME_BLOB *);
void GEN_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void GEN_PostLoad(GAME_BLOB *);

int EMUTestName(char *);
void * SG_RunFrames(void *);
void SG_WritePrevList(void);
int SG_InitGame(unsigned char *pszROM);
//void MSXPlay(HANDLE hInst, HWND hWnd, TCHAR *szGame);
//void MSXReset(void);
//BOOL MSXLoadGame(int);

//void SNESPlay(HANDLE hInst, HWND hWnd, TCHAR *szGame);
//void SNESReset(void);
//BOOL SNESLoadGame(int);

int CoinOp_Init(GAME_BLOB *, char *, int);
void CoinOp_Terminate(GAME_BLOB *);
void CoinOp_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void CoinOp_PostLoad(GAME_BLOB *);

int PCE_Init(GAME_BLOB *, char *, int);
void PCE_Terminate(GAME_BLOB *);
void PCE_Play(GAME_BLOB *, BOOL, BOOL, uint32_t);
void PCE_PostLoad(GAME_BLOB *);

// structure holding info for all of the emulators
EMULATOR emulators[7] = {{NULL, NULL, NULL, NULL}, // SYS_UNKNOWN
    {NES_Init, NES_Terminate, NES_Play, NES_PostLoad},
    {GG_Init, GG_Terminate, GG_Play, GG_PostLoad},
    {GB_Init, GB_Terminate, GB_Play, GB_PostLoad},
    {PCE_Init, PCE_Terminate, PCE_Play, PCE_PostLoad},
    {GEN_Init, GEN_Terminate, GEN_Play, GEN_PostLoad},
    {CoinOp_Init, CoinOp_Terminate, CoinOp_Play, CoinOp_PostLoad}};

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

//__android_loprintf(ANDROID_LOG_VERBOSE, "SG_SaveGameImage()", "entering");

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
//	   __android_loprintf(ANDROID_LOG_VERBOSE, "SG_SaveGameImage()", "width=%d, height=%d, name=%s", iWidth, iHeight, szOut);
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
//	   __android_loprintf(ANDROID_LOG_VERBOSE, "SG_SaveGameImage", "Error compressing bitmap = %d", rc);
   }

//   __android_loprintf(ANDROID_LOG_VERBOSE, "ScreenShot", "Leaving");
#endif // FUTURE
} /* SG_SaveGameImage() */

#ifdef BOGUS
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
struct timespec res;
int iTime;

   if (theimage != NULL)
   {
      clock_gettime(CLOCK_MONOTONIC, &res);
      iTime = 1000*res.tv_sec + res.tv_nsec/1000000;
      cairo_surface_flush(theimage);
      pv.pBitmap = cairo_image_surface_get_data(theimage);
      PILDraw(&pp, &pv, TRUE, NULL);
      clock_gettime(CLOCK_MONOTONIC, &res);
      iTime = (1000*res.tv_sec + res.tv_nsec/1000000) - iTime;
//      printf("PILDraw time = %dms\n", iTime);
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

gboolean onKeyPress(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	switch (event->keyval)
	{
		case GDK_KEY_o: // open a file
//			openClick (NULL, NULL);
		break;
		case GDK_KEY_Up:
			u32KeyBits |= RKEY_UP_P1;
		break;
		case GDK_KEY_Down:
			u32KeyBits |= RKEY_DOWN_P1;
		break;
		case GDK_KEY_Left:
			u32KeyBits |= RKEY_LEFT_P1;
		break;
		case GDK_KEY_Right:
			u32KeyBits |= RKEY_RIGHT_P1;
		break;
		case GDK_KEY_z:
			u32KeyBits |= RKEY_BUTTA_P1;
		break;
		case GDK_KEY_x:
			u32KeyBits |= RKEY_BUTTB_P1;
		break;
		case GDK_KEY_c:
			u32KeyBits |= RKEY_BUTTC_P1;
		break;
		case GDK_KEY_Return:
			u32KeyBits |= RKEY_START_P1;
		break;
		case GDK_KEY_s:
			u32KeyBits |= RKEY_SELECT_P1;
		break;
		case GDK_KEY_Escape:
			bRunning = FALSE;
		break;
	}
return TRUE;
} /* onKeyPress() */

gboolean onKeyRelease(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	switch (event->keyval)
	{
		case GDK_KEY_o: // open a file
//			openClick (NULL, NULL);
		break;
		case GDK_KEY_Up:
			u32KeyBits &= ~RKEY_UP_P1;
		break;
		case GDK_KEY_Down:
			u32KeyBits &= ~RKEY_DOWN_P1;
		break;
		case GDK_KEY_Left:
			u32KeyBits &= ~RKEY_LEFT_P1;
		break;
		case GDK_KEY_Right:
			u32KeyBits &= ~RKEY_RIGHT_P1;
		break;
		case GDK_KEY_z:
			u32KeyBits &= ~RKEY_BUTTA_P1;
		break;
		case GDK_KEY_x:
			u32KeyBits &= ~RKEY_BUTTB_P1;
		break;
		case GDK_KEY_c:
			u32KeyBits &= ~RKEY_BUTTC_P1;
		break;
		case GDK_KEY_Return:
			u32KeyBits &= ~RKEY_START_P1;
		break;
		case GDK_KEY_s:
			u32KeyBits &= ~RKEY_SELECT_P1;
		break;
	}
return TRUE;
} /* onKeyRelease() */

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
	memset(&pv, 0, sizeof(pv));
	pv.iScaleX = pv.iScaleY = 128;
	pv.iWidth = iScreenX;
	pv.iHeight = iScreenY;
	pv.iPitch = PILCalcSize(pv.iWidth, 16);
       printf ("drawing area size changed: %d x %d\n", allocation->width, allocation->height);

//       updateView(VIEW_FIT_TO_WINDOW, 0, 0, 0);
    }

}
#endif // BOGUS

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
//   __android_loprintf(ANDROID_LOG_VERBOSE, "SG_Rewind", "current frame = %d, 0=%d, 1=%d, 2=%d, 3=%d", iFrame, blobs[MAX_PREV_GAMES].iRewindFrame[0],blobs[MAX_PREV_GAMES].iRewindFrame[1], blobs[MAX_PREV_GAMES].iRewindFrame[2],blobs[MAX_PREV_GAMES].iRewindFrame[3]);
   for (i=0; i<4; i++)
      {
      j = iFrame - blobs[MAX_PREV_GAMES].iRewindFrame[i];
      if (j >= 480 && j < 720) // at least 8 seconds, but not more than 12
         {
//    	 __android_loprintf(ANDROID_LOG_VERBOSE, "SG_Rewind", "loading slot %d", i);
         SGLoadGame(NULL, &blobs[MAX_PREV_GAMES], i);
         iFrame = blobs[MAX_PREV_GAMES].iRewindFrame[i]; // set current frame to this one so that it can be rewound further
         return;
         }
      }
 // no valid rewind buffers found
// __android_loprintf(ANDROID_LOG_VERBOSE, "SG_Rewind", "no valid rewind slot found");
} /* SG_Rewind() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_PostLoad(GAME_BLOB *)                                   *
 *                                                                          *
 *  PURPOSE    : Update internal variables after loading a game state.      *
 *                                                                          *
 ****************************************************************************/
void SG_PostLoad(GAME_BLOB *pBlob)
{
    (*emulators[iGameType].pfnPostLoad)(pBlob);
} /* SG_PostLoad() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_Status()                                                *
 *                                                                          *
 *  PURPOSE    : Return status about the currently running game.            *
 *  -1 = not implemented, 0 = not playing, 1=1p game, 2=2p game             *
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

	strcpy(pszGame, (const char *)pszROM);
	printf("entering SG_InitGame(), file=%s\n", pszGame);
    currentBlob = &blobs[MAX_PREV_GAMES];
	iLen = (int)strlen(pszGame);
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
    printf("iGameType=%d\n", iGameType);
    if (iGameType) // if the game file has a .NES, .GG, GB, or .GBC extension, then try to load it
    {
        InQ = (SERIALQ *)EMUAlloc(4096);
        OutQ = (SERIALQ *)EMUAlloc(4096);
        blobs[MAX_PREV_GAMES].bHead2Head = bHead2Head;
        blobs[MAX_PREV_GAMES].b4WayJoystick = 0; // assume 8-way joysticks
        if (bUseSound)
           {
           if (iGameType == SYS_GENESIS || iGameType == SYS_COINOP || iGameType == SYS_TG16)
              {
              bStereo = TRUE; // these systems need 16-bit stereo always
              b16BitAudio = TRUE;
              }
               iAudioRate = 44100; // DEBUG
           blobs[MAX_PREV_GAMES].iSoundChannels = (bStereo) ? 2:1;
           blobs[MAX_PREV_GAMES].iAudioSampleRate = iAudioRate;
               sample_rate = iAudioRate; // DEBUG
           EMUOpenSound(44100/*blobs[MAX_PREV_GAMES].iAudioSampleRate*/, b16BitAudio ? 16:8, blobs[MAX_PREV_GAMES].iSoundChannels);
           blobs[MAX_PREV_GAMES].iAudioBlockSize = iSoundBlock;
           blobs[MAX_PREV_GAMES].iSampleCount = iSoundBlock;
           if (blobs[MAX_PREV_GAMES].iSoundChannels == 2)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
           blobs[MAX_PREV_GAMES].b16BitAudio = b16BitAudio;
           if (b16BitAudio)
              blobs[MAX_PREV_GAMES].iSampleCount >>= 1;
           printf("samplecount=%d, blocksize=%d\n", blobs[MAX_PREV_GAMES].iSampleCount, iSoundBlock);
           }
        // See if the file passed to us is a savegame file
        iGame = bAutoLoad ? 0 : -1;
        strcpy(pszTemp, pszGame);
        i = (int)strlen(pszGame);
        if (strcmp(&pszGame[i-5],"sgsav") == 0) // it's a savegame file
           {
           // get the rom filename from the savegame file
           ihandle = EMUOpenRO(pszGame);
           EMUSeek(ihandle, 4, 0); // seek to filename
           EMURead(ihandle, cTemp, 256);
           EMUClose(ihandle);
           strcpy(pszTemp, cTemp);
           i = (int)strlen(pszGame);
           iGame = (int)(pszGame[i-7] - '0');
           }

	iFrame = 0;
	printf("About to call game init\n");
        iError = (*emulators[iGameType].pfnInit)(&blobs[MAX_PREV_GAMES], (char *)pszTemp, iGame);
	printf("Returned from game init\n");
        if (iError == SG_NO_ERROR)
        {
            // process 1 frame so that TurboGfx-16 games can set the video mode
            (*emulators[iGameType].pfnPlay)(&blobs[MAX_PREV_GAMES], FALSE, FALSE, 0);
            // allocate 2 bitmaps for double buffering
			blobs[MAX_PREV_GAMES].pBitmap0 = EMUAlloc((blobs[MAX_PREV_GAMES].iWidth+32)*(blobs[MAX_PREV_GAMES].iHeight+32)*2); // allow for some slop over (PCE)
			blobs[MAX_PREV_GAMES].pBitmap1 = EMUAlloc((blobs[MAX_PREV_GAMES].iWidth+32)*(blobs[MAX_PREV_GAMES].iHeight+32)*2); // allow for some slop over (PCE)
			blobs[MAX_PREV_GAMES].iBitmapFrame0 = -1;
			blobs[MAX_PREV_GAMES].iBitmapFrame1 = -1;
			iPitch = blobs[MAX_PREV_GAMES].iPitch = (blobs[MAX_PREV_GAMES].iWidth+32)*2;
			blobs[MAX_PREV_GAMES].pBitmap0 += (32 + 16*iPitch); // slop over
			blobs[MAX_PREV_GAMES].pBitmap1 += (32 + 16*iPitch); // slop over
			blobs[MAX_PREV_GAMES].pBitmap = blobs[MAX_PREV_GAMES].pBitmap0;
			pBitmap = (unsigned char *)blobs[MAX_PREV_GAMES].pBitmap;
            iGameX = blobs[MAX_PREV_GAMES].iWidth;
            iGameY = blobs[MAX_PREV_GAMES].iHeight;
//            memset(&pp, 0, sizeof(pp)); // set up source image as a PIL_PAGE
//			pp.pData = pBitmap;
//			pp.iWidth = blobs[MAX_PREV_GAMES].iWidth;
//			pp.iHeight = blobs[MAX_PREV_GAMES].iHeight;
//			pp.iPitch = iPitch;
//			pp.cBitsperpixel = 16;
//			pp.iDataSize = pp.iPitch * pp.iHeight;
//			pp.cCompression = PIL_COMP_NONE;
		    printf("pBitmap=0x%08x, iPitch=%d\n", (int)pBitmap, iPitch);
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
void SG_PlayGame(BOOL bAudio, BOOL bVideo, uint32_t ulKeys)
{
GAME_BLOB *pBlob = &blobs[MAX_PREV_GAMES];
//int iFrameUsed;
//volatile int iTimeOut;
//    __android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_PlayGame(), iGameType=%d", iGameType);

//	wait_vsync();
	pBlob->pAudioBuf = (signed short *)pSoundBuf;

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
	(*emulators[iGameType].pfnPlay)(pBlob, bAudio, bVideo, ulKeys); // Execute 1 frame of the game

//	if (iFrameUsed == 1) // we used frame buffer 1
//    	pBlob->iBitmapFrame1 = iFrame;
//    else
 //   	pBlob->iBitmapFrame0 = iFrame;

    if ((iFrame & 0xff) == 0) // save the state every 4 seconds for rewind
       {
//       __android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "calling SGSaveGame at frame %d for rewind function", iFrame);
       pBlob->iRewindFrame[pBlob->iRewindIndex] = iFrame;
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
//    __android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_PlayGame(), frame=%d", iFrame);

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

   iLen = (int)strlen(fname);
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
   iLen = (int)strlen(leaf);
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
#ifdef FUTURE
PIL_FILE pf;
PIL_PAGE pp1, pp2;
PIL_VIEW pv;
char pszTemp[256], pszLeaf[256];
int i, j, iThumbCX;

//	__android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_SavePrevPlay()");
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
//	__android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_SavePrevPlay()");
#endif // FUTURE
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

//	__android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "entering SG_TerminateGame()");

//	SGSaveGame(blobs[MAX_PREV_GAMES].szGameName, &blobs[MAX_PREV_GAMES], 0); // always save the last state
    (*emulators[iGameType].pfnTerminate)(&blobs[MAX_PREV_GAMES]);
//    if (blobs[MAX_PREV_GAMES].iGameType != SYS_COINOP)
//    	SG_SavePrevPlay(&blobs[MAX_PREV_GAMES]);
#ifndef _WIN32
    usleep(16666); // give video frame time to use the last buffer
#endif
    blobs[MAX_PREV_GAMES].pBitmap0 -= (32 + 16*iPitch);
    blobs[MAX_PREV_GAMES].pBitmap1 -= (32 + 16*iPitch);
    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap0);
    EMUFree((void *)blobs[MAX_PREV_GAMES].pBitmap1);
    blobs[MAX_PREV_GAMES].pBitmap0 = NULL;
    blobs[MAX_PREV_GAMES].pBitmap1 = NULL;
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
//    __android_loprintf(ANDROID_LOG_VERBOSE, "smartgear", "exiting SG_TerminateGame()");

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
//        __android_loprintf(ANDROID_LOG_VERBOSE, "EMUSaveBRAM", "Error creating file %s", pszTemp);
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
	EMUFree(pBuffer);
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

#ifdef BOGUS
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
	printf("About to start running\n");
	bRunning = TRUE;
//	LoadCheats(); // try to load cheat codes for this game
    }

  }
} /* openClick() */
#endif

#ifdef BOGUS
static void activate (GtkApplication *app, gpointer user_data)
{

  GtkWidget *toolbar;
  GtkBox *box;
  GtkToolItem *openTb;
//  GtkToolItem *saveTb;
  GtkToolItem *sep;
  GtkToolItem *exitTb;
//  GtkToolItem *nextTb;
//  GtkToolItem *prevTb;
  GdkRGBA color;
//  GError *err;

  color.red = 0xffff;
  color.blue = 0xffff;
  color.green = 0xffff;
  color.alpha = 0xffff;

  window = (GtkWindow *)gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "SmartGear");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
//  gtk_window_set_icon_from_file(window, "./fiv_icon.png", &err);
  gtk_widget_override_background_color(GTK_WIDGET(window), GTK_STATE_NORMAL, &color);

  box = (GtkBox *)gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(box));

  toolbar = gtk_toolbar_new();
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
  openTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), openTb, -1);
//  saveTb = gtk_tool_button_new(gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), saveTb, -1);
//  prevTb = gtk_tool_button_new(gtk_image_new_from_icon_name("media-skip-backward", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), prevTb, -1);
//  pageButton = gtk_tool_button_new(NULL, NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pageButton, -1);
//  nextTb = gtk_tool_button_new(gtk_image_new_from_icon_name("media-skip-forward", GTK_ICON_SIZE_MENU), NULL);
//  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), nextTb, -1);
  sep = gtk_separator_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), sep, -1);
//  gtk_tool_item_set_expand(GTK_TOOL_ITEM(sep), -1);
//  gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(separator), FALSE);
  exitTb = gtk_tool_button_new(gtk_image_new_from_icon_name("application-exit", GTK_ICON_SIZE_MENU), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), exitTb, -1);

  gtk_box_pack_start(box, toolbar, FALSE, FALSE, 4);
  darea = gtk_drawing_area_new();
  gtk_widget_override_background_color(darea, GTK_STATE_NORMAL, &color);

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
//  g_signal_connect (exitTb, "clicked", G_CALLBACK (fivQuit), NULL);
//  g_signal_connect (prevTb, "clicked", G_CALLBACK (prevPage), NULL);
//  g_signal_connect (nextTb, "clicked", G_CALLBACK (nextPage), NULL);

  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);

  g_signal_connect(darea, "size-allocate", G_CALLBACK(dareaChanged), NULL);
//    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(windowChanged), NULL);
  g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(onKeyPress), NULL);
  g_signal_connect(G_OBJECT(window), "key_release_event", G_CALLBACK(onKeyRelease), NULL);
  gtk_widget_show_all(GTK_WIDGET(box));
  gtk_window_present(window);
}
#endif

#ifdef BOGUS
int main (int argc, char **argv)
{
  GtkApplication *app;
  pthread_t tinfo;

  if (sizeof(char *) == 4)
	printf("SmartGear - running on 32-bit system\n");
  else
	printf("SmartGear - running on 64-bit system\n");

// start a second thread to do the actual emulator work/timing
	pthread_create(&tinfo, NULL, SG_RunFrames, NULL);
//  g_timeout_add(16, SG_RunFrames, NULL);
//  initSG();

  app = gtk_application_new ("com.tdfsoftware.smartgeargtk", G_APPLICATION_HANDLES_OPEN/*G_APPLICATION_FLAGS_NONE*/);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  bDone = TRUE;
  return 0;
} // main()
#endif

uint64_t SG_Clock()
{
	uint64_t ns;
	struct timespec time;

#ifdef	__MACH__
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	time.tv_sec = mts.tv_sec;
	time.tv_nsec = mts.tv_nsec;
#else
	clock_gettime(CLOCK_MONOTONIC, &time);
#endif
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

void * SG_RunFrames(void *unused)
{
uint64_t ullTargetTime = 0;
uint64_t ullFrameDelta;

	ullFrameDelta = 1000000000 / 60; // time slice in nanoseconds

	while (!bDone)
	{
		if (bRunning)
		{
			uint64_t ns;
			SG_PlayGame(FALSE, TRUE, u32KeyBits);
//			gtk_widget_queue_draw(darea);
			ns = ullTargetTime - SG_Clock();
			SG_Sleep(ns);
			ullTargetTime += ullFrameDelta;
		}
		else
		{
			SG_Sleep(ullTargetTime * 10LL);
			ullTargetTime = SG_Clock() + ullFrameDelta;
		}
	}
	return NULL;
} /* game_thread() */
