#include "istgt_control_pipe.h"
#include "istgt_platform.h"


#ifndef _WIN32

int istgt_control_pipe_create(istgt_control_pipe_t* control_pipe) {
  control_pipe->fd[0] = -1;
  control_pipe->fd[1] = -1;

  return pipe(control_pipe->fd);
}

void istgt_control_pipe_destroy(istgt_control_pipe_t* control_pipe) {
  if (control_pipe->fd[0] != -1)
    close(control_pipe->fd[0]);

  if (control_pipe->fd[1] != -1)
    close(control_pipe->fd[1]);

  control_pipe->fd[0] = -1;
  control_pipe->fd[1] = -1;
}

ssize_t istgt_control_pipe_read(istgt_control_pipe_t* control_pipe,
                                void* buf,
                                size_t count) {
  return read(control_pipe->fd[0], buf, count);
}

ssize_t istgt_control_pipe_write(istgt_control_pipe_t* control_pipe,
                                 const void* buf,
                                 size_t count) {
  return write(control_pipe->fd[1], buf, count);
}


#else  // _WIN32

int istgt_control_pipe_create(istgt_control_pipe_t* control_pipe) {
  control_pipe->fd[0] = -1;
  control_pipe->fd[1] = -1;

  if (socketpair(AF_INET, SOCK_STREAM, IPPROTO_IP, control_pipe->fd) ==
      SOCKET_ERROR)
    return -1;

  return 0;
}

void istgt_control_pipe_destroy(istgt_control_pipe_t* control_pipe) {
  if (control_pipe->fd[0] != -1)
    closesocket(control_pipe->fd[0]);
  if (control_pipe->fd[1] != -1)
    closesocket(control_pipe->fd[1]);

  control_pipe->fd[0] = -1;
  control_pipe->fd[1] = -1;
}

ssize_t istgt_control_pipe_read(istgt_control_pipe_t* control_pipe,
                                void* buf,
                                size_t count) {
  return recv(control_pipe->fd[2], buf, count, 0);
}

ssize_t istgt_control_pipe_write(istgt_control_pipe_t* control_pipe,
                                 const void* buf,
                                 size_t count) {
  return send(control_pipe->fd[1], buf, count, 0);
}

#endif  // _WIN32
