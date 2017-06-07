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

#ifndef ISTGT_H
#define ISTGT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_ATOMIC
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_MACHINE_ATOMIC_H
#include <machine/atomic.h>
#endif
#ifdef HAVE_SYS_ATOMIC_H
#include <sys/atomic.h>
#endif
#endif /* USE_ATOMIC */

#include "istgt_conf.h"
#include "istgt_control_pipe.h"
#include "istgt_log.h"
#include "istgt_platform.h"

#if !defined(__GNUC__)
#undef __attribute__
#define __attribute__(x)
#endif
#if defined(__GNUC__) && defined(__GNUC_MINOR__)
#define ISTGT_GNUC_PREREQ(ma, mi) \
  (__GNUC__ > (ma) || (__GNUC__ == (ma) && __GNUC_MINOR__ >= (mi)))
#else
#define ISTGT_GNUC_PREREQ(ma, mi) 0
#endif

#define MAX_TMPBUF 1024
#define MAX_ADDRBUF 64
#define MAX_INITIATOR_ADDR (MAX_ADDRBUF)
#define MAX_INITIATOR_NAME 256
#define MAX_TARGET_ADDR (MAX_ADDRBUF)
#define MAX_TARGET_NAME 256
#define MAX_ISCSI_NAME 256

#define MAX_PORTAL 1024
#define MAX_INITIATOR 256
#define MAX_NETMASK 256
#define MAX_PORTAL_GROUP 4096
#define MAX_INITIATOR_GROUP 4096
#define MAX_LOGICAL_UNIT 4096
#define MAX_R2T 256

#define DEFAULT_NODEBASE "iqn.2007-09.jp.ne.peach.istgt"
#define DEFAULT_PORT 3260
#define DEFAULT_MAX_SESSIONS 32
#define DEFAULT_MAX_CONNECTIONS 4
#define DEFAULT_MAXOUTSTANDINGR2T 16
#define DEFAULT_DEFAULTTIME2WAIT 2
#define DEFAULT_DEFAULTTIME2RETAIN 20
#define DEFAULT_FIRSTBURSTLENGTH 65536
#define DEFAULT_MAXBURSTLENGTH 262144
#define DEFAULT_MAXRECVDATASEGMENTLENGTH 8192
#define DEFAULT_INITIALR2T 1
#define DEFAULT_IMMEDIATEDATA 1
#define DEFAULT_DATAPDUINORDER 1
#define DEFAULT_DATASEQUENCEINORDER 1
#define DEFAULT_ERRORRECOVERYLEVEL 0
#define DEFAULT_TIMEOUT 60
#define DEFAULT_NOPININTERVAL 20
#define DEFAULT_MAXR2T 16

#define ISTGT_PG_TAG_MAX 0x0000ffff
#define ISTGT_LU_TAG_MAX 0x0000ffff
#define ISTGT_UC_TAG 0x00010000
#define ISTGT_IOBUFSIZE (2 * 1024 * 1024)
#define ISTGT_SNSBUFSIZE 4096
#define ISTGT_SHORTDATASIZE 8192
#define ISTGT_SHORTPDUSIZE (48 + 4 + ISTGT_SHORTDATASIZE + 4)
#define ISTGT_CONDWAIT (50 * 1000)    /* ms */
#define ISTGT_CONDWAIT_MIN (5 * 1000) /* ms */

#define MTX_LOCK(MTX)                     \
  do {                                    \
    if (pthread_mutex_lock((MTX)) != 0) { \
      ISTGT_ERRLOG("lock error\n");       \
      pthread_exit(NULL);                 \
    }                                     \
  } while (0)
#define MTX_UNLOCK(MTX)                     \
  do {                                      \
    if (pthread_mutex_unlock((MTX)) != 0) { \
      ISTGT_ERRLOG("unlock error\n");       \
      pthread_exit(NULL);                   \
    }                                       \
  } while (0)
#define SPIN_LOCK(SPIN)                   \
  do {                                    \
    if (pthread_spin_lock((SPIN)) != 0) { \
      ISTGT_ERRLOG("lock error\n");       \
      pthread_exit(NULL);                 \
    }                                     \
  } while (0)
#define SPIN_UNLOCK(SPIN)                   \
  do {                                      \
    if (pthread_spin_unlock((SPIN)) != 0) { \
      ISTGT_ERRLOG("unlock error\n");       \
      pthread_exit(NULL);                   \
    }                                       \
  } while (0)

typedef struct istgt_portal_t {
  char* label;
  char* host;
  char* port;
  int ref;
  int idx;
  int tag;
  int sock;
} PORTAL;
typedef PORTAL* PORTAL_Ptr;

typedef struct istgt_portal_group_t {
  int nportals;
  PORTAL** portals;
  int ref;
  int idx;
  int tag;
} PORTAL_GROUP;
typedef PORTAL_GROUP* PORTAL_GROUP_Ptr;

typedef struct istgt_initiator_group_t {
  int ninitiators;
  char** initiators;
  int nnetmasks;
  char** netmasks;
  int ref;
  int idx;
  int tag;
} INITIATOR_GROUP;
typedef INITIATOR_GROUP* INITIATOR_GROUP_Ptr;

