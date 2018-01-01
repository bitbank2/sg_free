/* Structures and defines for Larry's Arcade Emulator */
/* Written by Larry Bank */
/* Copyright 1998-2017 BitBank Software, Inc. */
/* Project started 1/7/98 */
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

//#define DEMO      // demo version only allows 1 minute of play
//#define TIMESTAMP

/* Pending interrupt bits */
#define INT_NMI  1
#define INT_FIRQ 2
#define INT_IRQ  4
#define INT_IRQ2 2  // used in HUC6280
#define INT_IRQ3 8 // used in HUC6280 (timer)
#define INT_QUEUED_IRQ 0x80

/* Pending interrupt bits */
#define GB_INT_VBLANK  1
#define GB_INT_LCDSTAT 2
#define GB_INT_TIMER   4
#define GB_INT_SERIAL  8
#define GB_INT_BUTTONS 16
#define INT_OCI  8  /* Output compare interrupt for M6800 family */
#define INT_TOF  16 /* Timer overflow interrupt for M6800 family */

#define SPRITEFLAGS_NORMAL 0
#define SPRITEFLAGS_BLANK 1
#define SPRITEFLAGS_OPAQUE 2

#define WAVE_ON      0
#define WAVE_OFF     1
#define WAVE_RESET   2
#define WAVE_CHANNELS 3
#ifdef BOGUS
#define KEY_TOTAL 13
#define KEY_UP     0
#define KEY_DOWN   1
#define KEY_LEFT   2
#define KEY_RIGHT  3
#define KEY_A      4
#define KEY_B      5
#define KEY_START  6
#define KEY_SELECT 7 // not used on all systems
#define KEY_PAUSE  8
#define KEY_EXIT   9
#define KEY_RESET  10
#define KEY_VOL_UP 11
#define KEY_VOL_DN 12

/* Keyboard bit definitions for main program to process */
#define RKEY_UP_P1      0x0001
#define RKEY_DOWN_P1    0x0002
#define RKEY_LEFT_P1    0x0004
#define RKEY_RIGHT_P1   0x0008
#define RKEY_BUTTA_P1   0x0010
#define RKEY_BUTTB_P1   0x0020
#define RKEY_BUTTC_P1   0x0040
#define RKEY_BUTTD_P1   0x0080
#define RKEY_BUTTE_P1   0x0100
#define RKEY_BUTTF_P1   0x0200
#define RKEY_START_P1   0x0400
#define RKEY_SELECT_P1  0x0800

#define RKEY_UP_P2      0x00010000
#define RKEY_DOWN_P2    0x00020000
#define RKEY_LEFT_P2    0x00040000
#define RKEY_RIGHT_P2   0x00080000
#define RKEY_BUTTA_P2   0x00100000
#define RKEY_BUTTB_P2   0x00200000
#define RKEY_BUTTC_P2   0x00400000
#define RKEY_BUTTD_P2   0x00800000
#define RKEY_BUTTE_P2   0x01000000
#define RKEY_BUTTF_P2   0x02000000
#define RKEY_START_P2   0x04000000
#define RKEY_SELECT_P2  0x08000000
#endif // BOGUS
/* Queue wave changes */
typedef struct tagWAVEQUEUE
{
unsigned char ucScanLine;
unsigned char ucIndex;
unsigned char ucByte;
} WAVEQUEUE;

/* Machine info */
typedef struct tagMACHINES
{
long lVidStart; /* Start address to count down to find video memory */
long lVidIncrement; /* increment to count down to find video memory */
int iPitch; /* Distance between scanlines in bytes */
int iOrientation; /* Screen orientation in degrees clockwise from normal */
int iCorner; /* Which is the corner of the display to search for video */
long lVidOffset; /* Offset to center game on display */
BOOL bNoKeyup; /* Some machines like the Aero 2100 don't give keyup msgs for app buttons */
unsigned char cKeyMap[10]; /* Mappings of the program buttons - UP,DOWN,LEFT,RIGHT,COIN,P1_START,P2_START,FIRE,PAUSE,RECORD */
} MACHINES;

/* Memory map emulation offsets */
#define MEM_ROMRAM 0x0000 /* Simplified memory map */
#define MEM_FLAGS 0x10000 /* Offset to flags in memory map */
#define MEM_DECRYPT 0x20000 /* Offset to decrypted opcodes */
#define MEM_DECRYPT2 0x200000 /* Offset to decrypted opcodes */
#define MEM_FLAGS2 0x100000 /* Offset to flags in memory map */

