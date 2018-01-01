/*****************************************************/
/* Code to emulate Sega GameGear hardware            */
/*                                                   */
/* Written by Larry Bank                             */
/* Copyright 2004-2017 BitBank Software, Inc.        */
/* Drivers started 4/10/04                           */
/*****************************************************/
// Change Log
// 3/13/2005 - Changed per scanline clocks from 228 to 236.  This allowed Desert Strike to work
// 3/14/2005 - Fixed lineint problem in Sonic Spinball & GreenDog by NOT setting bit 6 of VDP status register
// 3/11/2007 - improved SN76496 code
// 12/05/2008 - fixed 8-bit audio support
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
#include "emuio.h"
#include "smartgear.h"
#include "emu.h"
#include "sound.h"
#include "sn76_gg.h"

#ifdef _WIN32_WCE
typedef void * (*GXBEGINDRAW)(void);
typedef void * (*GXENDDRAW)(void);
extern GXBEGINDRAW pfnGXBeginDraw;
extern GXENDDRAW pfnGXEndDraw;
#endif

extern int iCheatCount, iaCheatAddr[], iaCheatValue[], iaCheatCompare[];

void ARESETZ80(REGSZ80 *, int);
void AEXECZ80(unsigned char *memmap, REGSZ80 *regs, EMUHANDLERS *emuh, int *cpuClocks, unsigned char *ucIRQs);
int EMULoadGGRoms(char *pszROM, unsigned char **pBankROM, int *GameMode);

// GG state machine vars
typedef struct _gg_machine
{
BOOL bVideoEnable;
BOOL bHideCol1;
BOOL bNoHScroll;
BOOL bWindow;
BOOL bZoomSprites;
int iBottom;
int XOffset;
int YOffset;
int iTrueWidth;
signed int iScrollX;
signed int iScrollY;
int iJoyMode;
int iGameMode;
int iROMSize;
int iROMSizeMask;
int iROMPages;
int iSpriteSize;
int iBackColor;
int iNameTable;
int iSpriteBase;
int iSpritePattern;
int iSpriteCount;
uint32_t ulOldKeys[2];
unsigned short usPalConvert[64];
unsigned char ucSpriteDraw[256];
unsigned char ucSpriteList[64];
unsigned char ucVDPRegs[16];
uint32_t ulPalChanged;
EMUHANDLERS emuh[4];
int iVRAMAddr;
int iLineInt;
int iLineCount;
BOOL bVBlank;
BOOL bVBlankIntEnable;
BOOL bLineInt;
BOOL bSpriteCollision;
//BOOL bEarlyClock;
BOOL bLineIntEnable;
BOOL bSecondByte;
BOOL bAllowNMI;
unsigned short *usPalData; // 16 background colors + 16 object colors
unsigned char ucBanks[4]; // current ROM/RAM bank settings
unsigned char ucJoyButtons[4];
SERIALQ *InQ;
SERIALQ *OutQ;
REGSZ80 regsz80;
unsigned char ucDDR;
unsigned char ucLastRead;
unsigned char ucIRQs;
unsigned char ucPort1;
unsigned char ucPort5;
unsigned char ucRegion;
unsigned char ucVRAMBuf;
unsigned char ucVDPMode;
unsigned char ucVDPData;
unsigned char ucSoundChannels;
unsigned char *mem_map;
unsigned char *pVRAM;
unsigned char *pBankROM;
} GG_MACHINE;

extern BOOL bSlave, bMaster;
GG_MACHINE *pMachine, ggmachine[3]; // allow for head2head play and spare machine for restoring after game load
//void * ohandle;
static int iCPUTicks;
static int iGGRefCount = 0;
extern unsigned char *pSoundBuf;
int Z80OLDD, Z80OLDPC;
static SN76_GG *pPSG;
static EMU_EVENT_QUEUE *pEEQ;
BOOL bTrace, bColeco;
int iSG1000Tile, iSG1000Color;
int iTrace;
extern int iSampleRate;
extern int iDisplayWidth, iDisplayHeight;
extern int iVideoSize, iScreenX, iScreenY;
int iOldWidth;
static int iPlayer = 0; // current player (0 or 1)
extern SERIALQ *InQ, *OutQ;
//extern HBITMAP hbmScreen;
//extern HDC hdcMem;
extern BOOL bHead2Head, b16BitAudio, bSkipFrame, bStereo, bRunning, bAutoLoad, bRegistered;
extern char pszROMS[], pszSAVED[], pszCAPTURE[];
extern uint32_t *pTempBitmap;
extern int iScale;
extern unsigned char *pScreen;
extern int iScreenPitch;
//extern int iStat1, iStat2;

unsigned char *pLocalBitmap;
extern int iPitch, iScreenBpp;
static int iTotalClocks;
extern unsigned short *pColorConvert; // table to convert BGR555 to RGB565
//extern char szGameName[];
uint32_t crc32_table[256];
static uint32_t ulCRC;
void GGIOWrite(int, unsigned char);
unsigned char GGIORead(int);
void ColecoIOWrite(int, unsigned char);
unsigned char ColecoIORead(int);
void ColecoBankWrite(int, unsigned char);
unsigned char ColecoBankRead(int);
//void GGNetUpdate(void);
void ARM816FAST(unsigned char *pSrc, uint32_t *pDest, unsigned short *PalConvert, int iCount);
void ARMDrawGGTiles(unsigned short *pTile, unsigned short *pEdge, uint32_t *pSrc, uint32_t *pDest, int iWidth);
// List matching CRCs to game names (since Sega did not encode the names in the ROM)
char *szNameList[] = {TEXT("Pac Attack"),TEXT("5-in-1 Funpak"),TEXT("Alien Syndrome"),TEXT("Alladin"),
                       TEXT("Arcade Classics"), TEXT("Ax Battler - A Legend of Golden Axe"), TEXT("Baku Baku Animals"), TEXT("Bart vs. The Space Mutants"),
                       TEXT("Bubble Bobble"),TEXT("Bust-a-Move"),TEXT("Choplifter 3"),TEXT("Earthworm Jim"),
                       TEXT("Ecco the Dolphin"),TEXT("Fantasy Zone Gear"),TEXT("Galaga '91"),TEXT("Shinobi 2"),
                       TEXT("Shinobi"),TEXT("The Incredible Crash Dummies"),TEXT("Itchy & Scratchy"),TEXT("Klax"),
                       TEXT("Krusty's Fun House"),TEXT("The Lion King"),TEXT("The Little Mermaid"),TEXT("Mappy"),
                       TEXT("Marble Madness"),TEXT("Megaman"),TEXT("Mortal Kombat"),TEXT("Ms. Pac-Man"), // 24-27
                       TEXT("Outrun"),TEXT("Pac-Man"),TEXT("Paperboy"),TEXT("Pengo"), //28-31
                       TEXT("Puyo Puyo 2"),TEXT("Sega 4-in-1 Pack"),TEXT("Shining Force 2 - The Sword of Haija"),TEXT("Shining Force Gaiden"),
                       TEXT("Sokoban World"),TEXT("Sonic & Tails"),TEXT("Sonic Chaos"),TEXT("Sonic Drift"),
                       TEXT("Sonic Labryrinth"),TEXT("Sonic Spinball"),TEXT("Sonic the Hedgehog - Triple Trouble"),TEXT("Sonic & Tails 2"),
                       TEXT("Sonic Drift 2"),TEXT("Sonic the Hedgehog 2"),TEXT("Sonic the Hedgehog"),TEXT("Super Space Invaders"),
                       TEXT("Spiderman - Return of the Sinister Six"),TEXT("Stargate"),TEXT("Super Columns"),TEXT("Wonderboy 3 - Dragon's Trap"),
                       TEXT("X-Men"),TEXT("Back to the Future 3"),TEXT("Mean Bean Machine"),TEXT("G-Loc Air Battle"),TEXT("Putt & Putter"),
                       TEXT("Streets of Rage"),TEXT("Super Off-Road"), TEXT("Castle of Illusion"), // 57-59
                       TEXT("Cliffhanger"), TEXT("Coca Cola Kid"), TEXT("Columns"),TEXT("Cool Spot"), // 60-63
                       TEXT("Desert Strike"),TEXT("Double Dragon"),TEXT("Dragon Crystal"),TEXT("Echo the Dolphin 2"), // 64-67
                       TEXT("Frogger"),TEXT("Gamble Panic"),TEXT("Gear Works"),TEXT("Greendog"), // 68-71
                       TEXT("GP Rider"),TEXT("Jeopardy!"),TEXT("Judge Dredd"),TEXT("Mickey Mouse - Land of Illusion"), // 72-75
                       TEXT("Mickey Mouse - Land of Illusion"),TEXT("Micro Machines 2"),TEXT("Micro Machines"),TEXT("Nazo Puyo"), // 76-79
                       TEXT("Nazo Puyo 2"),TEXT("Predator 2"),TEXT("Prince of Persia"),TEXT("Ren & Stimpy"), // 80-83
                       TEXT("Road Rash"),TEXT("Shining Force 3"),TEXT("Slider"),TEXT("Sonic Blast"), // 84-87
                       TEXT("Strider Returns"),TEXT("Super Return of the Jedi"),TEXT("Tom and Jerry - The Movie"),TEXT("Winter Olympics - Lillihammer"), // 88-91
                       TEXT("Zool"),TEXT("Zoop"), // 92-93
                       NULL};
uint32_t ulCRCList[] = {0xec9b23e1, 0x1a5466ca, 0x810c208a, 0x0f27cb70, // 0-3
                             0xdfe24231, 0x843525a8, 0xf2a27956, 0x220e7856, // 4-7
                             0x19add2e7, 0x2b01c3cd, 0x20e64cb0, 0x28592905, // 8-11
                             0x5e068d92, 0xa8785a25, 0x7b7b77e9, 0x800f2cb6, // 12-15
                             0xd2ff23a6, 0xea712865, 0xa6e908f8, 0xe5a83143, // 16-19
                             0x3210926d, 0xed6edd1f, 0x75ed4bae, 0x7f3a10e7, // 20-23
                             0x7757091b, 0x6fa89903, 0xae990ecd, 0x20004618, // 24-27
                             0xab647fb1, 0xdf849395, 0x8ba3a5ce, 0xcf99c11c, // 28-31
                             0x4fd43397, 0x326fdc8e, 0xd3ac6505, 0x38794c35, // 32-35
                             0x71d6f58d, 0xffa6d072, 0xe3310f5d, 0x1d96adda, // 36-39
                             0x20361d97, 0xdc470e98, 0xa75c203f, 0x3c0dc4c8, // 40-43
                             0xa38ea9a9, 0xe0c7846b, 0xdc3f21ae, 0xcf806b9e, // 44-47
                             0x5e2aed5b, 0x1e728ec6, 0xf54cf73e, 0x3915b166, // 48-51
                             0x231c544a, 0xcf462bf1, 0xde23a56a, 0x5ddb3bd8, // 52-55
                             0x3e920dbd, 0xdf85253f, 0x50c9b263, 0x7b4c5cb9, // 56-59
                             0x5d7b5b71, 0xb23f812d, 0x82e5da7f, 0xce7b65ea, // 60-63
                             0x83a20a76, 0xf10948b2, 0x701a205e, 0x9795b8af, // 64-67
                             0x387cad32, 0x7c354dee, 0x974a2279, 0x1077cf92, // 68-71
                             0xf20891de, 0x358ce603, 0x71b43568, 0xbb3cd21b, // 72-75
                             0x27bdf94d, 0xae8e83f0, 0x15cbced4, 0xc2269219, // 76-79
                             0x919d77c6, 0x07f9639b, 0xd313c241, 0x1923144d, // 80-83
                             0xe36255da, 0x157ff4f2, 0x332e3898, 0xa42377b5, // 84-87
                             0xfcb14fe8, 0x3f5ebc1a, 0xbeddd5d0, 0xa43b39f7, // 88-91
                             0x01501dcf, 0x8d7f3d8c,        // 92-93
                             0};

