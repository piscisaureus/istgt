#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "istgt_log.h"
#include "istgt_lu.h"
#include "istgt_misc.h"
#include "istgt_platform.h"
#include "istgt_proto.h"


static int istgt_lu_disk_open_raw(ISTGT_LU_DISK* spec, int flags, int mode) {
  int rc;
  rc = open(spec->file, flags, mode);
  if (rc < 0) {
    return -1;
  }
  spec->fd = rc;
  return 0;
}

static int istgt_lu_disk_close_raw(ISTGT_LU_DISK* spec) {
  int rc;

  if (spec->fd == -1)
    return 0;
  rc = close(spec->fd);
  if (rc < 0) {
    return -1;
  }
  spec->fd = -1;
  return 0;
}

static int64_t istgt_lu_disk_pread_raw(ISTGT_LU_DISK* spec,
                                       void* buf,
                                       uint64_t nbytes,
                                       uint64_t offset) {
  printf("pread: %llu at %llu\n",
         (unsigned long long) nbytes,
         (unsigned long long) offset);

  ssize_t rc = pread(spec->fd, buf, nbytes, offset);
  if (rc < 0)
    return -1;

  return rc;
}

static int64_t istgt_lu_disk_pwrite_raw(ISTGT_LU_DISK* spec,
                                        const void* buf,
                                        uint64_t nbytes,
                                        uint64_t offset) {
  int64_t rc;
  printf("pwrite: %llu at %llu\n",
         (unsigned long long) nbytes,
         (unsigned long long) offset);

  rc = pwrite(spec->fd, buf, nbytes, offset);
  if (rc < 0)
    return -1;

  if (offset > spec->fsize) {
    spec->fsize = offset;
  }
  return rc;
}

static int64_t istgt_lu_disk_sync_raw(ISTGT_LU_DISK* spec,
                                      uint64_t nbytes,
                                      uint64_t offset) {
  printf("sync: %llu at %llu\n",
         (unsigned long long) nbytes,
         (unsigned long long) offset);
  return fsync(spec->fd);
}

static int istgt_lu_disk_allocate_raw(ISTGT_LU_DISK* spec) {
  uint8_t* data;
  uint64_t fsize;
  uint64_t size;
  uint64_t blocklen;
  uint64_t offset;
  uint64_t nbytes;
  int64_t rc;

  size = spec->size;
  blocklen = spec->blocklen;
  nbytes = blocklen;
  data = xmalloc(nbytes);
  memset(data, 0, nbytes);

  fsize = istgt_lu_get_filesize(spec->file);
  if (fsize > size) {
    xfree(data);
    return 0;
  }
  spec->fsize = fsize;

  offset = size - nbytes;
  rc = istgt_lu_disk_pread_raw(spec, data, nbytes, offset);
  /* EOF is OK */
  if (rc == -1) {
    ISTGT_ERRLOG("lu_disk_read() failed\n");
    xfree(data);
    return -1;
  }

  /* allocate complete size */
  rc = istgt_lu_disk_pwrite_raw(spec, data, nbytes, offset);
  if (rc == -1 || (uint64_t) rc != nbytes) {
    ISTGT_ERRLOG("lu_disk_write() failed\n");
    xfree(data);
    return -1;
  }

  xfree(data);
  return 0;
}

int istgt_lu_disk_raw_lun_init(ISTGT_LU_DISK* spec,
                               ISTGT_Ptr istgt,
                               ISTGT_LU_Ptr lu) {
  UNUSED(istgt);
  UNUSED(lu);

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "LU%d: LUN%d unsupported virtual disk\n",
                 spec->num,
                 spec->lun);
  spec->open = istgt_lu_disk_open_raw;
  spec->close = istgt_lu_disk_close_raw;
  spec->pread = istgt_lu_disk_pread_raw;
  spec->pwrite = istgt_lu_disk_pwrite_raw;
  spec->sync = istgt_lu_disk_sync_raw;
  spec->allocate = istgt_lu_disk_allocate_raw;

  spec->blocklen = lu->blocklen;
  if (spec->blocklen != 512 && spec->blocklen != 1024 &&
      spec->blocklen != 2048 && spec->blocklen != 4096 &&
      spec->blocklen != 8192 && spec->blocklen != 16384 &&
      spec->blocklen != 32768 && spec->blocklen != 65536 &&
      spec->blocklen != 131072 && spec->blocklen != 262144 &&
      spec->blocklen != 524288) {
    ISTGT_ERRLOG(
        "LU%d: invalid blocklen %" PRIu64 "\n", lu->num, spec->blocklen);
    errno = EINVAL;
    return -1;
  }

  return 0;
}

int istgt_lu_disk_raw_lun_shutdown(ISTGT_LU_DISK* spec,
                                   ISTGT_Ptr istgt,
                                   ISTGT_LU_Ptr lu) {
  UNUSED(istgt);

  int rc;
  if (!spec->lu->readonly) {
    rc = spec->sync(spec, spec->size, 0);
    if (rc < 0) {
      ISTGT_WARNLOG("LU%d: lu_disk_sync() failed\n", lu->num);
    }
  }

  return spec->close(spec);
}