#define Set68KIRQ(flags, level) flags |= (1<<level)

/* Definitions for external memory read and write routines */
typedef unsigned short (/*_cdecl*/ *MEMRPROC3)(uint32_t);
typedef void (/*_cdecl*/ *MEMWPROC3)(uint32_t, unsigned short);
typedef unsigned char (/*_cdecl*/ *MEMRPROC2)(uint32_t);
typedef void (/*_cdecl*/ *MEMWPROC2)(uint32_t, unsigned char);
typedef unsigned char (*MEMRPROC)(int);
typedef void (*MEMWPROC)(int, unsigned char);
typedef void (_cdecl *GAMEPROC)(HANDLE, HWND, char *, int);

/* SNES DMA structure */
typedef struct tagSNESDMA
{
BOOL bTransferDirection;
BOOL bHDMAIndirectAddressing;
BOOL bAddressFixed;
BOOL bAddressDecrement;
BOOL bRepeat;
unsigned char ucTransferMode;
unsigned int iSrc; // source address
unsigned char cDest; // destination register low byte (21xx)
unsigned char ucLineCount;
unsigned int iLen; // transfer length (0=64k)
unsigned int iIndirectAddress;
unsigned int iAltAddress;
} SNESDMA;

#define TILE_BLACK    1  // tile image is completely black; don't draw
#define TILE_CHANGED  2  // tile image changed, reconvert it
/* SNES video plane info */
typedef struct tagSNESPLANE
{
signed short sXScroll;
signed short sYScroll;
int iWidth; // in tiles
int iHeight; // in tiles
int iPitch; // horizontal pitch in bytes
int iBGOffset; // offset to tile data
int iBGCharOffset; // offset to character bitmap data
unsigned char *pPlane; // pointer to current image data for this plane
unsigned short *pusOldTiles; // buffer of tile data for comparison
unsigned char ucBitDepth; // tile bit depth of this plane
} SNESPLANE;

/* SNES VRAM and OAM access struct */
typedef struct tagSNESVRAM
{
	BOOL bHigh;
   BOOL bFirstVRAMRead;
	int iIncrement;
	int iAddress;
	unsigned short usMask;
	unsigned short usFullGraphicCount;
	unsigned short usShift;
   int iOAMAddress;
   int iOAMSavedAddress;
   unsigned char ucOAMWriteFlip;
   unsigned char ucOAMReadFlip;
   unsigned char ucOAMPriorityRotation;
   unsigned char ucFirstSprite;
} SNESVRAM;

/* Game display area definition structure */
typedef struct tagSCREENAREA
{
int iScrollX; /* Offset in pixels from left side of source bitmap */
int iScrollY; /* Offset in pixels from top side of source bitmap */
uint32_t lDirty; /* 32-bit dirty rectangle flags for each area */
int iWidth; /* Pixel width of source bitmap */
int iHeight; /* Pixel height of source bitmap */
unsigned char *pArea; /* Pointer to source bitmaps for each area */
RECT rc; /* Rectangles which define the destination area of the display */
} SCREENAREA;

typedef struct t_stars
{
unsigned char color, oldcolor, speed;
int x, y;
} STARS;

/* Sprite/character display structure */
typedef struct tagSPRITECHAR
{
unsigned char *pSprites;
unsigned char *pChars;
int iSpriteCount;
int iCharCount;
} SPRITECHAR;

/* Graphics decode structure */
typedef struct tagGFXDECODE
{
char cBitCount; /* Number of bits per tile */
int iDelta;   /* Bytes per original tile */
char cWidth, cHeight; /* Width and Height of the tile (8x8, 16x16, 32x32) */
int iPlanes[8]; /* Offset to each bit plane 0-7 */
int iBitsX[32], iBitsY[32]; /* Array of x,y coordinate bit offsets */
} GFXDECODE;

/* Sound samples structure */
typedef struct tagSNDSAMPLE
{
signed char *pSound; /* Pointer to 44.1Khz sample data */
int iLen; /* Length of the sample in bytes */
int iPos; /* Current position of sound cursor */
BOOL bLoop; /* Flag indicating whether sound should loop */
BOOL bActive; /* Flag indicating sound needs to be played */
} SNDSAMPLE;
/* Keyboard definition */
typedef struct tagEMUKEYS
{
char *szKeyName; /* ASCII name user sees */
unsigned char ucScancode; /* Put a default value here */
unsigned char ucDefault; /* keyboard default value */
unsigned char ucHotRod; /* hotrod default value */
uint32_t ulKeyMask;  /* defines a bit to return indicating key is down */
} EMUKEYS;