// Translate the bits from the parallel link cable (wired as follows)
//        WRITE           READ
//        Bit 0   ----->  Bit 2
//        Bit 1   ----->  Bit 3
//        Bit 2   ----->  Bit 0
//        Bit 3   ----->  Bit 1
//        Bit 4   ----->  Bit 5
//        Bit 5   ----->  Bit 4
//        Bit 6   ----->  Bit 6
//        Bit 7   Unused
unsigned char ucTranslate[128] =
{0x00,0x04,0x08,0x0c,0x01,0x05,0x09,0x0d,0x02,0x06,0x0a,0x0e,0x03,0x07,0x0b,0x0f, // 00-0F
 0x20,0x24,0x28,0x2c,0x21,0x25,0x29,0x2d,0x22,0x26,0x2a,0x2e,0x23,0x27,0x2b,0x2f, // 10-1F
 0x10,0x14,0x18,0x1c,0x11,0x15,0x19,0x1d,0x12,0x16,0x1a,0x1e,0x13,0x17,0x1b,0x1f, // 20-2F
 0x30,0x34,0x38,0x3c,0x31,0x35,0x39,0x3d,0x32,0x36,0x3a,0x3e,0x33,0x37,0x3b,0x3f, // 30-3F
 0x40,0x44,0x48,0x4c,0x41,0x45,0x49,0x4d,0x42,0x46,0x4a,0x4e,0x43,0x47,0x4b,0x4f, // 40-4F
 0x60,0x64,0x68,0x6c,0x61,0x65,0x69,0x6d,0x62,0x66,0x6a,0x6e,0x63,0x67,0x6b,0x6f, // 50-5F
 0x50,0x54,0x58,0x5c,0x51,0x55,0x59,0x5d,0x52,0x56,0x5a,0x5e,0x53,0x57,0x5b,0x5f, // 60-6F
 0x70,0x74,0x78,0x7c,0x71,0x75,0x79,0x7d,0x72,0x76,0x7a,0x7e,0x73,0x77,0x7b,0x7f  // 70-7F
};
unsigned char vcnt[263] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA9,/* <-- hack for Earthworm Jim to work */ 0xA8, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
                                  0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};
// Table to convert sequential scan line to strange number used by GG
unsigned char ucScanLineConvert[262] = 
   {0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
    0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
    0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xd5, 0xd6, 0xd7};

// Byte expansion tables used to convert tiles with separate bit planes into composite 8bpp images
// each 16-entry table converts a nibble into a 4-byte long.  These are then shifted over to all 8 bit positions
// Each 4-byte entry appears mirrored because on a little-endian machine the bytes are stored backwards
uint32_t ulExpand0[16] = {
0x00000000,0x01000000,0x00010000,0x01010000,0x00000100,0x01000100,0x00010100,0x01010100,
0x00000001,0x01000001,0x00010001,0x01010001,0x00000101,0x01000101,0x00010101,0x01010101};
uint32_t ulExpand1[16] = {
0x00000000,0x02000000,0x00020000,0x02020000,0x00000200,0x02000200,0x00020200,0x02020200,
0x00000002,0x02000002,0x00020002,0x02020002,0x00000202,0x02000202,0x00020202,0x02020202};
uint32_t ulExpand2[16] = {
0x00000000,0x04000000,0x00040000,0x04040000,0x00000400,0x04000400,0x00040400,0x04040400,
0x00000004,0x04000004,0x00040004,0x04040004,0x00000404,0x04000404,0x00040404,0x04040404};
uint32_t ulExpand3[16] = {
0x00000000,0x08000000,0x00080000,0x08080000,0x00000800,0x08000800,0x00080800,0x08080800,
0x00000008,0x08000008,0x00080008,0x08080008,0x00000808,0x08000808,0x00080808,0x08080808};
uint32_t ulExpandMirror0[16] = {
0x00000000,0x00000001,0x00000100,0x00000101,0x00010000,0x00010001,0x00010100,0x00010101,
0x01000000,0x01000001,0x01000100,0x01000101,0x01010000,0x01010001,0x01010100,0x01010101};
uint32_t ulExpandMirror1[16] = {
0x00000000,0x00000002,0x00000200,0x00000202,0x00020000,0x00020002,0x00020200,0x00020202,
0x02000000,0x02000002,0x02000200,0x02000202,0x02020000,0x02020002,0x02020200,0x02020202};
uint32_t ulExpandMirror2[16] = {
0x00000000,0x00000004,0x00000400,0x00000404,0x00040000,0x00040004,0x00040400,0x00040404,
0x04000000,0x04000004,0x04000400,0x04000404,0x04040000,0x04040004,0x04040400,0x04040404};
uint32_t ulExpandMirror3[16] = {
0x00000000,0x00000008,0x00000800,0x00000808,0x00080000,0x00080008,0x00080800,0x00080808,
0x08000000,0x08000008,0x08000800,0x08000808,0x08080000,0x08080008,0x08080800,0x08080808};

int iLoopCount, iScanLine;
extern BOOL bUserAbort, bFullScreen, bUseDDraw;
//extern int ohandle;
extern BOOL bTrace, bUseSound, bAutoLoad, bSmartphone;
extern PALETTEENTRY EMUpe[];
void EMUCreateColorTable(PALETTEENTRY *);
void EMUUpdatePalette(int, PALETTEENTRY *);

#ifdef BOGUS
/* Windows BMP header info (54 bytes) */
unsigned char winbmp[54] =
        {0x42,0x4d,
         0,0,0,0,         /* File size */
         0,0,0,0,0x36,0,0,0,0x28,0,0,0,
         0,0,0,0, /* Xsize */
         0,0,0,0, /* Ysize */
         1,0,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       /* number of planes, bits per pel */
         0,0,0,0};

/***************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDumpScreen(void)                                        *
 *                                                                          *
 *  PURPOSE    : Save the current screen to a BMP file.                     *
 *                                                                          *
 ****************************************************************************/
void EMUDumpScreen(void)
{
#ifndef _WIN32_WCE
int oHandle;
int cx, cy, lsize, i, x;
long *l;
char pszTemp[2048];
unsigned short us, *us1, *us2;

   cx = iScreenX;
   cy = iScreenY;
   wsprintf(pszTemp, TEXT("%s\\%s.bmp"), pszCAPTURE, szGameName);
//   strcpy(pszTemp, pszHome);
//   strcat(pszTemp, "CAPTURE.BMP");
   oHandle = EMUCreate(pszTemp);
   lsize = (cx*2 + 3) & 0xfffc; /* Width of each line (dword aligned) */
   /* Write the BMP header */
   l = (long *)&winbmp[2];
   *l = (cy * lsize) + 54; /* Store the file size */
   l = (long *)&winbmp[18];
   *l = cx;      /* width */
   *(l+1) = cy;  /* height */
   l = (long *)&winbmp[34]; // data size
   *l = (cy * lsize);
   EMUWrite(oHandle, winbmp, 54);

   /* Write the image data */
   for (i=cy-1; i>=0; i--)
      {
      // Scale the current image back down to 1:1
      us1 = (unsigned short *)&pTempBitmap[(i*cx*iScale*iScale)/2];
      us2 = (unsigned short *)pszTemp;
      for (x=0; x<iScreenX; x++)
         {
         us = us1[x*iScale];
         *us2++ = ((us & 0xffc0) >> 1) | (us & 0x1f); // 16bpp Win bmp is really RGB555
         }
      EMUWrite(oHandle, pszTemp, lsize);
      }
   EMUClose(oHandle);
#endif
} /* EMUDumpScreen() */
#endif // BOGUS

void ColecoIOWrite(int iAddr, unsigned char ucByte)
{
//unsigned char c;

    switch (iAddr & 0xe0)
      {
      case 0x80:
         pMachine->iJoyMode=0;
         break;
      case 0xc0:
         pMachine->iJoyMode=1;
         break;
      case 0xe0: // PSG port
         if (iPlayer == 0)
            {
            pEEQ->ulTime[pEEQ->iCount] = iTotalClocks + iCPUTicks;
            pEEQ->ucEvent[pEEQ->iCount++] = ucByte;
            }
//         WriteSN76496(0, pPSG, ucByte);
         break;

      case 0xa0: // VRAM access
         if (!(iAddr & 1)) // data access
            {
            if (pMachine->ucVDPMode == 0xc0) // Color RAM
               {
               iAddr = 0x4000 + (pMachine->iVRAMAddr & 0x3f);
               if (pMachine->pVRAM[iAddr] != ucByte)
                  {
                  pMachine->pVRAM[iAddr] = ucByte;
                  if (pMachine->iGameMode != MODE_SG1000) // that machine uses a fixed palette
                     pMachine->ulPalChanged |= 1 << ((iAddr & 0x3f)>>1);
                  }
               }
            else // Ax Battler uses an invalid write mode, but we need to allow it anyway
   //         if (pMachine->ucVDPMode <= 0x40)
               {
               pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff] = ucByte;
               }
            pMachine->iVRAMAddr++;
            pMachine->bSecondByte = FALSE;
            }
         else // control access
            {
            if (pMachine->bSecondByte) // write the data into the correct register
               {
               pMachine->iVRAMAddr = ((ucByte & 0x3f) << 8) | pMachine->ucVDPData; // set the new VRAM address
               pMachine->bSecondByte = FALSE;
               pMachine->ucVDPMode = ucByte & 0xc0;
   //            if (pMachine->ucVDPMode == 0x80) // registers
               if ((ucByte & 0xf0) == 0x80)
                  {
                  switch (ucByte & 0x3f) // register number
                     {
                     case 0:
   //                     pMachine->bEarlyClock = pMachine->ucVDPData & 8;
                        pMachine->bLineIntEnable = pMachine->ucVDPData & 0x10;
                        if (pMachine->bLineInt)
                           {
                           if (pMachine->bLineIntEnable)
                              pMachine->ucIRQs |= INT_IRQ;
                           else
                              pMachine->ucIRQs &= ~INT_IRQ;
                           }
                        pMachine->bWindow = pMachine->ucVDPData & 0x80;
                        pMachine->bHideCol1 =  (pMachine->ucVDPData & 0x20 && pMachine->iGameMode != MODE_GG);
                        if (pMachine->iGameMode != MODE_GG)
                           pMachine->bNoHScroll = pMachine->ucVDPData & 0x40;
                        pMachine->ucVDPRegs[0] = pMachine->ucVDPData;
                        break;
                     case 1:
                        if (pMachine->ucVDPData & 2)
                           pMachine->iSpriteSize = 16;
                        else
                           pMachine->iSpriteSize = 8;
                        pMachine->bVBlankIntEnable = pMachine->ucVDPData & 0x20;
                        if (pMachine->bVBlank)
                           {
                           if (pMachine->bVBlankIntEnable)
                              pMachine->ucIRQs |= INT_IRQ;
                           else
                              pMachine->ucIRQs &= ~INT_IRQ;
                           }
                        pMachine->bVideoEnable = pMachine->ucVDPData & 0x40;
                        pMachine->bZoomSprites = pMachine->ucVDPData & 1;
                        break;
                     case 2:
                        if (pMachine->iGameMode == MODE_SG1000)
                           pMachine->iNameTable = (pMachine->ucVDPData & 0xf) * 0x400; //address of pattern name table
                        else
                           pMachine->iNameTable = (pMachine->ucVDPData & 0xe) * 0x400; //address of pattern name table
                        break;
                     case 3: // Used on SG1000 (color table address)
                        iSG1000Color = (((pMachine->ucVDPData & 0x80)<< 6) | (pMachine->iLineInt << 14)) & 0x3fff;
                        pMachine->ucVDPRegs[3] = pMachine->ucVDPData;
                        break;
                     case 4: // Used on SG1000 (tile data)
                        iSG1000Tile = (pMachine->ucVDPData & 4) << 11;
                        break;
                     case 5: // sprite base address
                        if (pMachine->iGameMode == MODE_SG1000)
                           pMachine->iSpriteBase = (pMachine->ucVDPData & 0x7f) * 0x80;
                        else
                           pMachine->iSpriteBase = (pMachine->ucVDPData & 0x7e) * 0x80;
                        break;
                     case 6: // sprite pattern address
                        if (pMachine->iGameMode == MODE_SG1000)
                           pMachine->iSpritePattern = (pMachine->ucVDPData & 7) << 11;
                        else
                           pMachine->iSpritePattern = (pMachine->ucVDPData & 4) * 0x800;
                        break;
                     case 7:
                        pMachine->iBackColor = (pMachine->ucVDPData & 0xf) + 16;
                        break;
                     case 8:
                        pMachine->iScrollX = (255 - pMachine->ucVDPData + pMachine->XOffset + 1) & 0xff;
                        break;
                     case 9:
                        pMachine->iScrollY = (pMachine->ucVDPData + pMachine->YOffset) % 224;
                        break;
                     case 10:
                        pMachine->iLineInt = pMachine->ucVDPData;
   //                     if (pMachine->iLineInt == 0) // fixes Desert Strike
   //                        pMachine->iLineInt = 256;
                        if (pMachine->iGameMode == MODE_SG1000)
                           iSG1000Color = ((pMachine->ucVDPRegs[3] << 6) | (pMachine->iLineInt << 14)) & 0x3fff;
                        break;
                     default:
  //                      c = 0;
                        break;
                     }
                  return; // updating video registers
                  }
   //            pMachine->iVRAMAddr = ((ucByte & 0x3f) << 8) | pMachine->ucVDPData; // set the new VRAM address
               if (pMachine->ucVDPMode  == 0) // VRAM read
                  {
                  pMachine->ucVRAMBuf = pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff];
                  pMachine->iVRAMAddr++;
                  }
               }
            else
               {
               pMachine->ucVDPData = ucByte; // load the data register
               pMachine->bSecondByte = TRUE;
               }
            }
         break;
      }
} /* ColecoIOWrite() */

