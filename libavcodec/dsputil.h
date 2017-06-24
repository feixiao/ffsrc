#ifndef DSPUTIL_H
#define DSPUTIL_H

#define MAX_NEG_CROP 1024

extern uint8_t cropTbl[256+2 * MAX_NEG_CROP];

void dsputil_static_init(void);

#endif