/* ROM Loader structure */
typedef struct tagLOADROM
{
    char *szROMName;
    int  iROMStart;
    int  iROMLen;
    int  iCheckSum;
    MEMRPROC pfn_read;
    MEMWPROC pfn_write;

} LOADROM;

/* Structure to define special memory mapped device handler routines */
typedef struct tagMEMHANDLERS
{
    unsigned short usStart;
    unsigned short usLen;
    MEMRPROC pfn_read;
    MEMWPROC pfn_write;

} MEMHANDLERS;

// Used in 16-bit machines (8086/V30)
typedef struct tagMEMHANDLERS2
{
    uint32_t ulStart;
    uint32_t ulLen;
    MEMRPROC2 pfn_read;
    MEMWPROC2 pfn_write;

} MEMHANDLERS2;

/* Structure to pass to CPU emulator with memory handler routines */
typedef struct tagEMUHANDLERS
{
   MEMRPROC pfn_read;
   MEMWPROC pfn_write;

} EMUHANDLERS;

/* Structure to pass to CPU emulator with memory handler routines */
typedef struct tagEMUHANDLERS2
{
   MEMRPROC2 pfn_read;
   MEMWPROC2 pfn_write;

} EMUHANDLERS2;

// Used in 68k machines
typedef struct tagMEMHANDLERS3
{
    uint32_t ulStart;
    uint32_t ulLen;
    MEMRPROC2 pfn_read8;
    MEMWPROC2 pfn_write8;
    MEMRPROC3 pfn_read16;
    MEMWPROC3 pfn_write16;

} MEMHANDLERS3;

/* Structure to pass to CPU emulator with memory handler routines */
typedef struct tagEMUHANDLERS3
{
   MEMRPROC2 pfn_read8;
   MEMWPROC2 pfn_write8;
   MEMRPROC3 pfn_read16;
   MEMWPROC3 pfn_write16;

} EMUHANDLERS3;

#ifndef MEMORY_MACROS
#define MEMORY_MACROS
#define INTELSHORT(p) *p + *(p+1) * 0x100
#define INTELLONG(p) *p + *(p+1) * 0x100 + *(p+2) * 0x10000 + *(p+3) * 0x1000000
#define MOTOSHORT(p) ((*(p))<<8) + *(p+1)
#define MOTOLONG(p) ((*p)<<24) + ((*(p+1))<<16) + ((*(p+2))<<8) + *(p+3)
#define MOTO24(p) ((*(p))<<16) + ((*(p+1))<<8) + *(p+2)
#define WRITEMOTO32(p, o, l) p[o] = (unsigned char)(l >> 24); p[o+1] = (unsigned char)(l >> 16); p[o+2] = (unsigned char)(l >> 8); p[o+3] = (unsigned char)l;
#define WRITEMOTO16(p, o, l) p[o] = (unsigned char)(l >> 8); p[o+1] = (unsigned char)l;
#define WRITEPATTERN32(p, o, l) p[o] |= (unsigned char)(l >> 24); p[o+1] |= (unsigned char)(l >> 16); p[o+2] |= (unsigned char)(l >> 8); p[o+3] |= (unsigned char)l;
#endif


/* 6502 registers */
typedef struct tagREGS6502
{
    unsigned long  ulOffsets[32]; // for memory offsets and ARM temporary variables
    unsigned short usRegPC;
    unsigned char  ucRegX;
    unsigned char  ucRegY;
    unsigned char  ucRegS;
    unsigned char  ucRegA;
    unsigned char  ucRegP;
} REGS6502;

/* 68000 registers */
typedef struct tagREGS68K
{
MYLONG ulRegData[8];
MYLONG ulRegAddr[8];
uint32_t ulRegUSP; /* User stack pointer */
uint32_t ulRegSSP; /* Supervisor stack pointer */
uint32_t ulRegPC; /* Program counter */
uint32_t ulRegSR; /* Status Reg/condition codes */
uint32_t ulRegVBR; /* Vector base register */
unsigned char bStopped;
unsigned char ucCPUType;
uint32_t ulTemp[16];

} REGS68K;

