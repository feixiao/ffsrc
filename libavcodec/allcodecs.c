#include "avcodec.h"

extern AVCodec truespeech_decoder;
extern AVCodec msrle_decoder;

// 简单的注册/初始化函数，把编解码器用相应的链表串起来便于查找识别。
void avcodec_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
	return;

    inited = 1;
    // 把msrle_decoder 解码器串接到解码器链表，链表头指针是first_avcodec。
    register_avcodec(&msrle_decoder);
    // 把truespeech_decoder 解码器串接到解码器链表，链表头指针是first_avcodec。
    register_avcodec(&truespeech_decoder);
}
