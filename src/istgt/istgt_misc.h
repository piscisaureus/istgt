/*
 * Copyright (C) 2008-2014 Daisuke Aoyama <aoyama@peach.ne.jp>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef ISTGT_MISC_H
#define ISTGT_MISC_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define DSET8(B, D) (*((uint8_t*) (B)) = (uint8_t)(D))
#define DSET16(B, D)                                          \
  (((*((uint8_t*) (B) + 0)) = (uint8_t)((uint16_t)(D) >> 8)), \
   ((*((uint8_t*) (B) + 1)) = (uint8_t)((uint16_t)(D) >> 0)))
#define DSET24(B, D)                                           \
  (((*((uint8_t*) (B) + 0)) = (uint8_t)((uint32_t)(D) >> 16)), \
   ((*((uint8_t*) (B) + 1)) = (uint8_t)((uint32_t)(D) >> 8)),  \
   ((*((uint8_t*) (B) + 2)) = (uint8_t)((uint32_t)(D) >> 0)))
#define DSET32(B, D)                                           \
  (((*((uint8_t*) (B) + 0)) = (uint8_t)((uint32_t)(D) >> 24)), \
   ((*((uint8_t*) (B) + 1)) = (uint8_t)((uint32_t)(D) >> 16)), \
   ((*((uint8_t*) (B) + 2)) = (uint8_t)((uint32_t)(D) >> 8)),  \
   ((*((uint8_t*) (B) + 3)) = (uint8_t)((uint32_t)(D) >> 0)))
#define DSET48(B, D)                                           \
  (((*((uint8_t*) (B) + 0)) = (uint8_t)((uint64_t)(D) >> 40)), \
   ((*((uint8_t*) (B) + 1)) = (uint8_t)((uint64_t)(D) >> 32)), \
   ((*((uint8_t*) (B) + 2)) = (uint8_t)((uint64_t)(D) >> 24)), \
   ((*((uint8_t*) (B) + 3)) = (uint8_t)((uint64_t)(D) >> 16)), \
   ((*((uint8_t*) (B) + 4)) = (uint8_t)((uint64_t)(D) >> 8)),  \
   ((*((uint8_t*) (B) + 5)) = (uint8_t)((uint64_t)(D) >> 0)))
#define DSET64(B, D)                                           \
  (((*((uint8_t*) (B) + 0)) = (uint8_t)((uint64_t)(D) >> 56)), \
   ((*((uint8_t*) (B) + 1)) = (uint8_t)((uint64_t)(D) >> 48)), \
   ((*((uint8_t*) (B) + 2)) = (uint8_t)((uint64_t)(D) >> 40)), \
   ((*((uint8_t*) (B) + 3)) = (uint8_t)((uint64_t)(D) >> 32)), \
   ((*((uint8_t*) (B) + 4)) = (uint8_t)((uint64_t)(D) >> 24)), \
   ((*((uint8_t*) (B) + 5)) = (uint8_t)((uint64_t)(D) >> 16)), \
   ((*((uint8_t*) (B) + 6)) = (uint8_t)((uint64_t)(D) >> 8)),  \
   ((*((uint8_t*) (B) + 7)) = (uint8_t)((uint64_t)(D) >> 0)))
#define DGET8(B) (*((uint8_t*) (B)))
#define DGET16(B)                               \
  ((((uint16_t) * ((uint8_t*) (B) + 0)) << 8) | \
   (((uint16_t) * ((uint8_t*) (B) + 1)) << 0))
#define DGET24(B)                                \
  ((((uint32_t) * ((uint8_t*) (B) + 0)) << 16) | \
   (((uint32_t) * ((uint8_t*) (B) + 1)) << 8) |  \
   (((uint32_t) * ((uint8_t*) (B) + 2)) << 0))
#define DGET32(B)                                \
  ((((uint32_t) * ((uint8_t*) (B) + 0)) << 24) | \
   (((uint32_t) * ((uint8_t*) (B) + 1)) << 16) | \
   (((uint32_t) * ((uint8_t*) (B) + 2)) << 8) |  \
   (((uint32_t) * ((uint8_t*) (B) + 3)) << 0))
#define DGET48(B)                                \
  ((((uint64_t) * ((uint8_t*) (B) + 0)) << 40) | \
   (((uint64_t) * ((uint8_t*) (B) + 1)) << 32) | \
   (((uint64_t) * ((uint8_t*) (B) + 2)) << 24) | \
   (((uint64_t) * ((uint8_t*) (B) + 3)) << 16) | \
   (((uint64_t) * ((uint8_t*) (B) + 4)) << 8) |  \
   (((uint64_t) * ((uint8_t*) (B) + 5)) << 0))
#define DGET64(B)                                \
  ((((uint64_t) * ((uint8_t*) (B) + 0)) << 56) | \
   (((uint64_t) * ((uint8_t*) (B) + 1)) << 48) | \
   (((uint64_t) * ((uint8_t*) (B) + 2)) << 40) | \
   (((uint64_t) * ((uint8_t*) (B) + 3)) << 32) | \
   (((uint64_t) * ((uint8_t*) (B) + 4)) << 24) | \
   (((uint64_t) * ((uint8_t*) (B) + 5)) << 16) | \
   (((uint64_t) * ((uint8_t*) (B) + 6)) << 8) |  \
   (((uint64_t) * ((uint8_t*) (B) + 7)) << 0))

#define DMIN8(A, B) ((uint8_t)((A) > (B) ? (B) : (A)))
#define DMIN16(A, B) ((uint16_t)((A) > (B) ? (B) : (A)))
#define DMIN24(A, B) ((uint32_t)((A) > (B) ? (B) : (A)))
#define DMIN32(A, B) ((uint32_t)((A) > (B) ? (B) : (A)))
#define DMIN48(A, B) ((uint64_t)((A) > (B) ? (B) : (A)))
#define DMIN64(A, B) ((uint64_t)((A) > (B) ? (B) : (A)))
#define DMAX8(A, B) ((uint8_t)((A) > (B) ? (A) : (B)))
#define DMAX16(A, B) ((uint16_t)((A) > (B) ? (A) : (B)))
#define DMAX24(A, B) ((uint32_t)((A) > (B) ? (A) : (B)))
#define DMAX32(A, B) ((uint32_t)((A) > (B) ? (A) : (B)))
#define DMAX48(A, B) ((uint64_t)((A) > (B) ? (A) : (B)))
#define DMAX64(A, B) ((uint64_t)((A) > (B) ? (A) : (B)))

#define BSHIFTNW(N, W) \
  (((W) > 0) ? (((N) > ((W) -1)) ? ((N) - ((W) -1)) : 0) : 0)
#define BMASKW(W) (((W) > 0) ? (~((~0) << (W))) : 0)

#define BDSET8W(B, D, N, W) \
  DSET8((B), (((D) &BMASKW((W))) << BSHIFTNW((N), (W))))
#define BDADD8W(B, D, N, W)                                    \
  DSET8((B),                                                   \
        ((DGET8((B)) & ~(BMASKW((W)) << BSHIFTNW((N), (W)))) | \
         (uint8_t)(((D) &BMASKW((W))) << BSHIFTNW((N), (W)))))
#define BSET8W(B, N, W) \
  (*((uint8_t*) (B)) |= (uint8_t)(BMASKW((W))) << BSHIFTNW((N), (W)))
#define BCLR8W(B, N, W) \
  (*((uint8_t*) (B)) &= (uint8_t)(~(BMASKW((W))) << BSHIFTNW((N), (W))))
#define BGET8W(B, N, W) \
  ((*((uint8_t*) (B)) >> BSHIFTNW((N), (W))) & BMASKW((W)))

#define BDSET8(B, D, N) (BDSET8W((B), (D), (N), 1))
#define BDADD8(B, D, N) (BDADD8W((B), (D), (N), 1))
#define BSET8(B, N) (BSET8W((B), (N), 1))
#define BCLR8(B, N) (BCLR8W((B), (N), 1))
#define BGET8(B, N) (BGET8W((B), (N), 1))

#define UNUSED(V) ((void) (V))

#define STATIC_ASSERT(condition, message) \
  typedef char static_assertion_##__FILE__##_##__LINE__[(condition) ? 1 : -1]

#ifdef _WIN32
_declspec(noreturn)
#else  // _WIN32
__attribute__((__noreturn__, __format__(__printf__, 1, 2)))
#endif
    void istgt_fatal(const char* format, ...);

/* memory allocate */
void* xmalloc(size_t size);
void* xrealloc(void* p, size_t size);
void xfree(void* p);
char* xstrdup(const char* s);