unsigned char ColecoIORead(int iAddr)
{
unsigned char c = 0xff;

   switch (iAddr & 0xe0)
      {
      case 0xe0: // joysticks
         if (iAddr & 2) // player 2
            {
            c = pMachine->ucJoyButtons[2 + pMachine->iJoyMode];
            }
         else // player 1
            {
            c = pMachine->ucJoyButtons[pMachine->iJoyMode];
            }
         break;
      case 0xa0: // VRAM access
         if (!(iAddr & 1))
            {
            c = pMachine->ucVRAMBuf;
   //         if (pMachine->ucVDPMode == 0xc0) // Color RAM
   //            pMachine->ucVRAMBuf = pMachine->pVRAM[0x4000 + (iVRAMAddr & 0x3fff)];
   //         else
               pMachine->ucVRAMBuf = pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff]; // always reads from VRAM
            pMachine->iVRAMAddr++;
            pMachine->bSecondByte = FALSE;
            }
         else // VDP control
            {
            c = 0;
            pMachine->bSecondByte = FALSE; // resets the data write sequence
            if (pMachine->bVBlank) // vertical blank
               {
               c |= 0x80;
               pMachine->bVBlank = 0; // cleared on reading it
               }
            if (pMachine->bLineInt) // line interrupt
               {
   //            c |= 0x40;
               pMachine->bLineInt = 0; // cleared on reading it
               }
            if (pMachine->bSpriteCollision)
               {
               c |= 0x20;
               pMachine->bSpriteCollision = 0;
               }
            pMachine->ucIRQs &= ~INT_IRQ; // clear any pending interrupt
            }
         break;
      }

   return c;

} /* ColecoIORead() */

void ColecoBankWrite(int usAddr, unsigned char ucByte)
{
   pMachine->mem_map[usAddr & 0x3ff] = ucByte;
} /* ColecoBankWrite() */

unsigned char ColecoBankRead(int usAddr)
{
   return pMachine->mem_map[usAddr & 0x3ff];
} /* ColecoBankRead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGIORead(unsigned short)                                   *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the GameGear.                           *
 *                                                                          *
 ****************************************************************************/
unsigned char GGIORead(int iAddr)
{
unsigned char c = 0;

   switch (iAddr)
      {
      case 0: // start button
         c = pMachine->ucJoyButtons[2];
         break;

      case 1:
         c = (pMachine->OutQ->ucLink & ~pMachine->ucDDR); // last output bits are read back
         c |= (ucTranslate[pMachine->InQ->ucLink & 0x7f] & pMachine->ucDDR); // valid input bits
         break;

      case 2:
      case 3:
         c = 0;
         break;

      case 4: // receive serial data
         pMachine->bAllowNMI = TRUE;
         if (pMachine->ucPort5 & 0x20 && pMachine->InQ->iTail != pMachine->InQ->iHead) // if receive enabled
            {
            c = pMachine->InQ->cBuffer[pMachine->InQ->iTail]; // get the current serial data byte
            pMachine->ucLastRead = c;
            pMachine->InQ->iTail = (pMachine->InQ->iTail+1) & 0x7ff;
            }
         else
            c = pMachine->ucLastRead;
         break;

      case 5: // link up
         c = pMachine->ucPort5 & 0xf8; // baud rate and other output settings
         if (c & 0x20 && pMachine->InQ->iHead != pMachine->InQ->iTail) // receive is enabled and data is waiting to be read
            c |= 2;
         break;

      case 0x30: // ack?
         c = 0;
         break;

      case 0xc0:
      case 0xdc: // joystick port 1
         c = pMachine->ucJoyButtons[0];
         break;
      case 0xdd: // joystick port 2
         c = pMachine->ucJoyButtons[1];
         break;
         
      case 0xc1: // region detection
//      case 0xdd:
         c = 0x3f;
         c |= pMachine->ucRegion & 0x80; // bit 7->7
         c |= ((pMachine->ucRegion << 1) & 0x40); // bit 5->6
//         if (bJapanese)
//            c ^= 0xc0; //select japanese
         break;

      case 0x7e: // v-counter (scanline)
//         if (bGameGear)
            c = vcnt[iScanLine];
//         else
//            c = ucScanLineConvert[iScanLine]; // convert our number to the strange numbering used by GG
         break;

      case 0x7f: // h-counter
         c = 0x40; // return a middle value
         break;

      case 0xbe: // VRAM access
         c = pMachine->ucVRAMBuf;
//         if (pMachine->ucVDPMode == 0xc0) // Color RAM
//            pMachine->ucVRAMBuf = pMachine->pVRAM[0x4000 + (iVRAMAddr & 0x3fff)];
//         else
            pMachine->ucVRAMBuf = pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff]; // always reads from VRAM
         pMachine->iVRAMAddr++;
         pMachine->bSecondByte = FALSE;
         break;

      case 0xbd:
      case 0xbf: // video status register
         c = 0;
         pMachine->bSecondByte = FALSE; // resets the data write sequence
         if (pMachine->bVBlank) // vertical blank
            {
            c |= 0x80;
            pMachine->bVBlank = 0; // cleared on reading it
            }
         if (pMachine->bLineInt) // line interrupt
            {
//            c |= 0x40;
            pMachine->bLineInt = 0; // cleared on reading it
            }
         if (pMachine->bSpriteCollision)
            {
            c |= 0x20;
            pMachine->bSpriteCollision = 0;
            }
         pMachine->ucIRQs &= ~INT_IRQ; // clear any pending interrupt
         break;

      case 0xf0: // YM2413 address
         break;

      case 0xf1: // YM2413 data
         break;

      case 0xf2: // FM detection
         c = 0xff; // not present
         break;
      default:
         c = 0xff;
         break;
      }

   return c;

} /* GGIORead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGIOWrite(unsigned short, unsigned char)                   *
 *                                                                          *
 *  PURPOSE    : Emulate the I/O of the GameGear/SMS.                       *
 *                                                                          *
 ****************************************************************************/
void GGIOWrite(int iAddr, unsigned char ucByte)
{
//unsigned char c;

    switch (iAddr)
      {
      case 1: // external 7 bit I/O
         pMachine->OutQ->ucLink = ucByte | pMachine->ucDDR; // input bits will read as 1's (I hope)
#ifdef _WIN32_WCE
//         GGNetUpdate(); // send the status change
#endif
         break;

      case 2: // parallel I/O
         if (pMachine->ucDDR & 0x80 && (ucByte & 0x80) == 0) // falling edge activates NMI on other machine
            {
            if ((ggmachine[1-iPlayer].ucDDR & 0x80) == 0) // NMI enabled
               ggmachine[1-iPlayer].bAllowNMI = TRUE;
            }
         pMachine->ucDDR = ucByte; // data direction register
         break;

      case 3: // transmit serial data
         if (pMachine->ucPort5 & 0x10) // if sending is enabled
            {
            pMachine->OutQ->cBuffer[pMachine->OutQ->iHead] = ucByte; // store the output serial data
            pMachine->OutQ->iHead = (pMachine->OutQ->iHead + 1) & 0x7ff;
            }
         break;

      case 5: // serial communication mode
         pMachine->ucPort5 = ucByte;
         break;

      case 6: // sound output selector
         pMachine->ucSoundChannels = ucByte;
         break;

      case 0x3f: // region detection
         pMachine->ucRegion = ucByte;
         break;

      case 0x7e: // PSG port
      case 0x7f:
         if (iPlayer == 0)
            {
            pEEQ->ulTime[pEEQ->iCount] = iTotalClocks + iCPUTicks;
            pEEQ->ucEvent[pEEQ->iCount++] = ucByte;
            }
//         WriteSN76496(0, pPSG, ucByte);
         break;

//      case 0x9e:
      case 0xbe: // VRAM access
         if (pMachine->ucVDPMode == 0xc0) // Color RAM
            {
            iAddr = 0x4000 + (pMachine->iVRAMAddr & 0x3f);
            if (pMachine->pVRAM[iAddr] != ucByte)
               {
               pMachine->pVRAM[iAddr] = ucByte;
               if (pMachine->iGameMode != MODE_SG1000) // that machine uses a fixed palette
                  pMachine->ulPalChanged |= 1 << ((iAddr & 0x3f)>>1);
               }
            }
         else // Ax Battler uses an invalid write mode, but we need to allow it anyway
//         if (pMachine->ucVDPMode <= 0x40)
            {
            pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff] = ucByte;
            }
         pMachine->iVRAMAddr++;
         pMachine->bSecondByte = FALSE;
         break;

      case 0xbd:
      case 0xbf:
         if (pMachine->bSecondByte) // write the data into the correct register
            {
            pMachine->iVRAMAddr = ((ucByte & 0x3f) << 8) | pMachine->ucVDPData; // set the new VRAM address
            pMachine->bSecondByte = FALSE;
            pMachine->ucVDPMode = ucByte & 0xc0;
//            if (pMachine->ucVDPMode == 0x80) // registers
            if ((ucByte & 0xf0) == 0x80)
               {
               switch (ucByte & 0x3f) // register number
                  {
                  case 0:
//                     pMachine->bEarlyClock = pMachine->ucVDPData & 8;
                     pMachine->bLineIntEnable = pMachine->ucVDPData & 0x10;
                     if (pMachine->bLineInt)
                        {
                        if (pMachine->bLineIntEnable)
                           pMachine->ucIRQs |= INT_IRQ;
                        else
                           pMachine->ucIRQs &= ~INT_IRQ;
                        }
                     pMachine->bWindow = pMachine->ucVDPData & 0x80;
                     pMachine->bHideCol1 =  (pMachine->ucVDPData & 0x20 && pMachine->iGameMode != MODE_GG);
                     if (pMachine->iGameMode != MODE_GG)
                        pMachine->bNoHScroll = pMachine->ucVDPData & 0x40;
                     pMachine->ucVDPRegs[0] = pMachine->ucVDPData;
                     break;
                  case 1:
                     if (pMachine->ucVDPData & 2)
                        pMachine->iSpriteSize = 16;
                     else
                        pMachine->iSpriteSize = 8;
                     pMachine->bVBlankIntEnable = pMachine->ucVDPData & 0x20;
                     if (pMachine->bVBlank)
                        {
                        if (pMachine->bVBlankIntEnable)
                           pMachine->ucIRQs |= INT_IRQ;
                        else
                           pMachine->ucIRQs &= ~INT_IRQ;
                        }
                     pMachine->bVideoEnable = pMachine->ucVDPData & 0x40;
                     pMachine->bZoomSprites = pMachine->ucVDPData & 1;
                     break;
                  case 2:
                     if (pMachine->iGameMode == MODE_SG1000)
                        pMachine->iNameTable = (pMachine->ucVDPData & 0xf) * 0x400; //address of pattern name table
                     else
                        pMachine->iNameTable = (pMachine->ucVDPData & 0xe) * 0x400; //address of pattern name table
                     break;
                  case 3: // Used on SG1000 (color table address)
                     iSG1000Color = (((pMachine->ucVDPData & 0x80)<< 6) | (pMachine->iLineInt << 14)) & 0x3fff;
                     pMachine->ucVDPRegs[3] = pMachine->ucVDPData;
                     break;
                  case 4: // Used on SG1000 (tile data)
                     iSG1000Tile = (pMachine->ucVDPData & 4) << 11;
                     break;
                  case 5: // sprite base address
                     if (pMachine->iGameMode == MODE_SG1000)
                        pMachine->iSpriteBase = (pMachine->ucVDPData & 0x7f) * 0x80;
                     else
                        pMachine->iSpriteBase = (pMachine->ucVDPData & 0x7e) * 0x80;
                     break;
                  case 6: // sprite pattern address
                     if (pMachine->iGameMode == MODE_SG1000)
                        pMachine->iSpritePattern = (pMachine->ucVDPData & 7) << 11;
                     else
                        pMachine->iSpritePattern = (pMachine->ucVDPData & 4) * 0x800;
                     break;
                  case 7:
                     pMachine->iBackColor = (pMachine->ucVDPData & 0xf) + 16;
                     break;
                  case 8:
                     pMachine->iScrollX = (255 - pMachine->ucVDPData + pMachine->XOffset + 1) & 0xff;
                     break;
                  case 9:
                     pMachine->iScrollY = (pMachine->ucVDPData + pMachine->YOffset) % 224;
                     break;
                  case 10:
                     pMachine->iLineInt = pMachine->ucVDPData;
//                     if (pMachine->iLineInt == 0) // fixes Desert Strike
//                        pMachine->iLineInt = 256;
                     if (pMachine->iGameMode == MODE_SG1000)
                        iSG1000Color = ((pMachine->ucVDPRegs[3] << 6) | (pMachine->iLineInt << 14)) & 0x3fff;
                     break;
                  default:
//                     c = 0;
                     break;
                  }
               return; // updating video registers
               }
//            pMachine->iVRAMAddr = ((ucByte & 0x3f) << 8) | pMachine->ucVDPData; // set the new VRAM address
            if (pMachine->ucVDPMode  == 0) // VRAM read
               {
               pMachine->ucVRAMBuf = pMachine->pVRAM[pMachine->iVRAMAddr & 0x3fff];
               pMachine->iVRAMAddr++;
               }
            }
         else
            {
            pMachine->ucVDPData = ucByte; // load the data register
            pMachine->bSecondByte = TRUE;
            }
         break;

      case 0xde: // ?
//         c = 0;
         break;

      default:
//         c = 0;
         break;
      }
} /* GGIOWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGProcessSprites(void)                                     *
 *                                                                          *
 *  PURPOSE    : Scan the list of sprites to save time later.               *
 *                                                                          *
 ****************************************************************************/
