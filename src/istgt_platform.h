#ifndef ISTGT_PLATFORM_H
#define ISTGT_PLATFORM_H

void istgt_platform_init(void);


#ifndef _WIN32
/* == POSIX =============================================================== */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef HAVE_SCHED
#include <sched.h>
#endif

#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif

#ifdef HAVE_SYS_DISK_H
#include <sys/disk.h>
#endif

#ifdef HAVE_SYS_DISKLABEL_H
#include <sys/disklabel.h>
#endif

#ifdef __linux__
#include <linux/fs.h>
#endif


#else  // _WIN32
/* == WINDOWS ============================================================== */

#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE

#define HAVE_PTHREAD_YIELD

#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <io.h>
#include <process.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define snprintf _snprintf
#define vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#define SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#define SSIZE_MAX INTPTR_MAX

struct iovec {
  size_t iov_len;
  void* iov_base;
};

typedef DWORD pid_t;

typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

typedef DWORD pthread_t;

typedef struct {
  void* dummy;
} pthread_attr_t, pthread_mutexattr_t, pthread_condattr_t;

typedef int socklen_t;
typedef ULONG nfds_t;

int socketpair(int domain, int type, int protocolint, int sv[2]);
int poll(struct pollfd* fds, nfds_t nfds, int timeout);

int fsync(int fd);

#define S_ISCHR(mode) 0
#define S_ISBLK(mode) 0
#define S_ISREG(mode) (!!(mode & _S_IFREG))

int pthread_mutex_init(pthread_mutex_t* mutex,
                       const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);

int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_timedwait(pthread_cond_t* cond,
                           pthread_mutex_t* mutex,
                           const struct timespec* abstime);

int pthread_create(pthread_t* thread,
                   const pthread_attr_t* attr,
                   void* (*start_routine)(void*),
                   void* arg);
int pthread_detach(pthread_t thread);
int pthread_join(pthread_t thread, void** retval);

pthread_t pthread_self(void);
void pthread_exit(void* retval);
int pthread_yield(void);

long int random(void);
void srandom(unsigned int seed);

unsigned int sleep(unsigned int seconds);

#endif  // _WIN32
#endif  // ISTGT_PLATFORM_H
