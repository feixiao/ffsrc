#include "../berrno.h"

#include "avformat.h"
#include <fcntl.h>

#ifndef CONFIG_WIN32
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#else
#include <io.h>
#define open(fname,oflag,pmode) _open(fname,oflag,pmode)
#endif

static int file_open(URLContext *h, const char *filename, int flags)
{
    int access;
    int fd;

    strstart(filename, "file:", &filename);

    if (flags &URL_RDWR)
        access = O_CREAT | O_TRUNC | O_RDWR;
    else if (flags &URL_WRONLY)
        access = O_CREAT | O_TRUNC | O_WRONLY;
    else
        access = O_RDONLY;
#if defined(CONFIG_WIN32) || defined(CONFIG_OS2) || defined(__CYGWIN__)
    access |= O_BINARY;
#endif
    fd = open(filename, access, 0666);
    if (fd < 0)
        return  - ENOENT;
    h->priv_data = (void*)(size_t)fd;
    return 0;
}

static int file_read(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return read(fd, buf, size);
}

static int file_write(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return write(fd, buf, size);
}

static offset_t file_seek(URLContext *h, offset_t pos, int whence)
{
    int fd = (size_t)h->priv_data;
    return lseek(fd, pos, whence);
}

static int file_close(URLContext *h)
{
    int fd = (size_t)h->priv_data;
    return close(fd);
}

URLProtocol file_protocol =
{
    "file",
	file_open,
	file_read,
	file_write,
	file_seek,
	file_close,
};
