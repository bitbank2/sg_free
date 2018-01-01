//
// Common functions for the game emulators
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
//#include <gtk/gtk.h>

#ifdef _WIN32_WCE
#include <windows.h>
#include <tchar.h>
#else
#include "my_windows.h"
#endif

#include "smartgear.h"
#include "emu.h"
#include "emuio.h"
#include  <zlib.h>
#include "unzip.h"

extern char *szGameFourCC[];
extern char pszSAVED[], pszGame[], pszHighScores[], pszScreenShots[], pszDIPs[];
extern unsigned char ucISOHeader[];
extern int iGameType;
//extern HWND hwndClient;
extern uint32_t crc32_table[];
extern unsigned char ucMirror[];
extern GAMEDEF gameList[];
void SG_SaveGameImage(int iGame);
void SG_GetLeafName(char *fname, char *leaf);
int SG_MatchCoinOp(char *fname);
extern void Init_CRC32_Table(void);
int bLCDFlip, iLCDX, iLCDY;
BOOL bAudio = TRUE;
BOOL bHead2Head = FALSE;
BOOL bFramebuffer = FALSE;
BOOL bStretch = FALSE;
#define LCD_ILI9341 1
#define LCD_HX8357 2
#define LCD_ST7735 3

int iLCDType, iDisplayType, iDispOffX, iDispOffY, iSPIChan, iSPIFreq, iDC, iReset, iLED, bVerbose;
int iGamma; // LCD gamma table choice
int iP1Control, iP2Control;
int iButtonPins[CTRL_COUNT];
int iButtonKeys[CTRL_COUNT];
int iButtonGP[CTRL_COUNT];
int iButtonGPMap[16]; // map gamepad buttons back to SmartGear events
int iButtonMapping[CTRL_COUNT] = {RKEY_UP_P1, RKEY_DOWN_P1, RKEY_LEFT_P1, RKEY_RIGHT_P1,
    RKEY_EXIT, RKEY_START_P1, RKEY_SELECT_P1, RKEY_BUTT1_P1,
    RKEY_BUTT2_P1, RKEY_BUTT3_P1, RKEY_BUTT4_P1};

typedef struct {
    uint32_t ulCRC;
    char *name;
} pce_name_list;

