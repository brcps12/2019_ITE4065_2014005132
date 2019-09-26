#ifndef __BUFIO_H
#define __BUFIO_H
#include <sys/types.h>
#include <mytypes.hpp>

class BufferedIO {
private:
    int fd;
    size_t bufsiz, written;
    off_t offset;
    byte *buf;
    byte *ptr;
public:
    BufferedIO() {
        bufsiz = 0;
        reset();
    }

    void reset();

    void openfile(const char *path, int mod);

    void closefile();

    void flush();

    ssize_t read(void *buf, size_t nbytes);

    ssize_t append(void *buf, size_t nbytes);

    int getfd();

    void setbuf(byte *buf, size_t bufsiz);

    size_t filesize();
};

#endif
