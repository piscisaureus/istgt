#ifdef _WIN32

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>

#include "istgt_platform.h"

int fsync(int fd) {
  return _commit(fd);
}

ssize_t pread(int fd, void* buf, size_t count, uint64_t offset) {
  OVERLAPPED o = {0};
  LARGE_INTEGER offset_li = {.QuadPart = offset};
  o.Offset = offset_li.LowPart;
  o.OffsetHigh = offset_li.HighPart;
  HANDLE handle = (HANDLE) _get_osfhandle(fd);
  DWORD bytes_read;
  if (!ReadFile(handle, buf, count, &bytes_read, &o))
    return -1;
  return bytes_read;
}

ssize_t pwrite(int fd, const void* buf, size_t count, uint64_t offset) {
  OVERLAPPED o = {0};
  LARGE_INTEGER offset_li = {.QuadPart = offset};
  o.Offset = offset_li.LowPart;
  o.OffsetHigh = offset_li.HighPart;
  HANDLE handle = (HANDLE) _get_osfhandle(fd);
  DWORD bytes_written;
  if (!WriteFile(handle, buf, count, &bytes_written, &o))
    return -1;
  return bytes_written;
}

int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* attr) {
  InitializeCriticalSection(mutex);
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex) {
  DeleteCriticalSection(mutex);
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
  EnterCriticalSection(mutex);
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex) {
  if (TryEnterCriticalSection(mutex))
    return 0;
  else
    return EAGAIN;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
  LeaveCriticalSection(mutex);
  return 0;
}

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr) {
  InitializeConditionVariable(cond);
  return 0;
}

int pthread_cond_destroy(pthread_cond_t* cond) {
  // Nothing to do.
  return 0;
}

int pthread_cond_broadcast(pthread_cond_t* cond) {
  WakeAllConditionVariable(cond);
  return 0;
}

int pthread_cond_signal(pthread_cond_t* cond) {
  WakeConditionVariable(cond);
  return 0;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
  if (SleepConditionVariableCS(cond, mutex, INFINITE))
    return 0;
  else
    return EINVAL;
}

int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* abstime) {
  DWORD now_ms = time(NULL) * 1000;
  DWORD end_time_ms = (abstime->tv_sec * 1000) + (abstime->tv_nsec / 1000000);
  DWORD timeout_ms = (end_time_ms > now_ms) ? (end_time_ms - now_ms) : 0;

  if (SleepConditionVariableCS(cond, mutex, timeout_ms))
    return 0;
  else if (GetLastError() == ERROR_TIMEOUT)
    return ETIMEDOUT;
  else
    return EINVAL;
}

#define MAX_THREADS 256


typedef void* (*pthread_start_routine_t)(void*);

enum {
  THREAD_ENTRY_FREE = 0,
  THREAD_ENTRY_IN_USE = 1,
  THREAD_ENTRY_DETACHED = 2,
  THREAD_ENTRY_EXITED = 3
};

typedef struct {
  DWORD id;
  volatile long state;
  HANDLE handle;
  union {
    struct {
      void* arg;
      pthread_start_routine_t start_routine;
    } in;
    struct {
      void* retval;
    } out;
  };
} thread_map_entry_t;

thread_map_entry_t thread_map[MAX_THREADS] = {0};

static void fatal(const char* msg) {
  fprintf(stderr, msg);
  abort();
}

static thread_map_entry_t* thread_map_alloc(void) {
  for (size_t i = 0; i < MAX_THREADS; i++) {
    thread_map_entry_t* entry = &thread_map[i];
    if (InterlockedCompareExchange(&entry->state,
                                   THREAD_ENTRY_IN_USE,
                                   THREAD_ENTRY_FREE) == THREAD_ENTRY_FREE)
      return entry;
  }

  fatal("Thread table full");
  return NULL;
}

static thread_map_entry_t* thread_map_lookup(DWORD id) {
  for (size_t i = 0; i < MAX_THREADS; i++) {
    thread_map_entry_t* entry = &thread_map[i];
    if (entry->state != THREAD_ENTRY_FREE && entry->id == id)
      return entry;
  }

  fatal("Thread id not found in thread table");
  return INVALID_HANDLE_VALUE;
}

static void thread_map_free(thread_map_entry_t* entry) {
  if (entry == NULL)
    return;

  if (entry->state == THREAD_ENTRY_FREE)
    fatal("Thread map entry not in use");

  memset(entry, 0, sizeof *entry);
}


static void thread_exit_cleanup(thread_map_entry_t* entry) {
  if (InterlockedCompareExchange(
          &entry->state, THREAD_ENTRY_EXITED, THREAD_ENTRY_IN_USE) ==
      THREAD_ENTRY_DETACHED) {
    CloseHandle(entry->handle);
    thread_map_free(entry);
  }
}