pce_name_list PCENameList[] = {
	{0x888687c1, "1943 Kai"},
   {0x6c7f3ed1, "Adventure Island (J)"},
   {0xfebe9326, "Aero Blasters (U)"},
   {0xbf14a284, "After Burner II (J)"},
   {0xbcd55772, "Air Zonk (U)"},
   {0x08466eb6, "Alien Crush (U)"},
   {0xdce6ea45, "Andre Panza Kick Boxing (U)"},
   {0x28c2ecd9, "Ankoko Densetsu (J)"},
   {0x2cd1ace8, "Aoi (Blue) Blink (J)"},
   {0xa8715451, "Atomic Robokid Special (J)"},
   {0xcd00d82e, "Av Poker World Gambler (J)"},
   {0x68c12288, "Ballistix (J)"},
   {0xe616d52f, "Bari Bari Densetsu (J)"},
   {0x3f5bf966, "Barunba (J)"},
   {0x341a80c0, "Batman (J)"},
   {0xbbeaa567, "Battle Lode Runner (J)"},
   {0xc411fa31, "Be Ball (J)"},
   {0xed135d31, "Benkei Gaiden (J) [a1]"},
   {0x5ed8e3ae, "Blazing Lazers (U)"},
   {0xeb6300c4, "Blodia (J)"},
   {0x023994d0, "Bloody Wolf (U)"},
   {0x8abf2ef4, "Body Conquest 2 (J)"},
	{0xae9c1011, "Bomberman '93 (J)"},
	{0x237116b0, "Bomberman '93 (U)"},
	{0xa20ecf0a, "Bomberman '94 (J)"},
	{0x2b705ba0, "Bomberman (J)"},
   {0x9fe06c90, "Bonk 3 - Bonk's Big Adventure (U)"},
	{0x5745b27d, "Bonk's Adventure (U)"},
	{0x61430536, "Bonk's Revenge (U)"},
   {0x1eb32fde, "Boxy Boy (U)"},
   {0xb9c681ae, "Bravoman (U)"},
   {0x78106b95, "Bubblegum Crash (J)"},
   {0x783c2ee3, "Bullfight Ring no Haja (J)"},
   {0xf642f728, "Burning Angels (J)"},
   {0xa9b1ef2d, "Cadash (J)"},
   {0xbaad3d98, "Champion Wrestler (J)"},
   {0x60888236, "Champions Forever Boxing (U)"},
   {0x4b3ee0e2, "Chase HQ (J)"},
   {0x6edfd4b8, "Chew-Man-Fu (U)"},
   {0x40e0dc3f, "China Warrior (U)"},
   {0x4bc2abce, "Chouzetsu Rinjin (J) [b4]"},
   {0x7893763c, "Chouzetsu Rinjin (J)"},
   {0xdd6a322d, "City Hunter (J)"},
   {0xf57c580b, "College Pro Baseball '90 (J)"},
   {0xe71f68bf, "Columns (J)"},
   {0xc1b49497, "Coryoon Child of Dragon (J)"},
   {0x723d0218, "Cratermaze (U)"},
   {0x589f717c, "Cross Wiber - Cyber Combat Police (J)"},
   {0x8dfc1018, "Cyber Core (J)"},
   {0xf4b367b8, "Cyber Cross (J)"},
   {0xc05461ba, "Cyber Dodge (J)"},
   {0xd0f2f06c, "Cyber Knight (J)"},
   {0x45d3a42d, "Daichikun Crisis Do Natural (J)"},
   {0x01c1991d, "Darius Alpha (J) [b1]"},
   {0x94cb5fed, "Darius Alpha (J)"},
   {0xd92bf149, "Darkwing Duck (U)"},
   {0x3aece0ba, "Davis Cup Tennis (U)"},
   {0x2315916b, "Dead Moon (J)"},
   {0xe734e5a1, "Deep Blue (J)"},
   {0x202692b6, "Deep Blue (U) [h1]"},
   {0x2993972c, "Detana!! Twinbee (J)"},
   {0x6ab92df2, "Devil Crash (J)"},
   {0x6e3d161d, "Die Hard (J)"},
   {0xf5b4da10, "Digital Champ (J)"},
   {0xd05b904c, "Don Doko Don (J)"},
   {0x3e78e025, "Doraemon Meikyu Daisakusen (J)"},
   {0x745c7ed3, "Doraemon Nobita no Dorabian Night (J)"},
   {0x6406911b, "Double Dungeons (J)"},
   {0x9ae83762, "Double Ring (J)"},
   {0xe14f9964, "Download (J) [a1]"},
   {0xf076168c, "Download (J)"},
   {0x31420f79, "Dragon Egg! (J)"},
   {0x0cb83761, "Dragon Fighter (J)"},
   {0x477f8e30, "Dragon Saber (J)"},
   {0xea61feae, "Dragon Spirit (U)"},
   {0xfab2426f, "Dragon's Curse (U)"},
   {0x3f6bb7d0, "Dungeon Explorer (J)"},
   {0x28661503, "Energy (J)"},
   {0x95fc275f, "Eternal City Toshi Tenso Keikaku (J)"},
   {0xa2a9dda3, "F1 Circus '91 - World Championship (J)"},
   {0x363b2a8b, "F1 Circus '92 - The Speed of Sound (J)"},
   {0x942be4a4, "F1 Circus (J)"},
   {0x37ce337b, "F1 Triple Battle (J)"},
   {0x37011d12, "F-1 Dream (J)"},
   {0x2d75b606, "F-1 Pilot (J)"},
   {0x58e0f38c, "Falcon (U)"},
	{0x3eaca054, "Fantasy Zone (U)"},
   {0x6d4ed849, "Fighting Run (J)"},
   {0xed7846c8, "Final Blaster (J)"},
   {0xecb1b391, "Final Lap Twin (J)"},
   {0xc2e16ddf, "Formation Armed F (J)"},
   {0x67af0d94, "Formation Soccer Human Cup '90 (J)"},
   {0x383d5e45, "Fractal Test Demo Vx.x (PD)"},
   {0x36b5d18f, "Fushigi ni Yumi no Alice (J)"},
	{0xf88d79e4, "Galaga '88 (J)"},
	{0xcb0734e4, "Galaga '90 (U)"},
   {0xd8230750, "Genji Tsushin Agedama (J)"},
   {0xcc40cc2e, "Genpei Toumaden (J)"},
   {0xf2f57f20, "Genpei Toumaden Volume 2 (J)"},
   {0x58d2cb51, "Ghost Manor (U)"},
   {0xc338955e, "Gokuraku Chuka Taisen (J)"},
   {0x5d1768f9, "Gomola Speed (J) [o1]"},
   {0x6fa2b865, "Gomola Speed (J)"},
   {0xe7193047, "Gradius (J)"},
   {0x690aeeb1, "Gunboat (U)"},
   {0x6b5f48d7, "Gunhed (J) [b1]"},
   {0xb5ff698c, "Gunhed Taikai (J)"},
   {0xcf2b0778, "Hana Tahka Daka (J)"},
   {0x5d30c6e5, "Hani in the Sky (J)"},
   {0x3a0f129e, "Hatris (J)"},
   {0xbfcb7362, "Heavy Unit (J) [o1]"},
   {0xcfe30a97, "Heavy Unit (J)"},
   {0x38da2994, "Hisou Kihei Serd (J)"},
   {0xac84eb1c, "Hit the Ice (U) [!]"},
   {0xc578edaf, "Hono no Tataka Tamako Dodge Danpei (J)"},
   {0xeba0f6c0, "Idol Hanafuda Fan Club (J)"},
   {0xdd68a182, "Image Fight (J) [b1]"},
   {0xb1131faf, "Impossamole (U)"},
   {0x0212bb05, "JJ & Jeff (U)"},
   {0x82672fc5, "Kato Chan & Ken Chan (J)"},
   {0xa5439050, "Keith Courage in Alpha Zones (U)"},
   {0xed15b396, "Klax (U)"},
   {0x773ea667, "Legend of Hero Tonma (U)"},
   {0xc00055b3, "Legendary Axe II, The (U)"},
   {0xc7895f21, "Liquid Kids (J)"},
   {0x04e0fe4a, "Lode Runner - Ushina Wareta Maikyuu (J)"},
   {0xa868b520, "Magical Chase (J)"},
   {0xdeddd25b, "Magical Chase (U) [h1]"},
   {0xb7822185, "Military Madness (U)"},
   {0x099ab8d2, "Moto Roader (U) [h1]"},
   {0x6684cda8, "Neutopia (U)"},
   {0xba4814b6, "Neutopia II (U)"},
   {0x8f86f6cc, "Order of the Griffon (U)"},
   {0x92d920d8, "Ordyne (U)"},
   {0x9765f88f, "OutRun (J)"},
   {0x272597f6, "Pac-Land (J) [b1]"},
   {0xc5375305, "Pac-Land (U)"},
   {0x41afe0e9, "Parasol Stars (J) [b1]"},
   {0xc234b560, "Parasol Stars (U)"},
   {0x7d5a9fc6, "Populous (J)"},
   {0x015bf1db, "Raiden (J)"},
   {0x24b2b91b, "Rastan Saga II (J)"},
   {0x20f45e4c, "R-Type (U) [h1]"},
   {0x1009068c, "R-Type II (J)"},
   {0x98146b81, "Shinobi (J)"},
   {0x3397d6bd, "Sidearms (U)"},
   {0x39cc6145, "Silent Debuggers, The (U)"},
    {0x8f1857ca, "The Silent Debuggers"},
   {0x3ed081bf, "Soldier Blade (U)"},
   {0x359cf7d0, "Son Son II (J)"},
   {0x113e0e8b, "Space Harrier (J)"},
   {0xa56aade3, "Splatterhouse (U)"},
   {0x9071ac74, "Stratego (J)"},
   {0xb49a39ad, "Street Fighter II Champion Edition (J)"},
   {0x51ae2412, "Talespin (U)"},
   {0x6e4b0adb, "Terra Cresta II (J)"},
   {0x3b115f34, "Time Cruise (U) [b1]"},
   {0x07860168, "Tower of Druaga, The (J)"},
   {0x75cb04b8, "Toy Shop Boys (J)"},
   {0xdf3e397a, "Tricky Kick (U)"},
   {0x60602d9e, "Turrican (U)"},
   {0xd65633d4, "Valkyrie no Densetsu (J)"},
   {0xbda078c5, "Veigues Tactical Gladiator (U)"},
   {0x5da5ad7f, "Vigilante (U)"},
   {0x3fb25c44, "Violent Soldier (J)"},
   {0x89535842, "Volfied (J)"},
   {0x2563e7b5, "Wallaby!! (J)"},
   {0x7950567a, "Winning Shot (J)"},
   {0xbbde9936, "Wonder Momo (J)"},
   {0x7a3b0e9b, "Wonderboy in Monsterland (J) [b2]"},
   {0x1af6b4ce, "Xevious (J)"},
   {0xe67d6ab3, "Yo, Bro (U)"},
   {0x133f5d24, "Youkai Douchuuki (J)"},
   {0x229eb68b, "Yuu Yuu Jinsei (J)"},
   {0xc2192483, "Zero 4 Champ (J)"},
   {0x85a45d83, "Zipang (J)"},
   {0xd8e4c5ad, "Tricky (J)"},
   {0xa6df0c64, "Tora E no Michi (J)"},
   {0x3001d20d, "Titan (J)"},
   {0x26d172e7, "Toilet Kids (J)"},
   {0xc319ad44, "Timeball (U)"},
   {0xbc2c7e5f, "Tiger Road (U)"},
   {0xa8a5e2a5, "Thunder Blade (J)"},
   {0x0a56bbb9, "Tenseiryu Saint Dragon (J)"},
   {0x3534f507, "Tatsujin (J) [b1]"},
   {0x98c52739, "Talespin (U) [h1]"},
   {0x122c5431, "Takeda Shingen (J)"},
   {0x0bdbfdb5, "Takin' It to the Hoop (U)"},
   {0xba15d250, "Susano Oh Densetsu (J)"},
   {0xc65eaa91, "Super Volley Ball (U)"},
   {0xae4f42c3, "Super Star Soldier (U)"},
   {0xb4466114, "Super Metal Crusher (J)"},
   {0xd04f077c, "Spiral Wave (J)"},
   {0x7b478791, "Space Invaders Fukkatsu no Hi (J)"},
   {0xaf5d76bd, "Skweek (J)"},
   {0x6b1f1acf, "Sinistron (U)"},
   {0x52124c80, "Shockman (U)"},
   {0x0ba76460, "Rock On (J)"},
   {0xfc460a94, "Rabio Lepus Special (J)"},
   {0x5c858419, "Power Gate (J)"},
   {0x74527f91, "Puzznic (J)"},
   {0xee0052a7, "Populous (J) [o1]"},
   {0x9d1627fd, "Photograph Boy (J)"},
   {0x594ac394, "Paranoia (J)"},
   {0x943c31b2, "P-47 - The Freedom Fighter (J)"},
   {0x55402f40, "Override (J)"},
   {0xb291faef, "Ninja Warriors, The (J)"},
   {0xabecfb6d, "Ninja Spirit (U)"},
   {0x12313100, "Ninja Gaiden (J)"},
   {0xf7b8f342, "Niko Niko Pun (J)"},
   {0xaa3c42da, "New Zealand Story, The (J)"},
   {0x000c12ae, "New Adventure Island (U)"},
   {0x8e69d652, "Necros no Yousai (J)"},
   {0x59df283c, "Mr. Heli no Dai Bouken (J)"},
   {0x2f0e592d, "Moto Roader II (J)"},
   {0xeb5e7937, "Kyuukyoku Tiger (J)"},
   {0x575c2324, "Kung Fu, The (J)"},
   {0x83b6ea27, "Knight Rider Special (J)"},
   {0xcd20c862, "King of Casino (U)"},
   {0xbce6cdf4, "Hani on the Road (J)"},
   {0xb1883a9e, "Gai Flame (J)"},
   {0x310a73e0, "Devil's Crush (U)"},
   {0xfe611e7b, "Spin Pair (J)"},
   {0x18a80ba5, "Puzzle Boy (J)"},
   {0x08c4fa27, "Pc Genjin (J)"},
   {0x01629b6e, "Pc Denjin - Punkic Cyborgs (J)"},
   {0xda4bd803, "Final Soldier (J)"},
   {0x73e8634d, "Ryukyu (J)"},
   {0x1a154af9, "Power Eleven (J)"},
   {0x119c6639, "Salamander (J) [b3]"},
   {0x5554e432, "Salamander (J) [b1]"},
   {0x18e22402, "Salamander (J)"},
   {0xc34ff2e5, "Parodius (J)"},
   {0xc96c0f31, "Power Golf (U)"},
   {0xf889500d, "Power Tennis (J)"},
   {0xf33da152, "Victory Run (U)"},
   {0xf6871073, "Bomberman Users Battle (J) [!]"},
   {0xbd61d608, "Bomberman (U)"},
   {0x85df1009, "Soukoban World (J)"},
   {0x1540b491, "Sonic Spike (U)"},
   {0x8ecfe206, "Psychosis (U)"},
   {0xb43f7cb7, "Night Creatures (U)"},
   {0x2c3db886, "Nectaris (J) [o1]"},
   {0x26327249, "Nectaris (J)"},
   {0x5c6c04d7, "Makai Hakkenden Shada (J)"},
   {0x5086fc45, "Power Drift (J)"},
   {0xba4814b6, "Neutopia II (U)"},
   {0x00000000,NULL},
};

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadRoms16(HWND, LOADROM *, char *, char *)             *
 *                                                                          *
 *  PURPOSE    : Load the ROM images into our 16-bit memory map.            *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadRoms16(LOADROM *roms, char *szDir, char *mem_map)
{
int i, j, rc;
unzFile zHandle;
char *d, *pTemp;

    pTemp = EMUAlloc(0x800000); // largest size of any single ROM file
    i = 0;
    zHandle = unzOpen((const char *)szDir);
    if (zHandle == NULL)
       return TRUE;
    while (roms[i].szROMName) /* Load ROMs from the ZIP file directly */
       {
       rc = unzLocateFile(zHandle, roms[i].szROMName, 2);
       if (rc == UNZ_END_OF_LIST_OF_FILE) /* Report the file not found */
          {
          return TRUE;
          }
       rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
       rc = unzReadCurrentFile(zHandle, pTemp, roms[i].iROMLen);
       if (roms[i].iCheckSum) // it's a 16-bit ROM, read even and odd bytes together
          {
          d = &mem_map[roms[i].iROMStart];
//          memcpy(d, pTemp, roms[i].iROMLen);
// byte swap for faster access
          for (j=0; j<roms[i].iROMLen; j+=2)
             {
             d[j] = pTemp[j+1];
             d[j+1] = pTemp[j];
             }
          }
       else
          { // even/odd ROM
//          d = &mem_map[roms[i].iROMStart];
          d = &mem_map[roms[i].iROMStart ^ 1];
          for (j=0; j<roms[i].iROMLen; j++) // copy the ROM in even or odd locations
             {
//             d[j*2] = pTemp[j];
             d[j*2] = pTemp[j];
             }
          }
       rc = unzCloseCurrentFile(zHandle);
       i++;
       }
    unzClose(zHandle);
    EMUFree(pTemp);
    return FALSE; /* we are done */

} /* EMULoadRoms16() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadRoms(LOADROM *, char *, int *, char *, EMUHANDLERS) *
 *                                                                          *
 *  PURPOSE    : Load the ROM images into our memory map.                   *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadRoms(LOADROM *roms, char *szName, int *iNumHandlers, EMUHANDLERS *emuh, unsigned char *mem_map, BOOL bSetFlags)
{
int i, rc;
unzFile zHandle;

    i = 0;
    if (iNumHandlers == NULL) /* Loading character ROMS */
       {
/* First, try to read them from the ZIP file */
        zHandle = unzOpen((char *)szName);
       if (zHandle == NULL)
          return TRUE; // error
       while (roms[i].szROMName) /* Load ROMs from the ZIP file directly */
          {
          rc = unzLocateFile(zHandle, roms[i].szROMName, 2);
          if (rc == UNZ_END_OF_LIST_OF_FILE) /* Report the file not found */
             {
//              __android_log_print(ANDROID_LOG_VERBOSE, "EMULoadRoms", "file not found: %s", roms[i].szROMName);
        	 unzClose(zHandle);
             return TRUE;
             }
          rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
          rc = unzReadCurrentFile(zHandle, (char *)&mem_map[roms[i].iROMStart + MEM_ROMRAM], roms[i].iROMLen);
          rc = unzCloseCurrentFile(zHandle);
          if (bSetFlags)
          {
        	  memset(&mem_map[roms[i].iROMStart + MEM_FLAGS], 0xbf, roms[i].iROMLen);
          }
          i++;
          }
       unzClose(zHandle);
       return FALSE; /* we are done */
       }
    return TRUE; // unimplemented handler version

} /* EMULoadRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : GENLoadRoms(char *, char *, char *)                        *
 *                                                                          *
 *  PURPOSE    : Load the ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
int GENLoadRoms(char *szROM, unsigned char *mem_map, unsigned char **pBank)
{
int i, rc, iLen;
void * iHandle;
unsigned char c, *p;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];
BOOL bSMD = FALSE;

   iLen = 0;
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)szROM);
#endif
      if (zHandle == NULL)
         return iLen; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.BIN file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".bin") == 0 || stricmp(&szTemp[i-4],".gen") == 0) // it's a BIN or GEN file, load it
            goto found_file;
         if (stricmp(&szTemp[i-4], ".smd") == 0 || stricmp(&szTemp[i-3], ".md") == 0) // it's a SMD file, load it
            {
            bSMD = TRUE;
            goto found_file;
            }
         rc = unzGoToNextFile(zHandle);
         }
found_file:
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
            *pBank = EMUAlloc(iLen+0x200);
            if (*pBank == NULL) // out of memory
               {
               unzClose(zHandle);
               return 0;
               }
            unzReadCurrentFile(zHandle, *pBank, iLen); /* Read the file into memory */
