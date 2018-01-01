 /***************************************************************************
 *                                                                          *
 * MODULE:  EMUIO.C                                                         *
 *                                                                          *
 * DESCRIPTION: Generic I/O & Memory                                        *
 *                                                                          *
 * FUNCTIONS:                                                               *
 *            EMUOpen - Open a file for reading or writing                  *
 *            EMUCreate - Create a file for writing                         *
 *            EMUDelete - Delete a file                                     *
 *            EMUClose - Close a file                                       *
 *            EMUDelete - Delete a file                                     *
 *            EMURead - Read a block of data from a file                    *
 *            EMUWrite - write a block of data to a file                    *
 *            EMUSeek - Seek to a specific section in a file                *
 *            EMUAlloc - Allocate a block of memory                         *
 *            EMUFree - Free a block of memory                              *
 * COMMENTS:                                                                *
 *            Created 10/14/99 - Larry Bank                                 *
 *            Copyright (c) 2000-2017 BitBank Software, Inc.                *
 *            Updated for Android NDK 11/10/10                              *
 ****************************************************************************/
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
//#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//#define LOG_MEM
#ifdef LOG_MEM
static int iMemTotal = 0;
#endif // LOG_MEM
//int rand(void)
//{
//	return (int)lrand48();
//} /* rand() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUOpenRO(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading only.                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * EMUOpenRO(char * fname)
{
   void * ihandle;

   ihandle = (void *)fopen(fname, "rb");

   if (ihandle == 0)
   {
      return (void *)-1;
   }

   return ihandle;

} /* EMUOpenRO() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUOpen(char *)                                            *
 *                                                                          *
 *  PURPOSE    : Opens a file for reading or writing                        *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * EMUOpen(char * fname)
{
   void * ihandle;

   ihandle = (void *)fopen(fname, "r+b");
   if (ihandle == NULL)
      ihandle = EMUOpenRO(fname); /* Try readonly */
   return ihandle;

} /* EMUOpen() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUCreate(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Creates and opens a file for writing                       *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : Handle to file if successful, -1 if failure                *
 *                                                                          *
 ****************************************************************************/
void * EMUCreate(char * fname)
{
void * ohandle;

   ohandle = (void *)fopen(fname, "w+b");
   if (ohandle == NULL) // NULL means failure
      ohandle = (void *)-1;

   return ohandle;

} /* EMUCreate() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUSeek(void *, signed long, int)                          *
 *                                                                          *
 *  PURPOSE    : Seeks within an open file                                  *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Offset                                                     *
 *               Method - 0=from beginning, 1=from current spot, 2=from end *
 *                                                                          *
 *  RETURNS    : New offset within file.                                    *
 *                                                                          *
 ****************************************************************************/
uint32_t EMUSeek(void * iHandle, signed long lOffset, int iMethod)
{
   int iType;
   uint32_t ulNewPos;

   if (iMethod == 0) iType = SEEK_SET;
   else if (iMethod == 1) iType = SEEK_CUR;
   else iType = SEEK_END;

   fseek((FILE *)iHandle, lOffset, iType);
   ulNewPos = (uint32_t)ftell((FILE *)iHandle);

   return ulNewPos;

} /* EMUSeek() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUDelete(char *)                                          *
 *                                                                          *
 *  PURPOSE    : Delete a file.                                             *
 *                                                                          *
 *  PARAMETERS : filename                                                   *
 *                                                                          *
 *  RETURNS    : 0 if successful, -1 if failure.                            *
 *                                                                          *
 ****************************************************************************/
int EMUDelete(char *szFile)
{
   if (remove(szFile))
      return 0; // success
   else
      return -1;
} /* EMUDelete() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMURead(void *, void *, int)                               *
 *                                                                          *
 *  PURPOSE    : Read a block from an open file                             *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to read                                    *
 *                                                                          *
 *  RETURNS    : Number of bytes read                                       *
 *                                                                          *
 ****************************************************************************/
unsigned int EMURead(void * iHandle, void * lpBuff, unsigned int iNumBytes)
{
   unsigned int iBytes;

   iBytes = (int)fread(lpBuff, 1, iNumBytes, (FILE *)iHandle);
   return iBytes;

} /* EMURead() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUWrite(void *, void *, int)                              *
 *                                                                          *
 *  PURPOSE    : Write a block from an open file                            *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *               Buffer pointer                                             *
 *               Number of bytes to write                                   *
 *                                                                          *
 *  RETURNS    : Number of bytes written                                    *
 *                                                                          *
 ****************************************************************************/
unsigned int EMUWrite(void * iHandle, void * lpBuff, unsigned int iNumBytes)
{
   unsigned int iBytes;

   iBytes = (unsigned int)fwrite(lpBuff, 1, iNumBytes, (FILE *)iHandle);
   return iBytes;

} /* EMUWrite() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUClose(void *)                                           *
 *                                                                          *
 *  PURPOSE    : Close a file                                               *
 *                                                                          *
 *  PARAMETERS : File Handle                                                *
 *                                                                          *
 *  RETURNS    : NOTHING                                                    *
 *                                                                          *
 ****************************************************************************/
void EMUClose(void * iHandle)
{
   if (iHandle == NULL || iHandle == (void *)-1)
	   return; // system crashes if you try to close these handles
   fflush((FILE *)iHandle);
   fclose((FILE *)iHandle);

} /* EMUClose() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUAlloc(long)                                             *
 *                                                                          *
 *  PURPOSE    : Allocate a 16-byte aligned block of writable memory.       *
 *                                                                          *
 ****************************************************************************/
void * EMUAlloc(int size)
{
void *p = NULL;
unsigned long i;

   if (size == 0)
   {
      return NULL; // Linux seems to return a non-NULL pointer for 0 size
   }
   i = (unsigned long)malloc(size+4);
   if (i)
   {
   uint32_t *pul = (uint32_t *)i;
      p = (void *)(i+4);
      *pul = (uint32_t) size;
      memset(p, 0, size);
#ifdef LOG_MEM
      iMemTotal += (int)size;
      printf("alloc %08x bytes at %08lx, new total = %08x\n", (int)size, (long)p, iMemTotal);
#endif // LOG_MEM
   }
   return p;

} /* EMUAlloc() */

/****************************************************************************
 *                                                                          *
 *  FUNCTION   : EMUFree(void *)                                            *
 *                                                                          *
 *  PURPOSE    : Free a block of writable memory.                           *
 *                                                                          *
 ****************************************************************************/
void EMUFree(void *p)
{
uint32_t *pul = (uint32_t *)p;

   if (p == NULL || p == (void *)-1)
       return; /* Don't try to free bogus pointer */
#ifdef LOG_MEM
        iMemTotal -= pul[-1];
	printf("free %08x bytes at %08lx, new total = %08x\n", (int)pul[-1], (long)pul, iMemTotal);
#endif // LOG_MEM
   free((void *)&pul[-1]);

} /* EMUFree() */
