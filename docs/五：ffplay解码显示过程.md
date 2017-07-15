## ffplay解码显示过程

#### 解码器初始化

+ 根据AVStream中的AVCodecContext获取到解码器

  ```c
  // 找到从文件格式分析中得到的解码器上下文指针，便于引用其中的参数。
  enc = ic->streams[stream_index]->actx;
     
  // 依照编解码上下文的codec_id，遍历编解码器链表，找到相应的功能函数。
  codec = avcodec_find_decoder(enc->codec_id);

  // 核心功能之一,打开编解码器，初始化具体编解码器的运行环境。
  if (!codec || avcodec_open(enc, codec) < 0)
  	return  -1;
  ```

   其中**avcodec_find_decoder**也只是从链表中查找支持的**AVCodec类型**,列表节点类型如下

  ```c
  // 表示音视频编解码器，着重于功能函数，一种媒体类型对应一个AVCodec结构，在程序运行时有多个实例串联成链表便于查找。
  typedef struct AVCodec
  {
  	const char *name;	// 标示Codec的名字
  	enum CodecType type;// 标示Codec的类型，有Video ，Audio，Data 等类型
  	enum CodecID id;	// 标示Codec的ID，有CODEC_ID_MSRLE，CODEC_ID_TRUESPEECH 等
  	int priv_data_size;	// 标示具体的Codec对应的Context的大小
  	// 标示Codec对外提供的操作
  	int(*init)(AVCodecContext*);
  	int(*encode)(AVCodecContext *, uint8_t *buf, int buf_size, void *data);
  	int(*close)(AVCodecContext*);
  	int(*decode)(AVCodecContext *, void *outdata, int *outdata_size, uint8_t *buf, int buf_size);
  	int capabilities;	// 标示Codec的能力，在瘦身后的ffplay中没太大作用，可忽略

  	struct AVCodec *next; // 用于把所有Codec串成一个链表，便于遍历
  }AVCodec;
  ```
  **avcodec_open**打开编解码器，初始化具体编解码器的运行环境

  ```c
  int avcodec_open(AVCodecContext *avctx, AVCodec *codec);
  ```

  传入参数codec会赋值给AVCodecContext的codec，传入参数的codec priv_data_size大小决定了AVCodecContext中priv_data的大小，最后调用 `int(*init)(AVCodecContext*);`函数 

#### 解码数据

​	从队列中获取数据，然后调用**avcodec_decode_video**进行解码

```c
 int avcodec_decode_video(AVCodecContext *avctx, AVFrame *picture, int *got_picture_ptr,
		uint8_t *buf, int buf_size)
 {
    int ret;
    *got_picture_ptr = 0;
    if (buf_size)
    {
      	// 调用AVCodec的decode进行解码
		ret = avctx->codec->decode(avctx, picture, got_picture_ptr, buf, buf_size);
		if (*got_picture_ptr)
	    	avctx->frame_number++;
    }
    else
	ret = 0;
    return ret;
}
```

#### **显示数据**



​	