//            if (bSMD)
            rc = unzCloseCurrentFile(zHandle);
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return 0;
         }
      i = (int)strlen((char *)szROM);
      if (stricmp((char *)&szROM[i-4], ".smd") == 0 || stricmp((char *)&szROM[i-3], ".md") == 0) // it's a SMD file
         bSMD = TRUE;
      iLen = EMUSeek(iHandle, 0, 2);
      EMUSeek(iHandle, 0, 0); // seek to start of ROM data
      *pBank = EMUAlloc(iLen+0x200);
      if (*pBank == NULL) // out of memory
         {
         EMUClose(iHandle);
         return 0;
         }
      EMURead(iHandle, *pBank, iLen); // read the whole ROM image into the banked ROM area
//      if (bSMD)
      EMUClose(iHandle);
      }
   if (bSMD && (iLen & 0x200)) // need to interlace the ROM image and move the header
      {
      unsigned char *pTemp, *pSrc, *pDest;
      int x, iBlocks;
      pTemp = EMUAlloc(iLen);
      iBlocks = iLen / 16384; // number of blocks to decode
      pSrc = *pBank + 512;
      pDest = pTemp;
      for (i=0; i<iBlocks; i++)
         {
         for (x=0; x<8192; x++)
            {
            pDest[x*2+1] = pSrc[x];
            pDest[x*2] = pSrc[8192+x];
            }
         pSrc += 16384;
         pDest += 16384;
         }
      memcpy(*pBank, pTemp, iLen);
      EMUFree(pTemp);
      }
   // Swap even and odd bytes to make big-endian words into little-endian words for better 68k performance
   p = *pBank;
   for (i=0; i<iLen; i+=2)
      {
      c = p[i];
      p[i] = p[i+1];
      p[i+1] = c;
      }
   return iLen;

} /* GENLoadRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PCECalcCRC(char *, int)                                    *
 *                                                                          *
 *  PURPOSE    : Calculate the 32-bit CRC for the data block.               *
 *                                                                          *
 ****************************************************************************/
uint32_t PCECalcCRC(unsigned char *pData, int iLen)
{
int i;
uint32_t ulCRC;

    Init_CRC32_Table();
   ulCRC = 0;
   for (i=0; i<iLen; i++)
      ulCRC = (ulCRC >> 8) ^ crc32_table[(ulCRC & 0xFF) ^ *pData++]; 

   return ulCRC;

} /* PCECalcCRC() */

int BIN2ISO(unsigned char *pData, int iLen)
{
   int bFound, pos, i, j, iNewLen;
   bFound = pos = 0;
   iNewLen = -1;
   for (i=0; i<iLen && !bFound; i++)
   {
      if (pData[i] == ucISOHeader[0]) // see if it's the ISO header
      {
         j = 1;
         while (j < 0x800 && pData[i+j] == ucISOHeader[j]) { j++;}
         if (j == 0x800) // we found it !
         {
             bFound = 1;
             pos = i;
         }
      }
   }
   if (bFound)
   {
         unsigned char *pTemp;
         int iNumSectors;
         // repack the data and allocate a new buffer
//         pos += 2352; // don't copy the ISO header
         iNumSectors = (iLen - pos) / 2352;
         pTemp = EMUAlloc(iNumSectors * 2048);
         for (i=0; i<iNumSectors; i++)
         {
               memcpy(&pTemp[i*2048], &pData[pos+(i*2352)], 2048); // toss the 304 bytes of 'extra' data
         }
         iNewLen = iNumSectors * 2048;
         memcpy(pData, pTemp, iNewLen);
         EMUFree(pTemp);
   }
return iNewLen;
} /* BIN2ISO() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SG_LoadISO(char *, unsigned int *)                         *
 *                                                                          *
 *  PURPOSE    : Load an ISO image into a buffer.                           *
 *                                                                          *
 ****************************************************************************/
unsigned char * SG_LoadISO(char *szISO, unsigned int *piSize)
{
int i, rc, iLen;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];
unsigned char *pTemp = NULL;
int bISO = 1; // assume ISO image

   iLen = 0;
   i = (int)strlen((char *)szISO);
   if (stricmp((char *)&szISO[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
      zHandle = unzOpen((char *)szISO);
      if (zHandle == NULL)
         return NULL; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.iso file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".iso") == 0) // it's a ISO image, load it
            break;
         if (stricmp(&szTemp[i-4], ".bin") == 0) // it's a BIN, load it with special care
         {
               bISO = 0;
               break;
         }
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            if (bISO) // just read the raw track data
                  {
                  iLen = (int)unzInfo.uncompressed_size;
                  pTemp = EMUAlloc(iLen);
                  *piSize = (unsigned int)iLen;
                  unzReadCurrentFile(zHandle, pTemp, iLen); /* Read the file into memory */
                  rc = unzCloseCurrentFile(zHandle);
                  }
            else // must be a BIN file
                  {
                        int iNewLen;
                  iLen = (int)unzInfo.uncompressed_size;
                  pTemp = EMUAlloc(iLen);
                  unzReadCurrentFile(zHandle, pTemp, iLen); /* Read the file into memory */
                  rc = unzCloseCurrentFile(zHandle);
                  // Need to search for the track data
                  iNewLen = BIN2ISO(pTemp, iLen);
                  if (iNewLen != -1)
                      *piSize = (unsigned int)iNewLen;
                  }
            }
         }
      unzClose(zHandle);
      }
      else  if (stricmp((char *)&szISO[i-4], ".iso") == 0) // it's an uncompressed ISO
      {
            void *iHandle;
            iHandle = EMUOpen(szISO);
            if (iHandle != (void*)-1)
            {
                  iLen = EMUSeek(iHandle, 0, 2);
                  EMUSeek(iHandle, 0, 0);
                  *piSize = (unsigned int)iLen;
                  pTemp = EMUAlloc(iLen);
                  EMURead(iHandle, pTemp, iLen); /* Read the file into memory */
                  EMUClose(iHandle);
            }
      }
      else if (stricmp((char *)&szISO[i-4],".bin") == 0) // it's a BIN file
      {
            void *iHandle;
            iHandle = EMUOpen(szISO);
            if (iHandle != (void*)-1)
            {
                  int iNewLen;

                  iLen = EMUSeek(iHandle, 0, 2);
                  EMUSeek(iHandle, 0, 0);
                  pTemp = EMUAlloc(iLen);
                  EMURead(iHandle, pTemp, iLen); /* Read the file into memory */
                  EMUClose(iHandle);
                  // Need to search for the track data
                  iNewLen = BIN2ISO(pTemp, iLen);
                  if (iNewLen != -1)
                      *piSize = (unsigned int)iNewLen;
            }            
      }
      return pTemp;
} /* SG_LoadIOS() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : PCELoadRoms(char *, char *)                                *
 *                                                                          *
 *  PURPOSE    : Load the PC-Engine ROM images.                             *
 *                                                                          *
 ****************************************************************************/
BOOL PCELoadRoms(char *szROM, unsigned char *mem_map, char *szGameName)
{
int i, rc, iLen;
void * iHandle;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];
unsigned char *pTemp;
uint32_t ulCRC;

   iLen = 0;
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)szROM);
#endif
      if (zHandle == NULL)
         return TRUE; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.PCE file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".pce") == 0) // it's a PCE ROM, load it
            break;
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
            pTemp = EMUAlloc(iLen);
            if (iLen & 0x200) // has a 512-byte header
               {
               unzReadCurrentFile(zHandle, pTemp, 0x200); // skip the header
               iLen -= 0x200;
               }
            unzReadCurrentFile(zHandle, pTemp, iLen); /* Read the file into memory */
            rc = unzCloseCurrentFile(zHandle);
            goto check_list;
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      if (iHandle == (void*)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      pTemp = EMUAlloc(iLen);
      if (iLen & 0x200) // there is a 512 byte header
         {
         iLen -= 0x200;
         EMUSeek(iHandle, 0x200, 0); // seek to start of ROM data
         }
      else
         EMUSeek(iHandle, 0, 0); // start at the beginning of the file
      EMURead(iHandle, pTemp, iLen); // read the entire ROM into the temp buffer
      EMUClose(iHandle);
      goto check_list;
      }
   return TRUE;

