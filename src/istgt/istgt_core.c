/*
 * Copyright (C) 2008-2015 Daisuke Aoyama <aoyama@peach.ne.jp>.
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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "istgt_conf.h"
#include "istgt_core.h"
#include "istgt_crc32c.h"
#include "istgt_iscsi.h"
#include "istgt_log.h"
#include "istgt_lu.h"
#include "istgt_misc.h"
#include "istgt_platform.h"
#include "istgt_proto.h"
#include "istgt_sock.h"

#include "config_file.h"

#define POLLWAIT 5000
#define PORTNUMLEN 32

static int istgt_parse_portal(const char* portal, char** host, char** port) {
  const char* p;
  int n;

  if (portal == NULL) {
    ISTGT_ERRLOG("portal error\n");
    return -1;
  }

  if (portal[0] == '[') {
    /* IPv6 */
    p = strchr(portal + 1, ']');
    if (p == NULL) {
      ISTGT_ERRLOG("portal error\n");
      return -1;
    }
    p++;
    n = p - portal;
    if (host != NULL) {
      *host = xmalloc(n + 1);
      memcpy(*host, portal, n);
      (*host)[n] = '\0';
    }
    if (p[0] == '\0') {
      if (port != NULL) {
        *port = xmalloc(PORTNUMLEN);
        snprintf(*port, PORTNUMLEN, "%d", DEFAULT_PORT);
      }
    } else {
      if (p[0] != ':') {
        ISTGT_ERRLOG("portal error\n");
        if (host != NULL)
          xfree(*host);
        return -1;
      }
      if (port != NULL)
        *port = xstrdup(p + 1);
    }
  } else {
    /* IPv4 */
    p = strchr(portal, ':');
    if (p == NULL) {
      p = portal + strlen(portal);
    }
    n = p - portal;
    if (host != NULL) {
      *host = xmalloc(n + 1);
      memcpy(*host, portal, n);
      (*host)[n] = '\0';
    }
    if (p[0] == '\0') {
      if (port != NULL) {
        *port = xmalloc(PORTNUMLEN);
        snprintf(*port, PORTNUMLEN, "%d", DEFAULT_PORT);
      }
    } else {
      if (p[0] != ':') {
        ISTGT_ERRLOG("portal error\n");
        if (host != NULL)
          xfree(*host);
        return -1;
      }
      if (port != NULL)
        *port = xstrdup(p + 1);
    }
  }
  return 0;
}

static int istgt_add_portal_group(ISTGT_Ptr istgt,
                                  CF_SECTION* sp,
                                  int* pgp_idx) {
  const char* val;
  char *label, *portal, *host, *port;
  int alloc_len;
  int idx, free_idx;
  int portals;
  int rc;
  int i;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "add portal group %d\n", sp->num);

  val = istgt_get_val(sp, "Comment");
  if (val != NULL) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "Comment %s\n", val);
  }

  /* counts number of definition */
  for (i = 0;; i++) {
    label = istgt_get_nmval(sp, "Portal", i, 0);
    portal = istgt_get_nmval(sp, "Portal", i, 1);
    if (label == NULL || portal == NULL)
      break;
    rc = istgt_parse_portal(portal, NULL, NULL);
    if (rc < 0) {
      ISTGT_ERRLOG("parse portal error (%s)\n", portal);
      return -1;
    }
  }
  portals = i;
  if (portals > MAX_PORTAL) {
    ISTGT_ERRLOG("%d > MAX_PORTAL\n", portals);
    return -1;
  }

  MTX_LOCK(&istgt->mutex);
  idx = istgt->nportal_group;
  free_idx = -1;
  for (i = 0; i < istgt->nportal_group; i++) {
    if (istgt->portal_group[i].tag != 0)
      continue;
    if (istgt->portal_group[i].nportals == portals) {
      free_idx = i;
      break;
    }
  }
  if (free_idx >= 0)
    idx = free_idx;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "Index=%d, Tag=%d, Portals=%d\n",
                 idx,
                 sp->num,
                 portals);
  if (idx < MAX_PORTAL_GROUP) {
    if (free_idx < 0) {
      istgt->portal_group[idx].nportals = portals;
      alloc_len = sizeof(PORTAL*) * portals;
      istgt->portal_group[idx].portals = xmalloc(alloc_len);
    }
    istgt->portal_group[idx].ref = 0;
    istgt->portal_group[idx].idx = idx;
    istgt->portal_group[idx].tag = sp->num;

    for (i = 0; i < portals; i++) {
      label = istgt_get_nmval(sp, "Portal", i, 0);
      portal = istgt_get_nmval(sp, "Portal", i, 1);
      if (label == NULL || portal == NULL) {
        if (free_idx < 0) {
          xfree(istgt->portal_group[idx].portals);
          istgt->portal_group[idx].nportals = 0;
        }
        istgt->portal_group[idx].tag = 0;
        MTX_UNLOCK(&istgt->mutex);
        ISTGT_ERRLOG("portal error\n");
        return -1;
      }
      rc = istgt_parse_portal(portal, &host, &port);
      if (rc < 0) {
        if (free_idx < 0) {
          xfree(istgt->portal_group[idx].portals);
          istgt->portal_group[idx].nportals = 0;
        }
        istgt->portal_group[idx].tag = 0;
        MTX_UNLOCK(&istgt->mutex);
        ISTGT_ERRLOG("parse portal error (%s)\n", portal);
        return -1;
      }
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                     "RIndex=%d, Host=%s, Port=%s, Tag=%d\n",
                     i,
                     host,
                     port,
                     sp->num);

      if (free_idx < 0) {
        istgt->portal_group[idx].portals[i] = xmalloc(sizeof(PORTAL));
      } else {
        xfree(istgt->portal_group[idx].portals[i]->label);
        xfree(istgt->portal_group[idx].portals[i]->host);
        xfree(istgt->portal_group[idx].portals[i]->port);
      }
      istgt->portal_group[idx].portals[i]->label = xstrdup(label);
      istgt->portal_group[idx].portals[i]->host = host;
      istgt->portal_group[idx].portals[i]->port = port;
      istgt->portal_group[idx].portals[i]->ref = 0;
      istgt->portal_group[idx].portals[i]->idx = i;
      istgt->portal_group[idx].portals[i]->tag = sp->num;
      istgt->portal_group[idx].portals[i]->sock = -1;
    }

    if (pgp_idx != NULL)
      *pgp_idx = idx;
    if (free_idx < 0) {
      idx++;
      istgt->nportal_group = idx;
    }
  } else {
    MTX_UNLOCK(&istgt->mutex);
    ISTGT_ERRLOG("nportal_group(%d) >= MAX_PORTAL_GROUP\n", idx);
    return -1;
  }
  MTX_UNLOCK(&istgt->mutex);
  return 0;
}

