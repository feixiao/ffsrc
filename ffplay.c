
#include "./libavformat/avformat.h"

#if defined(CONFIG_WIN32)
#include <sys/types.h>
#include <sys/timeb.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/time.h>
#endif

#include <time.h>

#include <math.h>
#include <SDL.h>
#include <SDL_thread.h>

// SDL 里面定义了main 函数，所以在这里取消sdl 中的main 定义，避免重复定义。
#ifdef CONFIG_WIN32
#undef main // We don't want SDL to override our main()
#endif

// 导入SDL 库。
#pragma comment(lib, "SDL.lib")

#define FF_QUIT_EVENT   (SDL_USEREVENT + 2)

#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)

#define VIDEO_PICTURE_QUEUE_SIZE 1
// 音视频数据包/数据帧队列数据结构定义
typedef struct PacketQueue
{
	AVPacketList *first_pkt, *last_pkt;
	int size;
	int abort_request;
	SDL_mutex *mutex;
	SDL_cond *cond;
} PacketQueue;

// 视频图像数据结构定义
typedef struct VideoPicture
{
	SDL_Overlay *bmp;
	int width, height; // source height & width
} VideoPicture;
// 总控数据结构，把其他核心数据结构整合在一起，起一个中转的作用，便于在各个子结构之间跳转。
typedef struct VideoState
{
	SDL_Thread *parse_tid;		// Demux 解复用线程指针
	SDL_Thread *video_tid;		// video 解码线程指针

	int abort_request;			// 异常退出请求标记

	AVFormatContext *ic;		// 输入文件格式上下文指针，和iformat 配套使用

	int audio_stream;			// 音频流索引，表示AVFormatContext 中AVStream *streams[]数组索引
	int video_stream;			// 视频流索引，表示AVFormatContext 中AVStream *streams[]数组索引

	AVStream *audio_st;			// 音频流指针
	AVStream *video_st;			// 视频流指针

	PacketQueue audioq;			// 音频数据帧/数据包队列
	PacketQueue videoq;			// 视频数据帧/数据包队列

	VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];		// 解码后视频图像队列数组
	double frame_last_delay;							// 视频帧延迟，可简单认为是显示间隔时间

	uint8_t audio_buf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];	// 输出音频缓存
	unsigned int audio_buf_size;		// 解码后音频数据大小
	int audio_buf_index;				// 已输出音频数据大小
	AVPacket audio_pkt;					// 如果一个音频包中有多个帧，用于保存中间状态
	uint8_t *audio_pkt_data;			// 音频包数据首地址，配合audio_pkt 保存中间状态
	int audio_pkt_size;					// 音频包数据大小，配合audio_pkt 保存中间状态

	SDL_mutex *video_decoder_mutex;		// 视频数据包队列同步操作而定义的互斥量指针
	SDL_mutex *audio_decoder_mutex;		// 音频数据包队列同步操作而定义的互斥量指针

	char filename[240];					// 媒体文件名

} VideoState;

static AVInputFormat *file_iformat;
static const char *input_filename;
static VideoState *cur_stream;

// SDL 库需要的显示表面。
static SDL_Surface *screen;

// 取得当前时间，以1/1000000 秒为单位，为便于在各个平台上移植，由宏开关控制编译的代码。
int64_t av_gettime(void)
{
#if defined(CONFIG_WINCE)
	return timeGetTime() *int64_t_C(1000);
#elif defined(CONFIG_WIN32)
	struct _timeb tb;
	_ftime(&tb);
	return ((int64_t)tb.time *int64_t_C(1000) + (int64_t)tb.millitm) *int64_t_C(1000);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
#endif
}
// 初始化队列，初始化为0 后再创建线程同步使用的互斥和条件。
static void packet_queue_init(PacketQueue *q) // packet queue handling
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = SDL_CreateMutex();
	q->cond = SDL_CreateCond();
}

// 刷新队列，释放掉队列中所有动态分配的内存，包括音视频裸数据占用的内存和AVPacketList 结构占用的内存
static void packet_queue_flush(PacketQueue *q)
{
	AVPacketList *pkt, *pkt1;

	// 由于是多线程程序，需要同步，所以在遍历队列释放所有动态分配内存前，加锁。
	SDL_LockMutex(q->mutex);
	for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1)
	{
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);		// 释放音视频数据内存
		av_freep(&pkt);					// 释放AVPacketList 结构
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->size = 0;
	SDL_UnlockMutex(q->mutex);
}