typedef enum {
  ISTGT_STATE_INVALID = 0,
  ISTGT_STATE_INITIALIZED = 1,
  ISTGT_STATE_RUNNING = 2,
  ISTGT_STATE_EXITING = 3,
  ISTGT_STATE_SHUTDOWN = 4,
} ISTGT_STATE;


typedef struct istgt_t {
  CONFIG* config;
  CONFIG* config_old;
  char* nodebase;

  pthread_mutex_t mutex;
  pthread_mutex_t state_mutex;
  pthread_mutex_t reload_mutex;
  pthread_cond_t reload_cond;

  ISTGT_STATE state;
  uint32_t generation;
  istgt_control_pipe_t sig_pipe;
  int daemon;
  int pg_reload;

  int nportal_group;
  PORTAL_GROUP portal_group[MAX_PORTAL_GROUP];
  int ninitiator_group;
  INITIATOR_GROUP initiator_group[MAX_INITIATOR_GROUP];
  int nlogical_unit;
  struct istgt_lu_t* logical_unit[MAX_LOGICAL_UNIT];

  int timeout;
  int nopininterval;
  int maxr2t;
  int no_discovery_auth;
  int req_discovery_auth;
  int req_discovery_auth_mutual;
  int discovery_auth_group;
  int no_uctl_auth;
  int req_uctl_auth;
  int req_uctl_auth_mutual;
  int uctl_auth_group;

  int MaxSessions;
  int MaxConnections;
  int MaxOutstandingR2T;
  int DefaultTime2Wait;
  int DefaultTime2Retain;
  int FirstBurstLength;
  int MaxBurstLength;
  int MaxRecvDataSegmentLength;
  int InitialR2T;
  int ImmediateData;
  int DataPDUInOrder;
  int DataSequenceInOrder;
  int ErrorRecoveryLevel;
} ISTGT;
typedef ISTGT* ISTGT_Ptr;

char* istgt_get_nmval(CF_SECTION* sp, const char* key, int idx1, int idx2);
char* istgt_get_nval(CF_SECTION* sp, const char* key, int idx);
char* istgt_get_val(CF_SECTION* sp, const char* key);
int istgt_get_nintval(CF_SECTION* sp, const char* key, int idx);
int istgt_get_intval(CF_SECTION* sp, const char* key);

#ifdef USE_ATOMIC
static inline __attribute__((__always_inline__)) int istgt_get_state(
    ISTGT_Ptr istgt) {
  ISTGT_STATE state;
#if defined HAVE_ATOMIC_LOAD_ACQ_INT
  state = atomic_load_acq_int((unsigned int*) &istgt->state);
#elif defined HAVE_ATOMIC_OR_UINT_NV
  state = (int) atomic_or_uint_nv((unsigned int*) &istgt->state, 0);
#else
#error "no atomic operation"
#endif
  return state;
}
static inline __attribute__((__always_inline__)) void istgt_set_state(
    ISTGT_Ptr istgt, ISTGT_STATE state) {
#if defined HAVE_ATOMIC_STORE_REL_INT
  atomic_store_rel_int((unsigned int*) &istgt->state, state);
#elif defined HAVE_ATOMIC_SWAP_UINT
  (void) atomic_swap_uint((unsigned int*) &istgt->state, state);
#if defined HAVE_MEMBAR_PRODUCER
  membar_producer();
#endif
#else
#error "no atomic operation"
#endif
}
#elif defined(USE_GCC_ATOMIC)
/* gcc >= 4.1 builtin functions */
static inline __attribute__((__always_inline__)) int istgt_get_state(
    ISTGT_Ptr istgt) {
  ISTGT_STATE state;
  state = __sync_fetch_and_add((unsigned int*) &istgt->state, 0);
  return state;
}
static inline __attribute__((__always_inline__)) void istgt_set_state(
    ISTGT_Ptr istgt, ISTGT_STATE state) {
  ISTGT_STATE state_old;
  do {
    state_old = __sync_fetch_and_add((unsigned int*) &istgt->state, 0);
  } while (__sync_val_compare_and_swap(
               (unsigned int*) &istgt->state, state_old, state) != state_old);
#if defined(HAVE_GCC_ATOMIC_SYNCHRONIZE)
  __sync_synchronize();
#endif
}
#else  /* !USE_ATOMIC && !USE_GCC_ATOMIC */
static inline __attribute__((__always_inline__)) int istgt_get_state(
    ISTGT_Ptr istgt) {
  ISTGT_STATE state;
  MTX_LOCK(&istgt->state_mutex);
  state = istgt->state;
  MTX_UNLOCK(&istgt->state_mutex);
  return state;
}

static inline __attribute__((__always_inline__)) void istgt_set_state(
    ISTGT_Ptr istgt, ISTGT_STATE state) {
  MTX_LOCK(&istgt->state_mutex);
  istgt->state = state;
  MTX_UNLOCK(&istgt->state_mutex);
}
#endif /* USE_ATOMIC */

#endif /* ISTGT_H */
