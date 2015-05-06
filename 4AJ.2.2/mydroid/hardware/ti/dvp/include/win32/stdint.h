/*
 *  Copyright (C) 2009-2011 Texas Instruments, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This is a compatibilty file for Windows Builds ONLY

#if (defined(WIN32) || defined(UNDER_CE)) && !defined(CYGWIN)

#include <windows.h>

#ifndef __attribute__
#define __attribute__(x)
#endif

#ifndef unused
#define unused
#endif

#ifndef restrict
#define restrict
#endif

#ifndef _STDINT_H // this is the Standard C99 header define lock
#define _STDINT_H

typedef   signed char       int8_t;
typedef   signed short      int16_t;
typedef   signed int        int32_t;
typedef   signed long long  int64_t;
typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

#endif

#endif

