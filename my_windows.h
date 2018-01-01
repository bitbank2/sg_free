//
// my_windows.h
//
// created 11/09/2004 to allow my emulators to function in non-Windows environments
// recreates the required Windows structures and definitions
//
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

#ifndef _WIN32_WCE
#define stricmp strcasecmp
#endif
typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef long LONG;
typedef uint32_t DWORD;

typedef struct rect_tag
{
int top, left, bottom, right;
} RECT;

#ifdef  UNICODE
#define TEXT(quote) L##quote
#else // ASCII
#define TEXT(quote) quote
#endif // UNICODE

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* Define NULL pointer value */
#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

typedef struct _LARGE_INTEGER {
DWORD LowPart;
LONG HighPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

//typedef union _LARGE_INTEGER { 
//  struct {
//      DWORD LowPart; 
//      LONG  HighPart; 
//  };
//  LONGLONG QuadPart;
//} LARGE_INTEGER, *PLARGE_INTEGER;

typedef unsigned int HANDLE;

typedef HANDLE HWND;

//#define WINAPI __stdcall
//#define APIENTRY WINAPI
#define APIENTRY
#define _cdecl

typedef struct tagRGBQUAD { 
BYTE rgbBlue;
BYTE rgbGreen;
BYTE rgbRed;
BYTE rgbReserved;} 
RGBQUAD;

typedef struct tagPALETTEENTRY { 
  BYTE peRed; 
  BYTE peGreen; 
  BYTE peBlue; 
  BYTE peFlags; 
} PALETTEENTRY;

#define PC_RESERVED     0x01    /* palette index used for animation */

#define RGB(a,b,c) (a<<16) | (b<<8) | (c)
