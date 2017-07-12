#include "avformat.h"

// strstart 实际的功能就是在str字符串中搜索val 字符串指示的头，并且去掉头后用*ptr 返回。
// 在本例中，在播本地文件时，在命令行输入时可能会在文件路径名前加前缀"file:", 
// 为调用系统的open函数，需要把这几个前导字符去掉，仅仅传入完整有效的文件路径名。
int strstart(const char *str, const char *val, const char **ptr)
{
    const char *p, *q;
    p = str;
    q = val;
    while (*q != '\0')
    {
	if (*p != *q)
	    return 0;
	p++;
	q++;
    }
    if (ptr)
	*ptr = p;
    return 1;
}

// 字符串拷贝函数，拷贝的字符数由buf_size 指定，更安全的字符串拷贝操作。
void pstrcpy(char *buf, int buf_size, const char *str)
{
    int c;
    char *q = buf;

    if (buf_size <= 0)
	return;

    for (;;)
    {
	c = *str++;
	if (c == 0 || q >= buf + buf_size - 1)
	    break;
	*q++ = c;
    }
    *q = '\0';
}