void GGProcessSprites(void)
{
int i, y, iSize, iStart, iEnd;
unsigned char *pSprite;

   iSize = (pMachine->bZoomSprites || pMachine->iSpriteSize == 16) ? 16:8;
   pSprite = &pMachine->pVRAM[pMachine->iSpriteBase];
   memset(pMachine->ucSpriteDraw, 0, pMachine->iBottom);
   pMachine->iSpriteCount = 0;
   for (i=0; i<64; i++)
      {
      y = pSprite[i];
      if (y == 208) // end of table marker
         break;
//      y -= 1;
      if (y > 224)
         y -= 256; // negative
      if (y+iSize > 0 && y < pMachine->iBottom) // sprite is visible
         {
         pMachine->ucSpriteList[pMachine->iSpriteCount++] = i;
         iStart = y;
         iEnd = y + iSize;
         if (iStart < 0)
            iStart = 0;
         if (iEnd > pMachine->iBottom)
            iEnd = pMachine->iBottom;
         while (iStart <= iEnd)
            pMachine->ucSpriteDraw[iStart++] = 1;
         }
      }

} /* GGProcessSprites() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGDrawScanline(void)                                       *
 *                                                                          *
 *  PURPOSE    : Render the current scanline.                               *
 *                                                                          *
 ****************************************************************************/
