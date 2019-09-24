#include <buffered_io.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

void buffered_reset(buffered_io_fd *fd) {
    fd->offset = 0;
    fd->written = 0;
    fd->ptr = fd->buf;
}

buffered_io_fd * buffered_open(const char* path, int mode, byte* buf, size_t bufsiz) {
    buffered_io_fd *io = (buffered_io_fd*)malloc(sizeof(buffered_io_fd));
    if (io == NULL) return NULL;
    int fd = open(path, mode, 0777);
    if (fd < 0) return NULL;
    io->bufsiz = bufsiz;
    io->fd = fd;
    io->buf = buf;
    buffered_reset(io);
    return io;
}

void buffered_flush(buffered_io_fd *fd) {
    if (fd->written > 0) {
        pwrite(fd->fd, fd->buf, fd->written, fd->offset);
        fd->offset += fd->written;
        fd->written = 0;
        fd->ptr = fd->buf;
    }
}

void buffered_close(buffered_io_fd *fd) {
    // buffered_flush(fd);
    close(fd->fd);
    free(fd);
}

ssize_t buffered_read(buffered_io_fd *fd, void *buf, size_t nbytes) {
    ssize_t readbytes = pread(fd->fd, buf, nbytes, fd->offset);
    if (readbytes >= 0) {
        fd->offset += readbytes;
    }

    return readbytes;
}

ssize_t buffered_append(buffered_io_fd *fd, void *buf, size_t nbytes) {
    if (fd->ptr >= fd->buf + fd->bufsiz) {
        buffered_flush(fd);
        pwrite(fd->fd, fd->buf, fd->written, fd->offset);
        fd->offset += fd->written;
        fd->written = 0;
        fd->ptr = fd->buf;
    }

    memcpy(fd->ptr, buf, nbytes);
    fd->ptr += nbytes;
    fd->written += nbytes;
    return nbytes;
}
