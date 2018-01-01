/************************************************************/
/*--- Generic I/O and memory routines                    ---*/
/* Copyright 1999-2017, BitBank Software, Inc.              */
/* 11/10/10 - updated for Android NDK                       */
/************************************************************/
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

void * EMUOpen(char *);
void * EMUOpenRO(char *);
void * EMUCreate(char *);
uint32_t EMUSeek(void *, signed long, int);
unsigned int EMURead(void *, void *, unsigned int);
unsigned int EMUWrite(void *, void *, unsigned int);
void EMUClose(void *);
int EMUDelete(char *szFile);
void * EMUAlloc(int);
void EMUFree(void *);
