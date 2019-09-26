#ifndef __BUFIO_HPP
#define __BUFIO_HPP
#include <sys/types.h>
#include <mytypes.hpp>

typedef struct {
    int fd;
    size_t bufsiz, written;
    off_t offset;
    byte *buf;
    byte *ptr;
} buffered_io_fd;

void buffered_reset(buffered_io_fd *fd);

buffered_io_fd * buffered_open(const char* path, int mode, byte* buf, size_t bufsiz);

void buffered_flush(buffered_io_fd *fd);

void buffered_close(buffered_io_fd *fd);

ssize_t buffered_read(buffered_io_fd *fd, void *buf, size_t nbytes);

ssize_t buffered_append(buffered_io_fd *fd, void *buf, size_t nbytes);

#endif
