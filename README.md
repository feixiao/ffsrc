# ffsrc
裁剪版ffsrc,来自   http://www.cnblogs.com/mcodec/articles/1933754.html

ffsrc使用的ffmpeg版本

```c
#define LIBAVFORMAT_VERSION     50.4.0
#define LIBAVUTIL_VERSION       49.0.0
#define LIBAVCODEC_VERSION      51.8.0
```

ffmpeg-2.8.11使用的版本

```
// AVFORMAT 56.40.101
#define LIBAVFORMAT_VERSION_MAJOR 56
#define LIBAVFORMAT_VERSION_MINOR  40
#define LIBAVFORMAT_VERSION_MICRO 101

// AVUTIL 54.31.100
#define LIBAVUTIL_VERSION_MAJOR  54
#define LIBAVUTIL_VERSION_MINOR  31
#define LIBAVUTIL_VERSION_MICRO 100

// AVCODEC 56.60.100
#define LIBAVCODEC_VERSION_MAJOR 56
#define LIBAVCODEC_VERSION_MINOR 60
#define LIBAVCODEC_VERSION_MICRO 100
```

通过版本对比我们发现版本号变化很大，所以这边的分析主要是理清ffmpeg主要数据结构直接的联系以及框架代码，而不在于细节。

#### 文档索引

+ [《 一：播放器基本原理》](./docs/一：播放器基本原理.md)
+ [《 二：ffplay的架构分析》](./docs/二：ffplay的架构分析.md)
+ [《 三：ffmpeg主要数据结构之间的联系》](./docs/三：ffmpeg主要数据结构之间的联系.md)
+ [《 四：ffmpeg打开文件读取数据过程》](./docs/四：ffmpeg打开文件读取数据过程.md)
+ [《 五：ffplay解码显示过程》](./docs/五：ffplay解码显示过程.md)



