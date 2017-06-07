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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdint.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "istgt.h"
#include "istgt_misc.h"
#include "istgt_platform.h"

static void fatal(const char* format, ...)
    __attribute__((__noreturn__, __format__(__printf__, 1, 2)));

static void fatal(const char* format, ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  fprintf(stderr, "%s", buf);
  va_end(ap);
  exit(EXIT_FAILURE);
}

void* xmalloc(size_t size) {
  void* p;

  if (size < 1)
    size = 1;
  p = malloc(size);
  if (p == NULL)
    fatal("no memory\n");
  return p;
}

void* xrealloc(void* p, size_t size) {
  if (size < 1)
    size = 1;
  if (p == NULL) {
    p = malloc(size);
  } else {
    p = realloc(p, size);
  }
  if (p == NULL)
    fatal("no memory\n");
  return p;
}

void xfree(void* p) {
  if (p == NULL)
    return;
  free(p);
}

char* xstrdup(const char* s) {
  char* p;
  size_t size;

  if (s == NULL)
    return NULL;
  size = strlen(s) + 1;
  p = xmalloc(size);
  memcpy(p, s, size - 1);
  p[size - 1] = '\0';
  return p;
}

#ifndef _WIN32
char* strlwr(char* s) {
  char* p;

  if (s == NULL)
    return NULL;

  p = s;
  while (*p != '\0') {
    *p = tolower((int) *p);
    p++;
  }
  return s;
}

char* strupr(char* s) {
  char* p;

  if (s == NULL)
    return NULL;

  p = s;
  while (*p != '\0') {
    *p = toupper((int) *p);
    p++;
  }
  return s;
}
#endif

#ifdef _WIN32
char* strsep(char** stringp, const char* delim) {
  char* start = *stringp;
  char* token;

  if (start == NULL)
    return NULL;

  token = strpbrk(start, delim);
  if (token != NULL) {
    *stringp = NULL;
  } else {
    *token = '\0';
    *stringp = token + 1;
  }

  return start;
}
#endif  // _WIN32

char* strsepq(char** stringp, const char* delim) {
  char *p, *q, *r;
  int quoted = 0, bslash = 0;

  p = *stringp;
  if (p == NULL)
    return NULL;

  r = q = p;
  while (*q != '\0' && *q != '\n') {
    /* eat quoted characters */
    if (bslash) {
      bslash = 0;
      *r++ = *q++;
      continue;
    } else if (quoted) {
      if (quoted == '"' && *q == '\\') {
        bslash = 1;
        q++;
        continue;
      } else if (*q == quoted) {
        quoted = 0;
        q++;
        continue;
      }
      *r++ = *q++;
      continue;
    } else if (*q == '\\') {
      bslash = 1;
      q++;
      continue;
    } else if (*q == '"' || *q == '\'') {
      quoted = *q;
      q++;
      continue;
    }

    /* separator? */
    if (strchr(delim, (int) *q) == NULL) {
      *r++ = *q++;
      continue;
    }

    /* new string */
    q++;
    break;
  }
  *r = '\0';

  /* skip tailer */
  while (*q != '\0' && strchr(delim, (int) *q) != NULL) {
    q++;
  }
  if (*q != '\0') {
    *stringp = q;
  } else {
    *stringp = NULL;
  }

  return p;
}

char* trim_string(char* s) {
  char *p, *q;

  if (s == NULL)
    return NULL;

  /* remove header */
  p = s;
  while (*p != '\0' && isspace((int) *p)) {
    p++;
  }
  /* remove tailer */
  q = p + strlen(p);
  while (q - 1 >= p && isspace((int) *(q - 1))) {
    q--;
    *q = '\0';
  }
  /* if remove header, move */
  if (p != s) {
    q = s;
    while (*p != '\0') {
      *q++ = *p++;
    }
  }
  return s;
}

char* escape_string(const char* s) {
  const char* p;
  char *q, *r;
  size_t size;

  if (s == NULL)
    return NULL;

  p = s;
  size = 0;
  while (*p != '\0') {
    if (*p == '"' || *p == '\\' || *p == '\'') {
      size += 2;
    } else {
      size++;
    }
    p++;
  }

  p = s;
  r = q = xmalloc(size + 1);
  while (*p != '\0') {
    if (*p == '"' || *p == '\\' || *p == '\'') {
      *q++ = '\\';
      *q++ = *p++;
    } else {
      *q++ = *p++;
    }
  }
  *q++ = '\0';
  return r;
}

int istgt_difftime(time_t a, time_t b) {
  double d;
  /* don't want floating-point format */
  d = difftime(a, b);
  return (int) d;
}

void istgt_dump(const char* label, const uint8_t* buf, size_t len) {
  istgt_fdump(stdout, label, buf, len);
}