void GGDrawScanline(void)
{
uint32_t ulLeft  = 0;
uint32_t ulRight = 0;
register unsigned char *pDest;
int iLCDScan, iWindowDelta;
signed int i, x, y, iCount;
int iLocalScrollX = 0;
int iScanOffset, iTile, iWidth;
unsigned char c2, *pSprite, *pSpriteList;//, *pTileData;
uint32_t ul, ulColor, *pulDest, *pTileBits;
uint32_t *pSrc, *pSrcFlipped;
unsigned char ucTemp[1024]; // 8-bit pixel holding area
int iTileMask, iHeight;
unsigned short *pTile, *pEdge, usTile, usOldTile;

// Draw the background
      iLCDScan = iScanLine - pMachine->YOffset; // current scan line relative to start of displayed area
      if (!pMachine->bVideoEnable) // if video display is disabled, set to black
         {
         memset(&ucTemp[0], pMachine->iBackColor, pMachine->iTrueWidth+8); // draw the background color
         goto scanlinedone;
         }
      iWidth = pMachine->iTrueWidth;
      if (pMachine->iScrollX & 7)
         iWidth += 16; // make sure to get slop over
      pDest = &ucTemp[8];
      y = (pMachine->iScrollY + iLCDScan) % 224; // current scan line to draw
      pTile = (unsigned short *)&pMachine->pVRAM[pMachine->iNameTable + (y>>3)*64];
      if (pMachine->bWindow) // SMS can have rightmost 8 columns not scroll
         iWindowDelta = ((y>>3)*64) - ((iLCDScan) >> 3) * 64;
      else
         iWindowDelta = 0;
      pEdge = pTile + 32; // right edge to scroll around
      if (pMachine->bNoHScroll && iLCDScan < 16)
         iLocalScrollX = 0;
      else
         iLocalScrollX = pMachine->iScrollX;
      pTile += (iLocalScrollX>>3); // add scroll offset
      usOldTile = -1;
      iScanOffset = (y&7); // offset to correct byte in tile data
      pSrc = (uint32_t *)&pMachine->pVRAM[iScanOffset<<2];
      pSrcFlipped = (uint32_t *)&pMachine->pVRAM[(7-iScanOffset)<<2];
      if (pMachine->iGameMode == MODE_SG1000) // different tile drawing
         {
         unsigned char uc, ucColor1, ucColor2, ucTile, *pTile8, *pEdge8, *p1, *p1Color;
         pTile8 = (unsigned char *)pTile;
         pEdge8 = pTile8 + 32;
         p1 = &pMachine->pVRAM[iSG1000Tile + iScanOffset];
         p1Color = &pMachine->pVRAM[iSG1000Color + iScanOffset];
         for (x=0; x<iWidth; x+=8)
            {
            if (x == 192 && pMachine->bWindow) // point at which 8 columns stop scrolling in "sms windowed" mode
               {
               pTile8 -= iWindowDelta;
               pEdge8 -= iWindowDelta; // right edge to scroll around
               y = iLCDScan;
               }
            if (pTile8 >= pEdge8)
               pTile8 -= 32; // wrap around back to the left side because of horizontal scrolling
            ucTile = *pTile8++;
            uc = p1[ucTile<<3];
            ucColor1 = p1Color[ucTile<<3];
            ucColor2 = ucColor1 & 0xf;
            ucColor1 >>= 4;
            // Draw the 8 pixels of the tile
            pDest[0] = (uc & 0x80) ? ucColor1 : ucColor2;
            pDest[1] = (uc & 0x40) ? ucColor1 : ucColor2;
            pDest[2] = (uc & 0x20) ? ucColor1 : ucColor2;
            pDest[3] = (uc & 0x10) ? ucColor1 : ucColor2;
            pDest[4] = (uc & 0x08) ? ucColor1 : ucColor2;
            pDest[5] = (uc & 0x04) ? ucColor1 : ucColor2;
            pDest[6] = (uc & 0x02) ? ucColor1 : ucColor2;
            pDest[7] = (uc & 0x01) ? ucColor1 : ucColor2;
            pDest += 8;
            }
         }
      else if (iWidth > 192 && pMachine->bWindow) // SMS needs to check for non-scrolling window area
         {
         for (x=0; x<iWidth; x+=8)
            {
            if (x == 192 && pMachine->bWindow) // point at which 8 columns stop scrolling in "sms windowed" mode
               {
               pTile -= iWindowDelta;
               pEdge -= iWindowDelta; // right edge to scroll around
               y = iLCDScan;
               }
            if (pTile >= pEdge)
               pTile -= 32; // wrap around back to the left side because of horizontal scrolling
            usTile = *pTile++;
            if (usTile != usOldTile) // if something changed, recalc
               {
               iTile = usTile & 0x1ff;
               ulColor = ((usTile & 0x1800) >> 7); // include background priority bit
               if (usTile & 0x400) // vertically flipped
                  ul = pSrcFlipped[iTile<<3];
               else
                  ul = pSrc[iTile<<3];
               // Draw the 8 pixels of the tile
               ulLeft = ulRight = (ulColor | (ulColor << 8) | (ulColor << 16) | (ulColor << 24));
               if (ul)
                  {
                  if (usTile & 0x200) // horizontal flip
                     {
                     ulLeft |= ulExpandMirror0[ul & 0xf];
                     ulRight |= ulExpandMirror0[(ul >> 4) & 0xf];
                     ulLeft |= ulExpandMirror1[(ul >> 8) & 0xf];
                     ulRight |= ulExpandMirror1[(ul >> 12) & 0xf];
                     ulLeft |= ulExpandMirror2[(ul >> 16) & 0xf];
                     ulRight |= ulExpandMirror2[(ul >> 20) & 0xf];
                     ulLeft |= ulExpandMirror3[(ul >> 24) & 0xf];
                     ulRight |= ulExpandMirror3[(ul >> 28) & 0xf];
                     }
                  else
                     {
                     ulLeft |= ulExpand0[(ul >> 4) & 0xf];
                     ulRight |= ulExpand0[ul & 0xf];
                     ulLeft |= ulExpand1[(ul >> 12) & 0xf];
                     ulRight |= ulExpand1[(ul >> 8) & 0xf];
                     ulLeft |= ulExpand2[(ul >> 20) & 0xf];
                     ulRight |= ulExpand2[(ul >> 16) & 0xf];
                     ulLeft |= ulExpand3[(ul >> 28) & 0xf];
                     ulRight |= ulExpand3[(ul >> 24) & 0xf];
                     }
                  }
               usOldTile = usTile; // for next time
               } // if tile changed
            *(uint32_t *)pDest = ulLeft;
            *(uint32_t *)&pDest[4] = ulRight;
            pDest += 8;
            }
         }
      else // GG or no window means we can do it quicker
         {
#ifdef _WIN32_WCE // use fast technique
         ARMDrawGGTiles(pTile, pEdge, pSrc, (uint32_t *)pDest,iWidth>>3);
#else
         for (x=(iWidth>>3); x>0; x--)
            {
            if (pTile >= pEdge)
               pTile -= 32; // wrap around back to the left side because of horizontal scrolling
            usTile = *pTile++;
            if (usTile != usOldTile) // if something changed, recalc
               {
               iTile = usTile & 0x1ff;
               ulColor = ((usTile & 0x1800) >> 7); // include background priority bit
               if (usTile & 0x400) // vertically flipped
                  ul = pSrcFlipped[iTile<<3];
               else
                  ul = pSrc[iTile<<3];
               // Draw the 8 pixels of the tile
               ulLeft = ulRight = (ulColor | (ulColor << 8) | (ulColor << 16) | (ulColor << 24));
               if (ul)
                  {
                  if (usTile & 0x200) // horizontal flip
                     {
                     ulLeft |= ulExpandMirror0[ul & 0xf];
                     ulRight |= ulExpandMirror0[(ul >> 4) & 0xf];
                     ulLeft |= ulExpandMirror1[(ul >> 8) & 0xf];
                     ulRight |= ulExpandMirror1[(ul >> 12) & 0xf];
                     ulLeft |= ulExpandMirror2[(ul >> 16) & 0xf];
                     ulRight |= ulExpandMirror2[(ul >> 20) & 0xf];
                     ulLeft |= ulExpandMirror3[(ul >> 24) & 0xf];
                     ulRight |= ulExpandMirror3[(ul >> 28) & 0xf];
                     }
                  else
                     {
                     ulLeft |= ulExpand0[(ul >> 4) & 0xf];
                     ulRight |= ulExpand0[ul & 0xf];
                     ulLeft |= ulExpand1[(ul >> 12) & 0xf];
                     ulRight |= ulExpand1[(ul >> 8) & 0xf];
                     ulLeft |= ulExpand2[(ul >> 20) & 0xf];
                     ulRight |= ulExpand2[(ul >> 16) & 0xf];
                     ulLeft |= ulExpand3[(ul >> 28) & 0xf];
                     ulRight |= ulExpand3[(ul >> 24) & 0xf];
                     }
                  }
               usOldTile = usTile; // for next time
               } // if tile changed
            *(uint32_t *)pDest = ulLeft;
            *(uint32_t *)&pDest[4] = ulRight;
            pDest += 8;
            }
#endif
         } // no window needed
      if (pMachine->ucSpriteDraw[iScanLine]) // sprites are visible on this line
         {
// Draw the sprites which appear on this scan line
         if (pMachine->iSpriteSize == 16) // 8x16 sprites
            {
            iTileMask = 0xfe;
            iHeight = 15;
            }
         else // 8x8 sprites
            {
            iTileMask = 0xff;
            iHeight = 7;
            }
         pSprite = &pMachine->pVRAM[pMachine->iSpriteBase];
         pSpriteList = &pMachine->ucSpriteList[pMachine->iSpriteCount-1];
         iCount = 0;
         if (pMachine->bZoomSprites) // stretched to 16x16
            {
            iHeight = (iHeight<<1)+1;
            while (pSpriteList >= pMachine->ucSpriteList) // draw front to back (priority bit prevents higher priority sprites from being overwritten)
               {
               y = pSprite[*pSpriteList];
               if (y == 208)// || iCount == 8) // end of list or max sprites draw on this line
                  break; // end of table marker
               y -= (pMachine->YOffset-1);
               if (y > 224)
                  y -= 256; // make y negative
               x = pSprite[128+(*pSpriteList<<1)] - pMachine->XOffset + (iLocalScrollX & 7);
               if (x > -8 && x < pMachine->iTrueWidth && y <= iLCDScan && (y + iHeight) >= iLCDScan) // need to draw this line
                  {
                  iCount++;
                  pDest = &ucTemp[8+x];
                  iTile = pSprite[129+(*pSpriteList<<1)] & iTileMask;
                  iScanOffset = 4*(((iLCDScan-y) & iHeight) >> 1);
                  pTileBits = (uint32_t *)&pMachine->pVRAM[pMachine->iSpritePattern + (iTile<<5)+iScanOffset];
               // Draw the 8 pixels of the sprite
                  ul = pTileBits[0];
                  ulLeft = ulExpand0[(ul >> 4) & 0xf]; // plane 0
                  ulRight = ulExpand0[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand1[(ul >> 4) & 0xf]; // plane 1
                  ulRight |= ulExpand1[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand2[(ul >> 4) & 0xf]; // plane 2
                  ulRight |= ulExpand2[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand3[(ul >> 4) & 0xf]; // plane 3
                  ulRight |= ulExpand3[ul & 0xf];
                  ulLeft += 0x30303030; // object colors start at 16 + priority bit
                  ulRight += 0x30303030;
                  if (ulLeft & 0x0f0f0f0f) // if the first 4 bytes have something to draw
                     {
                     if (ulLeft & 0xf)
                        {
                        c2 = pDest[0] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[0] = (char)ulLeft;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[1] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[1] = (char)ulLeft;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0x0f00)
                        {
                        c2 = pDest[2] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[2] = (char)(ulLeft >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[3] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[3] = (char)(ulLeft >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0x0f0000)
                        {
                        c2 = pDest[4] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[4] = (char)(ulLeft >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[5] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[5] = (char)(ulLeft >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0x0f000000)
                        {
                        c2 = pDest[6] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[6] = (char)(ulLeft >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[7] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[7] = (char)(ulLeft >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     }
                  if (ulRight & 0x0f0f0f0f) // if the second 4 bytes have something to draw
                     {
                     if (ulRight & 0xf)
                        {
                        c2 = pDest[8] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[8] = (char)ulRight;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[9] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[9] = (char)ulRight;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0x0f00)
                        {
                        c2 = pDest[10] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[10] = (char)(ulRight >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[11] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[11] = (char)(ulRight >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0x0f0000)
                        {
                        c2 = pDest[12] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[12] = (char)(ulRight >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[13] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[13] = (char)(ulRight >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0x0f000000)
                        {
                        c2 = pDest[14] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[14] = (char)(ulRight >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        c2 = pDest[15] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[15] = (char)(ulRight >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     }
                  } // sprite needs to be drawn
               pSpriteList--;
               } // for each of the active sprites
            }
         else // normal sized
            {
            while (pSpriteList >= pMachine->ucSpriteList) // draw front to back (priority bit prevents higher priority sprites from being overwritten)
               {
               y = pSprite[*pSpriteList];
               if (y == 208)// || iCount == 8) // end of list or max sprites draw on this line
                  break; // end of table marker
               y -= (pMachine->YOffset-1);
               if (y > 224)
                  y -= 256; // make y negative
               x = pSprite[128+(*pSpriteList<<1)] - pMachine->XOffset + (iLocalScrollX & 7);
               if (x > -8 && x < pMachine->iTrueWidth && y <= iLCDScan && (y + iHeight) >= iLCDScan) // need to draw this line
                  {
                  iCount++;
                  pDest = &ucTemp[8+x];
                  iTile = pSprite[129+(*pSpriteList<<1)] & iTileMask;
                  iScanOffset = 4*((iLCDScan-y) & iHeight);
                  pTileBits = (uint32_t *)&pMachine->pVRAM[pMachine->iSpritePattern + (iTile<<5)+iScanOffset];
               // Draw the 8 pixels of the sprite
                  ul = pTileBits[0];
                  ulLeft = ulExpand0[(ul >> 4) & 0xf]; // plane 0
                  ulRight = ulExpand0[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand1[(ul >> 4) & 0xf]; // plane 1
                  ulRight |= ulExpand1[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand2[(ul >> 4) & 0xf]; // plane 2
                  ulRight |= ulExpand2[ul & 0xf];
                  ul >>= 8;
                  ulLeft |= ulExpand3[(ul >> 4) & 0xf]; // plane 3
                  ulRight |= ulExpand3[ul & 0xf];
                  ulLeft += 0x30303030; // object colors start at 16 + priority bit
                  ulRight += 0x30303030;
                  if (ulLeft & 0x0f0f0f0f) // if the first 4 bytes have something to draw
                     {
                     if (ulLeft & 0xf)
                        {
                        c2 = pDest[0] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[0] = (char)ulLeft;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0xf00)
                        {
                        c2 = pDest[1] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[1] = (char)(ulLeft >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0xf0000)
                        {
                        c2 = pDest[2] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[2] = (char)(ulLeft >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulLeft & 0xf000000)
                        {
                        c2 = pDest[3] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[3] = (char)(ulLeft >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     }
                  if (ulRight & 0x0f0f0f0f) // if the second 4 bytes have something to draw
                     {
                     if (ulRight & 0xf)
                        {
                        c2 = pDest[4] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[4] = (char)ulRight;
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0xf00)
                        {
                        c2 = pDest[5] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[5] = (char)(ulRight >> 8);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0xf0000)
                        {
                        c2 = pDest[6] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[6] = (char)(ulRight >> 16);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     if (ulRight & 0xf000000)
                        {
                        c2 = pDest[7] & 0x2f;
                        if (c2 <= 0x20) // BG is not higher priority
                           pDest[7] = (char)(ulRight >> 24);
                        else
                           pMachine->bSpriteCollision = TRUE;
                        }
                     }
                  } // sprite needs to be drawn
               pSpriteList--;
               } // for each of the active sprites
            }
         } // if sprites are visible on this scanline
// Update the 2 palettes (16 colors each)
if (pMachine->ulPalChanged)
   {
   if (pMachine->iGameMode == MODE_GG)
      {
      for (i=0; i<32; i++)
         {
         if (pMachine->ulPalChanged & (1 << i))
            pMachine->usPalConvert[i+32] = pMachine->usPalConvert[i] = pColorConvert[pMachine->usPalData[i] & 0xfff]; // convert to RGB565
         }
      }
   else // SMS uses a simpler bbggrr palette
      {
      for (i=0; i<16; i++)
         {
         if (pMachine->ulPalChanged & (1 << i))
            {
            pMachine->usPalConvert[i*2+32] = pMachine->usPalConvert[i*2] = pColorConvert[pMachine->usPalData[i] & 0xff]; // convert to RGB565
            pMachine->usPalConvert[i*2+33] = pMachine->usPalConvert[i*2+1] = pColorConvert[pMachine->usPalData[i]>>8]; // convert to RGB565
            }
         }
      }
   pMachine->ulPalChanged = 0;
   }
scanlinedone:
// Convert the 8-bit pixels to 16-bits by translating through the palette
   iCount = pMachine->iTrueWidth; // number of pixels to convert to 16bpp
#ifdef FAST_DRAWING //_WIN32_WCE
   if (bSmartphone) // if smartphone, draw directly on the display
      {
      if (bGameGear) // GameGear fits right on the SP display
         pulDest = (uint32_t *)&pScreen[iScreenPitch*(iLCDScan+38) + 16];
      else
         pulDest = (uint32_t *)&pLocalBitmap[iPitch * iLCDScan];
      }
   else
      {
      if (!bGameGear) // if SMS on PPC, draw directly on the display
         {
         pulDest = (uint32_t *)&pScreen[iScreenPitch*(iLCDScan+38)];
         if (pMachine->bHideCol1)
            iCount = 248; // only draw 240 pixels
         else
            iCount = 240;
         }
      else
         pulDest = (uint32_t *)&pLocalBitmap[iPitch * iLCDScan];
      }
#else
   pulDest = (uint32_t *)&pLocalBitmap[iPitch * iLCDScan];
#endif // WINCE
   pDest = &ucTemp[8+(iLocalScrollX & 7)];
   x = 0;
   if (pMachine->bHideCol1)
      {
      x = 8; // skip the first 8 pixels
      pDest += 8;
      }
#ifdef _WIN32_WCE
   ARM816FAST(pDest, pulDest, pMachine->usPalConvert, (iCount-x)>>3);
   pulDest += ((iCount-x)>>1);
#else
   if (iLocalScrollX & 3) // need to do it byte by byte
      {
      for (i=x;i<iCount; i+=8)
         {
         pulDest[0] = pMachine->usPalConvert[pDest[0]] | (pMachine->usPalConvert[pDest[1]] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pMachine->usPalConvert[pDest[2]] | (pMachine->usPalConvert[pDest[3]] << 16);
         pulDest[2] = pMachine->usPalConvert[pDest[4]] | (pMachine->usPalConvert[pDest[5]] << 16);
         pulDest[3] = pMachine->usPalConvert[pDest[6]] | (pMachine->usPalConvert[pDest[7]] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
   else // can read as longs
      {
      for (i=x;i<iCount; i+=8)
         {
         ul = *(uint32_t *)&pDest[0];
         pulDest[0] = pMachine->usPalConvert[ul & 0xff] | (pMachine->usPalConvert[(ul >> 8) & 0xff] << 16); // unrolling the loop speeds it up considerably
         pulDest[1] = pMachine->usPalConvert[(ul >> 16) & 0xff] | (pMachine->usPalConvert[(ul >> 24) & 0xff] << 16);
         ul = *(uint32_t *)&pDest[4];
         pulDest[2] = pMachine->usPalConvert[ul & 0xff] | (pMachine->usPalConvert[(ul >> 8) & 0xff] << 16);
         pulDest[3] = pMachine->usPalConvert[(ul >> 16) & 0xff] | (pMachine->usPalConvert[(ul >> 24) & 0xff] << 16);
         pulDest += 4;
         pDest += 8;
         }
      }
#endif // _WIN32_WCE         
   if (pMachine->bHideCol1 && iCount == pMachine->iTrueWidth)
      {
      ul = pMachine->usPalConvert[pMachine->iBackColor];
      ul |= ul << 16;
      pulDest[0] = ul; // draw last 8 pixels as background color
      pulDest[1] = ul;
      pulDest[2] = ul;
      pulDest[3] = ul;
      }
} /* GGDrawScanline() */

void TRACEZ80(REGSZ80 *regs, int iClocks)
{
#ifdef FUTURE
static uint32_t ulTraceCount = 0;
unsigned short usTemp[8];
//unsigned char *p;
//uint32_t ul;

if (!bTrace)
   return;

ulTraceCount++;
if (ulTraceCount >= 0x4FD6E)
   usTemp[0] = 0;

/* Need less bulk, only save critical regs */
usTemp[0] = regs->usRegPC; /* Copy PC on top of DE */
usTemp[1] = regs->usRegAF & 0xffd7; // don't show unused bits
usTemp[2] = regs->usRegBC;
usTemp[3] = regs->usRegDE;
usTemp[4] = regs->usRegHL;
usTemp[5] = regs->usRegIX;
//ul = regs->usRegPC;
//ul = 0xc100;
//p = (unsigned char *)(ul + pMachine->regsz80.ulOffsets[ul>>12]);
usTemp[6] = pMachine->bSpriteCollision; //p[0]; //pMachine->mem_map[0xc300];
//usTemp[6] = regs->usRegIY;
//usTemp[6] = 0;
usTemp[7] = regs->usRegSP;
//EMUWrite(ohandle, usTemp, 16);
#endif // FUTURE
} /* TRACEZ80() */

unsigned int EMURand(void)
{
   return rand();

} /* EMURand() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGSetBank(short, char)                                     *
 *                                                                          *
 *  PURPOSE    : Handles writes to the ROM bank switch.                     *
 *                                                                          *
 ****************************************************************************/
void GGSetBank(int usAddr, unsigned char ucByte)
{
int i, j;
unsigned char ucPage;
unsigned long ul;

   pMachine->mem_map[usAddr & 0xdfff] = ucByte; // RAM is always written
   i = usAddr & 3;
   if (pMachine->ucBanks[i] == ucByte) // no change, leave
      return;
   pMachine->ucBanks[i] = ucByte;
   if (i == 0)
      {
      if (ucByte & 8)
         {
         if (pMachine->ucBanks[0] & 4) // which RAM bank
            ul = (unsigned long)&pMachine->mem_map[-0x4000]; // offset in RAM = 0x4000
         else
            ul = (unsigned long)&pMachine->mem_map[0];
         for (j=0x8; j<0xc; j++) // map in 4K pages
            pMachine->regsz80.ulOffsets[j] = ul;
         }
      else
         {
         ul = (unsigned long)&pMachine->pBankROM[(pMachine->ucBanks[3] & pMachine->iROMSizeMask)*0x4000 - 0x8000];
         for (j=0x8; j<0xc; j++) // map in 4K pages
            pMachine->regsz80.ulOffsets[j] = ul;
         }
      }
// DEBUG - this never seems to be used
   if (i == 1) // change ROM bank 0
      {
//      ucPage = pMachine->ucBanks[1] & pMachine->iROMSizeMask;
//      for (j=1; j<0x10; j++) // map in 1k pages
//         pMachine->regsz80.ulOffsets[j] = (unsigned long)&pMachine->pBankROM[ucPage * 0x4000];
      }
   if (i == 2) // change ROM bank 1
      {
      ucPage = pMachine->ucBanks[2] & pMachine->iROMSizeMask;
      for (j=0x4; j<0x8; j++) // map in 4k pages
         pMachine->regsz80.ulOffsets[j] = (unsigned long)&pMachine->pBankROM[ucPage*0x4000 - 0x4000];
      }
   if (i == 3) // change ROM/RAM bank 2
      {
      if (!(pMachine->ucBanks[0] & 0x08)) // map ROM
         {
         ucPage = pMachine->ucBanks[3] & pMachine->iROMSizeMask;
         for (j=0x8; j<0xc; j++) // map in 4K pages
            pMachine->regsz80.ulOffsets[j] = (unsigned long)&pMachine->pBankROM[ucPage*0x4000 - 0x8000];
         }
      }
} /* GGSetBank() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGBankWrite(short, char)                                   *
 *                                                                          *
 *  PURPOSE    : Write a byte to memory, check for hardware.                *
 *                                                                          *
 ****************************************************************************/
void GGBankWrite(int usAddr, unsigned char ucByte)
{

//   if (usAddr >= 0xc000)
//      {
//      usAddr &= 0xdfff;
//      pMachine->mem_map[usAddr] = ucByte;
//      }
//   else
//      {
//      if (usAddr >= 0x8000)
//         {
         if (pMachine->ucBanks[0] & 8) // cartridge RAM
            {
            if (pMachine->ucBanks[0] & 4)
               usAddr -= 0x4000;
            pMachine->mem_map[usAddr] = ucByte;
            }
         else
            {
            if (usAddr == 0x8000) // SMS CodeMasters games have a mapper at 0x8000 which sets bank 2 (0x8000-0xc000)
               {
               int i;
               pMachine->ucBanks[3] = ucByte & (pMachine->iROMPages-1);
               for (i=0x8; i<0xc; i++)
                  pMachine->regsz80.ulOffsets[i] = (unsigned long)&pMachine->pBankROM[(ucByte & (pMachine->iROMPages-1))*0x4000 - 0x8000];
               }
            }
//         }
//      }

} /* GGBankWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGReset(GG_MACHINE *)                                      *
 *                                                                          *
 *  PURPOSE    : Reset the state of the emulator.                           *
 *                                                                          *
 ****************************************************************************/
void GGReset(GG_MACHINE *ggTemp)
{
int i, j;

    ggTemp->iVRAMAddr = 0;
    ggTemp->bSecondByte = FALSE;
    ggTemp->iSpriteSize = 8;
//    ggTemp->bEarlyClock = FALSE;
    ggTemp->bLineIntEnable = FALSE;
    ggTemp->bWindow = FALSE;
    ggTemp->bVBlankIntEnable = FALSE;
    ggTemp->bVideoEnable = FALSE;
    ggTemp->bZoomSprites = FALSE;
    ggTemp->bHideCol1 = FALSE;
    ggTemp->bNoHScroll = FALSE;
    ggTemp->ucBanks[0] = 0;
//    ggTemp->ucBanks[0] = 0x8;
    ggTemp->ucBanks[1] = 0;
    ggTemp->ucBanks[2] = 1;
    ggTemp->ucBanks[3] = 2; // initial page numbers
    ggTemp->iBackColor = 16; // "0" of second palette

    ARESETZ80(&ggTemp->regsz80, 0xff);
    ggTemp->regsz80.usRegSP = 0xff00;
    ggTemp->regsz80.ucIRQVal = 0xff;

    if (bColeco)
       {
       // Set up the palette
       for (i=0; i<16; i++)
          {
          ggTemp->usPalData[i] = i*16+3;
          }
       // BIOS occupies the first 8K
       ggTemp->regsz80.ulOffsets[0] = (unsigned long)ggTemp->pBankROM;
       ggTemp->regsz80.ulOffsets[1] = (unsigned long)ggTemp->pBankROM;
       for (i=0x2; i<0x8; i++) // unused areas
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)ggTemp->mem_map;
       // Cartridge ROM takes the top 32K
       j = (ggTemp->iROMSize >> 12)-1; // number of 4k pages -1 (for mask)
       for (i=0x8; i<0x10; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)&ggTemp->pBankROM[0x2000 - (i*0x1000) + ((i&j)*0x1000)];
       ggTemp->emuh[0].pfn_read = ColecoIORead;
       ggTemp->emuh[0].pfn_write = ColecoIOWrite;
       // 1K RAM repeated 8x from 6000-7FFF
       ggTemp->emuh[1].pfn_read = ColecoBankRead;
       ggTemp->emuh[1].pfn_write = ColecoBankWrite;

       memset(&ggTemp->mem_map[0x10000], 0xbf, 0x10000); // mark everything as ROM
       for (i=0x6000; i<0x8000; i++) // mirrored RAM
          ggTemp->mem_map[0x10000 + i] = 0xc1;
       }
    else // SMS/GG
       {
       // Memory is mapped in 1K pages (because of 1k stuck at page 0 in bank 0
       // map bottom pages by default
       for (i=0; i<0x4; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)ggTemp->pBankROM;
       for (i=0x4; i<0x8; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)ggTemp->pBankROM;
 
   //    for (i=0x20; i<0x30; i++) // map in 1K pages
   //        ggTemp->regsz80.ulOffsets[i] = (unsigned long)&ggTemp->mem_map[0];
       for (i=0x8; i<0xc; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)ggTemp->pBankROM;
       for (i=0xc; i<0xe; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)ggTemp->mem_map; // 0xc000-0xdfff
       for (i=0xe; i<0x10; i++)
          ggTemp->regsz80.ulOffsets[i] = (unsigned long)&ggTemp->mem_map[-0x2000]; // 0xc000-0xdfff mirror

       ggTemp->emuh[0].pfn_read = GGIORead;
       ggTemp->emuh[0].pfn_write = GGIOWrite;
       ggTemp->emuh[1].pfn_read = NULL;
       ggTemp->emuh[1].pfn_write = GGBankWrite;
       ggTemp->emuh[2].pfn_read = NULL;
       ggTemp->emuh[2].pfn_write = GGSetBank;

       memset(&ggTemp->mem_map[0x10000], 0xbf, 0x8000); // mark first 32K as ROM
       for (i=0x8000; i<0xc000/*0xfffc*/; i++) // cart ram, mirrored ram, bank switch
          ggTemp->mem_map[0x10000 + i] = 0x81;
       for (i=0xfffc; i<0x10000; i++)
          ggTemp->mem_map[0x10000 + i] = 0x82; // setbank routine
       }      
    ggTemp->iNameTable = 0x3800;
    ggTemp->iSpriteBase = 0x3f00;
    ggTemp->iSpritePattern = 0x2000;
    ggTemp->iLineInt = 1; // count for scan line interrupts
    ggTemp->ucSoundChannels = 0xff; // turn sound on

    ggTemp->iScrollX = ggTemp->XOffset;
    ggTemp->iScrollY = ggTemp->YOffset;

//    mymemcpy(&ggTemp->mem_map[0xff30], ucInitWave, 16); // initial sound wave table values

    memset(ggTemp->pVRAM, 0, 0x4100);
    memset(&ggTemp->mem_map[0x4000], 0, 0x8000); // cartridge RAM
    if (ggTemp->iGameMode == MODE_SG1000)
       ggTemp->ulPalChanged = 0; // uses a fixed palette
    else
       ggTemp->ulPalChanged = -1; // force all colors to reset to 0

    ggTemp->InQ->iHead = ggTemp->InQ->iTail = 0; // reset serial I/O queues
    ggTemp->OutQ->iHead = ggTemp->OutQ->iTail = 0;
    memset(ggTemp->InQ->cBuffer, 0, 0x800);
    memset(ggTemp->OutQ->cBuffer, 0, 0x800);
    ggTemp->ucIRQs = 0;

} /* GGReset() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGUpdateButtons(ULONG)                                     *
 *                                                                          *
 *  PURPOSE    : Update the gamegear button bits.                           *
 *                                                                          *
 ****************************************************************************/
void GGUpdateButtons(uint32_t ulKeys)
{
unsigned char c;

   if (bColeco) // buttons mapped differently for Colecovision
      {
      c = 0x7f;
      if (ulKeys & RKEY_LEFT_P1)
         c ^= 1;
      if (ulKeys & RKEY_RIGHT_P1)
         c ^= 2;
      if (ulKeys & RKEY_UP_P1)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P1)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P1)
         c ^= 0x10;
      if (ulKeys & RKEY_BUTTB_P1)
         c ^= 0x20;
      pMachine->ucJoyButtons[0] = c;
      c = 0x7f;
      if (ulKeys & RKEY_START_P1)
         c ^= 0x1;
      if (ulKeys & RKEY_SELECT_P1)
         c ^= 0x2;
      pMachine->ucJoyButtons[1] = c;
      c = 0x7f;
      if (ulKeys & RKEY_LEFT_P2)
         c ^= 1;
      if (ulKeys & RKEY_RIGHT_P2)
         c ^= 2;
      if (ulKeys & RKEY_UP_P2)
         c ^= 4;
      if (ulKeys & RKEY_DOWN_P2)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P2)
         c ^= 0x10;
      if (ulKeys & RKEY_BUTTB_P2)
         c ^= 0x20;
      pMachine->ucJoyButtons[2] = c;
      c = 0x7f;
      if (ulKeys & RKEY_START_P2)
         c ^= 0x1;
      if (ulKeys & RKEY_SELECT_P2)
         c ^= 0x2;
      pMachine->ucJoyButtons[3] = c;
      return;
      }
   c = 0xc0;
   if (iPlayer == 0)
      {
      if (ulKeys & RKEY_START_P1)// && !(ulOldKeys & RKEY_START_P1))
         c ^= 0x80;
      if (ulKeys & RKEY_SELECT_P1 && !(pMachine->ulOldKeys[iPlayer] & RKEY_SELECT_P1)) // pause button?
         pMachine->ucIRQs |= INT_NMI; // pause button generates an NMI
      }
   else // allow p2 inputs to start as well
      {
      if (ulKeys & RKEY_START_P2)
         c ^= 0x80;
      if (ulKeys & RKEY_SELECT_P2 && !(pMachine->ulOldKeys[iPlayer] & RKEY_SELECT_P2)) // pause button?
         pMachine->ucIRQs |= INT_NMI; // pause button generates an NMI
      }
   pMachine->ucJoyButtons[2] = c;


   if (iPlayer == 1) // map P2 controls onto P1
      {
      c = 0xff;
      // Player 1 (taken from P2 inputs)
      if (ulKeys & RKEY_UP_P2)
         c ^= 1;
      if (ulKeys & RKEY_DOWN_P2)
         c ^= 2;
      if (ulKeys & RKEY_LEFT_P2)
         c ^= 4;
      if (ulKeys & RKEY_RIGHT_P2)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P2)
         c ^= 0x10;
      if (ulKeys & RKEY_BUTTB_P2)
         c ^= 0x20;
      pMachine->ucJoyButtons[0] = c;
      pMachine->ucJoyButtons[1] = 0xff; // no P2 input
      }
   else
      {
      c = 0xff;
      // Player 1
      if (ulKeys & RKEY_UP_P1)
         c ^= 1;
      if (ulKeys & RKEY_DOWN_P1)
         c ^= 2;
      if (ulKeys & RKEY_LEFT_P1)
         c ^= 4;
      if (ulKeys & RKEY_RIGHT_P1)
         c ^= 8;
      if (ulKeys & RKEY_BUTTA_P1)
         c ^= 0x10;
      if (ulKeys & RKEY_BUTTB_P1)
         c ^= 0x20;
      if (bHead2Head && pMachine->iGameMode == MODE_GG) // P1 input only for each GameGear
         {
         pMachine->ucJoyButtons[0] = c;
         pMachine->ucJoyButtons[1] = 0xff;
         }
      else // allow 2 player simultaneous for Master System games
         {
         // Player 2
         if (ulKeys & RKEY_UP_P2)
            c ^= 0x40;
         if (ulKeys & RKEY_DOWN_P2)
            c ^= 0x80;
         pMachine->ucJoyButtons[0] = c;
         c = 0xff;
         if (ulKeys & RKEY_LEFT_P2)
            c ^= 1;
         if (ulKeys & RKEY_RIGHT_P2)
            c ^= 2;
         if (ulKeys & RKEY_BUTTA_P2)
            c ^= 4;
         if (ulKeys & RKEY_BUTTB_P2)
            c ^= 8;
         pMachine->ucJoyButtons[1] = c;
         }
      }
      
   pMachine->ulOldKeys[iPlayer] = ulKeys;

} /* GGUpdateButtons() */

uint32_t Reflect(uint32_t ref, char ch) 
{// Used only by Init_CRC32_Table(). 
uint32_t value = 0;
int i;

      // Swap bit 0 for bit 7 
      // bit 1 for bit 6, etc. 
      for(i=1; i < (ch + 1); i++) 
      { 
            if(ref & 1) 
                  value |= 1 << (ch - i); 
            ref >>= 1; 
      } 
      return value; 
}  /* Reflect() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : Init_CRC32_Table(void)                                     *
 *                                                                          *
 *  PURPOSE    : Call this function only once to initialize the CRC table.  *
 *                                                                          *
 ****************************************************************************/
void Init_CRC32_Table(void)
{
int i, j;

      // This is the official polynomial used by CRC-32 
      // in PKZip, WinZip and Ethernet. 
      uint32_t ulPolynomial = 0x04c11db7; 
      memset(crc32_table,0,256*sizeof(int));

      // 256 values representing ASCII character codes. 
      for(i = 0; i <= 0xFF; i++) 
      { 
            crc32_table[i]=Reflect(i, 8) << 24; 
            for (j = 0; j < 8; j++) 
                  crc32_table[i] = (crc32_table[i] << 1) ^ (crc32_table[i] & (1 << 31) ? ulPolynomial : 0); 
            crc32_table[i] = Reflect(crc32_table[i], 32); 
      } 
}  /* Init_CRC32_Table() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SN76496GenSamplesGG(int, SN76496 *, char *, int, int)      *
 *                                                                          *
 *  PURPOSE    : Generate n 8-bit sound samples for the specified chip.     *
 *                                                                          *
 ****************************************************************************/
void SN76496GenSamplesGG(int iChip, SN76_GG *pPSG, signed short *psBuf, int iLen, unsigned char ucChannels, int iSoundChannels, BOOL b16Bit, EMU_EVENT_QUEUE *eeq)
{
int i, iEvent;
int32_t ulClock = 0;
unsigned char c;
signed int cLeft, cRight;
SN76_GG *pChip;
unsigned char *pBuf;
static int iDelta;
static uint32_t ulOldClock = 0;

   pBuf = (unsigned char *)psBuf;
   pChip = &pPSG[iChip];

   if (iSoundChannels == 1)
      ucChannels = (ucChannels & 0xf) | (ucChannels >> 4); // combine left and right into mono
   if (b16Bit) iLen >>= 1; // 16-bit samples

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
      if (ulOldClock != ulClock) // try to avoid doing a divide
         {
         ulOldClock = ulClock;
         iDelta = ulClock / (iLen/iSoundChannels);
         }
          ulClock -= iDelta; // start out at -1 delta so we don't have left over events
      }
   for (i=0; i<iLen; i+=iSoundChannels)
      {
      if (eeq) // if there is an event queue, play back all events preceding this sample
         {
         while (ulClock <= (int32_t)(eeq->ulTime[iEvent]<<8) && iEvent < eeq->iCount)
            {
                WriteSN76_GG(iChip, pPSG, eeq->ucEvent[iEvent++]);
                /* Determine noise period */
                c = pChip->ucRegs[SN_N_MODE] & 3;
                if (c == 3) /* Use tone generator 3 */
                    pChip->iPeriodN = pChip->iPeriodC * 2;
                else
                    pChip->iPeriodN = pChip->iDivisor >> (9+c);
            }
         ulClock -= iDelta;
         }
      cLeft = cRight = 0;
      if (pChip->cVolA)
         {
         pChip->iCountA += pChip->iPeriodA;
         if (pChip->iCountA & 0x4000)
            {
            if (ucChannels & 1)
               cRight += pChip->cVolA;
            if (ucChannels & 0x10)
               cLeft += pChip->cVolA;
            }
         else
            {
            if (ucChannels & 1)
               cRight -= pChip->cVolA;
            if (ucChannels & 0x10)
               cLeft -= pChip->cVolA;
            }
         }
      if (pChip->cVolB)
         {
         pChip->iCountB += pChip->iPeriodB;
         if (pChip->iCountB & 0x4000)
            {
            if (ucChannels & 2)
               cRight += pChip->cVolB;
            if (ucChannels & 0x20)
               cLeft += pChip->cVolB;
            }
         else
            {
            if (ucChannels & 2)
               cRight -= pChip->cVolB;
            if (ucChannels & 0x20)
               cLeft -= pChip->cVolB;
            }
         }
      if (pChip->cVolC)
         {
         pChip->iCountC += pChip->iPeriodC;
         if (pChip->iCountC & 0x4000)
            {
            if (ucChannels & 4)
               cRight += pChip->cVolC;
            if (ucChannels & 0x40)
               cLeft += pChip->cVolC;
            }
         else
            {
            if (ucChannels & 4)
               cRight -= pChip->cVolC;
            if (ucChannels & 0x40)
               cLeft -= pChip->cVolC;
            }
         }
      if (pChip->cVolN)
         {
         pChip->iCountN += pChip->iPeriodN;
         if (pChip->iCountN & 0x4000)
            {
            if (pChip->iPRNG & 1)
               pChip->iPRNG ^= pChip->iNoiseBits;
            pChip->iPRNG >>= 1;
            if (pChip->iPRNG & 1) // if output bit active
               pChip->iNoise ^= 1; /* Toggles from 0 to 1 */
            }
         if (pChip->iNoise) /* If noise bit is active now */
            {
            if (ucChannels & 8)
               cRight += pChip->cVolN;
            if (ucChannels & 0x80)
               cLeft += pChip->cVolN;
            }
         else
            {
            if (ucChannels & 8)
               cRight -= pChip->cVolN;
            if (ucChannels & 0x80)
               cLeft -= pChip->cVolN;
            }
         }

      if (b16BitAudio)
         {
             cRight <<= 6;
//             if (cRight < -32768) cRight = -32768;
//             if (cRight > 32767) cRight = 32767;
             psBuf[i] = (signed short)cRight;
             if (iSoundChannels == 2)
             {
                 cLeft <<= 6;
//                 if (cLeft < -32768) cLeft = -32768;
//                 if (cLeft > 32767) cLeft = 32767;
                 psBuf[i+1] = (signed short)cLeft;
             }
         }
      else
         {
         pBuf[i] = 0x80 + (signed char)(cRight>>1);
         if (iSoundChannels == 2)
            pBuf[i+1] = 0x80 + (signed char)(cLeft>>1);
         }
      }
    while (eeq && iEvent < eeq->iCount) // finish any left over events
    {
        WriteSN76_GG(iChip, pPSG, eeq->ucEvent[iEvent++]);
    }
} /* SN76496GenSamplesGG() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GGCalcCRC(char *, int)                                     *
 *                                                                          *
 *  PURPOSE    : Calculate the 32-bit CRC for the data block.               *
 *                                                                          *
 ****************************************************************************/
uint32_t GGCalcCRC(unsigned char *pData, int iLen)
{
int i;
uint32_t ulCRC;

   ulCRC = 0;
   for (i=0; i<iLen; i++)
      ulCRC = (ulCRC >> 8) ^ crc32_table[(ulCRC & 0xFF) ^ *pData++]; 

   return ulCRC;

} /* GGCalcCRC() */

void GGProcess1Scan(BOOL bVideo)
{
      if (iScanLine <= 192)
         {
         if (iScanLine == 0)
            pMachine->iLineCount = pMachine->iLineInt; // reset line count interrupt
         if (pMachine->iLineCount <= 0)
            {
            pMachine->bLineInt = TRUE;
            if (pMachine->bLineIntEnable)
               pMachine->ucIRQs |= INT_IRQ; // generate an interrupt for this line count
            pMachine->iLineCount = pMachine->iLineInt; // reset line counter
            }
         else
            pMachine->iLineCount--;
         if (iScanLine >= pMachine->YOffset && iScanLine < pMachine->iBottom && bVideo) // 144 or 224 scan lines in the middle of the effective area
            GGDrawScanline();
         } // scanlines 0-192
      else
         { // scanlines 193-261
         if (iScanLine == 193)
            pMachine->bVBlank = TRUE;
         pMachine->iLineCount = pMachine->iLineInt; // reset line count interrupt
         if (iScanLine == 194 && pMachine->bVBlank)// && bVBlankIntEnable)
            pMachine->ucIRQs |= INT_IRQ; // generate an interrupt
         }
// 3.58Mhz = 228 total clocks per scanline x 262 effective scanlines (144 visible)
      iCPUTicks = 236;
      AEXECZ80(pMachine->mem_map, &pMachine->regsz80, pMachine->emuh, &iCPUTicks, &pMachine->ucIRQs);
   // Check for serial communication interrupt
      if (pMachine->InQ->iHead != pMachine->InQ->iTail && (pMachine->ucPort5 & 8) && pMachine->bAllowNMI) // interrupt enabled and data waiting
         {
         pMachine->ucIRQs |= INT_NMI;
//            iStat1++; // count number of NMIs generated
         pMachine->bAllowNMI = FALSE;
         }
} /* GGProcess1Scan() */

void GGProcess1Frame(GAME_BLOB *pBlob, uint32_t ulKeys, BOOL bSound, BOOL bVideo)
{
   iTotalClocks = 262*236;
   pEEQ->iCount = 0;
   GGUpdateButtons(ulKeys);
   GGProcessSprites();
   if (bHead2Head && pMachine->iGameMode == MODE_GG)
      {
      iPlayer = 1;
      pMachine = &ggmachine[iPlayer];
      GGProcessSprites();
      GGUpdateButtons(ulKeys);
      iPlayer = 0;
      pMachine = &ggmachine[iPlayer];
      }
      for (iScanLine=0; iScanLine<262; iScanLine++) // total display is 262 scan lines
         {
         iTotalClocks -= 236;
         if (bSlave) // only need to draw it, not process anything
            {
            if (iScanLine >= pMachine->YOffset && iScanLine < pMachine->iBottom && bVideo) // 144 or 224 scan lines in the middle of the effective area
               GGDrawScanline();
            }
         else
            {
            GGProcess1Scan(bVideo);
            }
         if (bHead2Head && pMachine->iGameMode == MODE_GG)
            {
//            if (iDisplayWidth > iDisplayHeight) // make it double wide
            	pLocalBitmap += 160*2; // use right half
//            else
//               pBitmap += (144 * iPitch); // point to second display
            iPlayer = 1;
            pMachine = &ggmachine[iPlayer];
            GGProcess1Scan(bVideo);
            iPlayer = 0;      
            pMachine = &ggmachine[iPlayer];
//            if (iDisplayWidth > iDisplayHeight) // make it double wide
               pLocalBitmap -= 160*2; // back to left half
//            else
//               pBitmap -= (144 * iPitch); // point to first display
            }
         } // for each scanline
#ifdef _WIN32_WCE
//      if (pMachine->OutQ->iHead != pMachine->OutQ->iTail)
//         GGNetUpdate(); // send serial data if available
#endif
      if (bSound && iPlayer == 0) // only player 1 gets sound output
         {
         SN76496GenSamplesGG(0, pPSG, pBlob->pAudioBuf, pBlob->iAudioBlockSize, pMachine->ucSoundChannels, pBlob->iSoundChannels, pBlob->b16BitAudio, pEEQ);
         }
      
} /* GGProcess1Frame() */

void GGCreateVideoBuffer(void)
{
    if (bHead2Head && pMachine->iGameMode == MODE_GG)
       {
       if (iDisplayWidth > iDisplayHeight) // make it double wide
          {
          iScreenX = 160*2;
          iScreenY = 144;
          }
       else // make it double high
          {
          iScreenX = 160;
          iScreenY = 144*2; // double height to fit the 2 games
          }
       }
//    EMUCreateVideoBuffer(iScreenX, iScreenY, 16, &pBitmap);
    iVideoSize = -1; // force a recalc of the display size
    
} /* GGCreateVideoBuffer() */

void GG_PostLoad(GAME_BLOB *pBlob)
{
GG_MACHINE *pNew, *pOld;
int i;
unsigned char ucPage;

   pNew = (GG_MACHINE *)pBlob->pMachine;
   pOld = (GG_MACHINE *)pBlob->OriginalMachine;

   pNew->ulPalChanged = -1; // force a palette update
   pNew->pVRAM = pOld->pVRAM;
   pNew->pBankROM = pOld->pBankROM;
   pNew->mem_map = pOld->mem_map;
   pNew->usPalData = pOld->usPalData;
   pNew->InQ = pOld->InQ;
   pNew->OutQ = pOld->OutQ;
   for (i=0; i<4; i++)
      pNew->emuh[i] = pOld->emuh[i];
      
   // Reset the memory areas
   for (i=0; i<4; i++)
      pNew->regsz80.ulOffsets[i] = (unsigned long)pNew->pBankROM; // ROM mapped to the bottom 16k
   ucPage = pNew->ucBanks[2] & pNew->iROMSizeMask;
   for (i=0x4; i<0x8; i++)
      pNew->regsz80.ulOffsets[i] = (unsigned long)&pNew->pBankROM[ucPage * 0x4000 - 0x4000];
   if (pNew->ucBanks[0] & 8) // RAM bank
      {
      unsigned long ul;
      if (pNew->ucBanks[0] & 4) // which RAM bank
         ul = (unsigned long)&pNew->mem_map[-0x4000]; // offset in RAM = 0x4000
      else
         ul = (unsigned long)&pNew->mem_map[0];
      for (i=0x8; i<0xc; i++) // map in 4K pages
         pNew->regsz80.ulOffsets[i] = ul;
      }
   else
      {
      ucPage = pNew->ucBanks[3] & pNew->iROMSizeMask;
      for (i=0x8; i<0xc; i++)
         pNew->regsz80.ulOffsets[i] = (unsigned long)&pNew->pBankROM[ucPage * 0x4000 - 0x8000];
      }
   for (i=0xc; i<0xe; i++)
      pNew->regsz80.ulOffsets[i] = (unsigned long)pNew->mem_map; // 0xc000-0xdfff
   for (i=0xe; i<0x10; i++)
      pNew->regsz80.ulOffsets[i] = (unsigned long)&pNew->mem_map[-0x2000]; // 0xc000-0xdfff mirror

} /* GG_PostLoad() */

int GG_Init(GAME_BLOB *pBlob, char *pszROM, int iGameLoad)
{
int i;
int iError = SG_NO_ERROR;
GG_MACHINE *pGGTemp; // for thread-safe inits

   if (pszROM == NULL)
      {
      if (iGameLoad == -1) // reset machine
         {
         GGReset((GG_MACHINE *)pBlob->pMachine);
         }
      else // load a game state
         {
         if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
            GG_PostLoad(pBlob);
         }
      return SG_NO_ERROR;
      }

//   pGGTemp = EMUAlloc(sizeof(GG_MACHINE));
//   if (pGGTemp == NULL)
//      return SG_OUTOFMEMORY;
   pGGTemp = &ggmachine[0];
   pBlob->pMachine = (void *)pGGTemp;
   pBlob->OriginalMachine = (void*)&ggmachine[2]; // spare copy
   pBlob->iGameType = SYS_GAMEGEAR;

    pGGTemp->iROMSize = EMULoadGGRoms(pszROM, &pGGTemp->pBankROM, &pGGTemp->iGameMode);
//     __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "returned from EMULoadGGRoms");
    if (pGGTemp->iROMSize == 0)
       {
       EMUFree(pGGTemp);
       return SG_OUTOFMEMORY;
       }
    i = pGGTemp->pBankROM[0x2000] + pGGTemp->pBankROM[0x2001];
    bColeco = (i == 0xff); // first 2 bytes of colecovision roms = 55AA or AA55
    Init_CRC32_Table();
    ulCRC = GGCalcCRC(pGGTemp->pBankROM, pGGTemp->iROMSize);
    pGGTemp->iROMSizeMask = (pGGTemp->iROMSize/0x4000)-1; // ROM page size
    pGGTemp->iROMPages = pGGTemp->iROMSize / 0x4000;

    if (iGGRefCount == 0) // first time through, allocate common stuff
       {
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "refcount == 0");
       pPSG = EMUAlloc(sizeof(SN76_GG));
       if (pPSG == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gg_init_leave;
          }
       ARM_InitSN76496(0, pPSG, 3580000, pBlob->iAudioSampleRate);
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "returned from InitSN76496, iAudioSampleRate=%d", pBlob->iAudioSampleRate);
       pEEQ = EMUAlloc(sizeof(EMU_EVENT_QUEUE));
       if (pEEQ == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gg_init_leave;
          }
       pEEQ->iFrameTicks = 61832; // (3.71Mhz / 60) = 262*236
       pColorConvert = EMUAlloc(0x2000);
       if (pColorConvert == NULL)
          {
          iError = SG_OUTOFMEMORY;
          goto gg_init_leave;
          }
       // Create a lookup table to convert BGR444 to RGB565
       if (pGGTemp->iGameMode == MODE_GG)
          {
          for (i=0; i<4096; i++)
             {
             unsigned char r, g, b;
             r = ((i & 0xf) << 1) | (i & 1); // 4->5 bits of red
             g = ((i & 0xf0) >> 2) | ((i & 0x30)>>4); // 4->6 bits of green
             b = ((i & 0xf00) >> 7) | ((i & 0x100)>>8);  // 4->5 bits of blue
             pColorConvert[i] = (r << 11) | (g << 5) | b;
             }
          }
       else
          {
          for (i=0; i<256; i++)
             {
             unsigned char r, g, b;
             r = ((i & 0x3) << 3) | ((i & 3) << 1) | (i & 1); // 2->5 bits of red
             g = ((i & 0xc) << 2) | (i & 0xc) | ((i & 0xc) >>2); // 2->6 bits of green
             b = ((i & 0x30) >> 1) | ((i & 0x30)>>3) | ((i & 0x10) >> 4);  // 2->5 bits of blue
             pColorConvert[i] = (r << 11) | (g << 5) | b;
             }
          }
       }
    memcpy(&ggmachine[1], &ggmachine[0], sizeof(GG_MACHINE)); // ready for head2head play
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "finished initializing palette");
    pGGTemp->mem_map = EMUAlloc(0x20000); /* Allocate 64k for the GB memory map + 64K for flags */
    if (pGGTemp->mem_map == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gg_init_leave;
       }
    pGGTemp->pVRAM = EMUAlloc(0x5000);
    if (pGGTemp->pVRAM == NULL)
       {
       iError = SG_OUTOFMEMORY;
       goto gg_init_leave;
       }
    pGGTemp->usPalData = (unsigned short *)&pGGTemp->pVRAM[0x4000];
    pGGTemp->InQ = InQ;
    pGGTemp->OutQ = OutQ;
    ggmachine[1].InQ = OutQ; // swap them
    ggmachine[1].OutQ = InQ;
    if (pBlob->bHead2Head && pGGTemp->iGameMode == MODE_GG)
       {
       ggmachine[1].mem_map = EMUAlloc(0x20000); /* Allocate 64k for the GB memory map + 64K for flags */
       ggmachine[1].pVRAM = EMUAlloc(0x5000);
       ggmachine[1].usPalData = (unsigned short *)&ggmachine[1].pVRAM[0x4000];
       }

//    pMachine->mem_map[0xff70] = 1; // start at RAM bank 1
       
    if (pGGTemp->iGameMode == MODE_GG)
       {
       if (bHead2Head)
       {
    	   pBlob->iWidth = 160*2;
       }
       else
       {
    	   pBlob->iWidth = 160;
       }
	   pBlob->iHeight = 144;
       pGGTemp->iTrueWidth = ggmachine[1].iTrueWidth = iScreenX = 160;
       pGGTemp->XOffset = ggmachine[1].XOffset = 48;
       pGGTemp->YOffset = ggmachine[1].YOffset = 24;
       pGGTemp->iBottom = ggmachine[1].iBottom = 168;
       //       iScreenY = 144;
       }
    else
       {
       pBlob->iWidth = 256;
       pBlob->iHeight = 192;
       pGGTemp->iTrueWidth = iScreenX = 256;
       pGGTemp->XOffset = 0;
       pGGTemp->YOffset = 0;
       pGGTemp->iBottom = 192;
       //       iScreenY = 192;
       }
    iOldWidth = iDisplayWidth; // keep track in case it changes (open keyboard)
//    GGCreateVideoBuffer();
    i=0;
    while (szNameList[i] && ulCRCList[i] != ulCRC)
       i++;
//    if (szNameList[i])
//       strcpy((char *)pBlob->szGameName, (const char *)szNameList[i]);
//    else
        SG_GetLeafName((char *)pszROM, (char *)pBlob->szGameName); // derive the game name from the ROM name
//       strcpy((char *)pBlob->szGameName, "GameGear"); // unknown game

    // Set up memory areas for load/save and networking
    // Area 0 = Video RAM
    pBlob->mem_areas[0].iAreaLength = 0x5000;
    pBlob->mem_areas[0].pPrimaryArea = pGGTemp->pVRAM;
    // Area 1 = machine state
    pBlob->mem_areas[1].iAreaLength = sizeof(GG_MACHINE);
    pBlob->mem_areas[1].pPrimaryArea = (unsigned char *)pGGTemp;
    // Area 2 = PSG
    pBlob->mem_areas[2].iAreaLength = sizeof(SN76_GG);
    pBlob->mem_areas[2].pPrimaryArea = (unsigned char *)pPSG;
    // Area 3 = main memory
    pBlob->mem_areas[3].iAreaLength = 0x10000; // top 64k is flags that don't change
    pBlob->mem_areas[3].pPrimaryArea = pGGTemp->mem_map;
    pBlob->mem_areas[4].iAreaLength = 0; // terminate the list
// Prepare for 2 player if we are a slave
    if (bSlave || bMaster) // both slave and master need this defined properly
       {
       SGPrepMemAreas(pBlob);
       }
//       __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "about to call GGReset()");
    GGReset(pGGTemp); // Reset the state of the gamegear emulator
    if (pBlob->bHead2Head && pGGTemp->iGameMode == MODE_GG)
       {
       GGReset(&ggmachine[1]); // setup player 2
       }
           
    memcpy(pBlob->OriginalMachine, pBlob->pMachine, sizeof(ggmachine[0])); // keep a copy of the original machine pointers to allow for game state loading
    if (iGameLoad >= 0)
       {
       if (!SGLoadGame(pBlob->szGameName, pBlob, iGameLoad))
          GG_PostLoad(pBlob);
       }
               
    for (i=0; i<64; i++)
      {
      unsigned short us5[4] = {0,7,15,31};
      unsigned short us6[4] = {0,15,31,63};
      unsigned short r, g, b;
      r = i & 3;
      g = (i >> 2) & 3;
      b = (i >> 4) & 3;
      pGGTemp->usPalConvert[i] = (us5[r]<<11) | (us6[g]<<5) | (us5[b]); 
      if (pBlob->bHead2Head && pGGTemp->iGameMode == MODE_GG)
         ggmachine[1].usPalConvert[i] = (us5[r]<<11) | (us6[g]<<5) | (us5[b]);
      }
   pBlob->bLoaded = TRUE;
   iGGRefCount++;

//   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "about to leave gg_init()");
gg_init_leave:
   if (iError != SG_NO_ERROR)
      {
      EMUFree(pGGTemp->pBankROM);
      EMUFree(pGGTemp->mem_map);
      EMUFree(pGGTemp->pVRAM);
      if (bHead2Head)
      {
    	  EMUFree(ggmachine[1].mem_map);
    	  EMUFree(ggmachine[1].pVRAM);
      }
//      EMUFree(pGGTemp); // free the whole machine structure
      if (iGGRefCount == 0) // last one, free common resources
         {
         EMUFree(pPSG);
         EMUFree(pColorConvert);
         EMUFree(pEEQ);
         }      
      }
   return iError;
} /* GG_Init() */

void GG_Terminate(GAME_BLOB *pBlob)
{
   if (iGGRefCount == 0)
      return; // already freed
   pBlob->bLoaded = FALSE;
   pMachine = (GG_MACHINE *)pBlob->pMachine;
   EMUFree(pMachine->pBankROM);
   EMUFree(pMachine->mem_map);
   EMUFree(pMachine->pVRAM);
//   EMUFree(pMachine); // free the whole machine structure
   if (bHead2Head)
   {
	   EMUFree(ggmachine[1].mem_map);
	   EMUFree(ggmachine[1].pVRAM);
   }
   if (iGGRefCount == 1) // last one, free common resources
      {
      EMUFree(pPSG);
      EMUFree(pColorConvert);
      EMUFree(pEEQ);
      }
   iGGRefCount--;
} /* GG_Terminate() */

void GG_Play(GAME_BLOB *pBlob, BOOL bSound, BOOL bVideo, uint32_t ulKeys)
{
unsigned char *p;
int i;

   pMachine = (GG_MACHINE *)pBlob->pMachine;
   pLocalBitmap = (unsigned char *)pBlob->pBitmap;
   iPitch = pBlob->iPitch;

    if (pBlob->bRewound)
	{
		pBlob->bRewound = FALSE;
		GG_PostLoad(pBlob);
	}

   for (i=0; i<iCheatCount; i++)
   {
	   p = (unsigned char *)(pMachine->regsz80.ulOffsets[iaCheatAddr[i] >> 12] + iaCheatAddr[i]);
	   if (iaCheatCompare[i] != -1)
	   {
		   if (p[0] == (unsigned char)iaCheatCompare[i])
			   p[0] = (unsigned char)iaCheatValue[i];
	   }
	   else
		   p[0] = (unsigned char)iaCheatValue[i];
   }
   
   GGProcess1Frame(pBlob, ulKeys, bSound, bVideo);
//   if (bMaster)
//      {
//      SGCompressMemAreas(); // send changes to slave
//      }

} /* GG_Play() */