typedef struct tagREGSH6280
{
    uint32_t MMR[8];
    unsigned char  ucFastFlags[8];
    unsigned short usRegPC;
    unsigned char  ucRegX;
    unsigned char  ucRegY;
    unsigned char  ucRegS;
    unsigned char  ucRegA;
    unsigned char  ucRegP;
    unsigned char  ucCPUSpeed;
    unsigned char  ucIRQMask;
    unsigned char  ucIRQState;
    unsigned char  ucTimerStatus;
    unsigned char  ucTimerAck;
    unsigned int iTimerPreload;
    signed int iTimerValue;
    uint32_t ulTemp[16];    // space for ASM temp vars
} REGSH6280;

/* Z80 registers */
typedef struct tagREGSZ80
{
  unsigned long  ulOffsets[32]; // use for bank switching and ARM temp vars
  union
    {
    unsigned short usRegAF;
    struct
       {
       unsigned char ucRegF;       /* Main register set */
       unsigned char ucRegA;
       };
    };
  unsigned short sdummy1;
  union
    {
    unsigned short usRegBC;
    struct
       {
       unsigned char ucRegC;
       unsigned char ucRegB;
       };
    };
  unsigned short sdummy2;
  union
    {
    unsigned short usRegDE;
    struct
       {
       unsigned char ucRegE;
       unsigned char ucRegD;
       };
    };
  unsigned short sdummy3;
  union
    {
    unsigned short usRegHL;
    struct
       {
       unsigned char ucRegL;
       unsigned char ucRegH;
       };
    };
  unsigned short sdummy4;
  union
    {
    unsigned short usRegIX;
    struct
       {
       unsigned char ucRegIXL;
       unsigned char ucRegIXH;
       };
    };
  unsigned short sdummy9;
  union
    {
    unsigned short usRegIY;
    struct
       {
       unsigned char ucRegIYL;
       unsigned char ucRegIYH;
       };
    };
unsigned short sdummy10;
unsigned short usRegSP;
unsigned short sdummy11;
unsigned short usRegPC;
unsigned short sdummy12;
unsigned int iPortMask;
unsigned char ucRegI;
unsigned char ucRegR;
unsigned char ucRegR2;
unsigned char ucRegNMI; // flag indicating in NMI processing
unsigned char ucRegIM;
unsigned char ucRegIFF1;
unsigned char ucRegIFF2;
unsigned char ucIRQVal;
unsigned char ucRegHALT;
/* Stick these at the end so that a context switch does not have to copy them every time */
  union
    {
    unsigned short usRegAF1;
    struct
       {
       unsigned char ucRegF1;      /* Auxillary register set */
       unsigned char ucRegA1;
       };
    };
  unsigned short sdummy5;
  union
    {
    unsigned short usRegBC1;
    struct
       {
       unsigned char ucRegC1;
       unsigned char ucRegB1;
       };
    };
  unsigned short sdummy6;
  union
    {
    unsigned short usRegDE1;
    struct
       {
       unsigned char ucRegE1;
       unsigned char ucRegD1;
       };
    };
  unsigned short sdummy7;
  union
    {
    unsigned short usRegHL1;
    struct
       {
       unsigned char ucRegL1;
       unsigned char ucRegH1;
       };
    };
  unsigned short sdummy8;
} REGSZ80;

/* GBCPU registers */
typedef struct tagREGSGB
{
  union
    {
    uint32_t usRegAF;
    struct
       {
       unsigned char ucRegF;       /* Main register set */
       unsigned char ucRegA;
       unsigned char unused1, unused2;
       };
    };
  union
    {
    uint32_t usRegBC;
    struct
       {
       unsigned char ucRegC;
       unsigned char ucRegB;
       unsigned char unused3, unused4;
       };
    };
  union
    {
    uint32_t usRegDE;
    struct
       {
       unsigned char ucRegE;
       unsigned char ucRegD;
       unsigned char unused5, unused6;
       };
    };
  union
    {
    uint32_t usRegHL;
    struct
       {
       unsigned char ucRegL;
       unsigned char ucRegH;
       unsigned char unused7, unused8;
       };
    };
uint32_t usRegSP;
uint32_t usRegPC;
unsigned char ucRegI;
unsigned char ucRegHalt;
} REGSGB;

/* 6809 registers */
typedef struct tagREGS6809
{
    unsigned long  ulOffsets[32]; // use for bank switching and ARM temp vars
    unsigned char *mem_map;
    signed int iClocks;
    EMUHANDLERS *pEMUH;
    uint32_t ulFastFlags;
    unsigned short usRegX;
    unsigned short usRegY;
    unsigned short usRegU;
    unsigned short usRegS;
    unsigned short usRegPC;
    unsigned short usRegDP;
    union
       {
       unsigned short usRegD;
       struct
          {
//#ifdef BIG_ENDIAN
//          unsigned char ucRegA;
//          unsigned char ucRegB;
//#else
          unsigned char ucRegB; /* On a big-endian machine, reverse A & B */
          unsigned char ucRegA;
//#endif
          };
       };
    unsigned char ucRegCC;
    unsigned char ucIRQ;
} REGS6809;

