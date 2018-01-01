//
// SmartGear definitions
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

#ifndef __SMARTGEAR__
#define __SMARTGEAR__

typedef enum
{
SYS_UNKNOWN=0, // unknown system
SYS_NES,
SYS_GAMEGEAR, // and MasterSystem
SYS_GAMEBOY,
SYS_TG16,
SYS_GENESIS,
SYS_COINOP,
SYS_COLECO,
SYS_SNES,
SYS_MSX,
} systemtypes;

// allocation per save game state (IREM games use a lot of memory)
#define MAX_SAVE_SIZE 0x80000
#define MAX_PREV_GAMES 14

#define SG_NO_ERROR         0
#define SG_GENERAL_ERROR   -1
#define SG_OUTOFMEMORY     -2
#define SG_FILENOTFOUND    -3
#define SG_ERROR_UNSUPPORTED -4

typedef struct tagMYLONG
{
union
   {
#ifdef BIG_ENDIAN
   unsigned char spaceb1, spaceb2, spaceb3, b;
   unsigned short spacew1, w;
#else
   unsigned char b;
   unsigned short w;
#endif
   uint32_t l;
   };
} MYLONG;

typedef struct tagMYSHORT
{
union
   {
   unsigned short w;
   struct
      {
#ifdef BIG_ENDIAN
      unsigned char h;
      unsigned char l;
#else
      unsigned char l;
      unsigned char h;
#endif
      };
   };
} MYSHORT;

// Memory area for multiplayer master/slave games
typedef struct tagMEMORY_AREA
{
unsigned char *pPrimaryArea; // pointer to active area
unsigned char *pCompareArea; // pointer to prev frame info for delta
int iAreaLength;
} MEMORY_AREA;

typedef struct tagGAME_BLOB
{
int iNetworkAreas;
int iTotalAreas;
MEMORY_AREA mem_areas[20]; // allow up to 20
char szGameName[256];
unsigned char *pRewind[4]; // 4 buffers for rewind feature
int iRewindSize[4];
int iRewindFrame[4];
int iRewindIndex;
void *pMachine; // pointer to machine specific blob
void *OriginalMachine; // used for storing current machine during a game state load
volatile unsigned char *pBitmap; // current destination bitmap
volatile unsigned char *pBitmap0; // first bitmap for double buffering
volatile unsigned char *pBitmap1; // second bitmap for double buffering
volatile int iBitmapFrame0;
volatile int iBitmapFrame1;
int iPitch; // bitmap pitch
int iWidth; // game width (returned by XX_INIT function)
int iHeight; // game height (returned by XX_INIT function)
int iFrame;
int iPatch; // for performance patching
signed short *pAudioBuf;
int iAudioSampleRate;
int iAudioBlockSize;
BOOL b16BitAudio;
BOOL bHead2Head;
BOOL bLoaded; // for multithreaded access
BOOL bRewound; // game was just rewound
BOOL b4WayJoystick; // games like Pac-Man require 4-way joystick data, not 8-way
int iGameType; // type of system
int iSampleCount;
int iSoundChannels; // 1=mono,2=stereo
} GAME_BLOB;
//
// Init function allocates memory and loads the ROM
// image.  It optionally loads a saved game state
// InitProc(GAME_BLOB *pBlob, char *szROMFile, int iGameLoad);
//
// Play function executes 1/60th of a second of gameplay
// and optionally generates a RGB565 bitmap and
// 8 or 16-bit, mono or stereo audio samples
// PlayProc(GAME_BLOB *pBlob, BOOL bVideo, BOOL bAudio, ULONG ulKeys);
//
// Terminate function frees all of the resources of the game
// shared resources are freed when the refcount decrements to 0
// TerminateProc(GAME_BLOB *pBlob);
//
// PostLoad function cleans up CPU registers and memory after loading
// a saved game state
//
typedef int (*GAMEINITPROC)(GAME_BLOB *, char *, int);
typedef void (*GAMETERMINATEPROC)(GAME_BLOB *);
typedef void (*GAMEPLAYPROC)(GAME_BLOB *, BOOL, BOOL, uint32_t);
typedef void (*GAMEPOSTLOADPROC)(GAME_BLOB *);
typedef int (*GAMESTATUSPROC)(int *, int *);
typedef struct tagEMULATOR
{
    GAMEINITPROC pfnInit;
    GAMETERMINATEPROC pfnTerminate;
    GAMEPLAYPROC pfnPlay;
    GAMEPOSTLOADPROC pfnPostLoad;
    GAMESTATUSPROC pfnStatus;

} EMULATOR;

