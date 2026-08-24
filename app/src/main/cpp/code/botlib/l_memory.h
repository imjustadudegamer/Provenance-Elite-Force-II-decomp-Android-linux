/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// SPDX-License-Identifier: GPL-2.0-or-later



#ifdef MEMDEBUG
#define GetMemory(size)				GetMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedMemory(size)		GetClearedMemoryDebug(size, #size, __FILE__, __LINE__);
void *GetMemoryDebug(unsigned long size, char *label, char *file, int line);
void *GetClearedMemoryDebug(unsigned long size, char *label, char *file, int line);
#define GetHunkMemory(size)			GetHunkMemoryDebug(size, #size, __FILE__, __LINE__);
#define GetClearedHunkMemory(size)	GetClearedHunkMemoryDebug(size, #size, __FILE__, __LINE__);
void *GetHunkMemoryDebug(unsigned long size, char *label, char *file, int line);
void *GetClearedHunkMemoryDebug(unsigned long size, char *label, char *file, int line);
#else
void *GetMemory(unsigned long size);
void *GetClearedMemory(unsigned long size);
#ifdef BSPC
#define GetHunkMemory GetMemory
#define GetClearedHunkMemory GetClearedMemory
#else
void *GetHunkMemory(unsigned long size);
void *GetClearedHunkMemory(unsigned long size);
#endif
#endif

void FreeMemory(void *ptr);
int AvailableMemory(void);
void PrintUsedMemorySize(void);
void PrintMemoryLabels(void);
int MemoryByteSize(void *ptr);
void DumpMemory(void);