#define INT_EXTERNAL 1
#define INT_TIMER    2
#define  I8039_p0	0x100   /* Not used */
#define  I8039_p1	0x101
#define  I8039_p2	0x102
#define  I8039_p4	0x104
#define  I8039_p5	0x105
#define  I8039_p6	0x106
#define  I8039_p7	0x107
#define  I8039_t0	0x110
#define  I8039_t1	0x111
#define  I8039_bus	0x120
/* 8039 registers */
typedef struct tagREGS8039
{
    unsigned short usRegPC;
    unsigned char  ucRegSP;
    unsigned char  ucRegPSW;
    unsigned char  ucRegA;
    unsigned char  ucRegF1;
    unsigned char  ucRegBank;
    unsigned char  ucTimerOn;
    unsigned char  ucCountOn;
    unsigned char  ucMasterClock;
    unsigned char  ucTimer;
    unsigned char  ucT0;
    unsigned char  ucT1;
    unsigned char  ucXIRQ;
    unsigned char  ucTIRQ;
    unsigned char  ucINT; // interrupt in process
    unsigned char  t_flag; /* Timer interrupt occurred flag */
    unsigned char  ucP1; // port 1 mask
    unsigned char  ucP2; // port 2 mask
    unsigned short usA11; // memory bank selector (address line 11)
    unsigned short usA11ff; // memory bank selector previous value
} REGS8039;

/* 6800/6803 registers */
typedef struct tagREGS6800
{
    unsigned long  ulOffsets[32]; // use for bank switching and ARM temp vars
    unsigned char *mem_map;
    signed int iClocks;
    EMUHANDLERS *pEMUH;
    unsigned short usRegX;
    unsigned short usRegS;
    unsigned short usRegPC;
    union
       {
       unsigned short usRegD;
       struct
          {
//#ifdef BIG_ENDIAN
//          unsigned char ucRegA;
//          unsigned char ucRegB;
//#else
          unsigned char ucRegB; /* On a big-endian machine, reverse A & B */
          unsigned char ucRegA;
//#endif
          };
       };
    unsigned char  ucRegCC;
    unsigned char  ucIRQ;
} REGS6800;

// Register array for quicker access
#define REG_AX 0
#define REG_CX 1
#define REG_DX 2
#define REG_BX 3
#define REG_SP 4
#define REG_BP 5
#define REG_SI 6
#define REG_DI 7
#define REG_F  8
#define REG_IP 9

#define REG_ES 0
#define REG_CS 1
#define REG_SS 2
#define REG_DS 3

/* 8086 registers */
typedef struct tagREGS8086
{
union
   {
   unsigned short w;
   struct
      {
      unsigned char l;
      unsigned char h;
      } b;
   } usData[10];

//  MYSHORT usData[10]; // AX, BX, CX, DX, F, SP, BP, SI, DI, IP
  uint32_t ulSegments[4]; // CS, DS, SS, ES
  unsigned char ucRegHALT;
  unsigned char ucRegNMI;
  unsigned char ucIRQVal;
  unsigned char ucREP; // repeat flag for string instructions
  uint32_t ulTemp[16];   // for ARM temp vars
} REGS8086;

/* GB-Z80 Emulator */
void APIENTRY RESETGB(REGSGB *);
void APIENTRY EXECGB(REGSGB *, int);
//#ifdef __arm__
//void APIENTRY ARESETGB(REGSGB *);
//void APIENTRY AEXECGB(REGSGB *, int);
//#else
   #define ARESETGB RESETGB
   #define AEXECGB EXECGB
//#endif

int EMUFindChangedRegion(unsigned char *pSrc, unsigned char *pDst, int iWidth,
 int iHeight, int iPitch, int iTileWidth, int iTileHeight, uint32_t *pRegions, int *iScrollX, int *iScrollY);