typedef int (*INITPROC)(GAME_BLOB *, char *, int);
typedef void (*PLAYPROC)(GAME_BLOB *, BOOL, BOOL, uint32_t);
typedef void (*TERMINATEPROC)(GAME_BLOB *);
typedef void (*POSTLOADPROC)(GAME_BLOB *);
typedef int (*STATUSPROC)(int *, int *);

/* Game options structure for allowing user control of dip switches and optional flags */
typedef struct taggameoptions
{
char *szName;   /* Name of the option to present to the user */
char *szChoices[16]; /* List of up to 16 choices for the user to choose */
int iChoice; /* Choice index, init with default value */
} GAMEOPTIONS;

typedef struct taggamedef
{
char *szName;
char *szROMName;
INITPROC pfnInit;
PLAYPROC pfnPlay;
TERMINATEPROC pfnTerminate;
POSTLOADPROC pfnPostLoad;
STATUSPROC pfnStatus;
int iWidth, iHeight; /* Screen width and height */
int iROMSet; /* ROM set # when the same driver can be used. e.g. PacMan/Ms. PacMan */
int iStartFrame; // frame number when POST is complete
int iFrameSkip; // number of frames that can be executed quickly to skip the self-test
int b4WayJoystick; // games like Pac-Man require 4-way joystick data, not 8-way
GAMEOPTIONS *pGameOptions; /* Pointer to array of n game options, last one has name = NULL */
} GAMEDEF;

typedef struct tagDRAWFTW
{
unsigned short *pSrc;
int iSrcPitch;
int iSrcWidth;
int iSrcHeight;
unsigned short *pDest;
int iDestPitch;
int iDestWidth;
int iDestHeight;
int iScaleX;
int iScaleY;
int iDestX;
int iDestY;
unsigned short *pLeftDest;
} DRAWFTW; // draw "FIT TO WINDOW" structure
#ifdef FUTURE
#define KEY_TOTAL 22
#define KEY_UP     0
#define KEY_DOWN   1
#define KEY_LEFT   2
#define KEY_RIGHT  3
#define KEY_A      4
#define KEY_B      5
#define KEY_C      6
#define KEY_D      7
#define KEY_E      8
#define KEY_F      9
#define KEY_START  10
#define KEY_SELECT 11 // not used on all systems
#define KEY_PAUSE  12
#define KEY_EXIT   13
#define KEY_RESET  14
#define KEY_VOL_UP 15
#define KEY_VOL_DN 16
#define KEY_SAVE   17
#define KEY_LOAD   18
#define KEY_CAPTURE 19
#define KEY_THROTTLE 20
#define KEY_REWIND 21
#endif // FUTURE

// Controller bit definitions
// Arranged as 8-bits per player to allow 4 players to fit in 32-bits
// For 4-player games, they usually only have 2 buttons per player
// so there are overlapping definitions for 1/2 player games that need
// more buttons.
// Number of bits to shift to move controls over 1 player
#define PLAYER_SHIFT 8
#define RKEY_UP_P1      0x00000001
#define RKEY_DOWN_P1    0x00000002
#define RKEY_LEFT_P1    0x00000004
#define RKEY_RIGHT_P1   0x00000008
#define RKEY_BUTTA_P1   0x00000010
#define RKEY_BUTTB_P1   0x00000020
#define RKEY_START_P1   0x00000040
#define RKEY_SELECT_P1  0x00000080

#define RKEY_UP_P2      0x00000100
#define RKEY_DOWN_P2    0x00000200
#define RKEY_LEFT_P2    0x00000400
#define RKEY_RIGHT_P2   0x00000800
#define RKEY_BUTTA_P2   0x00001000
#define RKEY_BUTTB_P2   0x00002000
#define RKEY_START_P2   0x00004000
#define RKEY_SELECT_P2  0x00008000

#define RKEY_UP_P3      0x00010000
#define RKEY_DOWN_P3    0x00020000
#define RKEY_LEFT_P3    0x00040000
#define RKEY_RIGHT_P3   0x00080000
#define RKEY_BUTTA_P3   0x00100000
#define RKEY_BUTTB_P3   0x00200000
#define RKEY_START_P3   0x00400000
#define RKEY_SELECT_P3  0x00800000

#define RKEY_UP_P4      0x01000000
#define RKEY_DOWN_P4    0x02000000
#define RKEY_LEFT_P4    0x04000000
#define RKEY_RIGHT_P4   0x08000000
#define RKEY_BUTTA_P4   0x10000000
#define RKEY_BUTTB_P4   0x20000000
#define RKEY_START_P4   0x40000000
#define RKEY_SELECT_P4  0x80000000

// odds and ends that overlap other buttons
#define RKEY_COIN1 RKEY_SELECT_P1
#define RKEY_COIN2 RKEY_SELECT_P2
#define RKEY_COIN3 RKEY_SELECT_P3
#define RKEY_COIN4 RKEY_SELECT_P4