static int istgt_build_portal_group_array(ISTGT_Ptr istgt) {
  CF_SECTION* sp;
  int rc;

  sp = istgt->config->section;
  while (sp != NULL) {
    if (sp->type == ST_PORTALGROUP) {
      if (sp->num == 0) {
        ISTGT_ERRLOG("Group 0 is invalid\n");
        return -1;
      }
      rc = istgt_add_portal_group(istgt, sp, NULL);
      if (rc < 0) {
        ISTGT_ERRLOG("add_portal_group() failed\n");
        return -1;
      }
    }
    sp = sp->next;
  }
  return 0;
}

static void istgt_destroy_portal_group_array(ISTGT_Ptr istgt) {
  int i, j;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_destory_portal_group_array\n");
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    for (j = 0; j < istgt->portal_group[i].nportals; j++) {
      xfree(istgt->portal_group[i].portals[j]->label);
      xfree(istgt->portal_group[i].portals[j]->host);
      xfree(istgt->portal_group[i].portals[j]->port);
      xfree(istgt->portal_group[i].portals[j]);
    }
    xfree(istgt->portal_group[i].portals);

    istgt->portal_group[i].nportals = 0;
    istgt->portal_group[i].portals = NULL;
    istgt->portal_group[i].ref = 0;
    istgt->portal_group[i].idx = i;
    istgt->portal_group[i].tag = 0;
  }
  istgt->nportal_group = 0;
  MTX_UNLOCK(&istgt->mutex);
}

static int istgt_open_portal_group(PORTAL_GROUP* pgp) {
  int port;
  int sock;
  int i;

  for (i = 0; i < pgp->nportals; i++) {
    if (pgp->portals[i]->sock < 0) {
      ISTGT_TRACELOG(ISTGT_TRACE_NET,
                     "open host %s, port %s, tag %d\n",
                     pgp->portals[i]->host,
                     pgp->portals[i]->port,
                     pgp->portals[i]->tag);
      port = (int) strtol(pgp->portals[i]->port, NULL, 0);
      sock = istgt_listen(pgp->portals[i]->host, port);
      if (sock < 0) {
        ISTGT_ERRLOG("listen error %.64s:%d\n", pgp->portals[i]->host, port);
        return -1;
      }
      pgp->portals[i]->sock = sock;
    }
  }
  return 0;
}

static int istgt_open_all_portals(ISTGT_Ptr istgt) {
  int rc;
  int i;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_open_portal\n");
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    rc = istgt_open_portal_group(&istgt->portal_group[i]);
    if (rc < 0) {
      MTX_UNLOCK(&istgt->mutex);
      return -1;
    }
  }
  MTX_UNLOCK(&istgt->mutex);
  return 0;
}

static int istgt_close_portal_group(PORTAL_GROUP* pgp) {
  int i;

  for (i = 0; i < pgp->nportals; i++) {
    if (pgp->portals[i]->sock >= 0) {
      ISTGT_TRACELOG(ISTGT_TRACE_NET,
                     "close host %s, port %s, tag %d\n",
                     pgp->portals[i]->host,
                     pgp->portals[i]->port,
                     pgp->portals[i]->tag);
      close(pgp->portals[i]->sock);
      pgp->portals[i]->sock = -1;
    }
  }
  return 0;
}

static int istgt_close_all_portals(ISTGT_Ptr istgt) {
  int rc;
  int i;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_close_portal\n");
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    rc = istgt_close_portal_group(&istgt->portal_group[i]);
    if (rc < 0) {
      MTX_UNLOCK(&istgt->mutex);
      return -1;
    }
  }
  MTX_UNLOCK(&istgt->mutex);
  return 0;
}

