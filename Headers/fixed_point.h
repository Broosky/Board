/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Name: Patchworks (C)                                                                                            //
// Author: Jeffrey Bednar                                                                                                  //
// Copyright (c) Illusion Interactive, 2011 - 2025.                                                                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _FIXED_POINT_H_
#define _FIXED_POINT_H_
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "common.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define FIXED_SHIFT 16
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Types:
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef int32_t fixed16_t; // 16.16: 32,767.9999847 to -32,768.0
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Function prototypes:
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
fixed16_t           toFixed                       (int32_t, uint8_t);
int32_t             fromFixed                     (fixed16_t, uint8_t);
fixed16_t           fixedMultiply                 (fixed16_t, fixed16_t, uint8_t);
fixed16_t           fixedDivide                   (fixed16_t, fixed16_t, uint8_t);
fixed16_t           extractComponentAsFixed       (fixed16_t, uint8_t);
uint32_t            extractFractionalAsNatural    (fixed16_t, uint8_t);
float               fixedToFloat                  (fixed16_t, uint8_t);
fixed16_t           fixedLog2                     (fixed16_t, uint8_t);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
