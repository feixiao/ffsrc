#ifndef RATIONAL_H
#define RATIONAL_H

typedef struct AVRational
{
    int num; // numerator   // ·Ö×Ó
    int den; // denominator // ·ÖÄ¸
} AVRational;

static inline double av_q2d(AVRational a)
{
    return a.num / (double)a.den;
}

#endif