check_list: // see if the data needs to be modified to load properly
   ulCRC = PCECalcCRC(pTemp, iLen);
// find the name of this game
   i = 0;
//   strcpy((char *)szGameName, "Turbo Grafx"); // name for unknown games
   SG_GetLeafName((char *)szROM, (char *)szGameName);
   while (PCENameList[i].ulCRC)
   {
	   if (PCENameList[i].ulCRC == ulCRC)
	   {
#ifdef _UNICODE
		   mbstowcs(szGameName, PCENameList[i].name, strlen(PCENameList[i].name)+1);
#else
		   strcpy((char *)szGameName, PCENameList[i].name);
#endif
		   break;
	   }
	   i++;
   }

   // RESET vector should be 0xExxx or 0xFxxx, of backwards, need to flip the bits
   i = pTemp[0x1ffe] + (pTemp[0x1fff]<<8);
   // TaleSpin, Legend of Hero Tonma
   if (i < 0xe000)// || ulCRC == 0x51ae2412 || ulCRC == 0x773ea667) // address looks fishy, mirror the bits
      {
      for (i=0; i<iLen; i++)
         pTemp[i] = ucMirror[pTemp[i]];
      }
   if (iLen == 0x40000) // special case where rom has mirrored addresses - deep blue (j)
      {
      memcpy(mem_map, pTemp, iLen);
      memcpy(&mem_map[0x40000], pTemp, iLen);
      memcpy(&mem_map[0x80000], pTemp, iLen);
      memcpy(&mem_map[0xc0000], pTemp, iLen);
      }
      
   if (iLen == 0x60000) // special case for 384K (needs to be split)
      {
      memcpy(mem_map, pTemp, 0x40000);
      memcpy(&mem_map[0x80000], &pTemp[0x40000], iLen - 0x40000);
      }
   else
      {
      if (iLen > 0x100000)
         iLen = 0x100000;
      memcpy(mem_map, pTemp, iLen);
      }
   EMUFree(pTemp);
   return FALSE;

} /* PCELoadRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadGGRoms(char *, char *, char *)                      *
 *                                                                          *
 *  PURPOSE    : Load and ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
int EMULoadGGRoms(char *szROM, unsigned char **pBank, int *iGGMode)
{
int i, rc, iLen;
void * iHandle;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];
BOOL bColeco = FALSE;

   iLen = 0;
   *iGGMode = MODE_SMS; // assume SMS
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
      zHandle = unzOpen(szTemp);
#else
//      __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "EMULoadGGRoms() about to open file with zlib");
      zHandle = unzOpen((char *)szROM);
//      __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "file opened, handle=0x%08x", (int)zHandle);
#endif
      if (zHandle == NULL)
         return iLen; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.GG file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-3], ".gg") == 0) // it's a gamegear ROM, load it
            {
            *iGGMode = MODE_GG;
            break;
            }
         if (stricmp(&szTemp[i-4], ".sms") == 0) // it's a SMS ROM, load it
            break;
         if (stricmp(&szTemp[i-4], ".rom") == 0) // it's a Colecovision ROM, load it
            {
            bColeco = TRUE;
            break;
            }
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
//    	    __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "found a gg file, about to open it");
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
//            __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "file size = %d", iLen);
            if (bColeco)
               {
               *pBank = EMUAlloc(iLen+8192);
               unzReadCurrentFile(zHandle, *pBank+8192, iLen); /* Read the file into memory */
               rc = unzCloseCurrentFile(zHandle);
               // Read the BIOS ROM
               i = (int)strlen((char *)szROM);
               while (i >= 0 && szROM[i] != '\\') // get rid of leaf name
                  i--;
               szROM[i+1] = 0; // get source path
               strcat((char *)szROM, "coleco.rom");
               iHandle = EMUOpenRO((char *)szROM);
               EMURead(iHandle, *pBank, 8192); // read the BIOS ROM image
               EMUClose(iHandle);
               }
            else
               {
               *pBank = EMUAlloc(iLen);
//               __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "about to read file into pBank");
               unzReadCurrentFile(zHandle, *pBank, iLen); /* Read the file into memory */
               rc = unzCloseCurrentFile(zHandle);
//               __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "finished reading file");
               }
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      i = (int)strlen((char *)szROM);
      if (stricmp((char *)&szROM[i-3], ".gg") == 0) // it's a gamegear ROM
         *iGGMode = MODE_GG;
      if (stricmp((char *)&szROM[i-3], ".rom") == 0) // it's a Coleco ROM
         bColeco = TRUE;
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      EMUSeek(iHandle, 0, 0); // seek to start of ROM data
      if (bColeco)
         {
         *pBank = EMUAlloc(iLen+8192);
         EMURead(iHandle, *pBank+8192, iLen); // read the whole ROM image into the banked ROM area
         EMUClose(iHandle);
         }
      else
         {
         *pBank = EMUAlloc(iLen);
         EMURead(iHandle, *pBank, iLen); // read the whole ROM image into the banked ROM area
         EMUClose(iHandle);
         }
      }
   if ((iLen & 0x3fff) == 0x200) // header present, punt it
      {
      memcpy(*pBank, *pBank+0x200, iLen-0x200);
      iLen -= 0x200;
      }
   return iLen;

} /* EMULoadGGRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadMSXRom(char *, char *, char *)                      *
 *                                                                          *
 *  PURPOSE    : Load and ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
int EMULoadMSXRom(char *szROM, unsigned char *mem_map, unsigned char **pBank)
{
int i, rc, iLen;
void * iHandle;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];

   iLen = 0;
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)szROM);
#endif
      if (zHandle == NULL)
         return iLen; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.GG file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".rom") == 0) // it's a ROM, load it
            break;
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
            *pBank = EMUAlloc(iLen);
            unzReadCurrentFile(zHandle, *pBank, iLen); /* Read the file into memory */
            rc = unzCloseCurrentFile(zHandle);
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      i = (int)strlen((char *)szROM);
//      if (stricmp(&szROM[i-3], TEXT(".gg")) == 0) // it's a gamegear ROM
//         *bGG = TRUE;
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      EMUSeek(iHandle, 0, 0); // seek to start of ROM data
      *pBank = EMUAlloc(iLen);
      EMURead(iHandle, *pBank, iLen); // read the whole ROM image into the banked ROM area
      EMUClose(iHandle);
      }
