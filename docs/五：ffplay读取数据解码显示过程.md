## ffplay读取数据解码显示过程

#### 读取数据解码显示数据

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

   其中**avcodec_find_decoder**也只是从链表中查看,列表节点类型如下

  ```c
  // 表示音视频编解码器，着重于功能函数，一种媒体类型对应一个AVCodec结构，在程序运行时有多个实例串联成链表便于查找。
  typedef struct AVCodec
  {
  	const char *name;				// 标示Codec的名字
  	enum CodecType type;			// 标示Codec的类型，有Video ，Audio，Data 等类型
  	enum CodecID id;	// 标示Codec的ID，有CODEC_ID_MSRLE，CODEC_ID_TRUESPEECH 等
  	int priv_data_size;				// 标示具体的Codec对应的Context的大小
  	// 标示Codec对外提供的操作
  	int(*init)(AVCodecContext*);
  	int(*encode)(AVCodecContext *, uint8_t *buf, int buf_size, void *data);
  	int(*close)(AVCodecContext*);
  	int(*decode)(AVCodecContext *, void *outdata, int *outdata_size, uint8_t *buf, int buf_size);
  	int capabilities;	// 标示Codec的能力，在瘦身后的ffplay中没太大作用，可忽略

  	struct AVCodec *next; // 用于把所有Codec串成一个链表，便于遍历
  }AVCodec;
  ```