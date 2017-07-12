#include "avformat.h"

// 简单的注册/初始化函数，把相应的协议，文件格式，解码器等用相应的链表串起来便于查找。

extern URLProtocol file_protocol;

void av_register_all(void)
{
    // inited 变量声明成static，做一下比较是为了避免此函数多次调用。
    static int inited = 0;

    if (inited != 0)
	return;
    inited = 1;
    // ffplay 把CPU 当做一个广义的DSP。有些计算可以用CPU 自带的加速指令来优化，
    // ffplay 把这类函数独立出来放到dsputil.h 和dsputil.c 文件中，用函数指针的方法映射到各个CPU 具体的加速优化实现函数，此处初始化这些函数指针。
    avcodec_init();

    // 把所有的解码器用链表的方式都串连起来，链表头指针是first_avcodec。
    avcodec_register_all();

    // 把所有的输入文件格式用链表的方式都串连起来，链表头指针是first_iformat。
    avidec_init();
    // 把所有的输入协议用链表的方式都串连起来，比如tcp/udp/file 等，链表头指针是first_protocol。
    register_protocol(&file_protocol);
}
