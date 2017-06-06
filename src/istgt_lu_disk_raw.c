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
  spec->foffset = 0;
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
  spec->foffset = 0;
  return 0;
}

#if 0
static off_t
istgt_lu_disk_lseek_raw(ISTGT_LU_DISK *spec, off_t offset, int whence)
{
  off_t rc;

  rc = lseek(spec->fd, offset, whence);
  if (rc < 0) {
    return -1;
  }
  spec->foffset = offset;
  return rc;
}
#endif

static int64_t istgt_lu_disk_seek_raw(ISTGT_LU_DISK* spec, uint64_t offset) {
  printf("Seek: %lld\n", (unsigned long long) offset);

#ifndef _WIN32
  // On windows we use the moral equivalent of pread/pwrite, so no need
  // actually seek before read/write.
  off_t rc = lseek(spec->fd, (off_t) offset, SEEK_SET);
  if (rc < 0) {
    return -1;
  }
  if (rc != offset)
    abort();
#endif  // !_WIN32
  spec->foffset = offset;
  return 0;
}

static int64_t istgt_lu_disk_read_raw(ISTGT_LU_DISK* spec,
                                      void* buf,
                                      uint64_t nbytes) {
  int64_t rc;
  printf("Read: %d\n", (int) nbytes);

#ifdef _WIN32
  OVERLAPPED o = {0};
  LARGE_INTEGER offset = {.QuadPart = spec->foffset};
  o.Offset = offset.LowPart;
  o.OffsetHigh = offset.HighPart;
  HANDLE handle = (HANDLE) _get_osfhandle(spec->fd);
  DWORD bytes_read;
  if (!ReadFile(handle, buf, nbytes, &bytes_read, &o)) {
    if (GetLastError() == ERROR_HANDLE_EOF) {
      memset(buf, 0, nbytes);
      rc = nbytes;
    } else {
      return -1;
    }
  } else {
    rc = bytes_read;
  }

#else   // _WIN32
  rc = (int64_t) read(spec->fd, buf, (size_t) nbytes);
  if (rc < 0) {
    return -1;
  }
#endif  // _WIN32

  spec->foffset += rc;
  return rc;
}

static int64_t istgt_lu_disk_write_raw(ISTGT_LU_DISK* spec,
                                       const void* buf,
                                       uint64_t nbytes) {
  int64_t rc;
  printf("Write: %d\n", (int) nbytes);

#ifdef _WIN32
  OVERLAPPED o = {0};
  LARGE_INTEGER offset = {.QuadPart = spec->foffset};
  o.Offset = offset.LowPart;
  o.OffsetHigh = offset.HighPart;
  HANDLE handle = (HANDLE) _get_osfhandle(spec->fd);
  DWORD bytes_written;
  if (!WriteFile(handle, buf, nbytes, &bytes_written, &o))
    return -1;

  rc = bytes_written;

#else   // _WIN32
  rc = (int64_t) write(spec->fd, buf, (size_t) nbytes);
  if (rc < 0)
    return -1;
#endif  // _WIN32


  spec->foffset += rc;
  if (spec->foffset > spec->fsize) {
    spec->fsize = spec->foffset;
  }
  return rc;
}

static int64_t istgt_lu_disk_sync_raw(ISTGT_LU_DISK* spec,
                                      uint64_t offset,
                                      uint64_t nbytes) {
  /*int64_t rc;

  rc = (int64_t) fsync(spec->fd);
  if (rc < 0) {
  return -1;
  }
  spec->foffset = offset + nbytes;
  return rc; */
  return 0;
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
  rc = istgt_lu_disk_seek_raw(spec, offset);
  if (rc == -1) {
    ISTGT_ERRLOG("lu_disk_seek() failed\n");
    xfree(data);
    return -1;
  }
  rc = istgt_lu_disk_read_raw(spec, data, nbytes);
  /* EOF is OK */
  if (rc == -1) {
    ISTGT_ERRLOG("lu_disk_read() failed\n");
    xfree(data);
    return -1;
  }

  /* allocate complete size */
  rc = istgt_lu_disk_seek_raw(spec, offset);
  if (rc == -1) {
    ISTGT_ERRLOG("lu_disk_seek() failed\n");
    xfree(data);
    return -1;
  }
  rc = istgt_lu_disk_write_raw(spec, data, nbytes);
  if (rc == -1 || (uint64_t) rc != nbytes) {
    ISTGT_ERRLOG("lu_disk_write() failed\n");
    xfree(data);
    return -1;
  }
  spec->foffset = size;

  xfree(data);
  return 0;
}

static int istgt_lu_disk_setcache_raw(ISTGT_LU_DISK* spec) {
#ifndef _WIN32
  int flags;
  int rc;
  int fd;

  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "istgt_lu_disk_setcache\n");

  fd = spec->fd;
  if (spec->read_cache) {
    /* not implement */
  } else {
    /* not implement */
  }

  flags = fcntl(fd, F_GETFL, 0);
  if (flags != -1) {
    if (spec->write_cache) {
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "write cache enable\n");
      rc = fcntl(fd, F_SETFL, (flags & ~O_FSYNC));
      spec->write_cache = 1;
    } else {
      ISTGT_TRACELOG(ISTGT_TRACE_DEBUG, "write cache disable\n");
      rc = fcntl(fd, F_SETFL, (flags | O_FSYNC));
      spec->write_cache = 0;
    }
    if (rc == -1) {
#if 0
      ISTGT_ERRLOG("LU%d: LUN%d: fcntl(F_SETFL) failed(errno=%d)\n",
        spec->num, spec->lun, errno);
#endif
    }
  } else {
    ISTGT_ERRLOG("LU%d: LUN%d: fcntl(F_GETFL) failed(errno=%d)\n",
                 spec->num,
                 spec->lun,
                 errno);
  }
#endif  // _WIN32

  return 0;
}

int istgt_lu_disk_raw_lun_init(ISTGT_LU_DISK* spec,
                               ISTGT_Ptr istgt __attribute__((__unused__)),
                               ISTGT_LU_Ptr lu __attribute__((__unused__))) {
  ISTGT_TRACELOG(ISTGT_TRACE_DEBUG,
                 "LU%d: LUN%d unsupported virtual disk\n",
                 spec->num,
                 spec->lun);
  spec->open = istgt_lu_disk_open_raw;
  spec->close = istgt_lu_disk_close_raw;
  spec->seek = istgt_lu_disk_seek_raw;
  spec->read = istgt_lu_disk_read_raw;
  spec->write = istgt_lu_disk_write_raw;
  spec->sync = istgt_lu_disk_sync_raw;
  spec->allocate = istgt_lu_disk_allocate_raw;
  spec->setcache = istgt_lu_disk_setcache_raw;

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
                                   ISTGT_Ptr istgt __attribute__((__unused__)),
                                   ISTGT_LU_Ptr lu
                                   __attribute__((__unused__))) {
  int rc;
  if (!spec->lu->readonly) {
    rc = spec->sync(spec, 0, spec->size);
    if (rc < 0) {
      ISTGT_WARNLOG("LU%d: lu_disk_sync() failed\n", lu->num);
    }
  }

  return spec->close(spec);
}