// 释放队列占用所有资源，首先释放掉所有动态分配的内存，接着释放申请的互斥量和条件量。
static void packet_queue_end(PacketQueue *q)
{
	packet_queue_flush(q);
	SDL_DestroyMutex(q->mutex);
	SDL_DestroyCond(q->cond);
}

// 往音视频队列中挂接音视频数据帧/数据包。
static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	AVPacketList *pkt1;

	// 先分配一个AVPacketList 结构内存
	pkt1 = av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return  -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;

	SDL_LockMutex(q->mutex);

	if (!q->last_pkt)
		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	// 添加到队列末尾
	q->last_pkt = pkt1;
	// 统计缓存的媒体数据大小
	q->size += pkt1->pkt.size;

	// 设置条件量为有信号状态，如果解码线程因等待而睡眠就及时唤醒。
	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);
	return 0;
}

// 设置异常请求退出状态。
static void packet_queue_abort(PacketQueue *q)
{
	SDL_LockMutex(q->mutex);

	q->abort_request = 1;	// 请求异常退出

	SDL_CondSignal(q->cond);

	SDL_UnlockMutex(q->mutex);
}

// 从队列中取出一帧/包数据。
/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int ret;

	SDL_LockMutex(q->mutex);

	for (;;)
	{
		// 如果异常请求退出标记置位，就带错误码返回。
		if (q->abort_request)
		{
			ret = -1; // 异常
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1)
		{
			// 如果队列中有数据，就取第一个数据包
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			// 修正缓存的媒体大小
			q->size -= pkt1->pkt.size;
			*pkt = pkt1->pkt;
			// 释放掉AVPacketList 结构
			av_free(pkt1);
			ret = 1;
			break;
		}
		else if (!block)// 阻塞标记，1(阻塞模式)，0(非阻塞模式)
		{
			ret = 0; // 非阻塞模式，没东西直接返回0
			break;
		}
		else
		{
			// 如果是阻塞模式，没数据就进入睡眠状态等待
			SDL_CondWait(q->cond, q->mutex);
		}
	}
	SDL_UnlockMutex(q->mutex);
	return ret;
}

// 分配SDL 库需要的Overlay 显示表面，并设置长宽属性。
static void alloc_picture(void *opaque)
{
	VideoState *is = opaque;
	VideoPicture *vp;

	vp = &is->pictq[0];

	if (vp->bmp)
		SDL_FreeYUVOverlay(vp->bmp);

	vp->bmp = SDL_CreateYUVOverlay(is->video_st->actx->width,
		is->video_st->actx->height,
		SDL_YV12_OVERLAY,
		screen);

	vp->width = is->video_st->actx->width;
	vp->height = is->video_st->actx->height;
}