static int istgt_add_initiator_group(ISTGT_Ptr istgt, CF_SECTION* sp) {
  const char* val;
  int alloc_len;
  int idx;
  int names;
  int masks;
  int i;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "add initiator group %d\n", sp->num);

  val = istgt_get_val(sp, "Comment");
  if (val != NULL) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "Comment %s\n", val);
  }

  /* counts number of definition */
  for (i = 0;; i++) {
    val = istgt_get_nval(sp, "InitiatorName", i);
    if (val == NULL)
      break;
  }
  names = i;
  if (names > MAX_INITIATOR) {
    ISTGT_ERRLOG("%d > MAX_INITIATOR\n", names);
    return -1;
  }
  for (i = 0;; i++) {
    val = istgt_get_nval(sp, "Netmask", i);
    if (val == NULL)
      break;
  }
  masks = i;
  if (masks > MAX_NETMASK) {
    ISTGT_ERRLOG("%d > MAX_NETMASK\n", masks);
    return -1;
  }

  MTX_LOCK(&istgt->mutex);
  idx = istgt->ninitiator_group;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "Index=%d, Tag=%d, Names=%d, Masks=%d\n",
                 idx,
                 sp->num,
                 names,
                 masks);
  if (idx < MAX_INITIATOR_GROUP) {
    istgt->initiator_group[idx].ninitiators = names;
    alloc_len = sizeof(char*) * names;
    istgt->initiator_group[idx].initiators = xmalloc(alloc_len);
    istgt->initiator_group[idx].nnetmasks = masks;
    alloc_len = sizeof(char*) * masks;
    istgt->initiator_group[idx].netmasks = xmalloc(alloc_len);
    istgt->initiator_group[idx].ref = 0;
    istgt->initiator_group[idx].idx = idx;
    istgt->initiator_group[idx].tag = sp->num;

    for (i = 0; i < names; i++) {
      val = istgt_get_nval(sp, "InitiatorName", i);
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "InitiatorName %s\n", val);
      istgt->initiator_group[idx].initiators[i] = xstrdup(val);
    }
    for (i = 0; i < masks; i++) {
      val = istgt_get_nval(sp, "Netmask", i);
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "Netmask %s\n", val);
      istgt->initiator_group[idx].netmasks[i] = xstrdup(val);
    }

    idx++;
    istgt->ninitiator_group = idx;
  } else {
    MTX_UNLOCK(&istgt->mutex);
    ISTGT_ERRLOG("ninitiator_group(%d) >= MAX_INITIATOR_GROUP\n", idx);
    return -1;
  }
  MTX_UNLOCK(&istgt->mutex);
  return 0;
}

static int istgt_build_initiator_group_array(ISTGT_Ptr istgt) {
  CF_SECTION* sp;
  int rc;

  sp = istgt->config->section;
  while (sp != NULL) {
    if (sp->type == ST_INITIATORGROUP) {
      if (sp->num == 0) {
        ISTGT_ERRLOG("Group 0 is invalid\n");
        return -1;
      }
      rc = istgt_add_initiator_group(istgt, sp);
      if (rc < 0) {
        ISTGT_ERRLOG("add_initiator_group() failed\n");
        return -1;
      }
    }
    sp = sp->next;
  }
  return 0;
}

static void istgt_destory_initiator_group_array(ISTGT_Ptr istgt) {
  int i, j;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_destory_initiator_group_array\n");
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->ninitiator_group; i++) {
    for (j = 0; j < istgt->initiator_group[i].ninitiators; j++) {
      xfree(istgt->initiator_group[i].initiators[j]);
    }
    xfree(istgt->initiator_group[i].initiators);
    for (j = 0; j < istgt->initiator_group[i].nnetmasks; j++) {
      xfree(istgt->initiator_group[i].netmasks[j]);
    }
    xfree(istgt->initiator_group[i].netmasks);

    istgt->initiator_group[i].ninitiators = 0;
    istgt->initiator_group[i].initiators = NULL;
    istgt->initiator_group[i].nnetmasks = 0;
    istgt->initiator_group[i].netmasks = NULL;
    istgt->initiator_group[i].ref = 0;
    istgt->initiator_group[i].idx = i;
    istgt->initiator_group[i].tag = 0;
  }
  istgt->ninitiator_group = 0;
  MTX_UNLOCK(&istgt->mutex);
}

char* istgt_get_nmval(CF_SECTION* sp, const char* key, int idx1, int idx2) {
  CF_ITEM* ip;
  CF_VALUE* vp;
  int i;

  ip = istgt_find_cf_nitem(sp, key, idx1);
  if (ip == NULL)
    return NULL;
  vp = ip->val;
  if (vp == NULL)
    return NULL;
  for (i = 0; vp != NULL; vp = vp->next) {
    if (i == idx2)
      return vp->value;
    i++;
  }
  return NULL;
}

char* istgt_get_nval(CF_SECTION* sp, const char* key, int idx) {
  CF_ITEM* ip;
  CF_VALUE* vp;

  ip = istgt_find_cf_nitem(sp, key, idx);
  if (ip == NULL)
    return NULL;
  vp = ip->val;
  if (vp == NULL)
    return NULL;
  return vp->value;
}

char* istgt_get_val(CF_SECTION* sp, const char* key) {
  return istgt_get_nval(sp, key, 0);
}

int istgt_get_nintval(CF_SECTION* sp, const char* key, int idx) {
  const char* v;
  int value;

  v = istgt_get_nval(sp, key, idx);
  if (v == NULL)
    return -1;
  value = (int) strtol(v, NULL, 10);
  return value;
}

int istgt_get_intval(CF_SECTION* sp, const char* key) {
  return istgt_get_nintval(sp, key, 0);
}

