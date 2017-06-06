#ifndef ISTGT_SIG_PIPE_H
#define ISTGT_SIG_PIPE_H

#include "istgt_platform.h"

typedef struct { int fd[2]; } istgt_control_pipe_t;

static inline istgt_control_pipe_t istgt_control_pipe_init(void) {
  istgt_control_pipe_t pipe = {.fd = {-1, -1}};
  return pipe;
}

int istgt_control_pipe_create(istgt_control_pipe_t* control_pipe);
void istgt_control_pipe_destroy(istgt_control_pipe_t* control_pipe);

ssize_t istgt_control_pipe_read(istgt_control_pipe_t* control_pipe,
                                void* buf,
                                size_t count);
ssize_t istgt_control_pipe_write(istgt_control_pipe_t* control_pipe,
                                 const void* buf,
                                 size_t count);

#endif  // ISTGT_SIG_PIPE_H
