#include <assert.h>
#include "avcodec.h"
#include "dsputil.h"

// 编解码库使用的帮助和工具函数
#define EDGE_WIDTH   16
#define STRIDE_ALIGN 16

#define INT_MAX 2147483647

#define FFMAX(a,b) ((a) > (b) ? (a) : (b))

// 内存动态分配函数，做一下简单参数校验后调用系统函数
void *av_malloc(unsigned int size)
{
    void *ptr;

    if (size > INT_MAX)
	return NULL;
    ptr = malloc(size);

    return ptr;
}
// 内存动态重分配函数，做一下简单参数校验后调用系统函数
void *av_realloc(void *ptr, unsigned int size)
{
    if (size > INT_MAX)
	return NULL;

    return realloc(ptr, size);
}
// 内存动态释放函数，做一下简单参数校验后调用系统函数
void av_free(void *ptr)
{
    if (ptr)
	free(ptr);
}
// 内存动态分配函数，复用av_malloc()函数，再把分配的内存清0
void *av_mallocz(unsigned int size)
{
    void *ptr;

    ptr = av_malloc(size);
    if (!ptr)
	return NULL;

    memset(ptr, 0, size);
    return ptr;
}
// 快速内存动态分配函数，预分配一些内存来避免多次调用系统函数达到快速的目的
void *av_fast_realloc(void *ptr, unsigned int *size, unsigned int min_size)
{
    if (min_size < *size)
	return ptr;

    *size = FFMAX(17 * min_size / 16 + 32, min_size);

    return av_realloc(ptr, *size);
}
// 动态内存释放函数，注意传入的变量的类型。
void av_freep(void *arg)
{
    void **ptr = (void **)arg;
    av_free(*ptr);
    *ptr = NULL;
}

AVCodec *first_avcodec = NULL;

// 把编解码器串联成一个链表，便于查找。
void register_avcodec(AVCodec *format)
{
    AVCodec **p;
    p = &first_avcodec;
    while (*p != NULL)
	p = &(*p)->next;
    *p = format;
    format->next = NULL;
}
// 编解码库内部使用的缓存区，因为视频图像有RGB 或YUV 分量格式，所以每个数组有四个分量。
typedef struct InternalBuffer
{
    uint8_t *base[4];
    uint8_t *data[4];
    int  linesize[4];
} InternalBuffer;

#define INTERNAL_BUFFER_SIZE 32

