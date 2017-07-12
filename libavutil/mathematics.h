#ifndef MATHEMATICS_H
#define MATHEMATICS_H

// 数学上的缩放运算。为避免计算误差，缩放因子用两整数表示做精确的整数运算。为防止计算溢出，强制转换为int 64 位整数后计算。
// 此处做了一些简化，运算精度会降低，但普通的人很难感知到计算误差。
static inline int64_t av_rescale(int64_t a, int64_t b, int64_t c)
{
    return a *b / c;
}

#endif