void istgt_fdump(FILE* fp, const char* label, const uint8_t* buf, size_t len) {
  char tmpbuf[MAX_TMPBUF];
  char buf8[8 + 1];
  size_t total;
  size_t idx;

  fprintf(fp, "%s\n", label);

  memset(buf8, 0, sizeof buf8);
  total = 0;
  for (idx = 0; idx < len; idx++) {
    if (idx != 0 && idx % 8 == 0) {
      total += snprintf(tmpbuf + total, sizeof tmpbuf - total, "%s", buf8);
      fprintf(fp, "%s\n", tmpbuf);
      total = 0;
    }
    total += snprintf(
        tmpbuf + total, sizeof tmpbuf - total, "%2.2x ", buf[idx] & 0xff);
    buf8[idx % 8] = isprint(buf[idx]) ? buf[idx] : '.';
  }
  for (; idx % 8 != 0; idx++) {
    total += snprintf(tmpbuf + total, sizeof tmpbuf - total, "   ");
    buf8[idx % 8] = ' ';
  }
  total += snprintf(tmpbuf + total, sizeof tmpbuf - total, "%s", buf8);
  fprintf(fp, "%s\n", tmpbuf);
  fflush(fp);
}

#ifndef HAVE_SRANDOMDEV
#include <time.h>
void srandomdev(void) {
  unsigned long seed;
  time_t now;
  pid_t pid;

  pid = getpid();
  now = time(NULL);
  seed = pid ^ now;
  srandom(seed);
}
#endif /* HAVE_SRANDOMDEV */

#ifndef HAVE_ARC4RANDOM
static int istgt_arc4random_initialized = 0;

uint32_t arc4random(void) {
  uint32_t r;
  uint32_t r1, r2;

  if (!istgt_arc4random_initialized) {
    srandomdev();
    istgt_arc4random_initialized = 1;
  }
  r1 = (uint32_t)(random() & 0xffff);
  r2 = (uint32_t)(random() & 0xffff);
  r = (r1 << 16) | r2;
  return r;
}
#endif /* HAVE_ARC4RANDOM */

void istgt_gen_random(uint8_t* buf, size_t len) {
#ifdef USE_RANDOM
  long l;
  size_t idx;

  srandomdev();
  for (idx = 0; idx < len; idx++) {
    l = random();
    buf[idx] = (uint8_t) l;
  }
#else
  uint32_t r;
  size_t idx;

  for (idx = 0; idx < len; idx++) {
    r = arc4random();
    buf[idx] = (uint8_t) r;
  }
#endif /* USE_RANDOM */
}

int istgt_bin2hex(char* buf,
                  size_t len,
                  const uint8_t* data,
                  size_t data_len) {
  const char* digits = "0123456789ABCDEF";
  size_t total = 0;
  size_t idx;

  if (len < 3)
    return -1;
  buf[total] = '0';
  total++;
  buf[total] = 'x';
  total++;
  buf[total] = '\0';

  for (idx = 0; idx < data_len; idx++) {
    if (total + 3 > len) {
      buf[total] = '\0';
      return -1;
    }
    buf[total] = digits[(data[idx] >> 4) & 0x0fU];
    total++;
    buf[total] = digits[data[idx] & 0x0fU];
    total++;
  }
  buf[total] = '\0';
  return total;
}

int istgt_hex2bin(uint8_t* data, size_t data_len, const char* str) {
  const char* digits = "0123456789ABCDEF";
  const char* dp;
  const char* p;
  size_t total = 0;
  int n0, n1;

  p = str;
  if (p[0] != '0' && (p[1] != 'x' && p[1] != 'X'))
    return -1;
  p += 2;

  while (p[0] != '\0' && p[1] != '\0') {
    if (total >= data_len) {
      return -1;
    }
    dp = strchr(digits, toupper((int) p[0]));
    if (dp == NULL) {
      return -1;
    }
    n0 = (int) (dp - digits);
    dp = strchr(digits, toupper((int) p[1]));
    if (dp == NULL) {
      return -1;
    }
    n1 = (int) (dp - digits);

    data[total] = (uint8_t)(((n0 & 0x0fU) << 4) | (n1 & 0x0fU));
    total++;
    p += 2;
  }
  return total;
}

#ifndef HAVE_STRLCPY
size_t strlcpy(char* dst, const char* src, size_t size) {
  size_t len;

  if (dst == NULL)
    return 0;
  if (size < 1) {
    return 0;
  }
  len = strlen(src);
  if (len > size - 1) {
    len = size - 1;
  }
  memcpy(dst, src, len);
  dst[len] = '\0';
  return len;
}
#endif /* HAVE_STRLCPY */
