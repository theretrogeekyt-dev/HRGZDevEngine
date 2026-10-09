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
#if defined(__BIG_ENDIAN__) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || \
    defined(_BIG_ENDIAN) || \
    defined(__powerpc__) || defined(__ppc__) || defined(__PPC__) || \
    defined(_XBOX) || defined(LIBXENON)
#ifndef DOOM_BIG_ENDIAN
#define DOOM_BIG_ENDIAN 1
#endif
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