//   if ((iLen & 0x3fff) == 0x200) // header present, punt it
//      {
//      memcpy(*pBank, *pBank+0x200, iLen-0x200);
//      iLen -= 0x200;
//      }
   return iLen;

} /* EMULoadMSXRom() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadNESRoms(char *, char *, char *)                     *
 *                                                                          *
 *  PURPOSE    : Load and ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
int EMULoadNESRoms(char *szROM, unsigned char *mem_map, unsigned char **pBank)
{
int i, rc, iLen;
void *iHandle;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];

   iLen = 0;
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)szROM);
#endif
      if (zHandle == NULL)
         return iLen; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.GG file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".nes") == 0) // it's a NES ROM, load it
            break;
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
            *pBank = EMUAlloc(iLen);
            unzReadCurrentFile(zHandle, *pBank, iLen); /* Read the file into memory */
            rc = unzCloseCurrentFile(zHandle);
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      EMUSeek(iHandle, 0, 0); // seek to start of ROM data
      *pBank = EMUAlloc(iLen);
      EMURead(iHandle, *pBank, iLen); // read the whole ROM image into the banked ROM area
      EMUClose(iHandle);
      }
   return iLen;

} /* EMULoadNESRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : SNESLoadRoms(char *, char *, char *)                       *
 *                                                                          *
 *  PURPOSE    : Load and ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
unsigned char * SNESLoadRoms(char *szROM, unsigned char *mem_flags, unsigned long *mem_offsets, BOOL *pbHighROM, BOOL *bPAL, int *iSRAMMask)
{
int i, iNumBlocks, rc, iLen, iOffset, iSrc = 0;
void * iHandle;
unzFile zHandle;
unz_file_info unzInfo;
unsigned char cSRAMSize = 0;
char szTemp[256];
//unsigned char *p;
unsigned short usCheckSum;
unsigned char *pMem = NULL;

   iLen = 0;
   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, strlen(szROM)+1);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)szROM);
#endif
      if (zHandle == NULL)
         return NULL; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.SMC file
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".smc") == 0 || stricmp(&szTemp[i-4], ".fig") == 0) // it's a SNES ROM, load it
            break;
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
            pMem = EMUAlloc(iLen + 0x28000); // leave room for WRAM + SRAM + PPU regs
            unzReadCurrentFile(zHandle, &pMem[0x8000], iLen); /* Read the file into memory */
            rc = unzCloseCurrentFile(zHandle);
            if (iLen & 0x200) // has 512 byte header
               {
               iLen -= 0x200;
               memcpy(&pMem[0x8000], &pMem[0x8200], iLen);
               }
            }
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return NULL;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      if (iLen & 0x200) // there is a 512 byte header
         {
         iLen -= 0x200;
         EMUSeek(iHandle, 0x200, 0); // seek to start of ROM data
         }
      else
         EMUSeek(iHandle, 0, 0); // start at the beginning of the file
      pMem = EMUAlloc(iLen + 0x28000);
      EMURead(iHandle, &pMem[0x8000], iLen); // read the entire ROM into the temp buffer
      EMUClose(iHandle);
      }

   // Arrange the pointers to the ROM data in the correct places
   if (iLen >= 0x10000)
      {
      usCheckSum = (pMem[0xffdc + 0x8000] + pMem[0xffdd + 0x8000]*256);
      usCheckSum |= (pMem[0xffde + 0x8000] + pMem[0xffdf + 0x8000]*256);
      if (usCheckSum == 0xffff && pMem[0xffc0 + 0x8000] >= 'A' && pMem[0xffc0 + 0x8000] <= 'Z') // HiROM
         {
         *pbHighROM = TRUE;
//         memcpy(szGameName, &pMem[0xffc0+0x8000], 21);
//         szGameName[21] = 0;
         cSRAMSize = pMem[0xffd8 + 0x8000]; // 0=none,1=2k,2=4k,3=8k
         if (pMem[0xffd9 + 0x8000] >= 2) // Europe, PAL system
            *bPAL = TRUE;
         memcpy(pMem, &pMem[0x8000], iLen-0x8000);
//         iLen -= 0x8000;
         iNumBlocks = iLen >> 16; // number of 64k blocks
	     iSrc = 0x0000; // source offset
        mem_offsets[0] = (unsigned long)pMem; // lowest 32k is mapped to bottom of memory map
        mem_offsets[0x800000>>15] = (unsigned long)(pMem - 0x800000); // lowest 32k is mapped to bottom of memory map
         for (i=0; i<iNumBlocks; i++)
            {
            iOffset = i*0x10000;
            mem_offsets[(iOffset + 0x400000) >> 15] = (unsigned long)&pMem[iSrc] - (iOffset+0x400000); // calculate pointer for this block of 64k (part 1)
            mem_offsets[(iOffset + 0x408000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0x408000); // part 2
            mem_offsets[(iOffset + 0xc00000) >> 15] = (unsigned long)&pMem[iSrc] - (iOffset + 0xc00000); // mirrored here (part 1)
            mem_offsets[(iOffset + 0xc08000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0xc08000); // mirrored here (part 2)
         // high parts of 64k blocks are shadowed in LOROM area
            mem_offsets[(iOffset + 0x008000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0x8000); // upper half of banks also used
            mem_offsets[(iOffset + 0x808000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0x808000); // mirrored here also
            if (iNumBlocks <= 0x20) // mirror it here
               {
               mem_offsets[(iOffset + 0x600000) >> 15] = (unsigned long)&pMem[iSrc] - (iOffset+0x600000); // calculate pointer for this block of 64k (part 1)
               mem_offsets[(iOffset + 0x608000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0x608000); // part 2
               mem_offsets[(iOffset + 0xe00000) >> 15] = (unsigned long)&pMem[iSrc] - (iOffset+0xe00000); // mirrored here (part 1)
               mem_offsets[(iOffset + 0xe08000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0xe08000); // mirrored here (part 2)
         // high parts of 64k blocks are shadowed in LOROM area
               mem_offsets[(iOffset + 0x208000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0x208000); // upper half of banks also used
               mem_offsets[(iOffset + 0xa08000) >> 15] = (unsigned long)&pMem[iSrc + 0x8000] - (iOffset + 0xa08000); // mirrored here also
               memset(&mem_flags[(iOffset + 0x600000)>>8], 0x81, 256); // mark this section as ROM
               memset(&mem_flags[(iOffset + 0xe00000)>>8], 0x81, 256); // mark this mirrored section as ROM
               }
//            memcpy(&mem_map[iOffset + 0x400000], &pMem[iOffset], 0x10000);
//            memcpy(&mem_map[iOffset + 0x8000], &pMem[iOffset + 0x8000], 0x8000);
//            memcpy(&mem_map[iOffset + 0x808000], &pMem[iOffset + 0x8000], 0x8000);
//            memcpy(&mem_map[iOffset + 0xc00000], &pMem[iOffset], 0x10000); // mirrored at bank 0x80-0xff
            memset(&mem_flags[(iOffset + 0x400000)>>8], 0x81, 256); // mark this section as ROM
            memset(&mem_flags[(iOffset + 0xc00000)>>8], 0x81, 256); // mark this mirrored section as ROM
            memset(&mem_flags[(iOffset + 0x000000)>>8], 0x81, 256); // mark this section as ROM
            memset(&mem_flags[(iOffset + 0x800000)>>8], 0x81, 256); // mark this section as ROM
			iSrc += 0x10000;
            }
         goto rom_done;
         }
      }
   bPAL = FALSE; // assume Japan/USA
   usCheckSum = (pMem[0x7fdc + 0x8000] + pMem[0x7fdd + 0x8000]*256);
   usCheckSum |= (pMem[0x7fde + 0x8000] + pMem[0x7fdf + 0x8000]*256);
   if (usCheckSum == 0xffff || usCheckSum == 0) // LoROM
      {
      *pbHighROM = FALSE;
//      memcpy(szGameName, &pMem[0x7fc0+0x8000], 21);
//      szGameName[21] = 0;
      cSRAMSize = pMem[0x7fd8 + 0x8000]; // 0=none,1=2k,2=4k,3=8k
	  if (pMem[0x7fd9 + 0x8000] >= 2) // Europe, PAL system
		  *bPAL = TRUE;
// The LoROM is laid out contiguously in the file, but it is really broken into 32k chunks which occupy the top 32k of each 64k segment
      iOffset = 0x8000; // target offset in the virtual memory map
      iSrc = 0x8000; // source offset
//      p = pMem;
      while (iLen >= 0x8000 && iOffset < 0x400000) // copy in 32k chunks
         {
         mem_offsets[iOffset >> 15] = (unsigned long)&pMem[iSrc] - iOffset; // calculate pointer for this block of 32k
         mem_offsets[(iOffset + 0x800000) >> 15] = (unsigned long)&pMem[iSrc] - (iOffset + 0x800000); // mirrored at bank 0x80-0xff
         mem_offsets[(iOffset - 0x008000) >> 15] = (unsigned long)pMem - (iOffset - 0x8000); // bottom half of each bank point to 0
         mem_offsets[(iOffset + 0x7f8000) >> 15] = (unsigned long)pMem - (iOffset + 0x7f8000); // mirrored too
         memset(&mem_flags[iOffset>>8], 0x81, 128); // mark this section as ROM
         memset(&mem_flags[(iOffset + 0x800000)>>8], 0x81, 128); // mark this mirrored section as ROM
         iOffset += 0x10000; // skip to next 64k bank
         iLen -= 0x8000;
         iSrc += 0x8000; // next 32k chunk
         }
      while (iOffset < 0x400000) // mark the rest of the memory map as ROM and map it to the first ROM page
         {
         mem_offsets[iOffset >> 15] = (unsigned long)&pMem[0x8000] - iOffset; // calculate pointer for this block of 32k
         mem_offsets[(iOffset + 0x800000) >> 15] = (unsigned long)&pMem[0x8000] - (iOffset + 0x800000); // mirrored at bank 0x80-0xff
         mem_offsets[(iOffset - 0x008000) >> 15] = (unsigned long)pMem - (iOffset - 0x8000); // bottom half of each bank point to 0
         mem_offsets[(iOffset + 0x7f8000) >> 15] = (unsigned long)pMem - (iOffset + 0x7f8000); // mirrored too
         memset(&mem_flags[iOffset>>8], 0x81, 128); // mark this section as ROM
         memset(&mem_flags[(iOffset + 0x800000)>>8], 0x81, 128); // mark this mirrored section as ROM
         iOffset += 0x10000; // skip to next 64k bank
         }
// DEBUG
//      if (iLen && iOffset > 0x400000)
//         iOffset -= 0x8000; // start at bank 0x40
//      while (iLen >= 0x8000) // copy in 64k chunks
//         {
//         memcpy(&mem_map[iOffset], p, 0x10000); // 64k
//         p += 0x10000;
//         memcpy(&mem_map[iOffset + 0x800000], &mem_map[iOffset], 0x10000); // mirrored at bank 0xc0-0xff
//         memset(&mem_flags[iOffset>>8], 0x81, 256); // mark this section as ROM
//         memset(&mem_flags[(iOffset+0x800000)>>8], 0x81, 256); // mark this mirrored section as ROM
//         iOffset += 0x10000; // skip to next 64k bank
//         iLen -= 0x10000;
//         }
      }
rom_done:
// Map the 128K of WRAM at the end of the ROM
   mem_offsets[0x7e0000 >> 15] = (unsigned long)&pMem[iSrc] - 0x7e0000;
   mem_offsets[0x7e8000 >> 15] = (unsigned long)&pMem[iSrc] - 0x7e0000;
   mem_offsets[0x7f0000 >> 15] = (unsigned long)&pMem[iSrc] - 0x7e0000;
   mem_offsets[0x7f8000 >> 15] = (unsigned long)&pMem[iSrc] - 0x7e0000;
   memset(&mem_flags[(0x7e0000)>>8], 0x0, 512); // mark 128K as RAM
// FE/FF map to the same place as workram (7E/7F)
   mem_offsets[0xfe0000 >> 15] = (unsigned long)&pMem[iSrc] - 0xfe0000;
   mem_offsets[0xfe8000 >> 15] = (unsigned long)&pMem[iSrc] - 0xfe0000;
   mem_offsets[0xff0000 >> 15] = (unsigned long)&pMem[iSrc] - 0xfe0000;
   mem_offsets[0xff8000 >> 15] = (unsigned long)&pMem[iSrc] - 0xfe0000;
   memset(&mem_flags[(0xfe0000)>>8], 0x0, 512); // mark 128K as RAM
// get the SRAM size mask
   switch (cSRAMSize)
   {
   case 1: // 16K bits
	   *iSRAMMask = 0x7ff;
	   break;
   case 2: // 32K bits
	   *iSRAMMask = 0xfff;
	   break;
   case 3: // 64K bits
	   *iSRAMMask = 0x1fff;
	   break;
   default:
	   *iSRAMMask = 0; // no SRAM present
	   break;
   }
// remove trailing spaces from the game name
//   i = 21;
//   while (i > 0 && szGameName[i] <= ' ')
//      {
//      szGameName[i] = 0;
//      i--;
//      }
   return pMem;

} /* SNESLoadRoms() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMULoadGBRoms(char *, char *)                              *
 *                                                                          *
 *  PURPOSE    : Load and ROM images.                                       *
 *                                                                          *
 ****************************************************************************/
BOOL EMULoadGBRoms(char *szROM, unsigned char **pBank)
{
int i, rc, iLen;
void * iHandle;
unzFile zHandle;
unz_file_info unzInfo;
char szTemp[256];
unsigned char *pTemp;

   i = (int)strlen((char *)szROM);
   if (stricmp((char *)&szROM[i-4], ".ZIP") == 0) // it's a zip file, load it another way
      {
#ifdef _UNICODE
      wcstombs(szTemp, szROM, 256);
#else
      strcpy(szTemp, (char *)szROM);
#endif
//      g_print("EMULoadGBRoms - About to call unzOpen() on file %s\n", szTemp);
      zHandle = unzOpen(szTemp);
//      g_print("Returned from unzOpen(), zHandle = %d\n", (int)(unsigned long)zHandle);
      if (zHandle == NULL)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE; // error opening the file
         }
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      while (rc == UNZ_OK) // look for a *.GB or *.GBC file
         {
//	g_print("looking for a GB/GBC file in the zip file\n");
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-3], ".gb") == 0) // it's a gameboy ROM, load it
            break;
         if (stricmp(&szTemp[i-4], ".gbc") == 0) // it's a gameboy color ROM, load it
            break;
         rc = unzGoToNextFile(zHandle);
         }
      if (rc == UNZ_OK) // we found the file
         {
//	g_print("about to call unzOpenCurrentFile\n");
         rc = unzOpenCurrentFile(zHandle); /* Try to open the file we want */
//	g_print("returned from unzOpenCurrentFile\n");
         if (rc == UNZ_OK)
            {
            iLen = (int)unzInfo.uncompressed_size;
//	g_print("about to allocate %d bytes, pBank = %08x\n", iLen, (int)(unsigned long)pBank);
            *pBank = EMUAlloc(iLen);
//	g_print("about to call unzReadCurrentFile\n");
            unzReadCurrentFile(zHandle, *pBank, iLen); /* Read the file into memory */
            rc = unzCloseCurrentFile(zHandle);
            pTemp = *pBank;
            if ((iLen & 0xfff) == 0x200 && pTemp[0x80] == 0) // 512 extra bytes, trim them
               {
               memcpy(pTemp, &pTemp[0x200], iLen-0x200);
               iLen -= 0x200;
               }
            }
         }
      else
         {
         unzClose(zHandle);
//         g_print("Unable to load game ROM image\n");
         return TRUE; // error opening the file
         }
      unzClose(zHandle);
      }
   else
      {
      iHandle = EMUOpenRO((char *)szROM);
      if (iHandle == (void *)-1)
         {
//         MessageBox(hwndClient, TEXT("Unable to load game ROM image"),szROM,MB_ICONSTOP | MB_OK);
         return TRUE;
         }
      iLen = EMUSeek(iHandle, 0, 2);
      EMUSeek(iHandle, 0, 0); // seek to start of ROM data
      *pBank = EMUAlloc(iLen);
      EMURead(iHandle, *pBank, iLen); // read the whole ROM image into the banked ROM area
      EMUClose(iHandle);
      }
   return FALSE;

} /* EMULoadGBRoms() */

/****************************************************************************

    FUNCTION: EMUTestName(char *)

    PURPOSE:  Test the game filename (zip files too) to see which system it
              is for.
    RETURNS:  Game Type (0=unknown, 1=NES, 2=GG/SMS, 3=GB/GBC

****************************************************************************/
int EMUTestName(char *pszName)
{
int i, iType, rc;
void * ihandle;
unz_file_info unzInfo;
char szTemp[256];
unzFile zHandle;
unz_global_info ugi;

   iType = SYS_UNKNOWN;
   // See if it's a save game file
   i = (int)strlen((char *)pszName);
   if (stricmp((char *)&pszName[i-5], "SGSAV") == 0) // our save game
      { // match the "fourcc" type
      ihandle = EMUOpen((char *)pszName);
      EMURead(ihandle, szTemp, 4);
      EMUClose(ihandle);
      for (i=1; i<6; i++)
         {
         if (memcmp(szTemp, szGameFourCC[i], 4) == 0) // a match
            {
            iType = i;
            break;
            }
         }
      if (iType != SYS_UNKNOWN) // valid file
         return iType;
      }
   if (stricmp((char *)&pszName[i-4], ".ZIP") == 0) // it's a zip file, test it another way
      {
//          if (SG_MatchCoinOp(pszName))
//          {
//              return SYS_COINOP;
//          }
#ifdef _UNICODE
      wcstombs(szTemp, pszName, 256);
      zHandle = unzOpen(szTemp);
#else
      zHandle = unzOpen((char *)pszName);
#endif
      if (zHandle == NULL)
         return -1; // error opening the file
      unzGoToFirstFile(zHandle);
      rc = UNZ_OK;
      rc = unzGetGlobalInfo(zHandle, &ugi);
//      if (rc == UNZ_OK && ugi.number_entry >= 6) // must be coin-op
//         {
//         unzClose(zHandle); // we're done with the file, close it
//         return SYS_COINOP;
//         }
      while (rc == UNZ_OK && !iType) // look for a game ROM file for the various systems
         {
         rc = unzGetCurrentFileInfo (zHandle, &unzInfo, szTemp, 256, NULL, 0, NULL,  0);
         i = (int)strlen(szTemp);
         if (stricmp(&szTemp[i-4], ".nes") == 0) // it's a NES ROM, try to load it
            iType = SYS_NES;
         if (stricmp(&szTemp[i-3], ".gg") == 0) // it's a gamegear ROM, try to load it
            iType = SYS_GAMEGEAR;
         if (stricmp(&szTemp[i-4], ".sms") == 0) // it's a Master System ROM, try to load it
            iType = SYS_GAMEGEAR;
         if (stricmp(&szTemp[i-3], ".gb") == 0) // it's a gameboy ROM, try to load it
            iType = SYS_GAMEBOY;
         if (stricmp(&szTemp[i-4], ".gbc") == 0) // it's a gameboy color ROM, try to load it
            iType = SYS_GAMEBOY;
         if (stricmp(&szTemp[i-4], ".pce") == 0) // it's a PC-Engine ROM, try to load it
            iType = SYS_TG16;
         if (stricmp(&szTemp[i-4], ".smd") == 0 || stricmp(&szTemp[i-3], ".md") == 0) // it's a Genesis ROM, try to load it
            iType = SYS_GENESIS;
         if (stricmp(&szTemp[i-4], ".bin") == 0 || stricmp(&szTemp[i-4],".gen") == 0) // it's a Genesis ROM, try to load it
            iType = SYS_GENESIS;
//         if (stricmp(&szTemp[i-4], ".rom") == 0) // it's a COLECO ROM, try to load it
//            iType = SYS_COLECO;
//         if (stricmp(&szTemp[i-4], ".smc") == 0) // it's a Super Nintendo ROM, try to load it
//            iType = SYS_SNES;
//         if (stricmp(&szTemp[i-4], ".fig") == 0) // it's a Super Nintendo ROM, try to load it
//            iType = 6;
         rc = unzGoToNextFile(zHandle);
         }
      unzClose(zHandle); // we're done with the file, close it
      return iType;
      }
   else // not a zip file
      {
      i = (int)strlen((char *)pszName);
      if (stricmp((char *)&pszName[i-4], ".nes") == 0) // it's a NES ROM
         return SYS_NES;
      if (stricmp((char *)&pszName[i-4], ".sms") == 0) // it's a Master System ROM
         return SYS_GAMEGEAR;
      if (stricmp((char *)&pszName[i-3], ".gg") == 0) // it's a gamegear ROM
         return SYS_GAMEGEAR;
      if (stricmp((char *)&pszName[i-3], ".gb") == 0) // it's a gameboy ROM
         return SYS_GAMEBOY;
      if (stricmp((char *)&pszName[i-4], ".gbc") == 0) // it's a gameboy color ROM
         return SYS_GAMEBOY;
      if (stricmp((char *)&pszName[i-4], ".pce") == 0) // it's a PC-Engine ROM
         return SYS_TG16;
      if (stricmp((char *)&pszName[i-4], ".smd") == 0 || stricmp((char *)&pszName[i-3], ".md") == 0) // it's a Genesis ROM
         return SYS_GENESIS;
      if (stricmp((char *)&pszName[i-4], ".bin") == 0 || stricmp((char *)&pszName[i-4],".gen") == 0) // it's a Genesis ROM
         return SYS_GENESIS;
//      if (stricmp((char *)&pszName[i-4], ".smc") == 0) // it's a Super Nintendo ROM
//         return 6;
//      if (stricmp((char *)&pszName[i-4], ".fig") == 0) // it's a Super Nintendo ROM
//         return 6;
      return SYS_UNKNOWN; // unrecognized extension
      }

} /* EMUTestName() */

BOOL SGLoadGame(char *szGame, GAME_BLOB *pBlob, int iGame)
{
int i, iFileLen, iDstOffset, iSrcOffset;
void *ihandle;
int iRepeat;
char szTemp[256];
unsigned char c, *d, *pTemp;

//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Entering SGLoadGame()");
if (szGame)
      {
#ifdef __MACH__
          strcpy(szTemp, szGame); // filename is coming from a file picker; don't change it
#else
   sprintf((char *)szTemp, "%s%s%s%02d.sgsav", pszSAVED, szGame, szGameFourCC[iGameType], iGame);
#endif
          //   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "About to open file %s", szTemp);
   ihandle = EMUOpenRO((char *)szTemp);
   if (ihandle == (void *)-1) // error, leave
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Error opening file %s", szTemp);
      return TRUE;
   }
   iFileLen = EMUSeek(ihandle, 0, 2); // get the length
//   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Filelength returned = %d", iFileLen);
   EMUSeek(ihandle,0,0);
   if (iFileLen < 1024) // must be a bad file
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "File too small, len = %d", iFileLen);
	   EMUClose(ihandle);
	   return TRUE;
   }
   pTemp = EMUAlloc(iFileLen+2);
   EMURead(ihandle, pTemp, iFileLen);
   pTemp[iFileLen] = 0; // make sure it doesn't go past the end
   pTemp[iFileLen+1] = 0;
   EMUClose(ihandle);
   }
