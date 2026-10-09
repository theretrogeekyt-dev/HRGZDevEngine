// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Endianess handling, swapping 16bit and 32bit.
//
//-----------------------------------------------------------------------------


#ifndef __M_SWAP__
#define __M_SWAP__


#ifdef __GNUG__
#pragma interface
#endif


// Endianess handling.
// WAD files are stored little endian.
#undef DOOM_BIG_ENDIAN

#if defined(PSP) || defined(__PSP__) || defined(__MIPSEL__) || defined(_MIPSEL) || \
    defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))
    // Explicit little-endian platform (PSP Allegrex MIPS is little-endian)
#elif defined(_XENON) || (defined(__powerpc__) && !defined(__LITTLE_ENDIAN__)) || \
      (defined(__ppc__) && !defined(__LITTLE_ENDIAN__)) || \
      (defined(__PPC__) && !defined(__LITTLE_ENDIAN__))
    #define DOOM_BIG_ENDIAN 1
#elif (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
    #define DOOM_BIG_ENDIAN 1
#elif (defined(_BYTE_ORDER) && defined(_BIG_ENDIAN) && (_BYTE_ORDER == _BIG_ENDIAN))
    #define DOOM_BIG_ENDIAN 1
#elif (defined(BYTE_ORDER) && defined(BIG_ENDIAN) && (BYTE_ORDER == BIG_ENDIAN))
    #define DOOM_BIG_ENDIAN 1
#endif

short	SwapSHORT(short);
long	SwapLONG(long);

#ifdef DOOM_BIG_ENDIAN
#define SHORT(x)	((short)SwapSHORT((short)(x)))
#define LONG(x)         ((long)SwapLONG((long)(x)))
#else
#define SHORT(x)	((short)(x))
#define LONG(x)         ((int)(x))
#endif





#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
