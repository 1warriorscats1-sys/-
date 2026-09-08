#pragma once
// Port shim for the MSVC <process.h> used by the TH095 reconstruction.
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
unsigned _beginthread(void (*start)(void *), unsigned stackSize, unsigned initFlag, void *arg);
unsigned _beginthreadex(void *security, unsigned stackSize,
                        unsigned (__cdecl *start)(void *), void *arg,
                        unsigned initFlag, unsigned *threadId);
void _endthread(void);
unsigned _endthreadex(unsigned code);
#ifdef __cplusplus
}
#endif