static unsigned int _stdcall thread_trampoline(void* entry_ptr) {
  thread_map_entry_t* entry = entry_ptr;
  void* arg = entry->in.arg;
  pthread_start_routine_t start_routine = entry->in.start_routine;

  entry->out.retval = (void*) (intptr_t) -1;

  void* retval = start_routine(arg);
  entry->out.retval = retval;

  thread_exit_cleanup(entry);

  return (DWORD)(uintptr_t) retval;
}


int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg) {
  thread_map_entry_t* entry = thread_map_alloc();
  assert(attr == NULL);

  unsigned int id;
  HANDLE handle;

  entry->in.start_routine = start_routine;
  entry->in.arg = arg;

  handle = (HANDLE) _beginthreadex(NULL, 0, thread_trampoline, entry, 0, &id);
  if (handle == 0 || handle == INVALID_HANDLE_VALUE) {
    thread_map_free(entry);
    return errno;
  }

  entry->id = id;
  entry->handle = handle;

  *thread = id;
  return 0;
}


int pthread_detach(pthread_t thread) {
  thread_map_entry_t* entry = thread_map_lookup(thread);

  if (entry->state != THREAD_ENTRY_IN_USE &&
      entry->state != THREAD_ENTRY_EXITED)
    fatal("Thread in unexpected state");

  if (InterlockedCompareExchange(&entry->state,
                                 THREAD_ENTRY_DETACHED,
                                 THREAD_ENTRY_IN_USE) == THREAD_ENTRY_EXITED) {
    CloseHandle(entry->handle);
    thread_map_free(entry);
  }

  return 0;
}

int pthread_join(pthread_t thread, void** retval) {
  thread_map_entry_t* entry = thread_map_lookup(thread);

  if (WaitForSingleObject(entry->handle, INFINITE) != WAIT_OBJECT_0) {
    fatal("WaitForSingleObject failure");
  }

  if (retval != NULL)
    *retval = entry->out.retval;

  CloseHandle(entry->handle);
  thread_map_free(entry);

  return 0;
}

pthread_t pthread_self(void) {
  return GetCurrentThreadId();
}

void pthread_exit(void* retval) {
  thread_map_entry_t* entry = thread_map_lookup(pthread_self());

  entry->out.retval = retval;

  thread_exit_cleanup(entry);
  _endthreadex((unsigned int) (uintptr_t) retval);
}

int pthread_yield(void) {
  Sleep(0);
  return 0;
}

long int random(void) {
  abort();
}

void srandom(unsigned int seed) {
  abort();
}

unsigned int sleep(unsigned int seconds) {
  Sleep(seconds * 1000);
  return 0;
}


/* socketpair.c
Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
The name of the author must not be used to endorse or promote products
derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

int socketpair(int domain, int type, int protocol, int socks[2]) {
  {
    union {
      struct sockaddr_in inaddr;
      struct sockaddr addr;
    } a;
    SOCKET listener;
    int e;
    socklen_t addrlen = sizeof(a.inaddr);
    DWORD flags = WSA_FLAG_OVERLAPPED;
    int reuse = 1;

    if (domain != AF_INET) {
      WSASetLastError(WSAEAFNOSUPPORT);
      return SOCKET_ERROR;
    }
    if (type != SOCK_STREAM) {
      WSASetLastError(WSAESOCKTNOSUPPORT);
      return SOCKET_ERROR;
    }
    if (protocol != IPPROTO_TCP) {
      WSASetLastError(WSAEPROTONOSUPPORT);
      return SOCKET_ERROR;
    }

    if (socks == 0) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
    socks[0] = socks[1] = -1;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
      return SOCKET_ERROR;

    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;

    for (;;) {
      if (setsockopt(listener,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     (char*) &reuse,
                     (socklen_t) sizeof(reuse)) == -1)
        break;
      if (bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
        break;

      memset(&a, 0, sizeof(a));
      if (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
        break;
      // win32 getsockname may only set the port number, p=0.0005.
      // ( http://msdn.microsoft.com/library/ms738543.aspx ):
      a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      a.inaddr.sin_family = AF_INET;

      if (listen(listener, 1) == SOCKET_ERROR)
        break;

      socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
      if (socks[0] == -1)
        break;
      if (connect(socks[0], &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
        break;

      socks[1] = accept(listener, NULL, NULL);
      if (socks[1] == -1)
        break;

      closesocket(listener);
      return 0;
    }

    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    socks[0] = socks[1] = -1;
    return SOCKET_ERROR;
  }
}

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  return WSAPoll(fds, nfds, timeout);
}

void istgt_platform_init(void) {
  _set_fmode(_O_BINARY);

  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    fatal("WSAStartup failed");
}

#endif  // _WIN32