#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))
// 计算各种图像格式要求的图像长宽的字节对齐数，是1 个还是2 个，4 个，8 个，16 个字节对齐。
void avcodec_align_dimensions(AVCodecContext *s, int *width, int *height)
{
    // 默认长宽是1 个字节对齐。
    int w_align = 1;
    int h_align = 1;

    switch (s->pix_fmt)
    {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUV422:
    case PIX_FMT_UYVY422:
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUV444P:
    case PIX_FMT_GRAY8:
    case PIX_FMT_YUVJ420P:
    case PIX_FMT_YUVJ422P:
    case PIX_FMT_YUVJ444P: //FIXME check for non mpeg style codecs and use less alignment
	w_align = 16;
	h_align = 16;
	break;
    case PIX_FMT_YUV411P:
    case PIX_FMT_UYVY411:
	w_align = 32;
	h_align = 8;
	break;
    case PIX_FMT_YUV410P:
    case PIX_FMT_RGB555:
    case PIX_FMT_PAL8:
	break;
    case PIX_FMT_BGR24:
	break;
    default:
	w_align = 1;
	h_align = 1;
	break;
    }

    *width = ALIGN(*width, w_align);
    *height = ALIGN(*height, h_align);
}
// 校验视频图像的长宽是否合法。
int avcodec_check_dimensions(void *av_log_ctx, unsigned int w, unsigned int h)
{
    if ((int)w > 0 && (int)h > 0 && (w + 128)*(uint64_t)(h + 128) < INT_MAX / 4)
	return 0;

    return  -1;
}
// 每次取internal_buffer_count 数据项，用base[0]来判断是否已分配内存，用data[0]来判断是否已被占用。base[]和data[]有多重意义。
// 在avcodec_alloc_context 中已把internal_buffer 各项清0，所以可以用base[0]来判断。
int avcodec_default_get_buffer(AVCodecContext *s, AVFrame *pic)
{
    int i;
    int w = s->width;
    int h = s->height;
    int align_off;
    InternalBuffer *buf;

    assert(pic->data[0] == NULL);
    assert(INTERNAL_BUFFER_SIZE > s->internal_buffer_count);
    // 校验视频图像的长宽是否合法。
    if (avcodec_check_dimensions(s, w, h))
	return  -1;
    // 如果没有分配内存，就分配动态内存并清0。
    if (s->internal_buffer == NULL)
	s->internal_buffer = av_mallocz(INTERNAL_BUFFER_SIZE * sizeof(InternalBuffer));
    // 取缓存中的第一个没有占用内存。
    buf = &((InternalBuffer*)s->internal_buffer)[s->internal_buffer_count];

    if (buf->base[0])
    {
	// 如果内存已分配就跳过
    }
    else
    {
	// 如果没有分配内存就按照图像格式要求分配内存，并设置一些标记和计算一些参数值。
	int h_chroma_shift, v_chroma_shift;
	int pixel_size, size[3];


	AVPicture picture;
	// 计算CbCr 色度分量长宽的与Y 亮度分量长宽的比，最后用移位实现。
	avcodec_get_chroma_sub_sample(s->pix_fmt, &h_chroma_shift, &v_chroma_shift);
	// 规整长宽满足特定图像像素格式的要求。
	avcodec_align_dimensions(s, &w, &h);

	// 把长宽放大一些，比如在mpeg4 视频中编码算法中的运动估计要把原始图像做扩展来满足不受限制运动矢量的要求(运动矢量可以超出原始图像边界)。
	w += EDGE_WIDTH * 2;
	h += EDGE_WIDTH * 2;
	// 计算特定格式的图像参数，包括各分量的大小，单行长度(linesize/stride)等等。
	avpicture_fill(&picture, NULL, s->pix_fmt, w, h);
	pixel_size = picture.linesize[0] * 8 / w;
	assert(pixel_size >= 1);

	if (pixel_size == 3 * 8)
	    w = ALIGN(w, STRIDE_ALIGN << h_chroma_shift);
	else
	    w = ALIGN(pixel_size *w, STRIDE_ALIGN << (h_chroma_shift + 3)) / pixel_size;

	size[1] = avpicture_fill(&picture, NULL, s->pix_fmt, w, h);
	size[0] = picture.linesize[0] * h;
	size[1] -= size[0];
	if (picture.data[2])
	    size[1] = size[2] = size[1] / 2;
	else
	    size[2] = 0;

	// 注意base[]和data[]数组还有作为标记的用途，free()时的非NULL 判断，这里要清0。
	memset(buf->base, 0, sizeof(buf->base));
	memset(buf->data, 0, sizeof(buf->data));

	for (i = 0; i < 3 && size[i]; i++)
	{
	    const int h_shift = i == 0 ? 0 : h_chroma_shift;
	    const int v_shift = i == 0 ? 0 : v_chroma_shift;

	    buf->linesize[i] = picture.linesize[i];
	    // 实质性分配内存，并且在202 行把内存清0。
	    buf->base[i] = av_malloc(size[i] + 16); //FIXME 16
	    if (buf->base[i] == NULL)
		return  -1;
	    memset(buf->base[i], 128, size[i]);
	    // 内存对齐计算。
	    align_off = ALIGN((buf->linesize[i] * EDGE_WIDTH >> v_shift) + (EDGE_WIDTH >> h_shift), STRIDE_ALIGN);

	    if ((s->pix_fmt == PIX_FMT_PAL8) || !size[2])
		buf->data[i] = buf->base[i];
	    else
		buf->data[i] = buf->base[i] + align_off;
	}
    }

    for (i = 0; i < 4; i++)
    {
	// 把分配的内存参数赋值到pic 指向的结构中，传递出去。
	pic->base[i] = buf->base[i];
	pic->data[i] = buf->data[i];
	pic->linesize[i] = buf->linesize[i];
    }
    // 内存数组计数+1，注意释放时的操作，保证计数对应的内存数组是空闲的。
    s->internal_buffer_count++;

    return 0;
}

