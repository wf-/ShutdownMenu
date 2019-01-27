#pragma once
#include <switch/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void read_payload(FILE* f);
Result reboot_to_payload(void);

#ifdef __cplusplus
}
#endif
