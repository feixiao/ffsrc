#ifndef __BSWAP_H__
#define __BSWAP_H__


// short 和int 整数类型字节顺序交换，通常和CPU 大端或小端有关。

// Int 16 位短整数字节交换，简单的移位再或运算。
static inline uint16_t bswap_16(uint16_t x)
{
    return (x >> 8) | (x << 8);
}

// Int 32 位长整数字节交换
static inline uint32_t bswap_32(uint32_t x)
{
    x = ((x << 8) & 0xFF00FF00) | ((x >> 8) & 0x00FF00FF);
    return (x >> 16) | (x << 16);
}

// be2me ... BigEndian to MachineEndian
// le2me ... LittleEndian to MachineEndian

#define be2me_16(x) bswap_16(x)
#define be2me_32(x) bswap_32(x)
#define le2me_16(x) (x)
#define le2me_32(x) (x)

#endif
