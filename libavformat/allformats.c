#include "avformat.h"

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
