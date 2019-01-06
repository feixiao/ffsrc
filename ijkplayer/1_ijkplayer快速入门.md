## ijkplayer快速入门

#### 相关资源

+ [Bilibili/ijkplayer](https://github.com/Bilibili/ijkplayer) 官方github仓库。
+ [CarGuo/GSYVideoPlayer](https://github.com/CarGuo/GSYVideoPlayer) 给予ijkplayer的android视频播放器。
+ [AndPlayer/ijkplayer-0.8.8](https://github.com/AndPlayer/ijkplayer-0.8.8) Android Studio 3.2工程，只支持armv7。

#### 项目结构

##### ijkplayer-example结构

+ ijkplayer-example

  整个app，通过设置不同的条件启动播放器。example支持三个播放器AndroidMeidaPlayer、IjkMediaPlayer和IjkExoMediaPlayer。

+ ijkplayer-exo

  [ExoPlayer](https://github.com/google/ExoPlayer) Google开源的Android播放器，example进行封装进行对比测试。

+ ijkplayer-java

  通用的API接口，IMediaPlayer接口和AbstractMediaPlayer抽象类。里面最主要的是IMediaPlayer，它也是用来渲染显示多媒体的。

  + AndroidMediaPlayer

    继承AbstractMediaPlayer类，封装系统自带MediaPlayer。

  + IjkMediaPlayer

    继承AbstractMediaPlayer类，基于ffmpeg的播放器实现。

##### ijkplayer项目结构

```
ijkmedia：最主要的核心代码
	ijkplayer 核心代码实现，其中包括与android上层交互的实现、基本的解协议、编解码的基本流程。
	ijksdl: 音视频渲染相关，ijksdk依赖了第三方开源库libyuv
	ijkj4a: 这个和音频的输出有关
```

#### 参考资料

+ [Ijkplayer Android介绍](https://blog.csdn.net/hack__zsmj/article/details/50734011)
+ [使用ijkplayer做个视频播放器](http://blog.51cto.com/13591594/2084627)
+ [dkplayer基于IjkPlayer的视频播放器](https://github.com/dueeeke/dkplayer)
+ [PlayerBase](https://github.com/jiajunhui/PlayerBase)
+ [GSYVideoPlayer](https://github.com/CarGuo/GSYVideoPlayer)





