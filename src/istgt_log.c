/*
 * Copyright (C) 2008-2010 Daisuke Aoyama <aoyama@peach.ne.jp>.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "istgt_core.h"
#include "istgt_log.h"
#include "istgt_misc.h"
#include "istgt_platform.h"

// static int g_trace_flag = 0;
int g_trace_flag = 0;
int g_warn_flag = 1;


void istgt_log(const char* file,
               const int line,
               const char* func,
               const char* format,
               ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  if (file != NULL) {
    if (func != NULL) {
      fprintf(stderr, "%s:%4d:%s: %s", file, line, func, buf);
    } else {
      fprintf(stderr, "%s:%4d: %s", file, line, buf);
    }
  } else {
    fprintf(stderr, "%s", buf);
  }
  va_end(ap);
}

void istgt_noticelog(const char* file,
                     const int line,
                     const char* func,
                     const char* format,
                     ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  if (file != NULL) {
    if (func != NULL) {
      fprintf(stderr, "%s:%4d:%s: %s", file, line, func, buf);
    } else {
      fprintf(stderr, "%s:%4d: %s", file, line, buf);
    }
  } else {
    fprintf(stderr, "%s", buf);
  }
  va_end(ap);
}

void istgt_tracelog(const int flag,
                    const char* file,
                    const int line,
                    const char* func,
                    const char* format,
                    ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  if (g_trace_flag & flag) {
    vsnprintf(buf, sizeof buf, format, ap);
    if (func != NULL) {
      fprintf(stderr, "%s:%4d:%s: %s", file, line, func, buf);
      // syslog(LOG_INFO, "%s:%4d:%s: %s", file, line, func, buf);
    } else {
      fprintf(stderr, "%s:%4d: %s", file, line, buf);
      // syslog(LOG_INFO, "%s:%4d: %s", file, line, buf);
    }
  }
  va_end(ap);
}

void istgt_errlog(const char* file,
                  const int line,
                  const char* func,
                  const char* format,
                  ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  if (func != NULL) {
    fprintf(stderr, "%s:%4d:%s: ***ERROR*** %s", file, line, func, buf);
  } else {
    fprintf(stderr, "%s:%4d: ***ERROR*** %s", file, line, buf);
  }
  va_end(ap);
}

void istgt_warnlog(const char* file,
                   const int line,
                   const char* func,
                   const char* format,
                   ...) {
  char buf[MAX_TMPBUF];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof buf, format, ap);
  if (func != NULL) {
    fprintf(stderr, "%s:%4d:%s: ***WARNING*** %s", file, line, func, buf);
  } else {
    fprintf(stderr, "%s:%4d: ***WARNING*** %s", file, line, buf);
  }
  va_end(ap);
}

void istgt_set_trace_flag(int flag) {
  if (flag == ISTGT_TRACE_NONE) {
    g_trace_flag = 0;
  } else {
    g_trace_flag |= flag;
  }
}

void istgt_trace_dump(int flag,
                      const char* label,
                      const uint8_t* buf,
                      size_t len) {
  if (g_trace_flag & flag) {
    istgt_fdump(stderr, label, buf, len);
  }
}