static int istgt_init(ISTGT_Ptr istgt) {
  CF_SECTION* sp;
  const char* ag_tag;
  const char* val;
  int ag_tag_i;
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
  int timeout;
  int nopininterval;
  int maxr2t;
  int rc;
  int i;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_init\n");
  sp = istgt_find_cf_section(istgt->config, "Global");
  if (sp == NULL) {
    ISTGT_ERRLOG("find_cf_section failed()\n");
    return -1;
  }

  val = istgt_get_val(sp, "Comment");
  if (val != NULL) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "Comment %s\n", val);
  }

  val = istgt_get_val(sp, "NodeBase");
  if (val == NULL) {
    val = DEFAULT_NODEBASE;
  }
  istgt->nodebase = xstrdup(val);
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "NodeBase %s\n", istgt->nodebase);

  MaxSessions = istgt_get_intval(sp, "MaxSessions");
  if (MaxSessions < 1) {
    MaxSessions = DEFAULT_MAX_SESSIONS;
  }
  istgt->MaxSessions = MaxSessions;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "MaxSessions %d\n", istgt->MaxSessions);

  MaxConnections = istgt_get_intval(sp, "MaxConnections");
  if (MaxConnections < 1) {
    MaxConnections = DEFAULT_MAX_CONNECTIONS;
  }
  istgt->MaxConnections = MaxConnections;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "MaxConnections %d\n", istgt->MaxConnections);

  /* limited to 16bits - RFC3720(12.2) */
  if (MaxSessions > 0xffff) {
    ISTGT_ERRLOG("over 65535 sessions are not supported\n");
    return -1;
  }
  if (MaxConnections > 0xffff) {
    ISTGT_ERRLOG("over 65535 connections are not supported\n");
    return -1;
  }

  MaxOutstandingR2T = istgt_get_intval(sp, "MaxOutstandingR2T");
  if (MaxOutstandingR2T < 1) {
    MaxOutstandingR2T = DEFAULT_MAXOUTSTANDINGR2T;
  }
  istgt->MaxOutstandingR2T = MaxOutstandingR2T;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "MaxOutstandingR2T %d\n", istgt->MaxOutstandingR2T);

  DefaultTime2Wait = istgt_get_intval(sp, "DefaultTime2Wait");
  if (DefaultTime2Wait < 0) {
    DefaultTime2Wait = DEFAULT_DEFAULTTIME2WAIT;
  }
  istgt->DefaultTime2Wait = DefaultTime2Wait;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "DefaultTime2Wait %d\n", istgt->DefaultTime2Wait);

  DefaultTime2Retain = istgt_get_intval(sp, "DefaultTime2Retain");
  if (DefaultTime2Retain < 0) {
    DefaultTime2Retain = DEFAULT_DEFAULTTIME2RETAIN;
  }
  istgt->DefaultTime2Retain = DefaultTime2Retain;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "DefaultTime2Retain %d\n", istgt->DefaultTime2Retain);

  /* check size limit - RFC3720(12.15, 12.16, 12.17) */
  if (istgt->MaxOutstandingR2T > 65535) {
    ISTGT_ERRLOG("MaxOutstandingR2T(%d) > 65535\n", istgt->MaxOutstandingR2T);
    return -1;
  }
  if (istgt->DefaultTime2Wait > 3600) {
    ISTGT_ERRLOG("DefaultTime2Wait(%d) > 3600\n", istgt->DefaultTime2Wait);
    return -1;
  }
  if (istgt->DefaultTime2Retain > 3600) {
    ISTGT_ERRLOG("DefaultTime2Retain(%d) > 3600\n", istgt->DefaultTime2Retain);
    return -1;
  }

  FirstBurstLength = istgt_get_intval(sp, "FirstBurstLength");
  if (FirstBurstLength < 0) {
    FirstBurstLength = DEFAULT_FIRSTBURSTLENGTH;
  }
  istgt->FirstBurstLength = FirstBurstLength;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "FirstBurstLength %d\n", istgt->FirstBurstLength);

  MaxBurstLength = istgt_get_intval(sp, "MaxBurstLength");
  if (MaxBurstLength < 0) {
    MaxBurstLength = DEFAULT_MAXBURSTLENGTH;
  }
  istgt->MaxBurstLength = MaxBurstLength;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "MaxBurstLength %d\n", istgt->MaxBurstLength);

  MaxRecvDataSegmentLength = istgt_get_intval(sp, "MaxRecvDataSegmentLength");
  if (MaxRecvDataSegmentLength < 0) {
    MaxRecvDataSegmentLength = DEFAULT_MAXRECVDATASEGMENTLENGTH;
  }
  istgt->MaxRecvDataSegmentLength = MaxRecvDataSegmentLength;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "MaxRecvDataSegmentLength %d\n",
                 istgt->MaxRecvDataSegmentLength);

  /* check size limit (up to 24bits - RFC3720(12.12)) */
  if (istgt->MaxBurstLength < 512) {
    ISTGT_ERRLOG("MaxBurstLength(%d) < 512\n", istgt->MaxBurstLength);
    return -1;
  }
  if (istgt->FirstBurstLength < 512) {
    ISTGT_ERRLOG("FirstBurstLength(%d) < 512\n", istgt->FirstBurstLength);
    return -1;
  }
  if (istgt->FirstBurstLength > istgt->MaxBurstLength) {
    ISTGT_ERRLOG("FirstBurstLength(%d) > MaxBurstLength(%d)\n",
                 istgt->FirstBurstLength,
                 istgt->MaxBurstLength);
    return -1;
  }
  if (istgt->MaxBurstLength > 0x00ffffff) {
    ISTGT_ERRLOG("MaxBurstLength(%d) > 0x00ffffff\n", istgt->MaxBurstLength);
    return -1;
  }
  if (istgt->MaxRecvDataSegmentLength < 512) {
    ISTGT_ERRLOG("MaxRecvDataSegmentLength(%d) < 512\n",
                 istgt->MaxRecvDataSegmentLength);
    return -1;
  }
  if (istgt->MaxRecvDataSegmentLength > 0x00ffffff) {
    ISTGT_ERRLOG("MaxRecvDataSegmentLength(%d) > 0x00ffffff\n",
                 istgt->MaxRecvDataSegmentLength);
    return -1;
  }

  val = istgt_get_val(sp, "InitialR2T");
  if (val == NULL) {
    InitialR2T = DEFAULT_INITIALR2T;
  } else if (strcasecmp(val, "Yes") == 0) {
    InitialR2T = 1;
  } else if (strcasecmp(val, "No") == 0) {
#if 0
		InitialR2T = 0;
#else
    ISTGT_ERRLOG("not supported value %s\n", val);
    return -1;
#endif
  } else {
    ISTGT_ERRLOG("unknown value %s\n", val);
    return -1;
  }
  istgt->InitialR2T = InitialR2T;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "InitialR2T %s\n", istgt->InitialR2T ? "Yes" : "No");

  val = istgt_get_val(sp, "ImmediateData");
  if (val == NULL) {
    ImmediateData = DEFAULT_IMMEDIATEDATA;
  } else if (strcasecmp(val, "Yes") == 0) {
    ImmediateData = 1;
  } else if (strcasecmp(val, "No") == 0) {
    ImmediateData = 0;
  } else {
    ISTGT_ERRLOG("unknown value %s\n", val);
    return -1;
  }
  istgt->ImmediateData = ImmediateData;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "ImmediateData %s\n",
                 istgt->ImmediateData ? "Yes" : "No");

  val = istgt_get_val(sp, "DataPDUInOrder");
  if (val == NULL) {
    DataPDUInOrder = DEFAULT_DATAPDUINORDER;
  } else if (strcasecmp(val, "Yes") == 0) {
    DataPDUInOrder = 1;
  } else if (strcasecmp(val, "No") == 0) {
#if 0
		DataPDUInOrder = 0;
#else
    ISTGT_ERRLOG("not supported value %s\n", val);
    return -1;
#endif
  } else {
    ISTGT_ERRLOG("unknown value %s\n", val);
    return -1;
  }
  istgt->DataPDUInOrder = DataPDUInOrder;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "DataPDUInOrder %s\n",
                 istgt->DataPDUInOrder ? "Yes" : "No");

  val = istgt_get_val(sp, "DataSequenceInOrder");
  if (val == NULL) {
    DataSequenceInOrder = DEFAULT_DATASEQUENCEINORDER;
  } else if (strcasecmp(val, "Yes") == 0) {
    DataSequenceInOrder = 1;
  } else if (strcasecmp(val, "No") == 0) {
#if 0
		DataSequenceInOrder = 0;
#else
    ISTGT_ERRLOG("not supported value %s\n", val);
    return -1;
#endif
  } else {
    ISTGT_ERRLOG("unknown value %s\n", val);
    return -1;
  }
  istgt->DataSequenceInOrder = DataSequenceInOrder;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "DataSequenceInOrder %s\n",
                 istgt->DataSequenceInOrder ? "Yes" : "No");

  ErrorRecoveryLevel = istgt_get_intval(sp, "ErrorRecoveryLevel");
  if (ErrorRecoveryLevel < 0) {
    ErrorRecoveryLevel = DEFAULT_ERRORRECOVERYLEVEL;
  } else if (ErrorRecoveryLevel == 0) {
    ErrorRecoveryLevel = 0;
  } else if (ErrorRecoveryLevel == 1) {
#if 0
		ErrorRecoveryLevel = 1;
#else
    ISTGT_ERRLOG("not supported value %d\n", ErrorRecoveryLevel);
    return -1;
#endif
  } else if (ErrorRecoveryLevel == 2) {
#if 0
		ErrorRecoveryLevel = 2;
#else
    ISTGT_ERRLOG("not supported value %d\n", ErrorRecoveryLevel);
    return -1;
#endif
  } else {
    ISTGT_ERRLOG("not supported value %d\n", ErrorRecoveryLevel);
    return -1;
  }
  istgt->ErrorRecoveryLevel = ErrorRecoveryLevel;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "ErrorRecoveryLevel %d\n", istgt->ErrorRecoveryLevel);

  timeout = istgt_get_intval(sp, "Timeout");
  if (timeout < 0) {
    timeout = DEFAULT_TIMEOUT;
  }
  istgt->timeout = timeout;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "Timeout %d\n", istgt->timeout);

  nopininterval = istgt_get_intval(sp, "NopInInterval");
  if (nopininterval < 0) {
    nopininterval = DEFAULT_NOPININTERVAL;
  }
  istgt->nopininterval = nopininterval;
  ISTGT_TRACELOG(
      ISTGT_TRACE_DEBUG, "NopInInterval %d\n", istgt->nopininterval);

  maxr2t = istgt_get_intval(sp, "MaxR2T");
  if (maxr2t < 0) {
    maxr2t = DEFAULT_MAXR2T;
  }
  if (maxr2t > MAX_R2T) {
    ISTGT_ERRLOG("MaxR2T(%d) > %d\n", maxr2t, MAX_R2T);
    return -1;
  }
  istgt->maxr2t = maxr2t;
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "MaxR2T %d\n", istgt->maxr2t);

  val = istgt_get_val(sp, "DiscoveryAuthMethod");
  if (val == NULL) {
    istgt->no_discovery_auth = 0;
    istgt->req_discovery_auth = 0;
    istgt->req_discovery_auth_mutual = 0;
  } else {
    istgt->no_discovery_auth = 0;
    for (i = 0;; i++) {
      val = istgt_get_nmval(sp, "DiscoveryAuthMethod", 0, i);
      if (val == NULL)
        break;
      if (strcasecmp(val, "CHAP") == 0) {
        istgt->req_discovery_auth = 1;
      } else if (strcasecmp(val, "Mutual") == 0) {
        istgt->req_discovery_auth_mutual = 1;
      } else if (strcasecmp(val, "Auto") == 0) {
        istgt->req_discovery_auth = 0;
        istgt->req_discovery_auth_mutual = 0;
      } else if (strcasecmp(val, "None") == 0) {
        istgt->no_discovery_auth = 1;
        istgt->req_discovery_auth = 0;
        istgt->req_discovery_auth_mutual = 0;
      } else {
        ISTGT_ERRLOG("unknown auth\n");
        return -1;
      }
    }
    if (istgt->req_discovery_auth_mutual && !istgt->req_discovery_auth) {
      ISTGT_ERRLOG("Mutual but not CHAP\n");
      return -1;
    }
  }
  if (istgt->no_discovery_auth != 0) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "DiscoveryAuthMethod None\n");
  } else if (istgt->req_discovery_auth == 0) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "DiscoveryAuthMethod Auto\n");
  } else {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                   "DiscoveryAuthMethod %s %s\n",
                   istgt->req_discovery_auth ? "CHAP" : "",
                   istgt->req_discovery_auth_mutual ? "Mutual" : "");
  }

  val = istgt_get_val(sp, "DiscoveryAuthGroup");
  if (val == NULL) {
    istgt->discovery_auth_group = 0;
  } else {
    ag_tag = val;
    if (strcasecmp(ag_tag, "None") == 0) {
      ag_tag_i = 0;
    } else {
      if (strncasecmp(ag_tag, "AuthGroup", strlen("AuthGroup")) != 0 ||
          sscanf(ag_tag, "%*[^0-9]%d", &ag_tag_i) != 1) {
        ISTGT_ERRLOG("auth group error\n");
        return -1;
      }
      if (ag_tag_i == 0) {
        ISTGT_ERRLOG("invalid auth group %d\n", ag_tag_i);
        return -1;
      }
    }
    istgt->discovery_auth_group = ag_tag_i;
  }
  if (istgt->discovery_auth_group == 0) {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "DiscoveryAuthGroup None\n");
  } else {
    ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                   "DiscoveryAuthGroup AuthGroup%d\n",
                   istgt->discovery_auth_group);
  }

  /* global mutex */
  rc = pthread_mutex_init(&istgt->mutex, NULL);
  if (rc != 0) {
    ISTGT_ERRLOG("mutex_init() failed\n");
    return -1;
  }

  rc = istgt_build_portal_group_array(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_build_portal_array() failed\n");
    return -1;
  }
  rc = istgt_build_initiator_group_array(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("build_initiator_group_array() failed\n");
    return -1;
  }

  rc = pthread_mutex_init(&istgt->state_mutex, NULL);
  if (rc != 0) {
    ISTGT_ERRLOG("mutex_init() failed\n");
    return -1;
  }

  rc = istgt_control_pipe_create(&istgt->sig_pipe);
  if (rc != 0) {
    ISTGT_ERRLOG("istgt_control_pipe_create() failed\n");
    return -1;
  }

  /* XXX TODO: add initializer */

  istgt_set_state(istgt, ISTGT_STATE_INITIALIZED);

  return 0;
}