static int video_display(VideoState *is, AVFrame *src_frame, double pts)
{
	VideoPicture *vp;
	int dst_pix_fmt;
	AVPicture pict;

	if (is->videoq.abort_request)
		return  -1;

	vp = &is->pictq[0];

	/* if the frame is not skipped, then display it */
	if (vp->bmp)
	{
		SDL_Rect rect;

		if (pts)
			Sleep((int)(is->frame_last_delay * 1000));
#if 1
		/* get a pointer on the bitmap */
		SDL_LockYUVOverlay(vp->bmp);

		dst_pix_fmt = PIX_FMT_YUV420P;
		pict.data[0] = vp->bmp->pixels[0];
		pict.data[1] = vp->bmp->pixels[2];
		pict.data[2] = vp->bmp->pixels[1];

		pict.linesize[0] = vp->bmp->pitches[0];
		pict.linesize[1] = vp->bmp->pitches[2];
		pict.linesize[2] = vp->bmp->pitches[1];

		img_convert(&pict,
			dst_pix_fmt,
			(AVPicture*)src_frame,
			is->video_st->actx->pix_fmt,
			is->video_st->actx->width,
			is->video_st->actx->height);

		SDL_UnlockYUVOverlay(vp->bmp); /* update the bitmap content */

		rect.x = 0;
		rect.y = 0;
		rect.w = is->video_st->actx->width;
		rect.h = is->video_st->actx->height;
		SDL_DisplayYUVOverlay(vp->bmp, &rect);
#endif
	}
	return 0;
}
// 视频解码线程，主要功能是分配解码帧缓存和SDL 显示缓存后进入解码循环(从队列中取数据帧，解码，计算时钟，显示)，释放视频数据帧 / 数据包缓存。
static int video_thread(void *arg)
{
	VideoState *is = arg;
	AVPacket pkt1, *pkt = &pkt1;
	int len1, got_picture;
	double pts = 0;
	// 分配解码帧缓存
	AVFrame *frame = av_malloc(sizeof(AVFrame));
	memset(frame, 0, sizeof(AVFrame));

	// 分配SDL 显示缓存
	alloc_picture(is);

	for (;;)
	{
		// 从队列中取数据帧/数据包
		if (packet_queue_get(&is->videoq, pkt, 1) < 0)
			break;

		// 实质性解码
		SDL_LockMutex(is->video_decoder_mutex);
		len1 = avcodec_decode_video(is->video_st->actx, frame, &got_picture, pkt->data, pkt->size);
		SDL_UnlockMutex(is->video_decoder_mutex);

		// 计算同步时钟
		if (pkt->dts != AV_NOPTS_VALUE)
			pts = av_q2d(is->video_st->time_base) *pkt->dts;

		// 判断得到图像，调用显示函数同步显示视频图像。
		if (got_picture)
		{
			if (video_display(is, frame, pts) < 0)
				goto the_end;
		}
		// 释放视频数据帧/数据包内存，此数据包内存是在av_get_packet()函数中调用av_malloc()分配的。
		av_free_packet(pkt);
	}

the_end:
	av_free(frame);
	return 0;
}
// 解码一个音频帧，返回解压的数据大小。特别注意一个音频包可能包含多个音频帧，但一次只解码一个音频帧，所以一包可能要多次才能解码完。
// 程序首先用while 语句判断包数据是否全部解完，如果没有就解码当前包中的帧，修改状态参数；否则，释放数据包，再从队列中取，记录初始值，再进循环。
/* decode one audio frame and returns its uncompressed size */
static int audio_decode_frame(VideoState *is, uint8_t *audio_buf, double *pts_ptr)
{
	AVPacket *pkt = &is->audio_pkt;
	int len1, data_size;

	for (;;)
	{
		/* NOTE: the audio packet can contain several frames */
		// 一个音频包可能包含多个音频帧，可能需多次解码，VideoState 用一个AVPacket 型变量保存多次解码的中间状态。
		// 如果多次解码但不是最后次解码，audio_decode_frame 直接进while 循环。
		while (is->audio_pkt_size > 0)
		{
			// 调用解码函数解码，avcodec_decode_audio()函数返回解码用掉的字节数。
			SDL_LockMutex(is->audio_decoder_mutex);
			len1 = avcodec_decode_audio(is->audio_st->actx, (int16_t*)audio_buf,
				&data_size, is->audio_pkt_data, is->audio_pkt_size);

			SDL_UnlockMutex(is->audio_decoder_mutex);
			if (len1 < 0)
			{
				/* if error, we skip the frame */
				// 如果发生错误，跳过当前帧，跳出底层循环。
				is->audio_pkt_size = 0;
				break;
			}
			// 修正解码后的音频帧缓存首地址和大小。
			is->audio_pkt_data += len1;
			is->audio_pkt_size -= len1;
			if (data_size <= 0) {
				// 如果没有得到解码后的数据，继续解码。
				// 可能有些帧第一次解码时只解一个帧头就返回，此时需要继续解码数据帧。
				continue;
			}
			// 返回解码后的数据大小。
			return data_size;
		}

		// 程序到这里，可能是初始时audio_pkt 没有赋值；或者一包已经解码完，此时需要释放包数据内存。
		/* free the current packet */
		if (pkt->data)
			av_free_packet(pkt);

		// 读取下一个数据包。
		/* read next packet */
		if (packet_queue_get(&is->audioq, pkt, 1) < 0)
			return  -1;

		// 初始化数据包首地址和大小，用于一包中包含多个音频帧需多次解码的情况。
		is->audio_pkt_data = pkt->data;
		is->audio_pkt_size = pkt->size;
	}
}
// 音频输出回调函数，每次音频输出缓存为空时，系统就调用此函数填充音频输出缓存。
// 目前采用比较简单的同步方式，音频按照自己的节拍往前走即可，不需要synchronize_audio()函数同步处理。
/* prepare a new audio buffer */
void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
	VideoState *is = opaque;
	int audio_size, len1;
	double pts = 0;

	while (len > 0)
	{
		if (is->audio_buf_index >= is->audio_buf_size)
		{
			// 如果解码后的数据已全部输出，就进行音频解码
			audio_size = audio_decode_frame(is, is->audio_buf, &pts);
			if (audio_size < 0)
			{
				/* if error, just output silence */
				is->audio_buf_size = 1024;
				memset(is->audio_buf, 0, is->audio_buf_size);
			}
			else
			{
				//              audio_size = synchronize_audio(is, (int16_t*)is->audio_buf, audio_size, pts);
				is->audio_buf_size = audio_size;
			}
			is->audio_buf_index = 0;
		}
		// 拷贝适当的数据到输出缓存，并修改解码缓存的参数，进下一轮循环。
		// 特别注意：由进下一轮循环可知，程序应填满SDL 库给出的输出缓存。
		len1 = is->audio_buf_size - is->audio_buf_index;
		if (len1 > len)
			len1 = len;
		memcpy(stream, (uint8_t*)is->audio_buf + is->audio_buf_index, len1);
		len -= len1;
		stream += len1;
		is->audio_buf_index += len1;
	}
}