else
   {
   pTemp = pBlob->pRewind[iGame];
   }
   iSrcOffset = 256+4; // skip the embedded filename + fourcc value
   i = 0; // walk through all of the defined memory areas
   while (pBlob->mem_areas[i].iAreaLength)
      {
      iDstOffset = 0;
      d = pBlob->mem_areas[i].pPrimaryArea;
      while (iDstOffset < pBlob->mem_areas[i].iAreaLength)
         {
         c = pTemp[iSrcOffset++];
         if (c == 0) // repeating zeros
            {
            iRepeat = pTemp[iSrcOffset++] + 1;
            if (iDstOffset+iRepeat <= pBlob->mem_areas[i].iAreaLength)
               memset(&d[iDstOffset], 0, iRepeat);
            iDstOffset += iRepeat;
            }
         else
            {
        	if (iDstOffset < pBlob->mem_areas[i].iAreaLength)
               d[iDstOffset++] = c;
            }
         }
      // decompress this memory area
      i++;
      }
   if (szGame)
      {
      EMUFree(pTemp);
      }
//   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Exiting SGLoadGame()");
   pBlob->bRewound = TRUE; // cause game to take appropriate action after being loaded
   return FALSE;
   
} /* SGLoadGame() */

BOOL SGSaveHighScore(char *szGame, unsigned char *pMem, int iLen)
{
char szTemp[256];
void * ohandle;

//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Entering SGSaveHighScore()");
   sprintf((char *)szTemp, "%s%s.hi", pszHighScores, szGame);
   ohandle = EMUCreate((char *)szTemp);
   if (ohandle == (void *)-1) // error, leave
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Error creating file %s", szTemp);
      return TRUE;
   }
   EMUWrite(ohandle, pMem, iLen);
   EMUClose(ohandle);
   return FALSE;

} /* SGSaveHighScore() */

