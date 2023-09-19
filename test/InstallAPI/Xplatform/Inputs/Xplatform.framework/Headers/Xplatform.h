#include <TargetConditionals.h>

#if TARGET_OS_MACCATALYST
#include <IOSMac/IOSMac.h>
#endif

inline int foo() {
  int x = 1;
#if TARGET_OS_MACCATALYST
  x += iOSAPI();
#endif
  return x;
}

extern int bar();
