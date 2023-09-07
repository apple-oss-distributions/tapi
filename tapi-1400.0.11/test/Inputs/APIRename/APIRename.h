#import <os/availability.h>

extern void NewAPIName() API_AVAILABLE(macos(10.8));

static inline void OldAPIName()
    API_DEPRECATED_WITH_REPLACEMENT("NewAPIName", macos(10.8, 10.10)) {
  return NewAPIName();
}

// There should be no warning for this inline function.
static inline void OtherInlinedFunction() {}

// There should be a warning for this API.
extern void OtherAPI();