void EMUScreenUpdate(int, int);
void EMUSaveState(int, unsigned char *, char *, int, char *, int);
BOOL EMULoadState(int, unsigned char *, char *, int, char *, int);
BOOL EMULoadGBRoms(char *szROM, unsigned char **pBank);
void EMUGetJoyInfo(uint32_t *);
unsigned char EMUReadNULL(int);
void EMUWriteNULL(int, unsigned char);
void ASMBLT200HQ(unsigned char *pSrc, int iSrcPitch, unsigned char *pDest, int iDestPitch, int iWidth, int iHeight);
void EMUCreateVideoBuffer(int, int, int, unsigned char **);
void EMUFreeVideoBuffer(unsigned char *);
void EMUUpdatePalette(int, PALETTEENTRY *);
void EMUDrawChar(int, unsigned char *, int, int, int, unsigned long *, unsigned char *);
void EMUDrawCharXY(int sx, int sy, int iPitch, int iChar, int iColor, unsigned char *pBitmap, unsigned char *pCharData);
void EMUDrawCharTransparent4(int iAddr, unsigned char *pColorPROM, int iPitch, int iChar, int iColor, unsigned char *pCharData, int iScroll);
void EMUDrawSprite16(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned short *, int);
void EMUDrawSprite16D(int, int, int, int, BOOL, BOOL, unsigned char *, int);
void EMUDrawTile16D(int, int, int, int, BOOL, BOOL, unsigned char *, int);
void EMUDrawSprite16S(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned short *, uint32_t *, int);
void EMUDrawSprite16SD(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, uint32_t *pScroll, int iScroll);
void EMUDrawSprite(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned char *);
void EMUDrawSprite2(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned char *);
void EMUDrawSprite3(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned char *, int);
void EMUDrawSpriteTransparent(int, int, int, int, BOOL, BOOL, unsigned char *, unsigned char *);
void EMUDrawGraphic(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int xCount, int yCount);
void EMUDrawGraphicTransparent(int sx, int sy, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int cx, int cy);
void EMUDrawGraphicI(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned char *pColorPROM, unsigned char *pTilemap, int iTilePitch, int xCount, int yCount);
void EMUDrawGraphicTransparentI(int sx, int sy, int iSprite, int iColor, unsigned char *pColorTable, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned char *pScreen, int iPitch2, int cx, int cy);
void EMUDrawCharBlack(int iAddr, int iPitch2, uint32_t ulBlack);
void EMUDrawCharTransparent(int iAddr, int iPitch, int iChar, int iColor, unsigned char *pBitmap, unsigned char *pCharData);
void EMUDrawScaledSprite16(int sx, int sy, int iScaleX, int iScaleY, int iSprite, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pSpriteData, unsigned short *pColorPROM);
void EMUDrawTile(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned char *pColorPROM, unsigned char *pTilemap, int iTilePitch);
void EMUDrawTile2(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned char *pTilemap, int iTilePitch);
void EMUDrawBigTile(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned char *pColorPROM, unsigned char *pTilemap, int iTilePitch);
void EMUTimerDelay(void);
void EMUEndTimer(void);
void EMUStartTimer(int);
void EMUFreeSamples(SNDSAMPLE *, int);
BOOL EMULoadSamples(SNDSAMPLE *, char **, int, char *);
void EMUPlaySamples(SNDSAMPLE *pSamples, signed short *pSoundData, int iBlockSize, int iVoiceShift, BOOL bMerge);
//void EMUCreatePalette(RGBQUAD *, int);
unsigned char * EMULoadSampleAsWave(char *, char *);
#ifdef _WIN32_WCE
void EMUCenterDialog(HWND);
//void EMUResize(int, int);
BOOL WINAPI SpriteCharDlgProc(HWND, unsigned, UINT, LONG);
BOOL WINAPI LoadGameDlgProc(HWND, unsigned, UINT, LONG);
BOOL WINAPI SaveGameDlgProc(HWND, unsigned, UINT, LONG);
#endif

unsigned char * EMUCreateSpriteReplicas(unsigned char *, int);
void EMUSetupHandlers(unsigned char *mem_map, EMUHANDLERS *emuh, MEMHANDLERS *mhHardware);
void EMUSetupHandlers3(unsigned char *pFlags, EMUHANDLERS2 *emuh, MEMHANDLERS2 *mhHardware);
void EMUSetLEDs(int);
void EMUDecodeGFX(GFXDECODE *, int, char *, char *, BOOL);
void InitHisto(void);
void TerminateHisto(void);
//void EMUSetPaletteEntries(HPALETTE hpalEMU, int iStart, int iCount, PALETTEENTRY *pe);
void mymemcpy(void *pDest, void *pSrc, int iLen);
void mymemset(void *pDest, unsigned char ucPattern, int iLen);
//BOOL GBLoadGame(int);
//BOOL GBSaveGame(int);
void EMUSaveBRAM(GAME_BLOB *pBlob, char *pData, int iLen,char *);
BOOL EMULoadBRAM(GAME_BLOB *pBlob, char *pData, int iLen,char *);

