#define OFFLINE_ASM_X86_64 0
#define OFFLINE_ASM_ARM 1
#define OFFLINE_ASM_ARMv7 0
#define OFFLINE_ASM_ARM64 0
#define OFFLINE_ASM_ARM64E 0
#define OFFLINE_ASM_RISCV64 0
#define OFFLINE_ASM_C_LOOP 0
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define LABEL_REFERENCE(x) STRINGIZE(x)
#define LOCAL_LABEL_STRING(x) ".L" STRINGIZE(x)
#define OFFLINE_ASM_BEGIN __asm__(".syntax unified\n.arm\n.text\n"
#define OFFLINE_ASM_END ".section .note.GNU-stack,\"\",%progbits\n");
#define OFFLINE_ASM_GLOBAL_LABEL(x) ".balign 4\n.globl " STRINGIZE(x) "\n.type " STRINGIZE(x) ",%function\n" STRINGIZE(x) ":\n"
#define OFFLINE_ASM_LOCAL_LABEL(x) LOCAL_LABEL_STRING(x) ":\n"
#define OFFLINE_ASM_GLUE_LABEL(x) OFFLINE_ASM_LOCAL_LABEL(x)
#include "ProbeAssembly.h"
