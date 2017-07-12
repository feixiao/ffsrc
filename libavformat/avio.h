#ifndef AVIO_H
#define AVIO_H

// 文件读写模块定义的数据结构和函数声明

#define URL_EOF (-1)

typedef int64_t offset_t;

// 简单的文件存取宏定义
#define URL_RDONLY 0
#define URL_WRONLY 1
#define URL_RDWR   2

// URLContext 结构表示程序运行的当前广义输入文件使用的上下文，
// 着重于所有广义输入文件共有的属性(并且是在程序运行时才能确定其值)和关联其他结构的字段
typedef struct URLContext
{
    struct URLProtocol *prot;	// prot 字段关联相应的广义输入文件
    int flags;			// 文件读写类型
    int max_packet_size;	// 如果非0，表示最大包大小，用于分配足够的缓存。
    void *priv_data;		// 在本例中，关联一个文件句柄
    char filename[1];		// specified filename
} URLContext;

// 表示广义的输入文件，着重于功能函数，瘦身后的ffplay只支持file一种输入文件
typedef struct URLProtocol
{
    const char *name;
    int(*url_open)(URLContext *h, const char *filename, int flags);
    int(*url_read)(URLContext *h, unsigned char *buf, int size);
    int(*url_write)(URLContext *h, unsigned char *buf, int size);
    offset_t(*url_seek)(URLContext *h, offset_t pos, int whence);
    int(*url_close)(URLContext *h);
    struct URLProtocol *next;			// 用于把所有支持的广义的输入文件连接成链表，便于遍历查找。
} URLProtocol;

// ByteIOContext结构
//	+-------------------+-------------------+--------------------------+--------------------+
//	|					|	缓存已经使用	   |  缓存未使用数据	|	文件未读数据	 |
//	+-------------------+-------------------+--------------------------+--------------------+
//	|                   |			|                  |				|
//文件起始位置		  buffer			 buf_ptr            buf_end


// 扩展URLProtocol结构成内部有缓冲机制的广泛意义上的文件，改善广义输入文件的IO性能。
typedef struct ByteIOContext
{
    unsigned char *buffer;		// 缓存首地址
    int buffer_size;			// 缓存大小
    unsigned char *buf_ptr, *buf_end;	// 缓存读指针和末指针
    void *opaque;			// 关联URLContext
    int(*read_buf)(void *opaque, uint8_t *buf, int buf_size);
    int(*write_buf)(void *opaque, uint8_t *buf, int buf_size);
    offset_t(*seek)(void *opaque, offset_t offset, int whence);
    offset_t pos;    // position in the file of the current buffer
    int must_flush;  // true if the next seek should flush
    int eof_reached; // true if eof reached
    int write_flag;  // true if open for writing
    int max_packet_size;	// 如果非0，表示最大数据帧大小，用于分配足够的缓存。
    int error;       // contains the error code or 0 if no error happened
} ByteIOContext;

int url_open(URLContext **h, const char *filename, int flags);
int url_read(URLContext *h, unsigned char *buf, int size);
int url_write(URLContext *h, unsigned char *buf, int size);
offset_t url_seek(URLContext *h, offset_t pos, int whence);
int url_close(URLContext *h);
int url_get_max_packet_size(URLContext *h);

int register_protocol(URLProtocol *protocol);

int init_put_byte(ByteIOContext *s,
    unsigned char *buffer,
    int buffer_size,
    int write_flag,
    void *opaque,
    int(*read_buf)(void *opaque, uint8_t *buf, int buf_size),
    int(*write_buf)(void *opaque, uint8_t *buf, int buf_size),
    offset_t(*seek)(void *opaque, offset_t offset, int whence));

offset_t url_fseek(ByteIOContext *s, offset_t offset, int whence);
void url_fskip(ByteIOContext *s, offset_t offset);
offset_t url_ftell(ByteIOContext *s);
offset_t url_fsize(ByteIOContext *s);
int url_feof(ByteIOContext *s);
int url_ferror(ByteIOContext *s);

int url_fread(ByteIOContext *s, unsigned char *buf, int size); // get_buffer
int get_byte(ByteIOContext *s);
unsigned int get_le32(ByteIOContext *s);
unsigned int get_le16(ByteIOContext *s);

int url_setbufsize(ByteIOContext *s, int buf_size);
int url_fopen(ByteIOContext *s, const char *filename, int flags);
int url_fclose(ByteIOContext *s);

int url_open_buf(ByteIOContext *s, uint8_t *buf, int buf_size, int flags);
int url_close_buf(ByteIOContext *s);

#endif