// 打开流模块，核心功能是打开相应codec，启动解码线程(我们把音频回调函数看做一个广义的线程)。
/* open a given stream. Return 0 if OK */
static int stream_component_open(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->ic;
	AVCodecContext *enc;
	AVCodec *codec;
	SDL_AudioSpec wanted_spec, spec;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return  -1;
	// 找到从文件格式分析中得到的解码器上下文指针，便于引用其中的参数。
	enc = ic->streams[stream_index]->actx;

	/* prepare audio output */
	if (enc->codec_type == CODEC_TYPE_AUDIO)
	{
		// 初始化音频输出参数，并调用SDL_OpenAudio()设置到SDL 库。
		wanted_spec.freq = enc->sample_rate;
		wanted_spec.format = AUDIO_S16SYS;
		/* hack for AC3. XXX: suppress that */
		if (enc->channels > 2)
			enc->channels = 2;
		wanted_spec.channels = enc->channels;
		wanted_spec.silence = 0;
		wanted_spec.samples = 1024; //SDL_AUDIO_BUFFER_SIZE;
		wanted_spec.callback = sdl_audio_callback; // 音频线程的回调函数
		wanted_spec.userdata = is;
		if (SDL_OpenAudio(&wanted_spec, &spec) < 0)
		{
			// wanted_spec 是应用程序设定给SDL 库的音频参数，spec 是SDL 库返回给应用程序它能支持的音频参数，通常是一致的。
			// 如果超过SDL 支持的参数范围，会返回最相近的参数。
			fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
			return  -1;
		}
	}

	// 依照编解码上下文的codec_id，遍历编解码器链表，找到相应的功能函数。
	codec = avcodec_find_decoder(enc->codec_id);

	// 核心功能之一,打开编解码器，初始化具体编解码器的运行环境。
	if (!codec || avcodec_open(enc, codec) < 0)
		return  -1;

	switch (enc->codec_type)
	{
	case CODEC_TYPE_AUDIO:
		// 在VideoState 中记录音频流参数。
		is->audio_stream = stream_index;
		is->audio_st = ic->streams[stream_index];
		is->audio_buf_size = 0;
		is->audio_buf_index = 0;
		// 初始化音频队列
		memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
		packet_queue_init(&is->audioq);
		SDL_PauseAudio(0);	// 启动广义的音频解码线程。
		break;
	case CODEC_TYPE_VIDEO:
		// 在VideoState 中记录视频流参数。
		is->video_stream = stream_index;
		is->video_st = ic->streams[stream_index];

		is->frame_last_delay = is->video_st->frame_last_delay;
		// 初始化视频队列
		packet_queue_init(&is->videoq);
		is->video_tid = SDL_CreateThread(video_thread, is);			// 直接启动视频解码线程。
		break;
	default:
		break;
	}
	return 0;
}
// 关闭流模块，停止解码线程，释放队列资源。
// 通过packet_queue_abort()函数置abort_request 标志位，解码线程判别此标志位并安全退出线程。
static void stream_component_close(VideoState *is, int stream_index)
{
	AVFormatContext *ic = is->ic;
	AVCodecContext *enc;
	// 简单的流索引参数校验。
	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return;
	// 找到从文件格式分析中得到的解码器上下文指针，便于引用其中的参数。
	enc = ic->streams[stream_index]->actx;

	switch (enc->codec_type)
	{
		// 停止解码线程，释放队列资源。
	case CODEC_TYPE_AUDIO:
		packet_queue_abort(&is->audioq);
		SDL_CloseAudio();
		packet_queue_end(&is->audioq);
		break;
	case CODEC_TYPE_VIDEO:
		packet_queue_abort(&is->videoq);
		SDL_WaitThread(is->video_tid, NULL);
		packet_queue_end(&is->videoq);
		break;
	default:
		break;
	}
	// 释放编解码器上下文资源
	avcodec_close(enc);
}
// 文件解析线程，函数名有点不名副其实。完成三大功能，直接识别文件格式和间接识别媒体格式，打开具体的编解码器并启动解码线程，分离音视频媒体包并挂接到相应队列。
static int decode_thread(void *arg)
{
	VideoState *is = arg;
	AVFormatContext *ic;
	int err, i, ret, video_index, audio_index;
	AVPacket pkt1, *pkt = &pkt1;
	AVFormatParameters params, *ap = &params;

	int flags = SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_RESIZABLE;

	// 初始化基本变量指示没有相应的流。
	video_index = -1;
	audio_index = -1;

	is->video_stream = -1;
	is->audio_stream = -1;

	memset(ap, 0, sizeof(*ap));
	// 调用函数直接识别文件格式，在此函数中再调用其他函数间接识别媒体格式。
	err = av_open_input_file(&ic, is->filename, NULL, 0, ap);
	if (err < 0)
	{
		ret = -1;
		goto fail;
	}
	// 保存文件格式上下文，便于各数据结构间跳转。
	is->ic = ic;

	for (i = 0; i < ic->nb_streams; i++)
	{
		AVCodecContext *enc = ic->streams[i]->actx;
		switch (enc->codec_type)
		{
			// 保存音视频流索引，并把显示视频参数设置到SDL 库。
		case CODEC_TYPE_AUDIO:
			if (audio_index < 0)
				audio_index = i;
			break;
		case CODEC_TYPE_VIDEO:
			if (video_index < 0)
				video_index = i;

			screen = SDL_SetVideoMode(enc->width, enc->height, 0, flags);

			SDL_WM_SetCaption("FFplay", "FFplay"); // 修改是为了适配视频大小

//          schedule_refresh(is, 40);
			break;
		default:
			break;
		}
	}
	// 如果有音频流，就调用函数打开音频编解码器并启动音频广义解码线程。
	if (audio_index >= 0)
		stream_component_open(is, audio_index);
	// 如果有视频流，就调用函数打开视频编解码器并启动视频解码线程。
	if (video_index >= 0)
		stream_component_open(is, video_index);
	// 如果既没有音频流，又没有视频流，就设置错误码返回。
	if (is->video_stream < 0 && is->audio_stream < 0)
	{
		fprintf(stderr, "%s: could not open codecs\n", is->filename);
		ret = -1;
		goto fail;
	}

	for (;;)
	{
		if (is->abort_request) {
			// 如果异常退出请求置位，就退出文件解析线程。
			break;
		}


		if (is->audioq.size > MAX_AUDIOQ_SIZE || is->videoq.size > MAX_VIDEOQ_SIZE || url_feof(&ic->pb))
		{
			// 如果队列满，就稍微延时一下。
			SDL_Delay(10); // if the queue are full, no need to read more,wait 10 ms
			continue;
		}
		// 从媒体文件中完整的读取一包音视频数据。
		ret = av_read_packet(ic, pkt); //av_read_frame(ic, pkt);		
		if (ret < 0)
		{
			if (url_ferror(&ic->pb) == 0)
			{
				SDL_Delay(100); // wait for user event
				continue;
			}
			else
				break;
		}

		{
			unsigned int *p1 = (unsigned int*)(pkt->data);
			unsigned int *p2 = p1 + 1;

			if ((*p1 == 0x3c8638) && (*p2 == 0x1185148))
			{
				int dbg = 0;
			}
		}
		// 判断包数据的类型，分别挂接到相应队列，如果是不识别的类型，就直接释放丢弃掉。
		if (pkt->stream_index == is->audio_stream)
		{
			packet_queue_put(&is->audioq, pkt);
		}
		else if (pkt->stream_index == is->video_stream)
		{
			packet_queue_put(&is->videoq, pkt);
		}
		else
		{
			av_free_packet(pkt);
		}
	}
	// 简单的延时，让后面的线程有机会把数据解码显示完。当然丢弃掉最后的一点点数据也可以。
	while (!is->abort_request)   // wait until the end
	{
		SDL_Delay(100);
	}

	ret = 0;

	// 释放掉在本线程中分配的各种资源，体现了谁申请谁释放的程序自封闭性。
fail:
	if (is->audio_stream >= 0)
		stream_component_close(is, is->audio_stream);

	if (is->video_stream >= 0)
		stream_component_close(is, is->video_stream);

	if (is->ic)
	{
		av_close_input_file(is->ic);
		is->ic = NULL;
	}

	if (ret != 0)
	{
		SDL_Event event;

		event.type = FF_QUIT_EVENT;
		event.user.data1 = is;
		SDL_PushEvent(&event);
	}
	return 0;
}

