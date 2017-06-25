#include "avformat.h"

// 简单的注册/初始化函数，把相应的协议，文件格式，解码器等用相应的链表串起来便于查找。

extern URLProtocol file_protocol;

void av_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
        return ;
    inited = 1;

    avcodec_init();

    avcodec_register_all();

    avidec_init();

    register_protocol(&file_protocol);
}