// 释放占用的内存数组项。保证从0 到internal_buffer_count-1 数据项为有效数据，其他是空闲数据项
void avcodec_default_release_buffer(AVCodecContext *s, AVFrame *pic)
{
    int i;
    InternalBuffer *buf, *last, temp;
    // 简单的参数校验，内存必须是已经分配过。
    assert(s->internal_buffer_count);

    buf = NULL;
    for (i = 0; i < s->internal_buffer_count; i++)
    {
	// 遍历内存数组，查找对应pic 的内存数组项，以data[0]内存地址为比较判别标记。
	buf = &((InternalBuffer*)s->internal_buffer)[i]; //just 3-5 checks so is not worth to optimize
	if (buf->data[0] == pic->data[0])
	    break;
    }
    assert(i < s->internal_buffer_count);
    // 内存数组计数-1, 删除最后一项.
    s->internal_buffer_count--;
    last = &((InternalBuffer*)s->internal_buffer)[s->internal_buffer_count];

    // 把将要空闲的数组项和数组最后一项交换，保证internal_buffer_count 计算正确无误。
    // 注意这里并没有内存释放的动作，便于下次复用已分配的内存
    temp = *buf;
    *buf = *last;
    *last = temp;

    for (i = 0; i < 3; i++)
    {
	pic->data[i] = NULL;
    }
}

int avcodec_default_reget_buffer(AVCodecContext *s, AVFrame *pic)
{
    if (pic->data[0] == NULL)  // If no picture return a new buffer
    {
	return s->get_buffer(s, pic);
    }

    return 0;
}

void avcodec_default_free_buffers(AVCodecContext *s)
{
    int i, j;

    if (s->internal_buffer == NULL)
	return;

    for (i = 0; i < INTERNAL_BUFFER_SIZE; i++)
    {
	InternalBuffer *buf = &((InternalBuffer*)s->internal_buffer)[i];
	for (j = 0; j < 4; j++)
	{
	    av_freep(&buf->base[j]);
	    buf->data[j] = NULL;
	}
    }
    av_freep(&s->internal_buffer);

    s->internal_buffer_count = 0;
}

AVCodecContext *avcodec_alloc_context(void)
{
    AVCodecContext *s = av_malloc(sizeof(AVCodecContext));

    if (s == NULL)
	return NULL;

    memset(s, 0, sizeof(AVCodecContext));

    s->get_buffer = avcodec_default_get_buffer;
    s->release_buffer = avcodec_default_release_buffer;

    s->pix_fmt = PIX_FMT_NONE;

    s->palctrl = NULL;
    s->reget_buffer = avcodec_default_reget_buffer;

    return s;
}

int avcodec_open(AVCodecContext *avctx, AVCodec *codec)
{
    int ret = -1;

    if (avctx->codec)
	goto end;

    if (codec->priv_data_size > 0)
    {
	avctx->priv_data = av_mallocz(codec->priv_data_size);
	if (!avctx->priv_data)
	    goto end;
    }
    else
    {
	avctx->priv_data = NULL;
    }

    avctx->codec = codec;
    avctx->codec_id = codec->id;
    avctx->frame_number = 0;
    ret = avctx->codec->init(avctx);
    if (ret < 0)
    {
	av_freep(&avctx->priv_data);
	avctx->codec = NULL;
	goto end;
    }
    ret = 0;
end:
    return ret;
}

int avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr,
    uint8_t *buf, int buf_size)
{
    int ret;

    *got_picture_ptr = 0;

    if (buf_size)
    {
	ret = avctx->codec->decode(avctx, picture, got_picture_ptr, buf, buf_size);

	if (*got_picture_ptr)
	    avctx->frame_number++;
    }
    else
	ret = 0;

    return ret;
}

int avcodec_decode_audio(AVCodecContext *avctx, int16_t *samples, int *frame_size_ptr,
    uint8_t *buf, int buf_size)
{
    int ret;

    *frame_size_ptr = 0;
    if (buf_size)
    {
	ret = avctx->codec->decode(avctx, samples, frame_size_ptr, buf, buf_size);
	avctx->frame_number++;
    }
    else
	ret = 0;
    return ret;
}

int avcodec_close(AVCodecContext *avctx)
{
    if (avctx->codec->close)
	avctx->codec->close(avctx);
    avcodec_default_free_buffers(avctx);
    av_freep(&avctx->priv_data);
    avctx->codec = NULL;
    return 0;
}

AVCodec *avcodec_find_decoder(enum CodecID id)
{
    AVCodec *p;
    p = first_avcodec;
    while (p)
    {
	if (p->decode != NULL && p->id == id)
	    return p;
	p = p->next;
    }
    return NULL;
}

void avcodec_init(void)
{
    static int inited = 0;

    if (inited != 0)
	return;
    inited = 1;

    dsputil_static_init();
}