static void istgt_shutdown(ISTGT_Ptr istgt) {
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_shutdown\n");

  istgt_destory_initiator_group_array(istgt);
  istgt_destroy_portal_group_array(istgt);
  istgt_control_pipe_destroy(&istgt->sig_pipe);

  xfree(istgt->nodebase);

  (void) pthread_mutex_destroy(&istgt->state_mutex);
  (void) pthread_mutex_destroy(&istgt->mutex);
}

#define SIG_PIPE_CMD_LENGTH 5

static int istgt_stop_loop(ISTGT_Ptr istgt) {
  char tmp[SIG_PIPE_CMD_LENGTH];
  int rc;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_stop_loop\n");
  tmp[0] = 'E';
  DSET32(&tmp[1], 0);
  rc = istgt_control_pipe_write(&istgt->sig_pipe, tmp, SIG_PIPE_CMD_LENGTH);
  if (rc < 0 || rc != SIG_PIPE_CMD_LENGTH) {
    ISTGT_ERRLOG("write() failed\n");
    /* ignore error */
  }
  return 0;
}

static PORTAL* istgt_get_sock_portal(ISTGT_Ptr istgt, int sock) {
  int i, j;

  if (sock < 0)
    return NULL;
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    for (j = 0; j < istgt->portal_group[i].nportals; j++) {
      if (istgt->portal_group[i].portals[j]->sock == sock) {
        MTX_UNLOCK(&istgt->mutex);
        return istgt->portal_group[i].portals[j];
      }
    }
  }
  MTX_UNLOCK(&istgt->mutex);
  return NULL;
}