#ifdef FUTURE
// Load the dip switch settings for all of the coin-op games
void SGLoadDips(void)
{
int i, j, iLen;
void * ihandle;
unsigned char szTemp[256];
int iValues[32];
GAMEOPTIONS *pOptions;

//   __android_log_print(ANDROID_LOG_VERBOSE, "SGLoadDips", "Entering");
   i = 0;
   while (gameList[i].szName != NULL)
   {
	   sprintf((char *)szTemp, "%s%s.dip", pszDIPs, gameList[i].szROMName);
	   ihandle = EMUOpenRO((char *)szTemp);
	   if (ihandle != (void *)-1)
	   {
		  pOptions = gameList[i].pGameOptions;
          iLen = EMURead(ihandle, (unsigned char *)&iValues[0], 32*sizeof(int)); // see how many values are there
          for (j=0; j<iLen/4; j++)
          {
        	  pOptions[j].iChoice = iValues[j]; // load up the values
          }
          EMUClose(ihandle);
	   }
	   i++;
   }
//   __android_log_print(ANDROID_LOG_VERBOSE, "SGLoadDips", "Leaving");

} /* SGLoadDips() */

// Save the dipswitch settings for a single game
void SGSaveDips(int iGame)
{
int i;
void *ohandle;
GAMEOPTIONS *pOptions;
unsigned char szTemp[256];

//__android_log_print(ANDROID_LOG_VERBOSE, "SGSaveDips", "Entering");
	pOptions = gameList[iGame].pGameOptions;
	sprintf((char *)szTemp, "%s%s.dip", pszDIPs, gameList[iGame].szROMName);
	ohandle = EMUCreate((char *)szTemp);
	if (ohandle != (void *)-1)
	{
		i = 0;
		while (pOptions[i].szName != NULL)
		{
			EMUWrite(ohandle, (unsigned char *)&pOptions[i].iChoice, sizeof(int));
			i++;
		}
		EMUClose(ohandle);
    }
//	__android_log_print(ANDROID_LOG_VERBOSE, "SGSaveDips", "Leaving");

} /* SGSaveDips() */
#endif // FUTURE

BOOL SGLoadHighScore(char *szGame, unsigned char *pMem, int iLen)
{
char szTemp[256];
void * ohandle;

//	__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Entering SGLoadHighScore()");
   sprintf((char *)szTemp, "%s%s.hi", pszHighScores, szGame);
   ohandle = EMUOpenRO((char *)szTemp);
   if (ohandle == (void *)-1) // error, leave
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Error opening file %s", szTemp);
      return TRUE;
   }
   EMURead(ohandle, pMem, iLen);
   EMUClose(ohandle);
   return FALSE;

} /* SGLoadHighScore() */

BOOL SGSaveGame(char *szGame, GAME_BLOB *pBlob, int iGame)
{
int i;
void * ohandle = NULL;
int iSrcOffset;
int iLen, iCount;
char szTemp[256];
char cTemp[256];
unsigned char c, *s, *pTemp;

//__android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Entering SGSaveGame()");

if (szGame)
   {
#ifdef __MACH__
       strcpy(szTemp, szGame); // don't change the name since it came from a file picker
#else
   sprintf((char *)szTemp, "%s%s%s%02d.sgsav", pszSAVED, szGame, szGameFourCC[iGameType], iGame);
#endif
       //   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "About to create file %s", szTemp);
   ohandle = EMUCreate((char *)szTemp);
   if (ohandle == (void *)-1) // error, leave
   {
//	   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Error creating file %s", szTemp);
      return TRUE;
   }
   memset(cTemp, 0, 256);
#ifdef _UNICODE
   wcstombs(cTemp, pszGame, wcslen(pszGame)+1);
#else
   strcpy(cTemp, (char *)pszGame);
#endif
   pTemp = EMUAlloc(MAX_SAVE_SIZE);
   }
else
   {
   pTemp = pBlob->pRewind[pBlob->iRewindIndex]; // saving the state for the rewind function
   }
   memcpy(pTemp, szGameFourCC[iGameType], 4);
   memcpy(&pTemp[4], cTemp, 256); // first 256 bytes are ROM filename
   iLen = 256+4; // skip the embedded filename + fourcc
   i = 0; // walk through all of the defined memory areas
   iCount = 0;
   while (pBlob->mem_areas[i].iAreaLength)
      {
      s = pBlob->mem_areas[i].pPrimaryArea;
      for (iSrcOffset = 0; iSrcOffset < pBlob->mem_areas[i].iAreaLength; iSrcOffset++)
         {
         c = s[iSrcOffset];
         if (c == 0) /* Compress repeating zeros */
            {
            iCount++;
            if (iCount == 256) /* Max count */
               {
               pTemp[iLen++] = 0; /* Store zero and length byte */
               pTemp[iLen++] = (unsigned char)(iCount-1);
               iCount = 0; /* start at zero again */
               }
            }
         else /* Non-zero */
            {
            if (iCount) /* Store any zeros we were holding on to */
               {
               pTemp[iLen++] = 0;
               pTemp[iLen++] = (unsigned char)(iCount-1);
               iCount = 0;
               }
            pTemp[iLen++] = c;
            }
         } // for each byte of the memory area
      if (iCount) /* left over 0 count */
         {
         pTemp[iLen++] = 0; /* Store zero and length byte */
         pTemp[iLen++] = (unsigned char)(iCount-1);
         iCount = 0; /* start at zero again */
         }
      // compress the next memory area
      i++;
      }
   if (szGame)
      {
      EMUWrite(ohandle, pTemp, iLen);
      EMUClose(ohandle);
      EMUFree(pTemp);
      SG_SaveGameImage(iGame); // save a PNG of this game state
      }
   else
      {
      pBlob->iRewindSize[pBlob->iRewindIndex] = iLen;
      pBlob->iRewindIndex = (pBlob->iRewindIndex + 1) & 3;
      }
//   __android_log_print(ANDROID_LOG_VERBOSE, "smartgear", "Exiting SGSaveGame()");
   return FALSE;
} /* SGSaveGame() */

void SkipToEnd(char *pBuf, int *i, int iLen)
{
    int j = *i;
    while (j < iLen)
    {
        if (pBuf[j] == 0xa || pBuf[j] == 0xd) // end of the line
            break;
        j++;
    }
    while (j < iLen && (pBuf[j] == 0xa || pBuf[j] == 0xd))
    {
        j++;
    }
    *i = j;
} /* SkipToEnd() */

int ParseNumber(char *pBuf, int *i, int iLen)
{
    char cTemp[32];
    int iValue;
    int k = 0;
    int j = *i;
    
    // skip spaces and non numbers
    while (j < iLen && (pBuf[j] < '0' || pBuf[j] > '9'))
    {
        j++;
    }
    if (j >= iLen) // went past end, problem
        return 0;
    while (j < iLen && k < 32 && (pBuf[j] >= '0' && pBuf[j] <= '9'))
    {
        cTemp[k++] = pBuf[j++];
    }
    cTemp[k] = '\0'; // terminate string
    iValue = atoi(cTemp);
    *i = j;
    return iValue;
} /* ParseNumber() */

void ParseString(char *szDest, char *pBuf, int *i, int iLen)
{
    int k = 0;
    int j = *i;
    
    // skip spaces
    while (j < iLen && pBuf[j] == ' ' && j < iLen)
    {
        j++;
    }
    if (j >= iLen) // went past end, problem
    {
        szDest[0] = '\0';
        return;
    }
    while (j < iLen && k < 256 && pBuf[j] > ' ')
    {
        szDest[k++] = pBuf[j++];
    }
    szDest[k] = '\0'; // terminate string
    *i = j;
} /* ParseString() */

