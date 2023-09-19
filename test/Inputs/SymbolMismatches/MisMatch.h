#include <TargetConditionals.h>

#if defined(TARGET_CPU_X86_64) && TARGET_CPU_X86_64
extern int foo_arch_x86();
#else
extern int foo_arch_arm();
#endif

int foo();
int baz() __attribute__((visibility("hidden")));

__attribute__((visibility("hidden"))) inline int inlinedFunc() { return 1; }