static int istgt_acceptor(ISTGT_Ptr istgt) {
  PORTAL* pp;
#ifdef ISTGT_USE_KQUEUE
  int kq;
  struct kevent kev;
  struct timespec kev_timeout;
  int kqsocks[MAX_PORTAL_GROUP + 1];
#else
  struct pollfd fds[MAX_PORTAL_GROUP + 1];
#endif /* ISTGT_USE_KQUEUE */
  struct sockaddr_storage sa;
  socklen_t salen;
  int sock;
  int rc, n;
  int spidx;
  int nidx;
  int i, j;

  if (istgt_get_state(istgt) != ISTGT_STATE_INITIALIZED) {
    ISTGT_ERRLOG("not initialized\n");
    return -1;
  }
  /* now running main thread */
  istgt_set_state(istgt, ISTGT_STATE_RUNNING);

  nidx = 0;
#ifdef ISTGT_USE_KQUEUE
  kq = kqueue();
  if (kq == -1) {
    ISTGT_ERRLOG("kqueue() failed\n");
    return -1;
  }
  for (i = 0; i < (int) (sizeof kqsocks / sizeof *kqsocks); i++) {
    kqsocks[i] = -1;
  }
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    for (j = 0; j < istgt->portal_group[i].nportals; j++) {
      if (istgt->portal_group[i].portals[j]->sock >= 0) {
        ISTGT_EV_SET(&kev,
                     istgt->portal_group[i].portals[j]->sock,
                     EVFILT_READ,
                     EV_ADD,
                     0,
                     0,
                     NULL);
        rc = kevent(kq, &kev, 1, NULL, 0, NULL);
        if (rc == -1) {
          MTX_UNLOCK(&istgt->mutex);
          ISTGT_ERRLOG("kevent() failed\n");
          close(kq);
          return -1;
        }
        kqsocks[nidx] = istgt->portal_group[i].portals[j]->sock;
        nidx++;
      }
    }
  }
  MTX_UNLOCK(&istgt->mutex);
  ucidx = nidx;
  for (i = 0; i < istgt->nuctl_portal; i++) {
    ISTGT_EV_SET(
        &kev, istgt->uctl_portal[i].sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
    rc = kevent(kq, &kev, 1, NULL, 0, NULL);
    if (rc == -1) {
      ISTGT_ERRLOG("kevent() failed\n");
      close(kq);
      return -1;
    }
    kqsocks[nidx] = istgt->uctl_portal[i].sock;
    nidx++;
  }
  ISTGT_EV_SET(&kev, istgt->sig_pipe.fd[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
  rc = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (rc == -1) {
    ISTGT_ERRLOG("kevent() failed\n");
    close(kq);
    return -1;
  }
  spidx = nidx++;
  kqsocks[spidx] = istgt->sig_pipe.fd[0];

  ISTGT_EV_SET(&kev, SIGINT, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  rc = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (rc == -1) {
    ISTGT_ERRLOG("kevent() failed\n");
    close(kq);
    return -1;
  }
  ISTGT_EV_SET(&kev, SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
  rc = kevent(kq, &kev, 1, NULL, 0, NULL);
  if (rc == -1) {
    ISTGT_ERRLOG("kevent() failed\n");
    close(kq);
    return -1;
  }
#else
  memset(&fds, 0, sizeof fds);
  MTX_LOCK(&istgt->mutex);
  for (i = 0; i < istgt->nportal_group; i++) {
    for (j = 0; j < istgt->portal_group[i].nportals; j++) {
      if (istgt->portal_group[i].portals[j]->sock >= 0) {
        fds[nidx].fd = istgt->portal_group[i].portals[j]->sock;
        fds[nidx].events = POLLIN;
        nidx++;
      }
    }
  }
  MTX_UNLOCK(&istgt->mutex);
  spidx = nidx++;
  fds[spidx].fd = istgt->sig_pipe.fd[0];
  fds[spidx].events = POLLIN;
#endif /* ISTGT_USE_KQUEUE */

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "loop start\n");
  while (1) {
    if (istgt_get_state(istgt) != ISTGT_STATE_RUNNING) {
      break;
    }
#ifdef ISTGT_USE_KQUEUE
    // ISTGT_TRACELOG(ISTGT_TRACE_NET, "kevent %d\n", nidx);
    kev_timeout.tv_sec = 10;
    kev_timeout.tv_nsec = 0;
    rc = kevent(kq, NULL, 0, &kev, 1, &kev_timeout);
    if (rc == -1 && errno == EINTR) {
      continue;
    }
    if (rc == -1) {
      ISTGT_ERRLOG("kevent() failed\n");
      break;
    }
    if (rc == 0) {
      /* idle timeout */
      // ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "kevent TIMEOUT\n");
      continue;
    }
    if (kev.filter == EVFILT_SIGNAL) {
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "kevent SIGNAL\n");
      if (kev.ident == SIGINT || kev.ident == SIGTERM) {
        ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "kevent SIGNAL SIGINT/SIGTERM\n");
        break;
      }
      continue;
    }
#else
    // ISTGT_TRACELOG(ISTGT_TRACE_NET, "poll %d\n", nidx);
    rc = poll(fds, nidx, POLLWAIT);
    if (rc == -1 && errno == EINTR) {
      continue;
    }
    if (rc == -1) {
      ISTGT_ERRLOG("poll() failed\n");
      break;
    }
    if (rc == 0) {
      /* no fds */
      // ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "poll TIMEOUT\n");
      continue;
    }
#endif /* ISTGT_USE_KQUEUE */

    n = rc;
    for (i = 0; n != 0 && i < spidx; i++) {
#ifdef ISTGT_USE_KQUEUE
      if (kev.ident == (uintptr_t) kqsocks[i]) {
        if (kev.flags) {
          ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "flags %x\n", kev.flags);
        }
#else
      if (fds[i].revents) {
        ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "events %x\n", fds[i].revents);
      }
      if (fds[i].revents & POLLIN) {
#endif /* ISTGT_USE_KQUEUE */
        n--;
        memset(&sa, 0, sizeof(sa));
        salen = sizeof(sa);
#ifdef ISTGT_USE_KQUEUE
        ISTGT_TRACELOG(
            ISTGT_TRACE_NET, "accept %ld\n", (unsigned long) kev.ident);
        pp = istgt_get_sock_portal(istgt, kev.ident);
        rc = accept(kev.ident, (struct sockaddr*) &sa, &salen);
#else
        ISTGT_TRACELOG(ISTGT_TRACE_NET, "accept %d\n", fds[i].fd);
        pp = istgt_get_sock_portal(istgt, fds[i].fd);
        rc = accept(fds[i].fd, (struct sockaddr*) &sa, &salen);
#endif /* ISTGT_USE_KQUEUE */
        if (rc < 0) {
          if (errno == ECONNABORTED || errno == ECONNRESET) {
            continue;
          }
          ISTGT_ERRLOG("accept error: %d(errno=%d)\n", rc, errno);
          continue;
        }
        sock = rc;
#if 0
				rc = fcntl(sock, F_GETFL, 0);
				if (rc == -1) {
					ISTGT_ERRLOG("fcntl() failed\n");
					continue;
				}
				rc = fcntl(sock, F_SETFL, (rc | O_NONBLOCK));
				if (rc == -1) {
					ISTGT_ERRLOG("fcntl() failed\n");
					continue;
				}
#endif
        rc = istgt_create_conn(istgt, pp, sock, (struct sockaddr*) &sa, salen);
        if (rc < 0) {
          close(sock);
          ISTGT_ERRLOG("istgt_create_conn() failed\n");
          continue;
        }
      }
    }

/* check for signal thread */
#ifdef ISTGT_USE_KQUEUE
    if (kev.ident == (uintptr_t) istgt->sig_pipe.fd[0]) {
      if (kev.flags & (EV_EOF | EV_ERROR)) {
        ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "kevent EOF/ERROR\n");
        break;
      }
#else
    if (fds[spidx].revents & POLLHUP) {
      break;
    }
    if (fds[spidx].revents & POLLIN) {
#endif /* ISTGT_USE_KQUEUE */
      char tmp[SIG_PIPE_CMD_LENGTH];
      // int pgp_idx;

      rc = istgt_control_pipe_read(&istgt->sig_pipe, tmp, SIG_PIPE_CMD_LENGTH);
      if (rc < 0 || rc == 0 || rc != SIG_PIPE_CMD_LENGTH) {
        ISTGT_ERRLOG("read() failed\n");
        break;
      }
      // pgp_idx = (int)DGET32(&tmp[1]);

      if (tmp[0] == 'E') {
        ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "exit request (main loop)\n");
        break;
      }

      istgt_fatal("Invalid command on signal pipe");
    }
  }