// 打开流。主要功能是分配全局总控数据结构，初始化相关参数，启动文件解析线程。
static VideoState *stream_open(const char *filename, AVInputFormat *iformat)
{
	VideoState *is;

	is = av_mallocz(sizeof(VideoState));
	if (!is)
		return NULL;
	pstrcpy(is->filename, sizeof(is->filename), filename);

	is->audio_decoder_mutex = SDL_CreateMutex();
	is->video_decoder_mutex = SDL_CreateMutex();

	is->parse_tid = SDL_CreateThread(decode_thread, is);
	if (!is->parse_tid)
	{
		av_free(is);
		return NULL;
	}
	return is;
}
// 关闭流。主要功能是释放资源。
static void stream_close(VideoState *is)
{
	VideoPicture *vp;
	int i;

	is->abort_request = 1;
	SDL_WaitThread(is->parse_tid, NULL);

	for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++)
	{
		vp = &is->pictq[i];
		if (vp->bmp)
		{
			SDL_FreeYUVOverlay(vp->bmp);
			vp->bmp = NULL;
		}
	}

	SDL_DestroyMutex(is->audio_decoder_mutex);
	SDL_DestroyMutex(is->video_decoder_mutex);

	free(is);
}

// 程序退出时调用的函数，关闭释放一些资源。
void do_exit(void)
{
	if (cur_stream)
	{
		stream_close(cur_stream);
		cur_stream = NULL;
	}

	SDL_Quit();
	exit(0);
}
// SDL 库的消息事件循环。
void event_loop(void) // handle an event sent by the GUI
{
	SDL_Event event;

	for (;;)
	{
		SDL_WaitEvent(&event);
		switch (event.type)
		{
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
			case SDLK_q:
				do_exit();
				break;
			default:
				break;
			}
			break;
		case SDL_QUIT:
		case FF_QUIT_EVENT:
			do_exit();
			break;
		default:
			break;
		}
	}
}
// 入口函数，初始化SDL 库，注册SDL 消息事件，启动文件解析线程，进入消息循环。
int main(int argc, char **argv)
{
	int flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;

	av_register_all();

	input_filename = "D:\\workspace\\ffsrc\\CLOCKTXT_320.avi";

	if (SDL_Init(flags))
		exit(1);

	SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

	cur_stream = stream_open(input_filename, file_iformat);

	event_loop();

	return 0;
}
