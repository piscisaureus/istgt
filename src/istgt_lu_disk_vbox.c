/*
 * Copyright (C) 2008-2012 Daisuke Aoyama <aoyama@peach.ne.jp>.
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>

#include "istgt.h"
#include "istgt_log.h"
#include "istgt_lu.h"
#include "istgt_misc.h"
#include "istgt_platform.h"
#include "istgt_proto.h"

int istgt_lu_disk_vbox_lun_init(ISTGT_LU_DISK* spec,
                                ISTGT_Ptr istgt,
                                ISTGT_LU_Ptr lu) {
  UNUSED(istgt);
  UNUSED(lu);

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "LU%d: LUN%d unsupported virtual disk\n",
                 spec->num,
                 spec->lun);
  return -1;
}

int istgt_lu_disk_vbox_lun_shutdown(ISTGT_LU_DISK* spec,
                                    ISTGT_Ptr istgt,
                                    ISTGT_LU_Ptr lu) {
  UNUSED(istgt);
  UNUSED(lu);

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "LU%d: LUN%d unsupported virtual disk\n",
                 spec->num,
                 spec->lun);
  return -1;
}