#ifdef ISTGT_USE_KQUEUE
  close(kq);
#endif /* ISTGT_USE_KQUEUE */
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "loop ended\n");
  istgt_set_state(istgt, ISTGT_STATE_EXITING);
  istgt_lu_set_all_state(istgt, ISTGT_STATE_EXITING);

  return 0;
}

int istgt_start() {
  ISTGT_Ptr istgt;
  CONFIG* config;
  int retry = 10;
  int rc;

  istgt_platform_init();

  istgt = xmalloc(sizeof *istgt);
  memset(istgt, 0, sizeof *istgt);

  istgt->state = ISTGT_STATE_INVALID;
  istgt->sig_pipe = istgt_control_pipe_init();

  /* read config files */
  config = istgt_allocate_config();
  rc = istgt_read_config(
      config, sizeof CONFIG_FILE / sizeof CONFIG_FILE[0], CONFIG_FILE);
  if (rc < 0) {
    fprintf(stderr, "config error\n");
    return -1;
  }
  if (config->section == NULL) {
    fprintf(stderr, "empty config\n");
    istgt_free_config(config);
    return -1;
  }
  istgt->config = config;
  istgt_print_config(config);

#ifdef ISTGT_USE_KQUEUE
  ISTGT_NOTICELOG("using kqueue\n");
#else
  ISTGT_NOTICELOG("using poll\n");
#endif /* ISTGT_USE_KQUEUE */
#ifdef USE_ATOMIC
  ISTGT_NOTICELOG("using host atomic\n");
#elif defined(USE_GCC_ATOMIC)
  ISTGT_NOTICELOG("using gcc atomic\n");
#else
  ISTGT_NOTICELOG("using generic atomic\n");
#endif /* USE_ATOMIC */

#ifdef ISTGT_USE_CRC32C_TABLE
  /* build crc32c table */
  istgt_init_crc32c_table();
#endif /* ISTGT_USE_CRC32C_TABLE */

  /* initialize sub modules */
  rc = istgt_init(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_init() failed\n");
  initialize_error:
    istgt_free_config(config);
    return -1;
  }
  rc = istgt_lu_init(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_lu_init() failed\n");
    goto initialize_error;
  }
  rc = istgt_iscsi_init(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_iscsi_init() failed\n");
    goto initialize_error;
  }

  /* create LUN threads for command queuing */
  rc = istgt_lu_create_threads(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("lu_create_threads() failed\n");
    goto initialize_error;
  }
  rc = istgt_lu_set_all_state(istgt, ISTGT_STATE_RUNNING);
  if (rc < 0) {
    ISTGT_ERRLOG("lu_set_all_state() failed\n");
    goto initialize_error;
  }

  /* open portals */
  rc = istgt_open_all_portals(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_open_all_portals() failed\n");
    goto initialize_error;
  }

  /* accept loop */
  rc = istgt_acceptor(istgt);
  if (rc < 0) {
    ISTGT_ERRLOG("istgt_acceptor() failed\n");
    istgt_close_all_portals(istgt);
    istgt_iscsi_shutdown(istgt);
    istgt_lu_shutdown(istgt);
    istgt_shutdown(istgt);
    config = istgt->config;
    istgt->config = NULL;
    istgt_free_config(config);
    return -1;
  }

  /* wait threads */
  istgt_stop_conns();
  while (retry > 0) {
    if (istgt_get_active_conns() == 0) {
      break;
    }
    sleep(1);
    retry--;
  }
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "retry=%d\n", retry);

  /* cleanup */
  istgt_close_all_portals(istgt);
  istgt_iscsi_shutdown(istgt);
  istgt_lu_shutdown(istgt);
  istgt_shutdown(istgt);
  config = istgt->config;
  istgt->config = NULL;
  istgt_free_config(config);
  istgt->state = ISTGT_STATE_SHUTDOWN;
  xfree(istgt);

  return 0;
}