#define RKEY_BUTTC_P1   RKEY_UP_P3
#define RKEY_BUTTD_P1   RKEY_DOWN_P3
#define RKEY_BUTTE_P1   RKEY_LEFT_P3
#define RKEY_BUTTF_P1   RKEY_RIGHT_P3
#define RKEY_UP2_P1     RKEY_BUTTA_P3
#define RKEY_DOWN2_P1   RKEY_BUTTB_P3
#define RKEY_LEFT2_P1   RKEY_START_P3
#define RKEY_RIGHT2_P1  RKEY_SELECT_P3

#define RKEY_BUTTC_P2   RKEY_UP_P4
#define RKEY_BUTTD_P2   RKEY_DOWN_P4
#define RKEY_BUTTE_P2   RKEY_LEFT_P4
#define RKEY_BUTTF_P2   RKEY_RIGHT_P4
#define RKEY_UP2_P2     RKEY_BUTTA_P4
#define RKEY_DOWN2_P2   RKEY_BUTTB_P4
#define RKEY_LEFT2_P2   RKEY_START_P4
#define RKEY_RIGHT2_P2  RKEY_SELECT_P4

#define RKEY_ODDKEY     0x80000000     // mark a special button that doesn't work properly

#define RKEY_SERVICE1 RKEY_BUTTF_P1
#define RKEY_P1_START RKEY_START_P1
#define RKEY_P2_START RKEY_START_P2
#define RKEY_BUTT1_P1 RKEY_BUTTA_P1
#define RKEY_BUTT2_P1 RKEY_BUTTB_P1
#define RKEY_BUTT3_P1 RKEY_BUTTC_P1
#define RKEY_BUTT4_P1 RKEY_BUTTD_P1
#define RKEY_BUTT5_P1 RKEY_BUTTE_P1
#define RKEY_BUTT6_P1 RKEY_BUTTF_P1
#define RKEY_BUTT1_P2 RKEY_BUTTA_P2
#define RKEY_BUTT2_P2 RKEY_BUTTB_P2
#define RKEY_BUTT3_P2 RKEY_BUTTC_P2
#define RKEY_BUTT4_P2 RKEY_BUTTD_P2
#define RKEY_BUTT5_P2 RKEY_BUTTE_P2
#define RKEY_BUTT6_P2 RKEY_BUTTF_P2

#define RKEY_BUTT1_P3 RKEY_BUTTA_P3
#define RKEY_BUTT2_P3 RKEY_BUTTB_P3
#define RKEY_P3_START RKEY_START_P3
#define RKEY_BUTT1_P4 RKEY_BUTTA_P4
#define RKEY_BUTT2_P4 RKEY_BUTTB_P4
#define RKEY_P4_START RKEY_START_P4

// control assignments
enum _controller_bits
{
    CTRL_UP = 0,
    CTRL_DOWN,
    CTRL_LEFT,
    CTRL_RIGHT,
    CTRL_EXIT,
    CTRL_START,
    CTRL_SELECT,
    CTRL_BUTT1,
    CTRL_BUTT2,
    CTRL_BUTT3,
    CTRL_BUTT4,
    CTRL_COUNT
};
enum _display_types
{
    DISPLAY_LCD=0,
    DISPLAY_FB0,
    DISPLAY_FB1
};

// Controller types
enum _controller_types
{
    CONTROLLER_KEYBOARD=0,
    CONTROLLER_ICADE,
    CONTROLLER_JOY0,
    CONTROLLER_JOY1,
    CONTROLLER_GPIO,
    CONTROLLER_COUNT
};

// mapping of inputs to controls
#define RKEY_EXIT (RKEY_SELECT_P1 | RKEY_START_P1)

void SGPrepMemAreas(GAME_BLOB *pBlob);
void SGFreeMemAreas(GAME_BLOB *pBlob);
void SGCompressMemAreas(GAME_BLOB *pBlob);
BOOL SGSaveHighScore(char *szGame, unsigned char *pMem, int iLen);
BOOL SGLoadHighScore(char *szGame, unsigned char *pMem, int iLen);
BOOL SGLoadGame(char *, GAME_BLOB *pBlob, int iGame);
BOOL SGSaveGame(char *, GAME_BLOB *pBlob, int iGame);
void SG_GetLeafName(char *fname, char *leaf);
BOOL EMULoadGBRoms(char *szROM, unsigned char **pBank);
void Convert16To32(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight)
;
void SG_VideoCopy(unsigned char *pCoinOpBitmap, int iCoinOpPitch, int iWidth, int iHeight, int iBpp, int iAngle);
int SG_ParseConfigFile(char *szConfig, char *szDir);

#endif //__SMARTGEAR__