// Different SMS/GG/SG1000 machine modes
enum {MODE_SMS=0, MODE_GG, MODE_SG1000};

#define TILE_DIRTY 1
#define SPRITE_DIRTY 2
#define SPRITE_OPAQUE 4
#define TILE_REDRAW 8
#define SPRITE_TEST 0x02020202

/* Structure for holding queued events such as DAC and fast sound chip updates */
typedef struct tagPCE_EVENT_QUEUE
{
int iEventCount; // total events
uint32_t ulTime[16384]; // timestamp of each event
unsigned char ucAddr[16384]; // address of event
unsigned char ucEvent[16384]; // byte event
} PCE_EVENT_QUEUE;

typedef struct tagQSCHANNEL
{
uint32_t ulBank; // bank (x 16)
uint32_t ulAddress; // start address
uint32_t ulFreq;   // frequency
uint32_t ulReg3; // unknown
uint32_t ulLoop; // loop address
uint32_t ulEnd; // end address
uint32_t ulVol; // master volume
uint32_t ulPan; // left/right pan control
uint32_t ulReg9; // unknown
uint32_t ulOffset; // current offset
unsigned char ucKey; // key on/off
unsigned char ucLVol; // left volume
unsigned char ucRVol; // right volume
signed char cLast; // last sample value
} QSCHANNEL;

// PSG Channel structure
typedef struct tagPSGCHANNEL
{
unsigned short usFreq;
unsigned char ucChanVol;
unsigned char ucBalance;
unsigned char ucSample; // single sample sent to DAC
unsigned char ucWaveBuf[32]; // 32-byte circular waveform buffer
unsigned char ucWaveIndex; // write index into the waveform data
unsigned int iRenderDelta; // sample delta * 65536 based on current channel freq and sample freq
unsigned int iRenderIndex; // current index for output of the waveform data
unsigned int iRenderSum; // sum which converts frequency into sample index
} PSGCHANNEL;

// PSG structure
typedef struct tagPCEPSG
{
unsigned char ucChannel;
unsigned char ucVolume;
unsigned char ucLFOFreq;
unsigned char ucLFOCtrl;
unsigned char ucNoiseState[2];
unsigned char ucNoise[2];
unsigned int iDivisor;
int iNoiseDelta[2];
int iNoiseIndex[2];
int iPRNG[2];
PSGCHANNEL chan[8];
} PCEPSG;

/* Structure for holding queued events such as DAC and fast sound chip updates */
typedef struct tagEMU_EVENT_QUEUE
{
int iCount;
int iFrameTicks; // 1 frame's worth of CPU ticks
uint32_t ulTime[16384]; // clock ticks of each event
unsigned char ucEvent[16384]; // byte event
} EMU_EVENT_QUEUE;

typedef struct _serialQ
{
int iHead;
int iTail;
unsigned char ucLink;
unsigned char cBuffer[0x800];
} SERIALQ;

#ifndef MAIN_MODULE
extern HWND hwndDebug, hwndClient;
extern int *iCharX, *iCharY;
extern int iFrame, iOldFrame;
extern unsigned char cTransparent, cTransparent2;
extern unsigned int iTimerFreq;
//extern int iFrameTime;
extern BOOL bPerformanceCounter;
extern uint32_t lTargTime, lCurrTime;
extern LARGE_INTEGER iLargeFreq;
extern uint32_t ulKeys, ulJoy, lDirtyRect;
extern signed char cRangeTable[];
extern unsigned char cSpriteFlags[];
extern int iFPS; /* Frames per second */
extern int iSampleRate; /* Audio sample rate 0-2 = (1<<i)*11025 */
extern int iSpriteLimitX, iSpriteLimitY;
extern SNDSAMPLE *pSounds; /* Sound samples structure */
extern unsigned long *lCharAddr, *lCharAddr2; /* Array to translate arcade screen address into PC video address */
//extern unsigned char *pSoundPROM, *cDirtyChar;
extern SCREENAREA *pSA;
extern unsigned short *pColorConvert;
//extern HDC hdcScreen;
extern int iNumAreas;
extern BOOL bHiLoaded, bThrottle;
#endif

