#include "../berrno.h"
#include "avformat.h"

URLProtocol *first_protocol = NULL;
// 把URLProtocol 串联起来做成链表，便于查找。
// register_protocol 实际就是串联的各个URLProtocol，全局表头为first_protocol。
int register_protocol(URLProtocol *protocol)
{
    URLProtocol **p;
    p = &first_protocol;
    while (*p != NULL)
        p = &(*p)->next;
    *p = protocol;
    protocol->next = NULL;
    return 0;
}
// 打开广义输入文件。此函数主要有三部分逻辑，首先从文件路径名中分离出协议字符串到proto_str字符数组中，
// 接着遍历URLProtocol 链表查找匹配proto_str字符数组中的字符串来确定使用的协议，最后调用相应的文件协议的打开函数打开输入文件。
int url_open(URLContext **puc, const char *filename, int flags)
{
    URLContext *uc;
    URLProtocol *up;
    const char *p;
    char proto_str[128],  *q;
    int err;

// 以冒号和结束符作为边界从文件名中分离出的协议字符串到proto_str 字符数组
    p = filename;
    q = proto_str;
    while (*p != '\0' &&  *p != ':')
    {
        if (!isalpha(*p))  // protocols can only contain alphabetic chars
            goto file_proto;
        if ((q - proto_str) < sizeof(proto_str) - 1)
            *q++ =  *p;
        p++;
    }
    // if the protocol has length 1, we consider it is a dos drive
    if (*p == '\0' || (q - proto_str) <= 1)
    {
file_proto: 
		strcpy(proto_str, "file");
    }
    else
    {
        *q = '\0';
    }

    up = first_protocol;
    while (up != NULL)
    {
        if (!strcmp(proto_str, up->name))
            goto found;
        up = up->next;
    }
    err =  - ENOENT;
    goto fail;
found: 
	uc = av_malloc(sizeof(URLContext) + strlen(filename));
    if (!uc)
    {
        err =  - ENOMEM;
        goto fail;
    }
    strcpy(uc->filename, filename);
    uc->prot = up;
    uc->flags = flags;
    uc->max_packet_size = 0; // default: stream file
    err = up->url_open(uc, filename, flags);
    if (err < 0)
    {
        av_free(uc);
        *puc = NULL;
        return err;
    }
    *puc = uc;
    return 0;
fail:
	*puc = NULL;
    return err;
}

int url_read(URLContext *h, unsigned char *buf, int size)
{
    int ret;
    if (h->flags &URL_WRONLY)
        return AVERROR_IO;
    ret = h->prot->url_read(h, buf, size);
    return ret;
}

offset_t url_seek(URLContext *h, offset_t pos, int whence)
{
    offset_t ret;

    if (!h->prot->url_seek)
        return  - EPIPE;
    ret = h->prot->url_seek(h, pos, whence);
    return ret;
}

int url_close(URLContext *h)
{
    int ret;

    ret = h->prot->url_close(h);
    av_free(h);
    return ret;
}

int url_get_max_packet_size(URLContext *h)
{
    return h->max_packet_size;
}