/* string functions */
#ifndef _WIN32
char* strlwr(char* s);
char* strupr(char* s);
#endif /* _WIN32 */
#ifdef _WIN32
char* strsep(char** stringp, const char* delim);
#endif
char* strsepq(char** stringp, const char* delim);
char* trim_string(char* s);
char* escape_string(const char* s);
#ifndef HAVE_STRLCPY
size_t strlcpy(char* dst, const char* src, size_t size);
#endif /* HAVE_STRLCPY */

/* random value generation */
void istgt_gen_random(uint8_t* buf, size_t len);
#ifndef HAVE_SRANDOMDEV
void srandomdev(void);
#endif /* HAVE_SRANDOMDEV */
#ifndef HAVE_ARC4RANDOM
uint32_t arc4random(void);
#endif /* HAVE_ARC4RANDOM */

/* convert from/to bin/hex */
int istgt_bin2hex(char* buf, size_t len, const uint8_t* data, size_t data_len);
int istgt_hex2bin(uint8_t* data, size_t data_len, const char* str);

/* other functions */
int istgt_difftime(time_t a, time_t b);
void istgt_dump(const char* label, const uint8_t* buf, size_t len);
void istgt_fdump(FILE* fp, const char* label, const uint8_t* buf, size_t len);

#endif /* ISTGT_MISC_H */
