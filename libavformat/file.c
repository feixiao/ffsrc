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

// ffplay把file当做类似于rtsp，rtp，tcp 等协议的一种协议，用file:前缀标示file协议。
// URLContext结构抽象统一表示这些广义上的协议，对外提供统一的抽象接口。
// 各具体的广义协议实现文件实现URLContext 接口。此文件实现了file 广义协议的URLContext 接口。

// 打开本地媒体文件，把本地文件句柄作为广义文件句柄存放在priv_data中。
static int file_open(URLContext *h, const char *filename, int flags)
{
    int access;
    int fd;
    // 规整本地路径文件名，去掉前面可能的"file:"字符串
    strstart(filename, "file:", &filename);
    // 设置本地文件存取属性。
    if (flags &	URL_RDWR)
	access = O_CREAT | O_TRUNC | O_RDWR;
    else if (flags & URL_WRONLY)
	access = O_CREAT | O_TRUNC | O_WRONLY;
    else
	access = O_RDONLY;
#if defined(CONFIG_WIN32) || defined(CONFIG_OS2) || defined(__CYGWIN__)
    access |= O_BINARY;
#endif
    // 调用open()打开本地文件，并把本地文件句柄作为广义的URL句柄存放在priv_data变量中。
    fd = open(filename, access, 0666);
    if (fd < 0)
	return  -ENOENT;
    h->priv_data = (void*)(size_t)fd;
    return 0;
}

// 转换广义URL句柄为本地文件句柄，调用read()函数读本地文件。
static int file_read(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return read(fd, buf, size);
}
// 转换广义URL句柄为本地文件句柄，调用wite()函数写本地文件，本播放器没实际使用此函数。
static int file_write(URLContext *h, unsigned char *buf, int size)
{
    int fd = (size_t)h->priv_data;
    return write(fd, buf, size);
}
// 转换广义URL句柄为本地文件句柄，调用lseek()函数设置本地文件读指针。
static offset_t file_seek(URLContext *h, offset_t pos, int whence)
{
    int fd = (size_t)h->priv_data;
    return lseek(fd, pos, whence);
}
// 转换广义URL 句柄为本地文件句柄，调用close()函数关闭本地文件。
static int file_close(URLContext *h)
{
    int fd = (size_t)h->priv_data;
    return close(fd);
}

// 用file协议相应函数初始化URLProtocol 结构。
URLProtocol file_protocol =
{
	"file",
	file_open,
	file_read,
	file_write,
	file_seek,
	file_close,
};

// https://github.com/feixiao/ffmpeg-2.8.11/blob/master/libavformat/file.c

// 其他协议如RTMP
// https://github.com/feixiao/ffmpeg-2.8.11/blob/master/libavformat/librtmp.c