//#if defined(_WIN32) || defined(PORTABLE)
#define ARESETZ80 RESETZ80
#define AEXECZ80 EXECZ80
#define ARESET6502 RESET6502
#define ARESETNES6502 RESETNES6502
#define AEXEC6502 EXEC6502
#define AEXECNES6502 EXECNES6502
#define AEXECH6280 EXECH6280
#define ARESETH6280 RESETH6280
#define AEXECH6280_2 EXECH6280_2
#define ARESETH6280_2 RESETH6280_2
#define ARESET68K RESET68K
#define AEXEC68K EXEC68K
//#endif

/* huc6280 Emulator */
void EXECH6280(unsigned char *memmap, REGSH6280 *regs, EMUHANDLERS *emuh, int *cpuClocks, unsigned char *ucIRQs, unsigned char *pFlags);
void RESETH6280(unsigned char *pFlags, unsigned char *mem, REGSH6280 *regs);
void EXECH6280_2(unsigned char *memmap, REGSH6280 *regs, EMUHANDLERS2 *emuh, int *cpuClocks, unsigned char *ucIRQs, unsigned char *pFlags);
void RESETH6280_2(unsigned char *pFlags, unsigned char *mem, REGSH6280 *regs);

/* 8086 Emulator */
void RESET8086(REGS8086 *);
void EXEC8086(char *, REGS8086 *, EMUHANDLERS2 *, int *, unsigned char *, int, unsigned long *);
/* 6502 Emulator */
void EXEC6502(unsigned char *, REGS6502 *, EMUHANDLERS *, int *, unsigned char *);
void EXEC6502DECRYPT(unsigned char *, REGS6502 *, EMUHANDLERS *, int *, unsigned char *);
void RESET6502(unsigned char *, REGS6502 *);
void EXECNES6502(unsigned char *, REGS6502 *, EMUHANDLERS *, int *, unsigned char *);
void RESETNES6502(unsigned char *, REGS6502 *);

void ARESETZ80(REGSZ80 *, int);
void AEXECZ80(unsigned char *memmap, REGSZ80 *regs, EMUHANDLERS *emuh, int *cpuClocks, unsigned char *ucIRQs);
void AEXEC68K(unsigned char *pFlags, REGS68K *pregs68K, EMUHANDLERS3 *pemuh, int *pCPUTicks, unsigned char *pIRQ, int iHack, unsigned long *pCPUOffsets);
void ARESET68K(unsigned long *pCPUOffsets, REGS68K *pegs68K, int iCPUType);
void EXEC68K(unsigned char *pFlags, REGS68K *pregs68K, EMUHANDLERS3 *pemuh, int *pCPUTicks, unsigned char *pIRQ, int iHack, unsigned long *pCPUOffsets);
void RESET68K(unsigned long *pCPUOffsets, REGS68K *pegs68K, int iCPUType);
/* V30 Emulator */
void RESETV30(REGS8086 *);
void EXECV30(unsigned char *, REGS8086 *, EMUHANDLERS2 *, int *, unsigned char *, int, unsigned long *);

/* I8039 Emulator */
void EXEC8039(unsigned char *, REGS8039 *, EMUHANDLERS *, int *, unsigned char *);
void RESET8039(unsigned char *, REGS8039 *);
void RESET6809(REGS6809 *);
void ARESET6809(REGS6809 *);
void RESET6800(unsigned char *mem, REGS6800 *regs);
void RESET6803(REGS6800 *);
void RESET6800(unsigned char *, REGS6800 *);
void EXEC6800(char *mem, REGS6800 *regs, EMUHANDLERS *emuh, int *iClocks, unsigned char *ucIRQs, int iHack, unsigned long *ulCPUOffs);
void EXEC6809(REGS6809 *);
void AEXEC6809(REGS6809 *);
void EXEC6803(REGS6800 *);
void M6803TimerUpdate(REGS6800 *regs);
BOOL EMULoadRoms(LOADROM *roms, char *szName, int *iNumHandlers, EMUHANDLERS *emuh, unsigned char *mem_map, BOOL bSetFlags);
BOOL EMULoadRoms16(LOADROM *, char *, char *);
unsigned int EMURand(void);
void EMUDrawCharTransparent2(int iAddr, int iPitch, int iChar, int iColor, unsigned char *pCharData);
void EMUDrawCharFast(int iAddr, int iPitch, int iChar, int iColor, unsigned long *pCharAddrs, unsigned char *pCharData);
void EMUDrawTile16(int sx, int sy, int iTile, int iColor, BOOL bFlipx, BOOL bFlipy, unsigned char *pTileData, unsigned short *pColorPROM, unsigned char *pTilemap, int iTilePitch, int iSize);
