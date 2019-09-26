#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <bufio.hpp>

void BufferedIO::openfile(const char *path, int mode) {
    this->fd = open(path, mode, 0777);
    this->reset();
}

void BufferedIO::closefile() {
    close(this->fd);
}

void BufferedIO::flush() {
    if (this->written > 0) {
        pwrite(this->fd, this->buf, this->written, this->offset);
        this->offset += this->written;
        this->written = 0;
        this->ptr = this->buf;
    }
}

void BufferedIO::setbuf(byte *buf, size_t bufsiz) {
    this->bufsiz = bufsiz;
    this->buf = buf;
    this->reset();
}

void BufferedIO::reset() {
    this->written = 0;
    this->offset = 0;
    this->ptr = buf;
}

ssize_t BufferedIO::read(void *buf, size_t nbytes) {
    ssize_t readbytes = pread(this->fd, buf, nbytes, this->offset);
    if (readbytes >= 0) {
        this->offset += readbytes;
    }
    return readbytes;
}

ssize_t BufferedIO::append(void *buf, size_t nbytes) {
    if (this->ptr >= this->buf + this->bufsiz) {
        this->flush();
    }

    memcpy(this->ptr, buf, nbytes);
    this->ptr += nbytes;
    this->written += nbytes;
    return nbytes;
}

size_t BufferedIO::filesize() {
    struct stat st;
    fstat(this->fd, &st);
    return (size_t)st.st_size;
}

int BufferedIO::getfd() {
    return this->fd;
}
