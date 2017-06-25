#ifndef AVFORMAT_H
#define AVFORMAT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define LIBAVFORMAT_VERSION_INT ((50<<16)+(4<<8)+0)
#define LIBAVFORMAT_VERSION     50.4.0
#define LIBAVFORMAT_BUILD       LIBAVFORMAT_VERSION_INT

#define LIBAVFORMAT_IDENT       "Lavf" AV_STRINGIFY(LIBAVFORMAT_VERSION)

#include "../libavcodec/avcodec.h"
#include "avio.h"

#define AVERROR_UNKNOWN     (-1)	// unknown error
#define AVERROR_IO          (-2)	// i/o error
#define AVERROR_NUMEXPECTED (-3)	// number syntax expected in filename
#define AVERROR_INVALIDDATA (-4)	// invalid data found
#define AVERROR_NOMEM       (-5)	// not enough memory
#define AVERROR_NOFMT       (-6)	// unknown format
#define AVERROR_NOTSUPP     (-7)	// operation not supported

#define AVSEEK_FLAG_BACKWARD 1		// seek backward
#define AVSEEK_FLAG_BYTE     2		// seeking based on position in bytes
#define AVSEEK_FLAG_ANY      4		// seek to any frame, even non keyframes

#define AVFMT_NOFILE        0x0001	// no file should be opened

#define PKT_FLAG_KEY		0x0001

#define AVINDEX_KEYFRAME	0x0001

#define AVPROBE_SCORE_MAX	100

#define MAX_STREAMS 20

// 代表音视频数据帧，固有的属性是一些标记，时钟信息，和压缩数据首地址，大小等信息。
typedef struct AVPacket
{
    int64_t pts; // presentation time stamp in time_base units
    int64_t dts; // decompression time stamp in time_base units
    int64_t pos; // byte position in stream, -1 if unknown
    uint8_t *data;
    int size;
    int stream_index;
    int flags;
    void(*destruct)(struct AVPacket*);
} AVPacket;

// 把音视频AVPacket组成一个小链表。
typedef struct AVPacketList
{
    AVPacket pkt;
    struct AVPacketList *next;
} AVPacketList;

static inline void av_destruct_packet(AVPacket *pkt)
{
    av_free(pkt->data);
    pkt->data = NULL;
    pkt->size = 0;
}

static inline void av_free_packet(AVPacket *pkt)
{
    if (pkt && pkt->destruct)
        pkt->destruct(pkt);
}

static inline int av_get_packet(ByteIOContext *s, AVPacket *pkt, int size)
{
    int ret;
    unsigned char *data;
    if ((unsigned)size > (unsigned)size + FF_INPUT_BUFFER_PADDING_SIZE)
        return AVERROR_NOMEM;

    data = av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!data)
        return AVERROR_NOMEM;

    memset(data + size, 0, FF_INPUT_BUFFER_PADDING_SIZE);

    pkt->pts = AV_NOPTS_VALUE;
    pkt->dts = AV_NOPTS_VALUE;
    pkt->pos =  - 1;
    pkt->flags = 0;
    pkt->stream_index = 0;
    pkt->data = data;
    pkt->size = size;
    pkt->destruct = av_destruct_packet;

    pkt->pos = url_ftell(s);

    ret = url_fread(s, pkt->data, size);
    if (ret <= 0)
        av_free_packet(pkt);
    else
        pkt->size = ret;

    return ret;
}

typedef struct AVProbeData
{
    const char *filename;
    unsigned char *buf;
    int buf_size;
} AVProbeData;

typedef struct AVIndexEntry
{
    int64_t pos;
    int64_t timestamp;
    int flags: 2;
    int size: 30; //yeah trying to keep the size of this small to reduce memory requirements (its 24 vs 32 byte due to possible 8byte align)
} AVIndexEntry;

// 表示当前媒体流的上下文，着重于所有媒体流共有的属性(并且是在程序运行时才能确定其值)和关联其他结构的字段
typedef struct AVStream
{
    AVCodecContext *actx;  // 关联当前音视频媒体使用的编解码器

    void *priv_data;       // AVIStream  关联解析各个具体媒体流与文件容器有关的独有的属性

    AVRational time_base; // 由 av_set_pts_info()函数初始化

    AVIndexEntry *index_entries; // only used if the format does not support seeking natively
    int nb_index_entries;
    int index_entries_allocated_size;

    double frame_last_delay;
} AVStream;

typedef struct AVFormatParameters
{
    int dbg; //only for debug
} AVFormatParameters;

// 文件容器格式
typedef struct AVInputFormat
{
    const char *name;

    int priv_data_size;				// 标示具体的文件容器格式对应的Context的大小

    int(*read_probe)(AVProbeData*);

    int(*read_header)(struct AVFormatContext *, AVFormatParameters *ap);

    int(*read_packet)(struct AVFormatContext *, AVPacket *pkt);

    int(*read_close)(struct AVFormatContext*);

    const char *extensions;			// 文件扩展名

    struct AVInputFormat *next;

} AVInputFormat;

// AVFormatContext 结构表示程序运行的当前文件容器格式使用的上下文，着重于所有文件容器共有的属性(并
// 且是在程序运行时才能确定其值)和关联其他结构的字段。
typedef struct AVFormatContext  // format I/O context
{
    struct AVInputFormat *iformat;	// 关联相应的文件容器格式

    void *priv_data;				// 指向具体的文件容器格式的上下文Context和priv_data_size配对使用

    ByteIOContext pb;				// 广泛意义的输入文件 ????  

    int nb_streams;

    AVStream *streams[MAX_STREAMS];	// 关联音视频流

} AVFormatContext;

int avidec_init(void);

void av_register_input_format(AVInputFormat *format);

void av_register_all(void);

AVInputFormat *av_probe_input_format(AVProbeData *pd, int is_opened);
int match_ext(const char *filename, const char *extensions);

int av_open_input_stream(AVFormatContext **ic_ptr, ByteIOContext *pb, const char *filename, 
						 AVInputFormat *fmt, AVFormatParameters *ap);

int av_open_input_file(AVFormatContext **ic_ptr, const char *filename, AVInputFormat *fmt, 
					   int buf_size, AVFormatParameters *ap);

int av_read_frame(AVFormatContext *s, AVPacket *pkt);
int av_read_packet(AVFormatContext *s, AVPacket *pkt);
void av_close_input_file(AVFormatContext *s);
AVStream *av_new_stream(AVFormatContext *s, int id);
void av_set_pts_info(AVStream *s, int pts_wrap_bits, int pts_num, int pts_den);

int av_index_search_timestamp(AVStream *st, int64_t timestamp, int flags);
int av_add_index_entry(AVStream *st, int64_t pos, int64_t timestamp, int size, int distance, int flags);

int strstart(const char *str, const char *val, const char **ptr);
void pstrcpy(char *buf, int buf_size, const char *str);

#ifdef __cplusplus
}

#endif

#endif