//
// Read the default settings from a plain text file
//
int SG_ParseConfigFile(char *szConfig, char *szDir)
{
    void *fh;
    char *pBuf;
    int i, j, iLen;
    int *pControl;
    int bValid = 0;
    
    fh = EMUOpenRO(szConfig);
    if (fh == (void *)-1) return -1; // no config file!
    // Set default values
    iLCDType = -1;
    iDisplayType = -1;
    iDispOffX = iDispOffY = 0;
    iSPIChan = -1;
    iSPIFreq = -1;
    iDC = 16;
    iReset = 18;
    iLED = 11;
    iGamma = 0; // LCD gamma setting (0/1)
    bVerbose = 0;
    bStretch = 0; // assume no display stretching
#ifndef __MACH__
    pBuf = getcwd(szDir, 256); // start in the current working directory
    if (pBuf == NULL) {};
    memset(iButtonGP, 0xff, sizeof(iButtonGP));
#endif
    iP1Control = CONTROLLER_JOY0;
    iP2Control = CONTROLLER_JOY1;
    memset(iButtonKeys, 0, sizeof(iButtonKeys));
    memset(iButtonPins, 0, sizeof(iButtonPins));
    // Read the config file and parse the options
    iLen = EMUSeek(fh, 0, 2); // get file size
    EMUSeek(fh, 0, 0);
    pBuf = (char *)EMUAlloc(iLen);
    EMURead(fh, pBuf, iLen);
    EMUClose(fh);
    // parse the input file
    i = 0;
    while (i < iLen)
    {
        if (pBuf[i] == '#') // comment, skip line
            SkipToEnd(pBuf, &i, iLen);
        else if (memcmp(&pBuf[i], "verbose", 7) == 0)
        {
            bVerbose = 1;
            i+= 7;
        }
#ifndef __MACH__
        else if (memcmp(&pBuf[i], "start_dir", 9) == 0)
        {
            i += 9;
            ParseString(szDir, pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "display_type",12) == 0)
        {
            i += 13;
            if (memcmp(&pBuf[i], "lcd",3) == 0)
                iDisplayType = DISPLAY_LCD;
            else if (memcmp(&pBuf[i], "fb0",3) == 0)
                iDisplayType = DISPLAY_FB0;
            else if (memcmp(&pBuf[i], "fb1",3) == 0)
                iDisplayType = DISPLAY_FB1;
        }
        else if (memcmp(&pBuf[i], "display_offset_x", 16) == 0)
        {
            i += 17;
            iDispOffX = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "display_offset_y", 16) == 0)
        {
            i += 17;
            iDispOffY = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "lcd_orientation",15) == 0)
        {
            i += 16;
            if (memcmp(&pBuf[i], "flipped",7) == 0)
            {
                bLCDFlip = 1;
            }
        }
        else if (memcmp(&pBuf[i], "lcd_channel",11) == 0)
        {
            i += 11;
            iSPIChan = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "lcd_speed",9) == 0)
        {
            i += 9;
            iSPIFreq = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "lcd_type",8) == 0)
        {
            i += 9;
            if (memcmp(&pBuf[i], "st7735", 6) == 0)
            {
                iLCDType = LCD_ST7735;
                iLCDX = 128; iLCDY = 160;
            }
            else if (memcmp(&pBuf[i], "ili9341",7) == 0)
            {
                iLCDType = LCD_ILI9341;
                iLCDX = 240; iLCDY = 320;
            }
            else if (memcmp(&pBuf[i], "hx8357",6) == 0)
            {
                iLCDType = LCD_HX8357;
                iLCDX = 320; iLCDY = 480;
            }
        }
        else if (memcmp(&pBuf[i], "lcd_dc", 6) == 0)
        {
            i += 6;
            iDC = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "lcd_rst", 7) == 0)
        {
            i += 7;
            iReset = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "lcd_led", 7) == 0)
        {
            i += 7;
            iLED = ParseNumber(pBuf, &i, iLen);
        }
	else if (memcmp(&pBuf[i], "gamma", 5) == 0)
	{
	    i += 6;
	    iGamma = ParseNumber(pBuf, &i, iLen);
	    if (iGamma < 0 || iGamma > 1) iGamma = 0;
	}
#endif // __MACH__
        else if (pBuf[i] == 'p' && (pBuf[i+1] == '1' || pBuf[i+1] == '2'))
        {
            char cTemp[32];
            pControl = (pBuf[i+1] == '1') ? &iP1Control:&iP2Control;
            i += 2;
            ParseString(cTemp, pBuf, &i, iLen);
            if (strcmp(cTemp, "keyboard") == 0)
                *pControl = CONTROLLER_KEYBOARD;
            else if (strcmp(cTemp, "gpio") == 0)
                *pControl = CONTROLLER_GPIO;
            else if (strcmp(cTemp, "gamepad0") == 0)
                *pControl = CONTROLLER_JOY0;
            else if (strcmp(cTemp, "gamepad1") == 0)
                *pControl = CONTROLLER_JOY1;
        }
	else if (memcmp(&pBuf[i], "use_framebuffer", 15) == 0)
	{
		i += 16;
		bFramebuffer = (memcmp(&pBuf[i], "on", 2) == 0);
	}
	else if (memcmp(&pBuf[i], "audio", 5) == 0)
	{
		i += 6;
		bAudio = (memcmp(&pBuf[i], "on", 2) == 0);
	}
	else if (memcmp(&pBuf[i], "head2head", 9) == 0)
	{
		i += 10;
		bHead2Head = (memcmp(&pBuf[i],"on",2) == 0);
	}
	else if (memcmp(&pBuf[i], "stretch_hq2x", 12) == 0)
	{
		i += 13;
		bStretch = (memcmp(&pBuf[i], "on", 2) == 0);
	}
        else if (memcmp(&pBuf[i], "gpio_start", 10) == 0)
        {
            i += 10;
            iButtonPins[CTRL_START] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_select", 11) == 0)
        {
            i += 11;
            iButtonPins[CTRL_SELECT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_exit", 9) == 0)
        {
            i += 9;
            iButtonPins[CTRL_EXIT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_up", 7) == 0)
        {
            i += 7;
            iButtonPins[CTRL_UP] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_down",9) == 0)
        {
            i += 9;
            iButtonPins[CTRL_DOWN] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_left", 9) == 0)
        {
            i += 9;
            iButtonPins[CTRL_LEFT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_right", 10) == 0)
        {
            i  += 10;
            iButtonPins[CTRL_RIGHT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_a", 6) == 0)
        {
            i += 6;
            iButtonPins[CTRL_BUTT1] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_b", 6) == 0)
        {
            i += 6;
            iButtonPins[CTRL_BUTT2] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_c", 6) == 0)
        {
            i += 6;
            iButtonPins[CTRL_BUTT3] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gpio_d", 6) == 0)
        {
            i += 6;
            iButtonPins[CTRL_BUTT4] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_start", 9) == 0)
        {
            i += 9;
            iButtonKeys[CTRL_START] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_select", 10) == 0)
        {
            i += 10;
            iButtonKeys[CTRL_SELECT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_exit", 8) == 0)
        {
            i += 8;
            iButtonKeys[CTRL_EXIT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_up", 6) == 0)
        {
            i += 6;
            iButtonKeys[CTRL_UP] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_down",8) == 0)
        {
            i += 8;
            iButtonKeys[CTRL_DOWN] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_left", 8) == 0)
        {
            i += 8;
            iButtonKeys[CTRL_LEFT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_right", 9) == 0)
        {
            i += 9;
            iButtonKeys[CTRL_RIGHT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_a", 5) == 0)
        {
            i += 5;
            iButtonKeys[CTRL_BUTT1] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_b", 5) == 0)
        {
            i += 5;
            iButtonKeys[CTRL_BUTT2] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_c", 5) == 0)
        {
            i += 5;
            iButtonKeys[CTRL_BUTT3] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "key_d", 5) == 0)
        {
            i += 5;
            iButtonKeys[CTRL_BUTT4] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_start", 13) == 0)
        {
            i += 13;
            iButtonGP[CTRL_START] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_select", 14) == 0)
        {
            i += 14;
            iButtonGP[CTRL_SELECT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_exit", 12) == 0)
        {
            i += 12;
            iButtonGP[CTRL_EXIT] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_a", 9) == 0)
        {
            i += 9;
            iButtonGP[CTRL_BUTT1] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_b", 9) == 0)
        {
            i += 9;
            iButtonGP[CTRL_BUTT2] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_c", 9) == 0)
        {
            i += 9;
            iButtonGP[CTRL_BUTT3] = ParseNumber(pBuf, &i, iLen);
        }
        else if (memcmp(&pBuf[i], "gamepad_d", 9) == 0)
        {
            i += 9;
            iButtonGP[CTRL_BUTT4] = ParseNumber(pBuf, &i, iLen);
        }
        else i++;
    }
    EMUFree(pBuf);
    bValid = 1;
    for (i=0; i<CTRL_COUNT; i++)
    {
        if (iP1Control == CONTROLLER_KEYBOARD || iP2Control == CONTROLLER_KEYBOARD)
        {
            if (iButtonKeys[i] == 0)
            {
                printf("Missing key %d\n", i);
                bValid = 0; // missing a key definition
            }
        }
        if (iP1Control == CONTROLLER_GPIO || iP2Control == CONTROLLER_GPIO)
        {
            if (iButtonPins[i] == 0)
            {
                printf("Missing gpio %d\n", i);
                bValid = 0; // missing a GPIO definition
            }
        }
        if ((iP1Control == CONTROLLER_JOY0 || iP1Control == CONTROLLER_JOY1 || iP2Control == CONTROLLER_JOY0 || iP2Control == CONTROLLER_JOY1) && i > CTRL_RIGHT)
        {
            if (iButtonGP[i] == -1)
            {
                printf("missing gamepad %d\n", i);
                bValid = 0; // missing gamepad definition
            }
        }
    } // for each control definition
    // Map Gamepad buttons to SmartGear events
    memset(iButtonGPMap, 0, sizeof(iButtonGPMap));
    for (i=0; i<16; i++) // possible GP buttons
    {
        for (j=CTRL_EXIT; j<CTRL_COUNT; j++)
        {
            if (iButtonGP[j] == i) // map the button
                iButtonGPMap[i] = iButtonMapping[j]; 
        } // for j
    } // for i
    if (iDisplayType == -1)
    {
        printf("Missing or invalid display type\n");
        return -1;
    }
    if (iDisplayType == DISPLAY_LCD)
    {
        if (iLCDType == -1)
        {
            printf("Missing or invalid LCD type\n");
            return -1;
        }
        if (iSPIChan == -1)
        {
            printf("Missing or invalid SPI channel\n");
            return -1;
        }
        if (iSPIFreq < 1000000 || iSPIFreq > 125000000)
        {
            printf("Missing or invalid SPI speed\n");
            return -1;
        }
    }
    else
    {
       iLCDX = 320; iLCDY = 320; // framebuffer
    }
    if (bVerbose)
    {
        printf("config file read successfully\n");
        printf("SPI Channel=%d, SPI Speed=%d, dc=%d, rst=%d, led=%d\n", iSPIChan, iSPIFreq, iDC, iReset, iLED);
        if (!bValid) printf("Incomplete keyboard, gamepad or GPIO control definitions\n");
    }
    
    return (bValid)? 0:-1;
    
} /* SG_ParseConfigFile() */

