// RUN: %tapi-frontend -isysroot %S/../../Inputs/MacOSX10.15.sdk -target i386-apple-macos10.15 %s 2>&1 | FileCheck -check-prefixes=CHECK,CHECK_MACOS %s
// RUN: %tapi-frontend -isysroot %S/../../Inputs/MacOSX10.15.sdk -target x86_64-apple-macos10.15 %s 2>&1 | FileCheck -check-prefixes=CHECK,CHECK_MACOS %s
// RUN: %tapi-frontend -isysroot %S/../../Inputs/MacOSX10.15.sdk -target x86_64-apple-ios13.1-macabi %s 2>&1 | FileCheck -check-prefixes=CHECK,CHECK_IOS %s

// CHECK:            - name: _func1
// CHECK-NEXT:         loc:
// CHECK_MACOS-NEXT:   availability: i:10.10 d:0 o:0 u:0
// CHECK_IOS-NEXT:     availability: i:14 d:0 o:0 u:0
void func1(void);
void func1(void) __attribute__((availability(macosx, introduced = 10.10)))
__attribute__((availability(ios, introduced = 14.0)));

// CHECK:            - name: _globalVar1
// CHECK-NEXT:         loc:
// CHECK_MACOS-NEXT:   availability: i:10.9 d:0 o:0 u:0
// CHECK_IOS-NEXT:     availability: i:13.1 d:0 o:0 u:0
extern int globalVar1 __attribute__((availability(macosx, introduced = 10.9)));
