/*==========================================================================
  Copyright (c) 2010, Code Aurora Forum. All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided with the distribution.
  * Neither the name of Code Aurora Forum, Inc. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
    WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
    OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
    IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
==========================================================================*/

/* IDCT routines */
EXTERN (void) idct_1x1_venum (INT16 * coeffPtr, INT16 * samplePtr, INT32 stride);
EXTERN (void) idct_2x2_venum (INT16 * coeffPtr, INT16 * samplePtr, INT32 stride);
EXTERN (void) idct_4x4_venum (INT16 * coeffPtr, INT16 * samplePtr, INT32 stride);
EXTERN (void) idct_8x8_venum (INT16 * coeffPtr, INT16 * samplePtr, INT32 stride);

/* Color conversion routines */
EXTERN (void) yvup2rgb565_venum (UINT8 *pLumaLine,
                 UINT8 *pCrLine,
                 UINT8 *pCbLine,
                 UINT8 *pRGB565Line,
                 JDIMENSION nLineWidth);
EXTERN (void) yyvup2rgb565_venum (UINT8 * pLumaLine,
                  UINT8 *pCrLine,
                  UINT8 *pCbLine,
                  UINT8 * pRGB565Line,
                  JDIMENSION nLineWidth);
EXTERN (void) yvup2bgr888_venum (UINT8 * pLumaLine,
                 UINT8 *pCrLine,
                 UINT8 *pCbLine,
                 UINT8 * pBGR888Line,
                 JDIMENSION nLineWidth);
EXTERN (void) yyvup2bgr888_venum (UINT8 * pLumaLine,
                  UINT8 *pCrLine,
                  UINT8 *pCbLine,
                  UINT8 * pBGR888Line,
                  JDIMENSION nLineWidth);
